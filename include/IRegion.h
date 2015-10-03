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

#ifndef IREGION_20120709_H_
#define IREGION_20120709_H_

#include "Types.h"
#include "API.h"
#include <QString>
#include <memory>

class EDB_EXPORT IRegion {
public:
	typedef std::shared_ptr<IRegion> pointer;
	typedef quint32                 permissions_t;

public:
	virtual ~IRegion() {}

public:
	virtual IRegion *clone() const = 0;

public:
	virtual bool accessible() const = 0;
	virtual bool readable() const = 0;
	virtual bool writable() const = 0;
	virtual bool executable() const = 0;
	virtual edb::address_t size() const = 0;

public:
	virtual void set_permissions(bool read, bool write, bool execute) = 0;
	virtual void set_start(edb::address_t address) = 0;
	virtual void set_end(edb::address_t address) = 0;


public:
	virtual edb::address_t start() const = 0;
	virtual edb::address_t end() const = 0; // NOTE: is the address of one past the last byte of the region
	virtual edb::address_t base() const = 0;
	virtual QString name() const = 0;
	virtual permissions_t permissions() const = 0;

public:
	bool contains(edb::address_t address) const {
		return address >= start() && address < end();
	}

	int compare(const IRegion::pointer &other) const {

		if(!other) {
			return -1;
		}

		if(start()        == other->start() &&
			end()         == other->end() &&
			base()        == other->base() &&
			name()        == other->name() &&
			permissions() == other->permissions()) {

			return 0;
		}

		return 1;
	}
};

#endif
