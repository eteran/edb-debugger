/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "State.h"
#include "IDebugger.h"
#include "IState.h"
#include "edb.h"

#include <array>

#include <QtAlgorithms>

/**
 * @brief Constructs a new State object.
 */
State::State()
	: impl_(edb::v1::debugger_core ? edb::v1::debugger_core->createState() : nullptr) {
}

/**
 * @brief Destroys the State object.
 */
State::~State() = default;

/**
 * @brief Constructs a new State object as a copy of another.
 *
 * @param other The State to copy.
 */
State::State(const State &other)
	: impl_(other.impl_ ? other.impl_->clone() : nullptr) {
}

/**
 * @brief Constructs a new State object as a move of another.
 *
 * @param other The State to move.
 */
State::State(State &&other) noexcept
	: impl_(std::move(other.impl_)) {
}

/**
 * @brief Swaps the contents of this State with another.
 *
 * @param other The State to swap with.
 */
void State::swap(State &other) noexcept {
	using std::swap;
	swap(impl_, other.impl_);
}

/**
 * @brief Moves the contents of another State to this one.
 *
 * @param rhs The State to move.
 * @return A reference to this State.
 */
State &State::operator=(State &&rhs) noexcept {
	if (this != &rhs) {
		impl_ = std::move(rhs.impl_);
	}
	return *this;
}

/**
 * @brief Copies the contents of another State to this one.
 *
 * @param rhs The State to copy.
 * @return A reference to this State.
 */
State &State::operator=(const State &rhs) {
	if (this != &rhs) {
		State(rhs).swap(*this);
	}
	return *this;
}

/**
 * @brief Clears the state.
 */
void State::clear() {
	if (impl_) {
		impl_->clear();
	}
}

/**
 * @brief Checks if the state is empty.
 *
 * @return true if the state is empty, false otherwise.
 */
bool State::empty() const {
	if (impl_) {
		return impl_->empty();
	}
	return true;
}

/**
 * @brief Returns the instruction pointer register.
 *
 * @return The instruction pointer register.
 */
Register State::instructionPointerRegister() const {
	if (impl_) {
		return impl_->instructionPointerRegister();
	}
	return Register();
}

/**
 * @brief Returns the instruction pointer.
 *
 * @return The instruction pointer.
 * @note This is often more efficient than reading the whole context and fetching the instruction pointer from it.
 */
edb::address_t State::instructionPointer() const {
	if (impl_) {
		return impl_->instructionPointer();
	}
	return edb::address_t(0);
}

/**
 * @brief Returns the stack pointer.
 *
 * @return The stack pointer.
 * @note This is often more efficient than reading the whole context and fetching the stack pointer from it.
 */
edb::address_t State::stackPointer() const {
	if (impl_) {
		return impl_->stackPointer();
	}
	return edb::address_t(0);
}

/**
 * @brief Returns the frame pointer.
 *
 * @return The frame pointer.
 * @note This is often more efficient than reading the whole context and fetching the frame pointer from it.
 */
edb::address_t State::framePointer() const {
	if (impl_) {
		return impl_->framePointer();
	}
	return edb::address_t(0);
}

/**
 * @brief Returns the flags register.
 *
 * @return The flags register.
 */
Register State::flagsRegister() const {
	if (impl_) {
		return impl_->flagsRegister();
	}
	return Register();
}

/**
 * @brief Returns the flags.
 *
 * @return The flags.
 */
edb::reg_t State::flags() const {
	if (impl_) {
		return impl_->flags();
	}
	return edb::reg_t(0);
}

/**
 * @brief Returns the value of a register based on its name.
 *
 * @param reg The name of the register.
 * @return The value of the register.
 */
