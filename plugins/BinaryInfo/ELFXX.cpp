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

#include "ELFXX.h"
#include "elf/elf_phdr.h"
#include "ByteShiftArray.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "Util.h"
#include "edb.h"
#include "string_hash.h"


#include <QDebug>
#include <QVector>
#include <QFile>
#include <cstring>
#include <cstdint>

namespace BinaryInfoPlugin {

ELFBinaryException::ELFBinaryException(reasonEnum reason): reason_(reason) {}
const char * ELFBinaryException::what() const noexcept { return "TODO"; }

template<typename elfxx_header>
ELFXX<elfxx_header>::ELFXX(const std::shared_ptr<IRegion> &region) : region_(region) {
	if (!region_) {
		throw ELFBinaryException(ELFBinaryException::reasonEnum::INVALID_ARGUMENTS);
	}
	IProcess *process = edb::v1::debugger_core->process();
	if (!process) {
		throw ELFBinaryException(ELFBinaryException::reasonEnum::READ_FAILURE);
	}

	if(!process->read_bytes(region_->start(), &header_, sizeof(elfxx_header))) {
		throw ELFBinaryException(ELFBinaryException::reasonEnum::READ_FAILURE);
	}

	validate_header();

	auto phdr_size = header_.e_phentsize;

	if (phdr_size < sizeof(typename elfxx_header::elf_phdr)) {
		qDebug()<< QString::number(region_->start(), 16)
			<< "program header size less than expected";
		base_address_ = region_->start();
		return;
	}

	typename elfxx_header::elf_phdr phdr;

	auto phdr_base = region_->start() + header_.e_phoff;
	edb::address_t lowest = ULLONG_MAX;

	// iterate all of the program headers
	for (quint16 entry = 0; entry < header_.e_phnum; entry++) {
		if (!process->read_bytes(
			phdr_base + (phdr_size * entry),
			&phdr,
			sizeof(typename elfxx_header::elf_phdr)
		)) {
			qDebug() << "Failed to read program header";
			base_address_ = region_->start();
			return;
		}

		if (phdr.p_type == PT_LOAD && phdr.p_vaddr < lowest) {
			lowest = phdr.p_vaddr;
		}
	}

	if (lowest == ULLONG_MAX) {
		qDebug() << "binary base address not found. Assuming "
			<< QString::number(region_->start(), 16);
		base_address_ = region->start();
	} else {
#if 0
		qDebug()
			<< "binary base address is" << QString::number(lowest, 16)
			<< "and loaded at" << QString::number(region_->start(), 16);
#endif
		base_address_ = lowest;
	}
}

template<typename elfxx_header>
ELFXX<elfxx_header>::~ELFXX() {}

//------------------------------------------------------------------------------
// Name: header_size
// Desc: returns the number of bytes in this executable's header
//------------------------------------------------------------------------------
template<typename elfxx_header>
size_t ELFXX<elfxx_header>::header_size() const {
	size_t size = header_.e_ehsize;
	// Do the program headers immediately follow the ELF header?
	if (size == header_.e_phoff) {
		size += header_.e_phentsize * header_.e_phnum;
	}
	return size;
}

//------------------------------------------------------------------------------
// Name: base_address
// Desc: returns the base address of the module
//------------------------------------------------------------------------------
template<typename elfxx_header>
edb::address_t ELFXX<elfxx_header>::base_address() const {
	return base_address_;
}

//------------------------------------------------------------------------------
// Name: headers
// Desc: returns a list of all headers in this binary
//------------------------------------------------------------------------------
template <typename elfxx_header>
QVector<IBinary::Header> ELFXX<elfxx_header>::headers() const {
	QVector<Header> results;

	results.push_back({region_->start(), header_.e_ehsize});
	results.push_back({region_->start() + header_.e_phoff, static_cast<size_t>(header_.e_phentsize * header_.e_phnum) });
	 
	return results;
}

template<typename elfxx_header>
void ELFXX<elfxx_header>::validate_header() {
	if(std::memcmp(header_.e_ident, ELFMAG, SELFMAG) != 0) {
		throw ELFBinaryException(ELFBinaryException::reasonEnum::INVALID_ELF);
	}
	if (header_.e_ident[EI_CLASS] != elfxx_header::ELFCLASS) {
		throw ELFBinaryException(ELFBinaryException::reasonEnum::INVALID_ARCHITECTURE);
	}
}


//------------------------------------------------------------------------------
// Name: entry_point
// Desc: returns the entry point if any of the binary
//------------------------------------------------------------------------------
template<typename elfxx_header>
edb::address_t ELFXX<elfxx_header>::entry_point() {
	return header_.e_entry + region_->start() - base_address_;
}

//------------------------------------------------------------------------------
// Name: header
// Desc: returns a copy of the file header or NULL if the region wasn't a valid,
//       known binary type
//------------------------------------------------------------------------------
template<typename elfxx_header>
const void *ELFXX<elfxx_header>::header() const {
	return &header_;
}


// explicit instantiations
template class ELFXX<elf32_header>;
template class ELFXX<elf64_header>;

}

