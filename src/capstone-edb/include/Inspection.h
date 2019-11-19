
#ifndef INSPECTION_H_20191119_
#define INSPECTION_H_20191119_

#include "API.h"

namespace CapstoneEDB {

class Formatter;
class Instruction;
class Operand;

EDB_EXPORT bool modifies_pc(const Instruction &insn);
EDB_EXPORT bool is_call(const Instruction &insn);
EDB_EXPORT bool is_conditional_jump(const Instruction &insn);
EDB_EXPORT bool is_halt(const Instruction &insn);
EDB_EXPORT bool is_jump(const Instruction &insn);
EDB_EXPORT bool is_repeat(const Instruction &insn);
EDB_EXPORT bool is_ret(const Instruction &insn);

// Check that instruction is x86-64 syscall
EDB_EXPORT bool is_syscall(const Instruction &insn);

// Check that instruction is P5 sysenter
EDB_EXPORT bool is_sysenter(const Instruction &insn);

// Check that instruction is any type of return
EDB_EXPORT bool is_return(const Instruction &insn);

// Check for any type of interrupt, including int3 etc.
EDB_EXPORT bool is_interrupt(const Instruction &insn);

// Check that instruction is retn (x86 0xC3)
EDB_EXPORT bool is_ret(const Instruction &insn);

// Check that instruction is int N (x86 0xCD N)
EDB_EXPORT bool is_int(const Instruction &insn);

// Check that instruction is any type of jump
EDB_EXPORT bool is_jump(const Instruction &insn);

EDB_EXPORT bool is_nop(const Instruction &insn);
EDB_EXPORT bool is_conditional_set(const Instruction &insn);
EDB_EXPORT bool is_unconditional_jump(const Instruction &insn);
EDB_EXPORT bool is_conditional_jump(const Instruction &insn);

EDB_EXPORT bool is_terminator(const Instruction &insn);
EDB_EXPORT bool is_unconditional_jump(const Instruction &insn);

EDB_EXPORT bool is_conditional_fpu_move(const Instruction &insn);
EDB_EXPORT bool is_conditional_gpr_move(const Instruction &insn);
EDB_EXPORT bool is_fpu(const Instruction &insn);
EDB_EXPORT bool is_conditional_move(const Instruction &insn);

// Check that instruction is an FPU instruction, taking only floating-point operands
EDB_EXPORT bool is_fpu_taking_float(const Instruction &insn);

// Check that instruction is an FPU instruction, one of operands of which is an integer
EDB_EXPORT bool is_fpu_taking_integer(const Instruction &insn);

// Check that instruction is an FPU instruction, one of operands of which is a packed BCD
EDB_EXPORT bool is_fpu_taking_bcd(const Instruction &insn);

// Check that instruction comes from any SIMD ISA extension
EDB_EXPORT bool is_simd(const Instruction &insn);

EDB_EXPORT bool is_expression(const Operand &operand);
EDB_EXPORT bool is_immediate(const Operand &operand);
EDB_EXPORT bool is_register(const Operand &operand);
EDB_EXPORT bool is_SIMD_PD(const Operand &operand);
EDB_EXPORT bool is_SIMD_PS(const Operand &operand);
EDB_EXPORT bool is_SIMD_SD(const Operand &operand);
EDB_EXPORT bool is_SIMD_SS(const Operand &operand);
EDB_EXPORT bool is_SIMD_SI(const Operand &operand);
EDB_EXPORT bool is_SIMD_USI(const Operand &operand);

}

#endif
