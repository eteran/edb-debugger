/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
