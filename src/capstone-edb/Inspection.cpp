
#include "Instruction.h"
#include "util/Container.h"

namespace CapstoneEDB {

bool is_repeat(const Instruction &insn) {
	if (!insn)
		return false;
	const auto &prefixes = insn->detail->x86.prefix;
	return (prefixes[0] == X86_PREFIX_REP || prefixes[0] == X86_PREFIX_REPNE);
}

bool is_terminator(const Instruction &insn) {
	return is_halt(insn) || is_jump(insn) || is_return(insn);
}

bool is_register(const Operand &operand) {
	return operand && static_cast<cs_op_type>(operand->type) == CS_OP_REG;
}

bool is_expression(const Operand &operand) {
	return operand && static_cast<cs_op_type>(operand->type) == CS_OP_MEM;
}

bool is_immediate(const Operand &operand) {
	return operand && static_cast<cs_op_type>(operand->type) == CS_OP_IMM;
}

bool is_halt(const Instruction &insn) {
	return insn && insn.operation() == X86_INS_HLT;
}

bool is_sysenter(const Instruction &insn) {
	return insn && insn.operation() == X86_INS_SYSENTER;
}

bool is_syscall(const Instruction &insn) {
	return insn && insn.operation() == X86_INS_SYSCALL;
}

bool is_interrupt(const Instruction &insn) {
	if (!insn) return false;
	const int op = insn.operation();
	return op == X86_INS_INT || op == X86_INS_INT1 || op == X86_INS_INT3 || op == X86_INS_INTO;
}

bool is_ret(const Instruction &insn) {
	return insn && insn.operation() == X86_INS_RET;
}

bool is_int(const Instruction &insn) {
	return insn && insn.operation() == X86_INS_INT;
}

bool is_nop(const Instruction &insn) {
	if (!insn) return false;
	return insn.operation() == X86_INS_NOP;
}

bool is_conditional_set(const Instruction &insn) {
	if (!insn) return false;

	switch (insn.operation()) {
	case X86_INS_SETAE:
	case X86_INS_SETA:
	case X86_INS_SETBE:
	case X86_INS_SETB:
	case X86_INS_SETE:
	case X86_INS_SETGE:
	case X86_INS_SETG:
	case X86_INS_SETLE:
	case X86_INS_SETL:
	case X86_INS_SETNE:
	case X86_INS_SETNO:
	case X86_INS_SETNP:
	case X86_INS_SETNS:
	case X86_INS_SETO:
	case X86_INS_SETP:
	case X86_INS_SETS:
		return true;
	default:
		return false;
	}
}

bool is_unconditional_jump(const Instruction &insn) {
	return insn && (insn.operation() == X86_INS_JMP || insn.operation() == X86_INS_LJMP);
}

bool is_conditional_jump(const Instruction &insn) {
	if (!insn) return false;

	switch (insn.operation()) {
	case X86_INS_JAE:
	case X86_INS_JA:
	case X86_INS_JBE:
	case X86_INS_JB:
	case X86_INS_JCXZ:
	case X86_INS_JECXZ:
	case X86_INS_JE:
	case X86_INS_JGE:
	case X86_INS_JG:
	case X86_INS_JLE:
	case X86_INS_JL:
	case X86_INS_JNE:
	case X86_INS_JNO:
	case X86_INS_JNP:
	case X86_INS_JNS:
	case X86_INS_JO:
	case X86_INS_JP:
	case X86_INS_JRCXZ:
	case X86_INS_JS:
		return true;
	default:
		return false;
	}
}

bool is_conditional_fpu_move(const Instruction &insn) {
	if (!insn) return false;

	switch (insn.operation()) {
	case X86_INS_FCMOVBE:
	case X86_INS_FCMOVB:
	case X86_INS_FCMOVE:
	case X86_INS_FCMOVNBE:
	case X86_INS_FCMOVNB:
	case X86_INS_FCMOVNE:
	case X86_INS_FCMOVNU:
	case X86_INS_FCMOVU:
		return true;
	default:
		return false;
	}
}

bool is_conditional_gpr_move(const Instruction &insn) {
	if (!insn) return false;

	switch (insn.operation()) {
	case X86_INS_CMOVA:
	case X86_INS_CMOVAE:
	case X86_INS_CMOVB:
	case X86_INS_CMOVBE:
	case X86_INS_CMOVE:
	case X86_INS_CMOVG:
	case X86_INS_CMOVGE:
	case X86_INS_CMOVL:
	case X86_INS_CMOVLE:
	case X86_INS_CMOVNE:
	case X86_INS_CMOVNO:
	case X86_INS_CMOVNP:
	case X86_INS_CMOVNS:
	case X86_INS_CMOVO:
	case X86_INS_CMOVP:
	case X86_INS_CMOVS:
		return true;
	default:
		return false;
	}
}

bool is_fpu(const Instruction &insn) {
	return insn && ((insn->detail->x86.opcode[0] & 0xd8) == 0xd8);
}

bool is_conditional_move(const Instruction &insn) {
	return is_conditional_fpu_move(insn) || is_conditional_gpr_move(insn);
}

bool is_fpu_taking_float(const Instruction &insn) {
	if (!insn)
		return false;

	if (!is_fpu(insn))
		return false;

	const auto modrm = insn->detail->x86.modrm;

	if (modrm > 0xbf)
		return true; // always works with st(i) in this case

	const auto ro = (modrm >> 3) & 7;
	switch (insn->detail->x86.opcode[0]) {
	case 0xd8:
	case 0xdc:
		return true;
	case 0xdb:
		return ro == 5 || ro == 7;
	case 0xd9:
	case 0xdd:
		return ro == 0 || ro == 2 || ro == 3;
	default:
		return false;
	}
}

bool is_fpu_taking_integer(const Instruction &insn) {
	if (!insn)
		return false;
	if (!is_fpu(insn))
		return false;

	const auto modrm = insn->detail->x86.modrm;

	if (modrm > 0xbf)
		return false; // always works with st(i) in this case

	const auto ro = (modrm >> 3) & 7;

	switch (insn->detail->x86.opcode[0]) {
	case 0xda:
		return true;
	case 0xdb:
		return 0 <= ro && ro <= 3;
	case 0xdd:
		return ro == 1;
	case 0xde:
		return true;
	case 0xdf:
		return (0 <= ro && ro <= 3) || ro == 5 || ro == 7;
	default:
		return false;
	}
}

bool is_fpu_taking_bcd(const Instruction &insn) {
	if (!insn)
		return false;
	if (!is_fpu(insn))
		return false;

	const auto modrm = insn->detail->x86.modrm;
	if (modrm > 0xbf)
		return false; // always works with st(i) in this case

	const auto ro = (modrm >> 3) & 7;
	return insn->detail->x86.opcode[0] == 0xdf && (ro == 4 || ro == 6);
}

bool is_simd(const Instruction &insn) {
	if (!insn)
		return false;

	const x86_insn_group simdGroups[] = {
		X86_GRP_3DNOW,
		X86_GRP_AVX,
		X86_GRP_AVX2,
		X86_GRP_AVX512,
		X86_GRP_FMA,
		X86_GRP_FMA4,
		X86_GRP_MMX,
		X86_GRP_SSE1,
		X86_GRP_SSE2,
		X86_GRP_SSE3,
		X86_GRP_SSE41,
		X86_GRP_SSE42,
		X86_GRP_SSE4A,
		X86_GRP_SSSE3,
		X86_GRP_XOP,
		X86_GRP_CDI,
		X86_GRP_ERI,
		X86_GRP_PFI,
		X86_GRP_VLX,
		X86_GRP_NOVLX,
	};

	for (auto g = 0; g < insn->detail->groups_count; ++g) {
		if (util::contains(simdGroups, insn->detail->groups[g])) {
			return true;
		}
	}

	return false;
}

}
