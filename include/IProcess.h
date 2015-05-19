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

#include "Types.h"

class IProcess {
public:
	virtual ~IProcess() {}
	
public:
	// returns true on success, false on failure, all bytes must be successfully
	// read/written in order for a success. The debugged application should be stopped
	// or this will return false immediately.
	virtual bool write_bytes(edb::address_t address, const void *buf, size_t len) = 0;
	virtual bool read_bytes(edb::address_t address, void *buf, size_t len) = 0;
	virtual bool read_pages(edb::address_t address, void *buf, size_t count) = 0;
};

#endif
