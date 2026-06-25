/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Creates a new PlatformRegion instance with the specified parameters.
 *
 * @param start The starting address of the memory region.
 * @param end The ending address of the memory region.
 * @param base The base address of the memory region.
 * @param name The name of the memory region.
 * @param permissions The permissions of the memory region.
 */
PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, QString name, permissions_t permissions)
	: start_(start), end_(end), base_(base), name_(std::move(name)), permissions_(permissions) {
}

/**
 * @brief Clones the current PlatformRegion instance.
 *
 * @return A pointer to a new PlatformRegion instance that is a copy of the current instance.
 */
IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

/**
 * @brief Gets whether the current PlatformRegion instance is accessible, meaning it has at least one of the read, write, or execute permissions.
 *
 * @return true if the region is accessible, false otherwise.
 */
bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

/**
 * @brief Gets whether the current PlatformRegion instance is readable, meaning it has read permissions.
 *
 * @return true if the region is readable, false otherwise.
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
 * @brief Gets whether the current PlatformRegion instance is writable, meaning it has write permissions.
 *
 * @return true if the region is writable, false otherwise.
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
 * @brief Gets whether the current PlatformRegion instance is executable, meaning it has execute permissions.
 *
 * @return true if the region is executable, false otherwise.
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
 * @brief Gets the size of the current PlatformRegion instance.
 *
 * @return The size of the region.
 */
size_t PlatformRegion::size() const {
	return end_ - start_;
}

/**
 * @brief Sets the permissions of the current PlatformRegion instance.
 *
 * @param read Whether the region should be readable.
 * @param write Whether the region should be writable.
 * @param execute Whether the region should be executable.
 */
void PlatformRegion::setPermissions(bool read, bool write, bool execute) {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (HANDLE ph = OpenProcess(PROCESS_VM_OPERATION, FALSE, process->pid())) {
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
}

/**
 * @brief Gets the starting address of the current PlatformRegion instance.
 *
 * @return The starting address of the region.
 */
edb::address_t PlatformRegion::start() const {
	return start_;
}

/**
 * @brief Gets the ending address of the current PlatformRegion instance.
 *
 * @return The ending address of the region.
 */
edb::address_t PlatformRegion::end() const {
	return end_;
}

/**
 * @brief Gets the base address of the current PlatformRegion instance.

 * @return The base address of the region.
 */
edb::address_t PlatformRegion::base() const {
	return base_;
}

/**
 * @brief Gets the name of the current PlatformRegion instance.
 *
 * @return The name of the region.
 */
QString PlatformRegion::name() const {
	return name_;
}

/**
 * @brief Gets the permissions of the current PlatformRegion instance.
 *
 * @return The permissions of the region.
 */
IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

/**
 * @brief Sets the starting address of the current PlatformRegion instance.
 *
 * @param address The starting address of the region.
 */
void PlatformRegion::setStart(edb::address_t address) {
	start_ = address;
}

/**
 * @brief Sets the ending address of the current PlatformRegion instance.
 *
 * @param address The ending address of the region.
 */
void PlatformRegion::setEnd(edb::address_t address) {
	end_ = address;
}

}
