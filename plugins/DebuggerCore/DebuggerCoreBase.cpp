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
#include "Configuration.h"
#include "edb.h"
#include <QtDebug>

namespace DebuggerCorePlugin {

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
std::shared_ptr<IBreakpoint> DebuggerCoreBase::add_breakpoint(edb::address_t address) {

	try {
		if(attached()) {
			if(!find_breakpoint(address)) {
				auto bp = std::make_shared<Breakpoint>(address);
				breakpoints_[address] = bp;
				return bp;
			}
		}

		return nullptr;
	} catch(const breakpoint_creation_error &) {
		qDebug() << "Failed to create breakpoint";
		return nullptr;
	}
}

//------------------------------------------------------------------------------
// Name: find_breakpoint
// Desc: returns the breakpoint at the given address or std::shared_ptr<IBreakpoint>()
//------------------------------------------------------------------------------
std::shared_ptr<IBreakpoint> DebuggerCoreBase::find_breakpoint(edb::address_t address) {
	if(attached()) {
		auto it = breakpoints_.find(address);
		if(it != breakpoints_.end()) {
			return it.value();
		}
	}
	return nullptr;
}


//------------------------------------------------------------------------------
// Name: find_breakpoint
// Desc: similarly to find_breakpoint, finds a breakpoint near given address. But
// 		 unlike find_breakpoint, this function looks for a breakpoint which ends
// 		 up at this address after being triggered, instead of just starting there.
//------------------------------------------------------------------------------
std::shared_ptr<IBreakpoint> DebuggerCoreBase::find_triggered_breakpoint(edb::address_t address) {
	if(attached()) {
		for(const size_t size : Breakpoint::possible_rewind_sizes()) {
			const edb::address_t bpAddr = address - size;
			const std::shared_ptr<IBreakpoint> bp = find_breakpoint(bpAddr);

			if(bp && bp->address() == bpAddr) {
				return bp;
			}
		}
	}
	return nullptr;
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
// Name: end_debug_session
// Desc: Ends debug session, detaching from or killing debuggee according to
//		 user preferences
//------------------------------------------------------------------------------
void DebuggerCoreBase::end_debug_session() {
	if(attached()) {
        switch(edb::v1::config().close_behavior) {
		case Configuration::Detach:
			detach();
			break;
		case Configuration::Kill:
			kill();
			break;
		case Configuration::KillIfLaunchedDetachIfAttached:
			if(last_means_of_capture() == MeansOfCapture::Launch) {
				kill();
			} else {
				detach();
			}
			break;
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
// Name: attached
// Desc:
//------------------------------------------------------------------------------
bool DebuggerCoreBase::attached() const {
	return process() != nullptr;
}

auto DebuggerCoreBase::supported_breakpoint_types() const -> std::vector<IBreakpoint::BreakpointType> {
	return Breakpoint::supported_types();
}

}
