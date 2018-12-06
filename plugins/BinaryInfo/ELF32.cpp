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
#include "edb.h"
#include "string_hash.h"
#include "IDebugger.h"

namespace BinaryInfoPlugin {

//------------------------------------------------------------------------------
// Name: native
// Desc: returns true if this binary is native to the arch edb was built for
//------------------------------------------------------------------------------
template<>
bool ELF32::native() const {
	return edb::v1::debugger_core->cpu_type() == edb::string_hash("x86");
}



}
