/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELF_VERDAUX_H_20121007_
#define ELF_VERDAUX_H_20121007_

#include "elf_types.h"

/* Auxialiary version information.  */
struct elf32_verdaux {
	elf32_word vda_name; /* Version or dependency names */
	elf32_word vda_next; /* Offset in bytes to next verdaux entry */
};

struct elf64_verdaux {
	elf64_word vda_name; /* Version or dependency names */
	elf64_word vda_next; /* Offset in bytes to next verdaux entry */
};

#endif
