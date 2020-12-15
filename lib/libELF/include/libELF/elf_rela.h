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

#ifndef ELF_RELA_H_20121007_
#define ELF_RELA_H_20121007_

#include "elf_types.h"

/* Relocation table entry with addend (in section of type SHT_RELA).  */
struct elf32_rela {
	elf32_addr r_offset;  /* Address */
	elf32_word r_info;    /* Relocation type and symbol index */
	elf32_sword r_addend; /* Addend */
};

struct elf64_rela {
	elf64_addr r_offset;   /* Address */
	elf64_xword r_info;    /* Relocation type and symbol index */
	elf64_sxword r_addend; /* Addend */
};

/* How to extract and insert information held in the r_info field.  */
template <class T>
constexpr T ELF32_R_SYM(T val) {
	return val >> 8;
}

template <class T>
constexpr T ELF32_R_TYPE(T val) {
	return val & 0xff;
}

template <class T1, class T2>
constexpr auto ELF32_R_INFO(T1 sym, T2 type) {
	return (sym << 8) + (type & 0xff);
}

template <class T>
constexpr T ELF64_R_SYM(T i) {
	return i >> 32;
}

template <class T>
constexpr T ELF64_R_TYPE(T i) {
	return i & 0xffffffff;
}

template <class T1, class T2>
constexpr auto ELF64_R_INFO(T1 sym, T2 type) {
	return (static_cast<elf64_xword>(sym) << 32) + type;
}

#endif
