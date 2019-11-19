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

#ifndef ELF_SYMINFO_H_20121007_
#define ELF_SYMINFO_H_20121007_

#include "elf_types.h"

/* The syminfo section if available contains additional information about
   every dynamic symbol.  */

struct elf32_syminfo {
	elf32_half si_boundto; /* Direct bindings, symbol bound to */
	elf32_half si_flags;   /* Per symbol flags */
};

struct elf64_syminfo {
	elf64_half si_boundto; /* Direct bindings, symbol bound to */
	elf64_half si_flags;   /* Per symbol flags */
};

/* Possible values for si_boundto.  */
enum {
	SYMINFO_BT_SELF       = 0xffff, /* Symbol bound to self */
	SYMINFO_BT_PARENT     = 0xfffe, /* Symbol bound to parent */
	SYMINFO_BT_LOWRESERVE = 0xff00, /* Beginning of reserved entries */
};

/* Possible bitmasks for si_flags.  */
enum {
	SYMINFO_FLG_DIRECT   = 0x0001, /* Direct bound symbol */
	SYMINFO_FLG_PASSTHRU = 0x0002, /* Pass-thru symbol for translator */
	SYMINFO_FLG_COPY     = 0x0004, /* Symbol is a copy-reloc */
	SYMINFO_FLG_LAZYLOAD = 0x0008, /* Symbol bound to object to be lazy loaded */
};

/* Syminfo version values.  */
enum {
	SYMINFO_NONE    = 0,
	SYMINFO_CURRENT = 1,
	SYMINFO_NUM     = 2,
};

/* How to extract and insert information held in the st_info field.  */
template <class T>
constexpr auto ELF32_ST_BIND(T val) {
	return static_cast<uint8_t>(val) >> 4;
}

template <class T>
constexpr uint8_t ELF32_ST_TYPE(T val) {
	return val & 0xf;
}

template <class T1, class T2>
constexpr auto ELF32_ST_INFO(T1 bind, T2 type) {
	return (bind << 4) + (type & 0xf);
}

/* Both Elf32_Sym and Elf64_Sym use the same one-byte st_info field.  */
template <class T>
constexpr uint8_t ELF64_ST_BIND(T val) {
	return ELF32_ST_BIND(val);
}

template <class T>
constexpr uint8_t ELF64_ST_TYPE(T val) {
	return ELF32_ST_TYPE(val);
}

template <class T1, class T2>
constexpr auto ELF64_ST_INFO(T1 bind, T2 type) {
	return ELF32_ST_INFO(bind, type);
}

/* Legal values for ST_BIND subfield of st_info (symbol binding).  */
enum {
	STB_LOCAL      = 0,  /* Local symbol */
	STB_GLOBAL     = 1,  /* Global symbol */
	STB_WEAK       = 2,  /* Weak symbol */
	STB_NUM        = 3,  /* Number of defined types.  */
	STB_LOOS       = 10, /* Start of OS-specific */
	STB_GNU_UNIQUE = 10, /* Unique symbol.  */
	STB_HIOS       = 12, /* End of OS-specific */
	STB_LOPROC     = 13, /* Start of processor-specific */
	STB_HIPROC     = 15, /* End of processor-specific */
};

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */
enum {
	STT_NOTYPE    = 0,  /* Symbol type is unspecified */
	STT_OBJECT    = 1,  /* Symbol is a data object */
	STT_FUNC      = 2,  /* Symbol is a code object */
	STT_SECTION   = 3,  /* Symbol associated with a section */
	STT_FILE      = 4,  /* Symbol's name is file name */
	STT_COMMON    = 5,  /* Symbol is a common data object */
	STT_TLS       = 6,  /* Symbol is thread-local data object*/
	STT_NUM       = 7,  /* Number of defined types.  */
	STT_LOOS      = 10, /* Start of OS-specific */
	STT_GNU_IFUNC = 10, /* Symbol is indirect code object */
	STT_HIOS      = 12, /* End of OS-specific */
	STT_LOPROC    = 13, /* Start of processor-specific */
	STT_HIPROC    = 15, /* End of processor-specific */
};

/* Symbol table indices are found in the hash buckets and chain table
   of a symbol hash table section.  This special index value indicates
   the end of a chain, meaning no further symbols are found in that bucket.  */
enum {
	STN_UNDEF = 0, /* End of a chain.  */
};

/* How to extract and insert information held in the st_other field.  */

template <class T>
constexpr T ELF32_ST_VISIBILITY(T o) {
	return ((o)&0x03);
}

/* For ELF64 the definitions are the same.  */
template <class T>
constexpr T ELF64_ST_VISIBILITY(T o) {
	return ELF32_ST_VISIBILITY(o);
}

/* Symbol visibility specification encoded in the st_other field.  */
enum {
	STV_DEFAULT   = 0, /* Default symbol visibility rules */
	STV_INTERNAL  = 1, /* Processor specific hidden class */
	STV_HIDDEN    = 2, /* Sym unavailable in other modules */
	STV_PROTECTED = 3, /* Not preemptible, not exported */
};

#endif
