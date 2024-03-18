/*
Copyright (C) 2006 - 2023 Evan Teran
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

#ifndef IDEBUG_EVENT_H_20121005_
#define IDEBUG_EVENT_H_20121005_

#include "OSTypes.h"
#include <QString>

class IDebugEvent {
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
		Message() = default;
		Message(QString c, QString m, QString s)
			: caption(std::move(c)), message(std::move(m)), statusMessage(std::move(s)) {
		}

		QString caption;
		QString message;
		QString statusMessage;
	};

public:
	virtual ~IDebugEvent() = default;

public:
	[[nodiscard]] virtual IDebugEvent *clone() const = 0;

public:
	[[nodiscard]] virtual Message errorDescription() const = 0;
	[[nodiscard]] virtual REASON reason() const            = 0;
	[[nodiscard]] virtual TRAP_REASON trapReason() const   = 0;
	[[nodiscard]] virtual bool exited() const              = 0;
	[[nodiscard]] virtual bool isError() const             = 0;
	[[nodiscard]] virtual bool isKill() const              = 0;
	[[nodiscard]] virtual bool isStop() const              = 0;
	[[nodiscard]] virtual bool isTrap() const              = 0;
	[[nodiscard]] virtual bool stopped() const             = 0;
	[[nodiscard]] virtual bool terminated() const          = 0;
	[[nodiscard]] virtual edb::pid_t process() const       = 0;
	[[nodiscard]] virtual edb::tid_t thread() const        = 0;
	[[nodiscard]] virtual int64_t code() const             = 0;
};

#endif
