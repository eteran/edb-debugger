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

#ifndef ELF_DYN_H_20121007_
#define ELF_DYN_H_20121007_

#include "elf_types.h"

/* Dynamic section entry.  */

struct elf32_dyn {
	elf32_sword d_tag; /* Dynamic entry type */
	union {
		elf32_word d_val; /* Integer value */
		elf32_addr d_ptr; /* Address value */
	} d_un;
};

struct elf64_dyn {
	elf64_sxword d_tag; /* Dynamic entry type */
	union {
		elf64_xword d_val; /* Integer value */
		elf64_addr d_ptr;  /* Address value */
	} d_un;
};

/* Legal values for d_tag field of Elf32_Dyn.  */
enum {
	DT_MIPS_RLD_VERSION           = 0x70000001, /* Runtime linker interface version */
	DT_MIPS_TIME_STAMP            = 0x70000002, /* Timestamp */
	DT_MIPS_ICHECKSUM             = 0x70000003, /* Checksum */
	DT_MIPS_IVERSION              = 0x70000004, /* Version string (string tbl index) */
	DT_MIPS_FLAGS                 = 0x70000005, /* Flags */
	DT_MIPS_BASE_ADDRESS          = 0x70000006, /* Base address */
	DT_MIPS_MSYM                  = 0x70000007,
	DT_MIPS_CONFLICT              = 0x70000008, /* Address of CONFLICT section */
	DT_MIPS_LIBLIST               = 0x70000009, /* Address of LIBLIST section */
	DT_MIPS_LOCAL_GOTNO           = 0x7000000a, /* Number of local GOT entries */
	DT_MIPS_CONFLICTNO            = 0x7000000b, /* Number of CONFLICT entries */
	DT_MIPS_LIBLISTNO             = 0x70000010, /* Number of LIBLIST entries */
	DT_MIPS_SYMTABNO              = 0x70000011, /* Number of DYNSYM entries */
	DT_MIPS_UNREFEXTNO            = 0x70000012, /* First external DYNSYM */
	DT_MIPS_GOTSYM                = 0x70000013, /* First GOT entry in DYNSYM */
	DT_MIPS_HIPAGENO              = 0x70000014, /* Number of GOT page table entries */
	DT_MIPS_RLD_MAP               = 0x70000016, /* Address of run time loader map.  */
	DT_MIPS_DELTA_CLASS           = 0x70000017, /* Delta C++ class definition.  */
	DT_MIPS_DELTA_CLASS_NO        = 0x70000018, /* Number of entries in DT_MIPS_DELTA_CLASS.  */
	DT_MIPS_DELTA_INSTANCE        = 0x70000019, /* Delta C++ class instances.  */
	DT_MIPS_DELTA_INSTANCE_NO     = 0x7000001a, /* Number of entries in DT_MIPS_DELTA_INSTANCE.  */
	DT_MIPS_DELTA_RELOC           = 0x7000001b, /* Delta relocations.  */
	DT_MIPS_DELTA_RELOC_NO        = 0x7000001c, /* Number of entries in DT_MIPS_DELTA_RELOC.  */
	DT_MIPS_DELTA_SYM             = 0x7000001d, /* Delta symbols that Delta    relocations refer to.  */
	DT_MIPS_DELTA_SYM_NO          = 0x7000001e, /* Number of entries in    DT_MIPS_DELTA_SYM.  */
	DT_MIPS_DELTA_CLASSSYM        = 0x70000020, /* Delta symbols that hold the	class declaration.  */
	DT_MIPS_DELTA_CLASSSYM_NO     = 0x70000021, /* Number of entries in DT_MIPS_DELTA_CLASSSYM.  */
	DT_MIPS_CXX_FLAGS             = 0x70000022, /* Flags indicating for C++ flavor.  */
	DT_MIPS_PIXIE_INIT            = 0x70000023,
	DT_MIPS_SYMBOL_LIB            = 0x70000024,
	DT_MIPS_LOCALPAGE_GOTIDX      = 0x70000025,
	DT_MIPS_LOCAL_GOTIDX          = 0x70000026,
	DT_MIPS_HIDDEN_GOTIDX         = 0x70000027,
	DT_MIPS_PROTECTED_GOTIDX      = 0x70000028,
	DT_MIPS_OPTIONS               = 0x70000029, /* Address of .options.  */
	DT_MIPS_INTERFACE             = 0x7000002a, /* Address of .interface.  */
	DT_MIPS_DYNSTR_ALIGN          = 0x7000002b,
	DT_MIPS_INTERFACE_SIZE        = 0x7000002c, /* Size of the .interface section. */
	DT_MIPS_RLD_TEXT_RESOLVE_ADDR = 0x7000002d, /* Address of rld_text_rsolve function stored in GOT.  */
	DT_MIPS_PERF_SUFFIX           = 0x7000002e, /* Default suffix of dso to be added by rld on dlopen() calls.  */
	DT_MIPS_COMPACT_SIZE          = 0x7000002f, /* (O32)Size of compact rel section. */
	DT_MIPS_GP_VALUE              = 0x70000030, /* GP value for aux GOTs.  */
	DT_MIPS_AUX_DYNAMIC           = 0x70000031, /* Address of aux .dynamic.  */

