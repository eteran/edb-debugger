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
#include "edb.h"

#include "MemoryRegions.h"
#include "PlatformEvent.h"
#include "PlatformProcess.h"
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "State.h"
#include "string_hash.h"

#include <QDateTime>
#include <QDebug>
#include <QStringList>
#include <QFileInfo>

#include <Windows.h>
#include <TlHelp32.h>
#include <Psapi.h>

#include <algorithm>

#ifdef _MSC_VER
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Psapi.lib")
#endif

namespace DebuggerCorePlugin {

namespace {

	class Win32Thread {
	public:
		Win32Thread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId) {
			handle_ = OpenThread(dwDesiredAccess, bInheritHandle, dwThreadId);
		}

		~Win32Thread() {
			if(handle_) {
				CloseHandle(handle_);
			}
		}

	public:
		BOOL GetThreadContext(LPCONTEXT lpContext) {
			return ::GetThreadContext(handle_, lpContext);
		}

		BOOL SetThreadContext(const CONTEXT *lpContext) {
			return ::SetThreadContext(handle_, lpContext);
		}

		BOOL GetThreadSelectorEntry(DWORD dwSelector, LPLDT_ENTRY lpSelectorEntry) {
			return ::GetThreadSelectorEntry(handle_, dwSelector, lpSelectorEntry);
		}

	public:
		operator void*() const {
			return reinterpret_cast<void *>(handle_ != nullptr);
		}

	private:
		HANDLE handle_;
	};

    /*
     * Required to debug and adjust the memory of a process owned by another account.
     * OpenProcess quote (MSDN):
     *   "If the caller has enabled the SeDebugPrivilege privilege, the requested access
     *    is granted regardless of the contents of the security descriptor."
     * Needed to open system processes (user SYSTEM)
     *
     * NOTE: You need to be admin to enable this privilege
     * NOTE: You need to have the 'Debug programs' privilege set for the current user,
     *       if the privilege is not present it can't be enabled!
     * NOTE: Detectable by antidebug code (changes debuggee privileges too)
     */
    bool set_debug_privilege(HANDLE process, bool set) {

        HANDLE token;
        bool ok = false;

        //process must have PROCESS_QUERY_INFORMATION
        if(OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES, &token)) {

            LUID luid;
			if(LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
                TOKEN_PRIVILEGES tp;
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Luid = luid;
                tp.Privileges[0].Attributes = set ? SE_PRIVILEGE_ENABLED : 0;

				ok = AdjustTokenPrivileges(token, false, &tp, NULL, nullptr, nullptr);
            }
            CloseHandle(token);
        }

        return ok;
    }
}


//------------------------------------------------------------------------------
// Name: DebuggerCore
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() {
	DebugSetProcessKillOnExit(false);

	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	page_size_ = sys_info.dwPageSize;

	set_debug_privilege(GetCurrentProcess(), true); // gogo magic powers
}

//------------------------------------------------------------------------------
// Name: ~DebuggerCore
// Desc:
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
	set_debug_privilege(GetCurrentProcess(), false);
}

//------------------------------------------------------------------------------
// Name: page_size
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
size_t DebuggerCore::page_size() const {
	return page_size_;
}

//------------------------------------------------------------------------------
// Name: has_extension
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::has_extension(quint64 ext) const {
#if !defined(EDB_X86_64)
	switch(ext) {
	case edb::string_hash("MMX"):
		return IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE);
	case edb::string_hash("XMM"):
		return IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE);
	default:
		return false;
	}
#else
	switch(ext) {
	case edb::string_hash("MMX"):
	case edb::string_hash("XMM"):
		return true;
	default:
		return false;
	}
#endif
}

