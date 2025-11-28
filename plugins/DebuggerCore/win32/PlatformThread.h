/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_THREAD_H_20191119_
#define PLATFORM_THREAD_H_20191119_

#include "DebuggerCore.h"
#include "IProcess.h"
#include "IThread.h"
#include <QCoreApplication>
#include <memory>

namespace DebuggerCorePlugin {

class PlatformThread : public IThread {
	Q_DECLARE_TR_FUNCTIONS(PlatformThread)
public:
	PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, const CREATE_THREAD_DEBUG_INFO *info);
	~PlatformThread() override = default;

public:
	edb::tid_t tid() const override;
	QString name() const override;
	int priority() const override;
	edb::address_t instructionPointer() const override;
	QString runState() const override;

public:
	void getState(State *state) override;
	void setState(const State &state) override;

public:
	Status step() override;
	Status step(edb::EventStatus status) override;
	Status resume() override;
	Status resume(edb::EventStatus status) override;

public:
	[[nodiscard]] bool isPaused() const override;

private:
	DebuggerCore *core_ = nullptr;
	std::shared_ptr<IProcess> process_;
	HANDLE hThread_ = nullptr;
	bool isWow64_   = false;
};

}

#endif