	/* The address of .got.plt in an executable using the new non-PIC ABI.  */
	DT_MIPS_PLTGOT = 0x70000032,

	/* The base of the PLT in an executable using the new non-PIC ABI if that
	   PLT is writable.  For a non-writable PLT, this is omitted or has a zero
	   value.  */
	DT_MIPS_RWPLT = 0x70000034,
	DT_MIPS_NUM   = 0x35,
};

/* Legal values for d_tag (dynamic entry type).  */
enum {
	DT_NULL            = 0,          /* Marks end of dynamic section */
	DT_NEEDED          = 1,          /* Name of needed library */
	DT_PLTRELSZ        = 2,          /* Size in bytes of PLT relocs */
	DT_PLTGOT          = 3,          /* Processor defined value */
	DT_HASH            = 4,          /* Address of symbol hash table */
	DT_STRTAB          = 5,          /* Address of string table */
	DT_SYMTAB          = 6,          /* Address of symbol table */
	DT_RELA            = 7,          /* Address of Rela relocs */
	DT_RELASZ          = 8,          /* Total size of Rela relocs */
	DT_RELAENT         = 9,          /* Size of one Rela reloc */
	DT_STRSZ           = 10,         /* Size of string table */
	DT_SYMENT          = 11,         /* Size of one symbol table entry */
	DT_INIT            = 12,         /* Address of init function */
	DT_FINI            = 13,         /* Address of termination function */
	DT_SONAME          = 14,         /* Name of shared object */
	DT_RPATH           = 15,         /* Library search path (deprecated) */
	DT_SYMBOLIC        = 16,         /* Start symbol search here */
	DT_REL             = 17,         /* Address of Rel relocs */
	DT_RELSZ           = 18,         /* Total size of Rel relocs */
	DT_RELENT          = 19,         /* Size of one Rel reloc */
	DT_PLTREL          = 20,         /* Type of reloc in PLT */
	DT_DEBUG           = 21,         /* For debugging; unspecified */
	DT_TEXTREL         = 22,         /* Reloc might modify .text */
	DT_JMPREL          = 23,         /* Address of PLT relocs */
	DT_BIND_NOW        = 24,         /* Process relocations of object */
	DT_INIT_ARRAY      = 25,         /* Array with addresses of init fct */
	DT_FINI_ARRAY      = 26,         /* Array with addresses of fini fct */
	DT_INIT_ARRAYSZ    = 27,         /* Size in bytes of DT_INIT_ARRAY */
	DT_FINI_ARRAYSZ    = 28,         /* Size in bytes of DT_FINI_ARRAY */
	DT_RUNPATH         = 29,         /* Library search path */
	DT_FLAGS           = 30,         /* Flags for the object being loaded */
	DT_ENCODING        = 32,         /* Start of encoded range */
	DT_PREINIT_ARRAY   = 32,         /* Array with addresses of preinit fct*/
	DT_PREINIT_ARRAYSZ = 33,         /* size in bytes of DT_PREINIT_ARRAY */
	DT_NUM             = 34,         /* Number used */
	DT_LOOS            = 0x6000000d, /* Start of OS-specific */
	DT_HIOS            = 0x6ffff000, /* End of OS-specific */
	DT_LOPROC          = 0x70000000, /* Start of processor-specific */
	DT_HIPROC          = 0x7fffffff, /* End of processor-specific */

