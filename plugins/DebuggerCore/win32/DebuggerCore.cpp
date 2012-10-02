/*
Copyright (C) 2006 - 2011 Evan Teran
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

#include "DebuggerCore.h"
#include "Debugger.h"
#include "DebugEvent.h"
#include "State.h"
#include "PlatformState.h"
#include "PlatformRegion.h"
#include "MemoryRegions.h"
#include "MemoryRegion.h"
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

namespace {
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process;

	class Win32Handle {
	public:
		Win32Handle() : handle(NULL) {}
		Win32Handle(HANDLE handle) : handle(handle) {}
		~Win32Handle() { if(valid()) { CloseHandle(handle); } }
	private:
		Win32Handle(const Win32Handle&);
		Win32Handle &operator=(const Win32Handle&);
		bool valid() const { return (handle != NULL && handle != INVALID_HANDLE_VALUE); }

	public:
		HANDLE get() const { return handle; }

	public:
		operator bool() const { return valid(); }
		operator HANDLE() const { return handle; }

	private:
		HANDLE handle;
	};

	QString get_user_token(HANDLE hProcess) {
		QString ret;
		HANDLE hToken;
		
		if(!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
			return ret;
		}

		DWORD needed;
		GetTokenInformation(hToken, TokenOwner, NULL, 0, &needed);
		
		if(TOKEN_OWNER *owner = static_cast<TOKEN_OWNER *>(malloc(needed))) {
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
}


//------------------------------------------------------------------------------
// Name: DebuggerCore()
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
// Name: ~DebuggerCore()
// Desc:
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
	set_debug_privilege(GetCurrentProcess(), false);
}

//------------------------------------------------------------------------------
// Name: page_size() const
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
}

//------------------------------------------------------------------------------
// Name: has_extension(quint64 ext) const
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
// Name: wait_debug_event(DebugEvent &event, bool &ok, int secs)
// Desc: waits for a debug event, secs is a timeout (but is not yet respected)
//       ok will be set to false if the timeout expires
//------------------------------------------------------------------------------
bool DebuggerCore::wait_debug_event(DebugEvent &event, int msecs) {
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
				start_address	= reinterpret_cast<edb::address_t>(de.u.CreateProcessInfo.lpStartAddress);
				image_base		= reinterpret_cast<edb::address_t>(de.u.CreateProcessInfo.lpBaseOfImage);
				break;
			case LOAD_DLL_DEBUG_EVENT:
				CloseHandle(de.u.LoadDll.hFile);
				break;
			case EXIT_PROCESS_DEBUG_EVENT:
				CloseHandle(process_handle_);
				process_handle_ = 0;
				pid_			= 0;
				start_address	= 0;
				image_base		= 0;
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
				event = DebugEvent(de);
				return true;
			}
			resume(edb::DEBUG_EXCEPTION_NOT_HANDLED);
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: read_pages(edb::address_t address, void *buf, std::size_t count)
// Desc: reads <count> pages from the process starting at <address>
// Note: buf's size must be >= count * page_size()
// Note: address should be page aligned.
//------------------------------------------------------------------------------
bool DebuggerCore::read_pages(edb::address_t address, void *buf, std::size_t count) {

	Q_ASSERT(address % page_size() == 0);

	return read_bytes(address, buf, page_size() * count);
}

//------------------------------------------------------------------------------
// Name: read_bytes(edb::address_t address, void *buf, std::size_t len)
// Desc: reads <len> bytes into <buf> starting at <address>
// Note: if the read failed, the part of the buffer that could not be read will
//       be filled with 0xff bytes
//------------------------------------------------------------------------------
bool DebuggerCore::read_bytes(edb::address_t address, void *buf, std::size_t len) {

	Q_CHECK_PTR(buf);

	bool ok = false;

	if(attached()) {

		if(len == 0) {
			return true;
		}

		memset(buf, 0xff, len);

		SIZE_T bytes_read = 0;
		ok = ReadProcessMemory(process_handle_, reinterpret_cast<void*>(address), buf, len, &bytes_read);

		if(ok) {
			Q_FOREACH(const IBreakpoint::pointer &bp, breakpoints_) {
				// TODO: handle if breakponts have a size more than 1!
				if(bp->address() >= address && bp->address() < address + bytes_read) {
					reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_bytes()[0];
				}
			}
		}
	}
	return ok;
}

//------------------------------------------------------------------------------
// Name: write_bytes(edb::address_t address, const void *buf, std::size_t len)
// Desc: writes <len> bytes from <buf> starting at <address>
//------------------------------------------------------------------------------
bool DebuggerCore::write_bytes(edb::address_t address, const void *buf, std::size_t len) {

	Q_CHECK_PTR(buf);

	bool ok = false;

	if(attached()) {

		if(len == 0) {
			return true;
		}

		SIZE_T bytes_written = 0;
		ok = WriteProcessMemory(process_handle_, reinterpret_cast<void*>(address), buf, len, &bytes_written);
	}
	return ok;
}

//------------------------------------------------------------------------------
// Name: attach(edb::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach(edb::pid_t pid) {

	detach();

	// These should be all the permissions we need
	const DWORD ACCESS = PROCESS_TERMINATE | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;
	HANDLE ph = OpenProcess(ACCESS, false, pid);

	if(ph) {
		if(DebugActiveProcess(pid)) {
			process_handle_ = ph;
			pid_ = pid;
			return true;
		}
		else {
			CloseHandle(ph);
		}
	}

	return false;
}

//------------------------------------------------------------------------------
// Name: detach()
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
		pid_			= 0;
		start_address	= 0;
		image_base		= 0;
		threads_.clear();
	}
}

//------------------------------------------------------------------------------
// Name: kill()
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		TerminateProcess(process_handle_, -1);
		detach();
	}
}

//------------------------------------------------------------------------------
// Name: pause()
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void DebuggerCore::pause() {
	if(attached()) {
		DebugBreakProcess(process_handle_);
	}
}

//------------------------------------------------------------------------------
// Name: resume(edb::EVENT_STATUS status)
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
// Name: step(edb::EVENT_STATUS status)
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::step(edb::EVENT_STATUS status) {
	// TODO: assert that we are paused

	if(attached()) {
		if(status != edb::DEBUG_STOP) {
			if(const Win32Handle th = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, active_thread())) {
				CONTEXT	context;
				context.ContextFlags = CONTEXT_CONTROL;
				GetThreadContext(th.get(), &context);

				context.EFlags |= (1 << 8); // set the trap flag
				SetThreadContext(th.get(), &context);

				resume(status);
				/*ContinueDebugEvent(
					pid(),
					active_thread(),
					(status == edb::DEBUG_CONTINUE) ? (DBG_CONTINUE) : (DBG_EXCEPTION_NOT_HANDLED));*/
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: get_state(State &state)
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::get_state(State &state) {
	// TODO: assert that we are paused

	PlatformState *state_impl = static_cast<PlatformState *>(state.impl_);

	if(attached() && state_impl) {
		if(const Win32Handle th = OpenThread(THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT, FALSE, active_thread())) {
			state_impl->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
			GetThreadContext(th.get(), &state_impl->context_);

			state_impl->gs_base_ = 0;
			state_impl->fs_base_ = 0;
			// GetThreadSelectorEntry always returns false on x64
			// on x64 gs_base == TEB, maybe we can use that somehow
#if !defined(EDB_X86_64)
			LDT_ENTRY ldt_entry;
			if(GetThreadSelectorEntry(th.get(), state_impl->context_.SegGs, &ldt_entry)) {
				state_impl->gs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
			}

			if(GetThreadSelectorEntry(th.get(), state_impl->context_.SegFs, &ldt_entry)) {
				state_impl->fs_base_ = ldt_entry.BaseLow | (ldt_entry.HighWord.Bits.BaseMid << 16) | (ldt_entry.HighWord.Bits.BaseHi << 24);
			}
#endif

		}
	} else {
		state.clear();
	}
}

