/*
Copyright (C) 2015 - 2015 Evan Teran
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
	~PlatformThread() override             = default;
	PlatformThread(const PlatformThread &) = delete;
	PlatformThread &operator=(const PlatformThread &) = delete;

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
	bool isPaused() const override;

private:
	void fillSegmentBases(PlatformState *state);
	bool fillStateFromPrStatus(PlatformState *state);
	bool fillStateFromSimpleRegs(PlatformState *state);
#if defined(EDB_ARM32)
	bool fillStateFromVFPRegs(PlatformState *state);
#endif

private:
	unsigned long getDebugRegister(std::size_t n);
	long setDebugRegister(std::size_t n, unsigned long value);

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
