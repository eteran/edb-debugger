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


// TODO: research usage of process_vm_readv, process_vm_writev

#include "DebuggerCore.h"
#include "edb.h"
#include "MemoryRegions.h"
#include "PlatformEvent.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "State.h"
#include "string_hash.h"

#include <QDebug>
#include <QDir>

#include <cerrno>
#include <cstring>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <asm/ldt.h>
#include <pwd.h>
#include <link.h>
#include <cpuid.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>

#if defined(__NR_process_vm_readv) || defined(__NR_process_vm_writev)
#include <sys/uio.h>
#endif

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

#ifndef PTRACE_EVENT_CLONE
#define PTRACE_EVENT_CLONE 3
#endif

#ifndef PTRACE_O_TRACECLONE
#define PTRACE_O_TRACECLONE (1 << PTRACE_EVENT_CLONE)
#endif

namespace DebuggerCore {

namespace {

//------------------------------------------------------------------------------
// Name: is_numeric
// Desc: returns true if the string only contains decimal digits
//------------------------------------------------------------------------------
bool is_numeric(const QString &s) {
	Q_FOREACH(QChar ch, s) {
		if(!ch.isDigit()) {
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
// Name: resume_code
// Desc:
//------------------------------------------------------------------------------
int resume_code(int status) {

	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP) {
		return 0;
	}

	if(WIFSIGNALED(status)) {
		return WTERMSIG(status);
	}

	if(WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name: is_clone_event
// Desc:
//------------------------------------------------------------------------------
bool is_clone_event(int status) {
	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
		return (((status >> 16) & 0xffff) == PTRACE_EVENT_CLONE);
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: process_map_line
// Desc: parses the data from a line of a memory map file
//------------------------------------------------------------------------------
IRegion::pointer process_map_line(const QString &line) {

	edb::address_t start;
	edb::address_t end;
	edb::address_t base;
	IRegion::permissions_t permissions;
	QString name;

	const QStringList items = line.split(" ", QString::SkipEmptyParts);
	if(items.size() >= 3) {
		bool ok;
		const QStringList bounds = items[0].split("-");
		if(bounds.size() == 2) {
			start = bounds[0].toULongLong(&ok, 16);
			if(ok) {
				end = bounds[1].toULongLong(&ok, 16);
				if(ok) {
					base = items[2].toULongLong(&ok, 16);
					if(ok) {
						const QString perms = items[1];
						permissions = 0;
						if(perms[0] == 'r') permissions |= PROT_READ;
						if(perms[1] == 'w') permissions |= PROT_WRITE;
						if(perms[2] == 'x') permissions |= PROT_EXEC;

						if(items.size() >= 6) {
							name = items[5];
						}

						return IRegion::pointer(new PlatformRegion(start, end, base, name, permissions));
					}
				}
			}
		}
	}
	return IRegion::pointer();
}

struct user_stat {
/* 01 */ int pid;
/* 02 */ char comm[256];
/* 03 */ char state;
/* 04 */ int ppid;
/* 05 */ int pgrp;
/* 06 */ int session;
/* 07 */ int tty_nr;
/* 08 */ int tpgid;
/* 09 */ unsigned flags;
/* 10 */ unsigned long minflt;
/* 11 */ unsigned long cminflt;
/* 12 */ unsigned long majflt;
/* 13 */ unsigned long cmajflt;
/* 14 */ unsigned long utime;
/* 15 */ unsigned long stime;
/* 16 */ long cutime;
/* 17 */ long cstime;
/* 18 */ long priority;
/* 19 */ long nice;
/* 20 */ long num_threads;
/* 21 */ long itrealvalue;
/* 22 */ unsigned long long starttime;
/* 23 */ unsigned long vsize;
/* 24 */ long rss;
/* 25 */ unsigned long rsslim;
/* 26 */ unsigned long startcode;
/* 27 */ unsigned long endcode;
/* 28 */ unsigned long startstack;
/* 29 */ unsigned long kstkesp;
/* 30 */ unsigned long kstkeip;
/* 31 */ unsigned long signal;
/* 32 */ unsigned long blocked;
/* 33 */ unsigned long sigignore;
/* 34 */ unsigned long sigcatch;
/* 35 */ unsigned long wchan;
/* 36 */ unsigned long nswap;
/* 37 */ unsigned long cnswap;
/* 38 */ int exit_signal;
/* 39 */ int processor;
/* 40 */ unsigned rt_priority;
/* 41 */ unsigned policy;
/* 42 */ unsigned long long delayacct_blkio_ticks;
/* 43 */ unsigned long guest_time;
/* 44 */ long cguest_time;
};

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat) {

	Q_ASSERT(user_stat);

	int r = -1;
	QFile file(QString("/proc/%1/stat").arg(pid));
	if(file.open(QIODevice::ReadOnly)) {
		QTextStream in(&file);
		const QString line = in.readLine();
		if(!line.isNull()) {
			r = sscanf(qPrintable(line), "%d %255s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld",
					&user_stat->pid,
					user_stat->comm,
					&user_stat->state,
					&user_stat->ppid,
					&user_stat->pgrp,
					&user_stat->session,
					&user_stat->tty_nr,
					&user_stat->tpgid,
					&user_stat->flags,
					&user_stat->minflt,
					&user_stat->cminflt,
					&user_stat->majflt,
					&user_stat->cmajflt,
					&user_stat->utime,
					&user_stat->stime,
					&user_stat->cutime,
					&user_stat->cstime,
					&user_stat->priority,
					&user_stat->nice,
					&user_stat->num_threads,
					&user_stat->itrealvalue,
					&user_stat->starttime,
					&user_stat->vsize,
					&user_stat->rss,
					&user_stat->rsslim,
					&user_stat->startcode,
					&user_stat->endcode,
					&user_stat->startstack,
					&user_stat->kstkesp,
					&user_stat->kstkeip,
					&user_stat->signal,
					&user_stat->blocked,
					&user_stat->sigignore,
					&user_stat->sigcatch,
					&user_stat->wchan,
					&user_stat->nswap,
					&user_stat->cnswap,
					&user_stat->exit_signal,
					&user_stat->processor,
					&user_stat->rt_priority,
					&user_stat->policy,
					&user_stat->delayacct_blkio_ticks,
					&user_stat->guest_time,
					&user_stat->cguest_time);
		}
		file.close();
	}

	return r;
}


}

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() : binary_info_(0) {
#if defined(_SC_PAGESIZE)
	page_size_ = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	page_size_ = sysconf(_SC_PAGE_SIZE);
#else
	page_size_ = PAGE_SIZE;
#endif
}

//------------------------------------------------------------------------------
// Name: has_extension
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::has_extension(quint64 ext) const {



#if defined(EDB_X86)

	quint32 eax;
	quint32 ebx;
	quint32 ecx;
	quint32 edx;
	__cpuid(1, eax, ebx, ecx, edx);

	switch(ext) {
	case edb::string_hash<'M', 'M', 'X'>::value:
		return (edx & bit_MMX);
	case edb::string_hash<'X', 'M', 'M'>::value:
		//return (edx & bit_SSE);
	default:
		return false;
	}

	return false;
#elif defined(EDB_X86_64)
	switch(ext) {
	case edb::string_hash<'M', 'M', 'X'>::value:
	case edb::string_hash<'X', 'M', 'M'>::value:
		return true;
	default:
		return false;
	}
#endif
}

//------------------------------------------------------------------------------
// Name: page_size
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
}

//------------------------------------------------------------------------------
// Name: ~DebuggerCore
// Desc: destructor
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
}

//------------------------------------------------------------------------------
// Name: ptrace_getsiginfo
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_getsiginfo(edb::tid_t tid, siginfo_t *siginfo) {
	Q_ASSERT(siginfo != 0);
	return ptrace(PTRACE_GETSIGINFO, tid, 0, siginfo);
}

//------------------------------------------------------------------------------
// Name: ptrace_traceme
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_traceme() {
	return ptrace(PTRACE_TRACEME, 0, 0, 0);
}

//------------------------------------------------------------------------------
// Name: ptrace_continue
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_continue(edb::tid_t tid, long status) {
	Q_ASSERT(waited_threads_.contains(tid));
	Q_ASSERT(tid != 0);
	waited_threads_.remove(tid);
	return ptrace(PTRACE_CONT, tid, 0, status);
}

//------------------------------------------------------------------------------
// Name: ptrace_step
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_step(edb::tid_t tid, long status) {
	Q_ASSERT(waited_threads_.contains(tid));
	Q_ASSERT(tid != 0);
	waited_threads_.remove(tid);
	return ptrace(PTRACE_SINGLESTEP, tid, 0, status);
}

//------------------------------------------------------------------------------
// Name: ptrace_set_options
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_set_options(edb::tid_t tid, long options) {
	Q_ASSERT(waited_threads_.contains(tid));
	Q_ASSERT(tid != 0);
	return ptrace(PTRACE_SETOPTIONS, tid, 0, options);
}

//------------------------------------------------------------------------------
// Name: ptrace_get_event_message
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptrace_get_event_message(edb::tid_t tid, unsigned long *message) {
	Q_ASSERT(waited_threads_.contains(tid));
	Q_ASSERT(tid != 0);
	Q_ASSERT(message != 0);
	return ptrace(PTRACE_GETEVENTMSG, tid, 0, message);
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc:
//------------------------------------------------------------------------------
IDebugEvent::const_pointer DebuggerCore::handle_event(edb::tid_t tid, int status) {

	// note that we have waited on this thread
	waited_threads_.insert(tid);

	// was it a thread exit event?
	if(WIFEXITED(status)) {
		threads_.remove(tid);
		waited_threads_.remove(tid);

		// if this was the last thread, return true
		// so we report it to the user.
		// if this wasn't, then we should silently
		// procceed.
		if(!threads_.empty()) {
			return IDebugEvent::const_pointer();
		}
	}

	// was it a thread create event?
	if(is_clone_event(status)) {

		unsigned long new_tid;
		if(ptrace_get_event_message(tid, &new_tid) != -1) {

			const thread_info info = { 0, thread_info::THREAD_STOPPED };
			threads_.insert(new_tid, info);

			int thread_status = 0;
			if(!waited_threads_.contains(new_tid)) {
				if(native::waitpid(new_tid, &thread_status, __WALL) > 0) {
					waited_threads_.insert(new_tid);
				}
			}

			if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
				qDebug("[warning] new thread [%d] received an event besides SIGSTOP", static_cast<int>(new_tid));
			}

			// TODO: what the heck do we do if this isn't a SIGSTOP?
			ptrace_continue(new_tid, resume_code(thread_status));
		}

		ptrace_continue(tid, 0);
		return IDebugEvent::const_pointer();
	}

	// normal event


	PlatformEvent *const e = new PlatformEvent;

	e->pid_    = pid();
	e->tid_    = tid;
	e->status_ = status;
	if(ptrace_getsiginfo(tid, &e->siginfo_) == -1) {
		// TODO: handle no info?
	}

	active_thread_       = tid;
	event_thread_        = tid;
	threads_[tid].status = status;

	stop_threads();
	return IDebugEvent::const_pointer(e);
}

//------------------------------------------------------------------------------
// Name: stop_threads
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::stop_threads() {
	for(threadmap_t::iterator it = threads_.begin(); it != threads_.end(); ++it) {
		if(!waited_threads_.contains(it.key())) {
			const edb::tid_t tid = it.key();

			syscall(SYS_tgkill, pid(), tid, SIGSTOP);

			int thread_status;
			if(native::waitpid(tid, &thread_status, __WALL) > 0) {
				waited_threads_.insert(tid);
				it->status = thread_status;

				if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
					qDebug("[warning] paused thread [%d] received an event besides SIGSTOP", tid);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: wait_debug_event
// Desc: waits for a debug event, msecs is a timeout
//      it will return false if an error or timeout occurs
//------------------------------------------------------------------------------
IDebugEvent::const_pointer DebuggerCore::wait_debug_event(int msecs) {

	if(attached()) {
		if(!native::wait_for_sigchld(msecs)) {
			Q_FOREACH(edb::tid_t thread, thread_ids()) {
				int status;
				const edb::tid_t tid = native::waitpid(thread, &status, __WALL | WNOHANG);
				if(tid > 0) {
					return handle_event(tid, status);
				}
			}
		}
	}
	return IDebugEvent::const_pointer();
}

//------------------------------------------------------------------------------
// Name: read_data
// Desc:
// Note: this will fail on newer versions of linux if called from a
//       different thread than the one which attached to process
//------------------------------------------------------------------------------
long DebuggerCore::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);

	errno = 0;
	const long v = ptrace(PTRACE_PEEKTEXT, pid(), address, 0);
	SET_OK(*ok, v);
	return v;
}

//------------------------------------------------------------------------------
// Name: read_pages
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::read_pages(edb::address_t address, void *buf, std::size_t count) {

	const std::size_t len = count * page_size();

	QFile memory_file(QString("/proc/%1/mem").arg(pid_));
	if(memory_file.open(QIODevice::ReadOnly)) {

		memory_file.seek(address);
		const qint64 n = memory_file.read(reinterpret_cast<char *>(buf), len);

		// TODO: handle if breakponts have a size more than 1!
		Q_FOREACH(const IBreakpoint::pointer &bp, breakpoints_) {
			if(bp->address() >= address && bp->address() < (address + n)) {
				// show the original bytes in the buffer..
				reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_bytes()[0];
			}
		}

		memory_file.close();
	}

	return true;
}


//------------------------------------------------------------------------------
// Name: write_data
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::write_data(edb::address_t address, long value) {
	return ptrace(PTRACE_POKETEXT, pid(), address, value) != -1;
}

//------------------------------------------------------------------------------
// Name: attach_thread
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach_thread(edb::tid_t tid) {
	if(ptrace(PTRACE_ATTACH, tid, 0, 0) == 0) {
		// I *think* that the PTRACE_O_TRACECLONE is only valid on
		// on stopped threads
		int status;
		if(native::waitpid(tid, &status, __WALL) > 0) {

			const thread_info info = { status, thread_info::THREAD_STOPPED };
			threads_[tid] = info;

			waited_threads_.insert(tid);
			if(ptrace_set_options(tid, PTRACE_O_TRACECLONE) == -1) {
				qDebug("[DebuggerCore] failed to set PTRACE_O_TRACECLONE: [%d] %s", tid, strerror(errno));
			}
		}
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach(edb::pid_t pid) {
	detach();

	bool attached;
	do {
		attached = false;
		QDir proc_directory(QString("/proc/%1/task/").arg(pid));
		Q_FOREACH(QString s, proc_directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
			// this can get tricky if the threads decide to spawn new threads
			// when we are attaching. I wish that linux had an atomic way to do this
			// all in one shot
			const edb::tid_t tid = s.toUInt();
			if(!threads_.contains(tid) && attach_thread(tid)) {
				attached = true;
			}
		}
	} while(attached);


	if(!threads_.empty()) {
		pid_            = pid;
		active_thread_  = pid;
		event_thread_   = pid;
		binary_info_    = edb::v1::get_binary_info(edb::v1::primary_code_region());
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::detach() {
	if(attached()) {

		stop_threads();

		clear_breakpoints();

		Q_FOREACH(edb::tid_t thread, thread_ids()) {
			if(ptrace(PTRACE_DETACH, thread, 0, 0) == 0) {
				native::waitpid(thread, 0, __WALL);
			}
		}

		reset();
	}
}

//------------------------------------------------------------------------------
// Name: kill
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		clear_breakpoints();

		ptrace(PTRACE_KILL, pid(), 0, 0);

		// TODO: do i need to actually do this wait?
		native::waitpid(pid(), 0, __WALL);

		reset();
	}
}

//------------------------------------------------------------------------------
// Name: pause
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void DebuggerCore::pause() {
	if(attached()) {
		// belive it or not, I belive that this is sufficient for all threads
		// this is because in the debug event handler above, a SIGSTOP is sent
		// to all threads when any event arrives, so no need to explicitly do
		// it here. We just need any thread to stop. So we'll just target the
		// pid() which will send it to any one of the threads in the process.
		::kill(pid(), SIGSTOP);
	}
}

//------------------------------------------------------------------------------
// Name: resume
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::resume(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused

	if(attached()) {
		if(status != edb::DEBUG_STOP) {
			const edb::tid_t tid = active_thread();
			const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(threads_[tid].status) : 0;
			ptrace_continue(tid, code);

			// resume the other threads passing the signal they originally reported had
			for(threadmap_t::const_iterator it = threads_.begin(); it != threads_.end(); ++it) {
				if(waited_threads_.contains(it.key())) {
					ptrace_continue(it.key(), resume_code(it->status));
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: step
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::step(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused

	if(attached()) {
		if(status != edb::DEBUG_STOP) {
			const edb::tid_t tid = active_thread();
			const int code = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(threads_[tid].status) : 0;
			ptrace_step(tid, code);
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State *state) {
	// TODO: assert that we are paused

	if(PlatformState *const state_impl = static_cast<PlatformState *>(state->impl_)) {
		if(attached()) {
			if(ptrace(PTRACE_GETREGS, active_thread(), 0, &state_impl->regs_) != -1) {
			#if defined(EDB_X86)
				struct user_desc desc;
				std::memset(&desc, 0, sizeof(desc));

				if(ptrace(PTRACE_GET_THREAD_AREA, active_thread(), (state_impl->regs_.xgs / LDT_ENTRY_SIZE), &desc) != -1) {
					state_impl->gs_base = desc.base_addr;
				} else {
					state_impl->gs_base = 0;
				}

				if(ptrace(PTRACE_GET_THREAD_AREA, active_thread(), (state_impl->regs_.xfs / LDT_ENTRY_SIZE), &desc) != -1) {
					state_impl->fs_base = desc.base_addr;
				} else {
					state_impl->fs_base = 0;
				}
			#elif defined(EDB_X86_64)
			#endif
			}

			// floating point registers
			if(ptrace(PTRACE_GETFPREGS, active_thread(), 0, &state_impl->fpregs_) != -1) {
			}

			// debug registers
			state_impl->dr_[0] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[0]), 0);
			state_impl->dr_[1] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[1]), 0);
			state_impl->dr_[2] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[2]), 0);
			state_impl->dr_[3] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[3]), 0);
			state_impl->dr_[4] = 0;
			state_impl->dr_[5] = 0;
			state_impl->dr_[6] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[6]), 0);
			state_impl->dr_[7] = ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[7]), 0);

		} else {
			state_impl->clear();
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_state(const State &state) {

	// TODO: assert that we are paused


	if(attached()) {

		if(PlatformState *const state_impl = static_cast<PlatformState *>(state.impl_)) {
			ptrace(PTRACE_SETREGS, active_thread(), 0, &state_impl->regs_);

			// debug registers
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[0]), state_impl->dr_[0]);
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[1]), state_impl->dr_[1]);
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[2]), state_impl->dr_[2]);
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[3]), state_impl->dr_[3]);
			//ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[4]), state_impl->dr_[4]);
			//ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[5]), state_impl->dr_[5]);
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[6]), state_impl->dr_[6]);
			ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[7]), state_impl->dr_[7]);
		}
	}
}

