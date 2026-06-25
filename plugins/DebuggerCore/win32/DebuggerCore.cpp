/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DebuggerCore.h"
#include "edb.h"

#include "MemoryRegions.h"
#include "PlatformEvent.h"
#include "PlatformProcess.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "PlatformThread.h"
#include "State.h"
#include "string_hash.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QStringList>

#include <Psapi.h>
#include <TlHelp32.h>
#include <Windows.h>

#include <algorithm>

#ifdef _MSC_VER
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Psapi.lib")
#endif

/* NOTE(eteran): from the MSDN:
 * Note that while reporting debug events, all threads within the reporting
 * process are frozen. Debuggers are expected to use the SuspendThread and
 * ResumeThread functions to limit the set of threads that can execute within a
 * process. By suspending all threads in a process except for the one reporting
 * a debug event, it is possible to "single step" a single thread. The other
 * threads are not released by a continue operation if they are suspended.
 */

namespace DebuggerCorePlugin {

namespace {

/**
 * @brief Enables or disables the debug privilege for the specified process.
 * This function adjusts the privileges of the specified process to enable or disable the debug privilege (SE_DEBUG_NAME).
 * This privilege allows the process to debug and adjust the memory of other processes, including those owned by other accounts.
 * Required to debug and adjust the memory of a process owned by another account.
 * OpenProcess quote (MSDN):
 *   "If the caller has enabled the SeDebugPrivilege privilege, the requested access
 *    is granted regardless of the contents of the security descriptor."
 * Needed to open system processes (user SYSTEM)
 *
 * @param process The handle to the process for which to set the debug privilege.
 * @param set True to enable the debug privilege, false to disable it.
 * @return True if the privilege was successfully set, false otherwise.
 *
 * @note You need to be admin to enable this privilege
 * @note Detectable by antidebug code (changes debuggee privileges too)
 * @note You need to have the 'Debug programs' privilege set for the current user, if the privilege is not present it can't be enabled!
 */
bool set_debug_privilege(HANDLE process, bool set) {

	HANDLE token;
	bool ok = false;

	// process must have PROCESS_QUERY_INFORMATION
	if (OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES, &token)) {

		LUID luid;
		if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
			TOKEN_PRIVILEGES tp;
			tp.PrivilegeCount           = 1;
			tp.Privileges[0].Luid       = luid;
			tp.Privileges[0].Attributes = set ? SE_PRIVILEGE_ENABLED : 0;

			ok = AdjustTokenPrivileges(token, false, &tp, NULL, nullptr, nullptr);
		}
		CloseHandle(token);
	}

	return ok;
}

}

/**
 * @brief Creates a new instance of the DebuggerCore class.
 */
DebuggerCore::DebuggerCore() {
	DebugSetProcessKillOnExit(false);

	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	pageSize_ = sys_info.dwPageSize;

	set_debug_privilege(GetCurrentProcess(), true); // gogo magic powers
}

/**
 * @brief Destroys the DebuggerCore instance and detaches from any attached process.
 */
DebuggerCore::~DebuggerCore() {
	detach();
	set_debug_privilege(GetCurrentProcess(), false);
}

/**
 * @brief Gets the size of a memory page on the current system.
 *
 * @return the size of a page on this system
 */
size_t DebuggerCore::pageSize() const {
	return pageSize_;
}

/**
 * @brief Determines if the debuggee processor has the specified extension or feature.
 *
 * @param ext The extension or feature to check for.
 * @return True if the extension is supported, false otherwise.
 */
bool DebuggerCore::hasExtension(uint64_t ext) const {
#if !defined(EDB_X86_64)
	switch (ext) {
	case edb::string_hash("MMX"):
		return IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE);
	case edb::string_hash("XMM"):
		return IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE);
	default:
		return false;
	}
#else
	switch (ext) {
	case edb::string_hash("MMX"):
	case edb::string_hash("XMM"):
		return true;
	default:
		return false;
	}
#endif
}

/**
 * @brief Waits for a debug event, msecs is a timeout (but is not yet respected)
 *
 * @param msecs The timeout in milliseconds to wait for a debug event. If zero, waits indefinitely.
 * @return A shared pointer to the IDebugEvent representing the debug event that occurred,
 * or nullptr if the wait timed out or if no debug event occurred within the specified time.
 */
