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
#include <QHash>

namespace DebuggerCore {

class DebuggerCore : public DebuggerCoreUNIX {
	Q_OBJECT
	Q_INTERFACES(IDebugger)
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
	// process properties
	virtual QList<QByteArray> process_args(edb::pid_t pid) const;
	virtual QString process_exe(edb::pid_t pid) const;
	virtual QString process_cwd(edb::pid_t pid) const;
	virtual edb::pid_t parent_pid(edb::pid_t pid) const;
	virtual QDateTime process_start(edb::pid_t pid) const;
	virtual quint64 cpu_type() const;

public:
	virtual IState *create_state() const;

private:
	virtual QMap<edb::pid_t, ProcessInfo> enumerate_processes() const;
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
	struct thread_info {
	public:
		thread_info() : status(0) {
		}

		thread_info(int s) : status(s) {
		}

		int status;
	};

	typedef QHash<edb::tid_t, thread_info> threadmap_t;

	edb::address_t page_size_;
	threadmap_t    threads_;
};

}

#endif
