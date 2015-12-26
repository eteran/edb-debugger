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


// TODO(eteran): research usage of process_vm_readv, process_vm_writev

#include "DebuggerCore.h"
#include "edb.h"
#include "MemoryRegions.h"
#include "PlatformEvent.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "PlatformProcess.h"
#include "PlatformCommon.h"
#include "State.h"
#include "string_hash.h"

#include <QDebug>
#include <QDir>

#include <cerrno>
#include <cstring>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#include <cpuid.h>
#include <sys/ptrace.h>

// doesn't always seem to be defined in the headers

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

const edb::address_t PageSize = 0x1000;

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
// Name: is_clone_event
// Desc:
//------------------------------------------------------------------------------
bool is_clone_event(int status) {
	if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
		return (((status >> 16) & 0xffff) == PTRACE_EVENT_CLONE);
	}

	return false;
}

bool in64BitSegment() {
	bool edbIsIn64BitSegment;
	// Check that we're running in 64 bit segment: this can be in cases
	// of LP64 and ILP32 programming models, so we can't rely on sizeof(void*)
	asm(R"(
		   .byte 0x33,0xc0 # XOR EAX,EAX
		   .byte 0x48      # DEC EAX for 32 bit, REX prefix for 64 bit
		   .byte 0xff,0xc0 # INC EAX for 32 bit, INC RAX due to REX.W in 64 bit
		 )":"=a"(edbIsIn64BitSegment));
	return edbIsIn64BitSegment;
}

bool os64Bit(bool edbIsIn64BitSegment) {
	bool osIs64Bit;
	if(edbIsIn64BitSegment)
		osIs64Bit=true;
	else {
		// We want to be really sure the OS is 32 bit, so we can't rely on such easy
		// to (even unintentionally) fake mechanisms as uname(2) (e.g. see setarch(8))
		asm(R"(.intel_syntax noprefix
			   mov eax,cs
			   cmp ax,0x23 # this value is set for 32-bit processes on 64-bit kernel
			   mov ah,0    # not sure this is really needed: usually the compiler will do
						   # MOVZX EAX,AL, but we have to be certain the result is correct
			   sete al
			   .att_syntax # restore default syntax
			   )":"=a"(osIs64Bit));
	}
	return osIs64Bit;
}


}

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() : 
	binary_info_(nullptr),
	process_(0),
	pointer_size_(sizeof(void*)),
	edbIsIn64BitSegment(in64BitSegment()),
	osIs64Bit(os64Bit(edbIsIn64BitSegment)),
	USER_CS_32(osIs64Bit?0x23:0x73),
	USER_CS_64(osIs64Bit?0x33:0xfff8), // RPL 0 can't appear in user segment registers, so 0xfff8 is safe
	USER_SS(osIs64Bit?0x2b:0x7b)
{
	qDebug() << "EDB is in" << (edbIsIn64BitSegment?"64":"32") << "bit segment";
	qDebug() << "OS is" << (osIs64Bit?"64":"32") << "bit";
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
	return PageSize;
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

			auto newThread            = std::make_shared<PlatformThread>(this, process_, new_tid);
			newThread->status_        = 0;
			newThread->signal_status_ = PlatformThread::Stopped;

			threads_.insert(new_tid, newThread);

			int thread_status = 0;
			if(!waited_threads_.contains(new_tid)) {
				if(native::waitpid(new_tid, &thread_status, __WALL) > 0) {
					waited_threads_.insert(new_tid);
				}
			}

			if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
				qDebug("[warning] new thread [%d] received an event besides SIGSTOP", static_cast<int>(new_tid));
			}
			
			newThread->status_ = thread_status;

			// copy the hardware debug registers from the current thread to the new thread
			if(process_) {
				if(auto thread = process_->current_thread()) {
					for(int i = 0; i < 8; ++i) {
						auto new_thread = std::static_pointer_cast<PlatformThread>(newThread);
						auto old_thread = std::static_pointer_cast<PlatformThread>(thread);
						new_thread->set_debug_register(i, old_thread->get_debug_register(i));
					}
				}
			}

			// TODO(eteran): what the heck do we do if this isn't a SIGSTOP?
			newThread->resume();
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

	active_thread_ = tid;
	
	auto it = threads_.find(tid);
	if(it != threads_.end()) {
		it.value()->status_ = status;
	}

	stop_threads();
	return e;
}

