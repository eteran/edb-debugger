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

#ifndef PLATFORM_EVENT_H_20121005_
#define PLATFORM_EVENT_H_20121005_

#include "IDebugEvent.h"
#include <QCoreApplication>

namespace DebuggerCorePlugin {

class PlatformEvent : public IDebugEvent {
	Q_DECLARE_TR_FUNCTIONS(PlatformEvent)
	friend class DebuggerCore;

public:
	PlatformEvent() = default;

public:
	PlatformEvent *clone() const override;

public:
	Message errorDescription() const override;
	REASON reason() const override;
	TRAP_REASON trapReason() const override;
	bool exited() const override;
	bool isError() const override;
	bool isKill() const override;
	bool isStop() const override;
	bool isTrap() const override;
	bool terminated() const override;
	bool stopped() const override;
	edb::pid_t process() const override;
	edb::tid_t thread() const override;
	int64_t code() const override;

private:
	DEBUG_EVENT event_ = {};
};

}

#endif
