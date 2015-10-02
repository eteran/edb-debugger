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


// TODO(eteran): research usage of process_vm_readv, process_vm_writev

#include "DebuggerCore.h"
#include "edb.h"
#include "MemoryRegions.h"
#include "PlatformEvent.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "PlatformProcess.h"
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
#include <elf.h>
#include <linux/uio.h>

namespace BinaryInfo {
// Bitness-templated version of struct r_debug defined in link.h
template<class Addr>
struct r_debug
{
	int r_version;
	Addr r_map; // struct link_map*
	Addr r_brk;
	enum {
		RT_CONSISTENT,
		RT_ADD,
		RT_DELETE
	} r_state;
	Addr r_ldbase;
};

// Bitness-templated version of struct link_map defined in link.h
template<class Addr>
struct link_map
{
	Addr l_addr;
	Addr l_name; // char*
	Addr l_ld; // ElfW(Dyn)*
	Addr l_next, l_prev; // struct link_map*
};
}

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
	for(QChar ch: s) {
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

// Linux 2.6.18
/* 42 */ unsigned long long delayacct_blkio_ticks;

// Linux 2.6.24
/* 43 */ unsigned long guest_time;
/* 44 */ long cguest_time;

// Linux 3.3
/* 45 */ unsigned long start_data;
/* 46 */ unsigned long end_data;
/* 47 */ unsigned long start_brk;

// Linux 3.5
/* 48 */ unsigned long arg_start;
/* 49 */ unsigned long arg_end;
/* 50 */ unsigned long env_start;
/* 51 */ unsigned long env_end;
/* 52 */ int exit_code;
};

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(const QString &path, struct user_stat *user_stat) {
	Q_ASSERT(user_stat);

	int r = -1;
	QFile file(path);
	if(file.open(QIODevice::ReadOnly)) {
		QTextStream in(&file);
		const QString line = in.readLine();
		if(!line.isNull()) {
			char ch;
			r = sscanf(qPrintable(line), "%d %c%255[0-9a-zA-Z_ #~/-]%c %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld",
					&user_stat->pid,
					&ch, // consume the (
					user_stat->comm,
					&ch, // consume the )
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

//------------------------------------------------------------------------------
// Name: get_user_stat
// Desc: gets the contents of /proc/<pid>/stat and returns the number of elements
//       successfully parsed
//------------------------------------------------------------------------------
int get_user_stat(edb::pid_t pid, struct user_stat *user_stat) {

	return get_user_stat(QString("/proc/%1/stat").arg(pid), user_stat);
}


}

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() : binary_info_(0), process_(0), pointer_size_(sizeof(void*)) {
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

	const auto mmxHash=edb::string_hash("MMX");
	const auto xmmHash=edb::string_hash("XMM");
	const auto ymmHash=edb::string_hash("YMM");
	if(EDB_IS_64_BIT && (ext==xmmHash || ext==mmxHash))
		return true;

	quint32 eax;
	quint32 ebx;
	quint32 ecx;
	quint32 edx;
	__cpuid(1, eax, ebx, ecx, edx);

	switch(ext) {
	case mmxHash:
		return (edx & bit_MMX);
	case xmmHash:
		return (edx & bit_SSE);
	case ymmHash:
	{
		// Check OSXSAVE and AVX feature flags
		if((ecx&0x18000000)!=0x18000000)
			return false;
		// Get XCR0, must be exactly after OSXSAVE feature check, otherwise #UD
		asm volatile("xgetbv" : "=a"(eax),"=d"(edx) : "c"(0));
		// Check that the OS has enabled XMM and YMM state support
		if((eax&0x6)!=0x6)
			return false;
		return true;
	}
	default:
		return false;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: page_size
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
}

std::size_t DebuggerCore::pointer_size() const {
	return pointer_size_;
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
			return nullptr;
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
		return nullptr;
	}

	// normal event


	auto e = std::make_shared<PlatformEvent>();

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
	return e;
}

//------------------------------------------------------------------------------
// Name: stop_threads
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::stop_threads() {
	for(auto it = threads_.begin(); it != threads_.end(); ++it) {
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
			for(edb::tid_t thread: thread_ids()) {
				int status;
				const edb::tid_t tid = native::waitpid(thread, &status, __WALL | WNOHANG);
				if(tid > 0) {
					return handle_event(tid, status);
				}
			}
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: read_data
// Desc:
// Note: this will fail on newer versions of linux if called from a
//       different thread than the one which attached to process
//------------------------------------------------------------------------------
long DebuggerCore::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);

	if(EDB_IS_32_BIT && address>0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses, try reading /proc/$PID/mem
		// FIXME: this is slow. Try keeping the file open, not reopening it on each read.
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::ReadOnly)) {

			memory_file.seek(address);
			long value;
			if(memory_file.read(reinterpret_cast<char*>(&value), sizeof(long))==sizeof(long)) {
				*ok=true;
				return value;
			}
		}
		return 0;
	}

	errno = 0;
	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress=reinterpret_cast<const void* const>(address.toUint());
	const long v = ptrace(PTRACE_PEEKTEXT, pid(), nativeAddress, 0);
	SET_OK(*ok, v);
	return v;
}

std::size_t DebuggerCore::read_bytes(edb::address_t address, void* buf, std::size_t len) {
	quint64 bytesRead=0;

	QFile memory_file(QString("/proc/%1/mem").arg(pid_));
	if(memory_file.open(QIODevice::ReadOnly)) {

		memory_file.seek(address);
		bytesRead = memory_file.read(reinterpret_cast<char *>(buf), len);
		if(bytesRead==0 || bytesRead==quint64(-1))
			return 0;

		for(const IBreakpoint::pointer &bp: breakpoints_) {
			if(bp->address() >= address && bp->address() < (address + bytesRead)) {
				// show the original bytes in the buffer..
				reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_byte();
			}
		}

		memory_file.close();
	}

	return bytesRead;
}

//------------------------------------------------------------------------------
// Name: read_pages
// Desc:
//------------------------------------------------------------------------------
std::size_t DebuggerCore::read_pages(edb::address_t address, void *buf, std::size_t count) {

	return read_bytes(address,buf,count*page_size())/page_size();
}


//------------------------------------------------------------------------------
// Name: write_data
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::write_data(edb::address_t address, long value) {
	if(EDB_IS_32_BIT && address>0xffffffffULL) {
		// 32 bit ptrace can't handle such long addresses
		QFile memory_file(QString("/proc/%1/mem").arg(pid_));
		if(memory_file.open(QIODevice::WriteOnly)) {

			memory_file.seek(address);
			if(memory_file.write(reinterpret_cast<char*>(&value), sizeof(long))==sizeof(long))
				return true;
		}
		return false;
	}
	// NOTE: on some Linux systems ptrace prototype has ellipsis instead of third and fourth arguments
	// Thus we can't just pass address as is on IA32 systems: it'd put 64 bit integer on stack and cause UB
	auto nativeAddress=reinterpret_cast<const void* const>(address.toUint());
	return ptrace(PTRACE_POKETEXT, pid(), nativeAddress, value) != -1;
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
			
#ifdef PTRACE_O_EXITKILL
			if(ptrace_set_options(tid, PTRACE_O_EXITKILL) == -1) {
				qDebug("[DebuggerCore] failed to set PTRACE_O_EXITKILL: [%d] %s", tid, strerror(errno));
			}
#endif
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
		for(const QString &s: proc_directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
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
		process_        = new PlatformProcess(this, pid);
		detectDebuggeeBitness();
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

		for(edb::tid_t thread: thread_ids()) {
			ptrace(PTRACE_DETACH, thread, 0, 0);
		}
		
		delete process_;
		process_ = nullptr;

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

		delete process_;
		process_ = 0;

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
			for(auto it = threads_.begin(); it != threads_.end(); ++it) {
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

void DebuggerCore::fillFSGSBases(PlatformState* state) {
	if(state->is32Bit()) {
		struct user_desc desc;
		std::memset(&desc, 0, sizeof(desc));

		if(ptrace(PTRACE_GET_THREAD_AREA, active_thread(), (state->x86.segRegs[PlatformState::X86::FS] / LDT_ENTRY_SIZE), &desc) != -1) {
			state->x86.segRegBases[PlatformState::X86::FS] = desc.base_addr;
			state->x86.segRegBasesFilled[PlatformState::X86::FS]=true;
		}
		if(ptrace(PTRACE_GET_THREAD_AREA, active_thread(), (state->x86.segRegs[PlatformState::X86::GS] / LDT_ENTRY_SIZE), &desc) != -1) {
			state->x86.segRegBases[PlatformState::X86::GS] = desc.base_addr;
			state->x86.segRegBasesFilled[PlatformState::X86::GS]=true;
		}
	}
}

bool DebuggerCore::fillStateFromPrStatus(PlatformState* state) {

	static bool prStatusSupported=true;
	if(!prStatusSupported)
		return false;


	PrStatus_X86_64 prstat64;

	iovec prstat_iov = {&prstat64, sizeof(prstat64)};

	if(ptrace(PTRACE_GETREGSET, active_thread(), NT_PRSTATUS, &prstat_iov) != -1) {
		if(prstat_iov.iov_len==sizeof(prstat64)) {
			state->fillFrom(prstat64);
		} else if(prstat_iov.iov_len==sizeof(PrStatus_X86)) {
			// In this case the actual structure returned is PrStatus_X86,
			// so copy it to the correct container (reinterpret_cast would
			// cause UB in any case). Good compiler should be able to optimize this out.
			PrStatus_X86 prstat32;
			std::memcpy(&prstat32,&prstat64,sizeof(prstat32));
			state->fillFrom(prstat32);
		} else {
			prStatusSupported=false;
			qWarning() << "PTRACE_GETREGSET(NT_PRSTATUS) returned unexpected length " << prstat_iov.iov_len;
			return false;
		}
	}
	else {
		prStatusSupported=false;
		perror("PTRACE_GETREGSET(NT_PRSTATUS) failed");
		return false;
	}
	fillFSGSBases(state);
	return true;
}

bool DebuggerCore::fillStateFromSimpleRegs(PlatformState* state) {

	user_regs_struct regs;
	if(ptrace(PTRACE_GETREGS, active_thread(), 0, &regs) != -1) {

		state->fillFrom(regs);
		fillFSGSBases(state);
		return true;
	}
	else {
		perror("PTRACE_GETREGS failed");
		return false;
	}
}

unsigned long DebuggerCore::get_debug_register(std::size_t n) {
	return ptrace(PTRACE_PEEKUSER, active_thread(), offsetof(struct user, u_debugreg[n]), 0);
}

long DebuggerCore::set_debug_register(std::size_t n, long value) {
	return ptrace(PTRACE_POKEUSER, active_thread(), offsetof(struct user, u_debugreg[n]), value);
}

void DebuggerCore::detectDebuggeeBitness() {

	bool detected=false;
	PrStatus_X86_64 prstat64;
	iovec prstat_iov = {&prstat64, sizeof(prstat64)};
	if(ptrace(PTRACE_GETREGSET, active_thread(), NT_PRSTATUS, &prstat_iov) != -1) {

		detected=true;
		if(prstat_iov.iov_len==sizeof prstat64)
			pointer_size_=sizeof(std::uint64_t);
		else if(prstat_iov.iov_len==sizeof(PrStatus_X86))
			pointer_size_=sizeof(std::uint32_t);
		else detected=false;
	}

	if(!detected) {
		qDebug() << "Warning: failed to detect bitness of debuggee, using native EDB bitness";
		pointer_size_=sizeof(void*);
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State *state) {
	// TODO: assert that we are paused

	if(auto state_impl = static_cast<PlatformState *>(state->impl_)) {
		// State must be cleared before filling to zero all presence flags, otherwise something
		// may remain not updated. Also, this way we'll mark all the unfilled values.
		state_impl->clear();
		if(attached()) {

			if(EDB_IS_64_BIT)
				fillStateFromSimpleRegs(state_impl); // 64-bit GETREGS call always returns 64-bit state, so use it
			else if(!fillStateFromPrStatus(state_impl)) // if EDB is 32 bit, use GETREGSET so that we get 64-bit state for 64-bit debuggee
				fillStateFromSimpleRegs(state_impl); // failing that, try to just get what we can

			long ptraceStatus=0;

			// First try to get full XSTATE
			X86XState xstate;
			iovec iov={&xstate,sizeof(xstate)};
			ptraceStatus=ptrace(PTRACE_GETREGSET, active_thread(), NT_X86_XSTATE, &iov);
			if(ptraceStatus!=-1) {
				state_impl->fillFrom(xstate,iov.iov_len);
			} else {

				// No XSTATE available, get just floating point and SSE registers
				static bool getFPXRegsSupported=(EDB_IS_32_BIT ? true : false);
				UserFPXRegsStructX86 fpxregs;
				// This should be automatically optimized out on amd64. If not, not a big deal.
				// Avoiding conditional compilation to facilitate syntax error checking
				if(getFPXRegsSupported)
					getFPXRegsSupported=(ptrace(PTRACE_GETFPXREGS, active_thread(), 0, &fpxregs)!=-1);

				if(getFPXRegsSupported) {
					state_impl->fillFrom(fpxregs);
				} else {
					// No GETFPXREGS: on x86 this means SSE is not supported
					//                on x86_64 FPREGS already contain SSE state
					user_fpregs_struct fpregs;
					if((ptraceStatus=ptrace(PTRACE_GETFPREGS, active_thread(), 0, &fpregs))!=-1)
						state_impl->fillFrom(fpregs);
					else
						perror("PTRACE_GETFPREGS failed");
				}
			}

			// debug registers
			for(std::size_t i=0;i<8;++i)
				state_impl->x86.dbgRegs[i] = get_debug_register(i);
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

		if(auto state_impl = static_cast<PlatformState *>(state.impl_)) {
			bool setRegSetDone=false;
			if(EDB_IS_32_BIT && state_impl->is64Bit()) {
				// Try to set 64-bit state
				PrStatus_X86_64 prstat64;
				state_impl->fillStruct(prstat64);
				iovec prstat_iov = {&prstat64, sizeof(prstat64)};
				if(ptrace(PTRACE_SETREGSET, active_thread(), NT_PRSTATUS, &prstat_iov) != -1)
					setRegSetDone=true;
				else
					perror("PTRACE_SETREGSET failed");
			}
			// Fallback to setting 32-bit set
			if(!setRegSetDone) {
				user_regs_struct regs;
				state_impl->fillStruct(regs);
				ptrace(PTRACE_SETREGS, active_thread(), 0, &regs);
			}

			// debug registers
			for(std::size_t i=0;i<8;++i)
				set_debug_register(i,state_impl->x86.dbgRegs[i]);
		}
	}
}

//------------------------------------------------------------------------------
// Name: open
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {
	detach();

	switch(pid_t pid = fork()) {
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
			
#ifdef PTRACE_O_EXITKILL
			if(ptrace_set_options(pid, PTRACE_O_EXITKILL) == -1) {
				qDebug("[DebuggerCore] failed to set PTRACE_SETOPTIONS: %s", strerror(errno));
				detach();
				return false;
			}
#endif

			// setup the first event data for the primary thread
			waited_threads_.insert(pid);

			const thread_info info = { status, thread_info::THREAD_STOPPED };
			threads_[pid]   = info;

			pid_            = pid;
			active_thread_  = pid;
			event_thread_   = pid;
			binary_info_    = edb::v1::get_binary_info(edb::v1::primary_code_region());

			process_ = new PlatformProcess(this, pid);

			detectDebuggeeBitness();

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
QMap<edb::pid_t, ProcessInfo> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, ProcessInfo> ret;

	QDir proc_directory("/proc/");
	QFileInfoList entries = proc_directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	for(const QFileInfo &info: entries) {
		const QString filename = info.fileName();
		if(is_numeric(filename)) {

			const edb::pid_t pid = filename.toULong();
			ProcessInfo process_info;

			struct user_stat user_stat;
			const int n = get_user_stat(pid, &user_stat);
			if(n >= 2) {
				process_info.name = user_stat.comm;
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
template<class Addr>
QList<Module> loaded_modules_(IProcess* process, IBinary* binary_info_) {
	QList<Module> ret;

	if(binary_info_) {
		BinaryInfo::r_debug<Addr> dynamic_info;
		if(const edb::address_t debug_pointer = binary_info_->debug_pointer()) {
			if(process) {
				if(process->read_bytes(debug_pointer, &dynamic_info, sizeof(dynamic_info))) {
					if(dynamic_info.r_map) {

						auto link_address = edb::address_t::fromZeroExtended(dynamic_info.r_map);

						while(link_address) {

							BinaryInfo::link_map<Addr> map;
							if(process->read_bytes(link_address, &map, sizeof(map))) {
								char path[PATH_MAX];
								if(!process->read_bytes(edb::address_t::fromZeroExtended(map.l_name), &path, sizeof(path))) {
									path[0] = '\0';
								}

								if(map.l_addr) {
									Module module;
									module.name         = path;
									module.base_address = map.l_addr;
									ret.push_back(module);
								}

								link_address = edb::address_t::fromZeroExtended(map.l_next);
							} else {
								break;
							}
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

		for(const IRegion::pointer &region: r) {

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
QList<Module> DebuggerCore::loaded_modules() const {
	if(edb::v1::debuggeeIs64Bit())
		return loaded_modules_<Elf64_Addr>(process(), binary_info_);
	else if(edb::v1::debuggeeIs32Bit())
		return loaded_modules_<Elf32_Addr>(process(), binary_info_);
	else return QList<Module>();
}

//------------------------------------------------------------------------------
// Name:
// Desc: Returns EDB's native CPU type
//------------------------------------------------------------------------------
quint64 DebuggerCore::cpu_type() const {
	if(EDB_IS_32_BIT)
		return edb::string_hash("x86");
	else
		return edb::string_hash("x86-64");
}


//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::format_pointer(edb::address_t address) const {
	return address.toPointerString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::stack_pointer() const {
	if(edb::v1::debuggeeIs32Bit())
		return "esp";
	else
		return "rsp";
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::frame_pointer() const {
	if(edb::v1::debuggeeIs32Bit())
		return "ebp";
	else
		return "rbp";
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::instruction_pointer() const {
	if(edb::v1::debuggeeIs32Bit())
		return "eip";
	else
		return "rip";
}

//------------------------------------------------------------------------------
// Name: flag_register
// Desc: Returns the name of the flag register as a QString.
//------------------------------------------------------------------------------
QString DebuggerCore::flag_register() const {
	if(edb::v1::debuggeeIs32Bit())
		return "eflags";
	else
		return "rflags";
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
ThreadInfo DebuggerCore::get_thread_info(edb::tid_t tid) {

	ThreadInfo info;
	struct user_stat thread_stat;
	int n = get_user_stat(QString("/proc/%1/task/%2/stat").arg(pid_).arg(tid), &thread_stat);	
	if(n >= 30) {
		info.name     = thread_stat.comm;     // 02
		info.tid      = tid;
		info.ip       = thread_stat.kstkeip;  // 18
		info.priority = thread_stat.priority; // 30
		
		switch(thread_stat.state) {           // 03
		case 'R':
			info.state = tr("%1 (Running)").arg(thread_stat.state);
			break;
		case 'S':
			info.state = tr("%1 (Sleeping)").arg(thread_stat.state);
			break;
		case 'D':
			info.state = tr("%1 (Disk Sleep)").arg(thread_stat.state);
			break;		
		case 'T':
			info.state = tr("%1 (Stopped)").arg(thread_stat.state);
			break;		
		case 't':
			info.state = tr("%1 (Tracing Stop)").arg(thread_stat.state);
			break;		
		case 'Z':
			info.state = tr("%1 (Zombie)").arg(thread_stat.state);
			break;		
		case 'X':
		case 'x':
			info.state = tr("%1 (Dead)").arg(thread_stat.state);
			break;
		case 'W':
			info.state = tr("%1 (Waking/Paging)").arg(thread_stat.state);
			break;			
		case 'K':
			info.state = tr("%1 (Wakekill)").arg(thread_stat.state);
			break;		
		case 'P':
			info.state = tr("%1 (Parked)").arg(thread_stat.state);
			break;		
		default:
			info.state = tr("%1").arg(thread_stat.state);
			break;		
		} 
	} else {
		info.name     = QString();
		info.tid      = tid;
		info.ip       = 0;
		info.priority = 0;
		info.state    = '?';
	}
	return info;
}

IProcess *DebuggerCore::process() const {
	return process_;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(DebuggerCore, DebuggerCore)
#endif

}
