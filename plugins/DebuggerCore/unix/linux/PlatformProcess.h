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
#include "Status.h"

#include <QCoreApplication>
#include <QFile>

namespace DebuggerCorePlugin {

class DebuggerCore;

class PlatformProcess final : public IProcess {
	Q_DECLARE_TR_FUNCTIONS(PlatformProcess)
	friend class PlatformThread;

public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	~PlatformProcess() override                         = default;
	PlatformProcess(const PlatformProcess &)            = delete;
	PlatformProcess& operator=(const PlatformProcess &) = delete;

public:
	QDateTime                       start_time() const override;
	QList<QByteArray>               arguments() const override;
	QString                         current_working_directory() const override;
	QString                         executable() const override;
	edb::pid_t                      pid() const override;
	std::shared_ptr<IProcess>       parent() const override;
	edb::address_t                  code_address() const override;
	edb::address_t                  data_address() const override;
	edb::address_t                  entry_point() const override;
	QList<std::shared_ptr<IRegion>> regions() const override;
	QList<std::shared_ptr<IThread>> threads() const override;
	std::shared_ptr<IThread>        current_thread() const override;
	void                            set_current_thread(IThread& thread) override;
	edb::uid_t                      uid() const override;
	QString                         user() const override;
	QString                         name() const override;
	QList<Module>                   loaded_modules() const override;

public:
	edb::address_t debug_pointer() const override;
	edb::address_t calculate_main() const override;

public:
	Status pause() override;
	Status resume(edb::EVENT_STATUS status) override;
	Status step(edb::EVENT_STATUS status) override;
	bool isPaused() const override;

public:
	std::size_t write_bytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t patch_bytes(edb::address_t address, const void *buf, size_t len) override;
	std::size_t read_bytes(edb::address_t address, void *buf, size_t len) const override;
	std::size_t read_pages(edb::address_t address, void *buf, size_t count) const override;
	QMap<edb::address_t, Patch> patches() const override;

private:
	bool ptrace_poke(edb::address_t address, long value);
	long ptrace_peek(edb::address_t address, bool *ok) const;
	quint8 read_byte_via_ptrace(edb::address_t address, bool *ok) const;
	void write_byte_via_ptrace(edb::address_t address, quint8 value, bool *ok);

private:
	DebuggerCore*               core_;
	edb::pid_t                  pid_;
	std::shared_ptr<QFile>      ro_mem_file_;
	std::shared_ptr<QFile>      rw_mem_file_;
	QMap<edb::address_t, Patch> patches_;
};

}

#endif