//------------------------------------------------------------------------------
// Name: set_state(const State &state)
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::set_state(const State &state) {

	// TODO: assert that we are paused

	PlatformState *state_impl = static_cast<PlatformState *>(state.impl_);

	if(attached()) {
		state_impl->context_.ContextFlags = CONTEXT_ALL; //CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;

		if(const Win32Handle th = OpenThread(THREAD_SET_CONTEXT, FALSE, active_thread())) {
			SetThreadContext(th.get(), &state_impl->context_);
		}
	}
}

//------------------------------------------------------------------------------
// Name: open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty)
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

	wchar_t *env_block = GetEnvironmentStringsW();

	// Set up command line
	QString command_str = '\"' + QFileInfo(path).canonicalPath() + '\"'; // argv[0] = full path (explorer style)
	if(!args.isEmpty()) {
		Q_FOREACH(QByteArray arg, args) {
			command_str += " ";
			command_str += arg;
		}
	}

	// CreateProcessW wants a writable copy of the command line :<
	wchar_t* command_path = new wchar_t[command_str.length() + sizeof(wchar_t)];
	wcscpy(command_path, command_str.utf16());

	if(CreateProcessW(
			path.utf16(),	// exe
			command_path,	// commandline
			NULL,			// default security attributes
			NULL,			// default thread security too
			FALSE,			// inherit handles
			CREATE_FLAGS,
			env_block,		// environment data
			tcwd.utf16(),	// working directory
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
// Name: create_state()
// Desc:
//------------------------------------------------------------------------------
IState *DebuggerCore::create_state() const {
	return new PlatformState;
}

//------------------------------------------------------------------------------
// Name: create_region(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, IRegion::permissions_t permissions)
// Desc:
//------------------------------------------------------------------------------
IRegion *DebuggerCore::create_region(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, IRegion::permissions_t permissions) const {
	return new PlatformRegion(start, end, base, name, permissions);
}

//------------------------------------------------------------------------------
// Name: pointer_size() const
// Desc: returns the size of a pointer on this arch
//------------------------------------------------------------------------------
int DebuggerCore::pointer_size() const {
	return sizeof(void *);
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
bool DebuggerCore::set_debug_privilege(HANDLE process, bool set) {

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

//------------------------------------------------------------------------------
// Name: enumerate_processes() const
// Desc:
//------------------------------------------------------------------------------
QMap<edb::pid_t, Process> DebuggerCore::enumerate_processes() const {
	QMap<edb::pid_t, Process> ret;
	
	HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(handle != INVALID_HANDLE_VALUE) {

		// resolve the "IsWow64Process" function since it may or may not exist
		fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

		PROCESSENTRY32 lppe;

		std::memset(&lppe, 0, sizeof(lppe));
		lppe.dwSize = sizeof(lppe);

		if(Process32First(handle, &lppe)) {
			do {
				Process pi;
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
	
	QueryFullProcessImageNameWPtr QueryFullProcessImageNameWFunc = 0;
	HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
	QueryFullProcessImageNameWFunc = (QueryFullProcessImageNameWPtr)GetProcAddress(kernel32, "QueryFullProcessImageNameW");
	
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
QList<MemoryRegion> DebuggerCore::memory_regions() const {
	QList<MemoryRegion> regions;

	if(pid_ != 0) {
		if(HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid_)) {
			edb::address_t addr = 0;
			LPVOID last_base    = reinterpret_cast<LPVOID>(-1);

			Q_FOREVER {
				MEMORY_BASIC_INFORMATION info;
				VirtualQueryEx(ph, reinterpret_cast<LPVOID>(addr), &info, sizeof(info));

				if(last_base == info.BaseAddress) {
					break;
				}

				last_base = info.BaseAddress;

				if(info.State == MEM_COMMIT) {

					const edb::address_t start = reinterpret_cast<edb::address_t>(info.BaseAddress);
					const edb::address_t end   = reinterpret_cast<edb::address_t>(info.BaseAddress) + info.RegionSize;
					const edb::address_t base  = reinterpret_cast<edb::address_t>(info.AllocationBase);
					const QString name         = QString();
					const IRegion::permissions_t permissions = info.Protect; // let MemoryRegion handle permissions and modifiers
					
					if(info.Type == MEM_IMAGE) {
						// set region.name to the module name
					}
					// get stack addresses, PEB, TEB, etc. and set name accordingly

					regions.push_back(MemoryRegion(start, end, base, name, permissions));
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
		// TODO: assert attached!
	}
	return ret;
}

Q_EXPORT_PLUGIN2(DebuggerCore, DebuggerCore)
