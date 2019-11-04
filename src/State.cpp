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

//------------------------------------------------------------------------------
// Name: State
// Desc: constructor
//------------------------------------------------------------------------------
State::State()
	: impl_(edb::v1::debugger_core ? edb::v1::debugger_core->createState() : nullptr) {
}

//------------------------------------------------------------------------------
// Name: ~State
// Desc:
//------------------------------------------------------------------------------
State::~State() = default;

//------------------------------------------------------------------------------
// Name: State
// Desc:
//------------------------------------------------------------------------------
State::State(const State &other)
	: impl_(other.impl_ ? other.impl_->clone() : nullptr) {
}

//------------------------------------------------------------------------------
// Name: swap
// Desc:
//------------------------------------------------------------------------------
void State::swap(State &other) {
	std::swap(impl_, other.impl_);
}

//------------------------------------------------------------------------------
// Name: operator=
// Desc:
//------------------------------------------------------------------------------
State &State::operator=(const State &rhs) {
	if (this != &rhs) {
		State(rhs).swap(*this);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void State::clear() {
	if (impl_) {
		impl_->clear();
	}
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
bool State::empty() const {
	if (impl_) {
		return impl_->empty();
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: instruction_pointer_register
// Desc:
//------------------------------------------------------------------------------
Register State::instructionPointerRegister() const {
	if (impl_) {
		return impl_->instructionPointerRegister();
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: instruction_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::instructionPointer() const {
	if (impl_) {
		return impl_->instructionPointer();
	}
	return edb::address_t(0);
}

//------------------------------------------------------------------------------
// Name: stack_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::stackPointer() const {
	if (impl_) {
		return impl_->stackPointer();
	}
	return edb::address_t(0);
}

//------------------------------------------------------------------------------
// Name: frame_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::framePointer() const {
	if (impl_) {
		return impl_->framePointer();
	}
	return edb::address_t(0);
}

//------------------------------------------------------------------------------
// Name: flags_register
// Desc:
//------------------------------------------------------------------------------
Register State::flagsRegister() const {
	if (impl_) {
		return impl_->flagsRegister();
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: flags
// Desc:
//------------------------------------------------------------------------------
edb::reg_t State::flags() const {
	if (impl_) {
		return impl_->flags();
	}
	return edb::reg_t(0);
}

//------------------------------------------------------------------------------
// Name: value
// Desc: a function to return the value of a register based on it's name
//------------------------------------------------------------------------------
Register State::value(const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: operator[]
// Desc:
//------------------------------------------------------------------------------
Register State::operator[](const QString &reg) const {
	if (impl_) {
		return impl_->value(reg);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void State::setRegister(const Register &reg) {
	if (impl_) {
		impl_->setRegister(reg);
	}
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void State::setRegister(const QString &name, edb::reg_t value) {
	if (impl_) {
		impl_->setRegister(name, value);
	}
}

//------------------------------------------------------------------------------
// Name: adjust_stack
// Desc:
//------------------------------------------------------------------------------
void State::adjustStack(int bytes) {
	if (impl_) {
		impl_->adjustStack(bytes);
	}
}

//------------------------------------------------------------------------------
// Name: set_instruction_pointer
// Desc:
//------------------------------------------------------------------------------
void State::setInstructionPointer(edb::address_t value) {
	if (impl_) {
		impl_->setInstructionPointer(value);
	}
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc:
//------------------------------------------------------------------------------
QString State::flags_to_string() const {
	if (impl_) {
		return impl_->flagsToString();
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc:
//------------------------------------------------------------------------------
QString State::flags_to_string(edb::reg_t flags) const {
	if (impl_) {
		return impl_->flagsToString(flags);
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: set_flags
// Desc:
//------------------------------------------------------------------------------
void State::setFlags(edb::reg_t flags) {
	if (impl_) {
		return impl_->setFlags(flags);
	}
}

//------------------------------------------------------------------------------
// Name: debug_register
// Desc:
//------------------------------------------------------------------------------
edb::reg_t State::debugRegister(size_t n) const {
	if (impl_) {
		return impl_->debugRegister(n);
	}
	return edb::reg_t(0);
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
void State::setDebugRegister(size_t n, edb::reg_t value) {
	if (impl_) {
		impl_->setDebugRegister(n, value);
	}
}

//------------------------------------------------------------------------------
// Name: arch_register
// Desc:
//------------------------------------------------------------------------------
Register State::archRegister(uint64_t type, size_t n) const {
	if (impl_) {
		return impl_->archRegister(type, n);
	}
	return Register();
}

#if defined(EDB_X86) || defined(EDB_X86_64)
//------------------------------------------------------------------------------
// Name: fpu_stack_pointer
// Desc:
//------------------------------------------------------------------------------
int State::fpuStackPointer() const {
	if (impl_) {
		return impl_->fpuStackPointer();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: fpu_register
// Desc:
//------------------------------------------------------------------------------
edb::value80 State::fpuRegister(size_t n) const {
	if (impl_) {
		return impl_->fpuRegister(n);
	}
	return edb::value80(std::array<std::uint8_t, 10>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

//------------------------------------------------------------------------------
// Name: fpu_register_is_empty
// Desc:
//------------------------------------------------------------------------------
bool State::fpuRegisterIsEmpty(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterIsEmpty(n);
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: fpu_status_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpuStatusWord() const {
	if (impl_) {
		return impl_->fpuStatusWord();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_control_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpuControlWord() const {
	if (impl_) {
		return impl_->fpuControlWord();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_tag_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpuTagWord() const {
	if (impl_) {
		return impl_->fpuTagWord();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_register_tag_string
// Desc:
//------------------------------------------------------------------------------
QString State::fpuRegisterTagString(std::size_t n) const {
	if (impl_) {
		return impl_->fpuRegisterTagString(n);
	}
	return QString();
}
#endif

//------------------------------------------------------------------------------
// Name: gp_register
// Desc:
//------------------------------------------------------------------------------
Register State::gpRegister(size_t n) const {
	if (impl_) {
		return impl_->gpRegister(n);
	}
	return Register();
}
