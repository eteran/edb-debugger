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

#ifndef ISTATE_20110315_H_
#define ISTATE_20110315_H_

#include "Types.h"
#include "API.h"
#include "Register.h"

// TODO(eteran): This file is still too taylored to x86 and family
// we should develop a better abstraction model at some point

class EDB_EXPORT IState {
public:
	virtual ~IState() {}

public:
	virtual IState *clone() const = 0;

public:
	virtual QString flags_to_string() const = 0;
	virtual QString flags_to_string(edb::reg_t flags) const = 0;
	virtual Register value(const QString &reg) const = 0;
	virtual Register instruction_pointer_register() const = 0;
	virtual Register flags_register() const = 0;
	virtual edb::address_t frame_pointer() const = 0;
	virtual edb::address_t instruction_pointer() const = 0;
	virtual edb::address_t stack_pointer() const = 0;
	virtual edb::reg_t debug_register(size_t n) const = 0;
	virtual edb::reg_t flags() const = 0;
	virtual void adjust_stack(int bytes) = 0;
	virtual void clear() = 0;
	virtual bool empty() const = 0;
	virtual void set_debug_register(size_t n, edb::reg_t value) = 0;
	virtual void set_flags(edb::reg_t flags) = 0;
	virtual void set_instruction_pointer(edb::address_t value) = 0;
	virtual void set_register(const QString &name, edb::reg_t value) = 0;
	virtual void set_register(const Register &reg) = 0;

public:
	// GP
	virtual Register gp_register(size_t n) const = 0;

public:
	// FPU
	virtual int fpu_stack_pointer() const = 0;
	virtual bool fpu_register_is_empty(std::size_t n) const = 0;
	virtual edb::value80 fpu_register(size_t n) const = 0;
	virtual QString fpu_register_tag_string(std::size_t n) const = 0;
	virtual edb::value16 fpu_control_word() const = 0;
	virtual edb::value16 fpu_status_word() const = 0;
	virtual edb::value16 fpu_tag_word() const = 0;

public:
	// MMX
	virtual Register mmx_register(std::size_t n) const = 0;
	
public:
	// SSE
	virtual Register xmm_register(std::size_t n) const = 0;
public:
	// AVX
	virtual Register ymm_register(std::size_t n) const = 0;
};

#endif
