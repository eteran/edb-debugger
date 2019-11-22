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

#include "State.h"
#include "IDebugger.h"
#include "IState.h"
#include "edb.h"

#include <QtAlgorithms>

/**
 * @brief State::State
 */
State::State()
	: impl_(edb::v1::debugger_core ? edb::v1::debugger_core->createState() : nullptr) {
}

/**
 * @brief State::~State
 */
State::~State() = default;

/**
 * @brief State::State
 * @param other
 */
State::State(const State &other)
	: impl_(other.impl_ ? other.impl_->clone() : nullptr) {
}

/**
 * @brief State::State
 * @param other
 */
State::State(State &&other) noexcept
	: impl_(std::move(other.impl_)) {
}

/**
 * @brief State::swap
 * @param other
 */
void State::swap(State &other) {
	using std::swap;
	swap(impl_, other.impl_);
}

/**
 * @brief State::operator =
 * @param rhs
 * @return
 */
State &State::operator=(State &&rhs) noexcept {
	if (this != &rhs) {
		impl_ = std::move(rhs.impl_);
	}
	return *this;
}

/**
 * @brief State::operator =
 * @param rhs
 * @return
 */
State &State::operator=(const State &rhs) {
	if (this != &rhs) {
		State(rhs).swap(*this);
	}
	return *this;
}

/**
 * @brief State::clear
 */
void State::clear() {
	if (impl_) {
		impl_->clear();
	}
}

/**
 * @brief State::empty
 * @return
 */
bool State::empty() const {
	if (impl_) {
		return impl_->empty();
	}
	return true;
}

/**
 * @brief State::instructionPointerRegister
 * @return
 */
Register State::instructionPointerRegister() const {
	if (impl_) {
		return impl_->instructionPointerRegister();
	}
	return Register();
}

/**
 * @brief State::instructionPointer
 * @return
 */
edb::address_t State::instructionPointer() const {
	if (impl_) {
		return impl_->instructionPointer();
	}
	return edb::address_t(0);
}

/**
 * @brief State::stackPointer
 * @return
 */
edb::address_t State::stackPointer() const {
	if (impl_) {
		return impl_->stackPointer();
	}
	return edb::address_t(0);
}

/**
 * @brief State::framePointer
 * @return
 */
edb::address_t State::framePointer() const {
	if (impl_) {
		return impl_->framePointer();
	}
	return edb::address_t(0);
}

/**
 * @brief State::flagsRegister
 * @return
 */
Register State::flagsRegister() const {
	if (impl_) {
		return impl_->flagsRegister();
	}
	return Register();
}

/**
 * @brief State::flags
 * @return
 */
edb::reg_t State::flags() const {
	if (impl_) {
		return impl_->flags();
	}
	return edb::reg_t(0);
}

/**
 * @brief State::value
 * @param reg
 * @return the value of a register based on it's name
 */
Register State::value(const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

/**
 * @brief State::operator []
 * @param reg
 * @return
 */
Register State::operator[](const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

/**
 * @brief State::setRegister
 * @param reg
 */
void State::setRegister(const Register &reg) {
	if (impl_) {
		impl_->setRegister(reg);
	}
}

/**
 * @brief State::setRegister
 * @param name
 * @param value
 */
void State::setRegister(const QString &name, edb::reg_t value) {
	if (impl_) {
		impl_->setRegister(name, value);
	}
}

/**
 * @brief State::adjustStack
 * @param bytes
 */
void State::adjustStack(int bytes) {
	if (impl_) {
		impl_->adjustStack(bytes);
	}
}

/**
 * @brief State::setInstructionPointer
 * @param value
 */
void State::setInstructionPointer(edb::address_t value) {
	if (impl_) {
		impl_->setInstructionPointer(value);
	}
}

/**
 * @brief State::flagsToString
 * @return
 */
QString State::flagsToString() const {
	if (impl_) {
		return impl_->flagsToString();
	}
	return QString();
}

/**
 * @brief State::flagsToString
 * @param flags
 * @return
 */
QString State::flagsToString(edb::reg_t flags) const {
	if (impl_) {
		return impl_->flagsToString(flags);
	}
	return QString();
}

/**
 * @brief State::setFlags
 * @param flags
 */
void State::setFlags(edb::reg_t flags) {
	if (impl_) {
		return impl_->setFlags(flags);
	}
}

/**
 * @brief State::debugRegister
 * @param n
 * @return
 */
edb::reg_t State::debugRegister(size_t n) const {
	if (impl_) {
		return impl_->debugRegister(n);
	}
	return edb::reg_t(0);
}

/**
 * @brief State::setDebugRegister
 * @param n
 * @param value
 */
void State::setDebugRegister(size_t n, edb::reg_t value) {
	if (impl_) {
		impl_->setDebugRegister(n, value);
	}
}

/**
 * @brief State::archRegister
 * @param type
 * @param n
 * @return
 */
Register State::archRegister(uint64_t type, size_t n) const {
	if (impl_) {
		return impl_->archRegister(type, n);
	}
	return Register();
}

#if defined(EDB_X86) || defined(EDB_X86_64)
/**
 * @brief State::fpuStackPointer
 * @return
 */
int State::fpuStackPointer() const {
	if (impl_) {
		return impl_->fpuStackPointer();
	}
	return 0;
}

/**
 * @brief State::fpuRegister
 * @param n
 * @return
 */
edb::value80 State::fpuRegister(size_t n) const {
	if (impl_) {
		return impl_->fpuRegister(n);
	}
	return edb::value80(std::array<std::uint8_t, 10>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

/**
 * @brief State::fpuRegisterIsEmpty
 * @param n
 * @return
 */
bool State::fpuRegisterIsEmpty(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterIsEmpty(n);
	}
	return true;
}

/**
 * @brief State::fpuStatusWord
 * @return
 */
edb::value16 State::fpuStatusWord() const {
	if (impl_) {
		return impl_->fpuStatusWord();
	}
	return edb::value16(0);
}

/**
 * @brief State::fpuControlWord
 * @return
 */
edb::value16 State::fpuControlWord() const {
	if (impl_) {
		return impl_->fpuControlWord();
	}
	return edb::value16(0);
}

/**
 * @brief State::fpuTagWord
 * @return
 */
edb::value16 State::fpuTagWord() const {
	if (impl_) {
		return impl_->fpuTagWord();
	}
	return edb::value16(0);
}

/**
 * @brief State::fpuRegisterTagString
 * @param n
 * @return
 */
QString State::fpuRegisterTagString(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterTagString(n);
	}
	return QString();
}
#endif

/**
 * @brief State::gpRegister
 * @param n
 * @return
 */
Register State::gpRegister(size_t n) const {
	if (impl_) {
		return impl_->gpRegister(n);
	}
	return Register();
}
