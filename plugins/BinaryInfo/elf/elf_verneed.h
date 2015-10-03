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

#ifndef ELF_VERNEED_20121007_H_
#define ELF_VERNEED_20121007_H_

#include "elf/elf_types.h"

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
#define VER_NEED_NONE    0 /* No version */
#define VER_NEED_CURRENT 1 /* Current version */
#define VER_NEED_NUM     2 /* Given version number */

#endif
