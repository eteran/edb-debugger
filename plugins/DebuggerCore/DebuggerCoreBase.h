/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DEBUGGER_CORE_BASE_H_20090529_
#define DEBUGGER_CORE_BASE_H_20090529_

#include "IDebugger.h"

class Status;

namespace DebuggerCorePlugin {

class DebuggerCoreBase : public QObject, public IDebugger {
public:
	~DebuggerCoreBase() override = default;

public:
	enum class MeansOfCapture {
		NeverCaptured,
		Attach,
		Launch
	};

	[[nodiscard]] virtual MeansOfCapture lastMeansOfCapture() const = 0;

public:
	[[nodiscard]] BreakpointList backupBreakpoints() const override;
	std::shared_ptr<IBreakpoint> addBreakpoint(edb::address_t address) override;
	std::shared_ptr<IBreakpoint> findBreakpoint(edb::address_t address) override;
	std::shared_ptr<IBreakpoint> findTriggeredBreakpoint(edb::address_t address) override;
	void clearBreakpoints() override;
	void removeBreakpoint(edb::address_t address) override;
	void endDebugSession() override;

	[[nodiscard]] std::vector<IBreakpoint::BreakpointType> supportedBreakpointTypes() const override;

protected:
	[[nodiscard]] bool attached() const;

protected:
	BreakpointList breakpoints_;
};

}

#endif
