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

#ifndef IPROCESS_H_20150516_
#define IPROCESS_H_20150516_

#include "OSTypes.h"
#include "Patch.h"
#include "Status.h"
#include "Types.h"
#include <QList>
#include <QMap>
#include <memory>

class IRegion;
class IThread;
class QDateTime;
class QString;
struct Module;

class IProcess {
public:
	virtual ~IProcess() = default;

public:
	// legal to call when not attached
	virtual QDateTime startTime() const                     = 0;
	virtual QList<QByteArray> arguments() const             = 0;
	virtual QString currentWorkingDirectory() const         = 0;
	virtual QString executable() const                      = 0;
	virtual QString stardardInput() const                   = 0;
	virtual QString stardardOutput() const                  = 0;
	virtual edb::pid_t pid() const                          = 0;
	virtual std::shared_ptr<IProcess> parent() const        = 0;
	virtual edb::address_t codeAddress() const              = 0;
	virtual edb::address_t dataAddress() const              = 0;
	virtual edb::address_t entryPoint() const               = 0;
	virtual QList<std::shared_ptr<IRegion>> regions() const = 0;
	virtual edb::uid_t uid() const                          = 0;
	virtual QString user() const                            = 0;
	virtual QString name() const                            = 0;
	virtual QList<Module> loadedModules() const             = 0;

public:
	virtual edb::address_t debugPointer() const { return 0; }
	virtual edb::address_t calculateMain() const { return 0; }

public:
	// only legal to call when attached
	virtual QList<std::shared_ptr<IThread>> threads() const                              = 0;
	virtual std::shared_ptr<IThread> currentThread() const                               = 0;
	virtual void setCurrentThread(IThread &thread)                                       = 0;
	virtual std::size_t writeBytes(edb::address_t address, const void *buf, size_t len)  = 0;
	virtual std::size_t patchBytes(edb::address_t address, const void *buf, size_t len)  = 0;
	virtual std::size_t readBytes(edb::address_t address, void *buf, size_t len) const   = 0;
	virtual std::size_t readPages(edb::address_t address, void *buf, size_t count) const = 0;
	virtual Status pause()                                                               = 0;
	virtual Status resume(edb::EventStatus status)                                       = 0;
	virtual Status step(edb::EventStatus status)                                         = 0;
	virtual bool isPaused() const                                                        = 0;
	virtual QMap<edb::address_t, Patch> patches() const                                  = 0;
};

#endif
