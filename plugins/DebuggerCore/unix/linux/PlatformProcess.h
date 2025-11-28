/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PLATFORM_PROCESS_H_20150517_
#define PLATFORM_PROCESS_H_20150517_

#include "IProcess.h"
#include "PlatformFile.h"
#include "Status.h"

#include <QCoreApplication>

namespace DebuggerCorePlugin {

class DebuggerCore;

class PlatformProcess final : public IProcess {
	Q_DECLARE_TR_FUNCTIONS(PlatformProcess)
	friend class PlatformThread;

public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	~PlatformProcess() override                         = default;
	PlatformProcess(const PlatformProcess &)            = delete;
	PlatformProcess &operator=(const PlatformProcess &) = delete;

public:
	[[nodiscard]] edb::address_t codeAddress() const override;
	[[nodiscard]] edb::address_t dataAddress() const override;
	[[nodiscard]] edb::address_t entryPoint() const override;
	[[nodiscard]] edb::pid_t pid() const override;
	[[nodiscard]] edb::uid_t uid() const override;
	[[nodiscard]] QDateTime startTime() const override;
	[[nodiscard]] QList<Module> loadedModules() const override;
	[[nodiscard]] QList<QByteArray> arguments() const override;
	[[nodiscard]] QList<std::shared_ptr<IRegion>> regions() const override;
	[[nodiscard]] QList<std::shared_ptr<IThread>> threads() const override;
	[[nodiscard]] QString currentWorkingDirectory() const override;
	[[nodiscard]] QString executable() const override;
	[[nodiscard]] QString name() const override;
	[[nodiscard]] QString standardInput() const override;
	[[nodiscard]] QString standardOutput() const override;
	[[nodiscard]] QString user() const override;
	[[nodiscard]] std::shared_ptr<IProcess> parent() const override;
	[[nodiscard]] std::shared_ptr<IThread> currentThread() const override;
	void setCurrentThread(IThread &thread) override;

public:
	[[nodiscard]] edb::address_t debugPointer() const override;
	[[nodiscard]] edb::address_t calculateMain() const override;

public:
	Status pause() override;
	Status resume(edb::EventStatus status) override;
	Status step(edb::EventStatus status) override;
	[[nodiscard]] bool isPaused() const override;

public:
	std::size_t writeBytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t patchBytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t readBytes(edb::address_t address, void *buf, size_t len) const override;
	std::size_t readPages(edb::address_t address, void *buf, size_t count) const override;
	[[nodiscard]] QMap<edb::address_t, Patch> patches() const override;

private:
	bool ptracePoke(edb::address_t address, long value);
	long ptracePeek(edb::address_t address, bool *ok) const;
	uint8_t ptraceReadByte(edb::address_t address, bool *ok) const;
	void ptraceWriteByte(edb::address_t address, uint8_t value, bool *ok);

private:
	DebuggerCore *core_ = nullptr;
	edb::pid_t pid_;
	std::shared_ptr<PlatformFile> readOnlyMemFile_;
	std::shared_ptr<PlatformFile> readWriteMemFile_;
	QMap<edb::address_t, Patch> patches_;
	QString input_;
	QString output_;
};

}

#endif
