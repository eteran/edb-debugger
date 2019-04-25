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

	CPUMode cpu_mode() const override { return cpu_mode_; }
public:
	DebuggerCore();
	~DebuggerCore() override;

public:
	std::size_t pointer_size() const override;
	size_t page_size() const override;
	bool has_extension(quint64 ext) const override;
	std::shared_ptr<IDebugEvent> wait_debug_event(int msecs) override;
	Status attach(edb::pid_t pid) override;
	Status detach() override;
	void kill() override;
	Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) override;
    MeansOfCapture last_means_of_capture() const override;
	void set_ignored_exceptions(const QList<qlonglong> &exceptions) override;
	uint8_t nopFillByte() const override;

public:
	QMap<qlonglong, QString> exceptions() const override;
	QString exceptionName(qlonglong value) override;
	qlonglong exceptionValue(const QString &name) override;

public:
	edb::pid_t parent_pid(edb::pid_t pid) const override;

public:
	std::unique_ptr<IState> create_state() const override;

public:
	quint64 cpu_type() const override;

private:
	QMap<edb::pid_t, std::shared_ptr<IProcess>> enumerate_processes() const override;

public:
	QString stack_pointer() const override;
	QString frame_pointer() const override;
	QString instruction_pointer() const override;
	QString flag_register() const override;

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
    void detectCPUMode();
    long ptraceOptions() const;

private:
	using threads_type = QHash<edb::tid_t, std::shared_ptr<PlatformThread>>;

private:
	// TODO(eteran): a few of these logically belong in PlatformProcess...
	QList<qlonglong>          ignored_exceptions_;
	threads_type              threads_;
	QSet<edb::tid_t>          waited_threads_;
	edb::tid_t                active_thread_;
	std::shared_ptr<IProcess> process_;
	std::size_t               pointer_size_ = sizeof(void*);
#if defined(EDB_X86) || defined(EDB_X86_64)
	const bool                edbIsIn64BitSegment;
	const bool                osIs64Bit;
	const edb::seg_reg_t      USER_CS_32;
	const edb::seg_reg_t      USER_CS_64;
	const edb::seg_reg_t      USER_SS;
#endif
	CPUMode					  cpu_mode_              = CPUMode::Unknown;
	MeansOfCapture	          lastMeansOfCapture     = MeansOfCapture::NeverCaptured;
	bool                      proc_mem_write_broken_ = true;
	bool                      proc_mem_read_broken_  = true;
};

}

#endif
