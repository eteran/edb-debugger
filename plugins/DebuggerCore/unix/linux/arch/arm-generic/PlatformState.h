/*
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.1010111@gmail.com

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

#ifndef PLATFORM_STATE_20170806_H_
#define PLATFORM_STATE_20170806_H_

#include "IState.h"
#include "Types.h"
#include "PrStatus.h"
#include "edb.h"
#include <cstddef>
#include <sys/user.h>

namespace DebuggerCorePlugin {

class PlatformState : public IState {
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformState();

public:
	IState *clone() const override;

	QString flags_to_string() const override;
	QString flags_to_string(edb::reg_t flags) const override;
	Register value(const QString &reg) const override;
	Register instruction_pointer_register() const override;
	Register flags_register() const override;
	edb::address_t frame_pointer() const override;
	edb::address_t instruction_pointer() const override;
	edb::address_t stack_pointer() const override;
	edb::reg_t debug_register(size_t n) const override;
	edb::reg_t flags() const override;
	void adjust_stack(int bytes) override;
	void clear() override;
	bool empty() const override;
	void set_debug_register(size_t n, edb::reg_t value) override;
	void set_flags(edb::reg_t flags) override;
	void set_instruction_pointer(edb::address_t value) override;
	void set_register(const Register &reg) override;
	void set_register(const QString &name, edb::reg_t value) override;
	Register gp_register(size_t n) const override;
};

}

#endif
