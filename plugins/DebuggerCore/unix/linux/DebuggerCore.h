/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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
#include <QHash>
#include <QSet>
#include <csignal>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <unistd.h>

// These system calls were added in Linux 3.2. Support is provided in glibc since version 2.15.

class IBinary;

namespace DebuggerCore {

class DebuggerCore : public DebuggerCoreUNIX {
	Q_OBJECT
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IDebuggerCore/1.0")
#endif
	Q_INTERFACES(IDebuggerCore)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	DebuggerCore();
	virtual ~DebuggerCore();

public:
	virtual edb::address_t page_size() const;
	virtual bool has_extension(quint64 ext) const;
	virtual IDebugEvent::const_pointer wait_debug_event(int msecs);
	virtual bool attach(edb::pid_t pid);
	virtual void detach();
	virtual void kill();
	virtual void pause();
	virtual void resume(edb::EVENT_STATUS status);
	virtual void step(edb::EVENT_STATUS status);
	virtual void get_state(State *state);
	virtual void set_state(const State &state);
	virtual bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty);
	virtual bool read_pages(edb::address_t address, void *buf, std::size_t count);

public:
	// thread support stuff (optional)
	virtual QList<edb::tid_t> thread_ids() const { return threads_.keys(); }
	virtual edb::tid_t active_thread() const     { return active_thread_; }
	virtual void set_active_thread(edb::tid_t);

public:
	virtual QList<IRegion::pointer> memory_regions() const;
	virtual edb::address_t process_code_address() const;
	virtual edb::address_t process_data_address() const;

public:
	virtual IState *create_state() const;
	virtual QWidget *create_register_view() const;

public:
	// process properties
	virtual QList<QByteArray> process_args(edb::pid_t pid) const;
	virtual QString process_cwd(edb::pid_t pid) const;
	virtual QString process_exe(edb::pid_t pid) const;
	virtual edb::pid_t parent_pid(edb::pid_t pid) const;
	virtual QDateTime process_start(edb::pid_t pid) const;
	virtual quint64 cpu_type() const;


public:
#if 0
#ifdef __NR_process_vm_readv
	virtual bool read_bytes(edb::address_t address, void *buf, std::size_t len);        // TODO: remind me why these aren't const...
#endif

#ifdef __NR_process_vm_writev
	virtual bool write_bytes(edb::address_t address, const void *buf, std::size_t len); // TODO: remind me why these aren't const...
#endif
#endif

private:
	virtual QMap<edb::pid_t, Process> enumerate_processes() const;
	virtual QList<Module> loaded_modules() const;

public:
	virtual QString stack_pointer() const;
	virtual QString frame_pointer() const;
	virtual QString instruction_pointer() const;

public:
	virtual QString format_pointer(edb::address_t address) const;

private:
	virtual long read_data(edb::address_t address, bool *ok);
	virtual bool write_data(edb::address_t address, long value);

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
	bool attach_thread(edb::tid_t tid);

private:
	struct thread_info {
		int status;
		enum {
			THREAD_STOPPED,
			THREAD_SIGNALED
		} state;
	};

	typedef QHash<edb::tid_t, thread_info> threadmap_t;

	edb::address_t   page_size_;
	threadmap_t      threads_;
	QSet<edb::tid_t> waited_threads_;
	edb::tid_t       event_thread_;
	IBinary          *binary_info_;
};

}

#endif
