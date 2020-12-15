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

#ifndef ELF_SHDR_H_20121007_
#define ELF_SHDR_H_20121007_

#include "elf_types.h"

// Section header.
struct elf32_shdr {
	elf32_word sh_name;      /* Section name (string tbl index) */
	elf32_word sh_type;      /* Section type */
	elf32_word sh_flags;     /* Section flags */
	elf32_addr sh_addr;      /* Section virtual addr at execution */
	elf32_off sh_offset;     /* Section file offset */
	elf32_word sh_size;      /* Section size in bytes */
	elf32_word sh_link;      /* Link to another section */
	elf32_word sh_info;      /* Additional section information */
	elf32_word sh_addralign; /* Section alignment */
	elf32_word sh_entsize;   /* Entry size if section holds table */
};

struct elf64_shdr {
	elf64_word sh_name;       /* Section name (string tbl index) */
	elf64_word sh_type;       /* Section type */
	elf64_xword sh_flags;     /* Section flags */
	elf64_addr sh_addr;       /* Section virtual addr at execution */
	elf64_off sh_offset;      /* Section file offset */
	elf64_xword sh_size;      /* Section size in bytes */
	elf64_word sh_link;       /* Link to another section */
	elf64_word sh_info;       /* Additional section information */
	elf64_xword sh_addralign; /* Section alignment */
	elf64_xword sh_entsize;   /* Entry size if section holds table */
};

/* Special section indices.  */
enum {
	SHN_UNDEF     = 0,      /* Undefined section */
	SHN_LORESERVE = 0xff00, /* Start of reserved indices */
	SHN_LOPROC    = 0xff00, /* Start of processor-specific */
	SHN_BEFORE    = 0xff00, /* Order section before all others (Solaris).  */
	SHN_AFTER     = 0xff01, /* Order section after all others (Solaris).  */
	SHN_HIPROC    = 0xff1f, /* End of processor-specific */
	SHN_LOOS      = 0xff20, /* Start of OS-specific */
	SHN_HIOS      = 0xff3f, /* End of OS-specific */
	SHN_ABS       = 0xfff1, /* Associated symbol is absolute */
	SHN_COMMON    = 0xfff2, /* Associated symbol is common */
	SHN_XINDEX    = 0xffff, /* Index is in extra table.  */
	SHN_HIRESERVE = 0xffff, /* End of reserved indices */
};

/* Legal values for sh_type (section type).  */
enum {
	SHT_NULL           = 0,          /* Section header table entry unused */
	SHT_PROGBITS       = 1,          /* Program data */
	SHT_SYMTAB         = 2,          /* Symbol table */
	SHT_STRTAB         = 3,          /* String table */
	SHT_RELA           = 4,          /* Relocation entries with addends */
	SHT_HASH           = 5,          /* Symbol hash table */
	SHT_DYNAMIC        = 6,          /* Dynamic linking information */
	SHT_NOTE           = 7,          /* Notes */
	SHT_NOBITS         = 8,          /* Program space with no data (bss) */
	SHT_REL            = 9,          /* Relocation entries, no addends */
	SHT_SHLIB          = 10,         /* Reserved */
	SHT_DYNSYM         = 11,         /* Dynamic linker symbol table */
	SHT_INIT_ARRAY     = 14,         /* Array of constructors */
	SHT_FINI_ARRAY     = 15,         /* Array of destructors */
	SHT_PREINIT_ARRAY  = 16,         /* Array of pre-constructors */
	SHT_GROUP          = 17,         /* Section group */
	SHT_SYMTAB_SHNDX   = 18,         /* Extended section indeces */
	SHT_NUM            = 19,         /* Number of defined types.  */
	SHT_LOOS           = 0x60000000, /* Start OS-specific.  */
	SHT_GNU_ATTRIBUTES = 0x6ffffff5, /* Object attributes.  */
	SHT_GNU_HASH       = 0x6ffffff6, /* GNU-style hash table.  */
	SHT_GNU_LIBLIST    = 0x6ffffff7, /* Prelink library list */
	SHT_CHECKSUM       = 0x6ffffff8, /* Checksum for DSO content.  */
	SHT_LOSUNW         = 0x6ffffffa, /* Sun-specific low bound.  */
	SHT_SUNW_move      = 0x6ffffffa,
	SHT_SUNW_COMDAT    = 0x6ffffffb,
	SHT_SUNW_syminfo   = 0x6ffffffc,
	SHT_GNU_verdef     = 0x6ffffffd, /* Version definition section.  */
	SHT_GNU_verneed    = 0x6ffffffe, /* Version needs section.  */
	SHT_GNU_versym     = 0x6fffffff, /* Version symbol table.  */
	SHT_HISUNW         = 0x6fffffff, /* Sun-specific high bound.  */
	SHT_HIOS           = 0x6fffffff, /* End OS-specific type */
	SHT_LOPROC         = 0x70000000, /* Start of processor-specific */
	SHT_HIPROC         = 0x7fffffff, /* End of processor-specific */
	SHT_LOUSER         = 0x80000000, /* Start of application-specific */
	SHT_HIUSER         = 0x8fffffff, /* End of application-specific */
};

/* Legal values for sh_flags (section flags).  */
enum {
	SHF_WRITE            = (1U << 0),  /* Writable */
	SHF_ALLOC            = (1U << 1),  /* Occupies memory during execution */
	SHF_EXECINSTR        = (1U << 2),  /* Executable */
	SHF_MERGE            = (1U << 4),  /* Might be merged */
	SHF_STRINGS          = (1U << 5),  /* Contains nul-terminated strings */
	SHF_INFO_LINK        = (1U << 6),  /* `sh_info' contains SHT index */
	SHF_LINK_ORDER       = (1U << 7),  /* Preserve order after combining */
	SHF_OS_NONCONFORMING = (1U << 8),  /* Non-standard OS specific handling required */
	SHF_GROUP            = (1U << 9),  /* Section is member of a group.  */
	SHF_TLS              = (1U << 10), /* Section hold thread-local data.  */
	SHF_MASKOS           = 0x0ff00000, /* OS-specific.  */
	SHF_MASKPROC         = 0xf0000000, /* Processor-specific */
	SHF_ORDERED          = (1U << 30), /* Special ordering requirement (Solaris).  */
	SHF_EXCLUDE          = (1U << 31), /* Section is excluded unless referenced or allocated (Solaris).*/
};

/* Section group handling.  */
enum {
	GRP_COMDAT = 0x1, /* Mark group as COMDAT.  */
};

#endif
