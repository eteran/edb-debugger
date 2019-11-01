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

#ifndef DEBUGGERCORE_20090529_H_
#define DEBUGGERCORE_20090529_H_

#include <QObject>
#include "DebuggerCoreBase.h"
#include <QHash>
#include <QSet>
#include <csignal>
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
	std::size_t pointerSize() const override;
	size_t pageSize() const override;
	bool hasExtension(quint64 ext) const override;
	std::shared_ptr<IDebugEvent> waitDebugEvent(int msecs) override;
	Status attach(edb::pid_t pid) override;
	Status detach() override;
	void kill() override;
	Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) override;
    MeansOfCapture lastMeansOfCapture() const override;
	void setIgnoredExceptions(const QList<qlonglong> &exceptions) override;
	uint8_t nopFillByte() const override;

public:
	QMap<qlonglong, QString> exceptions() const override;
	QString exceptionName(qlonglong value) override;
	qlonglong exceptionValue(const QString &name) override;

public:
	edb::pid_t parentPid(edb::pid_t pid) const override;

public:
	std::unique_ptr<IState> createState() const override;

public:
	quint64 cpuType() const override;

private:
	QMap<edb::pid_t, std::shared_ptr<IProcess>> enumerateProcesses() const override;

public:
	QString stackPointer() const override;
	QString framePointer() const override;
	QString instructionPointer() const override;
	QString flagRegister() const override;

public:
	IProcess *process() const override;

private:
	Status ptrace_getsiginfo(edb::tid_t tid, siginfo_t *siginfo);
	Status ptrace_continue(edb::tid_t tid, long status);
	Status ptrace_step(edb::tid_t tid, long status);
	Status ptrace_set_options(edb::tid_t tid, long options);
	Status ptrace_get_event_message(edb::tid_t tid, unsigned long *message);
	long ptrace_traceme();

private:
	void reset();
	Status stop_threads();
	std::shared_ptr<IDebugEvent> handle_event(edb::tid_t tid, int status);
	std::shared_ptr<IDebugEvent> handle_thread_create_event(edb::tid_t tid, int status);
	void handle_thread_exit(edb::tid_t tid, int status);
	int attach_thread(edb::tid_t tid);
	void detectCpuMode();
    long ptraceOptions() const;

private:
	using threads_type = QHash<edb::tid_t, std::shared_ptr<PlatformThread>>;

private:
	// TODO(eteran): a few of these logically belong in PlatformProcess...
	QList<qlonglong>          ignoredExceptions_;
	threads_type              threads_;
	QSet<edb::tid_t>          waitedThreads_;
	edb::tid_t                activeThread_;
	std::shared_ptr<IProcess> process_;
	std::size_t               pointerSize_ = sizeof(void*);
#if defined(EDB_X86) || defined(EDB_X86_64)
	const bool                edbIsIn64BitSegment_;
	const bool                osIs64Bit_;
	const edb::seg_reg_t      userCodeSegment32_;
	const edb::seg_reg_t      userCodeSegment64_;
	const edb::seg_reg_t      userStackSegment_;
#endif
	CpuMode					  cpuMode_              = CpuMode::Unknown;
	MeansOfCapture	          lastMeansOfCapture_     = MeansOfCapture::NeverCaptured;
	bool                      procMemWriteBroken_ = true;
	bool                      procMemReadBroken_  = true;
};

}

#endif
