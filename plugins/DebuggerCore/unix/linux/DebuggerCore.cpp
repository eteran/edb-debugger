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
#include "Configuration.h"
#include "DialogMemoryAccess.h"
#include "edb.h"
#include "FeatureDetect.h"
#include "MemoryRegions.h"
#include "PlatformCommon.h"
#include "PlatformEvent.h"
#include "PlatformProcess.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "PlatformThread.h"
#include "State.h"
#include "string_hash.h"
#include "Posix.h"
#include "Unix.h"

#include <QDebug>
#include <QDir>
#include <QSettings>

#include <cerrno>
#include <cstring>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#endif

#if defined(EDB_X86) || defined(EDB_X86_64)
#include <cpuid.h>
#endif

#include <sys/ptrace.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/wait.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */

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

#ifndef PTRACE_O_EXITKILL
#define PTRACE_O_EXITKILL	(1 << 20)
#endif

#ifndef PTRACE_O_TRACEEXIT
#define PTRACE_O_TRACEEXIT	(1 << PTRACE_EVENT_EXIT)
#endif

namespace DebuggerCorePlugin {

namespace {

const size_t PageSize = 0x1000;

/**
 * @brief disable_aslr
 */
void disable_aslr() {
	const int current = ::personality(UINT32_MAX);
	// This shouldn't fail, but let's at least perror if it does anyway
	if(current == -1) {
		perror("Failed to get current personality");
	} else if(::personality(current | ADDR_NO_RANDOMIZE) == -1) {
		perror("Failed to disable ASLR");
	}
}

/**
 * @brief disable_lazy_binding
 */
void disable_lazy_binding() {
	if(setenv("LD_BIND_NOW", "1", true) == -1) {
		perror("Failed to disable lazy binding");
	}
}

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
    return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_CLONE << 8)));
}

//------------------------------------------------------------------------------
// Name: is_exit_trace_event
// Desc:
//------------------------------------------------------------------------------
bool is_exit_trace_event(int status) {
    return (status >> 8 == (SIGTRAP | (PTRACE_EVENT_EXIT << 8)));
}

#if defined(EDB_X86) || defined(EDB_X86_64)
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

	if(edbIsIn64BitSegment) {
		osIs64Bit = true;
	} else {
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
#endif

}

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore()
#if defined(EDB_X86) || defined(EDB_X86_64)
    :
    edbIsIn64BitSegment(in64BitSegment()),
    osIs64Bit(os64Bit(edbIsIn64BitSegment)),
    USER_CS_32(osIs64Bit ? 0x23 : 0x73),
    USER_CS_64(osIs64Bit ? 0x33 : 0xfff8), // RPL 0 can't appear in user segment registers, so 0xfff8 is safe
    USER_SS(osIs64Bit    ? 0x2b : 0x7b)
#endif
	 {

	Posix::initialize();

	feature::detect_proc_access(&proc_mem_read_broken_, &proc_mem_write_broken_);

	if(proc_mem_read_broken_ || proc_mem_write_broken_) {

		qDebug() << "Detect that read /proc/<pid>/mem works  = " << !proc_mem_read_broken_;
		qDebug() << "Detect that write /proc/<pid>/mem works = " << !proc_mem_write_broken_;

		QSettings settings;
		const bool warn = settings.value("DebuggerCore/warn_on_broken_proc_mem.enabled", true).toBool();
		if(warn) {
			auto dialog = new DialogMemoryAccess(nullptr);
			dialog->exec();

			settings.setValue("DebuggerCore/warn_on_broken_proc_mem.enabled", dialog->warnNextTime());

			delete dialog;
		}
	}
}

//------------------------------------------------------------------------------
// Name: has_extension
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::has_extension(quint64 ext) const {
	
	Q_UNUSED(ext)
	
#if defined(EDB_X86) || defined(EDB_X86_64)
	static constexpr auto mmxHash = edb::string_hash("MMX");
	static constexpr auto xmmHash = edb::string_hash("XMM");
	static constexpr auto ymmHash = edb::string_hash("YMM");

	if(EDB_IS_64_BIT && (ext == xmmHash || ext == mmxHash)) {
		return true;
	}

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
		if((ecx & 0x18000000) != 0x18000000) {
			return false;
		}

		// Get XCR0, must be exactly after OSXSAVE feature check, otherwise #UD
		__asm__ __volatile__("xgetbv" : "=a"(eax),"=d"(edx) : "c"(0));

		// Check that the OS has enabled XMM and YMM state support
		if((eax & 0x6) != 0x6) {
			return false;
		}
		return true;
	}
	default:
		return false;
	}