Register State::value(const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

/**
 * @brief Returns the value of a register based on its name.
 *
 * @param reg The name of the register.
 * @return The value of the register.
 */
Register State::operator[](const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

/**
 * @brief Sets the value of a register.
 *
 * @param reg The register to set.
 */
void State::setRegister(const Register &reg) {
	if (impl_) {
		impl_->setRegister(reg);
	}
}

/**
 * @brief Sets the value of a register.
 *
 * @param name The name of the register.
 * @param value The value to set.
 */
void State::setRegister(const QString &name, edb::reg_t value) {
	if (impl_) {
		impl_->setRegister(name, value);
	}
}

/**
 * @brief Adjusts the stack pointer.
 *
 * @param bytes The number of bytes to adjust the stack by.
 */
void State::adjustStack(int bytes) {
	if (impl_) {
		impl_->adjustStack(bytes);
	}
}

/**
 * @brief Sets the instruction pointer.
 *
 * @param value The value to set the instruction pointer to.
 */
void State::setInstructionPointer(edb::address_t value) {
	if (impl_) {
		impl_->setInstructionPointer(value);
	}
}

/**
 * @brief Formats the flags as a string.
 *
 * @return The formatted flags.
 */
QString State::flagsToString() const {
	if (impl_) {
		return impl_->flagsToString();
	}
	return QString();
}

/**
 * @brief Formats the flags as a string.
 *
 * @param flags The flags to format.
 * @return The formatted flags.
 */
QString State::flagsToString(edb::reg_t flags) const {
	if (impl_) {
		return impl_->flagsToString(flags);
	}
	return QString();
}

/**
 * @brief Sets the flags.
 *
 * @param flags The flags to set.
 */
void State::setFlags(edb::reg_t flags) {
	if (impl_) {
		return impl_->setFlags(flags);
	}
}

/**
 * @brief Returns the value of a debug register.
 *
 * @param n The index of the debug register.
 * @return The value of the debug register.
 */
edb::reg_t State::debugRegister(size_t n) const {
	if (impl_) {
		return impl_->debugRegister(n);
	}
	return edb::reg_t(0);
}

/**
 * @brief Sets the value of a debug register.
 *
 * @param n The index of the debug register.
 * @param value The value to set.
 */
void State::setDebugRegister(size_t n, edb::reg_t value) {
	if (impl_) {
		impl_->setDebugRegister(n, value);
	}
}

/**
 * @brief Returns the value of an architecture specific register.
 *
 * @param type The type of the architecture register.
 * @param n The index of the architecture register.
 * @return The value of the architecture register.
 */
Register State::archRegister(uint64_t type, size_t n) const {
	if (impl_) {
		return impl_->archRegister(type, n);
	}
	return Register();
}

#if defined(EDB_X86) || defined(EDB_X86_64)
/**
 * @brief Returns the value of the FPU stack pointer.
 *
 * @return The value of the FPU stack pointer.
 */
int State::fpuStackPointer() const {
	if (impl_) {
		return impl_->fpuStackPointer();
	}
	return 0;
}

/**
 * @brief Returns the value of an FPU register.
 *
 * @param n The index of the FPU register.
 * @return The value of the FPU register.
 */
edb::value80 State::fpuRegister(size_t n) const {
	if (impl_) {
		return impl_->fpuRegister(n);
	}
	return edb::value80(std::array<std::uint8_t, 10>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

/**
 * @brief Returns whether an FPU register is empty.
 *
 * @param n The index of the FPU register.
 * @return True if the FPU register is empty, false otherwise.
 */
bool State::fpuRegisterIsEmpty(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterIsEmpty(n);
	}
	return true;
}

/**
 * @brief Returns the FPU status word.
 *
 * @return The FPU status word.
 */
edb::value16 State::fpuStatusWord() const {
	if (impl_) {
		return impl_->fpuStatusWord();
	}
	return edb::value16(0);
}

/**
 * @brief Returns the FPU control word.
 *
 * @return The FPU control word.
 */
edb::value16 State::fpuControlWord() const {
	if (impl_) {
		return impl_->fpuControlWord();
	}
	return edb::value16(0);
}

/**
 * @brief Returns the FPU tag word.
 *
 * @return The FPU tag word.
 */
edb::value16 State::fpuTagWord() const {
	if (impl_) {
		return impl_->fpuTagWord();
	}
	return edb::value16(0);
}

/**
 * @brief Returns the tag string for an FPU register.
 *
 * @param n The index of the FPU register.
 * @return The tag string for the FPU register.
 */
QString State::fpuRegisterTagString(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterTagString(n);
	}
	return QString();
}
#endif

/**
 * @brief Returns the value of a general purpose register.
 *
 * @param n The index of the general purpose register.
 * @return The value of the general purpose register.
 */
Register State::gpRegister(size_t n) const {
	if (impl_) {
		return impl_->gpRegister(n);
	}
	return Register();
}
