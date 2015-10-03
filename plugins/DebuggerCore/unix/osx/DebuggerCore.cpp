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

#include "DebuggerCore.h"
#include "PlatformEvent.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "State.h"
#include "string_hash.h"

#include <QDebug>

#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_region.h>
#include <mach/vm_statistics.h>
#include <paths.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCore {

namespace {
int resume_code(int status) {
	if(WIFSIGNALED(status)) {
		return WTERMSIG(status);
	} else if(WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}
	return 0;
}
}

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() {
	page_size_ = 0x1000;
}

//------------------------------------------------------------------------------
// Name: ~DebuggerCore
// Desc:
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::has_extension(quint64 ext) const {
	return false;
}

//------------------------------------------------------------------------------
// Name: page_size
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
}

//------------------------------------------------------------------------------
// Name: wait_debug_event
// Desc: waits for a debug event, secs is a timeout (but is not yet respected)
//       ok will be set to false if the timeout expires
//------------------------------------------------------------------------------
IDebugEvent::const_pointer DebuggerCore::wait_debug_event(int msecs) {
	if(attached()) {
		int status;
		bool timeout;

		const edb::tid_t tid = native::waitpid_timeout(pid(), &status, 0, msecs, &timeout);
		if(!timeout) {
			if(tid > 0) {
				// normal event
				auto e = std::make_shared<PlatformEvent>();
				e->pid    = pid();
				e->tid    = tid;
				e->status = status;

				active_thread_       = tid;
				threads_[tid].status = status;
				return e;
			}
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: read_data
// Desc:
//------------------------------------------------------------------------------
long DebuggerCore::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);

	mach_port_t task;
	kern_return_t err = task_for_pid(mach_task_self(), pid(), &task);
	if(err != KERN_SUCCESS) {
		qDebug("task_for_pid() failed with %x [%d]", err, pid());
		*ok = false;
		return -1;
	}

	long x;
	vm_size_t size;
	*ok =  vm_read_overwrite(task, address, sizeof(long), (vm_address_t)&x, &size) == 0;
	return x;
}

//------------------------------------------------------------------------------
// Name: write_data
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::write_data(edb::address_t address, long value) {
	return ptrace(PT_WRITE_D, pid(), (char *)address, value) != -1;
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach(edb::pid_t pid) {
	detach();

	const long ret = ptrace(PT_ATTACH, pid, 0, 0);
	if(ret == 0) {
		pid_           = pid;
		active_thread_ = pid;
		threads_.clear();
		threads_.insert(pid, thread_info());

		// TODO: attach to all of the threads
	}

	return ret == 0;
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::detach() {
	if(attached()) {

		// TODO: do i need to stop each thread first, and wait for them?

		clear_breakpoints();
		for(auto it = threads_.begin(); it != threads_.end(); ++it) {
			ptrace(PT_DETACH, it.key(), 0, 0);
		}

		pid_ = 0;
		threads_.clear();
	}
}

//------------------------------------------------------------------------------
// Name: kill
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		clear_breakpoints();
		ptrace(PT_KILL, pid(), 0, 0);
		native::waitpid(pid(), 0, WAIT_ANY);
		pid_ = 0;
		threads_.clear();
	}
}

//------------------------------------------------------------------------------
// Name: pause
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void DebuggerCore::pause() {
	if(attached()) {
		for(auto it = threads_.begin(); it != threads_.end(); ++it) {
			::kill(it.key(), SIGSTOP);
		}
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
			ptrace(PT_CONTINUE, tid, reinterpret_cast<caddr_t>(1), code);
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
			ptrace(PT_STEP, tid, reinterpret_cast<caddr_t>(1), code);
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State *state) {

	Q_ASSERT(state);

	// TODO: assert that we are paused
	auto state_impl = static_cast<PlatformState *>(state->impl_);

	if(attached()) {

		/* Get the mach task for the target process */
		mach_port_t task;
		kern_return_t err = task_for_pid(mach_task_self(), pid(), &task);
		if(err != KERN_SUCCESS) {
			qDebug("task_for_pid() failed with %x [%d]", err, pid());
			return;
		}

		/* Suspend the target process */
		err = task_suspend(task);
		if(err != KERN_SUCCESS) {
			qDebug("task_suspend() failed");
			return;
		}

		/* Get all threads in the specified task */
		thread_act_port_array_t thread_list;
		mach_msg_type_number_t  thread_count;

		err = task_threads(task, &thread_list, &thread_count);
		if(err != KERN_SUCCESS) {
			qDebug("task_threads() failed");
			err = task_resume(task);
			if(err != KERN_SUCCESS) {
				qDebug("task_resume() failed");
			}
		}

		Q_ASSERT(thread_count > 0);

        #ifdef EDB_X86
                mach_msg_type_number_t state_count           = x86_THREAD_STATE32_COUNT;
                const thread_state_flavor_t flavor           = x86_THREAD_STATE32;
                const thread_state_flavor_t debug_flavor     = x86_DEBUG_STATE32;
                const thread_state_flavor_t fpu_flavor       = x86_FLOAT_STATE32;
                const thread_state_flavor_t exception_flavor = x86_EXCEPTION_STATE32;
        #elif defined(EDB_X86_64)
                mach_msg_type_number_t state_count           = x86_THREAD_STATE64_COUNT;
                const thread_state_flavor_t flavor           = x86_THREAD_STATE64;
                const thread_state_flavor_t debug_flavor     = x86_DEBUG_STATE64;
                const thread_state_flavor_t fpu_flavor       = x86_FLOAT_STATE64;
                const thread_state_flavor_t exception_flavor = x86_EXCEPTION_STATE64;
        #endif
                // TODO Get all threads, not just the first one.
                err = thread_get_state(
                        thread_list[0],
                        flavor,
                        (thread_state_t)&state_impl->thread_state_,
                        &state_count);

                if(err != KERN_SUCCESS) {
                        qDebug("thread_get_state() failed with %.08x", err);
                        err = task_resume(task);
                        if(err != KERN_SUCCESS) {
                                qDebug("task_resume() failed");
                        }
                        return;
                }

                err = thread_get_state(
                        thread_list[0],
                        debug_flavor,
                        (thread_state_t)&state_impl->debug_state_,
                        &state_count);

                if(err != KERN_SUCCESS) {
                        qDebug("thread_get_state() failed with %.08x", err);
                        err = task_resume(task);
                        if(err != KERN_SUCCESS) {
                                qDebug("task_resume() failed");
                        }
                        return;
                }

                err = thread_get_state(
                        thread_list[0],
                        fpu_flavor,
                        (thread_state_t)&state_impl->float_state_,
                        &state_count);

                if(err != KERN_SUCCESS) {
                        qDebug("thread_get_state() failed with %.08x", err);
                        err = task_resume(task);
                        if(err != KERN_SUCCESS) {
                                qDebug("task_resume() failed");
                        }
                        return;
                }

                err = thread_get_state(
                        thread_list[0],
                        exception_flavor,
                        (thread_state_t)&state_impl->exception_state_,
                        &state_count);

                if(err != KERN_SUCCESS) {
                        qDebug("thread_get_state() failed with %.08x", err);
                        err = task_resume(task);
                        if(err != KERN_SUCCESS) {
                                qDebug("task_resume() failed");
                        }
                        return;
                }
	} else {
		state->clear();
	}
}

//------------------------------------------------------------------------------
// Name: set_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_state(const State &state) {

	// TODO: assert that we are paused
	auto state_impl = static_cast<PlatformState *>(state.impl_);

	if(attached()) {

		/* Get the mach task for the target process */
		mach_port_t task;
		kern_return_t err = task_for_pid(mach_task_self(), pid(), &task);
		if(err != KERN_SUCCESS) {
			qDebug("task_for_pid() failed with %x [%d]", err, pid());
			return;
		}

		/* Suspend the target process */
		err = task_suspend(task);
		if(err != KERN_SUCCESS) {
			qDebug("task_suspend() failed");
		}

		/* Get all threads in the specified task */
		thread_act_port_array_t thread_list;
		mach_msg_type_number_t  thread_count;
		err = task_threads(task, &thread_list, &thread_count);

		if(err != KERN_SUCCESS) {
			qDebug("task_threads() failed");
			err = task_resume(task);
			if(err != KERN_SUCCESS) {
				qDebug("task_resume() failed");
			}
		}

		Q_ASSERT(thread_count > 0);

        #ifdef EDB_X86
                mach_msg_type_number_t state_count           = x86_THREAD_STATE32_COUNT;
                const thread_state_flavor_t flavor           = x86_THREAD_STATE32;
                const thread_state_flavor_t debug_flavor     = x86_DEBUG_STATE32;
                //const thread_state_flavor_t fpu_flavor       = x86_FLOAT_STATE32;
                //const thread_state_flavor_t exception_flavor = x86_EXCEPTION_STATE32;
        #elif defined(EDB_X86_64)
                mach_msg_type_number_t state_count           = x86_THREAD_STATE64_COUNT;
                const thread_state_flavor_t flavor           = x86_THREAD_STATE64;
                const thread_state_flavor_t debug_flavor     = x86_DEBUG_STATE64;
                //const thread_state_flavor_t fpu_flavor       = x86_FLOAT_STATE64;
                //const thread_state_flavor_t exception_flavor = x86_EXCEPTION_STATE64;
        #endif

                // TODO Set for specific thread, not first one
                err = thread_set_state(
                        thread_list[0],
                        flavor,
                        (thread_state_t)&state_impl->thread_state_,
                        state_count);

		if(err != KERN_SUCCESS) {
			qDebug("thread_set_state() failed with %.08x", err);
			err = task_resume(task);
			if(err != KERN_SUCCESS) {
				qDebug("task_resume() failed");
			}
                        return;
		}

                err = thread_set_state(
                        thread_list[0],
                        debug_flavor,
                        (thread_state_t)&state_impl->debug_state_,
                        state_count);

                if(err != KERN_SUCCESS) {
                        qDebug("thread_set_state() failed with %.08x", err);
                        err = task_resume(task);
                        if(err != KERN_SUCCESS) {
                                qDebug("task_resume() failed");
                        }
                        return;
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
		ptrace(PT_TRACE_ME, 0, 0, 0);

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
		// error!
		pid_ = 0;
		return false;
	default:
		// parent
		do {
			threads_.clear();

			int status;
			if(native::waitpid(pid, &status, 0) == -1) {
				return false;
			}

			// the very first event should be a STOP of type SIGTRAP
			if(!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
				detach();
				return false;
			}

			// setup the first event data for the primary thread
			threads_.insert(pid, thread_info());
			pid_                 = pid;
			active_thread_       = pid;
			threads_[pid].status = status;
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
	Q_ASSERT(threads_.contains(tid));
	active_thread_ = tid;
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

	static const int name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
	size_t length = 0;

	sysctl(const_cast<int*>(name), (sizeof(name) / sizeof(*name)) - 1, 0, &length, 0, 0);
	auto proc_info = static_cast<struct kinfo_proc*>(malloc(length));
	sysctl(const_cast<int*>(name), (sizeof(name) / sizeof(*name)) - 1, proc_info, &length, 0, 0);

	size_t count = length / sizeof(struct kinfo_proc);
	for(size_t i = 0; i < count; ++i) {
		ProcessInfo procInfo;
		procInfo.pid  = proc_info[i].kp_proc.p_pid;
		procInfo.uid  = proc_info[i].kp_eproc.e_ucred.cr_uid;
		procInfo.name = proc_info[i].kp_proc.p_comm;

		ret.insert(procInfo.pid, procInfo);
	}

	free(proc_info);

	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_exe(edb::pid_t pid) const {
	// TODO: implement this
	return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_cwd(edb::pid_t pid) const {
	// TODO: implement this
	return QString();
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::pid_t DebuggerCore::parent_pid(edb::pid_t pid) const {
	// TODO: implement this
	return -1;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<IRegion::pointer> DebuggerCore::memory_regions() const {

#if 0
    static const char * inheritance_strings[] = {
		"SHARE", "COPY", "NONE", "DONATE_COPY",
	};

	static const char * behavior_strings[] = {
		"DEFAULT", "RANDOM", "SEQUENTIAL", "RESQNTL", "WILLNEED", "DONTNEED",
	};
#endif

	QList<IRegion::pointer> regions;
	if(pid_ != 0) {
		task_t the_task;
		kern_return_t kr = task_for_pid(mach_task_self(), pid_, &the_task);
		if(kr != KERN_SUCCESS) {
			qDebug("task_for_pid failed");
            return QList<IRegion::pointer>();
		}

		vm_size_t vmsize;
		vm_address_t address;
		vm_region_basic_info_data_64_t info;
		mach_msg_type_number_t info_count;
		vm_region_flavor_t flavor;
		memory_object_name_t object;

		kr = KERN_SUCCESS;
		address = 0;

		do {
			flavor     = VM_REGION_BASIC_INFO_64;
			info_count = VM_REGION_BASIC_INFO_COUNT_64;
			kr = vm_region_64(the_task, &address, &vmsize, flavor, (vm_region_info_64_t)&info, &info_count, &object);
			if(kr == KERN_SUCCESS) {

				const edb::address_t start               = address;
				const edb::address_t end                 = address + vmsize;
				const edb::address_t base                = address;
				const QString name                       = QString();
				const IRegion::permissions_t permissions =
					((info.protection & VM_PROT_READ)    ? PROT_READ  : 0) |
					((info.protection & VM_PROT_WRITE)   ? PROT_WRITE : 0) |
					((info.protection & VM_PROT_EXECUTE) ? PROT_EXEC  : 0);

				regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));

				/*
				printf("%016llx-%016llx %8uK %c%c%c/%c%c%c %11s %6s %10s uwir=%hu sub=%u\n",
				address, (address + vmsize), (vmsize >> 10),
				(info.protection & VM_PROT_READ)        ? 'r' : '-',
				(info.protection & VM_PROT_WRITE)       ? 'w' : '-',
				(info.protection & VM_PROT_EXECUTE)     ? 'x' : '-',
				(info.max_protection & VM_PROT_READ)    ? 'r' : '-',
				(info.max_protection & VM_PROT_WRITE)   ? 'w' : '-',
				(info.max_protection & VM_PROT_EXECUTE) ? 'x' : '-',
				inheritance_strings[info.inheritance],
				(info.shared) ? "shared" : "-",
				behavior_strings[info.behavior],
				info.user_wired_count,
				info.reserved);
				*/

				address += vmsize;
			} else if(kr != KERN_INVALID_ADDRESS) {
				if(the_task != MACH_PORT_NULL) {
					mach_port_deallocate(mach_task_self(), the_task);
				}
                return QList<IRegion::pointer>();
			}
		} while(kr != KERN_INVALID_ADDRESS);

		if(the_task != MACH_PORT_NULL) {
			mach_port_deallocate(mach_task_self(), the_task);
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
		// TODO: assert attached!
		qDebug() << "TODO: implement edb::v1::get_process_args";
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::process_code_address() const {
	qDebug() << "TODO: implement DebuggerCore::process_code_address";
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::process_data_address() const {
	qDebug() << "TODO: implement DebuggerCore::process_data_address";
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<Module> DebuggerCore::loaded_modules() const {
    QList<Module> modules;
	qDebug() << "TODO: implement DebuggerCore::loaded_modules";
    return modules;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QDateTime DebuggerCore::process_start(edb::pid_t pid) const {
	qDebug() << "TODO: implement DebuggerCore::process_start";
	return QDateTime();
}

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
