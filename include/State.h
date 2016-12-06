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

#ifndef STATE_20060715_H_
#define STATE_20060715_H_

#include "API.h"
#include "Register.h"
#include "Types.h"

#include <QMetaType>

class IState;
class QString;

namespace DebuggerCore {
	class DebuggerCore;
	class PlatformThread;
}

class EDB_EXPORT State {

	// I don't like needing to do this
	// need to revisit the IState/State/PlatformState stuff...
	friend class DebuggerCore::DebuggerCore;
	friend class DebuggerCore::PlatformThread;

public:
	State &operator=(const State &other);
	State();
	State(const State &other);
	~State();

public:
	void swap(State &other);

public:
	QString flags_to_string() const;
	QString flags_to_string(edb::reg_t flags) const;
	Register value(const QString &reg) const;
	Register instruction_pointer_register() const;
	Register flags_register() const;
	edb::address_t frame_pointer() const;
	edb::address_t instruction_pointer() const;
	edb::address_t stack_pointer() const;
	edb::reg_t debug_register(size_t n) const;
	edb::reg_t flags() const;
	Register gp_register(size_t n) const;
	int fpu_stack_pointer() const;
	edb::value80 fpu_register(size_t n) const;
	bool fpu_register_is_empty(std::size_t n) const;
	QString fpu_register_tag_string(std::size_t n) const;
	edb::value16 fpu_control_word() const;
	edb::value16 fpu_status_word() const;
	edb::value16 fpu_tag_word() const;
	Register mmx_register(std::size_t n) const;
	Register xmm_register(std::size_t n) const;
	Register ymm_register(std::size_t n) const;
	void adjust_stack(int bytes);
	void clear();
	bool empty() const;

public:
	void set_debug_register(size_t n, edb::reg_t value);
	void set_flags(edb::reg_t flags);
	void set_instruction_pointer(edb::address_t value);
	void set_register(const QString &name, edb::reg_t value);
	void set_register(const Register &reg);

public:
	Register operator[](const QString &reg) const;

private:
	IState *impl_;
};

Q_DECLARE_METATYPE(State)

#endif
