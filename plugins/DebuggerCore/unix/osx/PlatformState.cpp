/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformState.h"
#if __DARWIN_UNIX03
#define REG(x) __##x
#else
#define REG(x) x
#endif

namespace DebuggerCore {

/**
 * @brief Constructor for PlatformState
 */
PlatformState::PlatformState() {
	memset(&thread_state_, 0, sizeof(thread_state_));
	memset(&float_state_, 0, sizeof(float_state_));
	memset(&debug_state_, 0, sizeof(debug_state_));
	memset(&exception_state_, 0, sizeof(exception_state_));
}

/**
 * @brief Clones the current state object
 *
 * @return A unique pointer to the cloned state object
 */
std::unique_ptr<IState> PlatformState::clone() const {
	return std::make_unique<PlatformState>(*this);
}

/**
 * @brief Converts the flags to a string representation
 *
 * @param flags The flags to convert
 * @return The string representation of the flags
 */
QString PlatformState::flags_to_string(edb::reg_t flags) const {
	char buf[14];
	qsnprintf(
		buf,
		sizeof(buf),
		"%c %c %c %c %c %c %c",
		((flags & 0x001) ? 'C' : 'c'),
		((flags & 0x004) ? 'P' : 'p'),
		((flags & 0x010) ? 'A' : 'a'),
		((flags & 0x040) ? 'Z' : 'z'),
		((flags & 0x080) ? 'S' : 's'),
		((flags & 0x400) ? 'D' : 'd'),
		((flags & 0x800) ? 'O' : 'o'));

	return buf;
}

/**
 * @brief Returns the flags in a string form appropriate for this platform
 *
 * @return The string representation of the flags
 */
QString PlatformState::flags_to_string() const {
	return flags_to_string(flags());
}

/**
 * @brief Gets the value of a register by name
 *
 * @param reg The name of the register
 * @return The Register object representing the requested register
 */
Register PlatformState::value(const QString &reg) const {
	const QString lreg = reg.toLower();

#if defined(EDB_X86)
	if (lreg == "eax")
		return Register("eax", thread_state_.REG(eax), Register::TYPE_GPR);
	else if (lreg == "ebx")
		return Register("ebx", thread_state_.REG(ebx), Register::TYPE_GPR);
	else if (lreg == "ecx")
		return Register("ecx", thread_state_.REG(ecx), Register::TYPE_GPR);
	else if (lreg == "edx")
		return Register("edx", thread_state_.REG(edx), Register::TYPE_GPR);
	else if (lreg == "ebp")
		return Register("ebp", thread_state_.REG(ebp), Register::TYPE_GPR);
	else if (lreg == "esp")
		return Register("esp", thread_state_.REG(esp), Register::TYPE_GPR);
	else if (lreg == "esi")
		return Register("esi", thread_state_.REG(esi), Register::TYPE_GPR);
	else if (lreg == "edi")
		return Register("edi", thread_state_.REG(edi), Register::TYPE_GPR);
	else if (lreg == "eip")
		return Register("eip", thread_state_.REG(eip), Register::TYPE_IP);
	else if (lreg == "ax")
		return Register("ax", thread_state_.REG(eax) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bx")
		return Register("bx", thread_state_.REG(ebx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "cx")
		return Register("cx", thread_state_.REG(ecx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "dx")
		return Register("dx", thread_state_.REG(edx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bp")
		return Register("bp", thread_state_.REG(ebp) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "sp")
		return Register("sp", thread_state_.REG(esp) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "si")
		return Register("si", thread_state_.REG(esi) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "di")
		return Register("di", thread_state_.REG(edi) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "al")
		return Register("al", thread_state_.REG(eax) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bl")
		return Register("bl", thread_state_.REG(ebx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "cl")
		return Register("cl", thread_state_.REG(ecx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dl")
		return Register("dl", thread_state_.REG(edx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "ah")
		return Register("ah", (thread_state_.REG(eax) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bh")
		return Register("bh", (thread_state_.REG(ebx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "ch")
		return Register("ch", (thread_state_.REG(ecx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dh")
		return Register("dh", (thread_state_.REG(edx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "cs")
		return Register("cs", thread_state_.REG(cs), Register::TYPE_SEG);
	else if (lreg == "ds")
		return Register("ds", thread_state_.REG(ds), Register::TYPE_SEG);
	else if (lreg == "es")
		return Register("es", thread_state_.REG(es), Register::TYPE_SEG);
	else if (lreg == "fs")
		return Register("fs", thread_state_.REG(fs), Register::TYPE_SEG);
	else if (lreg == "gs")
		return Register("gs", thread_state_.REG(gs), Register::TYPE_SEG);
	else if (lreg == "ss")
		return Register("ss", thread_state_.REG(ss), Register::TYPE_SEG);
	else if (lreg == "fs_base")
		return Register("fs_base", 0 /* TODO fs_base */, Register::TYPE_SEG);
	else if (lreg == "gs_base")
		return Register("gs_base", 0 /* TODO gs_base */, Register::TYPE_SEG);
	else if (lreg == "eflags")
		return Register("eflags", thread_state_.REG(eflags), Register::TYPE_COND);
#elif defined(EDB_X86_64)
	if (lreg == "rax")
		return Register("rax", thread_state_.REG(rax), Register::TYPE_GPR);
	else if (lreg == "rbx")
		return Register("rbx", thread_state_.REG(rbx), Register::TYPE_GPR);
	else if (lreg == "rcx")
		return Register("rcx", thread_state_.REG(rcx), Register::TYPE_GPR);
	else if (lreg == "rdx")
		return Register("rdx", thread_state_.REG(rdx), Register::TYPE_GPR);
	else if (lreg == "rbp")
		return Register("rbp", thread_state_.REG(rbp), Register::TYPE_GPR);
	else if (lreg == "rsp")
		return Register("rsp", thread_state_.REG(rsp), Register::TYPE_GPR);
	else if (lreg == "rsi")
		return Register("rsi", thread_state_.REG(rsi), Register::TYPE_GPR);
	else if (lreg == "rdi")
		return Register("rdi", thread_state_.REG(rdi), Register::TYPE_GPR);
	else if (lreg == "rip")
		return Register("rip", thread_state_.REG(rip), Register::TYPE_IP);
	else if (lreg == "r8")
		return Register("r8", thread_state_.REG(r8), Register::TYPE_GPR);
	else if (lreg == "r9")
		return Register("r9", thread_state_.REG(r9), Register::TYPE_GPR);
	else if (lreg == "r10")
		return Register("r10", thread_state_.REG(r10), Register::TYPE_GPR);
	else if (lreg == "r11")
		return Register("r11", thread_state_.REG(r11), Register::TYPE_GPR);
	else if (lreg == "r12")
		return Register("r12", thread_state_.REG(r12), Register::TYPE_GPR);
	else if (lreg == "r13")
		return Register("r13", thread_state_.REG(r13), Register::TYPE_GPR);
	else if (lreg == "r14")
		return Register("r14", thread_state_.REG(r14), Register::TYPE_GPR);
	else if (lreg == "r15")
		return Register("r15", thread_state_.REG(r15), Register::TYPE_GPR);
	else if (lreg == "eax")
		return Register("eax", thread_state_.REG(rax) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "ebx")
		return Register("ebx", thread_state_.REG(rbx) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "ecx")
		return Register("ecx", thread_state_.REG(rcx) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "edx")
		return Register("edx", thread_state_.REG(rdx) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "ebp")
		return Register("ebp", thread_state_.REG(rbp) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "esp")
		return Register("esp", thread_state_.REG(rsp) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "esi")
		return Register("esi", thread_state_.REG(rsi) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "edi")
		return Register("edi", thread_state_.REG(rdi) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r8d")
		return Register("r8d", thread_state_.REG(r8) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r9d")
		return Register("r9d", thread_state_.REG(r9) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r10d")
		return Register("r10d", thread_state_.REG(r10) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r11d")
		return Register("r11d", thread_state_.REG(r11) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r12d")
		return Register("r12d", thread_state_.REG(r12) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r13d")
		return Register("r13d", thread_state_.REG(r13) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r14d")
		return Register("r14d", thread_state_.REG(r14) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "r15d")
		return Register("r15d", thread_state_.REG(r15) & 0xffffffff, Register::TYPE_GPR);
	else if (lreg == "ax")
		return Register("ax", thread_state_.REG(rax) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bx")
		return Register("bx", thread_state_.REG(rbx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "cx")
		return Register("cx", thread_state_.REG(rcx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "dx")
		return Register("dx", thread_state_.REG(rdx) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bp")
		return Register("bp", thread_state_.REG(rbp) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "sp")
		return Register("sp", thread_state_.REG(rsp) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "si")
		return Register("si", thread_state_.REG(rsi) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "di")
		return Register("di", thread_state_.REG(rdi) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r8w")
		return Register("r8w", thread_state_.REG(r8) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r9w")
		return Register("r9w", thread_state_.REG(r9) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r10w")
		return Register("r10w", thread_state_.REG(r10) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r11w")
		return Register("r11w", thread_state_.REG(r11) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r12w")
		return Register("r12w", thread_state_.REG(r12) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r13w")
		return Register("r13w", thread_state_.REG(r13) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r14w")
		return Register("r14w", thread_state_.REG(r14) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "r15w")
		return Register("r15w", thread_state_.REG(r15) & 0xffff, Register::TYPE_GPR);
	else if (lreg == "al")
		return Register("al", thread_state_.REG(rax) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bl")
		return Register("bl", thread_state_.REG(rbx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "cl")
		return Register("cl", thread_state_.REG(rcx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dl")
		return Register("dl", thread_state_.REG(rdx) & 0xff, Register::TYPE_GPR);
	else if (lreg == "ah")
		return Register("ah", (thread_state_.REG(rax) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bh")
		return Register("bh", (thread_state_.REG(rbx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "ch")
		return Register("ch", (thread_state_.REG(rcx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dh")
		return Register("dh", (thread_state_.REG(rdx) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "spl")
		return Register("spl", (thread_state_.REG(rsp) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bpl")
		return Register("bpl", (thread_state_.REG(rbp) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "sil")
		return Register("sil", (thread_state_.REG(rsi) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dil")
		return Register("dil", (thread_state_.REG(rdi) >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r8b")
		return Register("r8b", thread_state_.REG(r8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r9b")
		return Register("r9b", thread_state_.REG(r9) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r10b")
		return Register("r10b", thread_state_.REG(r10) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r11b")
		return Register("r11b", thread_state_.REG(r11) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r12b")
		return Register("r12b", thread_state_.REG(r12) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r13b")
		return Register("r13b", thread_state_.REG(r13) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r14b")
		return Register("r14b", thread_state_.REG(r14) & 0xff, Register::TYPE_GPR);
	else if (lreg == "r15b")
		return Register("r15b", thread_state_.REG(r15) & 0xff, Register::TYPE_GPR);
	else if (lreg == "cs")
		return Register("cs", thread_state_.REG(cs), Register::TYPE_SEG);
	else if (lreg == "fs")
		return Register("fs", thread_state_.REG(fs), Register::TYPE_SEG);
	else if (lreg == "gs")
		return Register("gs", thread_state_.REG(gs), Register::TYPE_SEG);
	else if (lreg == "fs_base")
		return Register("fs_base", 0 /* TODO fs_base */, Register::TYPE_SEG);
	else if (lreg == "gs_base")
		return Register("gs_base", 0 /* TODO gs_base */, Register::TYPE_SEG);
	else if (lreg == "rflags")
		return Register("rflags", thread_state_.REG(rflags), Register::TYPE_COND);
#endif

	return Register();
}

/**
 * @brief Returns the frame pointer for this platform
 *
 * @return The value of the frame pointer register
 */
edb::address_t PlatformState::frame_pointer() const {
#if defined(EDB_X86)
	return thread_state_.REG(ebp);
#elif defined(EDB_X86_64)
	return thread_state_.REG(rbp);
#endif
}

/**
 * @brief Returns the instruction pointer for this platform
 *
 * @return The value of the instruction pointer register
 */
edb::address_t PlatformState::instruction_pointer() const {
#if defined(EDB_X86)
	return thread_state_.REG(eip);
#elif defined(EDB_X86_64)
	return thread_state_.REG(rip);
#endif
}

/**
 * @brief Returns the stack pointer for this platform
 *
 * @return The value of the stack pointer register
 */
edb::address_t PlatformState::stack_pointer() const {
#if defined(EDB_X86)
	return thread_state_.REG(esp);
#elif defined(EDB_X86_64)
	return thread_state_.REG(rsp);
#endif
}

/**
 * @brief Returns the value of a debug register
 *
 * @param n The index of the debug register (0-7)
 * @return The value of the specified debug register
 */
edb::reg_t PlatformState::debug_register(int n) const {
	switch (n) {
	case 0:
		return debug_state_.REG(dr0);
	case 1:
		return debug_state_.REG(dr1);
	case 2:
		return debug_state_.REG(dr2);
	case 3:
		return debug_state_.REG(dr3);
	case 4:
		return debug_state_.REG(dr4);
	case 5:
		return debug_state_.REG(dr5);
	case 6:
		return debug_state_.REG(dr6);
	case 7:
		return debug_state_.REG(dr7);
	}
	return 0;
}

/**
 * @brief Returns the value of the flags register
 *
 * @return The value of the flags register
 */
edb::reg_t PlatformState::flags() const {
#if defined(EDB_X86)
	return thread_state_.REG(eflags);
#elif defined(EDB_X86_64)
	return thread_state_.REG(rflags);
#endif
}

/**
 * @brief Returns the value of an FPU register
 *
 * @param n The index of the FPU register (0-7)
 * @return The value of the specified FPU register
 */
long double PlatformState::fpu_register(int n) const {

	/*
	switch(n) {
	case 0: return static_cast<long double>(float_state_.REG(fpu_stmm0).REG(mmst_reg));
	case 1: return static_cast<long double>(float_state_.REG(fpu_stmm1).REG(mmst_reg));
	case 2: return static_cast<long double>(float_state_.REG(fpu_stmm2).REG(mmst_reg));
	case 3: return static_cast<long double>(float_state_.REG(fpu_stmm3).REG(mmst_reg));
	case 4: return static_cast<long double>(float_state_.REG(fpu_stmm4).REG(mmst_reg));
	case 5: return static_cast<long double>(float_state_.REG(fpu_stmm5).REG(mmst_reg));
	case 6: return static_cast<long double>(float_state_.REG(fpu_stmm6).REG(mmst_reg));
	case 7: return static_cast<long double>(float_state_.REG(fpu_stmm7).REG(mmst_reg));
	}
	*/

	return 0.0;
}

/**
 * @brief Adjusts the stack pointer by a specified number of bytes
 *
 * @param bytes The number of bytes to adjust the stack pointer by (positive or negative)

 */
void PlatformState::adjust_stack(int bytes) {
#if defined(EDB_X86)
	thread_state_.REG(esp) += bytes;
#elif defined(EDB_X86_64)
	thread_state_.REG(rsp) += bytes;
#endif
}

/**
 * @brief Clears all state information
 */
void PlatformState::clear() {
	memset(&thread_state_, 0, sizeof(thread_state_));
	memset(&float_state_, 0, sizeof(float_state_));
	memset(&debug_state_, 0, sizeof(debug_state_));
	memset(&exception_state_, 0, sizeof(exception_state_));
}

/**
 * @brief Sets the value of a debug register
 *
 * @param n The index of the debug register (0-7)
 * @param value The value to set the debug register to
 */
void PlatformState::set_debug_register(int n, edb::reg_t value) {
	switch (n) {
	case 0:
		debug_state_.REG(dr0) = value;
		break;
	case 1:
		debug_state_.REG(dr1) = value;
		break;
	case 2:
		debug_state_.REG(dr2) = value;
		break;
	case 3:
		debug_state_.REG(dr3) = value;
		break;
	case 4:
		debug_state_.REG(dr4) = value;
		break;
	case 5:
		debug_state_.REG(dr5) = value;
		break;
	case 6:
		debug_state_.REG(dr6) = value;
		break;
	case 7:
		debug_state_.REG(dr7) = value;
		break;
	default:
		break;
	}
}

/**
 * @brief Sets the value of the flags register
 *
 * @param flags The value to set the flags register to
 */
void PlatformState::set_flags(edb::reg_t flags) {
#if defined(EDB_X86)
	thread_state_.REG(eflags) = flags;
#elif defined(EDB_X86_64)
	thread_state_.REG(rflags) = flags;
#endif
}

/**
 * @brief Sets the value of the instruction pointer register
 *
 * @param value The value to set the instruction pointer register to
 */
void PlatformState::set_instruction_pointer(edb::address_t value) {
#if defined(EDB_X86)
	thread_state_.REG(eip) = value;
#elif defined(EDB_X86_64)
	thread_state_.REG(rip) = value;
#endif
}

/**
 * @brief Sets the value of a general-purpose register
 *
 * @param name The name of the register to set
 * @param value The value to set the register to
 */
void PlatformState::set_register(const QString &name, edb::reg_t value) {

	const QString lreg = name.toLower();
#if defined(EDB_X86)
	if (lreg == "eax") {
		thread_state_.REG(eax) = value;
	} else if (lreg == "ebx") {
		thread_state_.REG(ebx) = value;
	} else if (lreg == "ecx") {
		thread_state_.REG(ecx) = value;
	} else if (lreg == "edx") {
		thread_state_.REG(edx) = value;
	} else if (lreg == "ebp") {
		thread_state_.REG(ebp) = value;
	} else if (lreg == "esp") {
		thread_state_.REG(esp) = value;
	} else if (lreg == "esi") {
		thread_state_.REG(esi) = value;
	} else if (lreg == "edi") {
		thread_state_.REG(edi) = value;
	} else if (lreg == "eip") {
		thread_state_.REG(eip) = value;
	} else if (lreg == "cs") {
		thread_state_.REG(cs) = value;
	} else if (lreg == "ds") {
		thread_state_.REG(ds) = value;
	} else if (lreg == "es") {
		thread_state_.REG(es) = value;
	} else if (lreg == "fs") {
		thread_state_.REG(fs) = value;
	} else if (lreg == "gs") {
		thread_state_.REG(gs) = value;
	} else if (lreg == "ss") {
		thread_state_.REG(ss) = value;
	} else if (lreg == "eflags") {
		thread_state_.REG(eflags) = value;
	}
#elif defined(EDB_X86_64)
	if (lreg == "rax") {
		thread_state_.REG(rax) = value;
	} else if (lreg == "rbx") {
		thread_state_.REG(rbx) = value;
	} else if (lreg == "rcx") {
		thread_state_.REG(rcx) = value;
	} else if (lreg == "rdx") {
		thread_state_.REG(rdx) = value;
	} else if (lreg == "rbp") {
		thread_state_.REG(rbp) = value;
	} else if (lreg == "rsp") {
		thread_state_.REG(rsp) = value;
	} else if (lreg == "rsi") {
		thread_state_.REG(rsi) = value;
	} else if (lreg == "rdi") {
		thread_state_.REG(rdi) = value;
	} else if (lreg == "r8") {
		thread_state_.REG(r8) = value;
	} else if (lreg == "r9") {
		thread_state_.REG(r9) = value;
	} else if (lreg == "r10") {
		thread_state_.REG(r10) = value;
	} else if (lreg == "r11") {
		thread_state_.REG(r11) = value;
	} else if (lreg == "r12") {
		thread_state_.REG(r12) = value;
	} else if (lreg == "r13") {
		thread_state_.REG(r13) = value;
	} else if (lreg == "r14") {
		thread_state_.REG(r14) = value;
	} else if (lreg == "r15") {
		thread_state_.REG(r15) = value;
	} else if (lreg == "rip") {
		thread_state_.REG(rip) = value;
	} else if (lreg == "cs") {
		thread_state_.REG(cs) = value;
	} else if (lreg == "fs") {
		thread_state_.REG(fs) = value;
	} else if (lreg == "gs") {
		thread_state_.REG(gs) = value;
	} else if (lreg == "rflags") {
		thread_state_.REG(rflags) = value;
	}
#endif
}

/**
 * @brief Returns the value of an MMX register
 *
 * @param n The index of the MMX register (0-7)
 * @return The value of the specified MMX register
 */
quint64 PlatformState::mmx_register(int n) const {
	Q_UNUSED(n)
	return 0;
}

/**
 * @brief Returns the value of an XMM register
 *
 * @param n The index of the XMM register (0-15)
 * @return The value of the specified XMM register
 */
QByteArray PlatformState::xmm_register(int n) const {
	Q_UNUSED(n)
	return QByteArray();
}

}
