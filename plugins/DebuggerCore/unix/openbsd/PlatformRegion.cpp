/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PlatformRegion.h"

#include "IDebugEventHandler.h"
#include "IDebugger.h"
#include "MemoryRegions.h"
#include "State.h"
#include "edb.h"
#include <QMessageBox>
#include <sys/mman.h>
#include <sys/syscall.h>

namespace DebuggerCore {

PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, QString name, permissions_t permissions)
	: start_(start), end_(end), base_(base), name_(std::move(name)), permissions_(permissions) {
}

IRegion *PlatformRegion::clone() const {
	return new PlatformRegion(start_, end_, base_, name_, permissions_);
}

bool PlatformRegion::accessible() const {
	return readable() || writable() || executable();
}

bool PlatformRegion::readable() const {
	return (permissions_ & PROT_READ) != 0;
}

bool PlatformRegion::writable() const {
	return (permissions_ & PROT_WRITE) != 0;
}

bool PlatformRegion::executable() const {
	return (permissions_ & PROT_EXEC) != 0;
}

size_t PlatformRegion::size() const {
	return end_ - start_;
}

void PlatformRegion::set_permissions(bool read, bool write, bool execute) {
	Q_UNUSED(read)
	Q_UNUSED(write)
	Q_UNUSED(execute)
}

edb::address_t PlatformRegion::start() const {
	return start_;
}

edb::address_t PlatformRegion::end() const {
	return end_;
}

edb::address_t PlatformRegion::base() const {
	return base_;
}

QString PlatformRegion::name() const {
	return name_;
}

IRegion::permissions_t PlatformRegion::permissions() const {
	return permissions_;
}

void PlatformRegion::set_start(edb::address_t address) {
	start_ = address;
}

void PlatformRegion::set_end(edb::address_t address) {
	end_ = address;
}

}
