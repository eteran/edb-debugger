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
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

namespace DebuggerCorePlugin {

namespace {

constexpr uint64_t PageSize = 0x1000;

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
}

/**
 * @brief Constructor for the DebuggerCore class.
 */
DebuggerCore::DebuggerCore() {
#if defined(_SC_PAGESIZE)
	page_size_ = sysconf(_SC_PAGESIZE);
#elif defined(_SC_PAGE_SIZE)
	page_size_ = sysconf(_SC_PAGE_SIZE);
#else
	page_size_ = PageSize;
#endif
}

/**
 * @brief Checks if the debugger has a specific extension.
 *
 * @param ext The extension to check for.
 * @return True if the extension is available, false otherwise.
 */
bool DebuggerCore::has_extension(quint64 ext) const {
	Q_UNUSED(ext)
	return false;
}

/**
 * @brief Returns the size of a page on this system.
 *
 * @return The size of a page.
 */
size_t DebuggerCore::page_size() const {
	return page_size_;
}

/**
 * @brief Destructor for the DebuggerCore class.
 */
DebuggerCore::~DebuggerCore() {
	detach();
}

/**
 * @brief Waits for a debug event.
 *
 * @param msecs The timeout in milliseconds.
 * @return A shared pointer to the debug event or nullptr if an error or timeout occurs.
 */
std::shared_ptr<const IDebugEvent> DebuggerCore::wait_debug_event(int msecs) {
	if (attached()) {
		int status;
		bool timeout;

		const edb::tid_t tid = Posix::waitpid_timeout(pid(), &status, 0, msecs, &timeout);
		if (!timeout) {
			if (tid > 0) {

				// normal event
				auto e    = std::make_shared<PlatformEvent>();
				e->pid    = pid();
				e->tid    = tid;
				e->status = status;

				char errbuf[_POSIX2_LINE_MAX];
				if (kvm_t *const kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf)) {
					int rc;
					struct kinfo_proc *const proc = kvm_getprocs(kd, KERN_PROC_PID, pid(), &rc);

					struct proc p;
					kvm_read(kd, (unsigned long)proc->ki_paddr, &p, sizeof(p));

					struct ksiginfo siginfo;
					kvm_read(kd, (unsigned long)p.p_ksi, &siginfo, sizeof(siginfo));

					// TODO: why doesn't this get the fault address correctly?
					// perhaps I need to target the tid instead?
					e->fault_code_    = siginfo.ksi_code;
					e->fault_address_ = siginfo.ksi_addr;

					// printf("ps_sig   : %d\n", siginfo.ksi_signo);
					// printf("ps_type  : %d\n", p.p_stype);
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
 * @brief Reads data from the process.
 *
 * @param address The address to read from.
 * @param ok A pointer to a boolean to indicate if the operation was successful.
 * @return The value read from the process.
 */
long DebuggerCore::read_data(edb::address_t address, bool *ok) {

	Q_ASSERT(ok);
	errno        = 0;
	const long v = ptrace(PT_READ_D, pid(), reinterpret_cast<char *>(address), 0);
	SET_OK(*ok, v);
	return v;
}

/**
 * @brief Writes data to the process.
 *
 * @param address The address to write to.
 * @param value The value to write.
 * @return True if the operation was successful, false otherwise.
 */
bool DebuggerCore::write_data(edb::address_t address, long value) {
	return ptrace(PT_WRITE_D, pid(), reinterpret_cast<char *>(address), value) != -1;
}

/**
 * @brief Attaches to a process.
 *
 * @param pid The PID of the process to attach to.
 * @return True if the operation was successful, false otherwise.
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
 * @brief Detaches from the current process.
 */
void DebuggerCore::detach() {
	if (attached()) {

		// TODO: do i need to stop each thread first, and wait for them?

		clear_breakpoints();
		for (auto it = threads_.begin(); it != threads_.end(); ++it) {
			ptrace(PT_DETACH, it.key(), 0, 0);
		}

		pid_ = 0;
		threads_.clear();
	}
}

/**
 * @brief Kills the current process.
 */
void DebuggerCore::kill() {
	if (attached()) {
		clear_breakpoints();
		ptrace(PT_KILL, pid(), 0, 0);
		Posix::waitpid(pid(), 0, WAIT_ANY);
		pid_ = 0;
		threads_.clear();
	}
}

/**
 * @brief Pauses the current process.
 */
void DebuggerCore::pause() {
	if (attached()) {
		for (auto it = threads_.begin(); it != threads_.end(); ++it) {
			::kill(it.key(), SIGSTOP);
		}
	}
}

/**
 * @brief Resumes the current process.
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
 * @brief Opens a process for debugging.
 *
 * @param path The path to the executable.
 * @param cwd The current working directory.
 * @param args The command line arguments.
 * @param tty The terminal device to redirect I/O to.
 * @return True if the operation was successful, false otherwise.
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
			if (Posix::waitpid(pid, &status, 0) == -1) {
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
 * @brief Sets the active thread.
 *
 * @param tid The ID of the thread to set as active.
 */
void DebuggerCore::set_active_thread(edb::tid_t tid) {
	Q_ASSERT(threads_.contains(tid));
	active_thread_ = tid;
}

/**
 * @brief Creates a new state for the debugger.
 *
 * @return A unique pointer to the new state.
 */
std::unique_ptr<IState> DebuggerCore::create_state() const {
	return std::make_unique<PlatformState>();
}

/**
 * @brief Enumerates the processes running on the system.
 *
 * @return A map of process IDs to their information.
 */
QMap<edb::pid_t, ProcessInfo> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, ProcessInfo> ret;

	char ebuffer[_POSIX2_LINE_MAX];
	int numprocs;
	if (kvm_t *const kaccess = kvm_openfiles(_PATH_DEVNULL, _PATH_DEVNULL, 0, O_RDONLY, ebuffer)) {
		if (struct kinfo_proc *const kprocaccess = kvm_getprocs(kaccess, KERN_PROC_ALL, 0, &numprocs)) {
			for (int i = 0; i < numprocs; ++i) {
				ProcessInfo procInfo;

				procInfo.pid  = kprocaccess[i].ki_pid;
				procInfo.uid  = kprocaccess[i].ki_uid;
				procInfo.name = kprocaccess[i].ki_comm;
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
 * @brief Gets the parent process ID of a given process.
 *
 * @param pid The ID of the process.
 * @return The ID of the parent process, or -1 if not found.
 */
edb::pid_t DebuggerCore::parent_pid(edb::pid_t pid) const {
	// TODO: implement this
	return -1;
}

/**
 * @brief Gets the type of the CPU.
 *
 * @return The type of the CPU.
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
 *
 * @param address The address to format.
 * @return The formatted address as a string.
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
 * @brief Gets the name of the stack pointer register.
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
 * @brief Gets the name of the frame pointer register.
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
 * @brief Gets the name of the instruction pointer register.
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
