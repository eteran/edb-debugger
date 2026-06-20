/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include <csignal>
#include <fcntl.h>
#include <kvm.h>
#include <machine/reg.h>
#include <paths.h>
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
#include <sys/proc.h>
#include <sys/sysctl.h>

namespace DebuggerCore {

namespace {

void SET_OK(bool &ok, long value) {
	ok = (value != -1) || (errno == 0);
}

int resume_code(int status) {
	if (WIFSIGNALED(status)) {
		return WTERMSIG(status);
	} else if (WIFSTOPPED(status)) {
		return WSTOPSIG(status);
	}
	return 0;
}

#if defined(OpenBSD) && (OpenBSD > 201205)
/**
 * @brief Loads virtual memory map entries from the kernel.
 *
 * Download vmmap_entries from the kernel into our address space.
 * We fix up the addr tree while downloading.
 * Returns the size of the tree on success, or -1 on failure.
 * On failure, *rptr needs to be passed to unload_vmmap_entries to free
 * the lot.
 *
 * @param kd The kvm_t pointer.
 * @param kptr The kernel pointer to the entry.
 * @param rptr The pointer to the entry.
 * @param parent The parent entry.
 * @return The size of the tree on success, or -1 on failure.
 */
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
	left_kptr                         = (u_long)RB_LEFT(entry, daddrs.addr_entry);
	right_kptr                        = (u_long)RB_RIGHT(entry, daddrs.addr_entry);
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

/**
 * @brief Frees the vmmap entries in the given tree.
 *
 * @param entry The root entry of the tree to free.
 */
void unload_vmmap_entries(struct vm_map_entry *entry) {
	if (entry == NULL)
		return;

	unload_vmmap_entries(RB_LEFT(entry, daddrs.addr_entry));
	unload_vmmap_entries(RB_RIGHT(entry, daddrs.addr_entry));
	free(entry);
}

/**
 * @brief A function that does nothing and should not be called. Passed as the address comparison function.
 *
 * @param p The first parameter.
 * @param q The second parameter.
 * @return 0.
 */
int no_impl(void *p, void *q) {
	Q_UNUSED(p)
	Q_UNUSED(q)
	Q_ASSERT(0); /* Should not be called. */
	return 0;
}
#endif
}

#if defined(OpenBSD) && (OpenBSD > 201205)
RB_GENERATE(uvm_map_addr, vm_map_entry, daddrs.addr_entry, no_impl)
#endif

/**
 * @brief Constructor for DebuggerCore.
 */
DebuggerCore::DebuggerCore() {
#if defined(_SC_PAGESIZE)
	page_size_ = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	page_size_ = sysconf(_SC_PAGE_SIZE);
#else
	page_size_ = PAGE_SIZE;
#endif
}

/**
 * @brief Checks if the debugger has a specific extension.
 *
 * @param ext The extension to check for.
 * @return true if the extension is available, false otherwise.
 */
bool DebuggerCore::has_extension(quint64 ext) const {
	return false;
}

/**
 * @brief Returns the size of a page on this system.
 *
 * @return The page size.
 */
size_t DebuggerCore::page_size() const {
	return page_size_;
}

/**
 * @brief Destructor for DebuggerCore.
 */
DebuggerCore::~DebuggerCore() {
	detach();
}

/**
 * @brief Waits for a debug event.
 *
 * @param msecs The timeout in milliseconds.
 * @return A shared pointer to the debug event, or nullptr if an error or timeout occurs.
 */
std::shared_ptr<const IDebugEvent> DebuggerCore::wait_debug_event(int msecs) {
	if (attached()) {
		int status;
		bool timeout;

		const edb::tid_t tid = native::waitpid_timeout(pid(), &status, 0, msecs, &timeout);
		if (!timeout) {
			if (tid > 0) {

				// normal event
				auto e = std::make_shared<PlatformEvent>();

				e->pid    = pid();
				e->tid    = tid;
				e->status = status;

				char errbuf[_POSIX2_LINE_MAX];
				if (kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
					int rc;
					struct kinfo_proc *const kiproc = kvm_getprocs(kd, KERN_PROC_PID, pid(), sizeof(struct kinfo_proc), &rc);

					struct proc proc;
					kvm_read(kd, kiproc->p_paddr, &proc, sizeof(proc));

					e->fault_code_    = proc.p_sicode;
					e->fault_address_ = proc.p_sigval.sival_ptr;

					// printf("ps_sig   : %d\n", sigacts.ps_sig);
					// printf("ps_type  : %d\n", sigacts.ps_type);

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

/**
 * @brief Reads data from the debugged process.
 *
 * @param address The address to read from.
 * @param ok A pointer to a boolean to indicate success or failure.
 * @return The read value.
 */
long DebuggerCore::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);

	errno        = 0;
	const long v = ptrace(PT_READ_D, pid(), reinterpret_cast<char *>(address), 0);
	SET_OK(*ok, v);
	return v;
}

/**
 * @brief Writes data to the debugged process.
 *
 * @param address The address to write to.
 * @param value The value to write.
 * @return true if the write was successful, false otherwise.
 */
bool DebuggerCore::write_data(edb::address_t address, long value) {
	return ptrace(PT_WRITE_D, pid(), reinterpret_cast<char *>(address), value) != -1;
}

/**
 * @brief Attaches the debugger to a process.
 *
 * @param pid The process ID to attach to.
 * @return true if the attachment was successful, false otherwise.
 */
bool DebuggerCore::attach(edb::pid_t pid) {
	detach();

	const long ret = ptrace(PT_ATTACH, pid, 0, 0);
	if (ret == 0) {
		pid_           = pid;
		active_thread_ = pid;
		threads_.clear();
		threads_.insert(pid, thread_info());

		// TODO: attach to all of the threads
	}

	return ret == 0;
}

/**
 * @brief Detaches the debugger from the debugged process.
 */
void DebuggerCore::detach() {
	if (attached()) {

		// TODO: do i need to stop each thread first, and wait for them?

		clear_breakpoints();
		ptrace(PT_DETACH, pid(), 0, 0);
		pid_ = 0;
		threads_.clear();
	}
}

/**
 * @brief Kills the debugged process.
 */
void DebuggerCore::kill() {
	if (attached()) {
		clear_breakpoints();
		ptrace(PT_KILL, pid(), 0, 0);
		native::waitpid(pid(), 0, WAIT_ANY);
		pid_ = 0;
		threads_.clear();
	}
}

/**
 * @brief Pauses the debugged process.
 */
void DebuggerCore::pause() {
	if (attached()) {
		for (auto it = threads_.begin(); it != threads_.end(); ++it) {
			::kill(it.key(), SIGSTOP);
		}
	}
}

/**
 * @brief Resumes the debugged process.
 *
 * @param status The event status.
 */
void DebuggerCore::resume(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused

	if (attached()) {
		if (status != edb::DEBUG_STOP) {
			const edb::tid_t tid = active_thread();
			const int code       = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(threads_[tid].status) : 0;
			ptrace(PT_CONTINUE, tid, reinterpret_cast<caddr_t>(1), code);
		}
	}
}

/**
 * @brief Steps the debugged process.
 *
 * @param status The event status.
 */
void DebuggerCore::step(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused

	if (attached()) {
		if (status != edb::DEBUG_STOP) {
			const edb::tid_t tid = active_thread();
			const int code       = (status == edb::DEBUG_EXCEPTION_NOT_HANDLED) ? resume_code(threads_[tid].status) : 0;
			ptrace(PT_STEP, tid, reinterpret_cast<caddr_t>(1), code);
		}
	}
}

/**
 * @brief Gets the state of the debugged process.
 *
 * @param state A pointer to the state object to populate.
 */
void DebuggerCore::get_state(State *state) {

	Q_ASSERT(state);

	// TODO: assert that we are paused
	auto state_impl = static_cast<PlatformState *>(state->impl_);

	if (attached()) {
		if (ptrace(PT_GETREGS, active_thread(), reinterpret_cast<char *>(&state_impl->regs_), 0) != -1) {
			// TODO
			state_impl->gs_base = 0;
			state_impl->fs_base = 0;
		}

		if (ptrace(PT_GETFPREGS, active_thread(), reinterpret_cast<char *>(&state_impl->fpregs_), 0) != -1) {
		}

		// TODO: Debug Registers

	} else {
		state->clear();
	}
}

/**
 * @brief Sets the state of the debugged process.
 *
 * @param state The state object to set.
 */
void DebuggerCore::set_state(const State &state) {

	// TODO: assert that we are paused
	auto state_impl = static_cast<PlatformState *>(state.impl_);

	if (attached()) {
		ptrace(PT_SETREGS, active_thread(), reinterpret_cast<char *>(&state_impl->regs_), 0);

		// TODO: FPU
		// TODO: Debug Registers
	}
}

/**
 * @brief Opens a process for debugging.
 *
 * @param path The path to the executable to debug.
 * @param cwd The working directory for the debugged process.
 * @param args The arguments to pass to the debugged process.
 * @param tty The TTY to use for the debugged process's I/O.
 * @return true if the process was successfully opened for debugging, false otherwise.
 */
bool DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {
	detach();
	pid_t pid;

	switch (pid = fork()) {
	case 0:
		// we are in the child now...

		// set ourselves (the child proc) up to be traced
		ptrace(PT_TRACE_ME, 0, 0, 0);

		// redirect it's I/O
		if (!tty.isEmpty()) {
			FILE *const std_out = freopen(qPrintable(tty), "r+b", stdout);
			FILE *const std_in  = freopen(qPrintable(tty), "r+b", stdin);
			FILE *const std_err = freopen(qPrintable(tty), "r+b", stderr);

			Q_UNUSED(std_out)
			Q_UNUSED(std_in)
			Q_UNUSED(std_err)
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
			if (native::waitpid(pid, &status, 0) == -1) {
				return false;
			}

			// the very first event should be a STOP of type SIGTRAP
			if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
				detach();
				return false;
			}

			// setup the first event data for the primary thread
			threads_.insert(pid, thread_info());
			pid_                 = pid;
			active_thread_       = pid;
			threads_[pid].status = status;
			return true;
		} while (0);
		break;
	}
}

/**
 * @brief Sets the active thread for the debugger.
 *
 * @param tid The thread ID to set as active.
 */
void DebuggerCore::set_active_thread(edb::tid_t tid) {
	Q_ASSERT(threads_.contains(tid));
	active_thread_ = tid;
}

/**
 * @brief Creates a new state object for the debugger.
 *
 * @return A unique pointer to the newly created state object.
 */
std::unique_ptr<IState> DebuggerCore::create_state() const {
	return std::make_unique<PlatformState>();
}

/**
 * @brief Enumerates the processes currently running on the system.
 *
 * @return A map of process IDs to ProcessInfo objects for each running process.
 */
QMap<edb::pid_t, ProcessInfo> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, ProcessInfo> ret;

	char ebuffer[_POSIX2_LINE_MAX];
	int numprocs;

	if (kvm_t *const kaccess = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, ebuffer)) {
		if (struct kinfo_proc *const kprocaccess = kvm_getprocs(kaccess, KERN_PROC_ALL, 0, sizeof *kprocaccess, &numprocs)) {
			for (int i = 0; i < numprocs; ++i) {
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

/**
 * @brief Returns the executable path for a given process ID.
 *
 * @param pid The process ID.
 * @return The executable path for the process.
 */
QString DebuggerCore::process_exe(edb::pid_t pid) const {
	QString ret;

	char errbuf[_POSIX2_LINE_MAX];
	if (kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
		char p_comm[KI_MAXCOMLEN] = "";
		int rc;
		if (struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof(struct kinfo_proc), &rc)) {
			memcpy(p_comm, proc->p_comm, sizeof(p_comm));
		}

		kvm_close(kd);
		return p_comm;
	}

	return ret;
}

/**
 * @brief Returns the current working directory for a given process ID.
 *
 * @param pid The process ID.
 * @return The current working directory for the process, or an empty string if not implemented.
 */
QString DebuggerCore::process_cwd(edb::pid_t pid) const {
	// TODO: implement this
	return QString();
}

/**
 * @brief Returns the parent process ID for a given process ID.
 *
 * @param pid The process ID.
 * @return The parent process ID, or 0 if the process has no parent or an
 */
edb::pid_t DebuggerCore::parent_pid(edb::pid_t pid) const {
	edb::pid_t ret = 0;
	char errbuf[_POSIX2_LINE_MAX];
	if (kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
		int rc;
		struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid, sizeof *proc, &rc);
		ret                           = proc->p_ppid;
		kvm_close(kd);
	}
	return ret;
}

/**
 * @brief Returns the memory regions for the debugged process.
 *
 * @return A list of memory regions for the debugged process.
 */
QList<std::shared_ptr<IRegion>> DebuggerCore::memory_regions() const {

	QList<std::shared_ptr<IRegion>> regions;

	if (pid_ != 0) {

		char err_buf[_POSIX2_LINE_MAX];
		if (kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, err_buf)) {
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
				const edb::address_t start = e->start;
				const edb::address_t end   = e->end;
				const edb::address_t base  = e->offset;
				const QString name         = QString();
				const IRegion::permissions_t permissions =
					((e->protection & VM_PROT_READ) ? PROT_READ : 0) |
					((e->protection & VM_PROT_WRITE) ? PROT_WRITE : 0) |
					((e->protection & VM_PROT_EXECUTE) ? PROT_EXEC : 0);

				regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
			}

		do_unload:
			unload_vmmap_entries(RB_ROOT(&root));
#else
			struct vm_map_entry e;
			if (vmsp.vm_map.header.next != 0) {
				kvm_read(kd, (u_long)vmsp.vm_map.header.next, &e, sizeof(e));
				while (e.next != vmsp.vm_map.header.next) {

					const edb::address_t start = e.start;
					const edb::address_t end   = e.end;
					const edb::address_t base  = e.offset;
					const QString name         = QString();
					const IRegion::permissions_t permissions =
						((e.protection & VM_PROT_READ) ? PROT_READ : 0) |
						((e.protection & VM_PROT_WRITE) ? PROT_WRITE : 0) |
						((e.protection & VM_PROT_EXECUTE) ? PROT_EXEC : 0);

					regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
					kvm_read(kd, (u_long)e.next, &e, sizeof(e));
				}
			}
#endif
			kvm_close(kd);
		} else {
			fprintf(stderr, "sync: %s\n", err_buf);
			return QList<std::shared_ptr<IRegion>>();
		}
	}

	return regions;
}

/**
 * @brief Returns the command line arguments for a given process ID.
 *
 * @param pid The process ID.
 * @return A list of command line arguments for the process.
 */
QList<QByteArray> DebuggerCore::process_args(edb::pid_t pid) const {
	QList<QByteArray> ret;
	if (pid != 0) {

		// TODO: assert attached!
		char errbuf[_POSIX2_LINE_MAX];
		if (kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
			int rc;
			if (struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, sizeof *proc, pid, &rc)) {
				char **argv = kvm_getargv(kd, proc, 0);
				char **p    = argv;
				while (*p) {
					ret << *p++;
				}
			}
			kvm_close(kd);
		}
	}
	return ret;
}

