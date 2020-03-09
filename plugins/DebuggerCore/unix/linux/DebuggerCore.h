/*
Copyright (C) 2006 - 2015 Evan Teran
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

#ifndef DEBUGGER_CORE_H_20090529_
#define DEBUGGER_CORE_H_20090529_

#include "DebuggerCoreBase.h"
#include <QHash>
#include <QObject>
#include <csignal>
#include <set>
#include <unistd.h>

class IBinary;
class Status;

namespace DebuggerCorePlugin {

class PlatformThread;

class DebuggerCore final : public DebuggerCoreBase {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "edb.IDebugger/1.0")
	Q_INTERFACES(IDebugger)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")
	friend class PlatformProcess;
	friend class PlatformThread;

	CpuMode cpuMode() const override { return cpuMode_; }

public:
	DebuggerCore();
	~DebuggerCore() override;

public:
	MeansOfCapture lastMeansOfCapture() const override;
	Status attach(edb::pid_t pid) override;
	Status detach() override;
	Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &input, const QString &output) override;
	bool hasExtension(uint64_t ext) const override;
	size_t pageSize() const override;
	std::shared_ptr<IDebugEvent> waitDebugEvent(std::chrono::milliseconds msecs) override;
	std::size_t pointerSize() const override;
	uint8_t nopFillByte() const override;
	void kill() override;
	void setIgnoredExceptions(const QList<qlonglong> &exceptions) override;

public:
	QMap<qlonglong, QString> exceptions() const override;
	QString exceptionName(qlonglong value) override;
	qlonglong exceptionValue(const QString &name) override;

public:
	edb::pid_t parentPid(edb::pid_t pid) const override;

public:
	std::unique_ptr<IState> createState() const override;

public:
	uint64_t cpuType() const override;

private:
	QMap<edb::pid_t, std::shared_ptr<IProcess>> enumerateProcesses() const override;

public:
	QString flagRegister() const override;
	QString framePointer() const override;
	QString instructionPointer() const override;
	QString stackPointer() const override;

public:
	IProcess *process() const override;

private:
	Status ptraceContinue(edb::tid_t tid, long status);
	Status ptraceGetEventMessage(edb::tid_t tid, unsigned long *message);
	Status ptraceGetSigInfo(edb::tid_t tid, siginfo_t *siginfo);
	Status ptraceSetOptions(edb::tid_t tid, long options);
	Status ptraceStep(edb::tid_t tid, long status);
	long ptraceTraceme();

private:
	Status stopThreads();
	int attachThread(edb::tid_t tid);
	long ptraceOptions() const;
	std::shared_ptr<IDebugEvent> handleEvent(edb::tid_t tid, int status);
	std::shared_ptr<IDebugEvent> handleThreadCreate(edb::tid_t tid, int status);
	void detectCpuMode();
	void handleThreadExit(edb::tid_t tid, int status);
	void reset();

private:
	using threads_type = QHash<edb::tid_t, std::shared_ptr<PlatformThread>>;

private:
	// TODO(eteran): a few of these logically belong in PlatformProcess...
	CpuMode cpuMode_                   = CpuMode::Unknown;
	MeansOfCapture lastMeansOfCapture_ = MeansOfCapture::NeverCaptured;
	QList<qlonglong> ignoredExceptions_;
	std::set<edb::tid_t> waitedThreads_;
	edb::tid_t activeThread_;
	std::shared_ptr<IProcess> process_;
	threads_type threads_;
	bool procMemReadBroken_  = true;
	bool procMemWriteBroken_ = true;
	std::size_t pointerSize_ = sizeof(void *);
#if defined(EDB_X86) || defined(EDB_X86_64)
	const bool osIs64Bit_;
	const edb::seg_reg_t userCodeSegment32_;
	const edb::seg_reg_t userCodeSegment64_;
	const edb::seg_reg_t userStackSegment_;
#endif
};

}

#endif
