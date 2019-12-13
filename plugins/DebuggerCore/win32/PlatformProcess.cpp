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
#include "PlatformThread.h"
#include "edb.h"
#include <processthreadsapi.h>

namespace DebuggerCorePlugin {
namespace {

#pragma pack(push)
#pragma pack(1)
template <class T>
struct LIST_ENTRY_T {
	T Flink;
	T Blink;
};

template <class T>
struct UNICODE_STRING_T {
	union {
		struct {
			WORD Length;
			WORD MaximumLength;
		};
		T dummy;
	};
	T _Buffer;
};

template <class T, class NGF, int A>
struct _PEB_T {
	union {
		struct {
			BYTE InheritedAddressSpace;
			BYTE ReadImageFileExecOptions;
			BYTE BeingDebugged;
			BYTE _SYSTEM_DEPENDENT_01;
		};
		T dummy01;
	};
	T Mutant;
	T ImageBaseAddress;
	T Ldr;
	T ProcessParameters;
	T SubSystemData;
	T ProcessHeap;
	T FastPebLock;
	T _SYSTEM_DEPENDENT_02;
	T _SYSTEM_DEPENDENT_03;
	T _SYSTEM_DEPENDENT_04;
	union {
		T KernelCallbackTable;
		T UserSharedInfoPtr;
	};
	DWORD SystemReserved;
	DWORD _SYSTEM_DEPENDENT_05;
	T _SYSTEM_DEPENDENT_06;
	T TlsExpansionCounter;
	T TlsBitmap;
	DWORD TlsBitmapBits[2];
	T ReadOnlySharedMemoryBase;
	T _SYSTEM_DEPENDENT_07;
	T ReadOnlyStaticServerData;
	T AnsiCodePageData;
	T OemCodePageData;
	T UnicodeCaseTableData;
	DWORD NumberOfProcessors;
	union {
		DWORD NtGlobalFlag;
		NGF dummy02;
	};
	LARGE_INTEGER CriticalSectionTimeout;
	T HeapSegmentReserve;
	T HeapSegmentCommit;
	T HeapDeCommitTotalFreeThreshold;
	T HeapDeCommitFreeBlockThreshold;
	DWORD NumberOfHeaps;
	DWORD MaximumNumberOfHeaps;
	T ProcessHeaps;
	T GdiSharedHandleTable;
	T ProcessStarterHelper;
	T GdiDCAttributeList;
	T LoaderLock;
	DWORD OSMajorVersion;
	DWORD OSMinorVersion;
	WORD OSBuildNumber;
	WORD OSCSDVersion;
	DWORD OSPlatformId;
	DWORD ImageSubsystem;
	DWORD ImageSubsystemMajorVersion;
	T ImageSubsystemMinorVersion;
	union {
		T ImageProcessAffinityMask;
		T ActiveProcessAffinityMask;
	};
	T GdiHandleBuffer[A];
	T PostProcessInitRoutine;
	T TlsExpansionBitmap;
	DWORD TlsExpansionBitmapBits[32];
	T SessionId;
	ULARGE_INTEGER AppCompatFlags;
	ULARGE_INTEGER AppCompatFlagsUser;
	T pShimData;
	T AppCompatInfo;
	UNICODE_STRING_T<T> CSDVersion;
	T ActivationContextData;
	T ProcessAssemblyStorageMap;
	T SystemDefaultActivationContextData;
	T SystemAssemblyStorageMap;
	T MinimumStackCommit;
};
#pragma pack(pop)

typedef _PEB_T<DWORD, DWORD64, 34> PEB32;
typedef _PEB_T<DWORD64, DWORD, 30> PEB64;

typedef struct _PROCESS_BASIC_INFORMATION {
	PVOID Reserved1;
	PVOID PebBaseAddress;
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

bool getProcessEntry(edb::pid_t pid, PROCESSENTRY32 *entry) {

	bool ret = false;
	if (HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) {

		PROCESSENTRY32 peInfo;
		peInfo.dwSize = sizeof(peInfo); // this line is REQUIRED

		if (Process32First(hSnapshot, &peInfo)) {
			do {
				if (peInfo.th32ProcessID == pid) {
					*entry = peInfo;
					ret    = true;
					break;
				}
			} while (Process32Next(hSnapshot, &peInfo));
		}

		CloseHandle(hSnapshot);
	}

	return ret;
}

}

/**
 * @brief PlatformProcess::PlatformProcess
 * @param core
 * @param pid
 */
PlatformProcess::PlatformProcess(DebuggerCore *core, edb::pid_t pid)
	: core_(core) {
	hProcess_ = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
}

/**
 * @brief PlatformProcess::PlatformProcess
 * @param core
 * @param handle
 */
PlatformProcess::PlatformProcess(DebuggerCore *core, HANDLE handle)
	: core_(core) {

	DuplicateHandle(
		GetCurrentProcess(),
		handle,
		GetCurrentProcess(),
		&hProcess_,
		0,
		FALSE,
		DUPLICATE_SAME_ACCESS);
}

/**
 * @brief PlatformProcess::~PlatformProcess
 */
PlatformProcess::~PlatformProcess() {
	CloseHandle(hProcess_);
}

/**
 * @brief PlatformProcess::isWow64
 * @return
 */
bool PlatformProcess::isWow64() const {
#if defined(EDB_X86_64)
	BOOL wow64 = FALSE;

	using LPFN_ISWOW64PROCESS    = BOOL(WINAPI *)(HANDLE, PBOOL);
	static auto fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (fnIsWow64Process && fnIsWow64Process(hProcess_, &wow64)) {
		return wow64;
	}
#endif
	return false;
}

/**
 * @brief PlatformProcess::startTime
 * @return
 */
QDateTime PlatformProcess::startTime() const {
	Q_ASSERT(hProcess_);

	FILETIME create;
	FILETIME exit;
	FILETIME kernel;
	FILETIME user;

	if (GetProcessTimes(hProcess_, &create, &exit, &kernel, &user)) {

		ULARGE_INTEGER createTime;
		createTime.LowPart  = create.dwLowDateTime;
		createTime.HighPart = create.dwHighDateTime;
		return QDateTime::fromMSecsSinceEpoch(createTime.QuadPart / 10000);
	}

	return QDateTime();
}

/**
 * @brief PlatformProcess::pid
 * @return
 */
edb::pid_t PlatformProcess::pid() const {
	return GetProcessId(hProcess_);
}

/**
 * @brief PlatformProcess::name
 * @return
 */
QString PlatformProcess::name() const {

	PROCESSENTRY32 pEntry = {};
	getProcessEntry(pid(), &pEntry);

	QString name = QString::fromWCharArray(pEntry.szExeFile);
	if (isWow64()) {
		name += " *32";
	}
	return name;
}

/**
 * @brief PlatformProcess::user
 * @return
 */
QString PlatformProcess::user() const {

	QString user;

	HANDLE hToken;
	if (OpenProcessToken(hProcess_, TOKEN_QUERY, &hToken)) {

		DWORD needed;
		GetTokenInformation(hToken, TokenOwner, nullptr, 0, &needed);

		if (auto owner = static_cast<TOKEN_OWNER *>(std::malloc(needed))) {
			if (GetTokenInformation(hToken, TokenOwner, owner, needed, &needed)) {
				WCHAR user_buf[MAX_PATH];
				WCHAR domain[MAX_PATH];
				DWORD user_sz   = MAX_PATH;
				DWORD domain_sz = MAX_PATH;
				SID_NAME_USE snu;

				if (LookupAccountSid(nullptr, owner->Owner, user_buf, &user_sz, domain, &domain_sz, &snu) && snu == SidTypeUser) {
					user = QString::fromWCharArray(user_buf);
				}
			}
			std::free(owner);
		}

		CloseHandle(hToken);
	}
	return user;
}

/**
 * @brief PlatformProcess::parent
 * @return
 */
std::shared_ptr<IProcess> PlatformProcess::parent() const {
	edb::pid_t parent_pid = core_->parentPid(pid());
	return std::make_shared<PlatformProcess>(core_, parent_pid);
}

/**
 * @brief PlatformProcess::uid
 * @return
 */
edb::uid_t PlatformProcess::uid() const {
	Q_ASSERT(hProcess_);

	HANDLE token;
	TOKEN_USER user;
	DWORD length;

	// TODO(eteran): is this on the right track?
	if (OpenProcessToken(hProcess_, 0, &token)) {
		if (GetTokenInformation(token, TokenUser, &user, sizeof(user), &length)) {
		}
	}

	return 0;
}

/**
 * @brief PlatformProcess::patches
 * @return
 */
QMap<edb::address_t, Patch> PlatformProcess::patches() const {
	return patches_;
}

/**
 * @brief PlatformProcess::write_bytes
 * @param address
 * @param buf
 * @param len
 * @return
 */
std::size_t PlatformProcess::writeBytes(edb::address_t address, const void *buf, size_t len) {
	Q_ASSERT(buf);

	if (hProcess_) {
		if (len == 0) {
			return 0;
		}

		SIZE_T bytes_written = 0;
		if (WriteProcessMemory(hProcess_, reinterpret_cast<LPVOID>(address.toUint()), buf, len, &bytes_written)) {
			return bytes_written;
		}
	}
	return 0;
}

/**
 * @brief PlatformProcess::readBytes
 * @param address
 * @param buf
 * @param len
 * @return
 */
std::size_t PlatformProcess::readBytes(edb::address_t address, void *buf, size_t len) const {
	Q_ASSERT(buf);

	if (hProcess_) {
		if (len == 0) {
			return 0;
		}

		memset(buf, 0xff, len);
		SIZE_T bytes_read = 0;
		if (ReadProcessMemory(hProcess_, reinterpret_cast<LPCVOID>(address.toUint()), buf, len, &bytes_read)) {
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

/**
 * @brief PlatformProcess::readPages
 * @param address
 * @param buf
 * @param count
 * @return
 */
std::size_t PlatformProcess::readPages(edb::address_t address, void *buf, size_t count) const {
	Q_ASSERT(address % core_->pageSize() == 0);
	return readBytes(address, buf, core_->pageSize() * count);
}

/**
 * @brief PlatformProcess::pause
 * @return
 */
Status PlatformProcess::pause() {
	Q_ASSERT(hProcess_);
	if (DebugBreakProcess(hProcess_)) {
		return Status::Ok;
	}

	// TODO(eteran): use GetLastError/FormatMessage
	return Status("Failed to pause");
}

/**
 * @brief PlatformProcess::regions
 * @return
 */
QList<std::shared_ptr<IRegion>> PlatformProcess::regions() const {
	QList<std::shared_ptr<IRegion>> regions;

	if (hProcess_) {
		edb::address_t addr = 0;
		auto last_base      = reinterpret_cast<LPVOID>(-1);

		Q_FOREVER {
			MEMORY_BASIC_INFORMATION info;
			VirtualQueryEx(hProcess_, reinterpret_cast<LPVOID>(addr.toUint()), &info, sizeof(info));

			if (last_base == info.BaseAddress) {
				break;
			}

			last_base = info.BaseAddress;

			if (info.State == MEM_COMMIT) {

				const auto start                         = edb::address_t::fromZeroExtended(info.BaseAddress);
				const auto end                           = edb::address_t::fromZeroExtended(info.BaseAddress) + info.RegionSize;
				const auto base                          = edb::address_t::fromZeroExtended(info.AllocationBase);
				const QString name                       = QString();
				const IRegion::permissions_t permissions = info.Protect; // let std::shared_ptr<IRegion> handle permissions and modifiers

				if (info.Type == MEM_IMAGE) {
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

/**
 * @brief PlatformProcess::executable
 * @return
 */
QString PlatformProcess::executable() const {
	Q_ASSERT(hProcess_);

	// These functions don't work immediately after CreateProcess but only
	// after basic initialization, usually after the system breakpoint
	// The same applies to psapi/toolhelp, maybe using NtQueryXxxxxx is the way to go

	using QueryFullProcessImageNameWPtr = BOOL(WINAPI *)(
		HANDLE hProcess,
		DWORD dwFlags,
		LPWSTR lpExeName,
		PDWORD lpdwSize);

	HMODULE kernel32                    = GetModuleHandleW(L"kernel32.dll");
	auto QueryFullProcessImageNameWFunc = (QueryFullProcessImageNameWPtr)GetProcAddress(kernel32, "QueryFullProcessImageNameW");

	wchar_t name[MAX_PATH] = L"";

	if (QueryFullProcessImageNameWFunc /* && LOBYTE(GetVersion()) >= 6*/) { // Vista and up

		DWORD size = _countof(name);
		if (QueryFullProcessImageNameWFunc(hProcess_, 0, name, &size)) {
			return QString::fromWCharArray(name);
		}
	}

	return {};
}

/**
 * @brief PlatformProcess::arguments
 * @return
 */
QList<QByteArray> PlatformProcess::arguments() const {

	QList<QByteArray> ret;
	if (hProcess_) {

		using ZwQueryInformationProcessPtr = NTSTATUS (*WINAPI)(
			HANDLE ProcessHandle,
			PROCESSINFOCLASS ProcessInformationClass,
			PVOID ProcessInformation,
			ULONG ProcessInformationLength,
			PULONG ReturnLength);

		HMODULE ntdll                      = GetModuleHandleW(L"ntdll.dll");
		auto ZwQueryInformationProcessFunc = (ZwQueryInformationProcessPtr)GetProcAddress(ntdll, "NtQueryInformationProcess");
		PROCESS_BASIC_INFORMATION ProcessInfo;

		if (ZwQueryInformationProcessFunc) {
			ULONG l;
			NTSTATUS r = ZwQueryInformationProcessFunc(hProcess_, ProcessBasicInformation, &ProcessInfo, sizeof(PROCESS_BASIC_INFORMATION), &l);
			printf("TODO(eteran): implement this\n");
		}
	}
	return ret;
}

/**
 * @brief loadedModules
 * @return
 */
QList<Module> PlatformProcess::loadedModules() const {
	QList<Module> ret;
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid());
	if (hModuleSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me32;
		me32.dwSize = sizeof(me32);

		if (Module32First(hModuleSnap, &me32)) {
			do {
				Module module;
				module.baseAddress = edb::address_t::fromZeroExtended(me32.modBaseAddr);
				module.name        = QString::fromWCharArray(me32.szModule);
				ret.push_back(module);
			} while (Module32Next(hModuleSnap, &me32));
		}
	}
	CloseHandle(hModuleSnap);
	return ret;
}

/**
 * @brief PlatformProcess::threads
 * @return
 */
QList<std::shared_ptr<IThread>> PlatformProcess::threads() const {

	Q_ASSERT(core_->process_.get() == this);

	QList<std::shared_ptr<IThread>> threadList;

	for (auto &thread : core_->threads_) {
		threadList.push_back(thread);
	}

	return threadList;
}

/**
 * @brief PlatformProcess::currentThread
 * @return
 */
std::shared_ptr<IThread> PlatformProcess::currentThread() const {

	Q_ASSERT(core_->process_.get() == this);

	auto it = core_->threads_.find(core_->activeThread_);
	if (it != core_->threads_.end()) {
		return it.value();
	}
	return nullptr;
}

/**
 * @brief PlatformProcess::setCurrentThread
 * @param thread
 */
void PlatformProcess::setCurrentThread(IThread &thread) {
	core_->activeThread_ = static_cast<PlatformThread *>(&thread)->tid();
	edb::v1::update_ui();
}

Status PlatformProcess::step(edb::EventStatus status) {
	// TODO: assert that we are paused
	Q_ASSERT(core_->process_.get() == this);

	if (status != edb::DEBUG_STOP) {
		if (std::shared_ptr<IThread> thread = currentThread()) {
			return thread->step(status);
		}
	}
	return Status::Ok;
}

bool PlatformProcess::isPaused() const {
	for (auto &thread : threads()) {
		if (!thread->isPaused()) {
			return false;
		}
	}

	return true;
}

/**
 * @brief PlatformProcess::resume
 * @param status
 * @return
 */
Status PlatformProcess::resume(edb::EventStatus status) {

	int ret;

	switch (status) {
	case edb::EventStatus::DEBUG_CONTINUE:
		ret = ContinueDebugEvent(lastEvent_.dwProcessId, lastEvent_.dwThreadId, DBG_CONTINUE);
		break;
	case edb::EventStatus::DEBUG_EXCEPTION_NOT_HANDLED:
		ret = ContinueDebugEvent(lastEvent_.dwProcessId, lastEvent_.dwThreadId, DBG_EXCEPTION_NOT_HANDLED);
		break;
	case edb::EventStatus::DEBUG_NEXT_HANDLER:
	case edb::EventStatus::DEBUG_CONTINUE_BP:
	case edb::EventStatus::DEBUG_CONTINUE_STEP:
	case edb::EventStatus::DEBUG_STOP:
	default:
		break;
	}

	if (ret) {
		return Status::Ok;
	}

	return Status(QObject::tr("Error Continuing: %1").arg(GetLastError()));
}

}
