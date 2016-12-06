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

#ifndef ELF_VERDEF_20121007_H_
#define ELF_VERDEF_20121007_H_

#include "elf/elf_types.h"

/* Version definition sections.  */

struct elf32_verdef {
	elf32_half vd_version; /* Version revision */
	elf32_half vd_flags;   /* Version information */
	elf32_half vd_ndx;     /* Version Index */
	elf32_half vd_cnt;     /* Number of associated aux entries */
	elf32_word vd_hash;    /* Version name hash value */
	elf32_word vd_aux;     /* Offset in bytes to verdaux array */
	elf32_word vd_next;    /* Offset in bytes to next verdef entry */
};

struct elf64_verdef {
	elf64_half vd_version; /* Version revision */
	elf64_half vd_flags;   /* Version information */
	elf64_half vd_ndx;     /* Version Index */
	elf64_half vd_cnt;     /* Number of associated aux entries */
	elf64_word vd_hash;    /* Version name hash value */
	elf64_word vd_aux;     /* Offset in bytes to verdaux array */
	elf64_word vd_next;    /* Offset in bytes to next verdef entry */
};


/* Legal values for vd_version (version revision).  */
#define VER_DEF_NONE    0 /* No version */
#define VER_DEF_CURRENT 1 /* Current version */
#define VER_DEF_NUM     2 /* Given version number */

/* Legal values for vd_flags (version information flags).  */
#define VER_FLG_BASE 0x1 /* Version definition of file itself */
#define VER_FLG_WEAK 0x2 /* Weak version identifier */

/* Versym symbol index values.  */
#define VER_NDX_LOCAL     0      /* Symbol is local.  */
#define VER_NDX_GLOBAL    1      /* Symbol is global.  */
#define VER_NDX_LORESERVE 0xff00 /* Beginning of reserved entries.  */
#define VER_NDX_ELIMINATE 0xff01 /* Symbol is to be eliminated.  */

#endif
