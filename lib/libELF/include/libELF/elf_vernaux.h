/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELF_VERNAUX_H_20121007_
#define ELF_VERNAUX_H_20121007_

#include "elf_types.h"

/* Auxiliary needed version information.  */

struct elf32_vernaux {
	elf32_word vna_hash;  /* Hash value of dependency name */
	elf32_half vna_flags; /* Dependency specific information */
	elf32_half vna_other; /* Unused */
	elf32_word vna_name;  /* Dependency name string offset */
	elf32_word vna_next;  /* Offset in bytes to next vernaux entry */
};

struct elf64_vernaux {
	elf64_word vna_hash;  /* Hash value of dependency name */
	elf64_half vna_flags; /* Dependency specific information */
	elf64_half vna_other; /* Unused */
	elf64_word vna_name;  /* Dependency name string offset */
	elf64_word vna_next;  /* Offset in bytes to next vernaux entry */
};

/* Legal values for vna_flags.  */
#if 0
enum {
	VER_FLG_WEAK = 0x2 /* Weak version identifier */
};
#endif

#endif
