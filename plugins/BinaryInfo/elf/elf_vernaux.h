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

#ifndef ELF_VERNAUX_20121007_H_
#define ELF_VERNAUX_20121007_H_

#include "elf/elf_types.h"

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
#define VER_FLG_WEAK 0x2 /* Weak version identifier */

#endif
