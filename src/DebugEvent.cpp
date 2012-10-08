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

#include "DebugEvent.h"
#include "Debugger.h"
#include "IDebuggerCore.h"

//------------------------------------------------------------------------------
// Name: DebugEvent()
// Desc: constructor
//------------------------------------------------------------------------------
DebugEvent::DebugEvent() : impl_(edb::v1::debugger_core ? edb::v1::debugger_core->create_event() : 0) {
}

//------------------------------------------------------------------------------
// Name: ~DebugEvent()
// Desc:
//------------------------------------------------------------------------------
DebugEvent::~DebugEvent() {
	delete impl_;
}

//------------------------------------------------------------------------------
// Name: DebugEvent(const DebugEvent &other)
// Desc:
//------------------------------------------------------------------------------
DebugEvent::DebugEvent(const DebugEvent &other) : impl_(other.impl_ ? other.impl_->clone() : 0) {
}

//------------------------------------------------------------------------------
// Name: swap(DebugEvent &other)
// Desc:
//------------------------------------------------------------------------------
void DebugEvent::swap(DebugEvent &other) {
	qSwap(impl_, other.impl_);
}

//------------------------------------------------------------------------------
// Name: operator=(const DebugEvent &other)
// Desc:
//------------------------------------------------------------------------------
DebugEvent &DebugEvent::operator=(const DebugEvent &other) {
	if(this != &other) {
		DebugEvent(other).swap(*this);
	}
	return *this;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
IDebugEvent::Message DebugEvent::error_description() const {
	if(impl_) {
		return impl_->error_description();
	}
	return IDebugEvent::Message();
	
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
IDebugEvent::REASON DebugEvent::reason() const {
	if(impl_) {
		return impl_->reason();
	}
	return IDebugEvent::EVENT_UNKNOWN;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
IDebugEvent::TRAP_REASON DebugEvent::trap_reason() const {
	if(impl_) {
		return impl_->trap_reason();
	}
	return IDebugEvent::TRAP_STEPPING;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::exited() const {
	if(impl_) {
		return impl_->exited();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::is_error() const {
	if(impl_) {
		return impl_->is_error();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::is_kill() const {
	if(impl_) {
		return impl_->is_kill();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::is_stop() const {
	if(impl_) {
		return impl_->is_stop();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::is_trap() const {
	if(impl_) {
		return impl_->is_trap();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::terminated() const {
	if(impl_) {
		return impl_->terminated();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
bool DebugEvent::stopped() const {
	if(impl_) {
		return impl_->stopped();
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
edb::pid_t DebugEvent::process() const {
	if(impl_) {
		return impl_->process();
	}
	return static_cast<edb::pid_t>(0);
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
edb::tid_t DebugEvent::thread() const {
	if(impl_) {
		return impl_->thread();
	}
	return static_cast<edb::tid_t>(0);
}

//------------------------------------------------------------------------------
// Name: 
// Desc:
//------------------------------------------------------------------------------
int DebugEvent::code() const {
	if(impl_) {
		return impl_->code();
	}
	return 0;
}
