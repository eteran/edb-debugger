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

#ifndef PLATFORM_PROCESS_H_20150517_
#define PLATFORM_PROCESS_H_20150517_

#include "DebuggerCore.h"
#include "IProcess.h"
#include "Module.h"

#include <QDateTime>

#include <TlHelp32.h>

namespace DebuggerCorePlugin {

class DebuggerCore;

class PlatformProcess : public IProcess {
	friend class DebuggerCore;
	friend class PlatformThread;

public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	PlatformProcess(DebuggerCore *core, HANDLE handle);
	~PlatformProcess() override;

public:
	// legal to call when not attached
	QDateTime startTime() const override;

	QList<QByteArray> arguments() const override;

	QString stardardInput() const override {
		qDebug("TODO: implement PlatformProcess::stardardInput");
		return QString();
	}

	QString stardardOutput() const override {
		qDebug("TODO: implement PlatformProcess::currentWorkingDirectory");
		return QString();
	}

	QString currentWorkingDirectory() const override {
		qDebug("TODO: implement PlatformProcess::currentWorkingDirectory");
		return QString();
	}

	QString executable() const override;

	edb::address_t entryPoint() const override {
		qDebug("TODO: implement PlatformProcess::entryPoint");
		return edb::address_t();
		return 0;
	}

	edb::pid_t pid() const override;
	std::shared_ptr<IProcess> parent() const override;

	edb::address_t codeAddress() const override {
		qDebug("TODO: implement PlatformProcess::codeAddress");
		return edb::address_t();
	}

	edb::address_t dataAddress() const override {
		qDebug("TODO: implement PlatformProcess::dataAddress");
		return edb::address_t();
	}

	QList<std::shared_ptr<IRegion>> regions() const override;

	edb::uid_t uid() const override;
	QString user() const override;
	QString name() const override;
	QList<Module> loadedModules() const override;

public:
	// only legal to call when attached
	QList<std::shared_ptr<IThread>> threads() const override;
	std::shared_ptr<IThread> currentThread() const override;
	void setCurrentThread(IThread &thread) override;
	Status pause() override;
	std::size_t writeBytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t readBytes(edb::address_t address, void *buf, size_t len) const override;
	std::size_t readPages(edb::address_t address, void *buf, size_t count) const override;

	std::size_t patchBytes(edb::address_t address, const void *buf, size_t len) override {
		Q_UNUSED(address)
		Q_UNUSED(buf)
		Q_UNUSED(len)
		qDebug("TODO: implement PlatformProcess::patchBytes");
		return 0;
	}

	Status resume(edb::EventStatus status) override;
	Status step(edb::EventStatus status) override;
	bool isPaused() const override;
	QMap<edb::address_t, Patch> patches() const override;

private:
	bool isWow64() const;

private:
	edb::address_t startAddress_ = 0;
	edb::address_t imageBase_    = 0;
	DebuggerCore *core_          = nullptr;
	HANDLE hProcess_             = nullptr;
	QMap<edb::address_t, Patch> patches_;
	DEBUG_EVENT lastEvent_ = {};
};

}

#endif
