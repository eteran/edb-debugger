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
#include "IRegion.h"
#include "Module.h"
#include <QSet>

namespace DebuggerCorePlugin {

class PlatformProcess;
class PlatformThread;

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
	bool hasExtension(uint64_t ext) const override;
	size_t pageSize() const override;
	std::size_t pointerSize() const override {
		return sizeof(void *);
	}
	std::shared_ptr<IDebugEvent> waitDebugEvent(std::chrono::milliseconds msecs) override;
	Status attach(edb::pid_t pid) override;
	Status detach() override;
	void kill() override;

	Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &input, const QString &output) override;

	MeansOfCapture lastMeansOfCapture() const override {
		qDebug("TODO: Implement DebuggerCore::lastMeansOfCapture");
		return MeansOfCapture::NeverCaptured;
	}

	int sys_pointer_size() const;
	QMap<qlonglong, QString> exceptions() const override;
	QString exceptionName(qlonglong value) override {
		qDebug("TODO: Implement DebuggerCore::exceptionName");
		return "";
	}

	qlonglong exceptionValue(const QString &name) override {
		qDebug("TODO: Implement DebuggerCore::exceptionValue");
		return 0;
	}

public:
	// thread support stuff (optional)
	QList<edb::tid_t> thread_ids() const { return threads_.keys(); }
	edb::tid_t active_thread() const { return activeThread_; }
	void set_active_thread(edb::tid_t tid) {
		Q_ASSERT(threads_.contains(tid));
		activeThread_ = tid;
	}

public:
	// process properties
	edb::pid_t parentPid(edb::pid_t pid) const override;
	uint64_t cpuType() const override;

	CpuMode cpuMode() const override {
		qDebug("TODO: Implement DebuggerCore::cpu_mode");
		return CpuMode::Unknown;
	}

public:
	std::unique_ptr<IState> createState() const override;

private:
	QMap<edb::pid_t, std::shared_ptr<IProcess>> enumerateProcesses() const override;

public:
	QString stackPointer() const override;
	QString framePointer() const override;
	QString instructionPointer() const override;
	QString flagRegister() const override {
		qDebug("TODO: Implement DebuggerCore::flag_register");
		return "";
	}

	void setIgnoredExceptions(const QList<qlonglong> &exceptions) override {
		Q_UNUSED(exceptions)
		qDebug("TODO: Implement DebuggerCore::set_ignored_exceptions");
	}

public:
	uint8_t nopFillByte() const override;

public:
	IProcess *process() const override;

private:
	using threadmap_t = QHash<edb::tid_t, std::shared_ptr<PlatformThread>>;

private:
	size_t pageSize_ = 0;
	std::shared_ptr<IProcess> process_;
	threadmap_t threads_;
	edb::tid_t activeThread_;
};

}

#endif