	DT_PROCNUM = DT_MIPS_NUM /* Most used by any processor */
};

/* DT_* entries which fall between DT_VALRNGHI & DT_VALRNGLO use the
   Dyn.d_un.d_val field of the Elf*_Dyn structure.  This follows Sun's
   approach.  */
enum {
	DT_VALRNGLO       = 0x6ffffd00,
	DT_GNU_PRELINKED  = 0x6ffffdf5, /* Prelinking timestamp */
	DT_GNU_CONFLICTSZ = 0x6ffffdf6, /* Size of conflict section */
	DT_GNU_LIBLISTSZ  = 0x6ffffdf7, /* Size of library list */
	DT_CHECKSUM       = 0x6ffffdf8,
	DT_PLTPADSZ       = 0x6ffffdf9,
	DT_MOVEENT        = 0x6ffffdfa,
	DT_MOVESZ         = 0x6ffffdfb,
	DT_FEATURE_1      = 0x6ffffdfc, /* Feature selection (DTF_*).  */
	DT_POSFLAG_1      = 0x6ffffdfd, /* Flags for DT_* entries, effecting the following DT_* entry.  */
	DT_SYMINSZ        = 0x6ffffdfe, /* Size of syminfo table (in bytes) */
	DT_SYMINENT       = 0x6ffffdff, /* Entry size of syminfo */
	DT_VALRNGHI       = 0x6ffffdff,
	DT_VALNUM         = 12,
};

constexpr inline uint32_t DT_VALTAGIDX(uint32_t tag) {
	return DT_VALRNGHI - tag; /* Reverse order! */
}

/* DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
   Dyn.d_un.d_ptr field of the Elf*_Dyn structure.

   If any adjustment is made to the ELF object after it has been
   built these entries will need to be adjusted.  */
enum {
	DT_ADDRRNGLO    = 0x6ffffe00,
	DT_GNU_HASH     = 0x6ffffef5, /* GNU-style hash table.  */
	DT_TLSDESC_PLT  = 0x6ffffef6,
	DT_TLSDESC_GOT  = 0x6ffffef7,
	DT_GNU_CONFLICT = 0x6ffffef8, /* Start of conflict section */
	DT_GNU_LIBLIST  = 0x6ffffef9, /* Library list */
	DT_CONFIG       = 0x6ffffefa, /* Configuration information.  */
	DT_DEPAUDIT     = 0x6ffffefb, /* Dependency auditing.  */
	DT_AUDIT        = 0x6ffffefc, /* Object auditing.  */
	DT_PLTPAD       = 0x6ffffefd, /* PLT padding.  */
	DT_MOVETAB      = 0x6ffffefe, /* Move table.  */
	DT_SYMINFO      = 0x6ffffeff, /* Syminfo table.  */
	DT_ADDRRNGHI    = 0x6ffffeff,
	DT_ADDRNUM      = 11,
};

constexpr inline uint32_t DT_ADDRTAGIDX(uint32_t tag) {
	return DT_ADDRRNGHI - tag; /* Reverse order! */
}

/* The versioning entry types.  The next are defined as part of the
   GNU extension.  */
enum {
	DT_VERSYM = 0x6ffffff0,
};

enum {
	DT_RELACOUNT = 0x6ffffff9,
	DT_RELCOUNT  = 0x6ffffffa,
};

