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

#ifndef IPROCESS_20150516_H_
#define IPROCESS_20150516_H_

#include "IRegion.h"
#include "IThread.h"
#include "Module.h"
#include "Types.h"
#include <QList>
#include <memory>

class QDateTime;
class QString;

class IProcess {
public:
	typedef std::shared_ptr<IProcess> pointer;
public:
	virtual ~IProcess() {}

public:
	// legal to call when not attached
	virtual QDateTime               start_time() const = 0;
	virtual QList<QByteArray>       arguments() const = 0;
	virtual QString                 current_working_directory() const = 0;
	virtual QString                 executable() const = 0;
	virtual edb::pid_t              pid() const = 0;
	virtual pointer                 parent() const = 0;
	virtual edb::address_t          code_address() const = 0;
	virtual edb::address_t          data_address() const = 0;
	virtual QList<IRegion::pointer> regions() const = 0;
	virtual edb::uid_t              uid() const = 0;
	virtual QString                 user() const = 0;
	virtual QString                 name() const = 0;
	virtual QList<Module>           loaded_modules() const = 0;	
	
public:
	// only legal to call when attached
	virtual QList<IThread::pointer> threads() const = 0;
	virtual IThread::pointer        current_thread() const = 0;
	virtual std::size_t             write_bytes(edb::address_t address, const void *buf, size_t len) = 0;
	virtual std::size_t             read_bytes(edb::address_t address, void *buf, size_t len) const = 0;
	virtual std::size_t             read_pages(edb::address_t address, void *buf, size_t count) const = 0;
	virtual void                    pause() = 0;
	virtual void                    resume(edb::EVENT_STATUS status) = 0;
	virtual void                    step(edb::EVENT_STATUS status) = 0;	
};

#endif