std::shared_ptr<IDebugEvent> DebuggerCore::waitDebugEvent(std::chrono::milliseconds msecs) {
	if (attached()) {
		DEBUG_EVENT de;
		while (WaitForDebugEvent(&de, msecs.count() == 0 ? INFINITE : msecs.count())) {

			Q_ASSERT(process_->pid() == de.dwProcessId);

			activeThread_  = de.dwThreadId;
			bool propagate = false;

			switch (de.dwDebugEventCode) {
			case CREATE_THREAD_DEBUG_EVENT: {
				auto newThread = std::make_shared<PlatformThread>(this, process_, &de.u.CreateThread);
				threads_.insert(activeThread_, newThread);
				break;
			}
			case EXIT_THREAD_DEBUG_EVENT:
				threads_.remove(activeThread_);
				break;
			case CREATE_PROCESS_DEBUG_EVENT: {
				CloseHandle(de.u.CreateProcessInfo.hFile);

				process_ = std::make_shared<PlatformProcess>(this, de.u.CreateProcessInfo.hProcess);

				// fake a thread create event for the main thread..
				CREATE_THREAD_DEBUG_INFO thread_info;
				thread_info.hThread           = de.u.CreateProcessInfo.hThread;
				thread_info.lpStartAddress    = de.u.CreateProcessInfo.lpStartAddress;
				thread_info.lpThreadLocalBase = de.u.CreateProcessInfo.lpThreadLocalBase;
				auto newThread                = std::make_shared<PlatformThread>(this, process_, &thread_info);
				threads_.insert(activeThread_, newThread);
				break;
			}
			case LOAD_DLL_DEBUG_EVENT:
				CloseHandle(de.u.LoadDll.hFile);
				break;
			case EXIT_PROCESS_DEBUG_EVENT:
				process_->resume(edb::DEBUG_CONTINUE);
				process_ = nullptr;
				// handle_event_exited returns DEBUG_STOP, which in turn keeps the debugger from resuming the process
				// However, this is needed to close all internal handles etc. and finish the debugging session
				// So we do it manually here
				propagate = true;
				break;
			case EXCEPTION_DEBUG_EVENT:
				propagate = true;
				break;
			case RIP_EVENT:
				break;
			default:
				break;
			}

			if (auto p = static_cast<PlatformProcess *>(process_.get())) {
				p->lastEvent_ = de;
			}

			if (propagate) {
				// normal event
				auto e    = std::make_shared<PlatformEvent>();
				e->event_ = de;
				return e;
			}

			process_->resume(edb::DEBUG_EXCEPTION_NOT_HANDLED);
		}
	}
	return nullptr;
}

/**
 * @brief Attachs the debugger to a process with the specified process ID (pid).
 *
 * @param pid The process ID of the target process to attach to.
 * @return A Status object indicating the success or failure of the attach operation.
 */
Status DebuggerCore::attach(edb::pid_t pid) {

	detach();

	if (DebugActiveProcess(pid)) {
		process_ = std::make_shared<PlatformProcess>(this, pid);
		return Status::Ok;
	}

	return Status("Error DebuggerCore::attach");
}

/**
 * @brief Detaches the debugger from the currently attached process, if any.
 *
 * @return A Status object indicating the success or failure of the detach operation.
 */
Status DebuggerCore::detach() {
	if (attached()) {
		clearBreakpoints();
		// Make sure exceptions etc. are passed
		ContinueDebugEvent(process_->pid(), active_thread(), DBG_CONTINUE);
		DebugActiveProcessStop(process_->pid());
		process_ = nullptr;
		threads_.clear();
	}
	return Status::Ok;
}

/**
 * @brief Kills the currently attached process, if any, and detaches the debugger from it.
 */
void DebuggerCore::kill() {
	if (auto p = static_cast<PlatformProcess *>(process_.get())) {
		TerminateProcess(p->hProcess_, -1);
		detach();
	}
}

/**
 * @brief Opens a new process for debugging with the specified executable path, working directory, command-line arguments, and input/output settings.
 *
 * @param path The path to the executable to debug.
 * @param cwd The working directory for the new process.
 * @param args The command-line arguments for the new process.
 * @param tty The terminal device to use for input/output.
 * @return A Status object indicating the success or failure of the open operation.
 */
Status DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &input, const QString &output) {

	// TODO: Don't inherit security descriptors from this process (default values)
	//       Is this even possible?

	Q_UNUSED(input)
	Q_UNUSED(output)

	Q_ASSERT(!path.isEmpty());

	bool ok = false;

	detach();

	// default to process's directory
	QString tcwd = cwd.isEmpty() ? QFileInfo(path).canonicalPath() : cwd;

	STARTUPINFO startup_info         = {};
	PROCESS_INFORMATION process_info = {};

	const DWORD CREATE_FLAGS = DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;

	wchar_t *const env_block = GetEnvironmentStringsW();

	// Set up command line
	QString command_str = '\"' + QFileInfo(path).canonicalPath() + '\"'; // argv[0] = full path (explorer style)
	if (!args.isEmpty()) {
		for (QByteArray arg : args) {
			command_str += " ";
			command_str += arg;
		}
	}

	// CreateProcessW wants a writable copy of the command line :<
	auto command_path = new wchar_t[command_str.length() + sizeof(wchar_t)];
	wcscpy_s(command_path, command_str.length() + 1, reinterpret_cast<const wchar_t *>(command_str.utf16()));

	if (CreateProcessW(
			reinterpret_cast<const wchar_t *>(path.utf16()), // exe
			command_path,                                    // commandline
			nullptr,                                         // default security attributes
			nullptr,                                         // default thread security too
			FALSE,                                           // inherit handles
			CREATE_FLAGS,
			env_block,                                       // environment data
			reinterpret_cast<const wchar_t *>(tcwd.utf16()), // working directory
			&startup_info,
			&process_info)) {

		activeThread_ = process_info.dwThreadId;
		CloseHandle(process_info.hThread); // We don't need the thread handle
		set_debug_privilege(process_info.hProcess, false);

		// process_info.hProcess  has PROCESS_ALL_ACCESS
		process_ = std::make_shared<PlatformProcess>(this, process_info.hProcess);

		ok = true;
	}

	delete[] command_path;
	FreeEnvironmentStringsW(env_block);

	if (ok) {
		return Status::Ok;
	} else {
		return Status("Error DebuggerCore::open");
	}
}

