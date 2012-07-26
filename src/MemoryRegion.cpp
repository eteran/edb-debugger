
#include "MemoryRegion.h"
#include "Debugger.h"
#include "IDebuggerCore.h"

MemoryRegion::MemoryRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, IRegion::permissions_t permissions) : impl_(edb::v1::debugger_core ? edb::v1::debugger_core->create_region(start, end, base, name, permissions) : 0) {
}

MemoryRegion::MemoryRegion() : impl_(0) {
}

MemoryRegion::~MemoryRegion() {
	delete impl_;
}

MemoryRegion::MemoryRegion(const MemoryRegion &other) : impl_(other.impl_ ? other.impl_->clone() : 0) {
}

MemoryRegion &MemoryRegion::operator=(const MemoryRegion &other) {
	if(this != &other) {
		MemoryRegion(other).swap(*this);
	}
	return *this;
}

void MemoryRegion::swap(MemoryRegion &other) {
	qSwap(impl_, other.impl_);
}

bool MemoryRegion::accessible() const {
	if(impl_) {
		return impl_->accessible();
	}
	return false;
}

bool MemoryRegion::readable() const {
	if(impl_) {
		return impl_->readable();
	}
	return false;
}

bool MemoryRegion::writable() const {
	if(impl_) {
		return impl_->writable();
	}
	return false;
}
	
bool MemoryRegion::executable() const {
	if(impl_) {
		return impl_->executable();
	}
	return false;
}

edb::address_t MemoryRegion::size() const {
	if(impl_) {
		return impl_->size();
	}
	return 0;
}

edb::address_t MemoryRegion::base() const {
	if(impl_) {
		return impl_->base();
	}
	return 0;
}

edb::address_t MemoryRegion::start() const {
	if(impl_) {
		return impl_->start();
	}
	return 0;
}

edb::address_t MemoryRegion::end() const {
	if(impl_) {
		return impl_->end();
	}
	return 0;
}

QString MemoryRegion::name() const {
	if(impl_) {
		return impl_->name();
	}
	return "";
}

IRegion::permissions_t MemoryRegion::permissions() const {
	if(impl_) {
		return impl_->permissions();
	}
	return 0;
}

void MemoryRegion::set_permissions(bool read, bool write, bool execute) {
	if(impl_) {
		impl_->set_permissions(read, write, execute);
	}
}

bool MemoryRegion::operator==(const MemoryRegion &rhs) const {
	if(impl_ && rhs.impl_) {
		return
			impl_->start()       == rhs.impl_->start() &&
			impl_->end()         == rhs.impl_->end() &&
			impl_->base()        == rhs.impl_->base() &&
			impl_->name()        == rhs.impl_->name() &&
			impl_->permissions() == rhs.impl_->permissions();
	}
	return false;
}

bool MemoryRegion::operator!=(const MemoryRegion &rhs) const {
	return !operator==(rhs);
}

bool MemoryRegion::contains(edb::address_t address) const {
	if(impl_) {
		return address >= impl_->start() && address <= impl_->end();
	}
	return false;
}
