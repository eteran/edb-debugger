/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_EVENT_H_20121005_
#define PLATFORM_EVENT_H_20121005_

#include "IDebugEvent.h"
#include <QCoreApplication>

namespace DebuggerCore {

class PlatformEvent : IDebugEvent {
	Q_DECLARE_TR_FUNCTIONS(PlatformEvent)
	friend class DebuggerCore;

public:
	PlatformEvent();

public:
	PlatformEvent *clone() const override;

public:
	Message error_description() const override;
	REASON reason() const override;
	TRAP_REASON trap_reason() const override;
	bool exited() const override;
	bool is_error() const override;
	bool is_kill() const override;
	bool is_stop() const override;
	bool is_trap() const override;
	bool terminated() const override;
	bool stopped() const override;
	edb::pid_t process() const override;
	edb::tid_t thread() const override;
	int64_t code() const override;

private:
	int status;
	edb::pid_t pid;
	edb::tid_t tid;
	void *fault_address_;
	long fault_code_;
};

}

#endif
