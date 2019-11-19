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

#ifndef IBINARY_H_20070718_
#define IBINARY_H_20070718_

#include "API.h"
#include "Types.h"
#include <memory>
#include <vector>

class IRegion;

class EDB_EXPORT IBinary {
public:
	struct Header {
		edb::address_t address;
		size_t size;
		// TODO(eteran): maybe label/type/etc...
	};

public:
	virtual ~IBinary() = default;

public:
	virtual bool native() const                 = 0;
	virtual edb::address_t entryPoint()         = 0;
	virtual size_t headerSize() const           = 0;
	virtual const void *header() const          = 0;
	virtual std::vector<Header> headers() const = 0;

public:
	using create_func_ptr_t = std::unique_ptr<IBinary> (*)(const std::shared_ptr<IRegion> &);
};

#endif
