/*
Copyright (C) 2015 - 2015 Evan Teran
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


#include "PlatformProcess.h"
#include "PlatformRegion.h"
#include <processthreadsapi.h>

namespace DebuggerCorePlugin {

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
	ProcessDebugPort        = 7,
	ProcessWow64Information = 26,
	ProcessImageFileName    = 27
} PROCESSINFOCLASS;

    PlatformProcess::PlatformProcess(DebuggerCore *core, const PROCESSENTRY32 &pe) : core_(core) {
		pid_    = pe.th32ProcessID;
		name_   = QString::fromWCharArray(pe.szExeFile);
		handle_ = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid_);

		if (HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid_)) {
			BOOL wow64 = FALSE;
			using LPFN_ISWOW64PROCESS = BOOL(WINAPI *) (HANDLE, PBOOL);
			LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
			if (fnIsWow64Process && fnIsWow64Process(hProc, &wow64) && wow64) {
				name_ += " *32";
			}

			HANDLE hToken;
			if (OpenProcessToken(hProc, TOKEN_QUERY, &hToken)) {

				DWORD needed;
				GetTokenInformation(hToken, TokenOwner, nullptr, 0, &needed);

				if (auto owner = static_cast<TOKEN_OWNER *>(malloc(needed))) {
					if (GetTokenInformation(hToken, TokenOwner, owner, needed, &needed)) {
						WCHAR user[MAX_PATH];
						WCHAR domain[MAX_PATH];
						DWORD user_sz = MAX_PATH;
						DWORD domain_sz = MAX_PATH;
						SID_NAME_USE snu;

						if (LookupAccountSid(nullptr, owner->Owner, user, &user_sz, domain, &domain_sz, &snu) && snu == SidTypeUser) {
							user_ = QString::fromWCharArray(user);
						}
					}
					free(owner);
				}

				CloseHandle(hToken);
			}

			CloseHandle(hProc);
		}
	}

	PlatformProcess::PlatformProcess(DebuggerCore *core, HANDLE handle) : core_(core), handle_(handle) {
	}

	PlatformProcess::~PlatformProcess()  {
		if(handle_) {
			CloseHandle(handle_);
		}
	}

	QDateTime PlatformProcess::start_time() const {
		Q_ASSERT(handle_);

		FILETIME create;
		FILETIME exit;
		FILETIME kernel;
		FILETIME user;

		if(GetProcessTimes(handle_, &create, &exit, &kernel, &user)) {

			ULARGE_INTEGER createTime;
			createTime.LowPart  = create.dwLowDateTime;
			createTime.HighPart = create.dwHighDateTime;
			return QDateTime::fromMSecsSinceEpoch(createTime.QuadPart / 10000);
		}
	}

	edb::pid_t PlatformProcess::pid() const {
		Q_ASSERT(handle_);
		return GetProcessId(handle_);
	}

	QString PlatformProcess::name() const {
		return name_;
	}

	QString PlatformProcess::user() const {
		return user_;
	}

	std::shared_ptr<IProcess> PlatformProcess::parent() const  {
		edb::pid_t parent_pid = core_->parent_pid(pid_);

		qDebug("TODO: implement PlatformProcess::parent"); return std::shared_ptr<IProcess>();
	}

	edb::uid_t PlatformProcess::uid() const {
		Q_ASSERT(handle_);

		HANDLE token;
		TOKEN_USER user;
		DWORD length;

		// TODO(eteran): is this on the right track?
		if(OpenProcessToken(handle_, 0, &token)) {
			if(GetTokenInformation(token, TokenUser, &user, sizeof(user), &length)) {
			}
		}

		return 0;
	}

	QMap<edb::address_t, Patch> PlatformProcess::patches() const {
		return patches_;
	}

	std::size_t PlatformProcess::write_bytes(edb::address_t address, const void *buf, size_t len) {
		Q_ASSERT(buf);

		if(handle_) {
			if(len == 0) {
				return 0;
			}

			SIZE_T bytes_written = 0;
			if(WriteProcessMemory(handle_, reinterpret_cast<LPVOID>(address.toUint()), buf, len, &bytes_written)) {
				return bytes_written;
			}
		}
		return 0;
	}

	std::size_t PlatformProcess::read_bytes(edb::address_t address, void *buf, size_t len) const {
		Q_ASSERT(buf);

		if(handle_) {
			if(len == 0) {
				return 0;
			}

			memset(buf, 0xff, len);
			SIZE_T bytes_read = 0;
			if(ReadProcessMemory(handle_, reinterpret_cast<LPCVOID>(address.toUint()), buf, len, &bytes_read)) {
				// TODO(eteran): implement breakpoint stuff
#if 0
				for(const std::shared_ptr<IBreakpoint> &bp: breakpoints_) {

					if(bp->address() >= address && bp->address() < address + bytes_read) {
						reinterpret_cast<quint8 *>(buf)[bp->address() - address] = bp->original_bytes()[0];
					}
				}
#endif
				return bytes_read;
			}
		}
		return 0;
	}

	std::size_t PlatformProcess::read_pages(edb::address_t address, void *buf, size_t count) const {
		Q_ASSERT(address % core_->page_size() == 0);
		return read_bytes(address, buf, core_->page_size() * count);
	}

	Status PlatformProcess::pause()  {
		Q_ASSERT(handle_);
		if(DebugBreakProcess(handle_)) {
			return Status::Ok;
		}

		// TODO(eteran): use GetLastError/FormatMessage
		return Status("Failed to pause");
	}

	QList<std::shared_ptr<IRegion>> PlatformProcess::regions() const {
		QList<std::shared_ptr<IRegion>> regions;

		if(handle_) {
			    edb::address_t addr = 0;
				auto last_base    = reinterpret_cast<LPVOID>(-1);

				Q_FOREVER {
					MEMORY_BASIC_INFORMATION info;
					VirtualQueryEx(handle_, reinterpret_cast<LPVOID>(addr.toUint()), &info, sizeof(info));

					if(last_base == info.BaseAddress) {
						break;
					}

					last_base = info.BaseAddress;

					if(info.State == MEM_COMMIT) {

						const auto start   = edb::address_t::fromZeroExtended(info.BaseAddress);
						const auto end     = edb::address_t::fromZeroExtended(info.BaseAddress) + info.RegionSize;
						const auto base    = edb::address_t::fromZeroExtended(info.AllocationBase);
						const QString name = QString();
						const IRegion::permissions_t permissions = info.Protect; // let std::shared_ptr<IRegion> handle permissions and modifiers

						if(info.Type == MEM_IMAGE) {
							// set region.name to the module name
						}
						// get stack addresses, PEB, TEB, etc. and set name accordingly

						regions.push_back(std::make_shared<PlatformRegion>(start, end, base, name, permissions));
					}

					addr += info.RegionSize;
				}
		}

		return regions;
	}

	QString PlatformProcess::executable() const  {
		Q_ASSERT(handle_);

		// These functions don't work immediately after CreateProcess but only
		// after basic initialization, usually after the system breakpoint
		// The same applies to psapi/toolhelp, maybe using NtQueryXxxxxx is the way to go

		using QueryFullProcessImageNameWPtr = BOOL (WINAPI *)(
		    HANDLE hProcess,
		    DWORD dwFlags,
		    LPWSTR lpExeName,
		    PDWORD lpdwSize
		);

		HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
		QueryFullProcessImageNameWPtr QueryFullProcessImageNameWFunc = (QueryFullProcessImageNameWPtr)GetProcAddress(kernel32, "QueryFullProcessImageNameW");

		wchar_t name[MAX_PATH] = L"";

		if(QueryFullProcessImageNameWFunc/* && LOBYTE(GetVersion()) >= 6*/) { // Vista and up

			DWORD size = _countof(name);
			if(QueryFullProcessImageNameWFunc(handle_, 0, name, &size)) {
				return  QString::fromWCharArray(name);
			}
		}

		return {};
	}

	QList<QByteArray> PlatformProcess::arguments() const  {

		Q_ASSERT(handle_);

		QList<QByteArray> ret;
		if(handle_) {

			using ZwQueryInformationProcessPtr = NTSTATUS (*WINAPI)(
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
				    ULONG l;
					NTSTATUS r = ZwQueryInformationProcessFunc(handle_, ProcessBasicInformation, &ProcessInfo, sizeof(PROCESS_BASIC_INFORMATION), &l);

			}
		}
		return ret;
	}
}
