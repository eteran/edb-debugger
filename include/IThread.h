/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ITHREAD_H_20150529_
#define ITHREAD_H_20150529_

#include "OSTypes.h"
#include "Status.h"
#include "Types.h"

class State;

class IThread {
public:
	virtual ~IThread() = default;

public:
	[[nodiscard]] virtual edb::tid_t tid() const                    = 0;
	[[nodiscard]] virtual QString name() const                      = 0;
	[[nodiscard]] virtual int priority() const                      = 0;
	[[nodiscard]] virtual edb::address_t instructionPointer() const = 0;
	[[nodiscard]] virtual QString runState() const                  = 0;

public:
	virtual void getState(State *state)       = 0;
	virtual void setState(const State &state) = 0;

public:
	virtual Status step()                          = 0;
	virtual Status step(edb::EventStatus status)   = 0;
	virtual Status resume()                        = 0;
	virtual Status resume(edb::EventStatus status) = 0;

public:
	[[nodiscard]] virtual bool isPaused() const = 0;
};

#endif
