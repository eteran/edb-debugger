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
#include "DebugEvent.h"
#include "State.h"
#include "PlatformState.h"
#include <QDebug>
#include <QStringList>
#include <windows.h>


class Win32Handle {
public:
	Win32Handle() : handle(NULL) {}
	Win32Handle(HANDLE handle) : handle(handle) {}
	~Win32Handle() { if(valid()) { CloseHandle(handle); } }
private:
	Win32Handle(const Win32Handle&);
	Win32Handle &operator=(const Win32Handle&);
	bool valid() const { return (handle != 0 && handle != INVALID_HANDLE_VALUE); }

public:
	HANDLE get() const { return handle; }

public:
	//operator void*() const { return reinterpret_cast<void*>(handle != 0); }
	operator bool() const { return valid(); }
	operator HANDLE() const { return handle; }

private:
	HANDLE handle;
};

//------------------------------------------------------------------------------
// Name: DebuggerCore()
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCore::DebuggerCore() : page_size_(0), process_handle_(0), start_address(0), image_base(0) {
	DebugSetProcessKillOnExit(false);

	SYSTEM_INFO sys_info;
	GetSystemInfo(&sys_info);
	page_size_ = sys_info.dwPageSize;
}

//------------------------------------------------------------------------------
// Name: ~DebuggerCore()
// Desc:
//------------------------------------------------------------------------------
DebuggerCore::~DebuggerCore() {
	detach();
}

//------------------------------------------------------------------------------
// Name: page_size() const
// Desc: returns the size of a page on this system
//------------------------------------------------------------------------------
edb::address_t DebuggerCore::page_size() const {
	return page_size_;
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
	return read_bytes(address, buf, page_size() * count);
}

