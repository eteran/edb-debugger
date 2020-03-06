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

	virtual MeansOfCapture lastMeansOfCapture() const = 0;

public:
	BreakpointList backupBreakpoints() const override;
	std::shared_ptr<IBreakpoint> addBreakpoint(edb::address_t address) override;
	std::shared_ptr<IBreakpoint> findBreakpoint(edb::address_t address) override;
	std::shared_ptr<IBreakpoint> findTriggeredBreakpoint(edb::address_t address) override;
	void clearBreakpoints() override;
	void removeBreakpoint(edb::address_t address) override;
	void endDebugSession() override;

	std::vector<IBreakpoint::BreakpointType> supportedBreakpointTypes() const override;

protected:
	bool attached() const;

protected:
	BreakpointList breakpoints_;
};

}

#endif
