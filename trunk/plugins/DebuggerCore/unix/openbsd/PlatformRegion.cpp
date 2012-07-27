
#include "PlatformRegion.h"
#include "MemoryRegion.h"
#include "MemoryRegions.h"
#include "Debugger.h"
#include "IDebuggerCore.h"
#include "State.h"
#include "IDebugEventHandler.h"
#include <QMessageBox>
#include <sys/syscall.h>
#include <sys/mman.h>


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

