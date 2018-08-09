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

#include "DebuggerCoreBase.h"
#include "IRegion.h"
#include "Module.h"
#include <QSet>

namespace DebuggerCorePlugin {

class DebuggerCore : public DebuggerCoreBase {
	Q_OBJECT
    Q_PLUGIN_METADATA(IID "edb.IDebugger/1.0")
	Q_INTERFACES(IDebugger)
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	DebuggerCore();
	~DebuggerCore() override;

public:
	virtual bool has_extension(quint64 ext) const;
	virtual edb::address_t page_size() const;
	virtual std::size_t pointer_size() const {
		return sizeof(void*);
	};
	virtual std::shared_ptr<IDebugEvent> wait_debug_event(int msecs);
	virtual Status attach(edb::pid_t pid);
	virtual Status detach();
	virtual void kill();
	virtual void pause();
	virtual void resume(edb::EVENT_STATUS status);
	virtual void step(edb::EVENT_STATUS status);
	virtual void get_state(State *state);
	virtual void set_state(const State &state);
	virtual Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty);
	virtual MeansOfCapture last_means_of_capture() const {
		qDebug("TODO: Implement DebuggerCore::last_means_of_capture"); return MeansOfCapture::NeverCaptured;
	};
	virtual bool read_pages(edb::address_t address, void *buf, std::size_t count);
	virtual bool read_bytes(edb::address_t address, void *buf, std::size_t len);
	virtual bool write_bytes(edb::address_t address, const void *buf, std::size_t len);
	virtual int sys_pointer_size() const;
	virtual QMap<qlonglong, QString> exceptions() const;
	virtual QString exceptionName(qlonglong value) {
		qDebug("TODO: Implement DebuggerCore::exceptionName"); return "";
	};
	virtual qlonglong exceptionValue(const QString &name) {
		qDebug("TODO: Implement DebuggerCore::exceptionValue"); return 0;
	}

public:
	// thread support stuff (optional)
	virtual QList<edb::tid_t> thread_ids() const   { return threads_.toList(); }
	virtual edb::tid_t active_thread() const       { return active_thread_; }
	virtual void set_active_thread(edb::tid_t tid) { Q_ASSERT(threads_.contains(tid)); active_thread_ = tid; }

public:
	virtual QList<std::shared_ptr<IRegion>> memory_regions() const;
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
	virtual CPUMode cpu_mode() const {
		qDebug("TODO: Implement DebuggerCore::cpu_mode"); return CPUMode::Unknown;
	};

public:
	virtual IState *create_state() const;

private:
	virtual QMap<edb::pid_t, std::shared_ptr<IProcess> > enumerate_processes() const;
	virtual QList<Module> loaded_modules() const;

public:
	virtual QString stack_pointer() const;
	virtual QString frame_pointer() const;
	virtual QString instruction_pointer() const;
	virtual QString flag_register() const {
		qDebug("TODO: Implement DebuggerCore::flag_register"); return "";
	};

public:
	virtual QString format_pointer(edb::address_t address) const;

public:
	virtual IProcess *process() const {
		qDebug("TODO: Implement DebuggerCore::process"); return nullptr;
	};

public:
	// NOTE: win32 only stuff here!
	edb::address_t start_address;
	edb::address_t image_base;

private:
	bool attached() { return DebuggerCoreBase::attached() && process_handle_ != 0; }

private:
	edb::address_t   page_size_;
	HANDLE           process_handle_;
	QSet<edb::tid_t> threads_;
	edb::tid_t       active_thread_;
};

}

#endif
