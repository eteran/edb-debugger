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

#include "edb.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "PE32.h"
#include "string_hash.h"
#include "pe_binary.h"
#include <QDebug>

namespace BinaryInfoPlugin {

PEBinaryException::PEBinaryException(reasonEnum reason): reason_(reason) {}
const char * PEBinaryException::what() { return "TODO"; }

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
PE32::PE32(const std::shared_ptr<IRegion> &region) : region_(region) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	const WORD dos_magic = 0x5A4D;
	const LONG pe_magic = 0x00004550;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	const WORD dos_magic = 0x4D5A;
	const LONG pe_magic = 0x50450000;
#endif

	if (!region_) {
		throw PEBinaryException(PEBinaryException::reasonEnum::INVALID_ARGUMENTS);
	}
	IProcess *process = edb::v1::debugger_core->process();

	if (!process) {
		throw PEBinaryException(PEBinaryException::reasonEnum::READ_FAILURE);
	}

	if (!process->read_bytes(region_->start(), &dos_, sizeof(dos_))) {
		throw PEBinaryException(PEBinaryException::reasonEnum::READ_FAILURE);
	}

	if (dos_.e_magic != dos_magic || dos_.e_lfanew == 0) {
		throw PEBinaryException(PEBinaryException::reasonEnum::INVALID_PE);
	}

	if (!process->read_bytes(region_->start() + dos_.e_lfanew, &pe_, sizeof(pe_))) {
		throw PEBinaryException(PEBinaryException::reasonEnum::READ_FAILURE);
	}

	if (pe_.Signature != pe_magic) {
		throw PEBinaryException(PEBinaryException::reasonEnum::INVALID_PE);
	}
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
edb::address_t PE32::entry_point() {
	return region_->start() + pe_.OptionalHeader.AddressOfEntryPoint;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
edb::address_t PE32::base_address() const {
	return pe_.OptionalHeader.ImageBase;
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
	return sizeof(pe_) + dos_.e_lfanew;
}

//------------------------------------------------------------------------------
// Name: headers
// Desc: returns a list of all headers in this binary
//------------------------------------------------------------------------------
QVector<IBinary::Header> PE32::headers() const {
	QVector<Header> results;

	results.push_back({region_->start(), sizeof(pe_) + dos_.e_lfanew});
	 
	return results;
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
