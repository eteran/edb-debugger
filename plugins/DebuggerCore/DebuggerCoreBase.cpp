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

/**
 * removes all breakpoints
 *
 * @brief DebuggerCoreBase::clearBreakpoints
 */
void DebuggerCoreBase::clearBreakpoints() {
	if (attached()) {
		breakpoints_.clear();
	}
}

/**
 * creates a new breakpoint (only if there isn't already one at the given address)
 *
 * @brief DebuggerCoreBase::addBreakpoint
 * @param address
 * @return the breakpoint which was created/found
 */
std::shared_ptr<IBreakpoint> DebuggerCoreBase::addBreakpoint(edb::address_t address) {

	try {
		if (attached()) {
			if (std::shared_ptr<IBreakpoint> bp = findBreakpoint(address)) {
				return bp;
			}

			auto bp               = std::make_shared<Breakpoint>(address);
			breakpoints_[address] = bp;
			return bp;
		}

		return nullptr;
	} catch (const BreakpointCreationError &) {
		qDebug() << "Failed to create breakpoint";
		return nullptr;
	}
}

/**
 * @brief DebuggerCoreBase::findBreakpoint
 * @param address
 * @return the breakpoint at the given address or std::shared_ptr<IBreakpoint>()
 */
std::shared_ptr<IBreakpoint> DebuggerCoreBase::findBreakpoint(edb::address_t address) {
	if (attached()) {
		auto it = breakpoints_.find(address);
		if (it != breakpoints_.end()) {
			return it.value();
		}
	}
	return nullptr;
}

/**
 * similarly to findBreakpoint, finds a breakpoint near given address. But
 * unlike findBreakpoint, this function looks for a breakpoint which ends
 * up at this address after being triggered, instead of just starting there.
 *
 * @brief DebuggerCoreBase::findTriggeredBreakpoint
 * @param address
 * @return
 */
std::shared_ptr<IBreakpoint> DebuggerCoreBase::findTriggeredBreakpoint(edb::address_t address) {
	if (attached()) {
		for (const size_t size : Breakpoint::possibleRewindSizes()) {
			const edb::address_t bpAddr           = address - size;
			const std::shared_ptr<IBreakpoint> bp = findBreakpoint(bpAddr);

			if (bp && bp->address() == bpAddr) {
				return bp;
			}
		}
	}
	return nullptr;
}

/**
 * Decrements the reference count for the breakpoint found at the given address.
 * If the refernce count goes to zero, then it is removed.
 * This is a no-op if there is no breakpoint present.
 *
 * @brief DebuggerCoreBase::removeBreakpoint
 * @param address
 */
void DebuggerCoreBase::removeBreakpoint(edb::address_t address) {

	// TODO(eteran): assert paused
	if (attached()) {
		auto it = breakpoints_.find(address);
		if (it != breakpoints_.end()) {
			breakpoints_.erase(it);
		}
	}
}

/**
 * Ends debug session, detaching from or killing debuggee according to user preferences
 *
 * @brief DebuggerCoreBase::endDebugSession
 */
void DebuggerCoreBase::endDebugSession() {
	if (attached()) {
		switch (edb::v1::config().close_behavior) {
		case Configuration::Detach:
			detach();
			break;
		case Configuration::Kill:
			kill();
			break;
		case Configuration::KillIfLaunchedDetachIfAttached:
			if (lastMeansOfCapture() == MeansOfCapture::Launch) {
				kill();
			} else {
				detach();
			}
			break;
		}
	}
}

/**
 * returns a copy of the BP list, these count as references to the BPs
 * preventing full removal until this list is destructed.
 *
 * @brief DebuggerCoreBase::backupBreakpoints
 * @return a list of shared_ptr's to the BPs
 */
DebuggerCoreBase::BreakpointList DebuggerCoreBase::backupBreakpoints() const {
	return breakpoints_;
}

/**
 * @brief DebuggerCoreBase::attached
 * @return
 */
bool DebuggerCoreBase::attached() const {
	return process() != nullptr;
}

/**
 * @brief DebuggerCoreBase::supportedBreakpointTypes
 * @return
 */
std::vector<IBreakpoint::BreakpointType> DebuggerCoreBase::supportedBreakpointTypes() const {
	return Breakpoint::supportedTypes();
}

}
