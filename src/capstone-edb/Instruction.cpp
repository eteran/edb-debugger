/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Instruction.h>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <stdexcept>
#include "Util.h"
#include <sstream>

namespace CapstoneEDB {

static bool capstoneInitialized=false;
static Capstone::csh csh=0;
static Formatter activeFormatter;

bool isAMD64=false;

bool isX86_64() { return isAMD64; }

// TODO(eteran): I'm not the biggest fan (though I sometimes do it too) of APIs 
//               which take booleans, you end up having to check the headers to
//               know what true/false mean. Since we are going the capstone route,
//               let's go all in and make this take an enum specifying the mode
//               it'll be more clear from the call site
bool init(bool amd64)
{
	isAMD64=amd64;
    if(capstoneInitialized)
		Capstone::cs_close(&csh);
	capstoneInitialized=false;

	Capstone::cs_mode mode=Capstone::CS_MODE_32;
	if(amd64) mode=Capstone::CS_MODE_64;
	Capstone::cs_err result=Capstone::cs_open(Capstone::CS_ARCH_X86, mode, &csh);
	if(result!=Capstone::CS_ERR_OK)
		return false;
    capstoneInitialized=true;

	Capstone::cs_option(csh, Capstone::CS_OPT_DETAIL, Capstone::CS_OPT_ON);

	// Set selected formatting options on reinit
	activeFormatter.setOptions(activeFormatter.options());
	return true;
}

int Operand::size() const
{
	return owner_->cs_insn().detail->x86.operands[numberInInstruction_].size;
}

void Instruction::fillPrefix()
{
	// FIXME: Capstone seems to be unable to correctly report prefixes for
	// instructions like F0 F3 2E 3E 75 fa, where e.g. F0 and F3 are put
	// in the same slot, the same for 2E and 3E in this example.
	const auto& prefixes=detail_.x86.prefix;

	if(prefixes[0]==Capstone::X86_PREFIX_LOCK)
		prefix_|=PREFIX_LOCK;
	if(prefixes[0]==Capstone::X86_PREFIX_REP)
		prefix_|=PREFIX_REP;
	if(prefixes[0]==Capstone::X86_PREFIX_REPNE)
		prefix_|=PREFIX_REPNE;

	if(prefixes[1]==Capstone::X86_PREFIX_ES)
		prefix_|=PREFIX_ES;
	if(prefixes[1]==Capstone::X86_PREFIX_CS)
		prefix_|=PREFIX_CS;
	if(prefixes[1]==Capstone::X86_PREFIX_SS)
		prefix_|=PREFIX_SS;
	if(prefixes[1]==Capstone::X86_PREFIX_DS)
		prefix_|=PREFIX_DS;
	if(prefixes[1]==Capstone::X86_PREFIX_FS)
		prefix_|=PREFIX_FS;
	if(prefixes[1]==Capstone::X86_PREFIX_GS)
		prefix_|=PREFIX_GS;

	if(prefixes[2]==Capstone::X86_PREFIX_OPSIZE)
		prefix_|=PREFIX_OPERAND;
	if(prefixes[3]==Capstone::X86_PREFIX_ADDRSIZE)
		prefix_|=PREFIX_ADDRESS;

	if(is_conditional_jump())
	{
		if(prefix_&PREFIX_DS)
		{
			prefix_&=~PREFIX_DS;
			prefix_|=PREFIX_BRANCH_TAKEN;
		}
		if(prefix_&PREFIX_CS)
		{
			prefix_&=~PREFIX_CS;
			prefix_|=PREFIX_BRANCH_NOT_TAKEN;
		}
	}
}

Instruction& Instruction::operator=(const Instruction& other)
{
	insn_=other.insn_;
	detail_=other.detail_;
	valid_=other.valid_;
	rva_=other.rva_;
	firstByte_=other.firstByte_;
	prefix_=other.prefix_;
	operands_=other.operands_;
	// Update pointer after replacement
	insn_.detail=&detail_;

	return *this;
}

Instruction::Instruction(const Instruction& other)
{
	*this=other;
}

void adjustInstructionText(Capstone::cs_insn& insn)
{
	QString operands(insn.op_str);

	// Remove extra spaces
	operands.replace(" + ","+");
	operands.replace(" - ","-");

	operands.replace(QRegExp("\\bxword "),"tbyte ");
	operands.replace(QRegExp("(word|byte) ptr "),"\\1 ");

	if(activeFormatter.options().simplifyRIPRelativeTargets && isAMD64 && (insn.detail->x86.modrm&0xc7)==0x05)
	{
		QRegExp ripRel("\\brip ?[+-] ?((0x)?[0-9a-fA-F]+)\\b");
		operands.replace(ripRel,"rel 0x"+QString::number(insn.detail->x86.disp+insn.address+insn.size,16));
	}

	strcpy(insn.op_str,operands.toStdString().c_str());
}

void stripMemorySizes(char* op_str)
{
	QString operands(op_str);

	operands.replace(QRegExp("(\\b.?(mm)?word|byte)\\b( ptr)? "),"");

	strcpy(op_str,operands.toStdString().c_str());
}

Instruction::Instruction(const void* first, const void* last, uint64_t rva) noexcept
{
	assert(capstoneInitialized);
    const uint8_t* codeBegin=static_cast<const uint8_t*>(first);
    const uint8_t* codeEnd=static_cast<const uint8_t*>(last);
	rva_=rva;
	firstByte_=codeBegin[0];

	Capstone::cs_insn* insn=nullptr;
    if(first<last && Capstone::cs_disasm(csh, codeBegin, codeEnd-codeBegin, rva, 1, &insn))
	{
		valid_=true;
		std::memcpy(&insn_,insn,sizeof insn_);
		std::memcpy(&detail_,insn->detail, sizeof detail_);
		insn_.detail=&detail_;
		Capstone::cs_free(insn,1);
		adjustInstructionText(insn_);
		fillPrefix();
		Capstone::cs_x86_op* ops=insn_.detail->x86.operands;
		if((operation()==Operation::X86_INS_LCALL||operation()==Operation::X86_INS_LJMP) &&
				ops[0].type==Capstone::X86_OP_IMM)
		{
			assert(cs_insn_operand_count()==2);
			Operand operand(this,0);
			operand.type_=Operand::TYPE_ABSOLUTE;
			operand.abs_.seg=ops[0].imm;
			operand.abs_.offset=ops[1].imm;
			operands_.push_back(operand);
		}
		else for(std::size_t i=0;i<cs_insn_operand_count();++i)
		{
			Operand operand(this,i);
			switch(ops[i].type)
			{
			case Capstone::X86_OP_REG:
				operand.type_=Operand::TYPE_REGISTER;
				operand.reg_=ops[i].reg;
				break;
			case Capstone::X86_OP_IMM:
				operand.imm_=ops[i].imm;
				/* Operands can be relative in the following cases:
					* all versions of loop* instruction: E0, E1, E2 (always rel8)
					* some unconditional jumps: EB (rel8), E9 (rel16, rel32)
					* all conditional jumps have relative arguments (rel8, rel16, rel32)
					* some call instructions: E8 (rel16, rel32)
					* xbegin: C7 F8 (rel16, rel32)
				*/
				if(operation()==Operation::X86_INS_LOOP   ||
				   operation()==Operation::X86_INS_LOOPE  ||
				   operation()==Operation::X86_INS_LOOPNE ||
				   operation()==Operation::X86_INS_XBEGIN ||
				   is_conditional_jump() ||
				   (((operation()==Operation::X86_INS_JMP ||
					  operation()==Operation::X86_INS_CALL) && (insn_.detail->x86.opcode[0]==0xEB ||
																insn_.detail->x86.opcode[0]==0xE9 ||
																insn_.detail->x86.opcode[0]==0xE8)))
				  )
				{
					operand.type_=Operand::TYPE_REL;
					break;
				}
				switch(ops[i].size)
				{
				case 1: operand.type_=Operand::TYPE_IMMEDIATE8;  break;
				case 2: operand.type_=Operand::TYPE_IMMEDIATE16; break;
				case 4: operand.type_=Operand::TYPE_IMMEDIATE32; break;
				case 8: operand.type_=Operand::TYPE_IMMEDIATE64; break;
				default:operand.type_=Operand::TYPE_IMMEDIATE;   break;
				}
				break;
			case Capstone::X86_OP_MEM:
				switch(ops[i].size)
				{
				case 1:  operand.type_=Operand::TYPE_EXPRESSION8;   break;
				case 2:  operand.type_=Operand::TYPE_EXPRESSION16;  break;
				case 4:  operand.type_=Operand::TYPE_EXPRESSION32;  break;
				case 6:  operand.type_=Operand::TYPE_EXPRESSION48;  break;
				case 8:  operand.type_=Operand::TYPE_EXPRESSION64;  break;
				case 10: operand.type_=Operand::TYPE_EXPRESSION80;  break;
				case 16: operand.type_=Operand::TYPE_EXPRESSION128; break;
				case 32: operand.type_=Operand::TYPE_EXPRESSION256; break;
				case 64: operand.type_=Operand::TYPE_EXPRESSION512; break;
				}
                operand.expr_.displacement=ops[i].mem.disp; // FIXME: truncation or wrong type chosen by capstone?..
				if(ops[i].mem.disp!=0) // FIXME: this doesn't catch zero-valued existing displacements!
					operand.expr_.displacement_type=Operand::DISP_PRESENT;
				operand.expr_.base =static_cast<Operand::Register>(ops[i].mem.base);
				operand.expr_.index=static_cast<Operand::Register>(ops[i].mem.index);
				operand.expr_.scale=ops[i].mem.scale;
				switch(ops[i].mem.segment)
				{
				case Capstone::X86_REG_ES: operand.expr_.segment=Operand::Segment::ES; break;
				case Capstone::X86_REG_CS: operand.expr_.segment=Operand::Segment::CS; break;
				case Capstone::X86_REG_SS: operand.expr_.segment=Operand::Segment::SS; break;
				case Capstone::X86_REG_DS: operand.expr_.segment=Operand::Segment::DS; break;
				case Capstone::X86_REG_FS: operand.expr_.segment=Operand::Segment::FS; break;
				case Capstone::X86_REG_GS: operand.expr_.segment=Operand::Segment::GS; break;
				default: operand.expr_.segment=Operand::Segment::REG_INVALID; break;
				}
				if(operand.expr_.segment==Operand::Segment::REG_INVALID && !isAMD64)
				{
					if(operand.expr_.base==Capstone::X86_REG_BP ||
					   operand.expr_.base==Capstone::X86_REG_EBP ||
					   operand.expr_.base==Capstone::X86_REG_ESP)
						operand.expr_.segment=Operand::Segment::SS;
					else
						operand.expr_.segment=Operand::Segment::DS;
				}
			case Capstone::X86_OP_INVALID:
				break;
			}

			operands_.push_back(operand);
		}

		// Remove extraneous size specification as in 'mov eax, dword [ecx]'
		if(activeFormatter.options().syntax==Formatter::SyntaxIntel)
		{
			if(operand_count()==2 && operands_[0].size()==operands_[1].size() &&
					((operands_[0].general_type()==Operand::TYPE_REGISTER && operands_[1].general_type()==Operand::TYPE_EXPRESSION) ||
					 (operands_[1].general_type()==Operand::TYPE_REGISTER && operands_[0].general_type()==Operand::TYPE_EXPRESSION)))
			{
				stripMemorySizes(insn_.op_str);
			}
		}
	}
	else
	{
		std::memset(&insn_,0,sizeof insn_);
		std::memset(&detail_,0,sizeof detail_);
		if(first<last)
		{
			insn_.bytes[0]=firstByte_;
			insn_.size=1;
		}
		else insn_.size=0;
		insn_.id=Capstone::X86_INS_INVALID;
		std::strcpy(insn_.mnemonic,"(bad)");
	}

	// TODO: this is ugly. We should store only the operands we have, and
	//       demand that the user code checks bounds of its indexes.
	//       Something like removing operands() method and providing operand(size_t index).
	// Fill the rest with invalid operands
	while(operands_.size()<MAX_OPERANDS)
		operands_.push_back(Operand());
}

const uint8_t* Instruction::bytes() const
{
	return insn_.bytes;
}

std::size_t Instruction::cs_insn_operand_count() const
{
	return insn_.detail->x86.op_count;
}

std::size_t Instruction::operand_count() const
{
	std::size_t count=operands_.size();
	for(const auto& op : operands_)
		if(!op.valid()) --count;
	return count;
}

std::size_t Instruction::size() const
{
	return insn_.size;
}

Instruction::Operation Instruction::operation() const
{
	return static_cast<Operation>(insn_.id);
}

std::string Instruction::mnemonic() const
{
	return insn_.mnemonic;
}
Instruction::ConditionCode Instruction::condition_code() const
{
	switch(operation())
	{
	// J*CXZ
	case Operation::X86_INS_JRCXZ:    return CC_RCXZ;
	case Operation::X86_INS_JECXZ:    return CC_ECXZ;
	case Operation::X86_INS_JCXZ:     return CC_CXZ;
	// FPU conditional move
	case Operation::X86_INS_FCMOVBE:  return CC_BE;
	case Operation::X86_INS_FCMOVB:   return CC_B;
	case Operation::X86_INS_FCMOVE:   return CC_E;
	case Operation::X86_INS_FCMOVNBE: return CC_NBE;
	case Operation::X86_INS_FCMOVNB:  return CC_NB;
	case Operation::X86_INS_FCMOVNE:  return CC_NE;
	case Operation::X86_INS_FCMOVNU:  return CC_NP;
	case Operation::X86_INS_FCMOVU:   return CC_P;
	// TODO: handle LOOPcc, REPcc OP
	default:
		if(is_conditional_gpr_move() || is_conditional_jump() || is_conditional_set())
		{
			const uint8_t* opcode=insn_.detail->x86.opcode;
			if(opcode[0]==0x0f)
				return static_cast<ConditionCode>(opcode[1] & 0xf);
			return static_cast<ConditionCode>(opcode[0] & 0xf);
		}
	}
	return CC_UNCONDITIONAL;
}

bool Instruction::is_terminator() const
{
	return is_halt() || is_jump() || is_return();
}

bool Instruction::is_halt() const
{
	if(!valid_) return false;

	return operation()==Operation::X86_INS_HLT;
}

void Instruction::swap(Instruction &other)
{
	std::swap(*this,other);
}

bool Instruction::is_conditional_set() const
{
	switch(operation())
	{
	case Operation::X86_INS_SETAE:
	case Operation::X86_INS_SETA:
	case Operation::X86_INS_SETBE:
	case Operation::X86_INS_SETB:
	case Operation::X86_INS_SETE:
	case Operation::X86_INS_SETGE:
	case Operation::X86_INS_SETG:
	case Operation::X86_INS_SETLE:
	case Operation::X86_INS_SETL:
	case Operation::X86_INS_SETNE:
	case Operation::X86_INS_SETNO:
	case Operation::X86_INS_SETNP:
	case Operation::X86_INS_SETNS:
	case Operation::X86_INS_SETO:
	case Operation::X86_INS_SETP:
	case Operation::X86_INS_SETS:
		return true;
	default:
		return false;
	}
}

bool Instruction::is_unconditional_jump() const
{
	return operation()==Operation::X86_INS_JMP || operation()==Operation::X86_INS_LJMP;
}

bool Instruction::is_conditional_jump() const
{
	switch(operation())
	{
	case Operation::X86_INS_JAE:
	case Operation::X86_INS_JA:
	case Operation::X86_INS_JBE:
	case Operation::X86_INS_JB:
	case Operation::X86_INS_JCXZ:
	case Operation::X86_INS_JECXZ:
	case Operation::X86_INS_JE:
	case Operation::X86_INS_JGE:
	case Operation::X86_INS_JG:
	case Operation::X86_INS_JLE:
	case Operation::X86_INS_JL:
	case Operation::X86_INS_JNE:
	case Operation::X86_INS_JNO:
	case Operation::X86_INS_JNP:
	case Operation::X86_INS_JNS:
	case Operation::X86_INS_JO:
	case Operation::X86_INS_JP:
	case Operation::X86_INS_JRCXZ:
	case Operation::X86_INS_JS:
		return true;
	default:
		return false;
	}
}

bool Instruction::is_conditional_fpu_move() const
{
	switch(operation())
	{
	case Operation::X86_INS_FCMOVBE:
	case Operation::X86_INS_FCMOVB:
	case Operation::X86_INS_FCMOVE:
	case Operation::X86_INS_FCMOVNBE:
	case Operation::X86_INS_FCMOVNB:
	case Operation::X86_INS_FCMOVNE:
	case Operation::X86_INS_FCMOVNU:
	case Operation::X86_INS_FCMOVU:
		return true;
	default:
		return false;
	}
}

bool Instruction::is_conditional_gpr_move() const
{
	switch(operation())
	{
	case Operation::X86_INS_CMOVA:
	case Operation::X86_INS_CMOVAE:
	case Operation::X86_INS_CMOVB:
	case Operation::X86_INS_CMOVBE:
	case Operation::X86_INS_CMOVE:
	case Operation::X86_INS_CMOVG:
	case Operation::X86_INS_CMOVGE:
	case Operation::X86_INS_CMOVL:
	case Operation::X86_INS_CMOVLE:
	case Operation::X86_INS_CMOVNE:
	case Operation::X86_INS_CMOVNO:
	case Operation::X86_INS_CMOVNP:
	case Operation::X86_INS_CMOVNS:
	case Operation::X86_INS_CMOVO:
	case Operation::X86_INS_CMOVP:
	case Operation::X86_INS_CMOVS:
		return true;
	default:
		return false;
	}
}

bool Instruction::is_sysenter() const
{
	return operation()==Operation::X86_INS_SYSENTER;
}

bool Instruction::is_syscall() const
{
	return operation()==Operation::X86_INS_SYSCALL;
}

bool Instruction::is_call() const
{
	return operation()==Operation::X86_INS_CALL;
}

bool Instruction::is_return() const
{
	if(!valid_) return false;

	return Capstone::cs_insn_group(csh, &insn_, Capstone::X86_GRP_RET);
}

bool Instruction::is_interrupt() const
{
	const auto op=operation();
	return op==Operation::X86_INS_INT ||
		   op==Operation::X86_INS_INT1 ||
		   op==Operation::X86_INS_INT3 ||
		   op==Operation::X86_INS_INTO;
}

bool Instruction::is_ret() const
{
	return operation()==Operation::X86_INS_RET;
}

bool Instruction::is_int() const
{
	return operation()==Operation::X86_INS_INT;
}

bool Instruction::is_jump() const
{
	if(!valid_) return false;

	return Capstone::cs_insn_group(csh, &insn_, Capstone::X86_GRP_JUMP);
}

bool Instruction::is_nop() const
{
	if(!valid_) return false;

	return operation()==Operation::X86_INS_NOP;
}

bool Instruction::is_fpu() const
{
	return (detail_.x86.opcode[0]&0xd8)==0xd8;
}

bool Instruction::is_fpu_taking_float() const
{
	if(!is_fpu()) return false;

	const auto modrm=detail_.x86.modrm;
	if(modrm>0xbf) return true; // always works with st(i) in this case

	const auto ro=(modrm>>3)&7;
	switch(detail_.x86.opcode[0])
	{
	case 0xd8:
	case 0xdc:
		return true;
	case 0xdb:
		return ro==5||ro==7;
	case 0xd9:
	case 0xdd:
		return ro==0||ro==2||ro==3;
	default:
		return false;
	}
}

bool Instruction::is_fpu_taking_integer() const
{
	if(!is_fpu()) return false;

	const auto modrm=detail_.x86.modrm;
	if(modrm>0xbf) return false; // always works with st(i) in this case

	const auto ro=(modrm>>3)&7;
	switch(detail_.x86.opcode[0])
	{
	case 0xda: return true;
	case 0xdb: return 0<=ro&&ro<=3;
	case 0xdd: return ro==1;
	case 0xde: return true;
	case 0xdf: return (0<=ro&&ro<=3)||ro==5||ro==7;
	default: return false;
	}
}

bool Instruction::is_fpu_taking_bcd() const
{
	if(!is_fpu()) return false;

	const auto modrm=detail_.x86.modrm;
	if(modrm>0xbf) return false; // always works with st(i) in this case

	const auto ro=(modrm>>3)&7;
	return detail_.x86.opcode[0]==0xdf && (ro==4||ro==6);
}

bool Instruction::is_simd() const
{
	const Capstone::x86_insn_group simdGroups[]=
	{
		Capstone::X86_GRP_3DNOW,
		Capstone::X86_GRP_AVX,
		Capstone::X86_GRP_AVX2,
		Capstone::X86_GRP_AVX512,
		Capstone::X86_GRP_FMA,
		Capstone::X86_GRP_FMA4,
		Capstone::X86_GRP_MMX,
		Capstone::X86_GRP_SSE1,
		Capstone::X86_GRP_SSE2,
		Capstone::X86_GRP_SSE3,
		Capstone::X86_GRP_SSE41,
		Capstone::X86_GRP_SSE42,
		Capstone::X86_GRP_SSE4A,
		Capstone::X86_GRP_SSSE3,
		Capstone::X86_GRP_XOP,
		Capstone::X86_GRP_CDI,
		Capstone::X86_GRP_ERI,
		Capstone::X86_GRP_PFI,
		Capstone::X86_GRP_VLX,
		Capstone::X86_GRP_NOVLX,
	};
	for(auto g=0;g<cs_insn().detail->groups_count;++g)
		if(util::contains(simdGroups,cs_insn().detail->groups[g]))
			return true;
	return false;
}

void Formatter::setOptions(const Formatter::FormatOptions& options)
{
	assert(capstoneInitialized);
	options_=options;
	if(options.syntax==SyntaxATT)
		Capstone::cs_option(csh,Capstone::CS_OPT_SYNTAX, Capstone::CS_OPT_SYNTAX_ATT);
	else
		Capstone::cs_option(csh,Capstone::CS_OPT_SYNTAX, Capstone::CS_OPT_SYNTAX_INTEL);
	// FIXME: SmallNumberFormat is not yet supported
	activeFormatter=*this;
}

std::string Formatter::to_string(const Instruction& instruction) const
{
	if(!instruction) return "(bad)";
	
	enum
	{
		tab1Size=8,
		tab2Size=11,
	};
	std::ostringstream s;
	s << instruction.insn_.mnemonic;
	if(instruction.operand_count()>0) // prevent addition of trailing whitespace
	{
		if(options_.tabBetweenMnemonicAndOperands)
		{
			const auto pos = s.tellp();
			const auto pad = pos<tab1Size ? tab1Size-pos : pos<tab2Size ? tab2Size-pos : 1;
			s << std::string(pad,' ');
		}
		else s << ' ';
		s << instruction.insn_.op_str;
	}
	else
	{
		assert(instruction.insn_.op_str[0]==0);
	}
	auto str = s.str();
	checkCapitalize(str);
	return str;
}

void Formatter::checkCapitalize(std::string& str,bool canContainHex) const
{
	if(options_.capitalization==UpperCase)
	{
		std::transform(str.begin(),str.end(),str.begin(), ::toupper);
		if(canContainHex)
		{
			QString qstr=QString::fromStdString(str);
			str=qstr.replace(QRegExp("\\b0X([0-9A-F]+)\\b"),"0x\\1").toStdString();
		}
	}
}

std::vector<std::string> toOperands(QString str)
{
	// Remove any decorations: we want just operands themselves
	str.replace(QRegExp(",?\\{[^}]*\\}"),"");
	QStringList betweenCommas=str.split(",");
	std::vector<std::string> operands;
	// Have to work around inconvenient AT&T syntax for SIB, that's why so complicated logic
	for(auto it=betweenCommas.begin();it!=betweenCommas.end();++it)
	{
		QString& current(*it);
		// We've split operand string by commas, but there may be SIB scheme
		// in the form (B,I,S) or (B) or (I,S). Let's find missing parts of it.
		if(it->contains("(") && !it->contains(")"))
		{
			std::logic_error matchFailed("failed to find matching ')'");
			// the next part must exist and have continuation of SIB scheme
			if(std::next(it)==betweenCommas.end())
				throw matchFailed;
			current+=",";
			current+=*(++it);
			// This may still be not enough
			if(current.contains("(") && !current.contains(")"))
			{
				if(std::next(it)==betweenCommas.end())
					throw matchFailed;
				current+=",";
				current+=*(++it);
			}
			// The expected SIB string has at most three components.
			// If we still haven't found closing parenthesis, we're screwed
			if(current.contains("(") && !current.contains(")"))
				throw matchFailed;
		}
		operands.push_back(current.trimmed().toStdString());
	}
	if(operands.size()>Instruction::MAX_OPERANDS)
		throw std::logic_error("got more than "+std::to_string(Instruction::MAX_OPERANDS)+" operands");
	return operands;
}

std::string Formatter::to_string(const Operand& operand) const
{
	if(!operand) return "(bad)";

	std::string str;

	std::size_t totalOperands=operand.owner()->operand_count();
	if(operand.type_==Operand::TYPE_REGISTER)
		str=register_name(operand.reg());
	else if(totalOperands==1)
		str=operand.owner()->insn_.op_str;
	else
	{
		// Capstone doesn't provide a way to get operand string, so we try
		// to extract it from the formatted all-operands string
		try
		{
			const auto operands=toOperands(operand.owner()->insn_.op_str);
			if(operands.size()<=operand.numberInInstruction_)
				throw std::logic_error("got less than "+std::to_string(operand.numberInInstruction_)+" operands");
			str=operands[operand.numberInInstruction_];
		}
		catch(std::logic_error& error)
		{
			return std::string("(error splitting operands string: ")+error.what()+")";
		}
	}

	checkCapitalize(str);
	return str;
}

std::string Formatter::register_name(const Operand::Register reg) const
{
	assert(capstoneInitialized);
	const char* raw=Capstone::cs_reg_name(csh, reg);
	if(!raw)
		return "(invalid register)";
	std::string str(raw);
	checkCapitalize(str,false);
	return str;
}

bool Operand::is_simd_register() const
{
	if(general_type()!=TYPE_REGISTER) return false;
	const auto reg=this->reg();
	if(Capstone::X86_REG_MM0 <=reg && reg<=Capstone::X86_REG_MM7) return true;
	if(Capstone::X86_REG_XMM0<=reg && reg<=Capstone::X86_REG_XMM31) return true;
	if(Capstone::X86_REG_YMM0<=reg && reg<=Capstone::X86_REG_YMM31) return true;
	if(Capstone::X86_REG_ZMM0<=reg && reg<=Capstone::X86_REG_ZMM31) return true;
	return false;
}

bool Operand::apriori_not_simd() const
{
	if(!owner()->is_simd()) return true;
	if(general_type()==TYPE_REGISTER && !is_simd_register()) return true;
	if(general_type()==TYPE_IMMEDIATE) return true;
	return false;
}

bool KxRegisterPresent(Instruction const& insn)
{
	for(std::size_t i=0;i<insn.operand_count();++i)
	{
		const auto op=insn.operands()[i];
		if(op.general_type()==Operand::TYPE_REGISTER &&
			Capstone::X86_REG_K0 <= op.reg() && op.reg()<=Capstone::X86_REG_K7)
			return true;
	}
	return false;
}

std::size_t Operand::simdOperandNormalizedNumberInInstruction() const
{
	assert(!apriori_not_simd());

	std::size_t number=numberInInstruction_;

	const auto operandCount=owner()->operand_count();
	// normalized number is according to Intel order
	if(activeFormatter.options().syntax==Formatter::SyntaxATT)
	{
		assert(number<operandCount);
		number=operandCount-1-number;
	}
	if(number>0 && KxRegisterPresent(*owner()))
		--number;

	return number;
}

bool Operand::is_SIMD_PS() const
{
	if(apriori_not_simd()) return false;

	const auto number=simdOperandNormalizedNumberInInstruction();

	switch(owner()->operation())
	{
	case Instruction::Operation:: X86_INS_ADDPS:
	case Instruction::Operation::X86_INS_VADDPS:
	case Instruction::Operation:: X86_INS_ADDSUBPS:
	case Instruction::Operation::X86_INS_VADDSUBPS:
	case Instruction::Operation:: X86_INS_ANDNPS:
	case Instruction::Operation::X86_INS_VANDNPS:
	case Instruction::Operation:: X86_INS_ANDPS:
	case Instruction::Operation::X86_INS_VANDPS:
	case Instruction::Operation:: X86_INS_BLENDPS:   // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VBLENDPS:   // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_CMPPS:     // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VCMPPS:     // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_DIVPS:
	case Instruction::Operation::X86_INS_VDIVPS:
	case Instruction::Operation:: X86_INS_DPPS:      // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VDPPS:      // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_INSERTPS:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VINSERTPS:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_EXTRACTPS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VEXTRACTPS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_MOVAPS:
	case Instruction::Operation::X86_INS_VMOVAPS:
	case Instruction::Operation:: X86_INS_ORPS:
	case Instruction::Operation::X86_INS_VORPS:
	case Instruction::Operation:: X86_INS_XORPS:
	case Instruction::Operation::X86_INS_VXORPS:
	case Instruction::Operation:: X86_INS_HADDPS:
	case Instruction::Operation::X86_INS_VHADDPS:
	case Instruction::Operation:: X86_INS_HSUBPS:
	case Instruction::Operation::X86_INS_VHSUBPS:
	case Instruction::Operation:: X86_INS_MAXPS:
	case Instruction::Operation::X86_INS_VMAXPS:
	case Instruction::Operation:: X86_INS_MINPS:
	case Instruction::Operation::X86_INS_VMINPS:
	case Instruction::Operation:: X86_INS_MOVHLPS:
	case Instruction::Operation::X86_INS_VMOVHLPS:
	case Instruction::Operation:: X86_INS_MOVHPS:
	case Instruction::Operation::X86_INS_VMOVHPS:
	case Instruction::Operation:: X86_INS_MOVLHPS:
	case Instruction::Operation::X86_INS_VMOVLHPS:
	case Instruction::Operation:: X86_INS_MOVLPS:
	case Instruction::Operation::X86_INS_VMOVLPS:
	case Instruction::Operation:: X86_INS_MOVMSKPS: // GPR isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VMOVMSKPS: // GPR isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_MOVNTPS:
	case Instruction::Operation::X86_INS_VMOVNTPS:
	case Instruction::Operation:: X86_INS_MOVUPS:
	case Instruction::Operation::X86_INS_VMOVUPS:
	case Instruction::Operation:: X86_INS_MULPS:
	case Instruction::Operation::X86_INS_VMULPS:
	case Instruction::Operation:: X86_INS_RCPPS:
	case Instruction::Operation::X86_INS_VRCPPS:
	case Instruction::Operation:: X86_INS_ROUNDPS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VROUNDPS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_RSQRTPS:
	case Instruction::Operation::X86_INS_VRSQRTPS:
	case Instruction::Operation:: X86_INS_SHUFPS:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VSHUFPS:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_SQRTPS:
	case Instruction::Operation::X86_INS_VSQRTPS:
	case Instruction::Operation:: X86_INS_SUBPS:
	case Instruction::Operation::X86_INS_VSUBPS:
	case Instruction::Operation:: X86_INS_UNPCKHPS:
	case Instruction::Operation::X86_INS_VUNPCKHPS:
	case Instruction::Operation:: X86_INS_UNPCKLPS:
	case Instruction::Operation::X86_INS_VUNPCKLPS:
	case Instruction::Operation::X86_INS_VBLENDMPS:
#if CS_API_MAJOR>=4
	case Instruction::Operation::X86_INS_VCOMPRESSPS:
	case Instruction::Operation::X86_INS_VEXP2PS:
	case Instruction::Operation::X86_INS_VEXPANDPS:
#endif
	case Instruction::Operation::X86_INS_VFMADD132PS:
	case Instruction::Operation::X86_INS_VFMADD213PS:
	case Instruction::Operation::X86_INS_VFMADD231PS:
	case Instruction::Operation::X86_INS_VFMADDPS: 		 // FMA4 (AMD). XMM/YMM/mem operands only, ok
	case Instruction::Operation::X86_INS_VFMADDSUB132PS:
	case Instruction::Operation::X86_INS_VFMADDSUB213PS:
	case Instruction::Operation::X86_INS_VFMADDSUB231PS:
	case Instruction::Operation::X86_INS_VFMADDSUBPS:
	case Instruction::Operation::X86_INS_VFMSUB132PS:
	case Instruction::Operation::X86_INS_VFMSUBADD132PS:
	case Instruction::Operation::X86_INS_VFMSUBADD213PS:
	case Instruction::Operation::X86_INS_VFMSUBADD231PS:
	case Instruction::Operation::X86_INS_VFMSUBADDPS: 	 // FMA4 (AMD). Seems to have only XMM/YMM/mem operands (?)
	case Instruction::Operation::X86_INS_VFMSUB213PS:
	case Instruction::Operation::X86_INS_VFMSUB231PS:
	case Instruction::Operation::X86_INS_VFMSUBPS: 		 // FMA4?
	case Instruction::Operation::X86_INS_VFNMADD132PS:
	case Instruction::Operation::X86_INS_VFNMADD213PS:
	case Instruction::Operation::X86_INS_VFNMADD231PS:
	case Instruction::Operation::X86_INS_VFNMADDPS:		 // FMA4?
	case Instruction::Operation::X86_INS_VFNMSUB132PS:
	case Instruction::Operation::X86_INS_VFNMSUB213PS:
	case Instruction::Operation::X86_INS_VFNMSUB231PS:
	case Instruction::Operation::X86_INS_VFNMSUBPS:		 // FMA4?
	case Instruction::Operation::X86_INS_VFRCZPS:
	case Instruction::Operation::X86_INS_VRCP14PS:
	case Instruction::Operation::X86_INS_VRCP28PS:
	case Instruction::Operation::X86_INS_VRNDSCALEPS:
	case Instruction::Operation::X86_INS_VRSQRT14PS:
	case Instruction::Operation::X86_INS_VRSQRT28PS:
	case Instruction::Operation::X86_INS_VTESTPS:
		return true;

	case Instruction::Operation::X86_INS_VGATHERDPS:
	case Instruction::Operation::X86_INS_VGATHERQPS:
	// second operand is VSIB, it's not quite PS;
	// third operand, if present (VEX-encoded version) is mask
		return number==0;
	case Instruction::Operation::X86_INS_VGATHERPF0DPS:
	case Instruction::Operation::X86_INS_VGATHERPF0QPS:
	case Instruction::Operation::X86_INS_VGATHERPF1DPS:
	case Instruction::Operation::X86_INS_VGATHERPF1QPS:
	case Instruction::Operation::X86_INS_VSCATTERPF0DPS:
	case Instruction::Operation::X86_INS_VSCATTERPF0QPS:
	case Instruction::Operation::X86_INS_VSCATTERPF1DPS:
	case Instruction::Operation::X86_INS_VSCATTERPF1QPS:
		return false; // VSIB is not quite PS
	case Instruction::Operation::X86_INS_VSCATTERDPS:
	case Instruction::Operation::X86_INS_VSCATTERQPS:
		return number==1; // first operand is VSIB, it's not quite PS

	case Instruction::Operation:: X86_INS_CVTDQ2PS:
	case Instruction::Operation::X86_INS_VCVTDQ2PS:
	case Instruction::Operation:: X86_INS_CVTPD2PS:
	case Instruction::Operation::X86_INS_VCVTPD2PS:
	case Instruction::Operation::X86_INS_CVTPI2PS:
	case Instruction::Operation::X86_INS_VCVTPH2PS:
	case Instruction::Operation::X86_INS_VCVTUDQ2PS:
		return number==0;
	case Instruction::Operation:: X86_INS_CVTPS2DQ:
	case Instruction::Operation::X86_INS_VCVTPS2DQ:
	case Instruction::Operation:: X86_INS_CVTPS2PD:
	case Instruction::Operation::X86_INS_VCVTPS2PD:
	case Instruction::Operation:: X86_INS_CVTPS2PI:
	case Instruction::Operation::X86_INS_VCVTPS2PH:
	//case Instruction::Operation::X86_INS_VCVTPS2QQ: // FIXME: Capstone seems to not support it
	case Instruction::Operation::X86_INS_VCVTPS2UDQ:
	//case Instruction::Operation::X86_INS_VCVTPS2UQQ: // FIXME: Capstone seems to not support it
		return number==1;
	case Instruction::Operation::X86_INS_BLENDVPS:  // third operand (<XMM0> in docs) is mask
		return number!=2;
	case Instruction::Operation::X86_INS_VBLENDVPS: // fourth operand (xmm4 in docs) is mask
		return number!=3;
	case Instruction::Operation::X86_INS_VMASKMOVPS:// second (but not third) operand is mask
		return number!=1;
	case Instruction::Operation::X86_INS_VPERMI2PS: // first operand is indices
		return number!=0;
	case Instruction::Operation::X86_INS_VPERMILPS: // third operand is control (can be [xyz]mm register or imm8)
		return number!=2;
	case Instruction::Operation::X86_INS_VPERMT2PS: // second operand is indices
	case Instruction::Operation::X86_INS_VPERMPS:   // second operand is indices
		return number!=1;
	case Instruction::Operation::X86_INS_VPERMIL2PS: // XOP (AMD). Fourth operand is selector
		return number!=3;
	case Instruction::Operation::X86_INS_VRCPSS:
	case Instruction::Operation::X86_INS_VRCP14SS:
	case Instruction::Operation::X86_INS_VRCP28SS:
	case Instruction::Operation::X86_INS_VROUNDSS:
	case Instruction::Operation::X86_INS_VRSQRTSS:
	case Instruction::Operation::X86_INS_VSQRTSS:
	case Instruction::Operation::X86_INS_VRNDSCALESS:
	case Instruction::Operation::X86_INS_VRSQRT14SS:
	case Instruction::Operation::X86_INS_VRSQRT28SS:
	case Instruction::Operation::X86_INS_VCVTSD2SS:
	case Instruction::Operation::X86_INS_VCVTSI2SS:
	case Instruction::Operation::X86_INS_VCVTUSI2SS:
	// These are SS, but high PS are copied from second operand to first.
	// I.e. second operand is PS, and thus first one (destination) is also PS.
	// Only third operand is actually SS.
		return number<2;
	case Instruction::Operation::X86_INS_VBROADCASTSS: // dest is PS, src is SS
		return number==0;

	default:
		return false;
	}
}

bool Operand::is_SIMD_PD() const
{
	if(apriori_not_simd()) return false;

	const auto number=simdOperandNormalizedNumberInInstruction();

	switch(owner()->operation())
	{
	case Instruction::Operation::X86_INS_ADDPD:
	case Instruction::Operation::X86_INS_VADDPD:
	case Instruction::Operation::X86_INS_ADDSUBPD:
	case Instruction::Operation::X86_INS_VADDSUBPD:
	case Instruction::Operation::X86_INS_ANDNPD:
	case Instruction::Operation::X86_INS_VANDNPD:
	case Instruction::Operation::X86_INS_ANDPD:
	case Instruction::Operation::X86_INS_VANDPD:
	case Instruction::Operation:: X86_INS_BLENDPD:   // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VBLENDPD:   // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_CMPPD:     // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VCMPPD:     // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_DIVPD:
	case Instruction::Operation::X86_INS_VDIVPD:
	case Instruction::Operation:: X86_INS_DPPD:      // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VDPPD:      // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_MOVAPD:
	case Instruction::Operation::X86_INS_VMOVAPD:
	case Instruction::Operation:: X86_INS_ORPD:
	case Instruction::Operation::X86_INS_VORPD:
	case Instruction::Operation:: X86_INS_XORPD:
	case Instruction::Operation::X86_INS_VXORPD:
	case Instruction::Operation:: X86_INS_HADDPD:
	case Instruction::Operation::X86_INS_VHADDPD:
	case Instruction::Operation:: X86_INS_HSUBPD:
	case Instruction::Operation::X86_INS_VHSUBPD:
	case Instruction::Operation:: X86_INS_MAXPD:
	case Instruction::Operation::X86_INS_VMAXPD:
	case Instruction::Operation:: X86_INS_MINPD:
	case Instruction::Operation::X86_INS_VMINPD:
	case Instruction::Operation:: X86_INS_MOVHPD:
	case Instruction::Operation::X86_INS_VMOVHPD:
	case Instruction::Operation:: X86_INS_MOVLPD:
	case Instruction::Operation::X86_INS_VMOVLPD:
	case Instruction::Operation:: X86_INS_MOVMSKPD: // GPR isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VMOVMSKPD: // GPR isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_MOVNTPD:
	case Instruction::Operation::X86_INS_VMOVNTPD:
	case Instruction::Operation:: X86_INS_MOVUPD:
	case Instruction::Operation::X86_INS_VMOVUPD:
	case Instruction::Operation:: X86_INS_MULPD:
	case Instruction::Operation::X86_INS_VMULPD:
	case Instruction::Operation:: X86_INS_ROUNDPD: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VROUNDPD: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_SHUFPD:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VSHUFPD:  // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_SQRTPD:
	case Instruction::Operation::X86_INS_VSQRTPD:
	case Instruction::Operation:: X86_INS_SUBPD:
	case Instruction::Operation::X86_INS_VSUBPD:
	case Instruction::Operation:: X86_INS_UNPCKHPD:
	case Instruction::Operation::X86_INS_VUNPCKHPD:
	case Instruction::Operation:: X86_INS_UNPCKLPD:
	case Instruction::Operation::X86_INS_VUNPCKLPD:
	case Instruction::Operation::X86_INS_VBLENDMPD:
#if CS_API_MAJOR>=4
	case Instruction::Operation::X86_INS_VCOMPRESSPD:
	case Instruction::Operation::X86_INS_VEXP2PD:
	case Instruction::Operation::X86_INS_VEXPANDPD:
#endif
	case Instruction::Operation::X86_INS_VFMADD132PD:
	case Instruction::Operation::X86_INS_VFMADD213PD:
	case Instruction::Operation::X86_INS_VFMADD231PD:
	case Instruction::Operation::X86_INS_VFMADDPD: 		 // FMA4 (AMD). XMM/YMM/mem operands only (?)
	case Instruction::Operation::X86_INS_VFMADDSUB132PD:
	case Instruction::Operation::X86_INS_VFMADDSUB213PD:
	case Instruction::Operation::X86_INS_VFMADDSUB231PD:
	case Instruction::Operation::X86_INS_VFMADDSUBPD:
	case Instruction::Operation::X86_INS_VFMSUB132PD:
	case Instruction::Operation::X86_INS_VFMSUBADD132PD:
	case Instruction::Operation::X86_INS_VFMSUBADD213PD:
	case Instruction::Operation::X86_INS_VFMSUBADD231PD:
	case Instruction::Operation::X86_INS_VFMSUBADDPD: 	 // FMA4 (AMD). Seems to have only XMM/YMM/mem operands (?)
	case Instruction::Operation::X86_INS_VFMSUBPD: 		 // FMA4?
	case Instruction::Operation::X86_INS_VFMSUB213PD:
	case Instruction::Operation::X86_INS_VFMSUB231PD:
	case Instruction::Operation::X86_INS_VFNMADD132PD:
	case Instruction::Operation::X86_INS_VFNMADDPD: 		 // FMA4?
	case Instruction::Operation::X86_INS_VFNMADD213PD:
	case Instruction::Operation::X86_INS_VFNMADD231PD:
	case Instruction::Operation::X86_INS_VFNMSUB132PD:
	case Instruction::Operation::X86_INS_VFNMSUBPD: 		 // FMA4?
	case Instruction::Operation::X86_INS_VFNMSUB213PD:
	case Instruction::Operation::X86_INS_VFNMSUB231PD:
	case Instruction::Operation::X86_INS_VFRCZPD:
	case Instruction::Operation::X86_INS_VRCP14PD:
	case Instruction::Operation::X86_INS_VRCP28PD:
	case Instruction::Operation::X86_INS_VRNDSCALEPD:
	case Instruction::Operation::X86_INS_VRSQRT14PD:
	case Instruction::Operation::X86_INS_VRSQRT28PD:
	case Instruction::Operation::X86_INS_VTESTPD:
		return true;

	case Instruction::Operation::X86_INS_VGATHERDPD:
	case Instruction::Operation::X86_INS_VGATHERQPD:
	// second operand is VSIB, it's not quite PD;
	// third operand, if present (VEX-encoded version) is mask
		return number==0;
	case Instruction::Operation::X86_INS_VGATHERPF0DPD:
	case Instruction::Operation::X86_INS_VGATHERPF0QPD:
	case Instruction::Operation::X86_INS_VGATHERPF1DPD:
	case Instruction::Operation::X86_INS_VGATHERPF1QPD:
	case Instruction::Operation::X86_INS_VSCATTERPF0DPD:
	case Instruction::Operation::X86_INS_VSCATTERPF0QPD:
	case Instruction::Operation::X86_INS_VSCATTERPF1DPD:
	case Instruction::Operation::X86_INS_VSCATTERPF1QPD:
		return false; // VSIB is not quite PD
	case Instruction::Operation::X86_INS_VSCATTERDPD:
	case Instruction::Operation::X86_INS_VSCATTERQPD:
		return number==1; // first operand is VSIB, it's not quite PD

	case Instruction::Operation:: X86_INS_CVTDQ2PD:
	case Instruction::Operation::X86_INS_VCVTDQ2PD:
	case Instruction::Operation:: X86_INS_CVTPS2PD:
	case Instruction::Operation::X86_INS_VCVTPS2PD:
	case Instruction::Operation::X86_INS_CVTPI2PD:
	case Instruction::Operation::X86_INS_VCVTUDQ2PD:
		return number==0;
	case Instruction::Operation:: X86_INS_CVTPD2DQ:
	case Instruction::Operation::X86_INS_VCVTPD2DQ:
	case Instruction::Operation:: X86_INS_CVTPD2PI:
	case Instruction::Operation:: X86_INS_CVTPD2PS:
	case Instruction::Operation::X86_INS_VCVTPD2PS:
	case Instruction::Operation::X86_INS_VCVTPD2DQX: // FIXME: what's this?
	case Instruction::Operation::X86_INS_VCVTPD2PSX: // FIXME: what's this?
	//case Instruction::Operation::X86_INS_VCVTPD2QQ: // FIXME: Capstone seems to not support it
	case Instruction::Operation::X86_INS_VCVTPD2UDQ:
	//case Instruction::Operation::X86_INS_VCVTPD2UQQ: // FIXME: Capstone seems to not support it
		return number==1;
	case Instruction::Operation:: X86_INS_BLENDVPD: // third operand is mask
		return number!=2;
	case Instruction::Operation::X86_INS_VBLENDVPD: // fourth operand is mask
		return number!=3;
	case Instruction::Operation::X86_INS_VMASKMOVPD:// second (but not third) operand is mask
		return number!=1;
	case Instruction::Operation::X86_INS_VPERMI2PD: // first operand is indices
		return number!=0;
	case Instruction::Operation::X86_INS_VPERMT2PD: // second operand is indices
		return number!=1;
	case Instruction::Operation::X86_INS_VPERMILPD: // third operand is control (can be [xyz]mm register or imm8)
		return number!=2;
	case Instruction::Operation::X86_INS_VPERMPD: // if third operand is not imm8, then second is indices (always in VPERMPS)
		assert(owner()->operand_count()==3);
		if(owner()->operands()[2].general_type()!=TYPE_IMMEDIATE)
			return number!=1;
		else return true;
	case Instruction::Operation::X86_INS_VPERMIL2PD: // XOP (AMD). Fourth operand is selector (?)
		return number!=3;
	case Instruction::Operation::X86_INS_VRCP14SD:
	case Instruction::Operation::X86_INS_VRCP28SD:
	case Instruction::Operation::X86_INS_VROUNDSD:
	case Instruction::Operation::X86_INS_VSQRTSD:
	case Instruction::Operation::X86_INS_VRNDSCALESD:
	case Instruction::Operation::X86_INS_VRSQRT14SD:
	case Instruction::Operation::X86_INS_VRSQRT28SD:
	case Instruction::Operation::X86_INS_VCVTSS2SD:
	case Instruction::Operation::X86_INS_VCVTSI2SD:
	case Instruction::Operation::X86_INS_VCVTUSI2SD:
	// These are SD, but high PD are copied from second operand to first.
	// I.e. second operand is PD, and thus first one (destination) is also PD.
	// Only third operand is actually SD.
		return number<2;
	case Instruction::Operation::X86_INS_VBROADCASTSD: // dest is PD, src is SD
		return number==0;
	default:
		return false;
	}
}

bool Operand::is_SIMD_SS() const
{
	if(apriori_not_simd()) return false;

	const auto number=simdOperandNormalizedNumberInInstruction();

	switch(owner()->operation())
	{
	case Instruction::Operation:: X86_INS_ADDSS:
	case Instruction::Operation::X86_INS_VADDSS:
	case Instruction::Operation:: X86_INS_CMPSS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VCMPSS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_COMISS:
	case Instruction::Operation::X86_INS_VCOMISS:
	case Instruction::Operation:: X86_INS_DIVSS:
	case Instruction::Operation::X86_INS_VDIVSS:
	case Instruction::Operation:: X86_INS_UCOMISS:
	case Instruction::Operation::X86_INS_VUCOMISS:
	case Instruction::Operation:: X86_INS_MAXSS:
	case Instruction::Operation::X86_INS_VMAXSS:
	case Instruction::Operation:: X86_INS_MINSS:
	case Instruction::Operation::X86_INS_VMINSS:
	case Instruction::Operation:: X86_INS_MOVNTSS: // SSE4a (AMD)
	case Instruction::Operation:: X86_INS_MOVSS:
	case Instruction::Operation::X86_INS_VMOVSS:
	case Instruction::Operation:: X86_INS_MULSS:
	case Instruction::Operation::X86_INS_VMULSS:
	case Instruction::Operation:: X86_INS_RCPSS:   // SS, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_RSQRTSS: // SS, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_ROUNDSS: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_SQRTSS:  // SS, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_SUBSS:
	case Instruction::Operation::X86_INS_VSUBSS:
	case Instruction::Operation::X86_INS_VFMADD213SS:
	case Instruction::Operation::X86_INS_VFMADD132SS:
	case Instruction::Operation::X86_INS_VFMADD231SS:
	case Instruction::Operation::X86_INS_VFMADDSS: 		// AMD?
	case Instruction::Operation::X86_INS_VFMSUB213SS:
	case Instruction::Operation::X86_INS_VFMSUB132SS:
	case Instruction::Operation::X86_INS_VFMSUB231SS:
	case Instruction::Operation::X86_INS_VFMSUBSS: 		// AMD?
	case Instruction::Operation::X86_INS_VFNMADD213SS:
	case Instruction::Operation::X86_INS_VFNMADD132SS:
	case Instruction::Operation::X86_INS_VFNMADD231SS:
	case Instruction::Operation::X86_INS_VFNMADDSS: 	// AMD?
	case Instruction::Operation::X86_INS_VFNMSUB213SS:
	case Instruction::Operation::X86_INS_VFNMSUB132SS:
	case Instruction::Operation::X86_INS_VFNMSUB231SS:
	case Instruction::Operation::X86_INS_VFNMSUBSS: 	// AMD?
	case Instruction::Operation::X86_INS_VFRCZSS: 		// AMD?
		return true;

	case Instruction::Operation::X86_INS_VCVTSS2SD:
		return number==2;
	case Instruction::Operation:: X86_INS_CVTSS2SD:
	case Instruction::Operation:: X86_INS_CVTSS2SI:
	case Instruction::Operation::X86_INS_VCVTSS2SI:
	case Instruction::Operation::X86_INS_VCVTSS2USI:
		return number==1;
	case Instruction::Operation:: X86_INS_CVTSD2SS: // SS, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_CVTSI2SS: // SS, unlike VEX-encoded version
		return number==0;
	case Instruction::Operation::X86_INS_VCVTSD2SS:
	case Instruction::Operation::X86_INS_VCVTSI2SS:
	case Instruction::Operation::X86_INS_VCVTUSI2SS:
	// These instructions are SS, but high PS are copied from second operand to first,
	// so second operand is PS, thus first too. Third operand is not SS by its meaning.
		return false;
	case Instruction::Operation::X86_INS_VRCPSS:
	case Instruction::Operation::X86_INS_VRCP14SS:
	case Instruction::Operation::X86_INS_VRCP28SS:
	case Instruction::Operation::X86_INS_VROUNDSS:
	case Instruction::Operation::X86_INS_VRSQRTSS:
	case Instruction::Operation::X86_INS_VSQRTSS:
	case Instruction::Operation::X86_INS_VRNDSCALESS:
	case Instruction::Operation::X86_INS_VRSQRT14SS:
	case Instruction::Operation::X86_INS_VRSQRT28SS:
	// These are SS, but high PS are copied from second operand to first.
	// I.e. second operand is PS, and thus first one (destination) is also PS.
	// Only third operand is actually SS.
		return number==2;
	case Instruction::Operation::X86_INS_VBROADCASTSS: // dest is PS, src is SS
		return number==1;
	default:
		return false;
	}
}

bool Operand::is_SIMD_SD() const
{
	if(apriori_not_simd()) return false;

	const auto number=simdOperandNormalizedNumberInInstruction();

	switch(owner()->operation())
	{
	case Instruction::Operation:: X86_INS_ADDSD:
	case Instruction::Operation::X86_INS_VADDSD:
	case Instruction::Operation:: X86_INS_CMPSD: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation::X86_INS_VCMPSD: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_COMISD:
	case Instruction::Operation::X86_INS_VCOMISD:
	case Instruction::Operation:: X86_INS_DIVSD:
	case Instruction::Operation::X86_INS_VDIVSD:
	case Instruction::Operation:: X86_INS_UCOMISD:
	case Instruction::Operation::X86_INS_VUCOMISD:
	case Instruction::Operation:: X86_INS_MAXSD:
	case Instruction::Operation::X86_INS_VMAXSD:
	case Instruction::Operation:: X86_INS_MINSD:
	case Instruction::Operation::X86_INS_VMINSD:
	case Instruction::Operation:: X86_INS_MOVNTSD: // SSE4a (AMD)
	case Instruction::Operation:: X86_INS_MOVSD:
	case Instruction::Operation::X86_INS_VMOVSD:
	case Instruction::Operation:: X86_INS_MULSD:
	case Instruction::Operation::X86_INS_VMULSD:
	case Instruction::Operation:: X86_INS_ROUNDSD: // imm8 isn't a SIMD reg, so ok
	case Instruction::Operation:: X86_INS_SQRTSD:  // SD, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_SUBSD:
	case Instruction::Operation::X86_INS_VSUBSD:
	case Instruction::Operation::X86_INS_VFMADD213SD:
	case Instruction::Operation::X86_INS_VFMADD132SD:
	case Instruction::Operation::X86_INS_VFMADD231SD:
	case Instruction::Operation::X86_INS_VFMADDSD: 		// AMD?
	case Instruction::Operation::X86_INS_VFMSUB213SD:
	case Instruction::Operation::X86_INS_VFMSUB132SD:
	case Instruction::Operation::X86_INS_VFMSUB231SD:
	case Instruction::Operation::X86_INS_VFMSUBSD: 		// AMD?
	case Instruction::Operation::X86_INS_VFNMADD213SD:
	case Instruction::Operation::X86_INS_VFNMADD132SD:
	case Instruction::Operation::X86_INS_VFNMADD231SD:
	case Instruction::Operation::X86_INS_VFNMADDSD: 	// AMD?
	case Instruction::Operation::X86_INS_VFNMSUB213SD:
	case Instruction::Operation::X86_INS_VFNMSUB132SD:
	case Instruction::Operation::X86_INS_VFNMSUB231SD:
	case Instruction::Operation::X86_INS_VFNMSUBSD: 	// AMD?
	case Instruction::Operation::X86_INS_VFRCZSD: 		// AMD?
		return true;

	case Instruction::Operation::X86_INS_VCVTSD2SS:
		return number==2;
	case Instruction::Operation:: X86_INS_CVTSD2SS:
	case Instruction::Operation:: X86_INS_CVTSD2SI:
	case Instruction::Operation::X86_INS_VCVTSD2SI:
	case Instruction::Operation::X86_INS_VCVTSD2USI:
		return number==1;
	case Instruction::Operation:: X86_INS_CVTSS2SD: // SD, unlike VEX-encoded version
	case Instruction::Operation:: X86_INS_CVTSI2SD: // SD, unlike VEX-encoded version
		return number==0;
	case Instruction::Operation::X86_INS_VCVTSS2SD:
	case Instruction::Operation::X86_INS_VCVTSI2SD:
	case Instruction::Operation::X86_INS_VCVTUSI2SD:
	// These instructions are SD, but high PD are copied from second operand to first,
	// so second operand is PD, thus first too. Third operand is not SD by its meaning.
		return false;
	case Instruction::Operation::X86_INS_VRCP14SD:
	case Instruction::Operation::X86_INS_VRCP28SD:
	case Instruction::Operation::X86_INS_VROUNDSD:
	case Instruction::Operation::X86_INS_VSQRTSD:
	case Instruction::Operation::X86_INS_VRNDSCALESD:
	case Instruction::Operation::X86_INS_VRSQRT14SD:
	case Instruction::Operation::X86_INS_VRSQRT28SD:
	// These are SD, but high PD are copied from second operand to first.
	// I.e. second operand is PD, and thus first one (destination) is also PD.
	// Only third operand is actually SD.
		return number==2;
	case Instruction::Operation::X86_INS_VBROADCASTSD: // dest is PD, src is SD
		return number==1;
	default:
		return false;
	}
}

}
