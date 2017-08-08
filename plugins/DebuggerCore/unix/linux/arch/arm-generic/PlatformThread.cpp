/*
Copyright (C) 2015 - 2015 Evan Teran
                          evan.teran@gmail.com
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

#include "PlatformThread.h"
#include "DebuggerCore.h"
#include "IProcess.h"
#include "PlatformCommon.h"
#include "PlatformState.h"
#include <QtDebug>
#include "State.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <elf.h>
#include <linux/uio.h>
#include <sys/ptrace.h>
#include <sys/user.h>


// doesn't always seem to be defined in the headers
#ifndef PTRACE_GET_THREAD_AREA
#define PTRACE_GET_THREAD_AREA static_cast<__ptrace_request>(25)
#endif

#ifndef PTRACE_SET_THREAD_AREA
#define PTRACE_SET_THREAD_AREA static_cast<__ptrace_request>(26)
#endif

#ifndef PTRACE_GETSIGINFO
#define PTRACE_GETSIGINFO static_cast<__ptrace_request>(0x4202)
#endif

#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET static_cast<__ptrace_request>(0x4204)
#endif

#ifndef PTRACE_SETREGSET
#define PTRACE_SETREGSET static_cast<__ptrace_request>(0x4205)
#endif

namespace DebuggerCorePlugin {

//------------------------------------------------------------------------------
// Name: fillStateFromPrStatus
// Desc:
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromPrStatus(PlatformState* state) {

	return false;
}

//------------------------------------------------------------------------------
// Name: fillStateFromSimpleRegs
// Desc:
//------------------------------------------------------------------------------
bool PlatformThread::fillStateFromSimpleRegs(PlatformState* state) {

	user_regs regs;
	if(ptrace(PTRACE_GETREGS, tid_, 0, &regs) != -1) {

		state->fillFrom(regs);
		return true;
	}
	else {
		perror("PTRACE_GETREGS failed");
		return false;
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::get_state(State *state) {
	// TODO: assert that we are paused

	core_->detectDebuggeeBitness();

	if(auto state_impl = static_cast<PlatformState *>(state->impl_)) {

		fillStateFromSimpleRegs(state_impl);
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::set_state(const State &state) {

	// TODO: assert that we are paused

	if(auto state_impl = static_cast<PlatformState *>(state.impl_)) {

	}
}

//------------------------------------------------------------------------------
// Name: get_debug_register
// Desc:
//------------------------------------------------------------------------------
unsigned long PlatformThread::get_debug_register(std::size_t n) {
	return 0;
}

//------------------------------------------------------------------------------
// Name: set_debug_register
// Desc:
//------------------------------------------------------------------------------
long PlatformThread::set_debug_register(std::size_t n, long value) {
	return 0;
}

}
