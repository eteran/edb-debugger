/*
 * Copyright (C) 2018- 2025 Evan Teran
 *                          evan.teran@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_THREAD_H_20181225_
#define PLATFORM_THREAD_H_20181225_

#include "IBreakpoint.h"
#include "IThread.h"
#include <QCoreApplication>
#include <memory>

class IProcess;

namespace DebuggerCorePlugin {

class DebuggerCore;
class PlatformState;

class PlatformThread : public IThread {
	Q_DECLARE_TR_FUNCTIONS(PlatformThread)
	friend class DebuggerCore;
	friend class PlatformProcess;

public:
	edb::tid_t tid() const override;
	QString name() const override;
	int priority() const override;
	edb::address_t instruction_pointer() const override;
	QString runState() const override;

public:
	void get_state(State *state) override;
	void set_state(const State &state) override;

public:
	Status step() override;
	Status step(edb::EVENT_STATUS status) override;
	Status resume() override;
	Status resume(edb::EVENT_STATUS status) override;

public:
	bool isPaused() const override;

private:
	DebuggerCore *core_ = nullptr;
	std::shared_ptr<IProcess> process_;
	edb::tid_t tid_;
	int status_ = 0;
};

}

#endif
