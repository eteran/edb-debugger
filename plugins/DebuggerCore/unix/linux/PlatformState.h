/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef PLATFORMSTATE_20110330_H_
#define PLATFORMSTATE_20110330_H_

#include "IState.h"
#include "Types.h"
#include <cstddef>
#include <sys/user.h>

namespace DebuggerCore {

static constexpr std::size_t FPU_REG_COUNT=8;
static constexpr std::size_t MMX_REG_COUNT=FPU_REG_COUNT;
#if defined EDB_X86
static constexpr std::size_t GPR_COUNT=8;
#elif defined EDB_X86_64
static constexpr std::size_t GPR_COUNT=16;
#endif
static constexpr std::size_t XMM_REG_COUNT=GPR_COUNT;
static constexpr std::size_t YMM_REG_COUNT=GPR_COUNT;
static constexpr std::size_t ZMM_REG_COUNT=GPR_COUNT;

static constexpr bool gprIndexValid(std::size_t n) { return n<GPR_COUNT; }
static constexpr bool fpuIndexValid(std::size_t n) { return n<FPU_REG_COUNT; }
static constexpr bool mmxIndexValid(std::size_t n) { return n<MMX_REG_COUNT; }
static constexpr bool xmmIndexValid(std::size_t n) { return n<XMM_REG_COUNT; }
static constexpr bool ymmIndexValid(std::size_t n) { return n<YMM_REG_COUNT; }
static constexpr bool zmmIndexValid(std::size_t n) { return n<ZMM_REG_COUNT; }

class PlatformState : public IState {
	friend class DebuggerCore;

public:
	PlatformState();

public:
	virtual IState *clone() const;

public:
	virtual QString flags_to_string() const;
	virtual QString flags_to_string(edb::reg_t flags) const;
	virtual Register value(const QString &reg) const;
	virtual edb::address_t frame_pointer() const;
	virtual edb::address_t instruction_pointer() const;
	virtual edb::address_t stack_pointer() const;
	virtual edb::reg_t debug_register(int n) const;
	virtual edb::reg_t flags() const;
	virtual int fpu_stack_pointer() const;
	virtual edb::value80 fpu_register(int n) const;
	virtual bool fpu_register_is_empty(std::size_t n) const;
	virtual QString fpu_register_tag_string(std::size_t n) const;
	virtual edb::value16 fpu_control_word() const;
	virtual edb::value16 fpu_status_word() const;
	virtual edb::value16 fpu_tag_word() const;
	virtual void adjust_stack(int bytes);
	virtual void clear();
	virtual void set_debug_register(int n, edb::reg_t value);
	virtual void set_flags(edb::reg_t flags);
	virtual void set_instruction_pointer(edb::address_t value);
	virtual void set_register(const QString &name, edb::reg_t value);
	virtual edb::value64 mmx_register(int n) const;
	virtual edb::value128 xmm_register(int n) const;
	virtual Register gp_register(int n) const;

private:
	int fpu_register_tag(int n) const;
	int recreate_fpu_register_tag(edb::value80 regval) const;
	std::size_t fpu_fixup_index(std::size_t n) const;

private:
	struct user_regs_struct   regs_;
#if defined(EDB_X86)
	struct user_fpxregs_struct fpregs_;
#elif defined(EDB_X86_64)
	struct user_fpregs_struct fpregs_;
#endif
	edb::reg_t                dr_[8];
#if defined(EDB_X86)
	edb::address_t            fs_base;
	edb::address_t            gs_base;
#endif
};

}

#endif

