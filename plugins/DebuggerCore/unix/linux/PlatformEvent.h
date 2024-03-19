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

#ifndef PLATFORM_EVENT_H_20121005_
#define PLATFORM_EVENT_H_20121005_

#include "IDebugEvent.h"

#include <QCoreApplication>
#include <csignal> // for the SIG* definitions

namespace DebuggerCorePlugin {

class PlatformEvent final : public IDebugEvent {
	Q_DECLARE_TR_FUNCTIONS(PlatformEvent)
	friend class DebuggerCore;

public:
	PlatformEvent() = default;

private:
	PlatformEvent(const PlatformEvent &)            = default;
	PlatformEvent &operator=(const PlatformEvent &) = default;

public:
	[[nodiscard]] IDebugEvent *clone() const override;

public:
	[[nodiscard]] bool exited() const override;
	[[nodiscard]] bool isError() const override;
	[[nodiscard]] bool isKill() const override;
	[[nodiscard]] bool isStop() const override;
	[[nodiscard]] bool isTrap() const override;
	[[nodiscard]] bool stopped() const override;
	[[nodiscard]] bool terminated() const override;
	[[nodiscard]] edb::pid_t process() const override;
	[[nodiscard]] edb::tid_t thread() const override;
	[[nodiscard]] int64_t code() const override;
	[[nodiscard]] Message errorDescription() const override;
	[[nodiscard]] REASON reason() const override;
	[[nodiscard]] TRAP_REASON trapReason() const override;

private:
	static IDebugEvent::Message createUnexpectedSignalMessage(const QString &name, int number);

private:
	siginfo_t siginfo_ = {};
	edb::pid_t pid_    = 0;
	edb::tid_t tid_    = 0;
	int status_        = 0;
};

}

#endif
