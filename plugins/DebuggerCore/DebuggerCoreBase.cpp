/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DebuggerCoreBase.h"
#include "Breakpoint.h"
#include "Configuration.h"
#include "edb.h"
#include <QtDebug>

namespace DebuggerCorePlugin {

/**
 * @brief Removes all breakpoints if a process is currently attached.
 *
 * removes all breakpoints
 *
 */
void DebuggerCoreBase::clearBreakpoints() {
	if (attached()) {
		breakpoints_.clear();
	}
}

/**
 * @brief Creates a new breakpoint at the given address, or returns the existing one if already present.
 *
 * creates a new breakpoint (only if there isn't already one at the given address)
 *
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
 * @brief Returns the breakpoint at the given address, or an empty shared_ptr if none exists.
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
 * @brief Finds the breakpoint that triggered at the given address by checking possible rewind sizes.
 *
 * similarly to findBreakpoint, finds a breakpoint near given address. But
 * unlike findBreakpoint, this function looks for a breakpoint which ends
 * up at this address after being triggered, instead of just starting there.
 *
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
 * @brief Removes the breakpoint at the given address; this is a no-op if no breakpoint exists there.
 *
 * Decrements the reference count for the breakpoint found at the given address.
 * If the reference count goes to zero, then it is removed.
 * This is a no-op if there is no breakpoint present.
 *
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
 * @brief Ends the debug session by detaching from or killing the debuggee according to user preferences.
 *
 * Ends debug session, detaching from or killing debuggee according to user preferences
 *
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
 * @brief Returns a copy of the breakpoint map, keeping shared_ptr references alive until it is destroyed.
 *
 * returns a copy of the BP list, these count as references to the BPs
 * preventing full removal until this list is destructed.
 *
 * @return a list of shared_ptr's to the BPs
 */
DebuggerCoreBase::BreakpointList DebuggerCoreBase::backupBreakpoints() const {
	return breakpoints_;
}

/**
 * @brief Returns true if a process is currently attached for debugging.
 * @return
 */
bool DebuggerCoreBase::attached() const {
	return process() != nullptr;
}

/**
 * @brief Returns the list of all software breakpoint types supported on x86/x86-64.
 * @return
 */
std::vector<IBreakpoint::BreakpointType> DebuggerCoreBase::supportedBreakpointTypes() const {
	return Breakpoint::supportedTypes();
}

}
