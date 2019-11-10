/*
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.10110111@gmail.com

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

namespace DebuggerCorePlugin {

const std::array<PlatformState::GPR::RegNameVariants, GPR_COUNT> PlatformState::GPR::GPRegNames = {
	RegNameVariants{"r0", "a1"},
	RegNameVariants{"r1", "a2"},
	RegNameVariants{"r2", "a3"},
	RegNameVariants{"r3", "a4"},
	RegNameVariants{"r4", "v1"},
	RegNameVariants{"r5", "v2"},
	RegNameVariants{"r6", "v3"},
	RegNameVariants{"r7", "v4"},
	RegNameVariants{"r8", "v5"},
	RegNameVariants{"r9", "sb", "v6"},
	RegNameVariants{"r10", "sl", "v7"},
	RegNameVariants{"r11", "fp", "v8"},
	RegNameVariants{"r12", "ip", "v6"},
	RegNameVariants{"sp", "r13"},
	RegNameVariants{"lr", "r14"},
	RegNameVariants{"pc", "r15"}};

/**
 * @brief PlatformState::PlatformState
 */
PlatformState::PlatformState() {
	clear();
}

/**
 * @brief PlatformState::clone
 * @return
 */
std::unique_ptr<IState> PlatformState::clone() const {
	auto copy = std::make_unique<PlatformState>();
	copy->gpr = gpr;
	return copy;
}

/**
 * @brief PlatformState::flagsToString
 * @return
 */
QString PlatformState::flagsToString() const {
	return flagsToString(flagsRegister().valueAsInteger());
}

/**
 * @brief PlatformState::flagsToString
 * @param flags
 * @return
 */
QString PlatformState::flagsToString(edb::reg_t flags) const {
	return "flags string"; // FIXME: stub
}

/**
 * @brief PlatformState::findGPR
 * @param name
 */
auto PlatformState::findGPR(QString const &name) const -> decltype(gpr.GPRegNames.begin()) {

	return std::find_if(GPR::GPRegNames.begin(), GPR::GPRegNames.end(), [&name](const GPR::RegNameVariants &variants) {
		for (const char *const var : variants) {
			if (var == name) {
				return true;
			}
		}
		return false;
	});
}

/**
 * @brief PlatformState::value
 * @param reg
 * @return
 */
Register PlatformState::value(const QString &reg) const {
	const QString name = reg.toLower();
	if (name == "cpsr") {
		return flagsRegister();
	}

	if (vfp.filled && name == "fpscr") {
		return make_Register<32>("fpscr", vfp.fpscr, Register::TYPE_FPU);
	}

	const auto gprFoundIt = findGPR(name);
	if (gprFoundIt != GPR::GPRegNames.end()) {
		return gpRegister(gprFoundIt - GPR::GPRegNames.begin());
	}

	return Register();
}

/**
 * @brief PlatformState::instructionPointerRegister
 * @return
 */
Register PlatformState::instructionPointerRegister() const {
#ifdef EDB_ARM32
	return gpRegister(GPR::PC);
#else
	return Register(); // FIXME: stub
#endif
}

/**
 * @brief PlatformState::flagsRegister
 * @return
 */
Register PlatformState::flagsRegister() const {
#ifdef EDB_ARM32
	if (!gpr.filled)
		return Register();
	return make_Register<32>("cpsr", gpr.cpsr, Register::TYPE_GPR);
#else
	return Register(); // FIXME: stub
#endif
}

/**
 * @brief PlatformState::framePointer
 * @return
 */
edb::address_t PlatformState::framePointer() const {
	return gpr.GPRegs[GPR::FP];
}

/**
 * @brief PlatformState::instructionPointer
 * @return
 */
edb::address_t PlatformState::instructionPointer() const {
	return gpr.GPRegs[GPR::PC];
}

/**
 * @brief PlatformState::stackPointer
 * @return
 */
edb::address_t PlatformState::stackPointer() const {
	return gpr.GPRegs[GPR::SP];
}

/**
 * @brief PlatformState::debugRegister
 * @param n
 * @return
 */
edb::reg_t PlatformState::debugRegister(size_t n) const {
	return 0; // FIXME: stub
}

/**
 * @brief PlatformState::flags
 * @return
 */
edb::reg_t PlatformState::flags() const {
	return gpr.cpsr;
}

/**
 * @brief PlatformState::adjustStack
 * @param bytes
 */
