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

#ifndef IDEBUGGER_H_20061101_
#define IDEBUGGER_H_20061101_

#include "IBreakpoint.h"
#include "OSTypes.h"
#include "Types.h"
#include <QByteArray>
#include <QHash>
#include <QMap>
#include <QString>
#include <QtPlugin>
#include <chrono>
#include <memory>
#include <vector>

class IDebugEvent;
class IProcess;
class IState;
class State;
class Status;

class IDebugger {
public:
	using BreakpointList = QHash<edb::address_t, std::shared_ptr<IBreakpoint>>;

public:
	virtual ~IDebugger() = default;

public:
	enum class CpuMode {
		Unknown,
#if defined(EDB_X86) || defined(EDB_X86_64)
		x86_16,
		x86_32,
		x86_64,
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
		ARM32,
		Thumb,
		ARM64,
#endif
	};

public:
	// system properties
	virtual std::size_t pageSize() const                  = 0;
	virtual std::size_t pointerSize() const               = 0;
	virtual uint64_t cpuType() const                      = 0;
	virtual CpuMode cpuMode() const                       = 0;
	virtual bool hasExtension(uint64_t ext) const         = 0;
	virtual QMap<qlonglong, QString> exceptions() const   = 0;
	virtual QString exceptionName(qlonglong value)        = 0;
	virtual qlonglong exceptionValue(const QString &name) = 0;
	virtual uint8_t nopFillByte() const                   = 0;

public:
	// important register names
	virtual QString stackPointer() const       = 0;
	virtual QString framePointer() const       = 0;
	virtual QString instructionPointer() const = 0;
	virtual QString flagRegister() const       = 0;

public:
	// general process data
	virtual edb::pid_t parentPid(edb::pid_t pid) const                             = 0;
	virtual QMap<edb::pid_t, std::shared_ptr<IProcess>> enumerateProcesses() const = 0;

public:
	// basic process management
	virtual Status attach(edb::pid_t pid)                                                                                                    = 0;
	virtual Status open(const QString &path, const QString &cwd, const QList<QByteArray> &args, const QString &input, const QString &output) = 0;
	virtual std::shared_ptr<IDebugEvent> waitDebugEvent(std::chrono::milliseconds msecs)                                                     = 0;
	virtual Status detach()                                                                                                                  = 0;
	virtual void kill()                                                                                                                      = 0;
	virtual void endDebugSession()                                                                                                           = 0;

public:
	// basic breakpoint managment
	// TODO(eteran): these should be logically moved to IProcess
	virtual BreakpointList backupBreakpoints() const                                     = 0;
	virtual std::shared_ptr<IBreakpoint> addBreakpoint(edb::address_t address)           = 0;
	virtual std::shared_ptr<IBreakpoint> findBreakpoint(edb::address_t address)          = 0;
	virtual std::shared_ptr<IBreakpoint> findTriggeredBreakpoint(edb::address_t address) = 0;
	virtual void clearBreakpoints()                                                      = 0;
	virtual void removeBreakpoint(edb::address_t address)                                = 0;
	virtual std::vector<IBreakpoint::BreakpointType> supportedBreakpointTypes() const    = 0;

public:
	virtual void setIgnoredExceptions(const QList<qlonglong> &exceptions) = 0;

public:
	virtual std::unique_ptr<IState> createState() const = 0;

public:
	// nullptr if not attached
	virtual IProcess *process() const = 0;
};

Q_DECLARE_INTERFACE(IDebugger, "edb.IDebugger/1.0")

#endif
