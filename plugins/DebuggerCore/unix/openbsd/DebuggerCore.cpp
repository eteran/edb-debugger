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
#include <QMessageBox>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <kvm.h>
#include <machine/reg.h>
#include <paths.h>
#include <signal.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <uvm/uvm.h>

#define __need_process
#include <sys/sysctl.h>
#include <sys/proc.h>

namespace DebuggerCore {

namespace {

void SET_OK(bool &ok, long value) {
	ok = (value != -1) || (errno == 0);
}

int resume_code(int status) {
	if(WIFSIGNALED(status)) {
		return WTERMSIG(status);
	} else if(WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}
	return 0;
}

#if defined(OpenBSD) && (OpenBSD > 201205)
//------------------------------------------------------------------------------
// Name: load_vmmap_entries
// Desc: Download vmmap_entries from the kernel into our address space.
//       We fix up the addr tree while downloading.
//       Returns the size of the tree on success, or -1 on failure.
//       On failure, *rptr needs to be passed to unload_vmmap_entries to free
//       the lot.
//------------------------------------------------------------------------------
ssize_t load_vmmap_entries(kvm_t *kd, u_long kptr, struct vm_map_entry **rptr, struct vm_map_entry *parent) {
	struct vm_map_entry *entry;
	u_long left_kptr;
	u_long right_kptr;
	ssize_t left_sz;
	ssize_t right_sz;

	if (kptr == 0)
		return 0;

	/* Need space. */
	entry = (struct vm_map_entry *)malloc(sizeof(*entry));
	if (entry == NULL)
		return -1;

	/* Download entry at kptr. */
	if (!kvm_read(kd, kptr, (char *)entry, sizeof(*entry))) {
		free(entry);
		return -1;
	}

	/*
	 * Update addr pointers to have sane values in this address space.
	 * We save the kernel pointers in {left,right}_kptr, so we have them
	 * available to download children.
	 */
	left_kptr = (u_long) RB_LEFT(entry, daddrs.addr_entry);
	right_kptr = (u_long) RB_RIGHT(entry, daddrs.addr_entry);
	RB_LEFT(entry, daddrs.addr_entry) = RB_RIGHT(entry, daddrs.addr_entry) = NULL;
	/* Fill in parent pointer. */
	RB_PARENT(entry, daddrs.addr_entry) = parent;

	/*
	 * Consistent state reached, fill in *rptr.
	 */
	*rptr = entry;

	/*
	 * Download left, right.
	 * On failure, our map is in a state that can be handled by
	 * unload_vmmap_entries.
	 */
	left_sz = load_vmmap_entries(kd, left_kptr, &RB_LEFT(entry, daddrs.addr_entry), entry);
	if (left_sz == -1)
		return -1;
	right_sz = load_vmmap_entries(kd, right_kptr, &RB_RIGHT(entry, daddrs.addr_entry), entry);
	if (right_sz == -1)
		return -1;

	return 1 + left_sz + right_sz;
}

//------------------------------------------------------------------------------
// Name:
// Desc: Free the vmmap entries in the given tree.
//------------------------------------------------------------------------------
void unload_vmmap_entries(struct vm_map_entry *entry) {
	if (entry == NULL)
		return;

	unload_vmmap_entries(RB_LEFT(entry, daddrs.addr_entry));
	unload_vmmap_entries(RB_RIGHT(entry, daddrs.addr_entry));
	free(entry);
}

//------------------------------------------------------------------------------
// Name:
// Desc: Don't implement address comparison.
//------------------------------------------------------------------------------
int no_impl(void *p, void *q) {
	Q_UNUSED(p);
	Q_UNUSED(q);
	Q_ASSERT(0); /* Should not be called. */
	return 0;
}
#endif
}

#if defined(OpenBSD) && (OpenBSD > 201205)
RB_GENERATE(uvm_map_addr, vm_map_entry, daddrs.addr_entry, no_impl)
#endif

//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() {
#if defined(_SC_PAGESIZE)
	page_size_ = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	page_size_ = sysconf(_SC_PAGE_SIZE);
#else
	page_size_ = PAGE_SIZE;
#endif
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
// Name: ~DebuggerCore
// Desc:
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
}

//------------------------------------------------------------------------------
// Name: wait_debug_event
// Desc: waits for a debug event, msecs is a timeout
//      it will return false if an error or timeout occurs
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

				char errbuf[_POSIX2_LINE_MAX];
				if(kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
					int rc;
					struct kinfo_proc *const kiproc = kvm_getprocs(kd, KERN_PROC_PID, pid(), sizeof(struct kinfo_proc), &rc);

					struct proc proc;
					kvm_read(kd, kiproc->p_paddr, &proc, sizeof(proc));

					e->fault_code_    = proc.p_sicode;
					e->fault_address_ = proc.p_sigval.sival_ptr;

					//printf("ps_sig   : %d\n", sigacts.ps_sig);
					//printf("ps_type  : %d\n", sigacts.ps_type);

					kvm_close(kd);
				} else {
					e->fault_code_    = 0;
					e->fault_address_ = 0;
				}


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

	errno = 0;
	const long v = ptrace(PT_READ_D, pid(), reinterpret_cast<char*>(address), 0);
	SET_OK(*ok, v);
	return v;
}

