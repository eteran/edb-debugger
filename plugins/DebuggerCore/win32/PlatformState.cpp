/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#include "PlatformState.h"
#include "edb.h"
#include "string_hash.h"
#include <cmath>
#include <limits>

namespace DebuggerCorePlugin {

namespace {

/**
 * @brief read_float80
 * @param buffer
 * @return
 */
double read_float80(const uint8_t buffer[10]) {
	// little-endian!
	//80 bit floating point value according to IEEE-754:
	//1 bit sign, 15 bit exponent, 64 bit mantissa

	constexpr uint16_t SIGNBIT    = (1 << 15);
	constexpr uint16_t EXP_BIAS   = (1 << 14) - 1; // 2^(n-1) - 1 = 16383
	constexpr uint16_t SPECIALEXP = (1 << 15) - 1; // all bits set
	constexpr uint64_t HIGHBIT    = 1ull << 63;
	constexpr uint64_t QUIETBIT   = 1ull << 62;

	// Extract sign, exponent and mantissa
	auto exponent       = *reinterpret_cast<const uint16_t *>(&buffer[8]);
	const auto mantissa = *reinterpret_cast<const uint16_t *>(&buffer[0]);

	const double sign = (exponent & SIGNBIT) ? -1.0 : 1.0;
	exponent &= ~SIGNBIT;

	// Check for undefined values
	if ((!exponent && (mantissa & HIGHBIT)) || (exponent && !(mantissa & HIGHBIT))) {
		return std::numeric_limits<double>::quiet_NaN();
	}

	// Check for special values (infinity, NaN)
	if (exponent == 0) {
		if (mantissa == 0) {
			return sign * 0.0;
		} else {
			// denormalized
		}
	} else if (exponent == SPECIALEXP) {
		if (!(mantissa & ~HIGHBIT)) {
			return sign * std::numeric_limits<double>::infinity();
		} else {
			if (mantissa & QUIETBIT) {
				return std::numeric_limits<double>::quiet_NaN();
			} else {
				return std::numeric_limits<double>::signaling_NaN();
			}
		}
	}

	//value = (-1)^s * (m / 2^63) * 2^(e - 16383)
	double significand = (static_cast<double>(mantissa) / (1ull << 63));
	return sign * ldexp(significand, exponent - EXP_BIAS);
}
}

/**
 * @brief PlatformState::clone
 * @return a copy of the state object
 */
std::unique_ptr<IState> PlatformState::clone() const {
	return std::make_unique<PlatformState>(*this);
}

/**
 * @brief PlatformState::flagsToString
 * @param flags
 * @return the flags in a string form appropriate for this platform
 */
QString PlatformState::flagsToString(edb::reg_t flags) const {
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
 * @brief PlatformState::flagsToString
 * @return the flags in a string form appropriate for this platform
 */
QString PlatformState::flagsToString() const {
	return flagsToString(flags());
}

/**
 * @brief PlatformState::value
 * @param reg
 * @return a Register object which represents the register with the name supplied
 */
Register PlatformState::value(const QString &reg) const {
	const QString lreg = reg.toLower();

#if defined(EDB_X86)
	if (lreg == "eax")
		return make_Register("eax", context32_.Eax, Register::TYPE_GPR);
	else if (lreg == "ebx")
		return make_Register("ebx", context32_.Ebx, Register::TYPE_GPR);
	else if (lreg == "ecx")
		return make_Register("ecx", context32_.Ecx, Register::TYPE_GPR);
	else if (lreg == "edx")
		return make_Register("edx", context32_.Edx, Register::TYPE_GPR);
	else if (lreg == "ebp")
		return make_Register("ebp", context32_.Ebp, Register::TYPE_GPR);
	else if (lreg == "esp")
		return make_Register("esp", context32_.Esp, Register::TYPE_GPR);
	else if (lreg == "esi")
		return make_Register("esi", context32_.Esi, Register::TYPE_GPR);
	else if (lreg == "edi")
		return make_Register("edi", context32_.Edi, Register::TYPE_GPR);
	else if (lreg == "eip")
		return make_Register("eip", context32_.Eip, Register::TYPE_IP);
	else if (lreg == "ax")
		return make_Register("ax", context32_.Eax & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bx")
		return make_Register("bx", context32_.Ebx & 0xffff, Register::TYPE_GPR);
	else if (lreg == "cx")
		return make_Register("cx", context32_.Ecx & 0xffff, Register::TYPE_GPR);
	else if (lreg == "dx")
		return make_Register("dx", context32_.Edx & 0xffff, Register::TYPE_GPR);
	else if (lreg == "bp")
		return make_Register("bp", context32_.Ebp & 0xffff, Register::TYPE_GPR);
	else if (lreg == "sp")
		return make_Register("sp", context32_.Esp & 0xffff, Register::TYPE_GPR);
	else if (lreg == "si")
		return make_Register("si", context32_.Esi & 0xffff, Register::TYPE_GPR);
	else if (lreg == "di")
		return make_Register("di", context32_.Edi & 0xffff, Register::TYPE_GPR);
	else if (lreg == "al")
		return make_Register("al", context32_.Eax & 0xff, Register::TYPE_GPR);
	else if (lreg == "bl")
		return make_Register("bl", context32_.Ebx & 0xff, Register::TYPE_GPR);
	else if (lreg == "cl")
		return make_Register("cl", context32_.Ecx & 0xff, Register::TYPE_GPR);
	else if (lreg == "dl")
		return make_Register("dl", context32_.Edx & 0xff, Register::TYPE_GPR);
	else if (lreg == "ah")
		return make_Register("ah", (context32_.Eax >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "bh")
		return make_Register("bh", (context32_.Ebx >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "ch")
		return make_Register("ch", (context32_.Ecx >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "dh")
		return make_Register("dh", (context32_.Edx >> 8) & 0xff, Register::TYPE_GPR);
	else if (lreg == "cs")
		return make_Register("cs", context32_.SegCs, Register::TYPE_SEG);
	else if (lreg == "ds")
		return make_Register("ds", context32_.SegDs, Register::TYPE_SEG);
	else if (lreg == "es")
		return make_Register("es", context32_.SegEs, Register::TYPE_SEG);
	else if (lreg == "fs")
		return make_Register("fs", context32_.SegFs, Register::TYPE_SEG);
	else if (lreg == "gs")
		return make_Register("gs", context32_.SegGs, Register::TYPE_SEG);
	else if (lreg == "ss")
		return make_Register("ss", context32_.SegSs, Register::TYPE_SEG);
	else if (lreg == "fs_base")
		return make_Register("fs_base", fs_base_, Register::TYPE_SEG);
	else if (lreg == "gs_base")
		return make_Register("gs_base", gs_base_, Register::TYPE_SEG);
	else if (lreg == "eflags")
		return make_Register("eflags", context32_.EFlags, Register::TYPE_COND);
#elif defined(EDB_X86_64)
	if (!isWow64_) {
		if (lreg == "rax")
			return make_Register("rax", context64_.Rax, Register::TYPE_GPR);
		else if (lreg == "rbx")
			return make_Register("rbx", context64_.Rbx, Register::TYPE_GPR);
		else if (lreg == "rcx")
			return make_Register("rcx", context64_.Rcx, Register::TYPE_GPR);
		else if (lreg == "rdx")
			return make_Register("rdx", context64_.Rdx, Register::TYPE_GPR);
		else if (lreg == "rbp")
			return make_Register("rbp", context64_.Rbp, Register::TYPE_GPR);
		else if (lreg == "rsp")
			return make_Register("rsp", context64_.Rsp, Register::TYPE_GPR);
		else if (lreg == "rsi")
			return make_Register("rsi", context64_.Rsi, Register::TYPE_GPR);
		else if (lreg == "rdi")
			return make_Register("rdi", context64_.Rdi, Register::TYPE_GPR);
		else if (lreg == "rip")
			return make_Register("rip", context64_.Rip, Register::TYPE_IP);
		else if (lreg == "r8")
			return make_Register("r8", context64_.R8, Register::TYPE_GPR);
		else if (lreg == "r9")
			return make_Register("r9", context64_.R9, Register::TYPE_GPR);
		else if (lreg == "r10")
			return make_Register("r10", context64_.R10, Register::TYPE_GPR);
		else if (lreg == "r11")
			return make_Register("r11", context64_.R11, Register::TYPE_GPR);
		else if (lreg == "r12")
			return make_Register("r12", context64_.R12, Register::TYPE_GPR);
		else if (lreg == "r13")
			return make_Register("r13", context64_.R13, Register::TYPE_GPR);
		else if (lreg == "r14")
			return make_Register("r14", context64_.R14, Register::TYPE_GPR);
		else if (lreg == "r15")
			return make_Register("r15", context64_.R15, Register::TYPE_GPR);
		else if (lreg == "eax")
			return make_Register("eax", context64_.Rax & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "ebx")
			return make_Register("ebx", context64_.Rbx & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "ecx")
			return make_Register("ecx", context64_.Rcx & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "edx")
			return make_Register("edx", context64_.Rdx & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "ebp")
			return make_Register("ebp", context64_.Rbp & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "esp")
			return make_Register("esp", context64_.Rsp & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "esi")
			return make_Register("esi", context64_.Rsi & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "edi")
			return make_Register("edi", context64_.Rdi & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r8d")
			return make_Register("r8d", context64_.R8 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r9d")
			return make_Register("r9d", context64_.R9 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r10d")
			return make_Register("r10d", context64_.R10 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r11d")
			return make_Register("r11d", context64_.R11 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r12d")
			return make_Register("r12d", context64_.R12 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r13d")
			return make_Register("r13d", context64_.R13 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r14d")
			return make_Register("r14d", context64_.R14 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "r15d")
			return make_Register("r15d", context64_.R15 & 0xffffffff, Register::TYPE_GPR);
		else if (lreg == "ax")
			return make_Register("ax", context64_.Rax & 0xffff, Register::TYPE_GPR);
		else if (lreg == "bx")
			return make_Register("bx", context64_.Rbx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "cx")
			return make_Register("cx", context64_.Rcx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "dx")
			return make_Register("dx", context64_.Rdx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "bp")
			return make_Register("bp", context64_.Rbp & 0xffff, Register::TYPE_GPR);
		else if (lreg == "sp")
			return make_Register("sp", context64_.Rsp & 0xffff, Register::TYPE_GPR);
		else if (lreg == "si")
			return make_Register("si", context64_.Rsi & 0xffff, Register::TYPE_GPR);
		else if (lreg == "di")
			return make_Register("di", context64_.Rdi & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r8w")
			return make_Register("r8w", context64_.R8 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r9w")
			return make_Register("r9w", context64_.R9 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r10w")
			return make_Register("r10w", context64_.R10 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r11w")
			return make_Register("r11w", context64_.R11 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r12w")
			return make_Register("r12w", context64_.R12 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r13w")
			return make_Register("r13w", context64_.R13 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r14w")
			return make_Register("r14w", context64_.R14 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "r15w")
			return make_Register("r15w", context64_.R15 & 0xffff, Register::TYPE_GPR);
		else if (lreg == "al")
			return make_Register("al", context64_.Rax & 0xff, Register::TYPE_GPR);
		else if (lreg == "bl")
			return make_Register("bl", context64_.Rbx & 0xff, Register::TYPE_GPR);
		else if (lreg == "cl")
			return make_Register("cl", context64_.Rcx & 0xff, Register::TYPE_GPR);
		else if (lreg == "dl")
			return make_Register("dl", context64_.Rdx & 0xff, Register::TYPE_GPR);
		else if (lreg == "ah")
			return make_Register("ah", (context64_.Rax >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "bh")
			return make_Register("bh", (context64_.Rbx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "ch")
			return make_Register("ch", (context64_.Rcx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "dh")
			return make_Register("dh", (context64_.Rdx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "spl")
			return make_Register("spl", (context64_.Rsp >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "bpl")
			return make_Register("bpl", (context64_.Rbp >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "sil")
			return make_Register("sil", (context64_.Rsi >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "dil")
			return make_Register("dil", (context64_.Rdi >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "r8b")
			return make_Register("r8b", context64_.R8 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r9b")
			return make_Register("r9b", context64_.R9 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r10b")
			return make_Register("r10b", context64_.R10 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r11b")
			return make_Register("r11b", context64_.R11 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r12b")
			return make_Register("r12b", context64_.R12 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r13b")
			return make_Register("r13b", context64_.R13 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r14b")
			return make_Register("r14b", context64_.R14 & 0xff, Register::TYPE_GPR);
		else if (lreg == "r15b")
			return make_Register("r15b", context64_.R15 & 0xff, Register::TYPE_GPR);
		else if (lreg == "cs")
			return make_Register("cs", context64_.SegCs, Register::TYPE_SEG);
		else if (lreg == "ds")
			return make_Register("ds", context64_.SegDs, Register::TYPE_SEG);
		else if (lreg == "es")
			return make_Register("es", context64_.SegEs, Register::TYPE_SEG);
		else if (lreg == "fs")
			return make_Register("fs", context64_.SegFs, Register::TYPE_SEG);
		else if (lreg == "gs")
			return make_Register("gs", context64_.SegGs, Register::TYPE_SEG);
		else if (lreg == "ss")
			return make_Register("ss", context64_.SegSs, Register::TYPE_SEG);
		else if (lreg == "fs_base")
			return make_Register("fs_base", fs_base_, Register::TYPE_SEG);
		else if (lreg == "gs_base")
			return make_Register("gs_base", gs_base_, Register::TYPE_SEG);
		else if (lreg == "rflags")
			return make_Register("rflags", context64_.EFlags, Register::TYPE_COND);
	} else {
		if (lreg == "eax")
			return make_Register("eax", context32_.Eax, Register::TYPE_GPR);
		else if (lreg == "ebx")
			return make_Register("ebx", context32_.Ebx, Register::TYPE_GPR);
		else if (lreg == "ecx")
			return make_Register("ecx", context32_.Ecx, Register::TYPE_GPR);
		else if (lreg == "edx")
			return make_Register("edx", context32_.Edx, Register::TYPE_GPR);
		else if (lreg == "ebp")
			return make_Register("ebp", context32_.Ebp, Register::TYPE_GPR);
		else if (lreg == "esp")
			return make_Register("esp", context32_.Esp, Register::TYPE_GPR);
		else if (lreg == "esi")
			return make_Register("esi", context32_.Esi, Register::TYPE_GPR);
		else if (lreg == "edi")
			return make_Register("edi", context32_.Edi, Register::TYPE_GPR);
		else if (lreg == "eip")
			return make_Register("eip", context32_.Eip, Register::TYPE_IP);
		else if (lreg == "ax")
			return make_Register("ax", context32_.Eax & 0xffff, Register::TYPE_GPR);
		else if (lreg == "bx")
			return make_Register("bx", context32_.Ebx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "cx")
			return make_Register("cx", context32_.Ecx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "dx")
			return make_Register("dx", context32_.Edx & 0xffff, Register::TYPE_GPR);
		else if (lreg == "bp")
			return make_Register("bp", context32_.Ebp & 0xffff, Register::TYPE_GPR);
		else if (lreg == "sp")
			return make_Register("sp", context32_.Esp & 0xffff, Register::TYPE_GPR);
		else if (lreg == "si")
			return make_Register("si", context32_.Esi & 0xffff, Register::TYPE_GPR);
		else if (lreg == "di")
			return make_Register("di", context32_.Edi & 0xffff, Register::TYPE_GPR);
		else if (lreg == "al")
			return make_Register("al", context32_.Eax & 0xff, Register::TYPE_GPR);
		else if (lreg == "bl")
			return make_Register("bl", context32_.Ebx & 0xff, Register::TYPE_GPR);
		else if (lreg == "cl")
			return make_Register("cl", context32_.Ecx & 0xff, Register::TYPE_GPR);
		else if (lreg == "dl")
			return make_Register("dl", context32_.Edx & 0xff, Register::TYPE_GPR);
		else if (lreg == "ah")
			return make_Register("ah", (context32_.Eax >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "bh")
			return make_Register("bh", (context32_.Ebx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "ch")
			return make_Register("ch", (context32_.Ecx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "dh")
			return make_Register("dh", (context32_.Edx >> 8) & 0xff, Register::TYPE_GPR);
		else if (lreg == "cs")
			return make_Register("cs", context32_.SegCs, Register::TYPE_SEG);
		else if (lreg == "ds")
			return make_Register("ds", context32_.SegDs, Register::TYPE_SEG);
		else if (lreg == "es")
			return make_Register("es", context32_.SegEs, Register::TYPE_SEG);
		else if (lreg == "fs")
			return make_Register("fs", context32_.SegFs, Register::TYPE_SEG);
		else if (lreg == "gs")
			return make_Register("gs", context32_.SegGs, Register::TYPE_SEG);
		else if (lreg == "ss")
			return make_Register("ss", context32_.SegSs, Register::TYPE_SEG);
		else if (lreg == "fs_base")
			return make_Register("fs_base", fs_base_, Register::TYPE_SEG);
		else if (lreg == "gs_base")
			return make_Register("gs_base", gs_base_, Register::TYPE_SEG);
		else if (lreg == "eflags")
			return make_Register("eflags", context32_.EFlags, Register::TYPE_COND);
	}
#endif

	return Register();
}

/**
 * @brief PlatformState::framePointer
 * @return what is conceptually the frame pointer for this platform
 */
edb::address_t PlatformState::framePointer() const {
#if defined(EDB_X86)
	return context32_.Ebp;
#elif defined(EDB_X86_64)
	return isWow64_ ? context32_.Ebp : context64_.Rbp;
#endif
}

/**
 * @brief PlatformState::instructionPointer
 * @return the instruction pointer for this platform
 */
edb::address_t PlatformState::instructionPointer() const {
#if defined(EDB_X86)
	return context32_.Eip;
#elif defined(EDB_X86_64)
	return isWow64_ ? context32_.Eip : context64_.Rip;
#endif
}

/**
 * @brief PlatformState::stackPointer
 * @return the stack pointer for this platform
 */
edb::address_t PlatformState::stackPointer() const {
#if defined(EDB_X86)
	return context32_.Esp;
#elif defined(EDB_X86_64)
	return isWow64_ ? context32_.Esp : context64_.Rsp;
#endif
}

/**
 * @brief PlatformState::debugRegister
 * @param n
 * @return
 */
edb::reg_t PlatformState::debugRegister(size_t n) const {
#if defined(EDB_X86)
	switch (n) {
	case 0:
		return context32_.Dr0;
	case 1:
		return context32_.Dr1;
	case 2:
		return context32_.Dr2;
	case 3:
		return context32_.Dr3;
	case 6:
		return context32_.Dr6;
	case 7:
		return context32_.Dr7;
	}
#elif defined(EDB_X86_64)
	if (isWow64_) {
		switch (n) {
		case 0:
			return context32_.Dr0;
		case 1:
			return context32_.Dr1;
		case 2:
			return context32_.Dr2;
		case 3:
			return context32_.Dr3;
		case 6:
			return context32_.Dr6;
		case 7:
			return context32_.Dr7;
		}
	} else {
		switch (n) {
		case 0:
			return context64_.Dr0;
		case 1:
			return context64_.Dr1;
		case 2:
			return context64_.Dr2;
		case 3:
			return context64_.Dr3;
		case 6:
			return context64_.Dr6;
		case 7:
			return context64_.Dr7;
		}
	}
#endif
	return 0;
}

/**
 * @brief PlatformState::flags
 * @return
 */
edb::reg_t PlatformState::flags() const {
#if defined(EDB_X86)
	return context32_.EFlags;
#elif defined(EDB_X86_64)
	return isWow64_ ? context32_.EFlags : context64_.EFlags;
#endif
}

/**
 * @brief PlatformState::fpu_register
 * @param n
 * @return
 */
long double PlatformState::fpu_register(int n) const {
	double ret = 0.0;

	if (n >= 0 && n <= 7) {
#if defined(EDB_X86)
		auto p = reinterpret_cast<const uint8_t *>(&context32_.FloatSave.RegisterArea[n * 10]);
		if (sizeof(long double) == 10) { // can we check this at compile time?
			ret = *(reinterpret_cast<const long double *>(p));
		} else {
			ret = read_float80(p);
		}
#elif defined(EDB_X86_64)
		if (isWow64_) {
			auto p = reinterpret_cast<const uint8_t *>(&context32_.FloatSave.RegisterArea[n * 10]);
			if (sizeof(long double) == 10) { // can we check this at compile time?
				ret = *(reinterpret_cast<const long double *>(p));
			} else {
				ret = read_float80(p);
			}
		} else {
			auto p = reinterpret_cast<const uint8_t *>(&context64_.FltSave.FloatRegisters[n]);
			if (sizeof(long double) == 10) {
				ret = *(reinterpret_cast<const long double *>(p));
			} else {
				ret = read_float80(p);
			}
		}
#endif
	}
	return ret;
}

/**
 * @brief PlatformState::mmx_register
 * @param n
 * @return
 */
quint64 PlatformState::mmx_register(int n) const {
	quint64 ret = 0;

	if (n >= 0 && n <= 7) {
#if defined(EDB_X86)
		// MMX registers are an alias to the lower 64-bits of the FPU regs
		auto p = reinterpret_cast<const quint64 *>(&context32_.FloatSave.RegisterArea[n * 10]);
		ret    = *p; // little endian!
#elif defined(EDB_X86_64)
		if (isWow64_) {
			auto p = reinterpret_cast<const quint64 *>(&context32_.FloatSave.RegisterArea[n * 10]);
			ret    = *p; // little endian!
		} else {
			auto p = reinterpret_cast<const quint64 *>(&context64_.FltSave.FloatRegisters[n]);
			ret    = *p;
		}
#endif
	}
	return ret;
}

/**
 * @brief PlatformState::xmm_register
 * @param n
 * @return
 */
QByteArray PlatformState::xmm_register(int n) const {
	QByteArray ret(16, 0);

#if defined(EDB_X86)
	if (n >= 0 && n <= 7) {
		auto p = reinterpret_cast<const char *>(&context32_.ExtendedRegisters[(10 + n) * 16]);
		ret    = QByteArray(p, 16);
		std::reverse(ret.begin(), ret.end()); //little endian!
	}
#elif defined(EDB_X86_64)
	if (n >= 0 && n <= 15) {
		if (isWow64_) {
			auto p = reinterpret_cast<const char *>(&context32_.ExtendedRegisters[(10 + n) * 16]);
			ret    = QByteArray(p, 16);
			std::reverse(ret.begin(), ret.end()); //little endian!
		} else {
			auto p = reinterpret_cast<const char *>(&context64_.FltSave.XmmRegisters[n]);
			ret    = QByteArray(p, sizeof(M128A));
			std::reverse(ret.begin(), ret.end());
		}
	}
#endif

	return ret;
}

/**
 * @brief PlatformState::adjustStack
 * @param bytes
 */
void PlatformState::adjustStack(int bytes) {
#if defined(EDB_X86)
	context32_.Esp += bytes;
#elif defined(EDB_X86_64)
	if (isWow64_) {
		context32_.Esp += bytes;
	} else {
		context64_.Rsp += bytes;
	}
#endif
}

/**
 * @brief PlatformState::clear
 */
void PlatformState::clear() {
	context32_ = {};
#if defined(EDB_X86_64)
	context64_ = {};
#endif
	fs_base_ = 0;
	gs_base_ = 0;
}

/**
 * @brief PlatformState::setDebugRegister
 * @param n
 * @param value
 */
void PlatformState::setDebugRegister(size_t n, edb::reg_t value) {
#if defined(EDB_X86)
	switch (n) {
	case 0:
		context32_.Dr0 = value;
		break;
	case 1:
		context32_.Dr1 = value;
		break;
	case 2:
		context32_.Dr2 = value;
		break;
	case 3:
		context32_.Dr3 = value;
		break;
	case 6:
		context32_.Dr6 = value;
		break;
	case 7:
		context32_.Dr7 = value;
		break;
	default:
		break;
	}
#elif defined(EDB_X86_64)
	if (isWow64_) {
		switch (n) {
		case 0:
			context32_.Dr0 = value;
			break;
		case 1:
			context32_.Dr1 = value;
			break;
		case 2:
			context32_.Dr2 = value;
			break;
		case 3:
			context32_.Dr3 = value;
			break;
		case 6:
			context32_.Dr6 = value;
			break;
		case 7:
			context32_.Dr7 = value;
			break;
		default:
			break;
		}
	} else {
		switch (n) {
		case 0:
			context64_.Dr0 = value;
			break;
		case 1:
			context64_.Dr1 = value;
			break;
		case 2:
			context64_.Dr2 = value;
			break;
		case 3:
			context64_.Dr3 = value;
			break;
		case 6:
			context64_.Dr6 = value;
			break;
		case 7:
			context64_.Dr7 = value;
			break;
		default:
			break;
		}
	}
#endif
}

/**
 * @brief PlatformState::setFlags
 * @param flags
 */
void PlatformState::setFlags(edb::reg_t flags) {
#if defined(EDB_X86)
	context32_.EFlags = flags;
#elif defined(EDB_X86_64)
	if (isWow64_) {
		context32_.EFlags = flags;
	} else {
		context64_.EFlags = flags;
	}
#endif
}

/**
 * @brief PlatformState::setInstructionPointer
 * @param value
 */
void PlatformState::setInstructionPointer(edb::address_t value) {
#if defined(EDB_X86)
	context32_.Eip = value;
#elif defined(EDB_X86_64)
	if (isWow64_) {
		context32_.Eip = static_cast<uint32_t>(value);
	} else {
		context64_.Rip = value;
	}
#endif
}

/**
 * @brief PlatformState::setRegister
 * @param name
 * @param value
 */
void PlatformState::setRegister(const QString &name, edb::reg_t value) {

	const QString lreg = name.toLower();
#if defined(EDB_X86)
	if (lreg == "eax") {
		context32_.Eax = value;
	} else if (lreg == "ebx") {
		context32_.Ebx = value;
	} else if (lreg == "ecx") {
		context32_.Ecx = value;
	} else if (lreg == "edx") {
		context32_.Edx = value;
	} else if (lreg == "ebp") {
		context32_.Ebp = value;
	} else if (lreg == "esp") {
		context32_.Esp = value;
	} else if (lreg == "esi") {
		context32_.Esi = value;
	} else if (lreg == "edi") {
		context32_.Edi = value;
	} else if (lreg == "eip") {
		context32_.Eip = value;
	} else if (lreg == "cs") {
		context32_.SegCs = value;
	} else if (lreg == "ds") {
		context32_.SegDs = value;
	} else if (lreg == "es") {
		context32_.SegEs = value;
	} else if (lreg == "fs") {
		context32_.SegFs = value;
	} else if (lreg == "gs") {
		context32_.SegGs = value;
	} else if (lreg == "ss") {
		context32_.SegSs = value;
	} else if (lreg == "eflags") {
		context32_.EFlags = value;
	}
#elif defined(EDB_X86_64)
	if (!isWow64_) {
		if (lreg == "rax") {
			context64_.Rax = value;
		} else if (lreg == "rbx") {
			context64_.Rbx = value;
		} else if (lreg == "rcx") {
			context64_.Rcx = value;
		} else if (lreg == "rdx") {
			context64_.Rdx = value;
		} else if (lreg == "rbp") {
			context64_.Rbp = value;
		} else if (lreg == "rsp") {
			context64_.Rsp = value;
		} else if (lreg == "rsi") {
			context64_.Rsi = value;
		} else if (lreg == "rdi") {
			context64_.Rdi = value;
		} else if (lreg == "r8") {
			context64_.R8 = value;
		} else if (lreg == "r9") {
			context64_.R9 = value;
		} else if (lreg == "r10") {
			context64_.R10 = value;
		} else if (lreg == "r11") {
			context64_.R11 = value;
		} else if (lreg == "r12") {
			context64_.R12 = value;
		} else if (lreg == "r13") {
			context64_.R13 = value;
		} else if (lreg == "r14") {
			context64_.R14 = value;
		} else if (lreg == "r15") {
			context64_.R15 = value;
		} else if (lreg == "rip") {
			context64_.Rip = value;
		} else if (lreg == "cs") {
			context64_.SegCs = value;
		} else if (lreg == "ds") {
			context64_.SegDs = value;
		} else if (lreg == "es") {
			context64_.SegEs = value;
		} else if (lreg == "fs") {
			context64_.SegFs = value;
		} else if (lreg == "gs") {
			context64_.SegGs = value;
		} else if (lreg == "ss") {
			context64_.SegSs = value;
		} else if (lreg == "rflags") {
			context64_.EFlags = value;
		}
	} else {
		if (lreg == "eax") {
			context32_.Eax = value;
		} else if (lreg == "ebx") {
			context32_.Ebx = value;
		} else if (lreg == "ecx") {
			context32_.Ecx = value;
		} else if (lreg == "edx") {
			context32_.Edx = value;
		} else if (lreg == "ebp") {
			context32_.Ebp = value;
		} else if (lreg == "esp") {
			context32_.Esp = value;
		} else if (lreg == "esi") {
			context32_.Esi = value;
		} else if (lreg == "edi") {
			context32_.Edi = value;
		} else if (lreg == "eip") {
			context32_.Eip = value;
		} else if (lreg == "cs") {
			context32_.SegCs = value;
		} else if (lreg == "ds") {
			context32_.SegDs = value;
		} else if (lreg == "es") {
			context32_.SegEs = value;
		} else if (lreg == "fs") {
			context32_.SegFs = value;
		} else if (lreg == "gs") {
			context32_.SegGs = value;
		} else if (lreg == "ss") {
			context32_.SegSs = value;
		} else if (lreg == "eflags") {
			context32_.EFlags = value;
		}
	}
#endif
}

/**
 * @brief PlatformState::instructionPointerRegister
 * @return
 */
Register PlatformState::instructionPointerRegister() const {
#if defined(EDB_X86_64)
	if (!isWow64_) {
		return make_Register("rip", context64_.Rip, Register::TYPE_IP);
	} else {
		return make_Register("eip", context32_.Eip, Register::TYPE_IP);
	}
#elif defined(EDB_X86)
	return make_Register("eip", context32_.Eip, Register::TYPE_IP);
#endif
}

/**
 * @brief PlatformState::flagsRegister
 * @return
 */
Register PlatformState::flagsRegister() const {
#if defined(EDB_X86_64)
	if (!isWow64_) {
		return make_Register("rflags", context64_.EFlags, Register::TYPE_IP);
	} else {
		return make_Register("eflags", context32_.EFlags, Register::TYPE_IP);
	}
#elif defined(EDB_X86)
	return make_Register("eflags", context32_.EFlags, Register::TYPE_IP);
#endif
	return Register();
}

/**
 * @brief PlatformState::getThreadState
 * @param hThread
 * @param isWow64
 */
void PlatformState::getThreadState(HANDLE hThread, bool isWow64) {
#if defined(EDB_X86)
	context32_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
	GetThreadContext(hThread, &context32_);

	gs_base_ = 0;
	fs_base_ = 0;

	LDT_ENTRY ldt_entry;
	if (GetThreadSelectorEntry(hThread, context32_.SegGs, &ldt_entry)) {
		gs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
	}

	if (GetThreadSelectorEntry(hThread, context32_.SegFs, &ldt_entry)) {
		fs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
	}
#elif defined(EDB_X86_64)

	gs_base_ = 0;
	fs_base_ = 0;

	if (isWow64) {
		context32_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
		Wow64GetThreadContext(hThread, &context32_);

		WOW64_LDT_ENTRY ldt_entry;
		if (Wow64GetThreadSelectorEntry(hThread, context32_.SegGs, &ldt_entry)) {
			gs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
		}

		if (Wow64GetThreadSelectorEntry(hThread, context32_.SegFs, &ldt_entry)) {
			fs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
		}
	} else {
		context64_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
		GetThreadContext(hThread, &context64_);

		// GetThreadSelectorEntry always returns false on x64
		// on x64 gs_base == TEB, maybe we can use that somehow
	}

	isWow64_ = isWow64;
#endif
}

/**
 * @brief PlatformState::setThreadState
 * @param hThread
 */
void PlatformState::setThreadState(HANDLE hThread) const {
#if defined(EDB_X86)
	SetThreadContext(hThread, &context32_);
#elif defined(EDB_X86_64)
	if (isWow64_) {
		Wow64SetThreadContext(hThread, &context32_);
	} else {
		SetThreadContext(hThread, &context64_);
	}
#endif
}

/**
 * @brief PlatformState::archRegister
 * @param type
 * @param n
 * @return
 */
Register PlatformState::archRegister(uint64_t type, size_t n) const {
	switch (type) {
	case edb::string_hash("mmx"):
		return mmxRegister(n);
	case edb::string_hash("xmm"):
		return xmmRegister(n);
	case edb::string_hash("ymm"):
		return ymmRegister(n);
	default:
		break;
	}
	return Register();
}

}