/**
 * @brief Returns the address of the process code.
 *
 * @return The address of the process code, or 0 if not implemented.
 */
edb::address_t DebuggerCore::process_code_address() const {
	qDebug() << "TODO: implement DebuggerCore::process_code_address";
	return 0;
}

/**
 * @brief Returns the address of the process data.
 *
 * @return The address of the process data, or 0 if not implemented.
 */
edb::address_t DebuggerCore::process_data_address() const {
	qDebug() << "TODO: implement DebuggerCore::process_data_address";
	return 0;
}

/**
 * @brief Returns a list of loaded modules for the debugged process.
 *
 * @return A list of loaded modules for the debugged process.
 */
QList<Module> DebuggerCore::loaded_modules() const {
	QList<Module> modules;
	qDebug() << "TODO: implement DebuggerCore::loaded_modules";
	return modules;
}

/**
 * @brief Returns the start time of the specified process.
 *
 * @param pid The process ID.
 * @return The start time of the process.
 */
QDateTime DebuggerCore::process_start(edb::pid_t pid) const {
	qDebug() << "TODO: implement DebuggerCore::process_start";
	return QDateTime();
}

/**
 * @brief Returns the CPU type of the debugged process.
 *
 * @return The CPU type of the debugged process.
 */
quint64 DebuggerCore::cpu_type() const {
#ifdef EDB_X86
	return edb::string_hash<'x', '8', '6'>::value;
#elif defined(EDB_X86_64)
	return edb::string_hash<'x', '8', '6', '-', '6', '4'>::value;
#endif
}

