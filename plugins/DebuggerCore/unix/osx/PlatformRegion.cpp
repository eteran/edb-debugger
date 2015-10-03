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

#include "MemoryRegions.h"
#include "edb.h"
#include "IDebugger.h"
#include "State.h"
#include "IDebugEventHandler.h"
#include <QMessageBox>
#include <sys/syscall.h>
#include <sys/mman.h>

namespace DebuggerCore {

PlatformRegion::PlatformRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, permissions_t permissions) : start_(start), end_(end), base_(base), name_(name), permissions_(permissions) {
}

PlatformRegion::~PlatformRegion() {
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

edb::address_t PlatformRegion::size() const {
	return end_ - start_;
}

void PlatformRegion::set_permissions(bool read, bool write, bool execute) {
	Q_UNUSED(read);
	Q_UNUSED(write);
	Q_UNUSED(execute);
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
