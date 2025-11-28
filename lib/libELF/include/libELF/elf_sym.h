/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