void PlatformState::adjustStack(int bytes) {
	gpr.GPRegs[GPR::SP] += bytes;
}

/**
 * @brief PlatformState::clear
 */
void PlatformState::clear() {
	gpr.clear();
}

/**
 * @brief PlatformState::empty
 * @return
 */
bool PlatformState::empty() const {
	return gpr.empty();
}

/**
 * @brief PlatformState::GPR::empty
 * @return
 */
bool PlatformState::GPR::empty() const {
	return !filled;
}

/**
 * @brief PlatformState::GPR::clear
 */
void PlatformState::GPR::clear() {
	util::mark_memory(this, sizeof(*this));
	filled = false;
}

/**
 * @brief PlatformState::setDebugRegister
 * @param n
 * @param value
 */
void PlatformState::setDebugRegister(size_t n, edb::reg_t value) {
	// FIXME: stub
}

/**
 * @brief PlatformState::setFlags
 * @param flags
 */
void PlatformState::setFlags(edb::reg_t flags) {
	gpr.cpsr = flags;
}

/**
 * @brief PlatformState::setInstructionPointer
 * @param value
 */
void PlatformState::setInstructionPointer(edb::address_t value) {
	gpr.GPRegs[GPR::PC] = value;
}

/**
 * @brief PlatformState::setRegister
 * @param reg
 */
void PlatformState::setRegister(const Register &reg) {
	if (!reg) {
		return;
	}

	const QString name = reg.name().toLower();
	if (name == "cpsr") {
		setFlags(reg.value<edb::reg_t>());
		return;
	}

	if (name == "fpscr") {
		vfp.fpscr = reg.value<decltype(vfp.fpscr)>();
		return;
	}

	const auto gprFoundIt = findGPR(name);
	if (gprFoundIt != GPR::GPRegNames.end()) {
		const auto index = gprFoundIt - GPR::GPRegNames.begin();
		assert(index < 16);
		gpr.GPRegs[index] = reg.value<edb::reg_t>();
		return;
	}
}

/**
 * @brief PlatformState::setRegister
 * @param name
 * @param value
 */
void PlatformState::setRegister(const QString &name, edb::reg_t value) {
#ifdef EDB_ARM32
	const QString regName = name.toLower();
	setRegister(make_Register<32>(regName, value, Register::TYPE_GPR));
	// FIXME: this doesn't take into account any 64-bit registers - possibly FPU data?
#endif
}

/**
 * @brief PlatformState::gpRegister
 * @param n
 * @return
 */
Register PlatformState::gpRegister(size_t n) const {
#ifdef EDB_ARM32
	if (n < GPR::GPRegNames.size())
		return make_Register<32>(gpr.GPRegNames[n].front(), gpr.GPRegs[n], Register::TYPE_GPR);
	return Register();
#else
	return Register(); // FIXME: stub
#endif
}

/**
 * @brief PlatformState::fillFrom
 * @param regs
 */
void PlatformState::fillFrom(user_regs const &regs) {
	for (unsigned i = 0; i < gpr.GPRegs.size(); ++i) {
		gpr.GPRegs[i] = regs.uregs[i];
	}

	gpr.cpsr   = regs.uregs[16];
	gpr.filled = true;
}

/**
 * @brief PlatformState::fillFrom
 * @param regs
 */
void PlatformState::fillFrom(user_vfp const &regs) {
	for (unsigned i = 0; i < vfp.d.size(); ++i) {
		vfp.d[i] = regs.fpregs[i];
	}

	vfp.fpscr  = regs.fpscr;
	vfp.filled = true;
}

/**
 * @brief PlatformState::fillStruct
 * @param regs
 */
void PlatformState::fillStruct(user_regs &regs) const {
	util::mark_memory(&regs, sizeof(regs));
	if (gpr.filled) {
		for (unsigned i = 0; i < gpr.GPRegs.size(); ++i) {
			regs.uregs[i] = gpr.GPRegs[i];
		}

		regs.uregs[16] = gpr.cpsr;
		// FIXME: uregs[17] is not filled
	}
}

/**
 * @brief PlatformState::fillStruct
 * @param regs
 */
void PlatformState::fillStruct(user_vfp &regs) const {
	util::mark_memory(&regs, sizeof(regs));
	if (vfp.filled) {
		for (unsigned i = 0; i < vfp.d.size(); ++i) {
			regs.fpregs[i] = vfp.d[i];
		}

		regs.fpscr = vfp.fpscr;
	}
}

}
