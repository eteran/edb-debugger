/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TYPES_H_20071127_
#define TYPES_H_20071127_

#include "Value.h"
#include <QString>

namespace edb {

enum EventStatus {
	DEBUG_STOP,                  // do nothing, the UI will instigate the next event
	DEBUG_CONTINUE,              // the event has been addressed, continue as normal
	DEBUG_CONTINUE_STEP,         // the event has been addressed, step as normal
	DEBUG_CONTINUE_BP,           // the event was a BP, which we need to ignore
	DEBUG_EXCEPTION_NOT_HANDLED, // pass the event unmodified back thread and continue
	DEBUG_NEXT_HANDLER
};

}

/* Comment Type */
struct Comment {
	edb::address_t address;
	QString comment;
};

#include "ArchTypes.h"
#endif