//------------------------------------------------------------------------------
// Name: open
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {
	detach();
	pid_t pid;

	switch(pid = fork()) {
	case 0:
		// we are in the child now...

		// set ourselves (the child proc) up to be traced
		ptrace_traceme();

		// redirect it's I/O
		if(!tty.isEmpty()) {
			FILE *const std_out = freopen(qPrintable(tty), "r+b", stdout);
			FILE *const std_in  = freopen(qPrintable(tty), "r+b", stdin);
			FILE *const std_err = freopen(qPrintable(tty), "r+b", stderr);

			Q_UNUSED(std_out);
			Q_UNUSED(std_in);
			Q_UNUSED(std_err);
		}

		// do the actual exec
		execute_process(path, cwd, args);

		// we should never get here!
		abort();
		break;
	case -1:
		// error! for some reason we couldn't fork
		reset();
		return false;
	default:
		// parent
		do {
			reset();

			int status;
			if(native::waitpid(pid, &status, __WALL) == -1) {
				return false;
			}

			// the very first event should be a STOP of type SIGTRAP
			if(!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
				detach();
				return false;
			}

			waited_threads_.insert(pid);

			// enable following clones (threads)
			if(ptrace_set_options(pid, PTRACE_O_TRACECLONE) == -1) {
				qDebug("[DebuggerCore] failed to set PTRACE_SETOPTIONS: %s", strerror(errno));
				detach();
				return false;
			}

			// setup the first event data for the primary thread
			waited_threads_.insert(pid);

			const thread_info info = { status, thread_info::THREAD_STOPPED };
			threads_[pid]   = info;

			pid_            = pid;
			active_thread_  = pid;
			event_thread_   = pid;
			binary_info_    = edb::v1::get_binary_info(edb::v1::primary_code_region());

			return true;
		} while(0);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: set_active_thread
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_active_thread(edb::tid_t tid) {
	if(threads_.contains(tid)) {
#if 0
		active_thread_ = tid;
#else
		qDebug("[DebuggerCore::set_active_thread] not implemented yet");
#endif
	} else {
		qDebug("[DebuggerCore] warning, attempted to set invalid thread as active: %d", tid);
	}
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::reset() {
	threads_.clear();
	waited_threads_.clear();
	active_thread_ = 0;
	pid_           = 0;
	event_thread_  = 0;
	delete binary_info_;
	binary_info_   = 0;
}

//------------------------------------------------------------------------------
// Name: create_state
// Desc:
//------------------------------------------------------------------------------
IState *DebuggerCore::create_state() const {
	return new PlatformState;
}

//------------------------------------------------------------------------------
// Name: enumerate_processes
// Desc:
//------------------------------------------------------------------------------
QMap<edb::pid_t, Process> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, Process> ret;

	QDir proc_directory("/proc/");
	QFileInfoList entries = proc_directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	Q_FOREACH(const QFileInfo &info, entries) {
		const QString filename = info.fileName();
		if(is_numeric(filename)) {

			const edb::pid_t pid = filename.toULong();
			Process process_info;

			struct user_stat user_stat;
			const int n = get_user_stat(pid, &user_stat);
			if(n >= 2) {
				process_info.name = user_stat.comm;

				// remove silly '(' and ')'
				process_info.name = process_info.name.mid(1);
				process_info.name.chop(1);
			}

			process_info.pid = pid;
			process_info.uid = info.ownerId();

			if(const struct passwd *const pwd = ::getpwuid(process_info.uid)) {
				process_info.user = pwd->pw_name;
			}

			ret.insert(process_info.pid, process_info);
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_exe(edb::pid_t pid) const {
	return edb::v1::symlink_target(QString("/proc/%1/exe").arg(pid));
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_cwd(edb::pid_t pid) const {
	return edb::v1::symlink_target(QString("/proc/%1/cwd").arg(pid));
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::pid_t DebuggerCore::parent_pid(edb::pid_t pid) const {

	struct user_stat user_stat;
	int n = get_user_stat(pid, &user_stat);
	if(n >= 4) {
		return user_stat.ppid;
	}

	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<IRegion::pointer> DebuggerCore::memory_regions() const {
	QList<IRegion::pointer> regions;

	if(pid_ != 0) {
		const QString map_file(QString("/proc/%1/maps").arg(pid_));

		QFile file(map_file);
        if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {

			QTextStream in(&file);
			QString line = in.readLine();

			while(!line.isNull()) {
				if(IRegion::pointer region = process_map_line(line)) {
					regions.push_back(region);
				}
				line = in.readLine();
			}
		}
	}

	return regions;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<QByteArray> DebuggerCore::process_args(edb::pid_t pid) const {

	QList<QByteArray> ret;

	if(pid != 0) {
		const QString command_line_file(QString("/proc/%1/cmdline").arg(pid));
		QFile file(command_line_file);

		if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			QTextStream in(&file);

			QByteArray s;
			QChar ch;

			while(in.status() == QTextStream::Ok) {
				in >> ch;
				if(ch == '\0') {
					if(!s.isEmpty()) {
						ret << s;
					}
					s.clear();
				} else {
					s += ch;
				}
			}

			if(!s.isEmpty()) {
				ret << s;
			}
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::process_code_address() const {
	struct user_stat user_stat;
	int n = get_user_stat(pid(), &user_stat);
	if(n >= 26) {
		return user_stat.startcode;
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::process_data_address() const {
	struct user_stat user_stat;
	int n = get_user_stat(pid(), &user_stat);
	if(n >= 27) {
		return user_stat.endcode + 1; // endcode == startdata ?
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<Module> DebuggerCore::loaded_modules() const {
	QList<Module> ret;

	if(binary_info_) {
		struct r_debug dynamic_info;
		if(const edb::address_t debug_pointer = binary_info_->debug_pointer()) {
			if(edb::v1::debugger_core->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info))) {
				if(dynamic_info.r_map) {

					edb::address_t link_address = reinterpret_cast<edb::address_t>(dynamic_info.r_map);

					while(link_address) {

						struct link_map map;
						if(edb::v1::debugger_core->read_bytes(link_address, &map, sizeof(map))) {
							char path[PATH_MAX];
							if(!edb::v1::debugger_core->read_bytes(reinterpret_cast<edb::address_t>(map.l_name), &path, sizeof(path))) {
								path[0] = '\0';
							}

							if(map.l_addr) {
								Module module;
								module.name         = path;
								module.base_address = map.l_addr;
								ret.push_back(module);
							}

							link_address = reinterpret_cast<edb::address_t>(map.l_next);
						} else {
							break;
						}
					}
				}
			}
		}
	}

	// fallback
	if(ret.isEmpty()) {
		const QList<IRegion::pointer> r = edb::v1::memory_regions().regions();
		QSet<QString> found_modules;

		Q_FOREACH(const IRegion::pointer &region, r) {

			// we assume that modules will be listed by absolute path
			if(region->name().startsWith("/")) {
				if(!found_modules.contains(region->name())) {
					Module module;
					module.name         = region->name();
					module.base_address = region->start();
					found_modules.insert(region->name());
					ret.push_back(module);
				}
			}
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QDateTime DebuggerCore::process_start(edb::pid_t pid) const {
	QFileInfo info(QString("/proc/%1/stat").arg(pid));
	return info.created();
}

#if 0
#ifdef __NR_process_vm_readv
	bool DebuggerCore::read_bytes(edb::address_t address, void *buf, std::size_t len) {

		if(pid_ != 0) {
			struct iovec local[1];
			struct iovec remote[1];

			local[0].iov_base  = buf;
			local[0].iov_len   = len;
			remote[0].iov_base = reinterpret_cast<void *>(address);
			remote[0].iov_len  = len;

			const ssize_t n = syscall(__NR_process_vm_readv, (long)pid_, local, 1, remote, 1, 0);

			if(n > 0) {
				// TODO: handle if breakponts have a size more than 1!
				Q_FOREACH(const IBreakpoint::pointer &bp, breakpoints_) {
					if(bp->address() >= address && bp->address() < (address + n)) {
						// show the original bytes in the buffer..
						reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_bytes()[0];
					}
				}
				return true;
			}
		}

		return false;
	}
#endif

#ifdef __NR_process_vm_writev
	bool DebuggerCore::write_bytes(edb::address_t address, const void *buf, std::size_t len) {
		if(pid_ != 0) {
			struct iovec local[1];
			struct iovec remote[1];

			local[0].iov_base  = const_cast<void *>(buf);
			local[0].iov_len   = len;
			remote[0].iov_base = reinterpret_cast<void *>(address);
			remote[0].iov_len  = len;

			const ssize_t n = syscall(__NR_process_vm_writev, (long)pid_, local, 1, remote, 1, 0);

			if(n > 0) {
				return true;
			}
		}

		return false;
	}
#endif
#endif

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
quint64 DebuggerCore::cpu_type() const {
#ifdef EDB_X86
	return edb::string_hash<'x', '8', '6'>::value;
#elif defined(EDB_X86_64)
	return edb::string_hash<'x', '8', '6', '-', '6', '4'>::value;
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QWidget *DebuggerCore::create_register_view() const {
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::format_pointer(edb::address_t address) const {
	char buf[32];
#ifdef EDB_X86
	qsnprintf(buf, sizeof(buf), "%08x", address);
#elif defined(EDB_X86_64)
	qsnprintf(buf, sizeof(buf), "%016llx", address);
#endif
	return buf;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::stack_pointer() const {
#ifdef EDB_X86
	return "esp";
#elif defined(EDB_X86_64)
	return "rsp";
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::frame_pointer() const {
#ifdef EDB_X86
	return "ebp";
#elif defined(EDB_X86_64)
	return "rbp";
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::instruction_pointer() const {
#ifdef EDB_X86
	return "eip";
#elif defined(EDB_X86_64)
	return "rip";
#endif
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(DebuggerCore, DebuggerCore)
#endif

}
