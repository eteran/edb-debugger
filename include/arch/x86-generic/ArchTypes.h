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

#ifndef ARCHTYPES_20071127_H_
#define ARCHTYPES_20071127_H_

#include <cstdint>
#include <QtGlobal>
#include "Instruction.h"

#if INTPTR_MAX==INT32_MAX

#define EDB_X86
static constexpr const bool EDB_IS_64_BIT=false;
static constexpr const bool EDB_IS_32_BIT=true;

#elif INTPTR_MAX==INT64_MAX

#define EDB_X86_64
static constexpr const bool EDB_IS_64_BIT=true;
static constexpr const bool EDB_IS_32_BIT=false;

#endif

namespace edb {
	typedef value16                                    seg_reg_t;
	typedef CapstoneEDB::Instruction                   Instruction;
	typedef Instruction::operand_type                  Operand;
}

#endif
