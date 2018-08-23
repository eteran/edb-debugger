/*
Copyright (C) 2015 - 2015 Evan Teran
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
#include "DebuggerCore.h"
#include "IProcess.h"
#include "PlatformCommon.h"
#include <QDebug>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <sys/syscall.h>   /* For SYS_xxx definitions */

namespace DebuggerCorePlugin {

//------------------------------------------------------------------------------
// Name: PlatformThread
// Desc:
//------------------------------------------------------------------------------
PlatformThread::PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, edb::tid_t tid) : core_(core), process_(process), tid_(tid) {
	assert(process);
	assert(core);
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
	// FIXME(ARM): doesn't work at least on ARM32
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
		case 'S':
			return tr("%1 (Sleeping)").arg(thread_stat.state);
		case 'D':
			return tr("%1 (Disk Sleep)").arg(thread_stat.state);
		case 'T':
			return tr("%1 (Stopped)").arg(thread_stat.state);
		case 't':
			return tr("%1 (Tracing Stop)").arg(thread_stat.state);
		case 'Z':
			return tr("%1 (Zombie)").arg(thread_stat.state);
		case 'X':
		case 'x':
			return tr("%1 (Dead)").arg(thread_stat.state);
		case 'W':
			return tr("%1 (Waking/Paging)").arg(thread_stat.state);
		case 'K':
			return tr("%1 (Wakekill)").arg(thread_stat.state);
		case 'P':
			return tr("%1 (Parked)").arg(thread_stat.state);
		default:
			return tr("%1").arg(thread_stat.state);
		}
	}

	return tr("Unknown");
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread, passing the signal that stopped it
//       (unless the signal was SIGSTOP)
//------------------------------------------------------------------------------
Status PlatformThread::resume() {
	return core_->ptrace_continue(tid_, resume_code(status_));
}

//------------------------------------------------------------------------------
// Name: resume
// Desc: resumes this thread , passing the signal that stopped it
//       (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
//------------------------------------------------------------------------------
Status PlatformThread::resume(edb::EVENT_STATUS status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	return core_->ptrace_continue(tid_, code);
}

//------------------------------------------------------------------------------
// Name: stop
// Desc:
//------------------------------------------------------------------------------
Status PlatformThread::stop() {
	if(syscall(SYS_tgkill, process_->pid(), tid_, SIGSTOP)==-1) {

		const char*const strError=strerror(errno);
		qWarning() << "Unable to stop thread" << tid_ << ": tgkill failed:" << strError;
		return Status(strError);
	}
	// TODO(eteran): should this just be this?
	//::kill(tid_, SIGSTOP);
	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: isPaused
// Desc: returns true if this thread is currently in the debugger's wait list
//------------------------------------------------------------------------------
bool PlatformThread::isPaused() const {
	return core_->waited_threads_.contains(tid_);
}

}
