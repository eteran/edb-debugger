/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] virtual QDateTime startTime() const                     = 0;
	[[nodiscard]] virtual QList<QByteArray> arguments() const             = 0;
	[[nodiscard]] virtual QString currentWorkingDirectory() const         = 0;
	[[nodiscard]] virtual QString executable() const                      = 0;
	[[nodiscard]] virtual QString standardInput() const                   = 0;
	[[nodiscard]] virtual QString standardOutput() const                  = 0;
	[[nodiscard]] virtual edb::pid_t pid() const                          = 0;
	[[nodiscard]] virtual std::shared_ptr<IProcess> parent() const        = 0;
	[[nodiscard]] virtual edb::address_t codeAddress() const              = 0;
	[[nodiscard]] virtual edb::address_t dataAddress() const              = 0;
	[[nodiscard]] virtual edb::address_t entryPoint() const               = 0;
	[[nodiscard]] virtual QList<std::shared_ptr<IRegion>> regions() const = 0;
	[[nodiscard]] virtual edb::uid_t uid() const                          = 0;
	[[nodiscard]] virtual QString user() const                            = 0;
	[[nodiscard]] virtual QString name() const                            = 0;
	[[nodiscard]] virtual QList<Module> loadedModules() const             = 0;

public:
	[[nodiscard]] virtual edb::address_t debugPointer() const { return 0; }
	[[nodiscard]] virtual edb::address_t calculateMain() const { return 0; }

public:
	// only legal to call when attached
	[[nodiscard]] virtual bool isPaused() const                                          = 0;
	[[nodiscard]] virtual QList<std::shared_ptr<IThread>> threads() const                = 0;
	[[nodiscard]] virtual QMap<edb::address_t, Patch> patches() const                    = 0;
	[[nodiscard]] virtual std::shared_ptr<IThread> currentThread() const                 = 0;
	virtual Status pause()                                                               = 0;
	virtual Status resume(edb::EventStatus status)                                       = 0;
	virtual Status step(edb::EventStatus status)                                         = 0;
	virtual std::size_t patchBytes(edb::address_t address, const void *buf, size_t len)  = 0;
	virtual std::size_t readBytes(edb::address_t address, void *buf, size_t len) const   = 0;
	virtual std::size_t readPages(edb::address_t address, void *buf, size_t count) const = 0;
	virtual std::size_t writeBytes(edb::address_t address, const void *buf, size_t len)  = 0;
	virtual void setCurrentThread(IThread &thread)                                       = 0;
};

#endif
