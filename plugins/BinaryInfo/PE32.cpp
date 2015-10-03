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

#include "PE32.h"
#include "string_hash.h"
#include "pe_binary.h"

namespace BinaryInfo {

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PE32::PE32(const IRegion::pointer &region) : region_(region) {
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PE32::~PE32() {
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PE32::validate_header() {
	return false;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PE32::entry_point() {
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PE32::calculate_main() {
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool PE32::native() const {
	return true;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PE32::debug_pointer() {
	return 0;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
size_t PE32::header_size() const {
	return 0;
}

//------------------------------------------------------------------------------
// Name: header
// Desc: returns a copy of the file header or NULL if the region wasn't a valid,
//       known binary type
//------------------------------------------------------------------------------
const void *PE32::header() const {
	return 0;
}

}
