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
#include "PlatformRegion.h"
#include "PlatformState.h"
#include "State.h"
#include "string_hash.h"

#include <QDebug>
#include <QStringList>
#include <QFileInfo>

#include <windows.h>
#include <tlhelp32.h>
#include <Psapi.h>

#include <algorithm>

#ifdef _MSC_VER
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Psapi.lib")
#endif

namespace DebuggerCore {

typedef struct _LSA_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

typedef struct _PEB_LDR_DATA {
  BYTE       Reserved1[8];
  PVOID      Reserved2[3];
  LIST_ENTRY InMemoryOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
  BYTE           Reserved1[16];
  PVOID          Reserved2[10];
  UNICODE_STRING ImagePathName;
  UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef VOID (NTAPI *PPS_POST_PROCESS_INIT_ROUTINE)(VOID);

#ifdef Q_OS_WIN64
typedef struct _PEB {
    BYTE Reserved1[2];
    BYTE BeingDebugged;
    BYTE Reserved2[21];
    PPEB_LDR_DATA LoaderData;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
    BYTE Reserved3[520];
    PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
    BYTE Reserved4[136];
    ULONG SessionId;
} PEB, *PPEB;
#else
typedef struct _PEB {
  BYTE                          Reserved1[2];
  BYTE                          BeingDebugged;
  BYTE                          Reserved2[1];
  PVOID                         Reserved3[2];
  PPEB_LDR_DATA                 Ldr;
  PRTL_USER_PROCESS_PARAMETERS  ProcessParameters;
  BYTE                          Reserved4[104];
  PVOID                         Reserved5[52];
  PPS_POST_PROCESS_INIT_ROUTINE PostProcessInitRoutine;
  BYTE                          Reserved6[128];
  PVOID                         Reserved7[1];
  ULONG                         SessionId;
} PEB, *PPEB;
#endif

typedef struct _PROCESS_BASIC_INFORMATION {
    PVOID Reserved1;
    PPEB PebBaseAddress;
    PVOID Reserved2[2];
    ULONG_PTR UniqueProcessId;
    PVOID Reserved3;
} PROCESS_BASIC_INFORMATION;

typedef enum _PROCESSINFOCLASS {
    ProcessBasicInformation = 0,
    ProcessDebugPort = 7,
    ProcessWow64Information = 26,
    ProcessImageFileName = 27

} PROCESSINFOCLASS;

namespace {
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;


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
			return reinterpret_cast<void *>(handle_ != 0);
		}

	private:
		HANDLE handle_;
	};

