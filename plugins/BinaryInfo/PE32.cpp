/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
PE32::PE32(std::shared_ptr<IRegion> region)
	: region_(std::move(region)) {
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
