/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