#else
	return false;
#endif
}

//------------------------------------------------------------------------------
// Name: page_size
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
size_t DebuggerCore::page_size() const {
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
	end_debug_session();
}

//------------------------------------------------------------------------------
// Name: ptrace_getsiginfo
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::ptrace_getsiginfo(edb::tid_t tid, siginfo_t *siginfo) {

	Q_ASSERT(siginfo);

	if(ptrace(PTRACE_GETSIGINFO, tid, 0, siginfo)==-1) {
		const char *const strError = strerror(errno);
		qWarning() << "Unable to get signal info for thread" << tid << ": PTRACE_GETSIGINFO failed:" << strError;
		return Status(strError);
	}
	return Status::Ok;
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
Status DebuggerCore::ptrace_continue(edb::tid_t tid, long status) {
	// TODO(eteran): perhaps address this at a higher layer?
	//               I would like to not have these events show up
	//               in the first place if we aren't stopped on this TID :-(
	if(util::contains(waited_threads_, tid)) {
		Q_ASSERT(tid != 0);
		if(ptrace(PTRACE_CONT, tid, 0, status) == -1) {
			const char *const strError = strerror(errno);
			qWarning() << "Unable to continue thread" << tid << ": PTRACE_CONT failed:" << strError;
			return Status(strError);
		}
		waited_threads_.remove(tid);
		return Status::Ok;
	}
	return Status(tr("ptrace_continue(): waited_threads_ doesn't contain tid %1").arg(tid));
}

//------------------------------------------------------------------------------
// Name: ptrace_step
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::ptrace_step(edb::tid_t tid, long status) {
	// TODO(eteran): perhaps address this at a higher layer?
	//               I would like to not have these events show up
	//               in the first place if we aren't stopped on this TID :-(
	if(util::contains(waited_threads_, tid)) {
		Q_ASSERT(tid != 0);
		if(ptrace(PTRACE_SINGLESTEP, tid, 0, status)==-1) {
			const char *const strError = strerror(errno);
			qWarning() << "Unable to step thread" << tid << ": PTRACE_SINGLESTEP failed:" << strError;
			return Status(strError);
		}
		waited_threads_.remove(tid);
		return Status::Ok;
	}
	return Status(tr("ptrace_step(): waited_threads_ doesn't contain tid %1").arg(tid));
}

//------------------------------------------------------------------------------
// Name: ptrace_set_options
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::ptrace_set_options(edb::tid_t tid, long options) {
	Q_ASSERT(util::contains(waited_threads_, tid));
	Q_ASSERT(tid != 0);
	if(ptrace(PTRACE_SETOPTIONS, tid, 0, options)==-1) {
		const char *const strError = strerror(errno);
		qWarning() << "Unable to set ptrace options for thread" << tid << ": PTRACE_SETOPTIONS failed:" << strError;
		return Status(strError);
	}
	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: ptrace_get_event_message
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::ptrace_get_event_message(edb::tid_t tid, unsigned long *message) {
	Q_ASSERT(util::contains(waited_threads_, tid));
	Q_ASSERT(tid != 0);
	Q_ASSERT(message);

	if(ptrace(PTRACE_GETEVENTMSG, tid, 0, message)==-1) {
		const char *const strError = strerror(errno);
		qWarning() << "Unable to get event message for thread" << tid << ": PTRACE_GETEVENTMSG failed:" << strError;
		return Status(strError);
	}
	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: desired_ptrace_options
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::ptraceOptions() const {

    // we want to trace clone (thread) creation events
    long options = PTRACE_O_TRACECLONE;

    // if applicable, we want an auto SIGKILL sent to the child
    // process and its threads
    switch(edb::v1::config().close_behavior) {
    case Configuration::Kill:
        options |= PTRACE_O_EXITKILL;
        break;
    case Configuration::KillIfLaunchedDetachIfAttached:
        if(last_means_of_capture() == MeansOfCapture::Launch) {
            options |= PTRACE_O_EXITKILL;
        }
        break;
    default:
        break;
    }

#if 0
    // TODO(eteran): research this option for issue #46
    options |= PTRACE_O_TRACEEXIT;
#endif

    return options;
}

//------------------------------------------------------------------------------
// Name: handle_thread_exit
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::handle_thread_exit(edb::tid_t tid, int status) {
	Q_UNUSED(status)

	threads_.remove(tid);
	waited_threads_.remove(tid);
}

//------------------------------------------------------------------------------
// Name: handle_thread_create_event
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IDebugEvent> DebuggerCore::handle_thread_create_event(edb::tid_t tid, int status) {

	Q_UNUSED(status)

	unsigned long message;
	if(ptrace_get_event_message(tid, &message)) {

		auto new_tid = static_cast<edb::tid_t>(message);

		auto newThread = std::make_shared<PlatformThread>(this, process_, new_tid);

		threads_.insert(new_tid, newThread);

		int thread_status = 0;
		if(!util::contains(waited_threads_, new_tid)) {
			if(Posix::waitpid(new_tid, &thread_status, __WALL) > 0) {
				waited_threads_.insert(new_tid);
			}
		}

		// A new thread could exit before we have fully created it, no event then since it can't be the last thread
		if(WIFEXITED(thread_status)) {
			handle_thread_exit(tid, thread_status);
			return nullptr;
		}

		if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
			qWarning("handle_event(): new thread [%d] received an event besides SIGSTOP: status=0x%x", static_cast<int>(new_tid),thread_status);
		}

		newThread->status_ = thread_status;

		// copy the hardware debug registers from the current thread to the new thread
		if(process_) {
			if(auto cur_thread = process_->current_thread()) {
				for(size_t i = 0; i < 8; ++i) {
					auto new_thread = std::static_pointer_cast<PlatformThread>(newThread);
					auto old_thread = std::static_pointer_cast<PlatformThread>(cur_thread);
					new_thread->set_debug_register(i, old_thread->get_debug_register(i));
				}
			}
		}

		newThread->resume();
	}

	ptrace_continue(tid, 0);
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: handle_event
// Desc:
//------------------------------------------------------------------------------
std::shared_ptr<IDebugEvent> DebuggerCore::handle_event(edb::tid_t tid, int status) {

	// note that we have waited on this thread
	waited_threads_.insert(tid);

	// was it a thread exit event?
	if(WIFEXITED(status)) {

		handle_thread_exit(tid,status);
		// if this was the last thread, return nullptr
		// so we report it to the user.
		// if this wasn't, then we should silently
		// procceed.
		if(!threads_.empty()) {
			return nullptr;
		}
	}

    if(is_exit_trace_event(status)) {

    }

	// was it a thread create event?
	if(is_clone_event(status)) {
		return handle_thread_create_event(tid, status);
	}

	// normal event
	auto e = std::make_shared<PlatformEvent>();

	e->pid_    = process_->pid();
	e->tid_    = tid;
	e->status_ = status;
	if(!ptrace_getsiginfo(tid, &e->siginfo_)) {
		// TODO: handle no info?
	}

	// if necessary, just ignore this event
	if(util::contains(ignored_exceptions_, e->code())) {
		ptrace_continue(tid, resume_code(status));
	}

	/* NOTE(eteran): OK, so when we get an event, we generally want to stop
	 * any other threads as well. So we will call stop_threads() below
	 * which sends a SIGSTOP.
	 *
	 * We need to be very careful to avoid those future events causing the
	 * active thread to be set, because we want it to remain set to the thread
	 * which recieved the initial signal. This is all so that later when the
	 * user clicks resume, that the correct active thread gets (or doesn't)
	 * get signals, and the rest get resumed properly.
	 *
	 * To do this, we simply only alter the active_thread_ variable if this
	 * event was the first we saw after a resume/run (phew!).*/
	if(waited_threads_.size() == 1) {
		active_thread_ = tid;
	}

	auto it = threads_.find(tid);
	if(it != threads_.end()) {
		it.value()->status_ = status;
	}

	stop_threads();

	// Some breakpoint types result in SIGILL or SIGSEGV. We'll transform the
	// event into breakpoint event if such a breakpoint has triggered.
	if(it != threads_.end() && WIFSTOPPED(status)) {
		const auto signo = WSTOPSIG(status);
		if(signo == SIGILL || signo == SIGSEGV) {
			// no need to peekuser for SIGILL, but have to for SIGSEGV
			const auto address = signo == SIGILL ? edb::address_t::fromZeroExtended(e->siginfo_.si_addr)
											   : (*it)->instruction_pointer();

			if(edb::v1::find_triggered_breakpoint(address)) {
				e->status_ = SIGTRAP << 8 | 0x7f;
				e->siginfo_.si_signo = SIGTRAP;
				e->siginfo_.si_code  = TRAP_BRKPT;
			}
		}
	}

#if defined EDB_ARM32
	if(it != threads_.end()) {
		const auto& thread = *it;
		if(thread->singleStepBreakpoint) {

			remove_breakpoint(thread->singleStepBreakpoint->address());
			thread->singleStepBreakpoint = nullptr;

			assert(e->siginfo_.si_signo == SIGTRAP); // signo must have already be converted to SIGTRAP if needed
			e->siginfo_.si_code = TRAP_TRACE;
		}
	}
#endif
	return e;
}

//------------------------------------------------------------------------------
// Name: stop_threads
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::stop_threads() {

	QString errorMessage;

	if(process_) {
		for(auto &thread: process_->threads()) {
			const edb::tid_t tid = thread->tid();

			if(!util::contains(waited_threads_, tid)) {

				if(auto thread_ptr = std::static_pointer_cast<PlatformThread>(thread)) {

					if(syscall(SYS_tgkill, process_->pid(), thread->tid(), SIGSTOP) == -1) {
						const char *const error = strerror(errno);
						errorMessage += tr("Failed to stop thread %1: %2\n").arg(tid).arg(error);
					}

					int thread_status;
					if(Posix::waitpid(thread->tid(), &thread_status, __WALL/* | WNOHANG*/) > 0) {
						waited_threads_.insert(tid);
						thread_ptr->status_ = thread_status;

						// A thread could have exited between previous waitpid and the latest one...
						if(WIFEXITED(thread_status)) {
							handle_thread_exit(tid, thread_status);
						}

						// ..., otherwise it must have stopped.
						else if(!WIFSTOPPED(thread_status) || WSTOPSIG(thread_status) != SIGSTOP) {
							qWarning("stop_threads(): paused thread [%d] received an event besides SIGSTOP: status=0x%x", tid, thread_status);
						}
					}
				}
			}
		}
	}

	if(errorMessage.isEmpty()) {
		return Status::Ok;
	}

	qWarning() << qPrintable(errorMessage);
	return Status("\n" + errorMessage);
}

//------------------------------------------------------------------------------
// Name: wait_debug_event
// Desc: waits for a debug event, msecs is a timeout
//       it will return nullptr if an error or timeout occurs
//------------------------------------------------------------------------------
std::shared_ptr<IDebugEvent> DebuggerCore::wait_debug_event(int msecs) {

	if(process_) {
		if(!Posix::wait_for_sigchld(msecs)) {
			for(auto &thread : process_->threads()) {
				int status;
				const edb::tid_t tid = Posix::waitpid(thread->tid(), &status, __WALL | WNOHANG);
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
// Desc: returns 0 if successful, errno if failed
//------------------------------------------------------------------------------
int DebuggerCore::attach_thread(edb::tid_t tid) {

	if(ptrace(PTRACE_ATTACH, tid, 0, 0) == 0) {

		int status;
		const int ret = Posix::waitpid(tid, &status, __WALL);
		if(ret > 0) {

			auto newThread     = std::make_shared<PlatformThread>(this, process_, tid);
			newThread->status_ = status;

			threads_.insert(tid, newThread);
			waited_threads_.insert(tid);

            const long options = ptraceOptions();

			const auto setoptStatus = ptrace_set_options(tid, options);
			if(!setoptStatus)	{
				qDebug() << "[DebuggerCore] failed to set ptrace options: ["<< tid <<"]" << setoptStatus.error();
            }

			return 0;
		} else if(ret == -1) {
			return errno;
        } else {
            return -1; // unknown error
        }
    } else {
        return errno;
    }
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::attach(edb::pid_t pid) {

	end_debug_session();

	lastMeansOfCapture = MeansOfCapture::Attach;

	// create this, so the threads created can refer to it
	process_ = std::make_shared<PlatformProcess>(this, pid);

	int lastErr = attach_thread(pid); // Fail early if we are going to
	if(lastErr) {
		process_ = nullptr;
		return Status(std::strerror(lastErr));
	}

	lastErr = -2;
	bool attached;
	do {
		attached = false;
		QDir proc_directory(QString("/proc/%1/task/").arg(pid));
		for(const QString &s: proc_directory.entryList(QDir::NoDotAndDotDot | QDir::Dirs)) {
			// this can get tricky if the threads decide to spawn new threads
			// when we are attaching. I wish that linux had an atomic way to do this
			// all in one shot
			const edb::tid_t tid = s.toInt();
			if(!threads_.contains(tid)) {
                const auto errnum = attach_thread(tid);
				if(errnum == 0) {
					attached = true;
				} else {
					lastErr = errnum;
				}
			}
		}
	} while(attached);


	if(!threads_.empty()) {
		active_thread_  = pid;
		detectCPUMode();
		return Status::Ok;
	}

    process_ = nullptr;
	return Status(std::strerror(lastErr));
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::detach() {

	QString errorMessage;

	if(process_) {
		stop_threads();
		clear_breakpoints();

		for(auto &thread: process_->threads()) {
			if(ptrace(PTRACE_DETACH, thread->tid(), 0, 0)==-1) {
				const char *const error = strerror(errno);
				errorMessage+=tr("Unable to detach from thread %1: PTRACE_DETACH failed: %2\n").arg(thread->tid()).arg(error);
			}
		}

		process_ = nullptr;
		reset();
	}

	if(errorMessage.isEmpty()) {
		return Status::Ok;
	}

	qWarning() << errorMessage.toStdString().c_str();
	return Status(errorMessage);
}

//------------------------------------------------------------------------------
// Name: kill
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		clear_breakpoints();

		::kill(process_->pid(), SIGKILL);

		pid_t ret;
		while((ret = Posix::waitpid(-1, nullptr, __WALL)) != process_->pid() && ret != -1);

		process_ = nullptr;
		reset();
	}
}

void DebuggerCore::detectCPUMode() {

#if defined(EDB_X86) || defined(EDB_X86_64)
	const size_t offset = EDB_IS_64_BIT ?
	                          offsetof(UserRegsStructX86_64, cs) :
	                          offsetof(UserRegsStructX86, xcs);
	errno = 0;
	const edb::seg_reg_t cs = ptrace(PTRACE_PEEKUSER, active_thread_, offset, 0);

	if (!errno) {
		if (cs == USER_CS_32) {
			if (pointer_size_ == sizeof(quint64)) {
				qDebug() << "Debuggee is now 32 bit";
				cpu_mode_ = CPUMode::x86_32;
				CapstoneEDB::init(CapstoneEDB::Architecture::ARCH_X86);
			}
			pointer_size_ = sizeof(quint32);
			return;
		} else if (cs == USER_CS_64) {
			if (pointer_size_ == sizeof(quint32)) {
				qDebug() << "Debuggee is now 64 bit";
				cpu_mode_ = CPUMode::x86_64;
				CapstoneEDB::init(CapstoneEDB::Architecture::ARCH_AMD64);
			}
			pointer_size_ = sizeof(quint64);
			return;
		}
	}
#elif defined(EDB_ARM32)
	errno = 0;
	const auto cpsr = ptrace(PTRACE_PEEKUSER, active_thread_, sizeof(long) * 16, 0L);
	if (!errno) {
		const bool thumb = cpsr & 0x20;
		if (thumb) {
			cpu_mode_ = CPUMode::Thumb;
			CapstoneEDB::init(CapstoneEDB::Architecture::ARCH_ARM32_THUMB);
		} else {
			cpu_mode_ = CPUMode::ARM32;
			CapstoneEDB::init(CapstoneEDB::Architecture::ARCH_ARM32_ARM);
		}
	}
	pointer_size_ = sizeof(quint32);
#elif defined(EDB_ARM64)
	cpu_mode_ = CPUMode::ARM64;
	CapstoneEDB::init(CapstoneEDB::Architecture::ARCH_ARM64);
	pointer_size_ = sizeof(quint64);
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name: open
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {

	end_debug_session();

    lastMeansOfCapture = MeansOfCapture::Launch;

	static constexpr std::size_t sharedMemSize = 4096;

	void *const ptr = ::mmap(nullptr, sharedMemSize, PROT_READ|PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	const auto sharedMem = static_cast<QChar*>(ptr);

	std::memset(ptr, 0, sharedMemSize);

	switch(pid_t pid = fork()) {
	case 0:
	{
		// we are in the child now...

		// set ourselves (the child proc) up to be traced
		ptrace_traceme();

		// redirect it's I/O
		if(!tty.isEmpty()) {
			FILE *const std_out = freopen(qPrintable(tty), "r+b", stdout);
			FILE *const std_in  = freopen(qPrintable(tty), "r+b", stdin);
			FILE *const std_err = freopen(qPrintable(tty), "r+b", stderr);

			Q_UNUSED(std_out)
			Q_UNUSED(std_in)
			Q_UNUSED(std_err)
		}

		if(edb::v1::config().disableASLR) {
			disable_aslr();
		}

		if(edb::v1::config().disableLazyBinding) {
			disable_lazy_binding();
		}

		// do the actual exec
		const Status status = Unix::execute_process(path, cwd, args);

#if defined __GNUG__ && __GNUC__ >= 5 || !defined __GNUG__ || defined __clang__ && __clang_major__*100+__clang_minor__>=306
		static_assert(std::is_trivially_copyable<QChar>::value, "Can't copy string of QChar to shared memory");
#endif
		QString error = status.error();
		std::memcpy(sharedMem, error.constData(), std::min(sizeof(QChar) * error.size(), sharedMemSize - sizeof(QChar) /*prevent overwriting of last null*/ ));

		// we should never get here!
		abort();
	}
	case -1:
		// error! for some reason we couldn't fork
		reset();
		return Status(tr("Failed to fork"));
	default:
		// parent
		do {
			reset();

			int status;
			const auto wpidRet = Posix::waitpid(pid, &status, __WALL);
			const QString childError(sharedMem);
            ::munmap(sharedMem, sharedMemSize);

            if(wpidRet == -1) {
				return Status(tr("waitpid() failed: %1").arg(std::strerror(errno))+(childError.isEmpty()?"" : tr(".\nError returned by child:\n%1.").arg(childError)));
            }

            if(WIFEXITED(status)) {
				return Status(tr("The child unexpectedly exited with code %1. Error returned by child:\n%2").arg(WEXITSTATUS(status)).arg(childError));
            }

            if(WIFSIGNALED(status)) {
				return Status(tr("The child was unexpectedly killed by signal %1. Error returned by child:\n%2").arg(WTERMSIG(status)).arg(childError));
            }

			// This happens when exec failed, but just in case it's something another return some description.
            if(WIFSTOPPED(status) && WSTOPSIG(status) == SIGABRT) {
				return Status(childError.isEmpty() ? tr("The child unexpectedly aborted") : childError);
            }

			// the very first event should be a STOP of type SIGTRAP
			if(!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
				end_debug_session();
				return Status(tr("First event after waitpid() should be a STOP of type SIGTRAP, but wasn't, instead status=0x%1")
											.arg(status,0,16)+(childError.isEmpty() ? "" : tr(".\nError returned by child:\n%1.").arg(childError)));
			}

            waited_threads_.insert(pid);

            const long options = ptraceOptions();

            // enable following clones (threads) and other options we are concerned with
			const auto setoptStatus=ptrace_set_options(pid, options);
            if(!setoptStatus) {
                end_debug_session();
				return Status(tr("[DebuggerCore] failed to set ptrace options: %1").arg(setoptStatus.error()));
            }

            // create the process
			process_ = std::make_shared<PlatformProcess>(this, pid);

			// the PID == primary TID
			auto newThread     = std::make_shared<PlatformThread>(this, process_, pid);
			newThread->status_ = status;

			threads_.insert(pid, newThread);

            active_thread_  = pid;
			detectCPUMode();

			return Status::Ok;
		} while(0);
		break;
	}
}


//------------------------------------------------------------------------------
// Name: last_means_of_capture
// Desc: Returns how the last process was captured to debug
//------------------------------------------------------------------------------
DebuggerCore::MeansOfCapture DebuggerCore::last_means_of_capture() const {
	return lastMeansOfCapture;
}

//------------------------------------------------------------------------------
// Name: reset
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::reset() {
	threads_.clear();
	waited_threads_.clear();
	active_thread_ = 0;
}

//------------------------------------------------------------------------------
// Name: create_state
// Desc:
//------------------------------------------------------------------------------
std::unique_ptr<IState> DebuggerCore::create_state() const {
	return std::make_unique<PlatformState>();
}

//------------------------------------------------------------------------------
// Name: enumerate_processes
// Desc:
//------------------------------------------------------------------------------
QMap<edb::pid_t, std::shared_ptr<IProcess>> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, std::shared_ptr<IProcess>> ret;

	QDir proc_directory("/proc/");
	QFileInfoList entries = proc_directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);

	for(const QFileInfo &info: entries) {
		const QString filename = info.fileName();
		if(is_numeric(filename)) {
			const edb::pid_t pid = filename.toInt();

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
#if defined EDB_X86 || defined EDB_X86_64
	if(EDB_IS_32_BIT) {
		return edb::string_hash("x86");
	} else {
		return edb::string_hash("x86-64");
	}
#elif defined EDB_ARM32
	return edb::string_hash("arm");
#elif defined EDB_ARM64
	return edb::string_hash("AArch64");
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::stack_pointer() const {
#if defined EDB_X86 || defined EDB_X86_64
	if(edb::v1::debuggeeIs32Bit()) {
		return "esp";
	} else {
		return "rsp";
	}
#elif defined EDB_ARM32 || defined EDB_ARM64
	return "sp";
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::frame_pointer() const {
#if defined EDB_X86 || defined EDB_X86_64
	if(edb::v1::debuggeeIs32Bit()) {
		return "ebp";
	} else {
		return "rbp";
	}
#elif defined EDB_ARM32 || defined EDB_ARM64
	return "fp";
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::instruction_pointer() const {
#if defined EDB_X86 || defined EDB_X86_64
	if(edb::v1::debuggeeIs32Bit()) {
		return "eip";
	} else {
		return "rip";
	}
#elif defined EDB_ARM32 || defined EDB_ARM64
	return "pc";
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name: flag_register
// Desc: Returns the name of the flag register as a QString.
//------------------------------------------------------------------------------
QString DebuggerCore::flag_register() const {
#if defined EDB_X86 || defined EDB_X86_64
	if(edb::v1::debuggeeIs32Bit()) {
		return "eflags";
	} else {
		return "rflags";
	}
#elif defined EDB_ARM32 || defined EDB_ARM64
	return "cpsr";
#else
    #error "Unsupported Architecture"
#endif
}

//------------------------------------------------------------------------------
// Name: process
// Desc:
//------------------------------------------------------------------------------
IProcess *DebuggerCore::process() const {
	return process_.get();
}

//------------------------------------------------------------------------------
// Name: set_ignored_exceptions
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_ignored_exceptions(const QList<qlonglong> &exceptions) {
	ignored_exceptions_ = exceptions;
}

/**
 * @brief DebuggerCore::exceptions
 * @return
 */
QMap<qlonglong, QString> DebuggerCore::exceptions() const {
	return Unix::exceptions();
}

/**
 * @brief DebuggerCore::exceptionName
 * @param value
 * @return
 */
QString DebuggerCore::exceptionName(qlonglong value) {
	return Unix::exceptionName(value);
}

/**
 * @brief DebuggerCore::exceptionValue
 * @param name
 * @return
 */
qlonglong DebuggerCore::exceptionValue(const QString &name) {
	return Unix::exceptionValue(name);
}

uint8_t DebuggerCore::nopFillByte() const {
#if defined EDB_X86 || defined EDB_X86_64
	return 0x90;
#elif defined EDB_ARM32 || defined EDB_ARM64
	// TODO(eteran): does this concept even make sense for a multi-byte instruction encoding?
	return 0x00;
#else
	#error "Unsupported Architecture"
#endif
}

}
