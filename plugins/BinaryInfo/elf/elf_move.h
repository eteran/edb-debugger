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

#ifndef ELF_MOVE_20121007_H_
#define ELF_MOVE_20121007_H_

#include "elf/elf_types.h"

// Move records.  
struct elf32_move {
	elf32_xword m_value;  // Symbol value.  
	elf32_word m_info;    // Size and index.  
	elf32_word m_poffset; // Symbol offset.  
	elf32_half m_repeat;  // Repeat count.  
	elf32_half m_stride;  // Stride info.  
};

struct elf64_move {
	elf64_xword m_value;   // Symbol value.  
	elf64_xword m_info;    // Size and index.  
	elf64_xword m_poffset; // Symbol offset.  
	elf64_half m_repeat;   // Repeat count.  
	elf64_half m_stride;   // Stride info.  
};

// Macro to construct move records.  
#define ELF32_M_SYM(info)       ((info) >> 8)
#define ELF32_M_SIZE(info)      ((unsigned char) (info))
#define ELF32_M_INFO(sym, size) (((sym) << 8) + (unsigned char) (size))

#define ELF64_M_SYM(info)       ELF32_M_SYM (info)
#define ELF64_M_SIZE(info)      ELF32_M_SIZE (info)
#define ELF64_M_INFO(sym, size) ELF32_M_INFO (sym, size)

#endif
