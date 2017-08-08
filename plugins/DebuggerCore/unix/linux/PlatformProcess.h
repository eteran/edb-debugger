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

#include <QFile>

namespace DebuggerCorePlugin {

class DebuggerCore;

class PlatformProcess : public IProcess {
	friend class PlatformThread;
public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	virtual ~PlatformProcess();

private:
	PlatformProcess(const PlatformProcess &) = delete;
	PlatformProcess& operator=(const PlatformProcess &) = delete;

public:
	virtual QDateTime                       start_time() const override;
	virtual QList<QByteArray>               arguments() const override;
	virtual QString                         current_working_directory() const override;
	virtual QString                         executable() const override;
	virtual edb::pid_t                      pid() const override;
	virtual std::shared_ptr<IProcess>       parent() const override;
	virtual edb::address_t                  code_address() const override;
	virtual edb::address_t                  data_address() const override;
	virtual QList<std::shared_ptr<IRegion>> regions() const override;
	virtual QList<std::shared_ptr<IThread>> threads() const override;
	virtual std::shared_ptr<IThread>        current_thread() const override;
	virtual edb::uid_t                      uid() const override;
	virtual QString                         user() const override;
	virtual QString                         name() const override;
	virtual QList<Module>                   loaded_modules() const override;

public:
	virtual Status pause() override;
	virtual Status resume(edb::EVENT_STATUS status) override;
	virtual Status step(edb::EVENT_STATUS status) override;
	virtual bool isPaused() const override;

public:
	virtual std::size_t write_bytes(edb::address_t address, const void *buf, size_t len) override;
	virtual std::size_t patch_bytes(edb::address_t address, const void *buf, size_t len) override;
	virtual std::size_t read_bytes(edb::address_t address, void *buf, size_t len) const override;
	virtual std::size_t read_pages(edb::address_t address, void *buf, size_t count) const override;
	virtual QMap<edb::address_t, Patch> patches() const override;

private:
	bool ptrace_poke(edb::address_t address, long value);
	long ptrace_peek(edb::address_t address, bool *ok) const;
	quint8 read_byte_via_ptrace(edb::address_t address, bool *ok) const;
	void write_byte_via_ptrace(edb::address_t address, quint8 value, bool *ok);

private:
	DebuggerCore*               core_;
	edb::pid_t                  pid_;
	QFile*                      ro_mem_file_;
	QFile*                      rw_mem_file_;
	QMap<edb::address_t, Patch> patches_;
};

}

#endif
