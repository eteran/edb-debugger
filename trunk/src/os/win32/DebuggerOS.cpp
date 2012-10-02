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
#include "IDebuggerCore.h"
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
MemoryRegion edb::v1::primary_code_region() {
	MemoryRegion region;
	memory_regions().sync();
	if(memory_regions().find_region(0, region)) {
		return region;
	}

	return MemoryRegion();
}

//------------------------------------------------------------------------------
// Name: loaded_libraries()
// Desc:
//------------------------------------------------------------------------------
QList<Module> edb::v1::loaded_libraries() {

	QList<Module> ret;
	edb::pid_t pid = edb::v1::debugger_core->pid();
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if(hModuleSnap != INVALID_HANDLE_VALUE) {
		MODULEENTRY32 me32;

		me32.dwSize = sizeof(me32);

		if(!Module32First(hModuleSnap, &me32)) {
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
