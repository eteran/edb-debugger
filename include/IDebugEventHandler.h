/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IDEBUG_EVENT_HANDLER_H_20061101_
#define IDEBUG_EVENT_HANDLER_H_20061101_

#include "Types.h"
#include <memory>

class IDebugEvent;

class IDebugEventHandler {
public:
	virtual ~IDebugEventHandler() = default;

public:
	virtual edb::EventStatus handleEvent(const std::shared_ptr<IDebugEvent> &event) = 0;
};

#endif
