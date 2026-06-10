/*
 * Copyright (C) 2017 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Constructs a PlatformState with all register values cleared.
 */
PlatformState::PlatformState() {
	clear();
}

/**
 * @brief Creates and returns a heap-allocated copy of this register state.
 * @return
 */
std::unique_ptr<IState> PlatformState::clone() const {
	auto copy = std::make_unique<PlatformState>();
	copy->gpr = gpr;
	return copy;
}

/**
 * @brief Returns a string representation of the current CPSR flags.
 * @return
 */
QString PlatformState::flagsToString() const {
	return flagsToString(flagsRegister().valueAsInteger());
}

/**
 * @brief Returns a string representation of the given CPSR flags value.
 * @param flags
 * @return
 */
QString PlatformState::flagsToString(edb::reg_t flags) const {
	return "flags string"; // FIXME: stub
}

/**
 * @brief Finds and returns an iterator to the GPR name entry matching the given register name.
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
 * @brief Returns the Register value for the named register (GPR, CPSR, or FPSCR).
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
 * @brief Returns the instruction pointer register (PC) as a Register object.
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
 * @brief Returns the flags register (CPSR) as a Register object.
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
 * @brief Returns the current value of the frame pointer register (FP).
 * @return
 */
edb::address_t PlatformState::framePointer() const {
	return gpr.GPRegs[GPR::FP];
}

/**
 * @brief Returns the current value of the instruction pointer (PC).
 * @return
 */
edb::address_t PlatformState::instructionPointer() const {
	return gpr.GPRegs[GPR::PC];
}

/**
 * @brief Returns the current value of the stack pointer (SP).
 * @return
 */
edb::address_t PlatformState::stackPointer() const {
	return gpr.GPRegs[GPR::SP];
}

/**
 * @brief Returns the value of the hardware debug register n (currently a stub returning 0).
 * @param n
 * @return
 */
edb::reg_t PlatformState::debugRegister(size_t n) const {
	return 0; // FIXME: stub
}

/**
 * @brief Returns the current CPSR (flags) register value.
 * @return
 */
edb::reg_t PlatformState::flags() const {
	return gpr.cpsr;
}

/**
 * @brief Adjusts the stack pointer by adding the given number of bytes.
 * @param bytes
 */
void PlatformState::adjustStack(int bytes) {
	gpr.GPRegs[GPR::SP] += bytes;
}

/**
 * @brief Resets all register state to zeroed/uninitialized values.
 */
void PlatformState::clear() {
	gpr.clear();
}

/**
 * @brief Returns true if no register data has been loaded into this state object.
 * @return
 */
bool PlatformState::empty() const {
	return gpr.empty();
}

/**
 * @brief Returns true if no GPR data has been filled into this register group.
 * @return
 */
bool PlatformState::GPR::empty() const {
	return !filled;
}

/**
 * @brief Clears all GPR values and marks the group as unfilled.
 */
void PlatformState::GPR::clear() {
	util::mark_memory(this, sizeof(*this));
	filled = false;
}

/**
 * @brief Sets hardware debug register n to the given value (currently a stub).
 * @param n
 * @param value
 */
void PlatformState::setDebugRegister(size_t n, edb::reg_t value) {
	// FIXME: stub
}

/**
 * @brief Sets the CPSR (flags) register to the given value.
 * @param flags
 */
void PlatformState::setFlags(edb::reg_t flags) {
	gpr.cpsr = flags;
}

/**
 * @brief Sets the instruction pointer (PC) to the given address.
 * @param value
 */
void PlatformState::setInstructionPointer(edb::address_t value) {
	gpr.GPRegs[GPR::PC] = value;
}

/**
 * @brief Sets the named register from the given Register object.
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
 * @brief Sets the named register to the given 32-bit value.
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
 * @brief Returns the general-purpose register at index n as a Register object.
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
 * @brief Fills the GPR state from a user_regs ptrace structure.
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
 * @brief Fills the VFP floating-point state from a user_vfp ptrace structure.
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
 * @brief Fills a user_regs struct with the current GPR values for use with PTRACE_SETREGS.
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
 * @brief Fills a user_vfp struct with the current VFP values for use with PTRACE_SETVFPREGS.
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
