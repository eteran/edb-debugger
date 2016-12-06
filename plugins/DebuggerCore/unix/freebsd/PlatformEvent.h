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

#include <QCoreApplication>
#include "IDebugEvent.h"

namespace DebuggerCore {

class PlatformEvent : IDebugEvent {
	Q_DECLARE_TR_FUNCTIONS(PlatformEvent)
	friend class DebuggerCore;

public:
	PlatformEvent();

public:
	virtual PlatformEvent *clone() const;
	
public:
	virtual Message error_description() const;
	virtual REASON reason() const;
	virtual TRAP_REASON trap_reason() const;
	virtual bool exited() const;
	virtual bool is_error() const;
	virtual bool is_kill() const;
	virtual bool is_stop() const;
	virtual bool is_trap() const;
	virtual bool terminated() const;
	virtual bool stopped() const;
	virtual edb::pid_t process() const;
	virtual edb::tid_t thread() const;
	virtual int code() const;
	
private:
	int        status;
	edb::pid_t pid;
	edb::tid_t tid;
	void *     fault_address_;
	long       fault_code_;
};

}

#endif
