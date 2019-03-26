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

#ifndef PLATFORMSTATE_20110330_H_
#define PLATFORMSTATE_20110330_H_

#include "IState.h"
#include "Types.h"
#include <Windows.h>

namespace DebuggerCorePlugin {

class PlatformState : public IState {
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformState() = default;

public:
	std::unique_ptr<IState> clone() const override;

public:
	long double fpu_register(int n) const ;
	quint64 mmx_register(int n) const ;
	QByteArray xmm_register(int n) const ;

public:
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
	bool empty() const override {
		qDebug("TODO: implement PlatformState::empty"); return true;
	}
	void set_debug_register(size_t n, edb::reg_t value) override;
	void set_flags(edb::reg_t flags) override;
	void set_instruction_pointer(edb::address_t value) override;
	void set_register(const QString &name, edb::reg_t value) override;
	void set_register(const Register &reg) override {
		qDebug("TODO: implement PlatformState::set_register");
	}

	Register arch_register(uint64_t type, size_t n) const override;

	Register mmx_register(size_t n) const override {
		qDebug("TODO: implement PlatformState::mmx_register"); return Register();
	}
	Register xmm_register(size_t n) const override {
		qDebug("TODO: implement PlatformState::xmm_register"); return Register();
	}
	Register ymm_register(size_t n) const override {
		qDebug("TODO: implement PlatformState::ymm_register"); return Register();
	}
	Register gp_register(size_t n) const override {
		qDebug("TODO: implement PlatformState::gp_register"); return Register();
	}
	int fpu_stack_pointer() const override {
		qDebug("TODO: implement PlatformState::fpu_stack_pointer"); return 0;
	}
	edb::value80 fpu_register(size_t n) const override {
		qDebug("TODO: implement PlatformState::fpu_register"); return edb::value80();
	}
	bool fpu_register_is_empty(size_t n) const override {
		qDebug("TODO: implement PlatformState::fpu_register_is_empty"); return true;
	}
	QString fpu_register_tag_string(size_t n) const override {
		qDebug("TODO: implement PlatformState::fpu_register_tag_string"); return "";
	}
	edb::value16 fpu_control_word() const override {
		qDebug("TODO: implement PlatformState::fpu_control_word"); return edb::value16();
	}
	edb::value16 fpu_status_word() const override {
		qDebug("TODO: implement PlatformState::fpu_status_word"); return edb::value16();
	}
	edb::value16 fpu_tag_word() const override {
		qDebug("TODO: implement PlatformState::fpu_tag_word"); return edb::value16();
	}

public:
	void getThreadState(HANDLE hThread, bool isWow64);
	void setThreadState(HANDLE hThread) const;

private:
#ifdef EDB_X86_64
	union {
		CONTEXT        context64_ = {};
		WOW64_CONTEXT  context32_;
	};
	bool is_wow64_ = false;
#else
	CONTEXT        context32_;
#endif
	edb::address_t fs_base_ = 0;
	edb::address_t gs_base_ = 0;
};

}

#endif
