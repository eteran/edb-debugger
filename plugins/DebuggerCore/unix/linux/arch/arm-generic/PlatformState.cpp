/*
Copyright (C) 2017 Ruslan Kabatsayev
                   b7.10110111@gmail.com

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

#include "PlatformState.h"

namespace DebuggerCorePlugin {

PlatformState::PlatformState()
{
}

IState *PlatformState::clone() const
{
	auto*const copy=new PlatformState();
	copy->pc=pc;
	copy->sp=sp;
	copy->filled=filled;
	return copy;
}

QString PlatformState::flags_to_string() const
{
	return "flags string"; // FIXME: stub
}
QString PlatformState::flags_to_string(edb::reg_t flags) const
{
	return "flags string"; // FIXME: stub
}
Register PlatformState::value(const QString &reg) const
{
	return Register();  // FIXME: stub
}
Register PlatformState::instruction_pointer_register() const
{
	return Register();  // FIXME: stub
}
Register PlatformState::flags_register() const
{
	return Register();  // FIXME: stub
}
edb::address_t PlatformState::frame_pointer() const
{
	return 0; // FIXME: stub
}
edb::address_t PlatformState::instruction_pointer() const
{
	return pc;
}
edb::address_t PlatformState::stack_pointer() const
{
	return sp;
}
edb::reg_t PlatformState::debug_register(size_t n) const
{
	return 0; // FIXME: stub
}
edb::reg_t PlatformState::flags() const
{
	return 0; // FIXME: stub
}
void PlatformState::adjust_stack(int bytes)
{
	// FIXME: stub
}
void PlatformState::clear()
{
	// FIXME: stub
}
bool PlatformState::empty() const
{
	return !filled; // FIXME: stub
}
void PlatformState::set_debug_register(size_t n, edb::reg_t value)
{
	// FIXME: stub
}
void PlatformState::set_flags(edb::reg_t flags)
{
	// FIXME: stub
}
void PlatformState::set_instruction_pointer(edb::address_t value)
{
	// FIXME: stub
}
void PlatformState::set_register(const Register &reg)
{
	// FIXME: stub
}
void PlatformState::set_register(const QString &name, edb::reg_t value)
{
	// FIXME: stub
}
Register PlatformState::gp_register(size_t n) const
{
	return Register(); // FIXME: stub
}

void PlatformState::fillFrom(user_regs const& regs)
{
	sp=regs.uregs[13];
	pc=regs.uregs[15];
	filled=true;
}

}
