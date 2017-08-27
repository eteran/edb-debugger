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

const std::array<const char *, GPR_COUNT> PlatformState::GPR::GPRegNames = {
	"r0",
	"r1",
	"r2",
	"r3",
	"r4",
	"r5",
	"r6",
	"r7",
	"r8",
	"r9",
	"r10",
	"r11",
	"r12",
	"sp",
	"lr",
	"pc"
};

PlatformState::PlatformState()
{
	clear();
}

IState *PlatformState::clone() const
{
	auto*const copy=new PlatformState();
	copy->gpr=gpr;
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
#ifdef EDB_ARM32
	return gp_register(GPR::PC);
#else
	return Register();  // FIXME: stub
#endif
}
Register PlatformState::flags_register() const
{
#ifdef EDB_ARM32
	if(!gpr.filled) return Register();
	return make_Register<32>("cpsr", gpr.cpsr, Register::TYPE_GPR);
#else
	return Register();  // FIXME: stub
#endif
}
edb::address_t PlatformState::frame_pointer() const
{
	return gpr.GPRegs[GPR::FP];
}
edb::address_t PlatformState::instruction_pointer() const
{
	return gpr.GPRegs[GPR::PC];
}
edb::address_t PlatformState::stack_pointer() const
{
	return gpr.GPRegs[GPR::SP];
}
edb::reg_t PlatformState::debug_register(size_t n) const
{
	return 0; // FIXME: stub
}
edb::reg_t PlatformState::flags() const
{
	return gpr.cpsr;
}
void PlatformState::adjust_stack(int bytes)
{
	// FIXME: stub
}
void PlatformState::clear()
{
	gpr.clear();
}
bool PlatformState::empty() const
{
	return gpr.empty();
}
bool PlatformState::GPR::empty() const
{
	return !filled;
}
void PlatformState::GPR::clear()
{
	util::markMemory(this, sizeof(*this));
	filled=false;
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
#ifdef EDB_ARM32
	return make_Register<32>(gpr.GPRegNames[n], gpr.GPRegs[n], Register::TYPE_GPR);
#else
	return Register(); // FIXME: stub
#endif
}

void PlatformState::fillFrom(user_regs const& regs)
{
	for(unsigned i=0;i<gpr.GPRegs.size();++i)
		gpr.GPRegs[i]=regs.uregs[i];
	gpr.cpsr=regs.uregs[16];
	gpr.filled=true;
}

}
