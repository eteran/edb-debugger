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

#ifndef ELF_TYPES_20121007_H_
#define ELF_TYPES_20121007_H_

#if defined(_MSC_VER) && _MSC_VER < 1600
  typedef unsigned __int8  uint8_t;
  typedef unsigned __int16 uint16_t;
  typedef unsigned __int32 uint32_t;
  typedef unsigned __int64 uint64_t;
  typedef __int8           int8_t;
  typedef __int16          int16_t;
  typedef __int32          int32_t;
  typedef __int64          int64_t;
#else
	#include <stdint.h>
#endif

/* Type for a 16-bit quantity.  */
typedef uint16_t elf32_half;
typedef uint16_t elf64_half;

/* Types for signed and unsigned 32-bit quantities.  */
typedef uint32_t elf32_word;
typedef int32_t  elf32_sword;
typedef uint32_t elf64_word;
typedef int32_t  elf64_sword;

/* Types for signed and unsigned 64-bit quantities.  */
typedef uint64_t elf32_xword;
typedef int64_t  elf32_sxword;
typedef uint64_t elf64_xword;
typedef int64_t  elf64_sxword;

/* Type of addresses.  */
typedef uint32_t elf32_addr;
typedef uint64_t elf64_addr;

/* Type of file offsets.  */
typedef uint32_t elf32_off;
typedef uint64_t elf64_off;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t elf32_section;
typedef uint16_t elf64_section;

/* Type for version symbol information.  */
typedef elf32_half elf32_versym;
typedef elf64_half elf64_versym;

#endif