//------------------------------------------------------------------------------
// Name: wait_debug_event
// Desc: waits for a debug event, secs is a timeout (but is not yet respected)
//       ok will be set to false if the timeout expires
//------------------------------------------------------------------------------
std::shared_ptr<IDebugEvent> DebuggerCore::wait_debug_event(int msecs) {
	if(attached()) {
		DEBUG_EVENT de;
		while(WaitForDebugEvent(&de, msecs == 0 ? INFINITE : static_cast<DWORD>(msecs))) {

			Q_ASSERT(process_->pid() == de.dwProcessId);

			active_thread_ = de.dwThreadId;
			bool propagate = false;

			switch(de.dwDebugEventCode) {
			case CREATE_THREAD_DEBUG_EVENT:
				threads_.insert(active_thread_);
				break;
			case EXIT_THREAD_DEBUG_EVENT:
				threads_.remove(active_thread_);
				break;
			case CREATE_PROCESS_DEBUG_EVENT:
				// TODO(eteran): should we be closing this handle?
				CloseHandle(de.u.CreateProcessInfo.hFile);
				process_->start_address_ = edb::address_t::fromZeroExtended(de.u.CreateProcessInfo.lpStartAddress);
				process_->image_base_    = edb::address_t::fromZeroExtended(de.u.CreateProcessInfo.lpBaseOfImage);
				break;
			case LOAD_DLL_DEBUG_EVENT:
				CloseHandle(de.u.LoadDll.hFile);
				break;
			case EXIT_PROCESS_DEBUG_EVENT:
				process_        = nullptr;
				// handle_event_exited returns DEBUG_STOP, which in turn keeps the debugger from resuming the process
				// However, this is needed to close all internal handles etc. and finish the debugging session
				// So we do it manually here
				resume(edb::DEBUG_CONTINUE);
				propagate = true;
				break;
			case EXCEPTION_DEBUG_EVENT:
				propagate = true;
				break;
			default:
				break;
			}

			if(propagate) {
				// normal event
				auto e = std::make_shared<PlatformEvent>();
				e->event = de;
				return e;
			}
			resume(edb::DEBUG_EXCEPTION_NOT_HANDLED);
		}
	}
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::attach(edb::pid_t pid) {

	detach();

	// These should be all the permissions we need
	const DWORD ACCESS = PROCESS_TERMINATE | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;

	if(HANDLE ph = OpenProcess(ACCESS, false, pid)) {
		if(DebugActiveProcess(pid)) {
			process_ = std::make_shared<PlatformProcess>(this, ph);
			return Status::Ok;
		}
		else {
			CloseHandle(ph);
		}
	}

	return Status("Error DebuggerCore::attach");
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
Status DebuggerCore::detach() {
	if(attached()) {
		clear_breakpoints();
		// Make sure exceptions etc. are passed
		ContinueDebugEvent(process_->pid(), active_thread(), DBG_CONTINUE);
		DebugActiveProcessStop(process_->pid());
		process_ = nullptr;
		threads_.clear();
	}
	return Status::Ok;
}

//------------------------------------------------------------------------------
// Name: kill
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		TerminateProcess(process_->handle_, -1);
		detach();
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
			// TODO: does this resume *all* threads?
			// it does! (unless you manually paused one using SuspendThread)
			ContinueDebugEvent(
			    process_->pid(),
				active_thread(),
				(status == edb::DEBUG_CONTINUE) ? (DBG_CONTINUE) : (DBG_EXCEPTION_NOT_HANDLED));
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

			Win32Thread thread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, active_thread());
			if(thread) {
				CONTEXT context;
				context.ContextFlags = CONTEXT_CONTROL;
				thread.GetThreadContext(&context);
				context.EFlags |= (1 << 8); // set the trap flag
				thread.SetThreadContext(&context);

				resume(status);
				/*
				ContinueDebugEvent(
				pid(),
				active_thread(),
				(status == edb::DEBUG_CONTINUE) ? (DBG_CONTINUE) : (DBG_EXCEPTION_NOT_HANDLED));
				*/
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_state
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State *state) {
	// TODO: assert that we are paused
	Q_ASSERT(state);

    auto state_impl = static_cast<PlatformState *>(state->impl_.get());

	if(attached() && state_impl) {


		Win32Thread thread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, active_thread());

		if(thread) {
			state_impl->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
			thread.GetThreadContext(&state_impl->context_);

			state_impl->gs_base_ = 0;
			state_impl->fs_base_ = 0;
			// GetThreadSelectorEntry always returns false on x64
			// on x64 gs_base == TEB, maybe we can use that somehow
#if !defined(EDB_X86_64)
			LDT_ENTRY ldt_entry;
			if(thread.GetThreadSelectorEntry(state_impl->context_.SegGs, &ldt_entry)) {
				state_impl->gs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
			}

			if(thread.GetThreadSelectorEntry(state_impl->context_.SegFs, &ldt_entry)) {
				state_impl->fs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
			}
#endif

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

    auto state_impl = static_cast<PlatformState *>(state.impl_.get());

	if(attached()) {
		state_impl->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;

		Win32Thread thread(THREAD_SET_CONTEXT, FALSE, active_thread());

		if(thread) {
			thread.SetThreadContext(&state_impl->context_);
		}
	}
}

//------------------------------------------------------------------------------
// Name: open
// Desc:
// TODO: Don't inherit security descriptors from this process (default values)
//       Is this even possible?
//------------------------------------------------------------------------------
Status DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {

	Q_UNUSED(tty);

	Q_ASSERT(!path.isEmpty());

	bool ok = false;

	detach();

	// default to process's directory
	QString tcwd;
	if(cwd.isEmpty()) {
		tcwd = QFileInfo(path).canonicalPath();
	} else {
		tcwd = cwd;
	}

	STARTUPINFO         startup_info = { 0 };
	PROCESS_INFORMATION process_info = { nullptr };

	const DWORD CREATE_FLAGS = DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;

    wchar_t *const env_block = GetEnvironmentStringsW();

	// Set up command line
	QString command_str = '\"' + QFileInfo(path).canonicalPath() + '\"'; // argv[0] = full path (explorer style)
	if(!args.isEmpty()) {
		for(QByteArray arg: args) {
			command_str += " ";
			command_str += arg;
		}
	}

	// CreateProcessW wants a writable copy of the command line :<
	auto command_path = new wchar_t[command_str.length() + sizeof(wchar_t)];
    wcscpy_s(command_path, command_str.length() + 1, reinterpret_cast<const wchar_t*>(command_str.utf16()));

	if(CreateProcessW(
			reinterpret_cast<const wchar_t*>(path.utf16()), // exe
	        command_path,    // commandline
	        nullptr,         // default security attributes
	        nullptr,         // default thread security too
	        FALSE,           // inherit handles
			CREATE_FLAGS,
	        env_block,       // environment data
			reinterpret_cast<const wchar_t*>(tcwd.utf16()), // working directory
			&startup_info,
			&process_info)) {

		active_thread_ = process_info.dwThreadId;
		CloseHandle(process_info.hThread); // We don't need the thread handle
		set_debug_privilege(process_info.hProcess, false);

		//process_info.hProcess  has PROCESS_ALL_ACCESS
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

//------------------------------------------------------------------------------
// Name: create_state
// Desc:
//------------------------------------------------------------------------------
std::unique_ptr<IState> DebuggerCore::create_state() const {
	return std::make_unique<PlatformState>();
}

//------------------------------------------------------------------------------
// Name: sys_pointer_size
// Desc: returns the size of a pointer on this arch
//------------------------------------------------------------------------------
int DebuggerCore::sys_pointer_size() const {
	return sizeof(void *);
}

//------------------------------------------------------------------------------
// Name: enumerate_processes
// Desc:
//------------------------------------------------------------------------------
QMap<edb::pid_t, std::shared_ptr<IProcess> > DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, std::shared_ptr<IProcess> > ret;

	HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(handle != INVALID_HANDLE_VALUE) {

		PROCESSENTRY32 lppe;

		std::memset(&lppe, 0, sizeof(lppe));
		lppe.dwSize = sizeof(lppe);

		if(Process32First(handle, &lppe)) {
			do {
				// NOTE(eteran): the const_cast is reasonable here.
				// While we don't want THIS function to mutate the DebuggerCore object
				// we do want the associated PlatformProcess to be able to trigger
				// non-const operations in the future, at least hypothetically.
				auto pi = std::make_shared<PlatformProcess>(const_cast<DebuggerCore*>(this), lppe);
				if(pi->handle_ == nullptr) {
					continue;
				}

				ret.insert(pi->pid(), pi);

				std::memset(&lppe, 0, sizeof(lppe));
				lppe.dwSize = sizeof(lppe);
			} while(Process32Next(handle, &lppe));
		}

		CloseHandle(handle);
	}
	return ret;
}


//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::pid_t DebuggerCore::parent_pid(edb::pid_t pid) const {
	edb::pid_t parent = 1; // 1??
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);
	if(hProcessSnap != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(pe32);

		if(Process32FirstW(hProcessSnap, &pe32)) {
			do {
				if(pid == pe32.th32ProcessID) {
					parent = pe32.th32ParentProcessID;
					break;
				}
			} while(Process32NextW(hProcessSnap, &pe32));
		}
		CloseHandle(hProcessSnap);
	}
	return parent;
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
QMap<qlonglong, QString> DebuggerCore::exceptions() const {
	QMap<qlonglong, QString> exceptions;

	return exceptions;
}

//------------------------------------------------------------------------------
// Name: cpu_type
// Desc:
//------------------------------------------------------------------------------
quint64 DebuggerCore::cpu_type() const {
#ifdef EDB_X86
	return edb::string_hash("x86");
#elif defined(EDB_X86_64)
	return edb::string_hash("x86-64");
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

IProcess *DebuggerCore::process() const  {
	return process_.get();
}

}
