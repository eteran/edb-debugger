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

#ifndef STATE_H_20060715_
#define STATE_H_20060715_

#include "API.h"
#include "Types.h"

class IState;
class Register;
class QString;

namespace DebuggerCorePlugin {
class DebuggerCore;
class PlatformThread;
}

// NOTE(eteran): the purpose of this class is simply to give IState objects
// value semantics, so it tends to nearly replicate that interface.
class EDB_EXPORT State {

	// TODO(eteran): I don't like needing to do this
	// need to revisit the IState/State/PlatformState stuff...
	friend class DebuggerCorePlugin::DebuggerCore;
	friend class DebuggerCorePlugin::PlatformThread;

public:
	State();
	State(const State &other);
	State &operator=(const State &rhs);
	State(State &&other) noexcept;
	State &operator=(State &&rhs) noexcept;
	~State();

public:
	void swap(State &other);

public:
	QString flagsToString() const;
	QString flagsToString(edb::reg_t flags) const;
	Register value(const QString &reg) const;
	Register instructionPointerRegister() const;
	Register flagsRegister() const;
	edb::address_t framePointer() const;
	edb::address_t instructionPointer() const;
	edb::address_t stackPointer() const;
	edb::reg_t debugRegister(size_t n) const;
	edb::reg_t flags() const;
	Register gpRegister(size_t n) const;
	Register archRegister(uint64_t type, size_t n) const;
	void adjustStack(int bytes);
	void clear();
	bool empty() const;

public:
#if defined(EDB_X86) || defined(EDB_X86_64)
	int fpuStackPointer() const;
	edb::value80 fpuRegister(size_t n) const;
	bool fpuRegisterIsEmpty(std::size_t n) const;
	QString fpuRegisterTagString(std::size_t n) const;
	edb::value16 fpuControlWord() const;
	edb::value16 fpuStatusWord() const;
	edb::value16 fpuTagWord() const;
#endif

public:
	void setDebugRegister(size_t n, edb::reg_t value);
	void setFlags(edb::reg_t flags);
	void setInstructionPointer(edb::address_t value);
	void setRegister(const QString &name, edb::reg_t value);
	void setRegister(const Register &reg);

public:
	Register operator[](const QString &reg) const;

private:
	std::unique_ptr<IState> impl_;
};

Q_DECLARE_METATYPE(State)

#endif
