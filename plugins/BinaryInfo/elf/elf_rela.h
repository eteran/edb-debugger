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

#ifndef ELF_RELA_20121007_H_
#define ELF_RELA_20121007_H_

#include "elf/elf_types.h"

/* Relocation table entry with addend (in section of type SHT_RELA).  */

struct elf32_rela
{
  elf32_addr  r_offset; /* Address */
  elf32_word  r_info;   /* Relocation type and symbol index */
  elf32_sword r_addend; /* Addend */
};

struct elf64_rela
{
  elf64_addr   r_offset; /* Address */
  elf64_xword  r_info;   /* Relocation type and symbol index */
  elf64_sxword r_addend; /* Addend */
};

/* How to extract and insert information held in the r_info field.  */

#define ELF32_R_SYM(val)        ((val) >> 8)
#define ELF32_R_TYPE(val)       ((val) & 0xff)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type) & 0xff))

#define ELF64_R_SYM(i)         ((i) >> 32)
#define ELF64_R_TYPE(i)        ((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type) ((((elf64_xword) (sym)) << 32) + (type))


#endif