/* These were chosen by Sun.  */
enum {
	DT_FLAGS_1       = 0x6ffffffb, /* State flags, see DF_1_* below.  */
	DT_VERDEF        = 0x6ffffffc, /* Address of version definition table */
	DT_VERDEFNUM     = 0x6ffffffd, /* Number of version definitions */
	DT_VERNEED       = 0x6ffffffe, /* Address of table with needed versions */
	DT_VERNEEDNUM    = 0x6fffffff, /* Number of needed versions */
	DT_VERSIONTAGNUM = 16,
};

constexpr inline uint32_t DT_VERSIONTAGIDX(uint32_t tag) {
	return DT_VERNEEDNUM - tag; /* Reverse order! */
}

/* Sun added these machine-independent extensions in the "processor-specific"
   range.  Be compatible.  */
enum {
	DT_AUXILIARY = 0x7ffffffd, /* Shared object to load before self */
	DT_FILTER    = 0x7fffffff, /* Shared object to get values from */
	DT_EXTRANUM  = 3,
};

constexpr inline uint32_t DT_EXTRATAGIDX(uint32_t tag) {
	return static_cast<elf32_word>(-(static_cast<elf32_sword>(tag) << 1 >> 1) - 1);
}

/* Values of `d_un.d_val' in the DT_FLAGS entry.  */
enum {
	DF_ORIGIN     = 0x00000001, /* Object may use DF_ORIGIN */
	DF_SYMBOLIC   = 0x00000002, /* Symbol resolutions starts here */
	DF_TEXTREL    = 0x00000004, /* Object contains text relocations */
	DF_BIND_NOW   = 0x00000008, /* No lazy binding for this object */
	DF_STATIC_TLS = 0x00000010, /* Module uses the static TLS model */
};

/* State flags selectable in the `d_un.d_val' element of the DT_FLAGS_1
   entry in the dynamic section.  */
enum {
	DF_1_NOW        = 0x00000001, /* Set RTLD_NOW for this object.  */
	DF_1_GLOBAL     = 0x00000002, /* Set RTLD_GLOBAL for this object.  */
	DF_1_GROUP      = 0x00000004, /* Set RTLD_GROUP for this object.  */
	DF_1_NODELETE   = 0x00000008, /* Set RTLD_NODELETE for this object.*/
	DF_1_LOADFLTR   = 0x00000010, /* Trigger filtee loading at runtime.*/
	DF_1_INITFIRST  = 0x00000020, /* Set RTLD_INITFIRST for this object*/
	DF_1_NOOPEN     = 0x00000040, /* Set RTLD_NOOPEN for this object.  */
	DF_1_ORIGIN     = 0x00000080, /* $ORIGIN must be handled.  */
	DF_1_DIRECT     = 0x00000100, /* Direct binding enabled.  */
	DF_1_TRANS      = 0x00000200,
	DF_1_INTERPOSE  = 0x00000400, /* Object is used to interpose.  */
	DF_1_NODEFLIB   = 0x00000800, /* Ignore default lib search path.  */
	DF_1_NODUMP     = 0x00001000, /* Object can't be dldump'ed.  */
	DF_1_CONFALT    = 0x00002000, /* Configuration alternative created.*/
	DF_1_ENDFILTEE  = 0x00004000, /* Filtee terminates filters search. */
	DF_1_DISPRELDNE = 0x00008000, /* Disp reloc applied at build time. */
	DF_1_DISPRELPND = 0x00010000, /* Disp reloc applied at run-time.  */
};

/* Flags for the feature selection in DT_FEATURE_1.  */
enum {
	DTF_1_PARINIT = 0x00000001,
	DTF_1_CONFEXP = 0x00000002,
};

/* Flags in the DT_POSFLAG_1 entry effecting only the next DT_* entry.  */
enum {
	DF_P1_LAZYLOAD  = 0x00000001, /* Lazyload following object.  */
	DF_P1_GROUPPERM = 0x00000002, /* Symbols from next object are not generally available.  */
};

#endif
