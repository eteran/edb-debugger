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

#ifndef IDEBUGEVENT_20121005_H_
#define IDEBUGEVENT_20121005_H_

#include "API.h"
#include "Types.h"
#include <QString>
#include <memory>

class EDB_EXPORT IDebugEvent {
public:
	typedef std::shared_ptr<IDebugEvent>       pointer;
	typedef std::shared_ptr<const IDebugEvent> const_pointer;

public:
	enum REASON {
		EVENT_EXITED,     // exited normally
		EVENT_TERMINATED, // terminated by event
		EVENT_STOPPED,    // normal event
		EVENT_UNKNOWN
	};

	enum TRAP_REASON {
		TRAP_STEPPING,
		TRAP_BREAKPOINT
	};

	struct Message {
		Message(const QString &c, const QString &m) : caption(c), message(m) {
		}

		Message() {
		}

		QString caption;
		QString message;
	};
	
public:
	virtual ~IDebugEvent() {}
	
public:
	virtual IDebugEvent *clone() const = 0;

public:
	virtual Message error_description() const = 0;
	virtual REASON reason() const = 0;
	virtual TRAP_REASON trap_reason() const = 0;
	virtual bool exited() const = 0;
	virtual bool is_error() const = 0;
	virtual bool is_kill() const = 0;
	virtual bool is_stop() const = 0;
	virtual bool is_trap() const = 0;
	virtual bool stopped() const = 0;
	virtual bool terminated() const = 0;
	virtual edb::pid_t process() const = 0;
	virtual edb::tid_t thread() const = 0;
	virtual int code() const = 0;
};

#endif
