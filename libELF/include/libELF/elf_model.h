/*
Copyright (C) 2018 - 2018 Evan Teran
                          evan.teran@gmail.com

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

#ifndef ELF_MODEL_H_20181206_
#define ELF_MODEL_H_20181206_

#include "elf_auxv.h"
#include "elf_dyn.h"
#include "elf_header.h"
#include "elf_move.h"
#include "elf_nhdr.h"
#include "elf_phdr.h"
#include "elf_rel.h"
#include "elf_rela.h"
#include "elf_shdr.h"
#include "elf_sym.h"
#include "elf_syminfo.h"
#include "elf_types.h"
#include "elf_verdaux.h"
#include "elf_verdef.h"
#include "elf_vernaux.h"
#include "elf_verneed.h"

template <int>
struct elf_model;

template <>
struct elf_model<64> {
	// types
	using elf_half    = elf64_half;
	using elf_word    = elf64_word;
	using elf_sword   = elf64_sword;
	using elf_xword   = elf64_xword;
	using elf_sxword  = elf64_sxword;
	using elf_addr    = elf64_addr;
	using elf_off     = elf64_off;
	using elf_section = elf64_section;
	using elf_versym  = elf64_versym;

	// structures
	using elf_auxv_t = elf64_auxv_t;
	using elf_dyn    = elf64_dyn;
	using elf_header = elf64_header;
#if 0
	using elf_lib	  =  elf64_lib;
#endif
	using elf_move    = elf64_move;
	using elf_nhdr    = elf64_nhdr;
	using elf_phdr    = elf64_phdr;
	using elf_rel     = elf64_rel;
	using elf_rela    = elf64_rela;
	using elf_shdr    = elf64_shdr;
	using elf_sym     = elf64_sym;
	using elf_syminfo = elf64_syminfo;
	using elf_verdaux = elf64_verdaux;
	using elf_verdef  = elf64_verdef;
	using elf_vernaux = elf64_vernaux;
	using elf_verneed = elf64_verneed;
};

template <>
struct elf_model<32> {
	// types
	using elf_half    = elf32_half;
	using elf_word    = elf32_word;
	using elf_sword   = elf32_sword;
	using elf_xword   = elf32_xword;
	using elf_sxword  = elf32_sxword;
	using elf_addr    = elf32_addr;
	using elf_off     = elf32_off;
	using elf_section = elf32_section;
	using elf_versym  = elf32_versym;

	// structures
	using elf_auxv_t = elf32_auxv_t;
	using elf_dyn    = elf32_dyn;
	using elf_header = elf32_header;
#if 0
	using elf_lib	  =  elf32_lib;
#endif
	using elf_move    = elf32_move;
	using elf_nhdr    = elf32_nhdr;
	using elf_phdr    = elf32_phdr;
	using elf_rel     = elf32_rel;
	using elf_rela    = elf32_rela;
	using elf_shdr    = elf32_shdr;
	using elf_sym     = elf32_sym;
	using elf_syminfo = elf32_syminfo;
	using elf_verdaux = elf32_verdaux;
	using elf_verdef  = elf32_verdef;
	using elf_vernaux = elf32_vernaux;
	using elf_verneed = elf32_verneed;
};

#endif
