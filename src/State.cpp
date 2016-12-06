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
#include "edb.h"
#include "IDebugger.h"
#include "IState.h"

#include <QtAlgorithms>

//------------------------------------------------------------------------------
// Name: State
// Desc: constructor
//------------------------------------------------------------------------------
State::State() : impl_(edb::v1::debugger_core ? edb::v1::debugger_core->create_state() : 0) {
}

//------------------------------------------------------------------------------
// Name: ~State
// Desc:
//------------------------------------------------------------------------------
State::~State() {
	delete impl_;
}

//------------------------------------------------------------------------------
// Name: State
// Desc:
//------------------------------------------------------------------------------
State::State(const State &other) : impl_(other.impl_ ? other.impl_->clone() : 0) {
}

//------------------------------------------------------------------------------
// Name: swap
// Desc:
//------------------------------------------------------------------------------
void State::swap(State &other) {
	qSwap(impl_, other.impl_);
}

//------------------------------------------------------------------------------
// Name: operator=
// Desc:
//------------------------------------------------------------------------------
State &State::operator=(const State &other) {
	if(this != &other) {
		State(other).swap(*this);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void State::clear() {
	if(impl_) {
		impl_->clear();
	}
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
bool State::empty() const {
	if(impl_) {
		return impl_->empty();
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: instruction_pointer_register
// Desc:
//------------------------------------------------------------------------------
Register State::instruction_pointer_register() const {
	if(impl_) {
		return impl_->instruction_pointer_register();
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: instruction_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::instruction_pointer() const {
	if(impl_) {
		return impl_->instruction_pointer();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: stack_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::stack_pointer() const {
	if(impl_) {
		return impl_->stack_pointer();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: frame_pointer
// Desc:
//------------------------------------------------------------------------------
edb::address_t State::frame_pointer() const {
	if(impl_) {
		return impl_->frame_pointer();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: flags_register
// Desc:
//------------------------------------------------------------------------------
Register State::flags_register() const {
	if(impl_) {
		return impl_->flags_register();
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: flags
// Desc:
//------------------------------------------------------------------------------
edb::reg_t State::flags() const {
	if(impl_) {
		return impl_->flags();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: value
// Desc: a function to return the value of a register based on it's name
//------------------------------------------------------------------------------
Register State::value(const QString &reg) const {
	if(impl_) {
		return impl_->value(reg);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: operator[]
// Desc:
//------------------------------------------------------------------------------
Register State::operator[](const QString &reg) const {
	if(impl_) {
		return impl_->value(reg);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void State::set_register(const Register& reg) {
	if(impl_) {
		impl_->set_register(reg);
	}
}

//------------------------------------------------------------------------------
// Name: set_register
// Desc:
//------------------------------------------------------------------------------
void State::set_register(const QString &name, edb::reg_t value) {
	if(impl_) {
		impl_->set_register(name, value);
	}
}

//------------------------------------------------------------------------------
// Name: adjust_stack
// Desc:
//------------------------------------------------------------------------------
void State::adjust_stack(int bytes) {
	if(impl_) {
		impl_->adjust_stack(bytes);
	}
}

//------------------------------------------------------------------------------
// Name: set_instruction_pointer
// Desc:
//------------------------------------------------------------------------------
void State::set_instruction_pointer(edb::address_t value) {
	if(impl_) {
		impl_->set_instruction_pointer(value);
	}
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc:
//------------------------------------------------------------------------------
QString State::flags_to_string() const {
	if(impl_) {
		return impl_->flags_to_string();
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: flags_to_string
// Desc:
//------------------------------------------------------------------------------
QString State::flags_to_string(edb::reg_t flags) const {
	if(impl_) {
		return impl_->flags_to_string(flags);
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: set_flags
// Desc:
//------------------------------------------------------------------------------
void State::set_flags(edb::reg_t flags) {
	if(impl_) {
		return impl_->set_flags(flags);
	}
}

//------------------------------------------------------------------------------
// Name: debug_register
// Desc:
//------------------------------------------------------------------------------
edb::reg_t State::debug_register(size_t n) const {
	if(impl_) {
		return impl_->debug_register(n);
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
void State::set_debug_register(size_t n, edb::reg_t value) {
	if(impl_) {
		impl_->set_debug_register(n, value);
	}
}

//------------------------------------------------------------------------------
// Name: fpu_stack_pointer
// Desc:
//------------------------------------------------------------------------------
int State::fpu_stack_pointer() const {
	if(impl_) {
		return impl_->fpu_stack_pointer();
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: fpu_register
// Desc:
//------------------------------------------------------------------------------
edb::value80 State::fpu_register(size_t n) const {
	if(impl_) {
		return impl_->fpu_register(n);
	}
	return edb::value80(0.0L);
}

//------------------------------------------------------------------------------
// Name: fpu_register_is_empty
// Desc:
//------------------------------------------------------------------------------
bool State::fpu_register_is_empty(std::size_t n) const {
	if(impl_) {
		return impl_->fpu_register_is_empty(n);
	}
	return true;
}

//------------------------------------------------------------------------------
// Name: fpu_status_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpu_status_word() const {
	if(impl_) {
		return impl_->fpu_status_word();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_control_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpu_control_word() const {
	if(impl_) {
		return impl_->fpu_control_word();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_tag_word
// Desc:
//------------------------------------------------------------------------------
edb::value16 State::fpu_tag_word() const {
	if(impl_) {
		return impl_->fpu_tag_word();
	}
	return edb::value16(0);
}

//------------------------------------------------------------------------------
// Name: fpu_register_tag_string
// Desc:
//------------------------------------------------------------------------------
QString State::fpu_register_tag_string(std::size_t n) const {
	if(impl_) {
		return impl_->fpu_register_tag_string(n);
	}
	return QString();
}

//------------------------------------------------------------------------------
// Name: mmx_register
// Desc:
//------------------------------------------------------------------------------
Register State::mmx_register(std::size_t n) const {
	if(impl_) {
		return impl_->mmx_register(n);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: xmm_register
// Desc:
//------------------------------------------------------------------------------
Register State::xmm_register(std::size_t n) const {
	if(impl_) {
		return impl_->xmm_register(n);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: xmm_register
// Desc:
//------------------------------------------------------------------------------
Register State::ymm_register(std::size_t n) const {
	if(impl_) {
		return impl_->ymm_register(n);
	}
	return Register();
}

//------------------------------------------------------------------------------
// Name: gp_register
// Desc:
//------------------------------------------------------------------------------
Register State::gp_register(size_t n) const {
	if(impl_) {
		return impl_->gp_register(n);
	}
	return Register();
}
