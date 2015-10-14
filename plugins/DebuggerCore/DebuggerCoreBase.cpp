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

#include "DebuggerCoreBase.h"
#include "Breakpoint.h"

namespace DebuggerCore {

//------------------------------------------------------------------------------
// Name: DebuggerCoreBase
// Desc: constructor
//------------------------------------------------------------------------------
DebuggerCoreBase::DebuggerCoreBase() : pid_(0) {
}

//------------------------------------------------------------------------------
// Name: ~DebuggerCoreBase
// Desc: destructor
//------------------------------------------------------------------------------
DebuggerCoreBase::~DebuggerCoreBase() {
}

//------------------------------------------------------------------------------
// Name: clear_breakpoints
// Desc: removes all breakpoints
//------------------------------------------------------------------------------
void DebuggerCoreBase::clear_breakpoints() {
	if(attached()) {
		breakpoints_.clear();
	}
}

//------------------------------------------------------------------------------
// Name: add_breakpoint
// Desc: creates a new breakpoint
//       (only if there isn't already one at the given address)
//------------------------------------------------------------------------------
IBreakpoint::pointer DebuggerCoreBase::add_breakpoint(edb::address_t address) {

	if(attached()) {
		if(!find_breakpoint(address)) {
			IBreakpoint::pointer bp(new Breakpoint(address));
			breakpoints_[address] = bp;
			return bp;
		}
	}

	return IBreakpoint::pointer();
}

//------------------------------------------------------------------------------
// Name: find_breakpoint
// Desc: returns the breakpoint at the given address or IBreakpoint::pointer()
//------------------------------------------------------------------------------
IBreakpoint::pointer DebuggerCoreBase::find_breakpoint(edb::address_t address) {
	if(attached()) {
		auto it = breakpoints_.find(address);
		if(it != breakpoints_.end()) {
			return it.value();
		}
	}
	return IBreakpoint::pointer();
}


//------------------------------------------------------------------------------
// Name: remove_breakpoint
// Desc: removes the breakpoint at the given address, this is a no-op if there
//       is no breakpoint present.
// Note: if another part of the code has a reference to the BP, it will not
//       actually be removed until it releases the reference.
//------------------------------------------------------------------------------
void DebuggerCoreBase::remove_breakpoint(edb::address_t address) {

	// TODO(eteran): assert paused
	if(attached()) {
		auto it = breakpoints_.find(address);
		if(it != breakpoints_.end()) {
			breakpoints_.erase(it);
		}
	}
}

//------------------------------------------------------------------------------
// Name: backup_breakpoints
// Desc: returns a copy of the BP list, these count as references to the BPs
//       preventing full removal until this list is destructed.
//------------------------------------------------------------------------------
DebuggerCoreBase::BreakpointList DebuggerCoreBase::backup_breakpoints() const {
	return breakpoints_;
}

//------------------------------------------------------------------------------
// Name: open
// Desc: executes the given program
//------------------------------------------------------------------------------
bool DebuggerCoreBase::open(const QString &path, const QString &cwd, const QList<QByteArray> &args) {
	return open(path, cwd, args, QString());
}

//------------------------------------------------------------------------------
// Name: pid
// Desc: returns the pid of the currently debugged process (0 if not attached)
//------------------------------------------------------------------------------
edb::pid_t DebuggerCoreBase::pid() const {
	return pid_;
}

//------------------------------------------------------------------------------
// Name: attached
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCoreBase::attached() const {
	return pid() != 0;
}

}
