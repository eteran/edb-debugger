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

namespace DebuggerCorePlugin {

class DebuggerCore : public DebuggerCoreBase {
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "edb.IDebugger/1.0")
	Q_INTERFACES(IDebugger)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")
	friend class PlatformProcess;
	friend class PlatformThread;

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
	MeansOfCapture lastMeansOfCapture() const override;
	void set_ignored_exceptions(const QList<qlonglong> &exceptions) override;

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
	virtual long read_data(edb::address_t address, bool *ok);
	virtual bool write_data(edb::address_t address, long value);

private:
	struct thread_info {
	public:
		thread_info() = default;
		thread_info(int s)
			: status(s) {
		}

		int status = 0;
	};

	using threadmap_t = QHash<edb::tid_t, thread_info>;

	edb::address_t page_size_;
	threadmap_t threads_;
};

}

#endif
