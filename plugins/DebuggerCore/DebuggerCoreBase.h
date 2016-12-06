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

#ifndef DEBUGGERCOREBASE_20090529_H_
#define DEBUGGERCOREBASE_20090529_H_

#include "IDebugger.h"

namespace DebuggerCore {

class DebuggerCoreBase : public QObject, public IDebugger {
public:
	DebuggerCoreBase();
	virtual ~DebuggerCoreBase() override;

public:
	virtual QString open(const QString &path, const QString &cwd, const QList<QByteArray> &args) override;
	virtual QString open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) override = 0;
	enum class MeansOfCapture
	{
		NeverCaptured,
		Attach,
		Launch
	};
	virtual MeansOfCapture last_means_of_capture() = 0;

public:
	virtual BreakpointList backup_breakpoints() const override;
	virtual IBreakpoint::pointer add_breakpoint(edb::address_t address) override;
	virtual IBreakpoint::pointer find_breakpoint(edb::address_t address) override;
	virtual void clear_breakpoints() override;
	virtual void remove_breakpoint(edb::address_t address) override;
	virtual void end_debug_session() override;

public:
	virtual edb::pid_t pid() const;

protected:
	bool attached() const;

protected:
	edb::pid_t      pid_;
	BreakpointList  breakpoints_;
};

}

#endif
