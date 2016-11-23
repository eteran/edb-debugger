#include "ELFXX.h"
#include "ByteShiftArray.h"
#include "IDebugger.h"
#include "Util.h"
#include "edb.h"
#include "string_hash.h"

#include <QDebug>
#include <QVector>
#include <QFile>
#include <cstring>

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <link.h>
#endif

namespace BinaryInfo {


template<typename elfxx_header>
ELFXX<elfxx_header>::ELFXX(const IRegion::pointer &region) : region_(region), header_(0) {
}

template<typename elfxx_header>
ELFXX<elfxx_header>::~ELFXX() {
	delete header_;
}

//------------------------------------------------------------------------------
// Name: header_size
// Desc: returns the number of bytes in this executable's header
//------------------------------------------------------------------------------
template<typename elfxx_header>
size_t ELFXX<elfxx_header>::header_size() const {
	if (header_) {
		size_t size = header_->e_ehsize;
		// Do the program headers immediately follow the ELF header?
		if (size == header_->e_phoff) {
			size += header_->e_phentsize * header_->e_phnum;
		}
		return size;
	} else {
		return sizeof(elfxx_header);
	}
}


//------------------------------------------------------------------------------
// Name: read_header
// Desc: reads in enough of the file to get the header
//------------------------------------------------------------------------------
template<typename elfxx_header>
void ELFXX<elfxx_header>::read_header() {
	if(!header_) {
		if(IProcess *process = edb::v1::debugger_core->process()) {
			if(region_) {
				header_ = new elfxx_header;

				if(!process->read_bytes(region_->start(), header_, sizeof(elfxx_header))) {
					std::memset(header_, 0, sizeof(elfxx_header));
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: validate_header
// Desc: returns true if this file matches this particular info class
//------------------------------------------------------------------------------
template<typename elfxx_header>
bool ELFXX<elfxx_header>::validate_header() {
	read_header();
	if(header_) {
		if(std::memcmp(header_->e_ident, ELFMAG, SELFMAG) == 0) {
			return header_->e_ident[EI_CLASS] == elfxx_header::ELFCLASS;
		}
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: entry_point
// Desc: returns the entry point if any of the binary
//------------------------------------------------------------------------------
template<typename elfxx_header>
edb::address_t ELFXX<elfxx_header>::entry_point() {
	read_header();
	if(header_) {
		return header_->e_entry;
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: header
// Desc: returns a copy of the file header or NULL if the region wasn't a valid,
//       known binary type
//------------------------------------------------------------------------------
template<typename elfxx_header>
const void *ELFXX<elfxx_header>::header() const {
	return header_;
}


// explicit instantiations
template class ELFXX<elf32_header>;
template class ELFXX<elf64_header>;

};

