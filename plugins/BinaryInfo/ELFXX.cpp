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
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "MemoryRegions.h"
#include "Util.h"
#include "edb.h"
#include "libELF/elf_phdr.h"
#include "string_hash.h"

#include <QDebug>
#include <QFile>
#include <QVector>
#include <cstdint>
#include <cstring>

namespace BinaryInfoPlugin {

class ELFBinaryException : public std::exception {
	const char *what() const noexcept override = 0;
};

class InvalidArguments : public ELFBinaryException {
public:
	const char *what() const noexcept override {
		return "Invalid Arguments";
	}
};

class ReadFailure : public ELFBinaryException {
public:
	const char *what() const noexcept override {
		return "Read Failure";
	}
};

class InvalidELF : public ELFBinaryException {
public:
	const char *what() const noexcept override {
		return "Invalid ELF";
	}
};

class InvalidArchitecture : public ELFBinaryException {
public:
	const char *what() const noexcept override {
		return "Invalid Architecture";
	}
};

template <class ElfHeader>
ELFXX<ElfHeader>::ELFXX(const std::shared_ptr<IRegion> &region)
	: region_(region) {

	using phdr_type = typename ElfHeader::elf_phdr;

	if (!region_) {
		throw InvalidArguments();
	}

	IProcess *const process = edb::v1::debugger_core->process();
	if (!process) {
		throw ReadFailure();
	}

	if (!process->readBytes(region_->start(), &header_, sizeof(ElfHeader))) {
		throw ReadFailure();
	}

	validateHeader();

	headers_.push_back({region_->start(), header_.e_ehsize});
	headers_.push_back({region_->start() + header_.e_phoff, static_cast<size_t>(header_.e_phentsize * header_.e_phnum)});

	auto phdr_size = header_.e_phentsize;

	if (phdr_size < sizeof(phdr_type)) {
		qDebug() << QString::number(region_->start(), 16) << "program header size less than expected";
		baseAddress_ = region_->start();
		return;
	}

	phdr_type phdr;

	auto phdr_base        = region_->start() + header_.e_phoff;
	edb::address_t lowest = ULLONG_MAX;

	if (header_.e_type == ET_EXEC) {

		// iterate all of the program headers
		for (uint16_t entry = 0; entry < header_.e_phnum; ++entry) {

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
	} else if (header_.e_type == ET_DYN) {

		const QString process_executable = edb::v1::debugger_core->process()->name();
		for (const std::shared_ptr<IRegion> &r : edb::v1::memory_regions().regions()) {
			if (r->executable() && r->name() == region->name()) {
				lowest = std::min(lowest, r->start());
			}
		}
	}

	if (lowest == ULLONG_MAX) {
		qDebug() << "binary base address not found. Assuming " << QString::number(region_->start(), 16);
		baseAddress_ = region->start();
	} else {
		baseAddress_ = lowest;
	}
}

template <class ElfHeader>
/**
 * @brief ELFXX<ElfHeader>::headerSize
 * @return the number of bytes in this executable's header
 */
size_t ELFXX<ElfHeader>::headerSize() const {
	size_t size = header_.e_ehsize;
	// Do the program headers immediately follow the ELF header?
	if (size == header_.e_phoff) {
		size += header_.e_phentsize * header_.e_phnum;
	}
	return size;
}

/**
 * @brief ELFXX<ElfHeader>::headers
 * @return a list of all headers in this binary
 */
template <class ElfHeader>
std::vector<IBinary::Header> ELFXX<ElfHeader>::headers() const {
	return headers_;
}

/**
 * @brief ELFXX<ElfHeader>::validateHeader
 *
 * ensures that the header that we read was valid
 */
template <class ElfHeader>
void ELFXX<ElfHeader>::validateHeader() {
	if (std::memcmp(header_.e_ident, ELFMAG, SELFMAG) != 0) {
		throw InvalidELF();
	}
	if (header_.e_ident[EI_CLASS] != ElfHeader::ELFCLASS) {
		throw InvalidArchitecture();
	}
}

/**
 * @brief ELFXX<ElfHeader>::entryPoint
 * @return the entry point if any of the binary
 */
template <class ElfHeader>
edb::address_t ELFXX<ElfHeader>::entryPoint() {
	return header_.e_entry + baseAddress_;
}

/**
 * @brief ELFXX<ElfHeader>::header
 * @return a copy of the file header or nullptr if the region wasn't a valid,
 * known binary type
 */
template <class ElfHeader>
const void *ELFXX<ElfHeader>::header() const {
	return &header_;
}

// explicit instantiations
template class ELFXX<elf32_header>;
template class ELFXX<elf64_header>;

}