/**
 * @brief Formats a pointer address for display.
 */
QString DebuggerCore::format_pointer(edb::address_t address) const {
	char buf[32];
#ifdef EDB_X86
	qsnprintf(buf, sizeof(buf), "%08x", address);
#elif defined(EDB_X86_64)
	qsnprintf(buf, sizeof(buf), "%016llx", address);
#endif
	return buf;
}

/**
 * @brief Returns the name of the stack pointer register for the target architecture.
 *
 * @return The name of the stack pointer register.
 */
QString DebuggerCore::stack_pointer() const {
#ifdef EDB_X86
	return "esp";
#elif defined(EDB_X86_64)
	return "rsp";
#endif
}

/**
 * @brief Returns the name of the frame pointer register for the target architecture.
 *
 * @return The name of the frame pointer register.
 */
QString DebuggerCore::frame_pointer() const {
#ifdef EDB_X86
	return "ebp";
#elif defined(EDB_X86_64)
	return "rbp";
#endif
}

/**
 * @brief Returns the name of the instruction pointer register for the target architecture.
 *
 * @return The name of the instruction pointer register.
 */
QString DebuggerCore::instruction_pointer() const {
#ifdef EDB_X86
	return "eip";
#elif defined(EDB_X86_64)
	return "rip";
#endif
}

}
