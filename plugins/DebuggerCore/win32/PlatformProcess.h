/*
Copyright (C) 2015 - 2015 Evan Teran
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


#ifndef PLATOFORM_PROCESS_20150517_H_
#define PLATOFORM_PROCESS_20150517_H_

#include "IProcess.h"
#include "Module.h"

#include <QDateTime>

#include <TlHelp32.h>

namespace DebuggerCorePlugin {

class PlatformProcess : public IProcess {

public:
	PlatformProcess(const PROCESSENTRY32& pe);

public:
	// legal to call when not attached
	QDateTime start_time() const override {
		qDebug("TODO: implement PlatformProcess::start_time"); return QDateTime();
	}

	QList<QByteArray> arguments() const override {
		qDebug("TODO: implement PlatformProcess::arguments"); return QList<QByteArray>();
	}

	QString current_working_directory() const override {
		qDebug("TODO: implement PlatformProcess::current_working_directory"); return "";
	}

	QString executable() const override {
		qDebug("TODO: implement PlatformProcess::executable"); return "";
	}

	edb::pid_t pid() const override;

	std::shared_ptr<IProcess> parent() const override {
		qDebug("TODO: implement PlatformProcess::parent"); return std::shared_ptr<IProcess>();
	}

	edb::address_t code_address() const override {
		qDebug("TODO: implement PlatformProcess::code_address"); return edb::address_t();
	}

	edb::address_t data_address() const override {
		qDebug("TODO: implement PlatformProcess::data_address"); return edb::address_t();
	}

	QList<std::shared_ptr<IRegion>> regions() const override {
		qDebug("TODO: implement PlatformProcess::regions"); return QList<std::shared_ptr<IRegion>>();
	}

	edb::uid_t uid() const override {
		qDebug("TODO: implement PlatformProcess::uid"); return edb::uid_t();
	}

	QString user() const override;
	QString name() const override;

	QList<Module> loaded_modules() const override {
		qDebug("TODO: implement PlatformProcess::loaded_modules"); return QList<Module>();
	}

public:
	// only legal to call when attached
	QList<std::shared_ptr<IThread>> threads() const override {
		qDebug("TODO: implement PlatformProcess::threads");
		return QList<std::shared_ptr<IThread>>();
	}

	std::shared_ptr<IThread> current_thread() const override {
		qDebug("TODO: implement PlatformProcess::current_thread");
		return std::shared_ptr<IThread>();
	}

	void set_current_thread(IThread& thread) override {
		qDebug("TODO: implement PlatformProcess::set_current_thread");
	}

	std::size_t write_bytes(edb::address_t address, const void *buf, size_t len) override {
		qDebug("TODO: implement PlatformProcess::write_bytes");
		return 0;
	}

	std::size_t patch_bytes(edb::address_t address, const void *buf, size_t len) override {
		qDebug("TODO: implement PlatformProcess::patch_bytes");
		return 0;
	}

	std::size_t read_bytes(edb::address_t address, void *buf, size_t len) const override {
		qDebug("TODO: implement PlatformProcess::read_bytes");
		return 0;
	}

	std::size_t read_pages(edb::address_t address, void *buf, size_t count) const override {
		qDebug("TODO: implement PlatformProcess::read_pages");
		return 0;
	}

	Status pause() override {
		qDebug("TODO: implement PlatformProcess::pause");
		return Status("Not implemented");
	}

	Status resume(edb::EVENT_STATUS status) override {
		qDebug("TODO: implement PlatformProcess::resume");
		return Status("Not implemented");
	}

	Status step(edb::EVENT_STATUS status) override {
		qDebug("TODO: implement PlatformProcess::step");
		return Status("Not implemented");
	}

	bool isPaused() const override {
		qDebug("TODO: implement PlatformProcess::isPaused"); return true;
	}

	QMap<edb::address_t, Patch> patches() const override {
		qDebug("TODO: implement PlatformProcess::patches");
		return QMap<edb::address_t, Patch>();
	}

private:
	edb::pid_t _pid;
	QString _name;
	QString _user;
};
	
}

#endif
