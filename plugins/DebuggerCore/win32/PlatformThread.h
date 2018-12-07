#ifndef PLATFORM_THREAD_H
#define PLATFORM_THREAD_H

#include "IThread.h"
#include "IProcess.h"
#include "DebuggerCore.h"
#include <memory>
#include <QCoreApplication>

namespace DebuggerCorePlugin {

class PlatformThread : public IThread {
	Q_DECLARE_TR_FUNCTIONS(PlatformThread)
public:
	PlatformThread(DebuggerCore *core, std::shared_ptr<IProcess> &process, HANDLE hThread);
	~PlatformThread() override = default;

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
	Status stop() override;

public:
	bool isPaused() const override;

private:
	DebuggerCore *            core_;
	std::shared_ptr<IProcess> process_;
	HANDLE                    handle_;
};

}

#endif
