/*
 * Copyright (C) 2018- 2025 Evan Teran
 *                          evan.teran@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformThread.h"

namespace DebuggerCorePlugin {

void PlatformThread::get_state(State *state) {
	auto state_impl = static_cast<PlatformState *>(state->impl_);
	ptrace(PT_GETREGS, tid_, reinterpret_cast<char *>(&state_impl->regs_), 0);
}

void PlatformThread::set_state(const State &state) {
	auto state_impl = static_cast<PlatformState *>(state->impl_);
	ptrace(PT_SETREGS, tid_, reinterpret_cast<char *>(&state_impl->regs_), 0);
}

Status PlatformThread::step(edb::EVENT_STATUS status) {
	if (status != edb::DEBUG_STOP) {
		const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
		ptrace(PT_STEP, tid_, reinterpret_cast<caddr_t>(1), code);
	}

	return Status::Ok;
}

Status PlatformThread::resume(edb::EVENT_STATUS status) {
	if (status != edb::DEBUG_STOP) {
		const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
		ptrace(PT_CONTINUE, tid_, reinterpret_cast<caddr_t>(1), code);
	}

	return Status::Ok;
}

}
