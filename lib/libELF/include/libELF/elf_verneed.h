/*
 * Copyright (C) 2012 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * Copyright (C) 1995-2003,2004,2005,2006,2007,2008,2009,2010,2011
 * Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ELF_VERNEED_H_20121007_
#define ELF_VERNEED_H_20121007_

#include "elf_types.h"

/* Version dependency section.  */
struct elf32_verneed {
	elf32_half vn_version; /* Version of structure */
	elf32_half vn_cnt;     /* Number of associated aux entries */
	elf32_word vn_file;    /* Offset of filename for this dependency */
	elf32_word vn_aux;     /* Offset in bytes to vernaux array */
	elf32_word vn_next;    /* Offset in bytes to next verneed entry */
};

struct elf64_verneed {
	elf64_half vn_version; /* Version of structure */
	elf64_half vn_cnt;     /* Number of associated aux entries */
	elf64_word vn_file;    /* Offset of filename for this dependency */
	elf64_word vn_aux;     /* Offset in bytes to vernaux array */
	elf64_word vn_next;    /* Offset in bytes to next verneed entry */
};

/* Legal values for vn_version (version revision).  */
enum {
	VER_NEED_NONE    = 0, /* No version */
	VER_NEED_CURRENT = 1, /* Current version */
	VER_NEED_NUM     = 2  /* Given version number */
};

#endif
