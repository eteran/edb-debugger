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

#ifndef PLATFORM_STATE_H_20110330_
#define PLATFORM_STATE_H_20110330_

#include "IState.h"
#include "Types.h"
#include <mach/mach.h>
#include <sys/user.h>

namespace DebuggerCore {

class PlatformState : public IState {
	friend class DebuggerCore;

public:
	PlatformState();

public:
	std::unique_ptr<IState> clone() const override;

public:
	QString flagsToString() const override;
	QString flagsToString(edb::reg_t flags) const override;
	Register value(const QString &reg) const override;
	edb::address_t frame_pointer() const override;
	edb::address_t instruction_pointer() const override;
	edb::address_t stack_pointer() const override;
	edb::reg_t debug_register(int n) const override;
	edb::reg_t flags() const override;
	long double fpu_register(int n) const override;
	void adjust_stack(int bytes) override;
	void clear() override;
	void set_debug_register(int n, edb::reg_t value) override;
	void set_flags(edb::reg_t flags) override;
	void set_instruction_pointer(edb::address_t value) override;
	void set_register(const QString &name, edb::reg_t value) override;
	quint64 mmx_register(int n) const override;
	QByteArray xmm_register(int n) const override;

private:
#if defined(EDB_X86)
	x86_thread_state32_t thread_state_;
	x86_float_state32_t float_state_;
	x86_debug_state32_t debug_state_;
	x86_exception_state32_t exception_state_;
#elif defined(EDB_X86_64)
	x86_thread_state64_t thread_state_;
	x86_float_state64_t float_state_;
	x86_debug_state64_t debug_state_;
	x86_exception_state64_t exception_state_;
#endif
};

}

#endif
