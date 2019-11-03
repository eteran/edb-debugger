/*
Copyright (C) 2018 - 2018 Evan Teran
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
