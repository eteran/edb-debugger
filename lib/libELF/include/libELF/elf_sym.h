/*
Copyright (C) 2012 - 2023 Evan Teran
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

#ifndef ELF_SYM_H_20121007_
#define ELF_SYM_H_20121007_

#include "elf_types.h"

// Symbol table entry.

struct elf32_sym {
	elf32_word st_name;     // Symbol name (string tbl index)
	elf32_addr st_value;    // Symbol value
	elf32_word st_size;     // Symbol size
	uint8_t st_info;        // Symbol type and binding
	uint8_t st_other;       // Symbol visibility
	elf32_section st_shndx; // Section index
};

struct elf64_sym {
	elf64_word st_name;     // Symbol name (string tbl index)
	uint8_t st_info;        // Symbol type and binding
	uint8_t st_other;       // Symbol visibility
	elf64_section st_shndx; // Section index
	elf64_addr st_value;    // Symbol value
	elf64_xword st_size;    // Symbol size
};

#endif
