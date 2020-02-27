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
#include "util/Container.h"

#include <QDebug>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

namespace DebuggerCorePlugin {

/**
 * @brief PlatformThread::PlatformThread
 * @param core
 * @param process
 * @param tid
 */
PlatformThread::PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, edb::tid_t tid)
	: core_(core), process_(process), tid_(tid) {
	assert(process);
	assert(core);
}

/**
 * @brief PlatformThread::tid
 * @return
 */
edb::tid_t PlatformThread::tid() const {
	return tid_;
}

/**
 * @brief PlatformThread::name
 * @return
 */
QString PlatformThread::name() const {
	struct user_stat thread_stat;
	int n = get_user_task_stat(process_->pid(), tid_, &thread_stat);
	if (n >= 2) {
		return thread_stat.comm;
	}

	return QString();
}

/**
 * @brief PlatformThread::priority
 * @return
 */
int PlatformThread::priority() const {
	struct user_stat thread_stat;
	int n = get_user_task_stat(process_->pid(), tid_, &thread_stat);
	if (n >= 18) {
		return thread_stat.priority;
	}

	return 0;
}

/**
 * @brief PlatformThread::runState
 * @return
 */
QString PlatformThread::runState() const {
	struct user_stat thread_stat;
	int n = get_user_task_stat(process_->pid(), tid_, &thread_stat);
	if (n >= 3) {
		switch (thread_stat.state) { // 03
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

/**
 * resumes this thread, passing the signal that stopped it
 * (unless the signal was SIGSTOP)
 *
 * @brief PlatformThread::resume
 * @return
 */
Status PlatformThread::resume() {
	return core_->ptraceContinue(tid_, resume_code(status_));
}

/**
 * resumes this thread, passing the signal that stopped it
 * (unless the signal was SIGSTOP, or the passed status != DEBUG_EXCEPTION_NOT_HANDLED)
 * @brief PlatformThread::resume
 * @param status
 * @return
 */
Status PlatformThread::resume(edb::EventStatus status) {
	const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(status_) : 0;
	return core_->ptraceContinue(tid_, code);
}

/**
 * @brief PlatformThread::isPaused
 * @return true if this thread is currently in the debugger's wait list
 */
bool PlatformThread::isPaused() const {
	return util::contains(core_->waitedThreads_, tid_);
}

}
