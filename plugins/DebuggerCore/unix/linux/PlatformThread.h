/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_THREAD_H_20151013_
#define PLATFORM_THREAD_H_20151013_

#include "IBreakpoint.h"
#include "IThread.h"
#include <QCoreApplication>
#include <memory>

class IProcess;

namespace DebuggerCorePlugin {

class DebuggerCore;
class PlatformState;

class PlatformThread final : public IThread {
	Q_DECLARE_TR_FUNCTIONS(PlatformThread)
	friend class DebuggerCore;

public:
	PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, edb::tid_t tid);
	~PlatformThread() override                        = default;
	PlatformThread(const PlatformThread &)            = delete;
	PlatformThread &operator=(const PlatformThread &) = delete;

public:
	[[nodiscard]] edb::tid_t tid() const override;
	[[nodiscard]] QString name() const override;
	[[nodiscard]] int priority() const override;
	[[nodiscard]] edb::address_t instructionPointer() const override;
	[[nodiscard]] QString runState() const override;

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
	void fillSegmentBases(PlatformState *state);
	bool fillStateFromPrStatus(PlatformState *state);
	bool fillStateFromSimpleRegs(PlatformState *state);
#if defined(EDB_ARM32)
	bool fillStateFromVFPRegs(PlatformState *state);
#endif

private:
	[[nodiscard]] unsigned long getDebugRegister(std::size_t n) const;
	long setDebugRegister(std::size_t n, unsigned long value) const;

private:
	DebuggerCore *core_ = nullptr;
	std::shared_ptr<IProcess> process_;
	edb::tid_t tid_;
	int status_ = 0;

#if defined(EDB_ARM32) || defined(EDB_ARM64)
private:
	Status doStep(edb::tid_t tid, long status);
	std::shared_ptr<IBreakpoint> singleStepBreakpoint;
#endif
};

}

#endif
