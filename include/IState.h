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

#ifndef ISTATE_H_20110315_
#define ISTATE_H_20110315_

#include "Register.h"
#include "Types.h"

// TODO(eteran): This file is still too taylored to x86 and family
// we should develop a better abstraction model at some point

class IState {
public:
	virtual ~IState() = default;

public:
	virtual std::unique_ptr<IState> clone() const = 0;

public:
	virtual QString flagsToString() const                           = 0;
	virtual QString flagsToString(edb::reg_t flags) const           = 0;
	virtual Register value(const QString &reg) const                = 0;
	virtual Register instructionPointerRegister() const             = 0;
	virtual Register flagsRegister() const                          = 0;
	virtual edb::address_t framePointer() const                     = 0;
	virtual edb::address_t instructionPointer() const               = 0;
	virtual edb::address_t stackPointer() const                     = 0;
	virtual edb::reg_t debugRegister(size_t n) const                = 0;
	virtual edb::reg_t flags() const                                = 0;
	virtual void adjustStack(int bytes)                             = 0;
	virtual void clear()                                            = 0;
	virtual bool empty() const                                      = 0;
	virtual void setDebugRegister(size_t n, edb::reg_t value)       = 0;
	virtual void setFlags(edb::reg_t flags)                         = 0;
	virtual void setInstructionPointer(edb::address_t value)        = 0;
	virtual void setRegister(const QString &name, edb::reg_t value) = 0;
	virtual void setRegister(const Register &reg)                   = 0;

public:
	// GP
	virtual Register gpRegister(size_t n) const = 0;

public:
	// This is a more generic means to request architecture
	// specific registers. The type should be the result of
	// edb::string_hash, for example:
	// edb::string_hash("mmx"), string_hash("xmm"), and string_hash("ymm")
	// This will allow this interface to be much more platform independent
	virtual Register archRegister(uint64_t type, size_t n) const = 0;

#if defined(EDB_X86) || defined(EDB_X86_64)
public:
	// FPU
	virtual int fpuStackPointer() const                       = 0;
	virtual bool fpuRegisterIsEmpty(std::size_t n) const      = 0;
	virtual edb::value80 fpuRegister(size_t n) const          = 0;
	virtual QString fpuRegisterTagString(std::size_t n) const = 0;
	virtual edb::value16 fpuControlWord() const               = 0;
	virtual edb::value16 fpuStatusWord() const                = 0;
	virtual edb::value16 fpuTagWord() const                   = 0;
#endif
};

#endif
