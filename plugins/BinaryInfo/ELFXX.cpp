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
#include "libELF/elf_phdr.h"
#include "ByteShiftArray.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "Util.h"
#include "edb.h"
#include "string_hash.h"
#include "MemoryRegions.h"

#include <QDebug>
#include <QVector>
#include <QFile>
#include <cstring>
#include <cstdint>

namespace BinaryInfoPlugin {

class ELFBinaryException : public std::exception {
	const char *what() const noexcept override = 0;
};

class InvalidArguments : public ELFBinaryException {
public:
	const char * what() const noexcept override {
		return "Invalid Arguments";
	}
};

class ReadFailure : public ELFBinaryException {
public:
	const char * what() const noexcept override {
		return "Read Failure";
	}
};

class InvalidELF : public ELFBinaryException {
public:
	const char * what() const noexcept override {
		return "Invalid ELF";
	}
};

class InvalidArchitecture : public ELFBinaryException {
public:
	const char * what() const noexcept override {
		return "Invalid Architecture";
	}
};

template <class elfxx_header>
ELFXX<elfxx_header>::ELFXX(const std::shared_ptr<IRegion> &region) : region_(region) {

	using phdr_type = typename elfxx_header::elf_phdr;

	if (!region_) {
		throw InvalidArguments();
	}

	IProcess *const process = edb::v1::debugger_core->process();
	if (!process) {
		throw ReadFailure();
	}

	if(!process->readBytes(region_->start(), &header_, sizeof(elfxx_header))) {
		throw ReadFailure();
	}

	validate_header();
		
	headers_.push_back({region_->start(), header_.e_ehsize});
	headers_.push_back({region_->start() + header_.e_phoff, static_cast<size_t>(header_.e_phentsize * header_.e_phnum) });

	auto phdr_size = header_.e_phentsize;

	if (phdr_size < sizeof(phdr_type)) {
		qDebug()<< QString::number(region_->start(), 16) << "program header size less than expected";
		base_address_ = region_->start();
		return;
	}

	phdr_type phdr;

	auto phdr_base = region_->start() + header_.e_phoff;
	edb::address_t lowest = ULLONG_MAX;

	if(header_.e_type == ET_EXEC) {

		// iterate all of the program headers
		for (quint16 entry = 0; entry < header_.e_phnum; ++entry) {

			if (!process->readBytes(phdr_base + (phdr_size * entry), &phdr, sizeof(phdr_type))) {
				qDebug() << "Failed to read program header";
				break;
			}

			if (phdr.p_type == PT_LOAD && phdr.p_vaddr < lowest) {
				lowest = phdr.p_vaddr;
				// NOTE(eteran): they are defined to be in ascending order of vaddr
				break;
			}
		}
	} else if(header_.e_type == ET_DYN) {

		const QString process_executable = edb::v1::debugger_core->process()->name();
		for(const std::shared_ptr<IRegion> &r : edb::v1::memory_regions().regions()) {
			if(r->executable() && r->name() == region->name()) {
				lowest = std::min(lowest, r->start());
			}
		}
	}

	if (lowest == ULLONG_MAX) {
		qDebug() << "binary base address not found. Assuming " << QString::number(region_->start(), 16);
		base_address_ = region->start();
	} else {
		base_address_ = lowest;
	}
}

//------------------------------------------------------------------------------
// Name: header_size
// Desc: returns the number of bytes in this executable's header
//------------------------------------------------------------------------------
template <class elfxx_header>
size_t ELFXX<elfxx_header>::headerSize() const {
	size_t size = header_.e_ehsize;
	// Do the program headers immediately follow the ELF header?
	if (size == header_.e_phoff) {
		size += header_.e_phentsize * header_.e_phnum;
	}
	return size;
}

//------------------------------------------------------------------------------
// Name: headers
// Desc: returns a list of all headers in this binary
//------------------------------------------------------------------------------
template <class elfxx_header>
std::vector<IBinary::Header> ELFXX<elfxx_header>::headers() const {
	return headers_;
}

//------------------------------------------------------------------------------
// Name: validate_header
// Desc: ensures that the header that we read was valid
//------------------------------------------------------------------------------
template <class elfxx_header>
void ELFXX<elfxx_header>::validate_header() {
	if(std::memcmp(header_.e_ident, ELFMAG, SELFMAG) != 0) {
		throw InvalidELF();
	}
	if (header_.e_ident[EI_CLASS] != elfxx_header::ELFCLASS) {
		throw InvalidArchitecture();
	}
}


//------------------------------------------------------------------------------
// Name: entry_point
// Desc: returns the entry point if any of the binary
//------------------------------------------------------------------------------
template <class elfxx_header>
edb::address_t ELFXX<elfxx_header>::entryPoint() {
	return header_.e_entry + base_address_;
}

//------------------------------------------------------------------------------
// Name: header
// Desc: returns a copy of the file header or NULL if the region wasn't a valid,
//       known binary type
//------------------------------------------------------------------------------
template <class elfxx_header>
const void *ELFXX<elfxx_header>::header() const {
	return &header_;
}


// explicit instantiations
template class ELFXX<elf32_header>;
template class ELFXX<elf64_header>;

}

