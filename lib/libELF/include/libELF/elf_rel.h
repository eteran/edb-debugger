/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELF_REL_H_20121007_
#define ELF_REL_H_20121007_

#include "elf_types.h"

// Relocation table entry without addend (in section of type SHT_REL).
struct elf32_rel {
	elf32_addr r_offset; /* Address */
	elf32_word r_info;   /* Relocation type and symbol index */
};

/* I have seen two different definitions of the Elf64_Rel and
   Elf64_Rela structures, so we'll leave them out until Novell (or
   whoever) gets their act together.  */
/* The following, at least, is used on Sparc v9, MIPS, and Alpha.  */
struct elf64_rel {
	elf64_addr r_offset; /* Address */
	elf64_xword r_info;  /* Relocation type and symbol index */
};

#endif