//------------------------------------------------------------------------------
// Name: stop_threads
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::stop_threads() {
	if(process_) {
		for(auto &thread: process_->threads()) {
			const edb::tid_t tid = thread->tid();

			if(!waited_threads_.contains(tid)) {
			
				if(auto thread_ptr = std::static_pointer_cast<PlatformThread>(thread)) {
			
					thread->stop();

					int thread_status;
					if(native::waitpid(tid, &thread_status, __WALL) > 0) {
						waited_threads_.insert(tid);
						thread_ptr->status_ = thread_status;

						if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
							qDebug("[warning] paused thread [%d] received an event besides SIGSTOP", tid);
						}
					}
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

	if(process_) {
		if(!native::wait_for_sigchld(msecs)) {
			for(auto thread : process_->threads()) {
				int status;
				const edb::tid_t tid = native::waitpid(thread->tid(), &status, __WALL | WNOHANG);
				if(tid > 0) {
					return handle_event(tid, status);
				}
			}
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: attach_thread
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach_thread(edb::tid_t tid) {
	if(ptrace(PTRACE_ATTACH, tid, 0, 0) == 0) {
		// I *think* that the PTRACE_O_TRACECLONE is only valid on
		// stopped threads
		int status;
		if(native::waitpid(tid, &status, __WALL) > 0) {

			auto newThread            = std::make_shared<PlatformThread>(this, process_, tid);
			newThread->status_        = status;
			newThread->signal_status_ = PlatformThread::Stopped;

			threads_[tid] = newThread;

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
	
	// create this, so the threads created can refer to it
	process_ = new PlatformProcess(this, pid);

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
		binary_info_    = edb::v1::get_binary_info(edb::v1::primary_code_region());		
		detectDebuggeeBitness();
		return true;
	} else {
		delete process_;
		process_ = nullptr;
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::detach() {
	if(process_) {

		stop_threads();

		clear_breakpoints();

		for(auto thread: process_->threads()) {
			ptrace(PTRACE_DETACH, thread->tid(), 0, 0);
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

		::kill(pid(), SIGKILL);

		// TODO: do i need to actually do this wait?
		native::waitpid(pid(), 0, __WALL);

		delete process_;
		process_ = 0;

		reset();
	}
}

void DebuggerCore::detectDebuggeeBitness() {

	const size_t offset=EDB_IS_64_BIT ?
						offsetof(UserRegsStructX86_64, cs) :
						offsetof(UserRegsStructX86,   xcs);
	errno=0;
	const edb::seg_reg_t cs=ptrace(PTRACE_PEEKUSER, active_thread_, offset, 0);
	if(!errno) {
		if(cs==USER_CS_32) {
			if(pointer_size_==sizeof(quint64)) {
				qDebug() << "Debuggee is now 32 bit";
				CapstoneEDB::init(false);
			}
			pointer_size_=sizeof(quint32);
			return;
		} else if(cs==USER_CS_64) {
			if(pointer_size_==sizeof(quint32)) {
				qDebug() << "Debuggee is now 64 bit";
				CapstoneEDB::init(true);
			}
			pointer_size_=sizeof(quint64);
			return;
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State *state) {
	// TODO: assert that we are paused	
	if(process_) {
		if(IThread::pointer thread = process_->current_thread()) {
			thread->get_state(state);
		}
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_state(const State &state) {

	// TODO: assert that we are paused
	if(process_) {
		if(IThread::pointer thread = process_->current_thread()) {
			thread->set_state(state);
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
			
			// create the process
			process_ = new PlatformProcess(this, pid);
			

			// the PID == primary TID
			auto newThread            = std::make_shared<PlatformThread>(this, process_, pid);
			newThread->status_        = status;
			newThread->signal_status_ = PlatformThread::Stopped;
			
			threads_[pid]   = newThread;

			pid_            = pid;
			active_thread_   = pid;
			binary_info_    = edb::v1::get_binary_info(edb::v1::primary_code_region());

			detectDebuggeeBitness();

			return true;
		} while(0);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::reset() {
	threads_.clear();
	waited_threads_.clear();
	pid_           = 0;
	active_thread_ = 0;
	binary_info_   = nullptr;
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
QMap<edb::pid_t, IProcess::pointer> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, IProcess::pointer> ret;

	QDir proc_directory("/proc/");
	QFileInfoList entries = proc_directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	for(const QFileInfo &info: entries) {
		const QString filename = info.fileName();
		if(is_numeric(filename)) {
			const edb::pid_t pid = filename.toULong();
			
			// NOTE(eteran): the const_cast is reasonable here.
			// While we don't want THIS function to mutate the DebuggerCore object
			// we do want the associated PlatformProcess to be able to trigger
			// non-const operations in the future, at least hypothetically.
			ret.insert(pid, std::make_shared<PlatformProcess>(const_cast<DebuggerCore*>(this), pid));
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
// Name: process
// Desc: 
//------------------------------------------------------------------------------
IProcess *DebuggerCore::process() const {
	return process_;
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(DebuggerCore, DebuggerCore)
#endif

}