/**
 * @brief Creates a new instance of the PlatformState class, which represents the state of the debugged process.
 *
 * @return A unique pointer to the newly created PlatformState instance.
 */
std::unique_ptr<IState> DebuggerCore::createState() const {
	return std::make_unique<PlatformState>();
}

/**
 * @brief Gets the size of a pointer on the current architecture.
 *
 * @return the size of a pointer on this arch
 */
int DebuggerCore::sys_pointer_size() const {
	return sizeof(void *);
}

/**
 * @brief Enumerates all processes currently running on the system and returns a map of process IDs to shared pointers of IProcess instances.
 *
 * @return A map of process IDs to shared pointers of IProcess instances.
 */
QMap<edb::pid_t, std::shared_ptr<IProcess>> DebuggerCore::enumerateProcesses() const {
	QMap<edb::pid_t, std::shared_ptr<IProcess>> ret;

	HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (handle != INVALID_HANDLE_VALUE) {

		PROCESSENTRY32 lppe;

		std::memset(&lppe, 0, sizeof(lppe));
		lppe.dwSize = sizeof(lppe);

		if (Process32First(handle, &lppe)) {
			do {
				// NOTE(eteran): the const_cast is reasonable here.
				// While we don't want THIS function to mutate the DebuggerCore object
				// we do want the associated PlatformProcess to be able to trigger
				// non-const operations in the future, at least hypothetically.
				auto pi = std::make_shared<PlatformProcess>(const_cast<DebuggerCore *>(this), lppe.th32ProcessID);
				if (pi->hProcess_ == nullptr) {
					continue;
				}

				ret.insert(pi->pid(), pi);

				std::memset(&lppe, 0, sizeof(lppe));
				lppe.dwSize = sizeof(lppe);
			} while (Process32Next(handle, &lppe));
		}

		CloseHandle(handle);
	}
	return ret;
}

/**
 * @brief Gets the parent process ID of the specified process.
 *
 * @param pid The process ID of the target process.
 * @return The parent process ID of the specified process, or 1 if the parent process could not be determined.
 */
edb::pid_t DebuggerCore::parentPid(edb::pid_t pid) const {
	edb::pid_t parent   = 1; // 1??
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);
	if (hProcessSnap != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(pe32);

		if (Process32FirstW(hProcessSnap, &pe32)) {
			do {
				if (pid == pe32.th32ProcessID) {
					parent = pe32.th32ParentProcessID;
					break;
				}
			} while (Process32NextW(hProcessSnap, &pe32));
		}
		CloseHandle(hProcessSnap);
	}
	return parent;
}

/**
 * @brief Gets a map of exception values to their corresponding exception names.
 *
 * @return A map of exception values to their corresponding exception names.
 */
QMap<qlonglong, QString> DebuggerCore::exceptions() const {
	QMap<qlonglong, QString> exceptions;

	return exceptions;
}

/**
 * @brief Gets the CPU type of the debugged process as a string hash.
 *
 * @return The CPU type of the debugged process as a string hash.
 */
uint64_t DebuggerCore::cpuType() const {
#ifdef EDB_X86
	return edb::string_hash("x86");
#elif defined(EDB_X86_64)
	return edb::string_hash("x86-64");
#endif
}

/**
 * @brief Gets the stack pointer register name for the current architecture.
 *
 * @return The stack pointer register name.
 */
QString DebuggerCore::stackPointer() const {
#ifdef EDB_X86
	return "esp";
#elif defined(EDB_X86_64)
	// TODO(eteran): WOW64 support
	return "rsp";
#endif
}

/**
 * @brief Gets the frame pointer register name for the current architecture.
 *
 * @return The frame pointer register name.
 */
QString DebuggerCore::framePointer() const {
#ifdef EDB_X86
	return "ebp";
#elif defined(EDB_X86_64)
	// TODO(eteran): WOW64 support
	return "rbp";
#endif
}

/**
 * @brief Gets the instruction pointer register name for the current architecture.
 *
 * @return The instruction pointer register name.
 */
QString DebuggerCore::instructionPointer() const {
#ifdef EDB_X86
	return "eip";
#elif defined(EDB_X86_64)
	// TODO(eteran): WOW64 support
	return "rip";
#endif
}

/**
 * @brief Gets the process instance being debugged.
 *
 * @return A pointer to the IProcess instance.
 */
IProcess *DebuggerCore::process() const {
	return process_.get();
}

/**
 * @brief Gets the byte value used to fill unused space in the debugger.
 *
 * @return The byte value used to fill unused space (0x90 for x86/x86-64).
 */
uint8_t DebuggerCore::nopFillByte() const {
	return 0x90;
}

}
