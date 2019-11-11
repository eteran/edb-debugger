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
#include "edb.h"
#include "string_hash.h"

namespace BinaryInfoPlugin {

/**
 * @brief ELF64::native
 * @return true if this binary is native to the arch edb was built for
 */
template <>
bool ELF64::native() const {
#if defined(EDB_X86) || defined(EDB_X86_64)
	return edb::v1::debugger_core->cpuType() == edb::string_hash("x86-64");
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
	return edb::v1::debugger_core->cpuType() == edb::string_hash("AArch64");
#else
#error "Unsupported Architecture"
#endif
}

}
