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
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "edb.h"
#include "libPE/pe_binary.h"
#include "string_hash.h"
#include <QDebug>

namespace BinaryInfoPlugin {

PEBinaryException::PEBinaryException(Reason reason)
	: reason_(reason) {
}
const char *PEBinaryException::what() const noexcept {
	return "PEBinaryException";
}

/**
 * @brief PE32::PE32
 * @param region
 */
PE32::PE32(const std::shared_ptr<IRegion> &region)
	: region_(region) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	constexpr WORD DosMagic = 0x5A4D;
	constexpr LONG PeMagic  = 0x00004550;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	constexpr WORD dos_magic = 0x4D5A;
	constexpr LONG pe_magic  = 0x50450000;
#endif

	if (!region_) {
		throw PEBinaryException(PEBinaryException::Reason::INVALID_ARGUMENTS);
	}
	IProcess *process = edb::v1::debugger_core->process();

	if (!process) {
		throw PEBinaryException(PEBinaryException::Reason::READ_FAILURE);
	}

	if (!process->readBytes(region_->start(), &dos_, sizeof(dos_))) {
		throw PEBinaryException(PEBinaryException::Reason::READ_FAILURE);
	}

	if (dos_.e_magic != DosMagic || dos_.e_lfanew == 0) {
		throw PEBinaryException(PEBinaryException::Reason::INVALID_PE);
	}

	if (!process->readBytes(region_->start() + dos_.e_lfanew, &pe_, sizeof(pe_))) {
		throw PEBinaryException(PEBinaryException::Reason::READ_FAILURE);
	}

	if (pe_.Signature != PeMagic) {
		throw PEBinaryException(PEBinaryException::Reason::INVALID_PE);
	}
}

/**
 * @brief PE32::entryPoint
 * @return
 */
edb::address_t PE32::entryPoint() {
	// TODO(eteran): relative to pe_.OptionalHeader.ImageBase;?
	return pe_.OptionalHeader.AddressOfEntryPoint;
}

/**
 * @brief PE32::native
 * @return
 */
bool PE32::native() const {
	return true;
}

/**
 * @brief PE32::headerSize
 * @return
 */
size_t PE32::headerSize() const {
	return sizeof(pe_) + dos_.e_lfanew;
}

/**
 * @brief PE32::headers
 * @return a list of all headers in this binary
 */
std::vector<IBinary::Header> PE32::headers() const {
	std::vector<Header> results = {
		{region_->start(), sizeof(pe_) + dos_.e_lfanew}};
	return results;
}

/**
 * @brief PE32::header
 * @return a copy of the file header or nullptr if the region wasn't a valid,
 * known binary type
 */
const void *PE32::header() const {
	return nullptr;
}

}
