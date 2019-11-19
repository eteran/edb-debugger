/*
Copyright (C) 2012 - 2015 Evan Teran
                          evan.teran@gmail.com

Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
                   Free Software Foundation, Inc.

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

#ifndef ELF_TYPES_H_20121007_
#define ELF_TYPES_H_20121007_

#include <cstdint>

/* Type for a 16-bit quantity.  */
using elf32_half = uint16_t;
using elf64_half = uint16_t;

/* Types for signed and unsigned 32-bit quantities.  */
using elf32_word  = uint32_t;
using elf32_sword = int32_t;
using elf64_word  = uint32_t;
using elf64_sword = int32_t;

/* Types for signed and unsigned 64-bit quantities.  */
using elf32_xword  = uint64_t;
using elf32_sxword = int64_t;
using elf64_xword  = uint64_t;
using elf64_sxword = int64_t;

/* Type of addresses.  */
using elf32_addr = uint32_t;
using elf64_addr = uint64_t;

/* Type of file offsets.  */
using elf32_off = uint32_t;
using elf64_off = uint64_t;

/* Type for section indices, which are 16-bit quantities.  */
using elf32_section = uint16_t;
using elf64_section = uint16_t;

/* Type for version symbol information.  */
using elf32_versym = elf32_half;
using elf64_versym = elf64_half;

#endif
