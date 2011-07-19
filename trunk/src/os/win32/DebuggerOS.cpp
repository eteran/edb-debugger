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

#include "Debugger.h"
#include "DebuggerCoreInterface.h"
#include "MemoryRegions.h"
#include <QTextStream>
#include <tlhelp32.h>
#include <windows.h>
#include <psapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "Psapi.lib")
#endif

//------------------------------------------------------------------------------
// Name: primary_code_region()
// Desc:
//------------------------------------------------------------------------------
MemRegion edb::v1::primary_code_region() {
	MemRegion region;
	memory_regions().sync();
	if(memory_regions().find_region(0, region)) {
		return region;
	}

	return MemRegion();
}

//------------------------------------------------------------------------------
// Name: loaded_libraries()
// Desc:
//------------------------------------------------------------------------------
QStringList edb::v1::loaded_libraries() {
	QStringList ret;
	edb::pid_t pid = edb::v1::debugger_core->pid();
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if(hModuleSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me32;

		me32.dwSize = sizeof(me32);

		if(!Module32First(hModuleSnap, &me32)) {
			do {
				ret << QString::fromWCharArray(me32.szModule);
			} while(Module32Next(hModuleSnap, &me32));
		}
	}
	CloseHandle(hModuleSnap);
	return ret;
}

//------------------------------------------------------------------------------
// Name: get_process_exe()
// Desc:
//------------------------------------------------------------------------------
QString edb::v1::get_process_exe() {
	QString ret;
	edb::pid_t pid = edb::v1::debugger_core->pid();

	/*
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, pid);
	if(hProcessSnap != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32W pe32;
		pe32.dwSize = sizeof(pe32);

		if(Process32FirstW(hProcessSnap, &pe32)) {
			do {
				if(pid == pe32.th32ProcessID) {
					ret = QString::fromWCharArray(pe32.szExeFile);
					break;
				}
			} while(Process32NextW(hProcessSnap, &pe32));
		}
		CloseHandle(hProcessSnap);
	}
	*/

	HANDLE ph = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
	if(ph != 0) {
		wchar_t name[MAX_PATH] = L"";

		// fails right here
		/*
		if(GetModuleFileNameExW(ph, NULL, name, _countof(name))) {
			ret = QString::fromWCharArray(name);
		}
		*/

		static HMODULE kernel32 = LoadLibrary(L"kernel32.dll");

		typedef BOOL (WINAPI *QueryFullProcessImageNamePtr)(
		  HANDLE hProcess,
		  DWORD dwFlags,
		  LPTSTR lpExeName,
		  PDWORD lpdwSize
		);

		static QueryFullProcessImageNamePtr QueryFullProcessImageNameFunc = 0;

		if(kernel32 != 0 && QueryFullProcessImageNameFunc == 0) {
			QueryFullProcessImageNameFunc = (QueryFullProcessImageNamePtr)GetProcAddress(kernel32, "QueryFullProcessImageNameW");
		}

		if(QueryFullProcessImageNameFunc) {
			DWORD size = MAX_PATH;
			if(QueryFullProcessImageNameFunc(ph, 0, name, &size)) {
				ret = QString::fromWCharArray(name);
			}
		}

		CloseHandle(ph);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: get_process_cwd()
// Desc:
//------------------------------------------------------------------------------
QString edb::v1::get_process_cwd() {
	QString ret;
	return ret;
}

//------------------------------------------------------------------------------
// Name: get_process_args()
// Desc:
//------------------------------------------------------------------------------
QStringList edb::v1::get_process_args() {
	// TODO: http://wj32.wordpress.com/2009/01/24/howto-get-the-command-line-of-processes/
	QStringList ret;
	return ret;
}

//------------------------------------------------------------------------------
// Name: get_parent_pid(edb::pid_t pid)
// Desc:
//------------------------------------------------------------------------------
edb::pid_t edb::v1::get_parent_pid(edb::pid_t pid) {
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
