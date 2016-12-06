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

#ifndef PLATFORM_EVENT_20121005_H_
#define PLATFORM_EVENT_20121005_H_

#include "IDebugEvent.h"

#include <QCoreApplication>
#include <signal.h> // for the SIG* definitions

namespace DebuggerCore {

class PlatformEvent : public IDebugEvent {
	Q_DECLARE_TR_FUNCTIONS(PlatformEvent)
	friend class DebuggerCore;

public:
	PlatformEvent();
	
private:
	PlatformEvent(const PlatformEvent &) = default;
	PlatformEvent& operator=(const PlatformEvent &) = default;

public:
	virtual IDebugEvent *clone() const override;

public:
	virtual Message error_description() const override;
	virtual REASON reason() const override;
	virtual TRAP_REASON trap_reason() const override;
	virtual bool exited() const override;
	virtual bool is_error() const override;
	virtual bool is_kill() const override;
	virtual bool is_stop() const override;
	virtual bool is_trap() const override;
	virtual bool stopped() const override;
	virtual bool terminated() const override;
	virtual edb::pid_t process() const override;
	virtual edb::tid_t thread() const override;
	virtual int code() const override;

private:
	static IDebugEvent::Message createUnexpectedSignalMessage(const QString &name, int number);

private:
	siginfo_t  siginfo_;
	edb::pid_t pid_;
	edb::tid_t tid_;
	int        status_;
};

}

#endif
