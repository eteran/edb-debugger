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

#include "PlatformRegion.h"

#include "IDebugEventHandler.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "MemoryRegions.h"
#include "State.h"
#include "edb.h"
#include <QMessageBox>

namespace DebuggerCorePlugin {

namespace {

constexpr IRegion::permissions_t KnownPermissions = (PAGE_NOACCESS | PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);

}

/**
 * @brief PlatformRegion::PlatformRegion
 * @param start
 * @param end
 * @param base
 * @param name
 * @param permissions
 */
PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, permissions_t permissions)
	: start_(start), end_(end), base_(base), name_(name), permissions_(permissions) {
}

/**
 * @brief PlatformRegion::clone
 * @return
 */
IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

/**
 * @brief PlatformRegion::accessible
 * @return
 */
bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

/**
 * @brief PlatformRegion::readable
 * @return
 */
bool PlatformRegion::readable() const {
	switch (permissions_ & KnownPermissions) { // ignore modifiers
	case PAGE_EXECUTE_READ:
	case PAGE_EXECUTE_READWRITE:
	case PAGE_READONLY:
	case PAGE_READWRITE:
		return true;
	default:
		return false;
	}
}

/**
 * @brief PlatformRegion::writable
 * @return
 */
bool PlatformRegion::writable() const {
	switch (permissions_ & KnownPermissions) { // ignore modifiers
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY:
	case PAGE_READWRITE:
	case PAGE_WRITECOPY:
		return true;
	default:
		return false;
	}
}

/**
 * @brief PlatformRegion::executable
 * @return
 */
bool PlatformRegion::executable() const {
	switch (permissions_ & KnownPermissions) { // ignore modifiers
	case PAGE_EXECUTE:
	case PAGE_EXECUTE_READ:
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY:
		return true;
	default:
		return false;
	}
}

/**
 * @brief PlatformRegion::size
 * @return
 */
size_t PlatformRegion::size() const {
	return end_ - start_;
}

/**
 * @brief PlatformRegion::setPermissions
 * @param read
 * @param write
 * @param execute
 */
void PlatformRegion::setPermissions(bool read, bool write, bool execute) {
	if (HANDLE ph = OpenProcess(PROCESS_VM_OPERATION, FALSE, edb::v1::debugger_core->process()->pid())) {
		DWORD prot = PAGE_NOACCESS;

		switch ((static_cast<int>(read) << 2) | (static_cast<int>(write) << 1) | (static_cast<int>(execute) << 0)) {
		case 0x0:
			prot = PAGE_NOACCESS;
			break;
		case 0x1:
			prot = PAGE_EXECUTE;
			break;
		case 0x2:
			prot = PAGE_WRITECOPY;
			break;
		case 0x3:
			prot = PAGE_EXECUTE_WRITECOPY;
			break;
		case 0x4:
			prot = PAGE_READONLY;
			break;
		case 0x5:
			prot = PAGE_EXECUTE_READ;
			break;
		case 0x6:
			prot = PAGE_READWRITE;
			break;
		case 0x7:
			prot = PAGE_EXECUTE_READWRITE;
			break;
		}

		prot |= permissions_ & ~KnownPermissions; // keep modifiers

		DWORD prev_prot;
		if (VirtualProtectEx(ph, reinterpret_cast<LPVOID>(start().toUint()), size(), prot, &prev_prot)) {
			permissions_ = prot;
		}

		CloseHandle(ph);
	}
}

/**
 * @brief PlatformRegion::start
 * @return
 */
edb::address_t PlatformRegion::start() const {
	return start_;
}

/**
 * @brief PlatformRegion::end
 * @return
 */
edb::address_t PlatformRegion::end() const {
	return end_;
}

/**
 * @brief PlatformRegion::base
 * @return
 */
edb::address_t PlatformRegion::base() const {
	return base_;
}

/**
 * @brief PlatformRegion::name
 * @return
 */
QString PlatformRegion::name() const {
	return name_;
}

/**
 * @brief PlatformRegion::permissions
 * @return
 */
IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

/**
 * @brief PlatformRegion::setStart
 * @param address
 */
void PlatformRegion::setStart(edb::address_t address) {
	start_ = address;
}

/**
 * @brief PlatformRegion::setEnd
 * @param address
 */
void PlatformRegion::setEnd(edb::address_t address) {
	end_ = address;
}

}