//------------------------------------------------------------------------------
// Name: write_data
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::write_data(edb::address_t address, long value) {
	return ptrace(PT_WRITE_D, pid(), reinterpret_cast<char*>(address), value) != -1;
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
		ptrace(PT_DETACH, pid(), 0, 0);
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
		if(ptrace(PT_GETREGS, active_thread(), reinterpret_cast<char*>(&state_impl->regs_), 0) != -1) {
			// TODO
			state_impl->gs_base = 0;
			state_impl->fs_base = 0;
		}

		if(ptrace(PT_GETFPREGS, active_thread(), reinterpret_cast<char*>(&state_impl->fpregs_), 0) != -1) {
		}

		// TODO: Debug Registers

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
		ptrace(PT_SETREGS, active_thread(), reinterpret_cast<char*>(&state_impl->regs_), 0);

		// TODO: FPU
		// TODO: Debug Registers
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

	char ebuffer[_POSIX2_LINE_MAX];
	int numprocs;

	if(kvm_t *const kaccess = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, ebuffer)) {
		if(struct kinfo_proc *const kprocaccess = kvm_getprocs(kaccess, KERN_PROC_ALL, 0, sizeof *kprocaccess, &numprocs)) {
			for(int i = 0; i < numprocs; ++i) {
				ProcessInfo procInfo;
				procInfo.pid  = kprocaccess[i].p_pid;
				procInfo.uid  = kprocaccess[i].p_uid;
				procInfo.name = kprocaccess[i].p_comm;

				ret.insert(procInfo.pid, procInfo);
			}
		}
		kvm_close(kaccess);
	} else {
		QMessageBox::warning(0, "Error Listing Processes", ebuffer);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_exe(edb::pid_t pid) const {
	QString ret;

	char errbuf[_POSIX2_LINE_MAX];
	if(kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
		char p_comm[KI_MAXCOMLEN] = "";
		int rc;
		if(struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), &rc)) {
			memcpy(p_comm, proc->p_comm, sizeof(p_comm));
		}

		kvm_close(kd);
		return p_comm;
	}

	return ret;
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
	edb::pid_t ret = 0;
	char errbuf[_POSIX2_LINE_MAX];
	if(kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
		int rc;
		struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof *proc, &rc);
		ret = proc->p_ppid;
		kvm_close(kd);
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<IRegion::pointer> DebuggerCore::memory_regions() const {

	QList<IRegion::pointer> regions;

	if(pid_ != 0) {

		char err_buf[_POSIX2_LINE_MAX];
		if(kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, err_buf)) {
			int rc;
			struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid_, sizeof *proc, &rc);
			Q_ASSERT(proc);

			struct vmspace vmsp;


			kvm_read(kd, proc->p_vmspace, &vmsp, sizeof vmsp);

#if defined(OpenBSD) && (OpenBSD > 201205)
			uvm_map_addr root;
			RB_INIT(&root);
			if (load_vmmap_entries(kd, (u_long)RB_ROOT(&vmsp.vm_map.addr), &RB_ROOT(&root), NULL) == -1)
				goto do_unload;

			struct vm_map_entry *e;
			RB_FOREACH(e, uvm_map_addr, &root) {
				const edb::address_t start               = e->start;
				const edb::address_t end                 = e->end;
				const edb::address_t base                = e->offset;
				const QString name                       = QString();
				const IRegion::permissions_t permissions =
					((e->protection & VM_PROT_READ)    ? PROT_READ  : 0) |
					((e->protection & VM_PROT_WRITE)   ? PROT_WRITE : 0) |
					((e->protection & VM_PROT_EXECUTE) ? PROT_EXEC  : 0);

				regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
			}

do_unload:
			unload_vmmap_entries(RB_ROOT(&root));
#else
			struct vm_map_entry e;
			if(vmsp.vm_map.header.next != 0) {
				kvm_read(kd, (u_long)vmsp.vm_map.header.next, &e, sizeof(e));
				while(e.next != vmsp.vm_map.header.next) {

					const edb::address_t start               = e.start;
					const edb::address_t end                 = e.end;
					const edb::address_t base                = e.offset;
					const QString name                       = QString();
					const IRegion::permissions_t permissions =
						((e.protection & VM_PROT_READ)    ? PROT_READ  : 0) |
						((e.protection & VM_PROT_WRITE)   ? PROT_WRITE : 0) |
						((e.protection & VM_PROT_EXECUTE) ? PROT_EXEC  : 0);

					regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
					kvm_read(kd, (u_long)e.next, &e, sizeof(e));
				}
			}
#endif
			kvm_close(kd);
		} else {
			fprintf(stderr, "sync: %s\n", err_buf);
			return QList<IRegion::pointer>();
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
		char errbuf[_POSIX2_LINE_MAX];
		if(kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
			int rc;
			if(struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, sizeof *proc, pid, &rc)) {
				char **argv = kvm_getargv(kd, proc, 0);
				char **p = argv;
				while(*p) {
					ret << *p++;
				}
			}
			kvm_close(kd);
		}

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