//------------------------------------------------------------------------------
// Name: read_bytes(edb::address_t address, void *buf, std::size_t len)
// Desc: reads <len> bytes into <buf> starting at <address>
// Note: if the read failed, the part of the buffer that could not be read will
//       be filled with 0xff bytes
//------------------------------------------------------------------------------
bool DebuggerCore::read_bytes(edb::address_t address, void *buf, std::size_t len) {

	Q_ASSERT(buf != 0);

	bool ok = false;

	if(attached()) {
		const edb::address_t orig_address	= address;
		const edb::address_t end_address	= orig_address + len;

		// easier to use VirtualQueryEx, memory_regions() doesnt contain unallocated regions etc.
		/*
		MemoryRegions mem = edb::v1::memory_regions();

		edb::address_t cur_address = address
		while(cur_address < end_address)
		{
			MemRegion region;
			if(mem.find_region(cur_address, region))
			{
				cur_address += region.size();
			}
			else
				return false; // This shouldn't happen
		}
		*/

		if(process_handle_ != 0) {
			SIZE_T bytes_read;
			ok = ReadProcessMemory(
				process_handle_,
				reinterpret_cast<LPVOID>(address),
				buf,
				len,
				&bytes_read);
		}
		// TODO: do the documented 0xff fill

		if(ok) {
			quint8 *const orig_ptr = reinterpret_cast<quint8 *>(buf);
			// TODO: handle if breakponts have a size more than 1!
			Q_FOREACH(const QSharedPointer<Breakpoint> &bp, breakpoints_) {
				if(bp->address() >= orig_address && bp->address() < end_address) {
					// show the original bytes in the buffer..
					orig_ptr[bp->address() - orig_address] = bp->original_bytes()[0];
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

	Q_ASSERT(buf != 0);

	bool ok = false;
	if(attached()) {
		if(process_handle_ != 0) {
			SIZE_T bytes_written;
			ok = WriteProcessMemory(
				process_handle_,
				reinterpret_cast<LPVOID>(address),
				buf,
				len,
				&bytes_written);

			if(ok) {
				FlushInstructionCache(process_handle_, reinterpret_cast<LPVOID>(address), bytes_written);
			}
		}
	}
	return ok;
}

//------------------------------------------------------------------------------
// Name: attach(edb::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::attach(edb::pid_t pid) {
	detach();

	// TODO: Call SeDebugPrivilege to be able to attach to system processes

	if(DebugActiveProcess(pid)) {
		// These should be all the permissions we need
		const DWORD ACCESS = PROCESS_TERMINATE | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME;
		process_handle_ = OpenProcess(ACCESS, false, pid);
		pid_ = pid;

		return true;
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
		threads_.clear();

		// Make sure exceptions etc. are passed
		resume(edb::DEBUG_CONTINUE);

		CloseHandle(process_handle_);
		process_handle_ = 0;

		DebugActiveProcessStop(pid());
		pid_ = 0;
	}
}

//------------------------------------------------------------------------------
// Name: kill()
// Desc:
//------------------------------------------------------------------------------
void DebuggerCore::kill() {
	if(attached()) {
		if(process_handle_ != 0) {
			TerminateProcess(process_handle_, -1);
		}
		detach();
		/*
		clear_breakpoints();
		if(process_handle_ != 0) {
			TerminateProcess(process_handle_, -1);
			// loop until we reach EXIT_PROCESS_DEBUG_EVENT so all handles are closed
			DEBUG_EVENT ev;
			BOOL success;
			resume(edb::DEBUG_CONTINUE);
			do {
				if(success = WaitForDebugEvent(&ev, INFINITE));
					ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
			} while (success && ev.dwDebugEventCode != EXIT_PROCESS_DEBUG_EVENT);
			CloseHandle(process_handle_);
			process_handle_ = 0;
		}
		pid_ = 0;
		*/
	}
}

//------------------------------------------------------------------------------
// Name: pause()
// Desc: stops *all* threads of a process
//------------------------------------------------------------------------------
void DebuggerCore::pause() {
	if(attached()) {
		if(process_handle_ != 0) {
			DebugBreakProcess(process_handle_);
		}
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

				context.EFlags |= (1 << 8); // set the TF bit
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
			state_impl->context_.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;
			GetThreadContext(th.get(), &state_impl->context_);

			state_impl->gs_base_ = 0;
			state_impl->fs_base_ = 0;
			// GetThreadSelectorEntry always returns false on x64
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
		state_impl->context_.ContextFlags = CONTEXT_FULL | CONTEXT_DEBUG_REGISTERS | CONTEXT_FLOATING_POINT;

		if(const Win32Handle th = OpenThread(/*THREAD_QUERY_INFORMATION | */THREAD_SET_CONTEXT, FALSE, active_thread())) {
			SetThreadContext(th.get(), &state_impl->context_);
		}
	}
}

//------------------------------------------------------------------------------
// Name: open(const QString &path, const QString &cwd, const QStringList &args, const QString &tty)
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCore::open(const QString &path, const QString &cwd, const QStringList &args, const QString &tty) {

	Q_UNUSED(tty);

	bool result = false;

	detach();

	STARTUPINFO         startup_info = {};
	PROCESS_INFORMATION process_info = {};

	const wchar_t *env_block = GetEnvironmentStringsW();

	QString command_str = "\"" + path + "\" ";

	QStringList::const_iterator it;
	for(it = args.constBegin(); it != args.constEnd(); ++it) {
		command_str += *it;
	}

	// CreateProcess seems to require a writable copy :0
	wchar_t* command_path = new wchar_t[command_str.length() + sizeof(wchar_t)];
	wcscpy(command_path, command_str.utf16());

	const DWORD create_flags = /*CREATE_SUSPENDED | */DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS | CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE;

	if(CreateProcessW(
			NULL,			// exe
			command_path,	// commandline
			NULL,			// default security attributes
			NULL,			// default thread security too
			FALSE,			// inherit handles
			create_flags,
			env_block,		// environment
			cwd.utf16(),	// cwd
			&startup_info,
			&process_info)) {

		pid_           = process_info.dwProcessId;
		//active_thread_ = process_info.dwThreadId;

		//ResumeThread(process_info.hThread);

		process_handle_ = process_info.hProcess;
		CloseHandle(process_info.hThread);
		result = true;
	}

	delete[] command_path;
	FreeEnvironmentStringsW(env_block);

	return result;
}

//------------------------------------------------------------------------------
// Name: create_state()
// Desc:
//------------------------------------------------------------------------------
StateInterface *DebuggerCore::create_state() const {
	return new PlatformState;
}

//------------------------------------------------------------------------------
// Name: pointer_size() const
// Desc: returns the size of a pointer on this arch
//------------------------------------------------------------------------------
int DebuggerCore::pointer_size() const {
	return sizeof(void *);
}

Q_EXPORT_PLUGIN2(DebuggerCore, DebuggerCore)
