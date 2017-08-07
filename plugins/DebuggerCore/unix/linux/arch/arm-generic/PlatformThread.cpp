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
#include <sys/syscall.h>   /* For SYS_xxx definitions */
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
// Name: PlatformThread
// Desc:
//------------------------------------------------------------------------------
PlatformThread::PlatformThread(DebuggerCore *core, IProcess *process, edb::tid_t tid) : core_(core), process_(process), tid_(tid) {
	assert(process);
	assert(core);
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PlatformThread::~PlatformThread() {
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::tid_t PlatformThread::tid() const {
	return tid_;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformThread::name() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 2) {
		return thread_stat.comm;
	}

	return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
int PlatformThread::priority() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 30) {
		return thread_stat.priority;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PlatformThread::instruction_pointer() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 18) {
		return thread_stat.kstkeip;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString PlatformThread::runState() const  {
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(process_->pid()).arg(tid_), &thread_stat);
	if(n >= 3) {
		switch(thread_stat.state) {           // 03
		case 'R':
			return tr("%1 (Running)").arg(thread_stat.state);
			break;
		case 'S':
			return tr("%1 (Sleeping)").arg(thread_stat.state);
			break;
		case 'D':
			return tr("%1 (Disk Sleep)").arg(thread_stat.state);
			break;
		case 'T':
			return tr("%1 (Stopped)").arg(thread_stat.state);
			break;
		case 't':
			return tr("%1 (Tracing Stop)").arg(thread_stat.state);
			break;
		case 'Z':
			return tr("%1 (Zombie)").arg(thread_stat.state);
			break;
		case 'X':
		case 'x':
			return tr("%1 (Dead)").arg(thread_stat.state);
			break;
		case 'W':
			return tr("%1 (Waking/Paging)").arg(thread_stat.state);
			break;
		case 'K':
			return tr("%1 (Wakekill)").arg(thread_stat.state);
			break;
		case 'P':
			return tr("%1 (Parked)").arg(thread_stat.state);
			break;
		default:
			return tr("%1").arg(thread_stat.state);
			break;
		}
	}

	return tr("Unknown");
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------
void PlatformThread::step() {
	core_->ptrace_step(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: step
// Desc: steps this thread one instruction, passing the signal that stopped it
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
void PlatformThread::step(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	core_->ptrace_step(tid_, code);
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread, passing the signal that stopped it
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------
void PlatformThread::resume() {
	core_->ptrace_continue(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread , passing the signal that stopped it
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
void PlatformThread::resume(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	core_->ptrace_continue(tid_, code);
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

//------------------------------------------------------------------------------
// Name: stop
// Desc:
//------------------------------------------------------------------------------
void PlatformThread::stop() {
	syscall(SYS_tgkill, process_->pid(), tid_, SIGSTOP);
	// TODO(eteran): should this just be this?
	//::kill(tid_, SIGSTOP);
}

//------------------------------------------------------------------------------
// Name: isPaused
// Desc: returns true if this thread is currently in the debugger's wait list
//------------------------------------------------------------------------------
bool PlatformThread::isPaused() const {
	return core_->waited_threads_.contains(tid_);
}


}
