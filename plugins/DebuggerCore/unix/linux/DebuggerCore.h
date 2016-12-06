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

#include "DebuggerCoreUNIX.h"
#include "PlatformState.h"
#include "PlatformThread.h"
#include <QHash>
#include <QSet>
#include <csignal>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <unistd.h>

class IBinary;

namespace DebuggerCore {

class DebuggerCore : public DebuggerCoreUNIX {
	Q_OBJECT
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IDebugger/1.0")
#endif
	Q_INTERFACES(IDebugger)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")
	friend class PlatformProcess;
	friend class PlatformThread;
	
public:
	DebuggerCore();
	virtual ~DebuggerCore() override;

public:
	virtual std::size_t pointer_size() const override;
	virtual edb::address_t page_size() const override;
	virtual bool has_extension(quint64 ext) const override;
	virtual IDebugEvent::const_pointer wait_debug_event(int msecs) override;
	virtual QString attach(edb::pid_t pid) override;
	virtual void detach() override;
	virtual void kill() override;
	virtual void get_state(State *state) override;
	virtual void set_state(const State &state) override;
	virtual QString open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) override;
	virtual MeansOfCapture last_means_of_capture() override;
	
public:
	virtual edb::pid_t parent_pid(edb::pid_t pid) const override;

public:
	virtual IState *create_state() const override;

public:
	virtual quint64 cpu_type() const override;

private:
	virtual QMap<edb::pid_t, IProcess::pointer> enumerate_processes() const override;

public:
	virtual QString stack_pointer() const override;
	virtual QString frame_pointer() const override;
	virtual QString instruction_pointer() const override;
	virtual QString flag_register() const override;

public:
	virtual QString format_pointer(edb::address_t address) const override;
	
public:
	virtual IProcess *process() const override;

private:
	long ptrace_getsiginfo(edb::tid_t tid, siginfo_t *siginfo);
	long ptrace_continue(edb::tid_t tid, long status);
	long ptrace_step(edb::tid_t tid, long status);
	long ptrace_set_options(edb::tid_t tid, long options);
	long ptrace_get_event_message(edb::tid_t tid, unsigned long *message);
	long ptrace_traceme();

private:
	void reset();
	void stop_threads();
	IDebugEvent::const_pointer handle_event(edb::tid_t tid, int status);
	int attach_thread(edb::tid_t tid);
	void detectDebuggeeBitness();
	
private:
	typedef QHash<edb::tid_t, PlatformThread::pointer> threadmap_t;

private:
	threadmap_t              threads_;
	QSet<edb::tid_t>         waited_threads_;
	edb::tid_t               active_thread_;
	std::unique_ptr<IBinary> binary_info_;
	IProcess                *process_;
	std::size_t              pointer_size_;
	const bool               edbIsIn64BitSegment;
	const bool               osIs64Bit;
	const edb::seg_reg_t     USER_CS_32;
	const edb::seg_reg_t     USER_CS_64;
	const edb::seg_reg_t     USER_SS;
	MeansOfCapture	         lastMeansOfCapture = MeansOfCapture::NeverCaptured;
	bool                     proc_mem_write_broken_;
	bool                     proc_mem_read_broken_;
};

}

#endif
