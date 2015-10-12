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

namespace DebuggerCore {

class DebuggerCore;

class PlatformProcess : public IProcess {
public:
	PlatformProcess(DebuggerCore *core, edb::pid_t pid);
	virtual ~PlatformProcess();

public:
	virtual QDateTime               start_time() const;
	virtual QList<QByteArray>       arguments() const;
	virtual QString                 current_working_directory() const;
	virtual QString                 executable() const;
	virtual edb::pid_t              pid() const;
	virtual IProcess::pointer       parent() const;
	virtual edb::address_t          code_address() const;
	virtual edb::address_t          data_address() const;
	virtual QList<IRegion::pointer> regions() const;

public:
	virtual std::size_t write_bytes(edb::address_t address, const void *buf, size_t len);
	virtual std::size_t read_bytes(edb::address_t address, void *buf, size_t len);
	virtual std::size_t read_pages(edb::address_t address, void *buf, size_t count);

private:
	DebuggerCore* core_;
	edb::pid_t    pid_;
};

}

#endif
