/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_STATE_H_20110330_
#define PLATFORM_STATE_H_20110330_

#include "IState.h"
#include "Types.h"
#include "edb.h"
#include <cstddef>
#include <kvm.h>
#include <machine/reg.h>

namespace DebuggerCorePlugin {

class PlatformState : public IState {
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformState();

public:
	std::unique_ptr<IState> clone() const override;

public:
	QString flagsToString() const override;
	QString flagsToString(edb::reg_t flags) const override;
	Register value(const QString &reg) const override;
	Register instructionPointeRregister() const override;
	Register flags_register() const override;
	edb::address_t frame_pointer() const override;
	edb::address_t instruction_pointer() const override;
	edb::address_t stack_pointer() const override;
	edb::reg_t debug_register(size_t n) const override;
	edb::reg_t flags() const override;
	int fpu_stack_pointer() const override;
	edb::value80 fpu_register(size_t n) const override;
	bool fpu_register_is_empty(size_t n) const override;
	QString fpu_register_tag_string(size_t n) const override;
	edb::value16 fpu_control_word() const override;
	edb::value16 fpu_status_word() const override;
	edb::value16 fpu_tag_word() const override;
	void adjust_stack(int bytes) override;
	void clear() override;
	bool empty() const override;
	void set_debug_register(size_t n, edb::reg_t value) override;
	void set_flags(edb::reg_t flags) override;
	void set_instruction_pointer(edb::address_t value) override;
	void set_register(const Register &reg) override;
	void set_register(const QString &name, edb::reg_t value) override;
	Register mmx_register(size_t n) const override;
	Register xmm_register(size_t n) const override;
	Register ymm_register(size_t n) const override;
	Register gp_register(size_t n) const override;

private:
	reg regs_;
	fpreg fpregs_;
	dbreg dbregs_;
};

}

#endif
