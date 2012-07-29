/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef MEMORY_REGION_20120709_H_
#define MEMORY_REGION_20120709_H_

#include "Types.h"
#include "API.h"
#include "IRegion.h"

#include <QString>
#include <QHash>

class MemoryRegions;

class EDB_EXPORT MemoryRegion {
public:
	MemoryRegion(edb::address_t start, edb::address_t end, edb::address_t base, const QString &name, IRegion::permissions_t permissions);
	MemoryRegion();
	~MemoryRegion();

public:
	MemoryRegion(const MemoryRegion &other);
	MemoryRegion &operator=(const MemoryRegion &rhs);

	bool operator==(const MemoryRegion &rhs) const;
	bool operator!=(const MemoryRegion &rhs) const;
	bool contains(edb::address_t address) const;

	void swap(MemoryRegion &other);

public:
	bool accessible() const;
	bool readable() const;
	bool writable() const;
	bool executable() const;
	edb::address_t size() const;
	void set_permissions(bool read, bool write, bool execute);
	
public:
	edb::address_t start() const;
	edb::address_t end() const;
	edb::address_t base() const;
	QString name() const;
	IRegion::permissions_t permissions() const;

private:
	IRegion *impl_;
};

// provide a reasonable hash function for the region
inline uint qHash(const MemoryRegion &region) {
	return qHash(region.start());
}

#endif