	QString get_user_token(HANDLE hProcess) {
		QString ret;
		HANDLE hToken;

		if(!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
			return ret;
		}

		DWORD needed;
		GetTokenInformation(hToken, TokenOwner, NULL, 0, &needed);

		if(auto owner = static_cast<TOKEN_OWNER *>(malloc(needed))) {
			if(GetTokenInformation(hToken, TokenOwner, owner, needed, &needed)) {
				WCHAR user[MAX_PATH];
				WCHAR domain[MAX_PATH];
				DWORD user_sz   = MAX_PATH;
				DWORD domain_sz = MAX_PATH;
				SID_NAME_USE snu;

				if(LookupAccountSid(NULL, owner->Owner, user, &user_sz, domain, &domain_sz, &snu) && snu == SidTypeUser) {
					ret = QString::fromWCharArray(user);
				}
			}
			free(owner);
		}

		CloseHandle(hToken);
		return ret;
	}

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
            if(LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
                TOKEN_PRIVILEGES tp;
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Luid = luid;
                tp.Privileges[0].Attributes = set ? SE_PRIVILEGE_ENABLED : 0;

                ok = AdjustTokenPrivileges(token, false, &tp, NULL, NULL, NULL);
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
DebuggerCore::DebuggerCore() : start_address(0), image_base(0), page_size_(0), process_handle_(0) {
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
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
}

//------------------------------------------------------------------------------
// Name: has_extension
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::has_extension(quint64 ext) const {
#if !defined(EDB_X86_64)
	switch(ext) {
	case edb::string_hash<'M', 'M', 'X'>::value:
		return IsProcessorFeaturePresent(PF_MMX_INSTRUCTIONS_AVAILABLE);
	case edb::string_hash<'X', 'M', 'M'>::value:
		return IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE);
	default:
		return false;
	}
#else
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
// Name: wait_debug_event
// Desc: waits for a debug event, secs is a timeout (but is not yet respected)
//       ok will be set to false if the timeout expires
//------------------------------------------------------------------------------
IDebugEvent::const_pointer DebuggerCore::wait_debug_event(int msecs) {
	if(attached()) {
		DEBUG_EVENT de;
		while(WaitForDebugEvent(&de, msecs == 0 ? INFINITE : msecs)) {

			Q_ASSERT(pid_ == de.dwProcessId);

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
				CloseHandle(de.u.CreateProcessInfo.hFile);
				start_address = reinterpret_cast<edb::address_t>(de.u.CreateProcessInfo.lpStartAddress);
				image_base    = reinterpret_cast<edb::address_t>(de.u.CreateProcessInfo.lpBaseOfImage);
				break;
			case LOAD_DLL_DEBUG_EVENT:
				CloseHandle(de.u.LoadDll.hFile);
				break;
			case EXIT_PROCESS_DEBUG_EVENT:
				CloseHandle(process_handle_);
				process_handle_ = 0;
				pid_            = 0;
				start_address   = 0;
				image_base      = 0;
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
// Name: read_pages
// Desc: reads <count> pages from the process starting at <address>
// Note: buf's size must be >= count * page_size()
// Note: address MUST be page aligned.
//------------------------------------------------------------------------------
bool DebuggerCore::read_pages(edb::address_t address, void *buf, std::size_t count) {

	Q_ASSERT(address % page_size() == 0);

	return read_bytes(address, buf, page_size() * count);
}

//------------------------------------------------------------------------------
// Name: read_bytes
// Desc: reads <len> bytes into <buf> starting at <address>
// Note: if the read failed, the part of the buffer that could not be read will
//       be filled with 0xff bytes
//------------------------------------------------------------------------------
bool DebuggerCore::read_bytes(edb::address_t address, void *buf, std::size_t len) {

	Q_ASSERT(buf);

	if(attached()) {
		if(len == 0) {
			return true;
		}

		memset(buf, 0xff, len);
		SIZE_T bytes_read = 0;
        if(ReadProcessMemory(process_handle_, reinterpret_cast<void*>(address), buf, len, &bytes_read)) {
			for(const IBreakpoint::pointer &bp: breakpoints_) {

				if(bp->address() >= address && bp->address() < address + bytes_read) {
					reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_byte();
				}
			}
            return true;
		}
	}
    return false;
}

//------------------------------------------------------------------------------
// Name: write_bytes
// Desc: writes <len> bytes from <buf> starting at <address>
//------------------------------------------------------------------------------
bool DebuggerCore::write_bytes(edb::address_t address, const void *buf, std::size_t len) {

	Q_ASSERT(buf);

	if(attached()) {
		if(len == 0) {
			return true;
		}

		SIZE_T bytes_written = 0;
        return WriteProcessMemory(process_handle_, reinterpret_cast<void*>(address), buf, len, &bytes_written);
	}
    return false;
}

//------------------------------------------------------------------------------
// Name: attach
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach(edb::pid_t pid) {

	detach();

	// These should be all the permissions we need
	const DWORD ACCESS = PROCESS_TERMINATE | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;

	if(HANDLE ph = OpenProcess(ACCESS, false, pid)) {
		if(DebugActiveProcess(pid)) {
			process_handle_ = ph;
			pid_            = pid;
			return true;
		}
		else {
			CloseHandle(ph);
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: detach
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::detach() {
	if(attached()) {
		clear_breakpoints();
		// Make sure exceptions etc. are passed
		ContinueDebugEvent(pid(), active_thread(), DBG_CONTINUE);
		DebugActiveProcessStop(pid());
		CloseHandle(process_handle_);
		process_handle_ = 0;
		pid_            = 0;
		start_address   = 0;
		image_base      = 0;
		threads_.clear();
	}
}

//------------------------------------------------------------------------------
// Name: kill
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		TerminateProcess(process_handle_, -1);
		detach();
	}
}

//------------------------------------------------------------------------------
// Name: pause
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void DebuggerCore::pause() {
	if(attached()) {
		DebugBreakProcess(process_handle_);
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
				pid(),
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

	auto state_impl = static_cast<PlatformState *>(state->impl_);

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

	auto state_impl = static_cast<PlatformState *>(state.impl_);

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
bool DebuggerCore::open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) {

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
	PROCESS_INFORMATION process_info = { 0 };

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
    wcscpy_s(command_path, command_str.length() + 1, command_str.utf16());

	if(CreateProcessW(
			path.utf16(), // exe
			command_path, // commandline
			NULL,         // default security attributes
			NULL,         // default thread security too
			FALSE,        // inherit handles
			CREATE_FLAGS,
			env_block,    // environment data
			tcwd.utf16(), // working directory
			&startup_info,
			&process_info)) {

		pid_ = process_info.dwProcessId;
		active_thread_ = process_info.dwThreadId;
		process_handle_ = process_info.hProcess; // has PROCESS_ALL_ACCESS
		CloseHandle(process_info.hThread); // We don't need the thread handle
		set_debug_privilege(process_handle_, false);
		ok = true;
	}

	delete[] command_path;
	FreeEnvironmentStringsW(env_block);

	return ok;
}

//------------------------------------------------------------------------------
// Name: create_state
// Desc:
//------------------------------------------------------------------------------
IState *DebuggerCore::create_state() const {
	return new PlatformState;
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
QMap<edb::pid_t, ProcessInfo> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, ProcessInfo> ret;

	HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(handle != INVALID_HANDLE_VALUE) {

		// resolve the "IsWow64Process" function since it may or may not exist
		fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

		PROCESSENTRY32 lppe;

		std::memset(&lppe, 0, sizeof(lppe));
		lppe.dwSize = sizeof(lppe);

		if(Process32First(handle, &lppe)) {
			do {
				ProcessInfo pi;
				pi.pid = lppe.th32ProcessID;
				pi.uid = 0; // TODO
				pi.name = QString::fromWCharArray(lppe.szExeFile);

				if(HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, lppe.th32ProcessID)) {
					BOOL wow64 = FALSE;
					if(fnIsWow64Process && fnIsWow64Process(hProc, &wow64) && wow64) {
						pi.name += " *32";
					}

					pi.user = get_user_token(hProc);

					CloseHandle(hProc);
				}

				ret.insert(pi.pid, pi);

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
QString DebuggerCore::process_exe(edb::pid_t pid) const {
	QString ret;

	// These functions don't work immediately after CreateProcess but only
	// after basic initialization, usually after the system breakpoint
	// The same applies to psapi/toolhelp, maybe using NtQueryXxxxxx is the way to go

	typedef BOOL (WINAPI *QueryFullProcessImageNameWPtr)(
	  HANDLE hProcess,
	  DWORD dwFlags,
	  LPWSTR lpExeName,
	  PDWORD lpdwSize
	);

	HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
	QueryFullProcessImageNameWPtr QueryFullProcessImageNameWFunc = (QueryFullProcessImageNameWPtr)GetProcAddress(kernel32, "QueryFullProcessImageNameW");

	wchar_t name[MAX_PATH] = L"";

	if(QueryFullProcessImageNameWFunc/* && LOBYTE(GetVersion()) >= 6*/) { // Vista and up
		const DWORD ACCESS = PROCESS_QUERY_LIMITED_INFORMATION;

		if(HANDLE ph = OpenProcess(ACCESS, false, pid)) {
			DWORD size = _countof(name);
			if(QueryFullProcessImageNameWFunc(ph, 0, name, &size)) {
				ret = QString::fromWCharArray(name);
			}
			CloseHandle(ph);
		}
	} else {
		// Attempting to get an unknown privilege will fail unless we have
		// debug privilege set, so 2 calls to OpenProcess
		// (PROCESS_QUERY_LIMITED_INFORMATION is Vista and up)
		const DWORD ACCESS = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;

		if(HANDLE ph = OpenProcess(ACCESS, false, pid)) {
			if(GetModuleFileNameExW(ph, NULL, name, _countof(name))) {
				ret = QString::fromWCharArray(name);
			}
			CloseHandle(ph);
		}
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QString DebuggerCore::process_cwd(edb::pid_t pid) const {
    Q_UNUSED(pid);
	// TODO: implement this
	return QString();
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
QList<IRegion::pointer> DebuggerCore::memory_regions() const {
	QList<IRegion::pointer> regions;

	if(pid_ != 0) {
		if(HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_)) {
			edb::address_t addr = 0;
			auto last_base    = reinterpret_cast<LPVOID>(-1);

			Q_FOREVER {
				MEMORY_BASIC_INFORMATION info;
				VirtualQueryEx(ph, reinterpret_cast<LPVOID>(addr), &info, sizeof(info));

				if(last_base == info.BaseAddress) {
					break;
				}

				last_base = info.BaseAddress;

				if(info.State == MEM_COMMIT) {

					const auto start   = reinterpret_cast<edb::address_t>(info.BaseAddress);
					const auto end     = reinterpret_cast<edb::address_t>(info.BaseAddress) + info.RegionSize;
					const auto base    = reinterpret_cast<edb::address_t>(info.AllocationBase);
					const QString name = QString();
					const IRegion::permissions_t permissions = info.Protect; // let IRegion::pointer handle permissions and modifiers

					if(info.Type == MEM_IMAGE) {
						// set region.name to the module name
					}
					// get stack addresses, PEB, TEB, etc. and set name accordingly

					regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
				}

				addr += info.RegionSize;
			}

			CloseHandle(ph);
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

       typedef NTSTATUS (*WINAPI ZwQueryInformationProcessPtr)(
          HANDLE ProcessHandle,
          PROCESSINFOCLASS ProcessInformationClass,
          PVOID ProcessInformation,
          ULONG ProcessInformationLength,
          PULONG ReturnLength
        );


        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        ZwQueryInformationProcessPtr ZwQueryInformationProcessFunc = (ZwQueryInformationProcessPtr)GetProcAddress(ntdll, "NtQueryInformationProcess");
        PROCESS_BASIC_INFORMATION ProcessInfo;

        if(ZwQueryInformationProcessFunc) {
            if(HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_)) {
                ULONG l;
                NTSTATUS r = ZwQueryInformationProcessFunc(ph, ProcessBasicInformation, &ProcessInfo, sizeof(PROCESS_BASIC_INFORMATION), &l);

                CloseHandle(ph);
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
QMap<long, QString> DebuggerCore::exceptions() const {
	QMap<long, QString> exceptions;

	return exceptions;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
QList<Module> DebuggerCore::loaded_modules() const {

	QList<Module> ret;
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid());
	if(hModuleSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me32;
		me32.dwSize = sizeof(me32);

        if(Module32First(hModuleSnap, &me32)) {
			do {
				Module module;
				module.base_address = reinterpret_cast<edb::address_t>(me32.modBaseAddr);
				module.name         = QString::fromWCharArray(me32.szModule);
				ret.push_back(module);
			} while(Module32Next(hModuleSnap, &me32));
		}
	}
	CloseHandle(hModuleSnap);
	return ret;
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
// Name: cpu_type
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
