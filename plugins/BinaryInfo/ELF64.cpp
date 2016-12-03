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

//------------------------------------------------------------------------------
// Name: nativ
// Desc: returns true if this binary is native to the arch edb was built for
//------------------------------------------------------------------------------
template<>
bool ELF64::native() const {
	return edb::v1::debugger_core->cpu_type() == edb::string_hash("x86-64");
}

//------------------------------------------------------------------------------
// Name: debug_pointer
// Desc: attempts to locate the ELF debug pointer in the target process and
//       returns it, 0 of not found
//------------------------------------------------------------------------------
template<>
edb::address_t ELF64::debug_pointer() {
	if(IProcess *process = edb::v1::debugger_core->process()) {
		const edb::address_t section_offset = header_.e_phoff;
		const std::size_t count             = header_.e_phnum;

		elf64_phdr section_header;
		for(std::size_t i = 0; i < count; ++i) {
			if(process->read_bytes(region_->start() + (section_offset + i * sizeof(elf64_phdr)), &section_header, sizeof(elf64_phdr))) {
				if(section_header.p_type == PT_DYNAMIC) {
					try {
						QVector<quint8> buf(section_header.p_memsz);
						if(process->read_bytes(section_header.p_vaddr, &buf[0], section_header.p_memsz)) {
							auto dynamic = reinterpret_cast<elf64_dyn *>(&buf[0]);
							while(dynamic->d_tag != DT_NULL) {
								if(dynamic->d_tag == DT_DEBUG) {
									return dynamic->d_un.d_val;
								}
								++dynamic;
							}
						}
					} catch(const std::bad_alloc &) {
						qDebug() << "[Elf64::debug_pointer] no more memory";
						return 0;
					}
				}
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------------
// Name: calculate_main
// Desc: uses a heuristic to locate "main"
//------------------------------------------------------------------------------
template<>
edb::address_t ELF64::calculate_main() {

	edb::address_t entry_point = this->entry_point();

	ByteShiftArray ba(14);
	if(IProcess *process = edb::v1::debugger_core->process()) {
		for(int i = 0; i < 50; ++i) {
			quint8 byte;
			if(process->read_bytes(entry_point + i, &byte, sizeof(byte))) {
				ba << byte;

				edb::address_t address = 0;

				if(ba.size() >= 13) {
					// beginning of a call preceeded by a 64-bit mov and followed by a hlt
					if(ba[0] == 0x48 && ba[1] == 0xc7 && ba[7] == 0xe8 && ba[12] == 0xf4) {
						// Seems that this 64-bit mov still has a 32-bit immediate
						address = *reinterpret_cast<const edb::address_t *>(ba.data() + 3) & 0xffffffff;
					}

					// same heuristic except for PIC binaries
					else if (ba.size() >= 14 && ba[0] == 0x48 && ba[1] == 0x8d && ba[2] == 0x3d && ba[7] == 0xFF && ba[8] == 0x15 && ba[13] == 0xf4) {
						auto rel = *reinterpret_cast<const edb::address_t *>(ba.data() + 3) & 0xffffffff;
						// ba[0] is entry_point + i - 13. instruction is 7 bytes long.
						address = rel + entry_point + i - 13 + 7;
					}
					if (address) {
						// TODO: make sure that this address resides in an executable region
						qDebug() << "No main symbol found, calculated it to be " << edb::v1::format_pointer(address) << " using heuristic";
						return address;
					}
				}
			} else {
				break;
			}
		}
	}

	return 0;
}

}
