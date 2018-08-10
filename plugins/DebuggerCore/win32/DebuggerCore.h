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
	~DebuggerCore() ;

public:
	bool has_extension(quint64 ext) const ;
	edb::address_t page_size() const ;
	std::size_t pointer_size() const  {
		return sizeof(void*);
	};
	std::shared_ptr<IDebugEvent> wait_debug_event(int msecs) ;
	Status attach(edb::pid_t pid) ;
	Status detach() ;
	void kill() ;
	void pause() ;
	void resume(edb::EVENT_STATUS status) ;
	void step(edb::EVENT_STATUS status) ;
	void get_state(State *state) ;
	void set_state(const State &state) ;
	Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &tty) ;

	MeansOfCapture last_means_of_capture() const  {
		qDebug("TODO: Implement DebuggerCore::last_means_of_capture");
		return MeansOfCapture::NeverCaptured;
	}

	bool read_pages(edb::address_t address, void *buf, std::size_t count) ;
	bool read_bytes(edb::address_t address, void *buf, std::size_t len) ;
	bool write_bytes(edb::address_t address, const void *buf, std::size_t len) ;
	int sys_pointer_size() const ;
	QMap<qlonglong, QString> exceptions() const ;
	QString exceptionName(qlonglong value)  {
		qDebug("TODO: Implement DebuggerCore::exceptionName"); return "";
	}
	
	qlonglong exceptionValue(const QString &name)  {
		qDebug("TODO: Implement DebuggerCore::exceptionValue"); return 0;
	}

public:
	// thread support stuff (optional)
	QList<edb::tid_t> thread_ids() const    { return threads_.toList(); }
	edb::tid_t active_thread() const        { return active_thread_; }
	void set_active_thread(edb::tid_t tid)  { Q_ASSERT(threads_.contains(tid)); active_thread_ = tid; }

public:
	QList<std::shared_ptr<IRegion>> memory_regions() const;
	edb::address_t process_code_address() const;
	edb::address_t process_data_address() const;

public:
	// process properties
	QList<QByteArray> process_args(edb::pid_t pid) const ;
	QString process_exe(edb::pid_t pid) const ;
	QString process_cwd(edb::pid_t pid) const ;
	edb::pid_t parent_pid(edb::pid_t pid) const ;
	QDateTime process_start(edb::pid_t pid) const ;
	quint64 cpu_type() const ;

	CPUMode cpu_mode() const  {
		qDebug("TODO: Implement DebuggerCore::cpu_mode");
		return CPUMode::Unknown;
	}

public:
	IState *create_state() const ;

private:
	QMap<edb::pid_t, std::shared_ptr<IProcess> > enumerate_processes() const ;
	QList<Module> loaded_modules() const ;

public:
	QString stack_pointer() const ;
	QString frame_pointer() const ;
	QString instruction_pointer() const ;
	QString flag_register() const  {
		qDebug("TODO: Implement DebuggerCore::flag_register");
		return "";
	}

public:
	QString format_pointer(edb::address_t address) const ;

public:
	IProcess *process() const  {
		qDebug("TODO: Implement DebuggerCore::process");
		return nullptr;
	}

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
