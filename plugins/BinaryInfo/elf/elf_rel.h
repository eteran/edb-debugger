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

#ifndef ELF_REL_20121007_H_
#define ELF_REL_20121007_H_

#include "elf/elf_types.h"

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
	elf64_addr  r_offset; /* Address */
	elf64_xword r_info;   /* Relocation type and symbol index */
};

#endif
