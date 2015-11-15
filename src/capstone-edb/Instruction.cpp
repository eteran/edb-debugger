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

	strcpy(insn.op_str,operands.toStdString().c_str());
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
			assert(operand_count()==2);
			Operand operand(this,0);
			operand.type_=Operand::TYPE_ABSOLUTE;
			operand.abs_.seg=ops[0].imm;
			operand.abs_.offset=ops[1].imm;
		}
		else for(std::size_t i=0;i<operand_count();++i)
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
			case Capstone::X86_OP_FP: // FIXME: what instructions have this?
				break;
			case Capstone::X86_OP_INVALID:
				break;
			}

			operands_.push_back(operand);
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

std::size_t Instruction::operand_count() const
{
	return insn_.detail->x86.op_count;
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

	std::string str(std::string(instruction.insn_.mnemonic)+" "+instruction.insn_.op_str);
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

}
