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

static bool capstoneInitialized=false;
static Capstone::csh csh=0;

bool CapstoneEDB::init(bool amd64)
{
    if(capstoneInitialized)
		Capstone::cs_close(&csh);
	capstoneInitialized=false;

	Capstone::cs_mode mode=Capstone::CS_MODE_32;
	if(amd64) mode=Capstone::CS_MODE_64;
	Capstone::cs_err result=Capstone::cs_open(Capstone::CS_ARCH_X86, mode, &csh);
	if(result!=Capstone::CS_ERR_OK)
		return false;

	Capstone::cs_option(csh, Capstone::CS_OPT_DETAIL, Capstone::CS_OPT_ON);

    capstoneInitialized=true;
	return true;
}

void CapstoneEDB::Instruction::fillPrefix()
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

CapstoneEDB::Instruction& CapstoneEDB::Instruction::operator=(const CapstoneEDB::Instruction& other)
{
	static_assert(std::is_standard_layout<Instruction>::value,"Instruction should have standard layout");
	std::memcpy(this,&other,sizeof other);
	// Update pointer after replacement
	insn_.detail=&detail_;

	return *this;
}

CapstoneEDB::Instruction::Instruction(const CapstoneEDB::Instruction& other)
{
	*this=other;
}

CapstoneEDB::Instruction::Instruction(const void* first, const void* last, uint64_t rva, const std::nothrow_t&) throw()
{
	assert(capstoneInitialized);
    const uint8_t* codeBegin=static_cast<const uint8_t*>(first);
    const uint8_t* codeEnd=static_cast<const uint8_t*>(last);
	assert(last>first);
	rva_=rva;
	firstByte_=codeBegin[0];

	Capstone::cs_insn* insn=nullptr;
    if(Capstone::cs_disasm(csh, codeBegin, codeEnd-codeBegin-1, rva, 1, &insn))
	{
		valid_=true;
		std::memcpy(&insn_,insn,sizeof insn_);
		std::memcpy(&detail_,insn->detail, sizeof detail_);
		insn_.detail=&detail_;
		Capstone::cs_free(insn,1);
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
				if(ops[i].mem.disp!=0) // FIXME: this doesn't catch zero-valued existing displacements!
				{
					operand.expr_.displacement_type=Operand::DISP_S32; // FIXME: we don't have a good way to correctly get size
					operand.expr_.s_disp32=ops[i].mem.disp; // FIXME: truncation or wrong type chosen by capstone?..
				}
				operand.expr_.base =static_cast<Operand::Register>(ops[i].mem.base);
				operand.expr_.index=static_cast<Operand::Register>(ops[i].mem.index);
				operand.expr_.scale=ops[i].mem.scale;
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
		insn_.bytes[0]=firstByte_;
		insn_.size=1;
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

const uint8_t* CapstoneEDB::Instruction::bytes() const
{
	return insn_.bytes;
}

std::size_t CapstoneEDB::Instruction::operand_count() const
{
	return insn_.detail->x86.op_count;
}

std::size_t CapstoneEDB::Instruction::size() const
{
	return insn_.size;
}

CapstoneEDB::Instruction::Operation CapstoneEDB::Instruction::operation() const
{
	return static_cast<Operation>(insn_.id);
}

std::string CapstoneEDB::Instruction::mnemonic() const
{
	return insn_.mnemonic;
}
CapstoneEDB::Instruction::ConditionCode CapstoneEDB::Instruction::condition_code() const
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

bool CapstoneEDB::Instruction::is_terminator() const
{
	return is_halt() || is_jump() || is_return();
}

bool CapstoneEDB::Instruction::is_halt() const
{
	if(!valid_) return false;

	return operation()==Operation::X86_INS_HLT;
}

void CapstoneEDB::Instruction::swap(Instruction &other)
{
	std::swap(*this,other);
}

bool CapstoneEDB::Instruction::is_conditional_set() const
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

bool CapstoneEDB::Instruction::is_unconditional_jump() const
{
	return operation()==Operation::X86_INS_JMP || operation()==Operation::X86_INS_LJMP;
}

bool CapstoneEDB::Instruction::is_conditional_jump() const
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

bool CapstoneEDB::Instruction::is_conditional_fpu_move() const
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

bool CapstoneEDB::Instruction::is_conditional_gpr_move() const
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

bool CapstoneEDB::Instruction::is_syscall() const
{
	return operation()==Operation::X86_INS_SYSCALL;
}

bool CapstoneEDB::Instruction::is_call() const
{
	return operation()==Operation::X86_INS_CALL;
}

bool CapstoneEDB::Instruction::is_return() const
{
	if(!valid_) return false;

	return Capstone::cs_insn_group(csh, &insn_, Capstone::X86_GRP_RET);
}

bool CapstoneEDB::Instruction::is_interrupt() const
{
	const auto op=operation();
	return op==Operation::X86_INS_INT ||
		   op==Operation::X86_INS_INT1 ||
		   op==Operation::X86_INS_INT3 ||
		   op==Operation::X86_INS_INTO;
}

bool CapstoneEDB::Instruction::is_ret() const
{
	return operation()==Operation::X86_INS_RET;
}

bool CapstoneEDB::Instruction::is_int() const
{
	return operation()==Operation::X86_INS_INT;
}

bool CapstoneEDB::Instruction::is_jump() const
{
	if(!valid_) return false;

	return Capstone::cs_insn_group(csh, &insn_, Capstone::X86_GRP_JUMP);
}

bool CapstoneEDB::Instruction::is_nop() const
{
	if(!valid_) return false;

	return operation()==Operation::X86_INS_NOP;
}

void CapstoneEDB::Formatter::setOptions(const CapstoneEDB::Formatter::FormatOptions& options)
{
	assert(capstoneInitialized);
	options_=options;
	if(options.syntax==SyntaxATT)
		Capstone::cs_option(csh,Capstone::CS_OPT_SYNTAX, Capstone::CS_OPT_SYNTAX_ATT);
	else
		Capstone::cs_option(csh,Capstone::CS_OPT_SYNTAX, Capstone::CS_OPT_SYNTAX_INTEL);
	// FIXME: SmallNumberFormat is not yet supported
}

std::string CapstoneEDB::Formatter::to_string(const CapstoneEDB::Instruction& instruction) const
{
	if(!instruction) return "(bad)";

	std::string str(std::string(instruction.insn_.mnemonic)+" "+instruction.insn_.op_str);
	checkCapitalize(str);
	return str;
}

void CapstoneEDB::Formatter::checkCapitalize(std::string& str) const
{
	// FIXME: undo capitalization of 0x prefix
	if(options_.capitalization==UpperCase)
		std::transform(str.begin(),str.end(),str.begin(), ::toupper);
}

std::string CapstoneEDB::Formatter::to_string(const CapstoneEDB::Operand& operand) const
{
	if(!operand) return "(bad)";

	std::string str;

	if(operand.type_==Operand::TYPE_REGISTER)
		str=register_name(operand.reg());
	else if(operand.owner()->operand_count()==1)
		str=operand.owner()->insn_.op_str;
	else
	{
		// XXX: Capstone doesn't provide a way to get operand string. We might try to split op_str
		// by ',', but in AT&T syntax this can be easily confused with SIB syntax - (s,i,b) or whatever it is.
		str="(formatting of non-register operands not implemented)";
	}

	checkCapitalize(str);
	return str;
}

std::string CapstoneEDB::Formatter::register_name(const CapstoneEDB::Operand::Register reg) const
{
	assert(capstoneInitialized);
	const char* raw=Capstone::cs_reg_name(csh, reg);
	if(!raw)
		return "(invalid register)";
	std::string str(raw);
	checkCapitalize(str);
	return str;
}
