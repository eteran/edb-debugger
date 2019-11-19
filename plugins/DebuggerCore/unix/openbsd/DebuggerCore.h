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
	~DebuggerCore() override;

public:
	size_t page_size() const override;
	bool has_extension(quint64 ext) const override;
	std::shared_ptr<const IDebugEvent> wait_debug_event(int msecs) override;
	bool attach(edb::pid_t pid) override;
	void detach() override;
	void kill() override;
	void pause() override;
	void resume(edb::EVENT_STATUS status) override;
	void step(edb::EVENT_STATUS status) override;
	void get_state(State *state) override;
	void set_state(const State &state) override;
	bool open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) override;

public:
	// thread support stuff (optional)
	QList<edb::tid_t> thread_ids() const override { return threads_.keys(); }
	edb::tid_t active_thread() const override { return active_thread_; }
	void set_active_thread(edb::tid_t) override;

public:
	QList<std::shared_ptr<IRegion>> memory_regions() const override;
	edb::address_t process_code_address() const override;
	edb::address_t process_data_address() const override;

public:
	std::unique_ptr<IState> create_state() const override;

public:
	// process properties
	QList<QByteArray> process_args(edb::pid_t pid) const override;
	QString process_exe(edb::pid_t pid) const override;
	QString process_cwd(edb::pid_t pid) const override;
	edb::pid_t parent_pid(edb::pid_t pid) const override;
	QDateTime process_start(edb::pid_t pid) const override;
	quint64 cpu_type() const override;

private:
	QMap<edb::pid_t, ProcessInfo> enumerate_processes() const override;
	QList<Module> loaded_modules() const override;

public:
	QString stack_pointer() const override;
	QString frame_pointer() const override;
	QString instruction_pointer() const override;

public:
	QString format_pointer(edb::address_t address) const override;

private:
	long read_data(edb::address_t address, bool *ok) override;
	bool write_data(edb::address_t address, long value) override;

private:
	struct thread_info {
	public:
		thread_info()
			: status(0) {
		}

		thread_info(int s)
			: status(s) {
		}

		int status;
	};

	using threadmap_t = QHash<edb::tid_t, thread_info>;

	size_t page_size_;
	threadmap_t threads_;
};

}

#endif
