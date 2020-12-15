/*
Copyright (C) 2012 Evan Teran
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

#ifndef ELF_BINARY_H_20121007_
#define ELF_BINARY_H_20121007_

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

/* Motorola 68k specific definitions.  */

/* Values for Elf32_Ehdr.e_flags.  */
enum { EF_CPU32 = 0x00810000 };

/* m68k relocs.  */
enum {
	R_68K_NONE         = 0,  /* No reloc */
	R_68K_32           = 1,  /* Direct 32 bit  */
	R_68K_16           = 2,  /* Direct 16 bit  */
	R_68K_8            = 3,  /* Direct 8 bit  */
	R_68K_PC32         = 4,  /* PC relative 32 bit */
	R_68K_PC16         = 5,  /* PC relative 16 bit */
	R_68K_PC8          = 6,  /* PC relative 8 bit */
	R_68K_GOT32        = 7,  /* 32 bit PC relative GOT entry */
	R_68K_GOT16        = 8,  /* 16 bit PC relative GOT entry */
	R_68K_GOT8         = 9,  /* 8 bit PC relative GOT entry */
	R_68K_GOT32O       = 10, /* 32 bit GOT offset */
	R_68K_GOT16O       = 11, /* 16 bit GOT offset */
	R_68K_GOT8O        = 12, /* 8 bit GOT offset */
	R_68K_PLT32        = 13, /* 32 bit PC relative PLT address */
	R_68K_PLT16        = 14, /* 16 bit PC relative PLT address */
	R_68K_PLT8         = 15, /* 8 bit PC relative PLT address */
	R_68K_PLT32O       = 16, /* 32 bit PLT offset */
	R_68K_PLT16O       = 17, /* 16 bit PLT offset */
	R_68K_PLT8O        = 18, /* 8 bit PLT offset */
	R_68K_COPY         = 19, /* Copy symbol at runtime */
	R_68K_GLOB_DAT     = 20, /* Create GOT entry */
	R_68K_JMP_SLOT     = 21, /* Create PLT entry */
	R_68K_RELATIVE     = 22, /* Adjust by program base */
	R_68K_TLS_GD32     = 25, /* 32 bit GOT offset for GD */
	R_68K_TLS_GD16     = 26, /* 16 bit GOT offset for GD */
	R_68K_TLS_GD8      = 27, /* 8 bit GOT offset for GD */
	R_68K_TLS_LDM32    = 28, /* 32 bit GOT offset for LDM */
	R_68K_TLS_LDM16    = 29, /* 16 bit GOT offset for LDM */
	R_68K_TLS_LDM8     = 30, /* 8 bit GOT offset for LDM */
	R_68K_TLS_LDO32    = 31, /* 32 bit module-relative offset */
	R_68K_TLS_LDO16    = 32, /* 16 bit module-relative offset */
	R_68K_TLS_LDO8     = 33, /* 8 bit module-relative offset */
	R_68K_TLS_IE32     = 34, /* 32 bit GOT offset for IE */
	R_68K_TLS_IE16     = 35, /* 16 bit GOT offset for IE */
	R_68K_TLS_IE8      = 36, /* 8 bit GOT offset for IE */
	R_68K_TLS_LE32     = 37, /* 32 bit offset relative to static TLS block */
	R_68K_TLS_LE16     = 38, /* 16 bit offset relative to static TLS block */
	R_68K_TLS_LE8      = 39, /* 8 bit offset relative to static TLS block */
	R_68K_TLS_DTPMOD32 = 40, /* 32 bit module number */
	R_68K_TLS_DTPREL32 = 41, /* 32 bit module-relative offset */
	R_68K_TLS_TPREL32  = 42, /* 32 bit TP-relative offset */

	/* Keep this the last entry.  */
	R_68K_NUM = 43
};

/* Intel 80386 specific definitions.  */

/* i386 relocs.  */
enum {
	R_386_NONE         = 0,  /* No reloc */
	R_386_32           = 1,  /* Direct 32 bit  */
	R_386_PC32         = 2,  /* PC relative 32 bit */
	R_386_GOT32        = 3,  /* 32 bit GOT entry */
	R_386_PLT32        = 4,  /* 32 bit PLT address */
	R_386_COPY         = 5,  /* Copy symbol at runtime */
	R_386_GLOB_DAT     = 6,  /* Create GOT entry */
	R_386_JMP_SLOT     = 7,  /* Create PLT entry */
	R_386_RELATIVE     = 8,  /* Adjust by program base */
	R_386_GOTOFF       = 9,  /* 32 bit offset to GOT */
	R_386_GOTPC        = 10, /* 32 bit PC relative offset to GOT */
	R_386_32PLT        = 11,
	R_386_TLS_TPOFF    = 14, /* Offset in static TLS block */
	R_386_TLS_IE       = 15, /* Address of GOT entry for static TLS block offset */
	R_386_TLS_GOTIE    = 16, /* GOT entry for static TLS block offset */
	R_386_TLS_LE       = 17, /* Offset relative to static TLS block */
	R_386_TLS_GD       = 18, /* Direct 32 bit for GNU version of general dynamic thread local data */
	R_386_TLS_LDM      = 19, /* Direct 32 bit for GNU version of local dynamic thread local data in LE code */
	R_386_16           = 20,
	R_386_PC16         = 21,
	R_386_8            = 22,
	R_386_PC8          = 23,
	R_386_TLS_GD_32    = 24, /* Direct 32 bit for general dynamic thread local data */
	R_386_TLS_GD_PUSH  = 25, /* Tag for pushl in GD TLS code */
	R_386_TLS_GD_CALL  = 26, /* Relocation for call to __tls_get_addr() */
	R_386_TLS_GD_POP   = 27, /* Tag for popl in GD TLS code */
	R_386_TLS_LDM_32   = 28, /* Direct 32 bit for local dynamic thread local data in LE code */
	R_386_TLS_LDM_PUSH = 29, /* Tag for pushl in LDM TLS code */
	R_386_TLS_LDM_CALL = 30, /* Relocation for call to __tls_get_addr() in LDM code */
	R_386_TLS_LDM_POP  = 31, /* Tag for popl in LDM TLS code */
	R_386_TLS_LDO_32   = 32, /* Offset relative to TLS block */
	R_386_TLS_IE_32    = 33, /* GOT entry for negated static TLS block offset */
	R_386_TLS_LE_32    = 34, /* Negated offset relative to static TLS block */
	R_386_TLS_DTPMOD32 = 35, /* ID of module containing symbol */
	R_386_TLS_DTPOFF32 = 36, /* Offset in TLS block */
	R_386_TLS_TPOFF32  = 37, /* Negated offset in static TLS block */
	/* 38? */
	R_386_TLS_GOTDESC   = 39, /* GOT offset for TLS descriptor.  */
	R_386_TLS_DESC_CALL = 40, /* Marker of call through TLS descriptor for relaxation.  */
	R_386_TLS_DESC      = 41, /* TLS descriptor containing pointer to code and to argument, returning the TLS offset for the symbol.  */
	R_386_IRELATIVE     = 42, /* Adjust indirectly by program base */

	/* Keep this the last entry.  */
	R_386_NUM = 43
};

/* SUN SPARC specific definitions.  */

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */
enum {
	STT_SPARC_REGISTER = 13 /* Global register reserved to app. */
};

/* Values for Elf64_Ehdr.e_flags.  */
enum {
	EF_SPARCV9_MM     = 3,
	EF_SPARCV9_TSO    = 0,
	EF_SPARCV9_PSO    = 1,
	EF_SPARCV9_RMO    = 2,
	EF_SPARC_LEDATA   = 0x800000, /* little endian data */
	EF_SPARC_EXT_MASK = 0xFFFF00,
	EF_SPARC_32PLUS   = 0x000100, /* generic V8+ features */
	EF_SPARC_SUN_US1  = 0x000200, /* Sun UltraSPARC1 extensions */
	EF_SPARC_HAL_R1   = 0x000400, /* HAL R1 extensions */
	EF_SPARC_SUN_US3  = 0x000800  /* Sun UltraSPARCIII extensions */
};

/* SPARC relocs.  */
enum {
	R_SPARC_NONE     = 0,  /* No reloc */
	R_SPARC_8        = 1,  /* Direct 8 bit */
	R_SPARC_16       = 2,  /* Direct 16 bit */
	R_SPARC_32       = 3,  /* Direct 32 bit */
	R_SPARC_DISP8    = 4,  /* PC relative 8 bit */
	R_SPARC_DISP16   = 5,  /* PC relative 16 bit */
	R_SPARC_DISP32   = 6,  /* PC relative 32 bit */
	R_SPARC_WDISP30  = 7,  /* PC relative 30 bit shifted */
	R_SPARC_WDISP22  = 8,  /* PC relative 22 bit shifted */
	R_SPARC_HI22     = 9,  /* High 22 bit */
	R_SPARC_22       = 10, /* Direct 22 bit */
	R_SPARC_13       = 11, /* Direct 13 bit */
	R_SPARC_LO10     = 12, /* Truncated 10 bit */
	R_SPARC_GOT10    = 13, /* Truncated 10 bit GOT entry */
	R_SPARC_GOT13    = 14, /* 13 bit GOT entry */
	R_SPARC_GOT22    = 15, /* 22 bit GOT entry shifted */
	R_SPARC_PC10     = 16, /* PC relative 10 bit truncated */
	R_SPARC_PC22     = 17, /* PC relative 22 bit shifted */
	R_SPARC_WPLT30   = 18, /* 30 bit PC relative PLT address */
	R_SPARC_COPY     = 19, /* Copy symbol at runtime */
	R_SPARC_GLOB_DAT = 20, /* Create GOT entry */
	R_SPARC_JMP_SLOT = 21, /* Create PLT entry */
	R_SPARC_RELATIVE = 22, /* Adjust by program base */
	R_SPARC_UA32     = 23, /* Direct 32 bit unaligned */
};

/* Additional Sparc64 relocs.  */
enum {
	R_SPARC_PLT32            = 24, /* Direct 32 bit ref to PLT entry */
	R_SPARC_HIPLT22          = 25, /* High 22 bit PLT entry */
	R_SPARC_LOPLT10          = 26, /* Truncated 10 bit PLT entry */
	R_SPARC_PCPLT32          = 27, /* PC rel 32 bit ref to PLT entry */
	R_SPARC_PCPLT22          = 28, /* PC rel high 22 bit PLT entry */
	R_SPARC_PCPLT10          = 29, /* PC rel trunc 10 bit PLT entry */
	R_SPARC_10               = 30, /* Direct 10 bit */
	R_SPARC_11               = 31, /* Direct 11 bit */
	R_SPARC_64               = 32, /* Direct 64 bit */
	R_SPARC_OLO10            = 33, /* 10bit with secondary 13bit addend */
	R_SPARC_HH22             = 34, /* Top 22 bits of direct 64 bit */
	R_SPARC_HM10             = 35, /* High middle 10 bits of ... */
	R_SPARC_LM22             = 36, /* Low middle 22 bits of ... */
	R_SPARC_PC_HH22          = 37, /* Top 22 bits of pc rel 64 bit */
	R_SPARC_PC_HM10          = 38, /* High middle 10 bit of ... */
	R_SPARC_PC_LM22          = 39, /* Low miggle 22 bits of ... */
	R_SPARC_WDISP16          = 40, /* PC relative 16 bit shifted */
	R_SPARC_WDISP19          = 41, /* PC relative 19 bit shifted */
	R_SPARC_GLOB_JMP         = 42, /* was part of v9 ABI but was removed */
	R_SPARC_7                = 43, /* Direct 7 bit */
	R_SPARC_5                = 44, /* Direct 5 bit */
	R_SPARC_6                = 45, /* Direct 6 bit */
	R_SPARC_DISP64           = 46, /* PC relative 64 bit */
	R_SPARC_PLT64            = 47, /* Direct 64 bit ref to PLT entry */
	R_SPARC_HIX22            = 48, /* High 22 bit complemented */
	R_SPARC_LOX10            = 49, /* Truncated 11 bit complemented */
	R_SPARC_H44              = 50, /* Direct high 12 of 44 bit */
	R_SPARC_M44              = 51, /* Direct mid 22 of 44 bit */
	R_SPARC_L44              = 52, /* Direct low 10 of 44 bit */
	R_SPARC_REGISTER         = 53, /* Global register usage */
	R_SPARC_UA64             = 54, /* Direct 64 bit unaligned */
	R_SPARC_UA16             = 55, /* Direct 16 bit unaligned */
	R_SPARC_TLS_GD_HI22      = 56,
	R_SPARC_TLS_GD_LO10      = 57,
	R_SPARC_TLS_GD_ADD       = 58,
	R_SPARC_TLS_GD_CALL      = 59,
	R_SPARC_TLS_LDM_HI22     = 60,
	R_SPARC_TLS_LDM_LO10     = 61,
	R_SPARC_TLS_LDM_ADD      = 62,
	R_SPARC_TLS_LDM_CALL     = 63,
	R_SPARC_TLS_LDO_HIX22    = 64,
	R_SPARC_TLS_LDO_LOX10    = 65,
	R_SPARC_TLS_LDO_ADD      = 66,
	R_SPARC_TLS_IE_HI22      = 67,
	R_SPARC_TLS_IE_LO10      = 68,
	R_SPARC_TLS_IE_LD        = 69,
	R_SPARC_TLS_IE_LDX       = 70,
	R_SPARC_TLS_IE_ADD       = 71,
	R_SPARC_TLS_LE_HIX22     = 72,
	R_SPARC_TLS_LE_LOX10     = 73,
	R_SPARC_TLS_DTPMOD32     = 74,
	R_SPARC_TLS_DTPMOD64     = 75,
	R_SPARC_TLS_DTPOFF32     = 76,
	R_SPARC_TLS_DTPOFF64     = 77,
	R_SPARC_TLS_TPOFF32      = 78,
	R_SPARC_TLS_TPOFF64      = 79,
	R_SPARC_GOTDATA_HIX22    = 80,
	R_SPARC_GOTDATA_LOX10    = 81,
	R_SPARC_GOTDATA_OP_HIX22 = 82,
	R_SPARC_GOTDATA_OP_LOX10 = 83,
	R_SPARC_GOTDATA_OP       = 84,
	R_SPARC_H34              = 85,
	R_SPARC_SIZE32           = 86,
	R_SPARC_SIZE64           = 87,
	R_SPARC_JMP_IREL         = 248,
	R_SPARC_IRELATIVE        = 249,
	R_SPARC_GNU_VTINHERIT    = 250,
	R_SPARC_GNU_VTENTRY      = 251,
	R_SPARC_REV32            = 252,

	/* Keep this the last entry.  */
	R_SPARC_NUM = 253,
};

/* For Sparc64, legal values for d_tag of Elf64_Dyn.  */
enum {
	DT_SPARC_REGISTER = 0x70000001,
	DT_SPARC_NUM      = 2,
};

/* MIPS R3000 specific definitions.  */

/* Legal values for e_flags field of Elf32_Ehdr.  */
enum {
	EF_MIPS_NOREORDER   = 1, /* A .noreorder directive was used */
	EF_MIPS_PIC         = 2, /* Contains PIC code */
	EF_MIPS_CPIC        = 4, /* Uses PIC calling sequence */
	EF_MIPS_XGOT        = 8,
	EF_MIPS_64BIT_WHIRL = 16,
	EF_MIPS_ABI2        = 32,
	EF_MIPS_ABI_ON32    = 64,
	EF_MIPS_ARCH        = 0xf0000000 /* MIPS architecture level */
};

/* Legal values for MIPS architecture level.  */
enum {
	EF_MIPS_ARCH_1  = 0x00000000, /* -mips1 code.  */
	EF_MIPS_ARCH_2  = 0x10000000, /* -mips2 code.  */
	EF_MIPS_ARCH_3  = 0x20000000, /* -mips3 code.  */
	EF_MIPS_ARCH_4  = 0x30000000, /* -mips4 code.  */
	EF_MIPS_ARCH_5  = 0x40000000, /* -mips5 code.  */
	EF_MIPS_ARCH_32 = 0x60000000, /* MIPS32 code.  */
	EF_MIPS_ARCH_64 = 0x70000000, /* MIPS64 code.  */
};

/* The following are non-official names and should not be used.  */
enum {
	E_MIPS_ARCH_1  = 0x00000000, /* -mips1 code.  */
	E_MIPS_ARCH_2  = 0x10000000, /* -mips2 code.  */
	E_MIPS_ARCH_3  = 0x20000000, /* -mips3 code.  */
	E_MIPS_ARCH_4  = 0x30000000, /* -mips4 code.  */
	E_MIPS_ARCH_5  = 0x40000000, /* -mips5 code.  */
	E_MIPS_ARCH_32 = 0x60000000, /* MIPS32 code.  */
	E_MIPS_ARCH_64 = 0x70000000, /* MIPS64 code.  */
};

/* Special section indices.  */
enum {
	SHN_MIPS_ACOMMON    = 0xff00, /* Allocated common symbols */
	SHN_MIPS_TEXT       = 0xff01, /* Allocated test symbols.  */
	SHN_MIPS_DATA       = 0xff02, /* Allocated data symbols.  */
	SHN_MIPS_SCOMMON    = 0xff03, /* Small common symbols */
	SHN_MIPS_SUNDEFINED = 0xff04, /* Small undefined symbols */
};

/* Legal values for sh_type field of Elf32_Shdr.  */
enum {
	SHT_MIPS_LIBLIST       = 0x70000000, /* Shared objects used in link */
	SHT_MIPS_MSYM          = 0x70000001,
	SHT_MIPS_CONFLICT      = 0x70000002, /* Conflicting symbols */
	SHT_MIPS_GPTAB         = 0x70000003, /* Global data area sizes */
	SHT_MIPS_UCODE         = 0x70000004, /* Reserved for SGI/MIPS compilers */
	SHT_MIPS_DEBUG         = 0x70000005, /* MIPS ECOFF debugging information*/
	SHT_MIPS_REGINFO       = 0x70000006, /* Register usage information */
	SHT_MIPS_PACKAGE       = 0x70000007,
	SHT_MIPS_PACKSYM       = 0x70000008,
	SHT_MIPS_RELD          = 0x70000009,
	SHT_MIPS_IFACE         = 0x7000000b,
	SHT_MIPS_CONTENT       = 0x7000000c,
	SHT_MIPS_OPTIONS       = 0x7000000d, /* Miscellaneous options.  */
	SHT_MIPS_SHDR          = 0x70000010,
	SHT_MIPS_FDESC         = 0x70000011,
	SHT_MIPS_EXTSYM        = 0x70000012,
	SHT_MIPS_DENSE         = 0x70000013,
	SHT_MIPS_PDESC         = 0x70000014,
	SHT_MIPS_LOCSYM        = 0x70000015,
	SHT_MIPS_AUXSYM        = 0x70000016,
	SHT_MIPS_OPTSYM        = 0x70000017,
	SHT_MIPS_LOCSTR        = 0x70000018,
	SHT_MIPS_LINE          = 0x70000019,
	SHT_MIPS_RFDESC        = 0x7000001a,
	SHT_MIPS_DELTASYM      = 0x7000001b,
	SHT_MIPS_DELTAINST     = 0x7000001c,
	SHT_MIPS_DELTACLASS    = 0x7000001d,
	SHT_MIPS_DWARF         = 0x7000001e, /* DWARF debugging information.  */
	SHT_MIPS_DELTADECL     = 0x7000001f,
	SHT_MIPS_SYMBOL_LIB    = 0x70000020,
	SHT_MIPS_EVENTS        = 0x70000021, /* Event section.  */
	SHT_MIPS_TRANSLATE     = 0x70000022,
	SHT_MIPS_PIXIE         = 0x70000023,
	SHT_MIPS_XLATE         = 0x70000024,
	SHT_MIPS_XLATE_DEBUG   = 0x70000025,
	SHT_MIPS_WHIRL         = 0x70000026,
	SHT_MIPS_EH_REGION     = 0x70000027,
	SHT_MIPS_XLATE_OLD     = 0x70000028,
	SHT_MIPS_PDR_EXCEPTION = 0x70000029,
};

/* Legal values for sh_flags field of Elf32_Shdr.  */
enum {
	SHF_MIPS_GPREL   = 0x10000000, /* Must be part of global data area */
	SHF_MIPS_MERGE   = 0x20000000,
	SHF_MIPS_ADDR    = 0x40000000,
	SHF_MIPS_STRINGS = 0x80000000,
	SHF_MIPS_NOSTRIP = 0x08000000,
	SHF_MIPS_LOCAL   = 0x04000000,
	SHF_MIPS_NAMES   = 0x02000000,
	SHF_MIPS_NODUPE  = 0x01000000,
};

/* Symbol tables.  */

/* MIPS specific values for `st_other'.  */
enum {
	STO_MIPS_DEFAULT         = 0x0,
	STO_MIPS_INTERNAL        = 0x1,
	STO_MIPS_HIDDEN          = 0x2,
	STO_MIPS_PROTECTED       = 0x3,
	STO_MIPS_PLT             = 0x8,
	STO_MIPS_SC_ALIGN_UNUSED = 0xff,
};

/* MIPS specific values for `st_info'.  */
enum {
	STB_MIPS_SPLIT_COMMON = 13,
};

/* Entries found in sections of type SHT_MIPS_GPTAB.  */

union elf32_gptab {
	struct {
		elf32_word gt_current_g_value; /* -G value used for compilation */
		elf32_word gt_unused;          /* Not used */
	} gt_header;                       /* First entry in section */

	struct {
		elf32_word gt_g_value; /* If this value were used for -G */
		elf32_word gt_bytes;   /* This many bytes would be used */
	} gt_entry;                /* Subsequent entries in section */
};

/* Entry found in sections of type SHT_MIPS_REGINFO.  */
struct elf32_reginfo {
	elf32_word ri_gprmask;    /* General registers used */
	elf32_word ri_cprmask[4]; /* Coprocessor registers used */
	elf32_sword ri_gp_value;  /* $gp register value */
};

/* Entries found in sections of type SHT_MIPS_OPTIONS.  */
struct elf_options {
	uint8_t kind;          /* Determines interpretation of the variable part of descriptor.  */
	uint8_t size;          /* Size of descriptor, including header.  */
	elf32_section section; /* Section header index of section affected, 0 for global options.  */
	elf32_word info;       /* Kind-specific information.  */
};

/* Values for `kind' field in Elf_Options.  */
enum {
	ODK_NULL       = 0, /* Undefined.  */
	ODK_REGINFO    = 1, /* Register usage information.  */
	ODK_EXCEPTIONS = 2, /* Exception processing options.  */
	ODK_PAD        = 3, /* Section padding options.  */
	ODK_HWPATCH    = 4, /* Hardware workarounds performed */
	ODK_FILL       = 5, /* record the fill value used by the linker. */
	ODK_TAGS       = 6, /* reserve space for desktop tools to write. */
	ODK_HWAND      = 7, /* HW workarounds.  'AND' bits when merging. */
	ODK_HWOR       = 8, /* HW workarounds.  'OR' bits when merging.  */
};

/* Values for `info' in Elf_Options for ODK_EXCEPTIONS entries.  */
enum {
	OEX_FPU_MIN   = 0x1f,    /* FPE's which MUST be enabled.  */
	OEX_FPU_MAX   = 0x1f00,  /* FPE's which MAY be enabled.  */
	OEX_PAGE0     = 0x10000, /* page zero must be mapped.  */
	OEX_SMM       = 0x20000, /* Force sequential memory mode?  */
	OEX_FPDBUG    = 0x40000, /* Force floating point debug mode?  */
	OEX_PRECISEFP = OEX_FPDBUG,
	OEX_DISMISS   = 0x80000, /* Dismiss invalid address faults?  */

	OEX_FPU_INVAL = 0x10,
	OEX_FPU_DIV0  = 0x08,
	OEX_FPU_OFLO  = 0x04,
	OEX_FPU_UFLO  = 0x02,
	OEX_FPU_INEX  = 0x01,
};

/* Masks for `info' in Elf_Options for an ODK_HWPATCH entry.  */
enum {
	OHW_R4KEOP    = 0x1, /* R4000 end-of-page patch.  */
	OHW_R8KPFETCH = 0x2, /* may need R8000 prefetch patch.  */
	OHW_R5KEOP    = 0x4, /* R5000 end-of-page patch.  */
	OHW_R5KCVTL   = 0x8, /* R5000 cvt.[ds].l bug.  clean=1.  */

	OPAD_PREFIX  = 0x1,
	OPAD_POSTFIX = 0x2,
	OPAD_SYMBOL  = 0x4,
};

/* Entry found in `.options' section.  */
struct elf_options_hw {
	elf32_word hwp_flags1; /* Extra flags.  */
	elf32_word hwp_flags2; /* Extra flags.  */
};

/* Masks for `info' in ElfOptions for ODK_HWAND and ODK_HWOR entries.  */
enum {
	OHWA0_R4KEOP_CHECKED = 0x00000001,
	OHWA1_R4KEOP_CLEAN   = 0x00000002,
};

/* MIPS relocs.  */
enum {
	R_MIPS_NONE            = 0,  /* No reloc */
	R_MIPS_16              = 1,  /* Direct 16 bit */
	R_MIPS_32              = 2,  /* Direct 32 bit */
	R_MIPS_REL32           = 3,  /* PC relative 32 bit */
	R_MIPS_26              = 4,  /* Direct 26 bit shifted */
	R_MIPS_HI16            = 5,  /* High 16 bit */
	R_MIPS_LO16            = 6,  /* Low 16 bit */
	R_MIPS_GPREL16         = 7,  /* GP relative 16 bit */
	R_MIPS_LITERAL         = 8,  /* 16 bit literal entry */
	R_MIPS_GOT16           = 9,  /* 16 bit GOT entry */
	R_MIPS_PC16            = 10, /* PC relative 16 bit */
	R_MIPS_CALL16          = 11, /* 16 bit GOT entry for function */
	R_MIPS_GPREL32         = 12, /* GP relative 32 bit */
	R_MIPS_SHIFT5          = 16,
	R_MIPS_SHIFT6          = 17,
	R_MIPS_64              = 18,
	R_MIPS_GOT_DISP        = 19,
	R_MIPS_GOT_PAGE        = 20,
	R_MIPS_GOT_OFST        = 21,
	R_MIPS_GOT_HI16        = 22,
	R_MIPS_GOT_LO16        = 23,
	R_MIPS_SUB             = 24,
	R_MIPS_INSERT_A        = 25,
	R_MIPS_INSERT_B        = 26,
	R_MIPS_DELETE          = 27,
	R_MIPS_HIGHER          = 28,
	R_MIPS_HIGHEST         = 29,
	R_MIPS_CALL_HI16       = 30,
	R_MIPS_CALL_LO16       = 31,
	R_MIPS_SCN_DISP        = 32,
	R_MIPS_REL16           = 33,
	R_MIPS_ADD_IMMEDIATE   = 34,
	R_MIPS_PJUMP           = 35,
	R_MIPS_RELGOT          = 36,
	R_MIPS_JALR            = 37,
	R_MIPS_TLS_DTPMOD32    = 38, /* Module number 32 bit */
	R_MIPS_TLS_DTPREL32    = 39, /* Module-relative offset 32 bit */
	R_MIPS_TLS_DTPMOD64    = 40, /* Module number 64 bit */
	R_MIPS_TLS_DTPREL64    = 41, /* Module-relative offset 64 bit */
	R_MIPS_TLS_GD          = 42, /* 16 bit GOT offset for GD */
	R_MIPS_TLS_LDM         = 43, /* 16 bit GOT offset for LDM */
	R_MIPS_TLS_DTPREL_HI16 = 44, /* Module-relative offset, high 16 bits */
	R_MIPS_TLS_DTPREL_LO16 = 45, /* Module-relative offset, low 16 bits */
	R_MIPS_TLS_GOTTPREL    = 46, /* 16 bit GOT offset for IE */
	R_MIPS_TLS_TPREL32     = 47, /* TP-relative offset, 32 bit */
	R_MIPS_TLS_TPREL64     = 48, /* TP-relative offset, 64 bit */
	R_MIPS_TLS_TPREL_HI16  = 49, /* TP-relative offset, high 16 bits */
	R_MIPS_TLS_TPREL_LO16  = 50, /* TP-relative offset, low 16 bits */
	R_MIPS_GLOB_DAT        = 51,
	R_MIPS_COPY            = 126,
	R_MIPS_JUMP_SLOT       = 127,

	/* Keep this the last entry.  */
	R_MIPS_NUM = 128,
};

/* Legal values for p_type field of Elf32_Phdr.  */
enum {
	PT_MIPS_REGINFO = 0x70000000, /* Register usage information */
	PT_MIPS_RTPROC  = 0x70000001, /* Runtime procedure table. */
	PT_MIPS_OPTIONS = 0x70000002,
};

/* Special program header types.  */
enum {
	PF_MIPS_LOCAL = 0x10000000,
};

/* Legal values for DT_MIPS_FLAGS Elf32_Dyn entry.  */
enum {
	RHF_NONE                   = 0,        /* No flags */
	RHF_QUICKSTART             = (1 << 0), /* Use quickstart */
	RHF_NOTPOT                 = (1 << 1), /* Hash size not power of 2 */
	RHF_NO_LIBRARY_REPLACEMENT = (1 << 2), /* Ignore LD_LIBRARY_PATH */
	RHF_NO_MOVE                = (1 << 3),
	RHF_SGI_ONLY               = (1 << 4),
	RHF_GUARANTEE_INIT         = (1 << 5),
	RHF_DELTA_C_PLUS_PLUS      = (1 << 6),
	RHF_GUARANTEE_START_INIT   = (1 << 7),
	RHF_PIXIE                  = (1 << 8),
	RHF_DEFAULT_DELAY_LOAD     = (1 << 9),
	RHF_REQUICKSTART           = (1 << 10),
	RHF_REQUICKSTARTED         = (1 << 11),
	RHF_CORD                   = (1 << 12),
	RHF_NO_UNRES_UNDEF         = (1 << 13),
	RHF_RLD_ORDER_SAFE         = (1 << 14),
};

/* Entries found in sections of type SHT_MIPS_LIBLIST.  */

struct elf32_lib {
	elf32_word l_name;       /* Name (string table index) */
	elf32_word l_time_stamp; /* Timestamp */
	elf32_word l_checksum;   /* Checksum */
	elf32_word l_version;    /* Interface version */
	elf32_word l_flags;      /* Flags */
};

struct elf64_lib {
	elf64_word l_name;       /* Name (string table index) */
	elf64_word l_time_stamp; /* Timestamp */
	elf64_word l_checksum;   /* Checksum */
	elf64_word l_version;    /* Interface version */
	elf64_word l_flags;      /* Flags */
};

/* Legal values for l_flags.  */
enum {
	LL_NONE           = 0,
	LL_EXACT_MATCH    = (1 << 0), /* Require exact match */
	LL_IGNORE_INT_VER = (1 << 1), /* Ignore interface version */
	LL_REQUIRE_MINOR  = (1 << 2),
	LL_EXPORTS        = (1 << 3),
	LL_DELAY_LOAD     = (1 << 4),
	LL_DELTA          = (1 << 5),
};

/* Entries found in sections of type SHT_MIPS_CONFLICT.  */

using elf32_conflict = elf32_addr;

/* HPPA specific definitions.  */

/* Legal values for e_flags field of Elf32_Ehdr.  */
enum {
	EF_PARISC_TRAPNIL  = 0x00010000, /* Trap nil pointer dereference.  */
	EF_PARISC_EXT      = 0x00020000, /* Program uses arch. extensions. */
	EF_PARISC_LSB      = 0x00040000, /* Program expects little endian. */
	EF_PARISC_WIDE     = 0x00080000, /* Program expects wide mode.  */
	EF_PARISC_NO_KABP  = 0x00100000, /* No kernel assisted branch prediction.  */
	EF_PARISC_LAZYSWAP = 0x00400000, /* Allow lazy swapping.  */
	EF_PARISC_ARCH     = 0x0000ffff, /* Architecture version.  */
};

/* Defined values for `e_flags & EF_PARISC_ARCH' are:  */
enum {
	EFA_PARISC_1_0 = 0x020b, /* PA-RISC 1.0 big-endian.  */
	EFA_PARISC_1_1 = 0x0210, /* PA-RISC 1.1 big-endian.  */
	EFA_PARISC_2_0 = 0x0214, /* PA-RISC 2.0 big-endian.  */
};

/* Additional section indeces.  */
enum {
	SHN_PARISC_ANSI_COMMON = 0xff00, /* Section for tenatively declared symbols in ANSI C.  */
	SHN_PARISC_HUGE_COMMON = 0xff01, /* Common blocks in huge model.  */
};

/* Legal values for sh_type field of Elf32_Shdr.  */
enum {
	SHT_PARISC_EXT    = 0x70000000, /* Contains product specific ext. */
	SHT_PARISC_UNWIND = 0x70000001, /* Unwind information.  */
	SHT_PARISC_DOC    = 0x70000002, /* Debug info for optimized code. */
};

/* Legal values for sh_flags field of Elf32_Shdr.  */
enum {
	SHF_PARISC_SHORT = 0x20000000, /* Section with short addressing. */
	SHF_PARISC_HUGE  = 0x40000000, /* Section far from gp.  */
	SHF_PARISC_SBP   = 0x80000000, /* Static branch prediction code. */
};

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */
enum {
	STT_PARISC_MILLICODE = 13 /* Millicode function entry point.  */
};

enum {
	STT_HP_OPAQUE = (STT_LOOS + 0x1),
	STT_HP_STUB   = (STT_LOOS + 0x2),
};

/* HPPA relocs.  */
enum {
	R_PARISC_NONE           = 0,   /* No reloc.  */
	R_PARISC_DIR32          = 1,   /* Direct 32-bit reference.  */
	R_PARISC_DIR21L         = 2,   /* Left 21 bits of eff. address.  */
	R_PARISC_DIR17R         = 3,   /* Right 17 bits of eff. address.  */
	R_PARISC_DIR17F         = 4,   /* 17 bits of eff. address.  */
	R_PARISC_DIR14R         = 6,   /* Right 14 bits of eff. address.  */
	R_PARISC_PCREL32        = 9,   /* 32-bit rel. address.  */
	R_PARISC_PCREL21L       = 10,  /* Left 21 bits of rel. address.  */
	R_PARISC_PCREL17R       = 11,  /* Right 17 bits of rel. address.  */
	R_PARISC_PCREL17F       = 12,  /* 17 bits of rel. address.  */
	R_PARISC_PCREL14R       = 14,  /* Right 14 bits of rel. address.  */
	R_PARISC_DPREL21L       = 18,  /* Left 21 bits of rel. address.  */
	R_PARISC_DPREL14R       = 22,  /* Right 14 bits of rel. address.  */
	R_PARISC_GPREL21L       = 26,  /* GP-relative, left 21 bits.  */
	R_PARISC_GPREL14R       = 30,  /* GP-relative, right 14 bits.  */
	R_PARISC_LTOFF21L       = 34,  /* LT-relative, left 21 bits.  */
	R_PARISC_LTOFF14R       = 38,  /* LT-relative, right 14 bits.  */
	R_PARISC_SECREL32       = 41,  /* 32 bits section rel. address.  */
	R_PARISC_SEGBASE        = 48,  /* No relocation, set segment base.  */
	R_PARISC_SEGREL32       = 49,  /* 32 bits segment rel. address.  */
	R_PARISC_PLTOFF21L      = 50,  /* PLT rel. address, left 21 bits.  */
	R_PARISC_PLTOFF14R      = 54,  /* PLT rel. address, right 14 bits.  */
	R_PARISC_LTOFF_FPTR32   = 57,  /* 32 bits LT-rel. function pointer. */
	R_PARISC_LTOFF_FPTR21L  = 58,  /* LT-rel. fct ptr, left 21 bits. */
	R_PARISC_LTOFF_FPTR14R  = 62,  /* LT-rel. fct ptr, right 14 bits. */
	R_PARISC_FPTR64         = 64,  /* 64 bits function address.  */
	R_PARISC_PLABEL32       = 65,  /* 32 bits function address.  */
	R_PARISC_PLABEL21L      = 66,  /* Left 21 bits of fdesc address.  */
	R_PARISC_PLABEL14R      = 70,  /* Right 14 bits of fdesc address.  */
	R_PARISC_PCREL64        = 72,  /* 64 bits PC-rel. address.  */
	R_PARISC_PCREL22F       = 74,  /* 22 bits PC-rel. address.  */
	R_PARISC_PCREL14WR      = 75,  /* PC-rel. address, right 14 bits.  */
	R_PARISC_PCREL14DR      = 76,  /* PC rel. address, right 14 bits.  */
	R_PARISC_PCREL16F       = 77,  /* 16 bits PC-rel. address.  */
	R_PARISC_PCREL16WF      = 78,  /* 16 bits PC-rel. address.  */
	R_PARISC_PCREL16DF      = 79,  /* 16 bits PC-rel. address.  */
	R_PARISC_DIR64          = 80,  /* 64 bits of eff. address.  */
	R_PARISC_DIR14WR        = 83,  /* 14 bits of eff. address.  */
	R_PARISC_DIR14DR        = 84,  /* 14 bits of eff. address.  */
	R_PARISC_DIR16F         = 85,  /* 16 bits of eff. address.  */
	R_PARISC_DIR16WF        = 86,  /* 16 bits of eff. address.  */
	R_PARISC_DIR16DF        = 87,  /* 16 bits of eff. address.  */
	R_PARISC_GPREL64        = 88,  /* 64 bits of GP-rel. address.  */
	R_PARISC_GPREL14WR      = 91,  /* GP-rel. address, right 14 bits.  */
	R_PARISC_GPREL14DR      = 92,  /* GP-rel. address, right 14 bits.  */
	R_PARISC_GPREL16F       = 93,  /* 16 bits GP-rel. address.  */
	R_PARISC_GPREL16WF      = 94,  /* 16 bits GP-rel. address.  */
	R_PARISC_GPREL16DF      = 95,  /* 16 bits GP-rel. address.  */
	R_PARISC_LTOFF64        = 96,  /* 64 bits LT-rel. address.  */
	R_PARISC_LTOFF14WR      = 99,  /* LT-rel. address, right 14 bits.  */
	R_PARISC_LTOFF14DR      = 100, /* LT-rel. address, right 14 bits.  */
	R_PARISC_LTOFF16F       = 101, /* 16 bits LT-rel. address.  */
	R_PARISC_LTOFF16WF      = 102, /* 16 bits LT-rel. address.  */
	R_PARISC_LTOFF16DF      = 103, /* 16 bits LT-rel. address.  */
	R_PARISC_SECREL64       = 104, /* 64 bits section rel. address.  */
	R_PARISC_SEGREL64       = 112, /* 64 bits segment rel. address.  */
	R_PARISC_PLTOFF14WR     = 115, /* PLT-rel. address, right 14 bits.  */
	R_PARISC_PLTOFF14DR     = 116, /* PLT-rel. address, right 14 bits.  */
	R_PARISC_PLTOFF16F      = 117, /* 16 bits LT-rel. address.  */
	R_PARISC_PLTOFF16WF     = 118, /* 16 bits PLT-rel. address.  */
	R_PARISC_PLTOFF16DF     = 119, /* 16 bits PLT-rel. address.  */
	R_PARISC_LTOFF_FPTR64   = 120, /* 64 bits LT-rel. function ptr.  */
	R_PARISC_LTOFF_FPTR14WR = 123, /* LT-rel. fct. ptr., right 14 bits. */
	R_PARISC_LTOFF_FPTR14DR = 124, /* LT-rel. fct. ptr., right 14 bits. */
	R_PARISC_LTOFF_FPTR16F  = 125, /* 16 bits LT-rel. function ptr.  */
	R_PARISC_LTOFF_FPTR16WF = 126, /* 16 bits LT-rel. function ptr.  */
	R_PARISC_LTOFF_FPTR16DF = 127, /* 16 bits LT-rel. function ptr.  */
	R_PARISC_LORESERVE      = 128,
	R_PARISC_COPY           = 128, /* Copy relocation.  */
	R_PARISC_IPLT           = 129, /* Dynamic reloc, imported PLT */
	R_PARISC_EPLT           = 130, /* Dynamic reloc, exported PLT */
	R_PARISC_TPREL32        = 153, /* 32 bits TP-rel. address.  */
	R_PARISC_TPREL21L       = 154, /* TP-rel. address, left 21 bits.  */
	R_PARISC_TPREL14R       = 158, /* TP-rel. address, right 14 bits.  */
	R_PARISC_LTOFF_TP21L    = 162, /* LT-TP-rel. address, left 21 bits. */
	R_PARISC_LTOFF_TP14R    = 166, /* LT-TP-rel. address, right 14 bits.*/
	R_PARISC_LTOFF_TP14F    = 167, /* 14 bits LT-TP-rel. address.  */
	R_PARISC_TPREL64        = 216, /* 64 bits TP-rel. address.  */
	R_PARISC_TPREL14WR      = 219, /* TP-rel. address, right 14 bits.  */
	R_PARISC_TPREL14DR      = 220, /* TP-rel. address, right 14 bits.  */
	R_PARISC_TPREL16F       = 221, /* 16 bits TP-rel. address.  */
	R_PARISC_TPREL16WF      = 222, /* 16 bits TP-rel. address.  */
	R_PARISC_TPREL16DF      = 223, /* 16 bits TP-rel. address.  */
	R_PARISC_LTOFF_TP64     = 224, /* 64 bits LT-TP-rel. address.  */
	R_PARISC_LTOFF_TP14WR   = 227, /* LT-TP-rel. address, right 14 bits.*/
	R_PARISC_LTOFF_TP14DR   = 228, /* LT-TP-rel. address, right 14 bits.*/
	R_PARISC_LTOFF_TP16F    = 229, /* 16 bits LT-TP-rel. address.  */
	R_PARISC_LTOFF_TP16WF   = 230, /* 16 bits LT-TP-rel. address.  */
	R_PARISC_LTOFF_TP16DF   = 231, /* 16 bits LT-TP-rel. address.  */
	R_PARISC_GNU_VTENTRY    = 232,
	R_PARISC_GNU_VTINHERIT  = 233,
	R_PARISC_TLS_GD21L      = 234, /* GD 21-bit left.  */
	R_PARISC_TLS_GD14R      = 235, /* GD 14-bit right.  */
	R_PARISC_TLS_GDCALL     = 236, /* GD call to __t_g_a.  */
	R_PARISC_TLS_LDM21L     = 237, /* LD module 21-bit left.  */
	R_PARISC_TLS_LDM14R     = 238, /* LD module 14-bit right.  */
	R_PARISC_TLS_LDMCALL    = 239, /* LD module call to __t_g_a.  */
	R_PARISC_TLS_LDO21L     = 240, /* LD offset 21-bit left.  */
	R_PARISC_TLS_LDO14R     = 241, /* LD offset 14-bit right.  */
	R_PARISC_TLS_DTPMOD32   = 242, /* DTP module 32-bit.  */
	R_PARISC_TLS_DTPMOD64   = 243, /* DTP module 64-bit.  */
	R_PARISC_TLS_DTPOFF32   = 244, /* DTP offset 32-bit.  */
	R_PARISC_TLS_DTPOFF64   = 245, /* DTP offset 32-bit.  */
	R_PARISC_TLS_LE21L      = R_PARISC_TPREL21L,
	R_PARISC_TLS_LE14R      = R_PARISC_TPREL14R,
	R_PARISC_TLS_IE21L      = R_PARISC_LTOFF_TP21L,
	R_PARISC_TLS_IE14R      = R_PARISC_LTOFF_TP14R,
	R_PARISC_TLS_TPREL32    = R_PARISC_TPREL32,
	R_PARISC_TLS_TPREL64    = R_PARISC_TPREL64,
	R_PARISC_HIRESERVE      = 255,
};

/* Legal values for p_type field of Elf32_Phdr/Elf64_Phdr.  */
enum {
	PT_HP_TLS           = (PT_LOOS + 0x0),
	PT_HP_CORE_NONE     = (PT_LOOS + 0x1),
	PT_HP_CORE_VERSION  = (PT_LOOS + 0x2),
	PT_HP_CORE_KERNEL   = (PT_LOOS + 0x3),
	PT_HP_CORE_COMM     = (PT_LOOS + 0x4),
	PT_HP_CORE_PROC     = (PT_LOOS + 0x5),
	PT_HP_CORE_LOADABLE = (PT_LOOS + 0x6),
	PT_HP_CORE_STACK    = (PT_LOOS + 0x7),
	PT_HP_CORE_SHM      = (PT_LOOS + 0x8),
	PT_HP_CORE_MMF      = (PT_LOOS + 0x9),
	PT_HP_PARALLEL      = (PT_LOOS + 0x10),
	PT_HP_FASTBIND      = (PT_LOOS + 0x11),
	PT_HP_OPT_ANNOT     = (PT_LOOS + 0x12),
	PT_HP_HSL_ANNOT     = (PT_LOOS + 0x13),
	PT_HP_STACK         = (PT_LOOS + 0x14),
};

enum {
	PT_PARISC_ARCHEXT = 0x70000000,
	PT_PARISC_UNWIND  = 0x70000001,
};

/* Legal values for p_flags field of Elf32_Phdr/Elf64_Phdr.  */
enum {
	PF_PARISC_SBP = 0x08000000,
};

enum {
	PF_HP_PAGE_SIZE   = 0x00100000,
	PF_HP_FAR_SHARED  = 0x00200000,
	PF_HP_NEAR_SHARED = 0x00400000,
	PF_HP_CODE        = 0x01000000,
	PF_HP_MODIFY      = 0x02000000,
	PF_HP_LAZYSWAP    = 0x04000000,
	PF_HP_SBP         = 0x08000000,
};

/* Alpha specific definitions.  */

/* Legal values for e_flags field of Elf64_Ehdr.  */
enum {
	EF_ALPHA_32BIT    = 1, /* All addresses must be < 2GB.  */
	EF_ALPHA_CANRELAX = 2  /* Relocations for relaxing exist.  */
};

/* Legal values for sh_type field of Elf64_Shdr.  */

/* These two are primerily concerned with ECOFF debugging info.  */
enum {
	SHT_ALPHA_DEBUG   = 0x70000001,
	SHT_ALPHA_REGINFO = 0x70000002,
};

/* Legal values for sh_flags field of Elf64_Shdr.  */
enum {
	SHF_ALPHA_GPREL = 0x10000000,
};

/* Legal values for st_other field of Elf64_Sym.  */
enum {
	STO_ALPHA_NOPV       = 0x80, /* No PV required.  */
	STO_ALPHA_STD_GPLOAD = 0x88, /* PV only used for initial ldgp.  */
};

/* Alpha relocs.  */
enum {
	R_ALPHA_NONE      = 0,  /* No reloc */
	R_ALPHA_REFLONG   = 1,  /* Direct 32 bit */
	R_ALPHA_REFQUAD   = 2,  /* Direct 64 bit */
	R_ALPHA_GPREL32   = 3,  /* GP relative 32 bit */
	R_ALPHA_LITERAL   = 4,  /* GP relative 16 bit w/optimization */
	R_ALPHA_LITUSE    = 5,  /* Optimization hint for LITERAL */
	R_ALPHA_GPDISP    = 6,  /* Add displacement to GP */
	R_ALPHA_BRADDR    = 7,  /* PC+4 relative 23 bit shifted */
	R_ALPHA_HINT      = 8,  /* PC+4 relative 16 bit shifted */
	R_ALPHA_SREL16    = 9,  /* PC relative 16 bit */
	R_ALPHA_SREL32    = 10, /* PC relative 32 bit */
	R_ALPHA_SREL64    = 11, /* PC relative 64 bit */
	R_ALPHA_GPRELHIGH = 17, /* GP relative 32 bit, high 16 bits */
	R_ALPHA_GPRELLOW  = 18, /* GP relative 32 bit, low 16 bits */
	R_ALPHA_GPREL16   = 19, /* GP relative 16 bit */
	R_ALPHA_COPY      = 24, /* Copy symbol at runtime */
	R_ALPHA_GLOB_DAT  = 25, /* Create GOT entry */
	R_ALPHA_JMP_SLOT  = 26, /* Create PLT entry */
	R_ALPHA_RELATIVE  = 27, /* Adjust by program base */
	R_ALPHA_TLS_GD_HI = 28,
	R_ALPHA_TLSGD     = 29,
	R_ALPHA_TLS_LDM   = 30,
	R_ALPHA_DTPMOD64  = 31,
	R_ALPHA_GOTDTPREL = 32,
	R_ALPHA_DTPREL64  = 33,
	R_ALPHA_DTPRELHI  = 34,
	R_ALPHA_DTPRELLO  = 35,
	R_ALPHA_DTPREL16  = 36,
	R_ALPHA_GOTTPREL  = 37,
	R_ALPHA_TPREL64   = 38,
	R_ALPHA_TPRELHI   = 39,
	R_ALPHA_TPRELLO   = 40,
	R_ALPHA_TPREL16   = 41,

	/* Keep this the last entry.  */
	R_ALPHA_NUM = 46,
};

/* Magic values of the LITUSE relocation addend.  */
enum {
	LITUSE_ALPHA_ADDR    = 0,
	LITUSE_ALPHA_BASE    = 1,
	LITUSE_ALPHA_BYTOFF  = 2,
	LITUSE_ALPHA_JSR     = 3,
	LITUSE_ALPHA_TLS_GD  = 4,
	LITUSE_ALPHA_TLS_LDM = 5,
};

/* Legal values for d_tag of Elf64_Dyn.  */
enum {
	DT_ALPHA_PLTRO = (DT_LOPROC + 0),
	DT_ALPHA_NUM   = 1,
};

/* PowerPC specific declarations */

/* Values for Elf32/64_Ehdr.e_flags.  */
enum {
	EF_PPC_EMB = 0x80000000, /* PowerPC embedded flag */
};

/* Cygnus local bits below */
enum {
	EF_PPC_RELOCATABLE     = 0x00010000, /* PowerPC -mrelocatable flag*/
	EF_PPC_RELOCATABLE_LIB = 0x00008000, /* PowerPC -mrelocatable-lib flag */
};

/* PowerPC relocations defined by the ABIs */
enum {
	R_PPC_NONE            = 0,
	R_PPC_ADDR32          = 1, /* 32bit absolute address */
	R_PPC_ADDR24          = 2, /* 26bit address, 2 bits ignored.  */
	R_PPC_ADDR16          = 3, /* 16bit absolute address */
	R_PPC_ADDR16_LO       = 4, /* lower 16bit of absolute address */
	R_PPC_ADDR16_HI       = 5, /* high 16bit of absolute address */
	R_PPC_ADDR16_HA       = 6, /* adjusted high 16bit */
	R_PPC_ADDR14          = 7, /* 16bit address, 2 bits ignored */
	R_PPC_ADDR14_BRTAKEN  = 8,
	R_PPC_ADDR14_BRNTAKEN = 9,
	R_PPC_REL24           = 10, /* PC relative 26 bit */
	R_PPC_REL14           = 11, /* PC relative 16 bit */
	R_PPC_REL14_BRTAKEN   = 12,
	R_PPC_REL14_BRNTAKEN  = 13,
	R_PPC_GOT16           = 14,
	R_PPC_GOT16_LO        = 15,
	R_PPC_GOT16_HI        = 16,
	R_PPC_GOT16_HA        = 17,
	R_PPC_PLTREL24        = 18,
	R_PPC_COPY            = 19,
	R_PPC_GLOB_DAT        = 20,
	R_PPC_JMP_SLOT        = 21,
	R_PPC_RELATIVE        = 22,
	R_PPC_LOCAL24PC       = 23,
	R_PPC_UADDR32         = 24,
	R_PPC_UADDR16         = 25,
	R_PPC_REL32           = 26,
	R_PPC_PLT32           = 27,
	R_PPC_PLTREL32        = 28,
	R_PPC_PLT16_LO        = 29,
	R_PPC_PLT16_HI        = 30,
	R_PPC_PLT16_HA        = 31,
	R_PPC_SDAREL16        = 32,
	R_PPC_SECTOFF         = 33,
	R_PPC_SECTOFF_LO      = 34,
	R_PPC_SECTOFF_HI      = 35,
	R_PPC_SECTOFF_HA      = 36,
};

/* PowerPC relocations defined for the TLS access ABI.  */
enum {
	R_PPC_TLS             = 67, /* none	(sym+add)@tls */
	R_PPC_DTPMOD32        = 68, /* word32	(sym+add)@dtpmod */
	R_PPC_TPREL16         = 69, /* half16*	(sym+add)@tprel */
	R_PPC_TPREL16_LO      = 70, /* half16	(sym+add)@tprel@l */
	R_PPC_TPREL16_HI      = 71, /* half16	(sym+add)@tprel@h */
	R_PPC_TPREL16_HA      = 72, /* half16	(sym+add)@tprel@ha */
	R_PPC_TPREL32         = 73, /* word32	(sym+add)@tprel */
	R_PPC_DTPREL16        = 74, /* half16*	(sym+add)@dtprel */
	R_PPC_DTPREL16_LO     = 75, /* half16	(sym+add)@dtprel@l */
	R_PPC_DTPREL16_HI     = 76, /* half16	(sym+add)@dtprel@h */
	R_PPC_DTPREL16_HA     = 77, /* half16	(sym+add)@dtprel@ha */
	R_PPC_DTPREL32        = 78, /* word32	(sym+add)@dtprel */
	R_PPC_GOT_TLSGD16     = 79, /* half16*	(sym+add)@got@tlsgd */
	R_PPC_GOT_TLSGD16_LO  = 80, /* half16	(sym+add)@got@tlsgd@l */
	R_PPC_GOT_TLSGD16_HI  = 81, /* half16	(sym+add)@got@tlsgd@h */
	R_PPC_GOT_TLSGD16_HA  = 82, /* half16	(sym+add)@got@tlsgd@ha */
	R_PPC_GOT_TLSLD16     = 83, /* half16*	(sym+add)@got@tlsld */
	R_PPC_GOT_TLSLD16_LO  = 84, /* half16	(sym+add)@got@tlsld@l */
	R_PPC_GOT_TLSLD16_HI  = 85, /* half16	(sym+add)@got@tlsld@h */
	R_PPC_GOT_TLSLD16_HA  = 86, /* half16	(sym+add)@got@tlsld@ha */
	R_PPC_GOT_TPREL16     = 87, /* half16*	(sym+add)@got@tprel */
	R_PPC_GOT_TPREL16_LO  = 88, /* half16	(sym+add)@got@tprel@l */
	R_PPC_GOT_TPREL16_HI  = 89, /* half16	(sym+add)@got@tprel@h */
	R_PPC_GOT_TPREL16_HA  = 90, /* half16	(sym+add)@got@tprel@ha */
	R_PPC_GOT_DTPREL16    = 91, /* half16*	(sym+add)@got@dtprel */
	R_PPC_GOT_DTPREL16_LO = 92, /* half16*	(sym+add)@got@dtprel@l */
	R_PPC_GOT_DTPREL16_HI = 93, /* half16*	(sym+add)@got@dtprel@h */
	R_PPC_GOT_DTPREL16_HA = 94, /* half16*	(sym+add)@got@dtprel@ha */
};

/* The remaining relocs are from the Embedded ELF ABI, and are not
   in the SVR4 ELF ABI.  */
enum {
	R_PPC_EMB_NADDR32    = 101,
	R_PPC_EMB_NADDR16    = 102,
	R_PPC_EMB_NADDR16_LO = 103,
	R_PPC_EMB_NADDR16_HI = 104,
	R_PPC_EMB_NADDR16_HA = 105,
	R_PPC_EMB_SDAI16     = 106,
	R_PPC_EMB_SDA2I16    = 107,
	R_PPC_EMB_SDA2REL    = 108,
	R_PPC_EMB_SDA21      = 109, /* 16 bit offset in SDA */
	R_PPC_EMB_MRKREF     = 110,
	R_PPC_EMB_RELSEC16   = 111,
	R_PPC_EMB_RELST_LO   = 112,
	R_PPC_EMB_RELST_HI   = 113,
	R_PPC_EMB_RELST_HA   = 114,
	R_PPC_EMB_BIT_FLD    = 115,
	R_PPC_EMB_RELSDA     = 116, /* 16 bit relative offset in SDA */
};

/* Diab tool relocations.  */
enum {
	R_PPC_DIAB_SDA21_LO  = 180, /* like EMB_SDA21, but lower 16 bit */
	R_PPC_DIAB_SDA21_HI  = 181, /* like EMB_SDA21, but high 16 bit */
	R_PPC_DIAB_SDA21_HA  = 182, /* like EMB_SDA21, adjusted high 16 */
	R_PPC_DIAB_RELSDA_LO = 183, /* like EMB_RELSDA, but lower 16 bit */
	R_PPC_DIAB_RELSDA_HI = 184, /* like EMB_RELSDA, but high 16 bit */
	R_PPC_DIAB_RELSDA_HA = 185, /* like EMB_RELSDA, adjusted high 16 */
};

/* GNU extension to support local ifunc.  */
enum {
	R_PPC_IRELATIVE = 248,
};

/* GNU relocs used in PIC code sequences.  */
enum {
	R_PPC_REL16    = 249, /* half16   (sym+add-.) */
	R_PPC_REL16_LO = 250, /* half16   (sym+add-.)@l */
	R_PPC_REL16_HI = 251, /* half16   (sym+add-.)@h */
	R_PPC_REL16_HA = 252, /* half16   (sym+add-.)@ha */
};

/* This is a phony reloc to handle any old fashioned TOC16 references
   that may still be in object files.  */
enum {
	R_PPC_TOC16 = 255,
};

/* PowerPC specific values for the Dyn d_tag field.  */
enum {
	DT_PPC_GOT = (DT_LOPROC + 0),
	DT_PPC_NUM = 1,
};

/* PowerPC64 relocations defined by the ABIs */
enum {
	R_PPC64_NONE            = R_PPC_NONE,
	R_PPC64_ADDR32          = R_PPC_ADDR32,    /* 32bit absolute address */
	R_PPC64_ADDR24          = R_PPC_ADDR24,    /* 26bit address, word aligned */
	R_PPC64_ADDR16          = R_PPC_ADDR16,    /* 16bit absolute address */
	R_PPC64_ADDR16_LO       = R_PPC_ADDR16_LO, /* lower 16bits of address */
	R_PPC64_ADDR16_HI       = R_PPC_ADDR16_HI, /* high 16bits of address. */
	R_PPC64_ADDR16_HA       = R_PPC_ADDR16_HA, /* adjusted high 16bits.  */
	R_PPC64_ADDR14          = R_PPC_ADDR14,    /* 16bit address, word aligned */
	R_PPC64_ADDR14_BRTAKEN  = R_PPC_ADDR14_BRTAKEN,
	R_PPC64_ADDR14_BRNTAKEN = R_PPC_ADDR14_BRNTAKEN,
	R_PPC64_REL24           = R_PPC_REL24, /* PC-rel. 26 bit, word aligned */
	R_PPC64_REL14           = R_PPC_REL14, /* PC relative 16 bit */
	R_PPC64_REL14_BRTAKEN   = R_PPC_REL14_BRTAKEN,
	R_PPC64_REL14_BRNTAKEN  = R_PPC_REL14_BRNTAKEN,
	R_PPC64_GOT16           = R_PPC_GOT16,
	R_PPC64_GOT16_LO        = R_PPC_GOT16_LO,
	R_PPC64_GOT16_HI        = R_PPC_GOT16_HI,
	R_PPC64_GOT16_HA        = R_PPC_GOT16_HA,
	R_PPC64_COPY            = R_PPC_COPY,
	R_PPC64_GLOB_DAT        = R_PPC_GLOB_DAT,
	R_PPC64_JMP_SLOT        = R_PPC_JMP_SLOT,
	R_PPC64_RELATIVE        = R_PPC_RELATIVE,
	R_PPC64_UADDR32         = R_PPC_UADDR32,
	R_PPC64_UADDR16         = R_PPC_UADDR16,
	R_PPC64_REL32           = R_PPC_REL32,
	R_PPC64_PLT32           = R_PPC_PLT32,
	R_PPC64_PLTREL32        = R_PPC_PLTREL32,
	R_PPC64_PLT16_LO        = R_PPC_PLT16_LO,
	R_PPC64_PLT16_HI        = R_PPC_PLT16_HI,
	R_PPC64_PLT16_HA        = R_PPC_PLT16_HA,
	R_PPC64_SECTOFF         = R_PPC_SECTOFF,
	R_PPC64_SECTOFF_LO      = R_PPC_SECTOFF_LO,
	R_PPC64_SECTOFF_HI      = R_PPC_SECTOFF_HI,
	R_PPC64_SECTOFF_HA      = R_PPC_SECTOFF_HA,
	R_PPC64_ADDR30          = 37, /* word30 (S + A - P) >> 2 */
	R_PPC64_ADDR64          = 38, /* doubleword64 S + A */
	R_PPC64_ADDR16_HIGHER   = 39, /* half16 #higher(S + A) */
	R_PPC64_ADDR16_HIGHERA  = 40, /* half16 #highera(S + A) */
	R_PPC64_ADDR16_HIGHEST  = 41, /* half16 #highest(S + A) */
	R_PPC64_ADDR16_HIGHESTA = 42, /* half16 #highesta(S + A) */
	R_PPC64_UADDR64         = 43, /* doubleword64 S + A */
	R_PPC64_REL64           = 44, /* doubleword64 S + A - P */
	R_PPC64_PLT64           = 45, /* doubleword64 L + A */
	R_PPC64_PLTREL64        = 46, /* doubleword64 L + A - P */
	R_PPC64_TOC16           = 47, /* half16* S + A - .TOC */
	R_PPC64_TOC16_LO        = 48, /* half16 #lo(S + A - .TOC.) */
	R_PPC64_TOC16_HI        = 49, /* half16 #hi(S + A - .TOC.) */
	R_PPC64_TOC16_HA        = 50, /* half16 #ha(S + A - .TOC.) */
	R_PPC64_TOC             = 51, /* doubleword64 .TOC */
	R_PPC64_PLTGOT16        = 52, /* half16* M + A */
	R_PPC64_PLTGOT16_LO     = 53, /* half16 #lo(M + A) */
	R_PPC64_PLTGOT16_HI     = 54, /* half16 #hi(M + A) */
	R_PPC64_PLTGOT16_HA     = 55, /* half16 #ha(M + A) */
	R_PPC64_ADDR16_DS       = 56, /* half16ds* (S + A) >> 2 */
	R_PPC64_ADDR16_LO_DS    = 57, /* half16ds  #lo(S + A) >> 2 */
	R_PPC64_GOT16_DS        = 58, /* half16ds* (G + A) >> 2 */
	R_PPC64_GOT16_LO_DS     = 59, /* half16ds  #lo(G + A) >> 2 */
	R_PPC64_PLT16_LO_DS     = 60, /* half16ds  #lo(L + A) >> 2 */
	R_PPC64_SECTOFF_DS      = 61, /* half16ds* (R + A) >> 2 */
	R_PPC64_SECTOFF_LO_DS   = 62, /* half16ds  #lo(R + A) >> 2 */
	R_PPC64_TOC16_DS        = 63, /* half16ds* (S + A - .TOC.) >> 2 */
	R_PPC64_TOC16_LO_DS     = 64, /* half16ds  #lo(S + A - .TOC.) >> 2 */
	R_PPC64_PLTGOT16_DS     = 65, /* half16ds* (M + A) >> 2 */
	R_PPC64_PLTGOT16_LO_DS  = 66, /* half16ds  #lo(M + A) >> 2 */
};

/* PowerPC64 relocations defined for the TLS access ABI.  */
enum {
	R_PPC64_TLS                = 67,  /* none	(sym+add)@tls */
	R_PPC64_DTPMOD64           = 68,  /* doubleword64 (sym+add)@dtpmod */
	R_PPC64_TPREL16            = 69,  /* half16*	(sym+add)@tprel */
	R_PPC64_TPREL16_LO         = 70,  /* half16	(sym+add)@tprel@l */
	R_PPC64_TPREL16_HI         = 71,  /* half16	(sym+add)@tprel@h */
	R_PPC64_TPREL16_HA         = 72,  /* half16	(sym+add)@tprel@ha */
	R_PPC64_TPREL64            = 73,  /* doubleword64 (sym+add)@tprel */
	R_PPC64_DTPREL16           = 74,  /* half16*	(sym+add)@dtprel */
	R_PPC64_DTPREL16_LO        = 75,  /* half16	(sym+add)@dtprel@l */
	R_PPC64_DTPREL16_HI        = 76,  /* half16	(sym+add)@dtprel@h */
	R_PPC64_DTPREL16_HA        = 77,  /* half16	(sym+add)@dtprel@ha */
	R_PPC64_DTPREL64           = 78,  /* doubleword64 (sym+add)@dtprel */
	R_PPC64_GOT_TLSGD16        = 79,  /* half16*	(sym+add)@got@tlsgd */
	R_PPC64_GOT_TLSGD16_LO     = 80,  /* half16	(sym+add)@got@tlsgd@l */
	R_PPC64_GOT_TLSGD16_HI     = 81,  /* half16	(sym+add)@got@tlsgd@h */
	R_PPC64_GOT_TLSGD16_HA     = 82,  /* half16	(sym+add)@got@tlsgd@ha */
	R_PPC64_GOT_TLSLD16        = 83,  /* half16*	(sym+add)@got@tlsld */
	R_PPC64_GOT_TLSLD16_LO     = 84,  /* half16	(sym+add)@got@tlsld@l */
	R_PPC64_GOT_TLSLD16_HI     = 85,  /* half16	(sym+add)@got@tlsld@h */
	R_PPC64_GOT_TLSLD16_HA     = 86,  /* half16	(sym+add)@got@tlsld@ha */
	R_PPC64_GOT_TPREL16_DS     = 87,  /* half16ds*	(sym+add)@got@tprel */
	R_PPC64_GOT_TPREL16_LO_DS  = 88,  /* half16ds (sym+add)@got@tprel@l */
	R_PPC64_GOT_TPREL16_HI     = 89,  /* half16	(sym+add)@got@tprel@h */
	R_PPC64_GOT_TPREL16_HA     = 90,  /* half16	(sym+add)@got@tprel@ha */
	R_PPC64_GOT_DTPREL16_DS    = 91,  /* half16ds*	(sym+add)@got@dtprel */
	R_PPC64_GOT_DTPREL16_LO_DS = 92,  /* half16ds (sym+add)@got@dtprel@l */
	R_PPC64_GOT_DTPREL16_HI    = 93,  /* half16	(sym+add)@got@dtprel@h */
	R_PPC64_GOT_DTPREL16_HA    = 94,  /* half16	(sym+add)@got@dtprel@ha */
	R_PPC64_TPREL16_DS         = 95,  /* half16ds*	(sym+add)@tprel */
	R_PPC64_TPREL16_LO_DS      = 96,  /* half16ds	(sym+add)@tprel@l */
	R_PPC64_TPREL16_HIGHER     = 97,  /* half16	(sym+add)@tprel@higher */
	R_PPC64_TPREL16_HIGHERA    = 98,  /* half16	(sym+add)@tprel@highera */
	R_PPC64_TPREL16_HIGHEST    = 99,  /* half16	(sym+add)@tprel@highest */
	R_PPC64_TPREL16_HIGHESTA   = 100, /* half16	(sym+add)@tprel@highesta */
	R_PPC64_DTPREL16_DS        = 101, /* half16ds* (sym+add)@dtprel */
	R_PPC64_DTPREL16_LO_DS     = 102, /* half16ds	(sym+add)@dtprel@l */
	R_PPC64_DTPREL16_HIGHER    = 103, /* half16	(sym+add)@dtprel@higher */
	R_PPC64_DTPREL16_HIGHERA   = 104, /* half16	(sym+add)@dtprel@highera */
	R_PPC64_DTPREL16_HIGHEST   = 105, /* half16	(sym+add)@dtprel@highest */
	R_PPC64_DTPREL16_HIGHESTA  = 106, /* half16	(sym+add)@dtprel@highesta */
};

/* GNU extension to support local ifunc.  */
enum {
	R_PPC64_JMP_IREL  = 247,
	R_PPC64_IRELATIVE = 248,
	R_PPC64_REL16     = 249, /* half16   (sym+add-.) */
	R_PPC64_REL16_LO  = 250, /* half16   (sym+add-.)@l */
	R_PPC64_REL16_HI  = 251, /* half16   (sym+add-.)@h */
	R_PPC64_REL16_HA  = 252, /* half16   (sym+add-.)@ha */
};

/* PowerPC64 specific values for the Dyn d_tag field.  */
enum {
	DT_PPC64_GLINK = (DT_LOPROC + 0),
	DT_PPC64_OPD   = (DT_LOPROC + 1),
	DT_PPC64_OPDSZ = (DT_LOPROC + 2),
	DT_PPC64_NUM   = 3,
};

/* ARM specific declarations */

/* Processor specific flags for the ELF header e_flags field.  */
enum {
	EF_ARM_RELEXEC        = 0x01,
	EF_ARM_HASENTRY       = 0x02,
	EF_ARM_INTERWORK      = 0x04,
	EF_ARM_APCS_26        = 0x08,
	EF_ARM_APCS_FLOAT     = 0x10,
	EF_ARM_PIC            = 0x20,
	EF_ARM_ALIGN8         = 0x40, /* 8-bit structure alignment is in use */
	EF_ARM_NEW_ABI        = 0x80,
	EF_ARM_OLD_ABI        = 0x100,
	EF_ARM_SOFT_FLOAT     = 0x200,
	EF_ARM_VFP_FLOAT      = 0x400,
	EF_ARM_MAVERICK_FLOAT = 0x800,
};

/* Other constants defined in the ARM ELF spec. version B-01.  */
/* NB. These conflict with values defined above.  */
enum {
	EF_ARM_SYMSARESORTED    = 0x04,
	EF_ARM_DYNSYMSUSESEGIDX = 0x08,
	EF_ARM_MAPSYMSFIRST     = 0x10,
	EF_ARM_EABIMASK         = 0XFF000000
};

/* Constants defined in AAELF.  */
enum {
	EF_ARM_BE8 = 0x00800000,
	EF_ARM_LE8 = 0x00400000,
};

template <class T>
constexpr T EF_ARM_EABI_VERSION(T flags) {
	return flags & EF_ARM_EABIMASK;
}

enum {
	EF_ARM_EABI_UNKNOWN = 0x00000000,
	EF_ARM_EABI_VER1    = 0x01000000,
	EF_ARM_EABI_VER2    = 0x02000000,
	EF_ARM_EABI_VER3    = 0x03000000,
	EF_ARM_EABI_VER4    = 0x04000000,
	EF_ARM_EABI_VER5    = 0x05000000,
};

/* Additional symbol types for Thumb.  */
enum {
	STT_ARM_TFUNC = STT_LOPROC, /* A Thumb function.  */
	STT_ARM_16BIT = STT_HIPROC, /* A Thumb label.  */
};

/* ARM-specific values for sh_flags */
enum {
	SHF_ARM_ENTRYSECT = 0x10000000, /* Section contains an entry point */
	SHF_ARM_COMDEF    = 0x80000000, /* Section may be multiply defined	in the input to a link step.  */
};

/* ARM-specific program header flags */
enum {
	PF_ARM_SB  = 0x10000000, /* Segment contains the location addressed by the static base. */
	PF_ARM_PI  = 0x20000000, /* Position-independent segment.  */
	PF_ARM_ABS = 0x40000000, /* Absolute segment.  */
};

/* Processor specific values for the Phdr p_type field.  */
enum {
	PT_ARM_EXIDX = (PT_LOPROC + 1), /* ARM unwind segment.  */
};

/* Processor specific values for the Shdr sh_type field.  */
enum {
	SHT_ARM_EXIDX      = (SHT_LOPROC + 1), /* ARM unwind section.  */
	SHT_ARM_PREEMPTMAP = (SHT_LOPROC + 2), /* Preemption details.  */
	SHT_ARM_ATTRIBUTES = (SHT_LOPROC + 3), /* ARM attributes section.  */
};

/* ARM relocs.  */
enum {
	R_ARM_NONE            = 0, /* No reloc */
	R_ARM_PC24            = 1, /* PC relative 26 bit branch */
	R_ARM_ABS32           = 2, /* Direct 32 bit  */
	R_ARM_REL32           = 3, /* PC relative 32 bit */
	R_ARM_PC13            = 4,
	R_ARM_ABS16           = 5, /* Direct 16 bit */
	R_ARM_ABS12           = 6, /* Direct 12 bit */
	R_ARM_THM_ABS5        = 7,
	R_ARM_ABS8            = 8, /* Direct 8 bit */
	R_ARM_SBREL32         = 9,
	R_ARM_THM_PC22        = 10,
	R_ARM_THM_PC8         = 11,
	R_ARM_AMP_VCALL9      = 12,
	R_ARM_SWI24           = 13, /* Obsolete static relocation.  */
	R_ARM_TLS_DESC        = 13, /* Dynamic relocation.  */
	R_ARM_THM_SWI8        = 14,
	R_ARM_XPC25           = 15,
	R_ARM_THM_XPC22       = 16,
	R_ARM_TLS_DTPMOD32    = 17, /* ID of module containing symbol */
	R_ARM_TLS_DTPOFF32    = 18, /* Offset in TLS block */
	R_ARM_TLS_TPOFF32     = 19, /* Offset in static TLS block */
	R_ARM_COPY            = 20, /* Copy symbol at runtime */
	R_ARM_GLOB_DAT        = 21, /* Create GOT entry */
	R_ARM_JUMP_SLOT       = 22, /* Create PLT entry */
	R_ARM_RELATIVE        = 23, /* Adjust by program base */
	R_ARM_GOTOFF          = 24, /* 32 bit offset to GOT */
	R_ARM_GOTPC           = 25, /* 32 bit PC relative offset to GOT */
	R_ARM_GOT32           = 26, /* 32 bit GOT entry */
	R_ARM_PLT32           = 27, /* 32 bit PLT address */
	R_ARM_ALU_PCREL_7_0   = 32,
	R_ARM_ALU_PCREL_15_8  = 33,
	R_ARM_ALU_PCREL_23_15 = 34,
	R_ARM_LDR_SBREL_11_0  = 35,
	R_ARM_ALU_SBREL_19_12 = 36,
	R_ARM_ALU_SBREL_27_20 = 37,
	R_ARM_TLS_GOTDESC     = 90,
	R_ARM_TLS_CALL        = 91,
	R_ARM_TLS_DESCSEQ     = 92,
	R_ARM_THM_TLS_CALL    = 93,
	R_ARM_GNU_VTENTRY     = 100,
	R_ARM_GNU_VTINHERIT   = 101,
	R_ARM_THM_PC11        = 102, /* thumb unconditional branch */
	R_ARM_THM_PC9         = 103, /* thumb conditional branch */
	R_ARM_TLS_GD32        = 104, /* PC-rel 32 bit for global dynamic thread local data */
	R_ARM_TLS_LDM32       = 105, /* PC-rel 32 bit for local dynamic thread local data */
	R_ARM_TLS_LDO32       = 106, /* 32 bit offset relative to TLS block */
	R_ARM_TLS_IE32        = 107, /* PC-rel 32 bit for GOT entry of static TLS block offset */
	R_ARM_TLS_LE32        = 108, /* 32 bit offset relative to static TLS block */
	R_ARM_THM_TLS_DESCSEQ = 129,
	R_ARM_IRELATIVE       = 160,
	R_ARM_RXPC25          = 249,
	R_ARM_RSBREL32        = 250,
	R_ARM_THM_RPC22       = 251,
	R_ARM_RREL32          = 252,
	R_ARM_RABS22          = 253,
	R_ARM_RPC24           = 254,
	R_ARM_RBASE           = 255,

	/* Keep this the last entry.  */
	R_ARM_NUM = 256,
};

/* IA-64 specific declarations.  */

/* Processor specific flags for the Ehdr e_flags field.  */
enum {
	EF_IA_64_MASKOS = 0x0000000f, /* os-specific flags */
	EF_IA_64_ABI64  = 0x00000010, /* 64-bit ABI */
	EF_IA_64_ARCH   = 0xff000000, /* arch. version mask */
};

/* Processor specific values for the Phdr p_type field.  */
enum {
	PT_IA_64_ARCHEXT     = (PT_LOPROC + 0), /* arch extension bits */
	PT_IA_64_UNWIND      = (PT_LOPROC + 1), /* ia64 unwind bits */
	PT_IA_64_HP_OPT_ANOT = (PT_LOOS + 0x12),
	PT_IA_64_HP_HSL_ANOT = (PT_LOOS + 0x13),
	PT_IA_64_HP_STACK    = (PT_LOOS + 0x14),
};

/* Processor specific flags for the Phdr p_flags field.  */
enum {
	PF_IA_64_NORECOV = 0x80000000, /* spec insns w/o recovery */
};

/* Processor specific values for the Shdr sh_type field.  */
enum {
	SHT_IA_64_EXT    = (SHT_LOPROC + 0), /* extension bits */
	SHT_IA_64_UNWIND = (SHT_LOPROC + 1), /* unwind bits */
};

/* Processor specific flags for the Shdr sh_flags field.  */
enum {
	SHF_IA_64_SHORT   = 0x10000000, /* section near gp */
	SHF_IA_64_NORECOV = 0x20000000, /* spec insns w/o recovery */
};

/* Processor specific values for the Dyn d_tag field.  */
enum {
	DT_IA_64_PLT_RESERVE = (DT_LOPROC + 0),
	DT_IA_64_NUM         = 1,
};

/* IA-64 relocations.  */
enum {
	R_IA64_NONE            = 0x00, /* none */
	R_IA64_IMM14           = 0x21, /* symbol + addend, add imm14 */
	R_IA64_IMM22           = 0x22, /* symbol + addend, add imm22 */
	R_IA64_IMM64           = 0x23, /* symbol + addend, mov imm64 */
	R_IA64_DIR32MSB        = 0x24, /* symbol + addend, data4 MSB */
	R_IA64_DIR32LSB        = 0x25, /* symbol + addend, data4 LSB */
	R_IA64_DIR64MSB        = 0x26, /* symbol + addend, data8 MSB */
	R_IA64_DIR64LSB        = 0x27, /* symbol + addend, data8 LSB */
	R_IA64_GPREL22         = 0x2a, /* @gprel(sym + add), add imm22 */
	R_IA64_GPREL64I        = 0x2b, /* @gprel(sym + add), mov imm64 */
	R_IA64_GPREL32MSB      = 0x2c, /* @gprel(sym + add), data4 MSB */
	R_IA64_GPREL32LSB      = 0x2d, /* @gprel(sym + add), data4 LSB */
	R_IA64_GPREL64MSB      = 0x2e, /* @gprel(sym + add), data8 MSB */
	R_IA64_GPREL64LSB      = 0x2f, /* @gprel(sym + add), data8 LSB */
	R_IA64_LTOFF22         = 0x32, /* @ltoff(sym + add), add imm22 */
	R_IA64_LTOFF64I        = 0x33, /* @ltoff(sym + add), mov imm64 */
	R_IA64_PLTOFF22        = 0x3a, /* @pltoff(sym + add), add imm22 */
	R_IA64_PLTOFF64I       = 0x3b, /* @pltoff(sym + add), mov imm64 */
	R_IA64_PLTOFF64MSB     = 0x3e, /* @pltoff(sym + add), data8 MSB */
	R_IA64_PLTOFF64LSB     = 0x3f, /* @pltoff(sym + add), data8 LSB */
	R_IA64_FPTR64I         = 0x43, /* @fptr(sym + add), mov imm64 */
	R_IA64_FPTR32MSB       = 0x44, /* @fptr(sym + add), data4 MSB */
	R_IA64_FPTR32LSB       = 0x45, /* @fptr(sym + add), data4 LSB */
	R_IA64_FPTR64MSB       = 0x46, /* @fptr(sym + add), data8 MSB */
	R_IA64_FPTR64LSB       = 0x47, /* @fptr(sym + add), data8 LSB */
	R_IA64_PCREL60B        = 0x48, /* @pcrel(sym + add), brl */
	R_IA64_PCREL21B        = 0x49, /* @pcrel(sym + add), ptb, call */
	R_IA64_PCREL21M        = 0x4a, /* @pcrel(sym + add), chk.s */
	R_IA64_PCREL21F        = 0x4b, /* @pcrel(sym + add), fchkf */
	R_IA64_PCREL32MSB      = 0x4c, /* @pcrel(sym + add), data4 MSB */
	R_IA64_PCREL32LSB      = 0x4d, /* @pcrel(sym + add), data4 LSB */
	R_IA64_PCREL64MSB      = 0x4e, /* @pcrel(sym + add), data8 MSB */
	R_IA64_PCREL64LSB      = 0x4f, /* @pcrel(sym + add), data8 LSB */
	R_IA64_LTOFF_FPTR22    = 0x52, /* @ltoff(@fptr(s+a)), imm22 */
	R_IA64_LTOFF_FPTR64I   = 0x53, /* @ltoff(@fptr(s+a)), imm64 */
	R_IA64_LTOFF_FPTR32MSB = 0x54, /* @ltoff(@fptr(s+a)), data4 MSB */
	R_IA64_LTOFF_FPTR32LSB = 0x55, /* @ltoff(@fptr(s+a)), data4 LSB */
	R_IA64_LTOFF_FPTR64MSB = 0x56, /* @ltoff(@fptr(s+a)), data8 MSB */
	R_IA64_LTOFF_FPTR64LSB = 0x57, /* @ltoff(@fptr(s+a)), data8 LSB */
	R_IA64_SEGREL32MSB     = 0x5c, /* @segrel(sym + add), data4 MSB */
	R_IA64_SEGREL32LSB     = 0x5d, /* @segrel(sym + add), data4 LSB */
	R_IA64_SEGREL64MSB     = 0x5e, /* @segrel(sym + add), data8 MSB */
	R_IA64_SEGREL64LSB     = 0x5f, /* @segrel(sym + add), data8 LSB */
	R_IA64_SECREL32MSB     = 0x64, /* @secrel(sym + add), data4 MSB */
	R_IA64_SECREL32LSB     = 0x65, /* @secrel(sym + add), data4 LSB */
	R_IA64_SECREL64MSB     = 0x66, /* @secrel(sym + add), data8 MSB */
	R_IA64_SECREL64LSB     = 0x67, /* @secrel(sym + add), data8 LSB */
	R_IA64_REL32MSB        = 0x6c, /* data 4 + REL */
	R_IA64_REL32LSB        = 0x6d, /* data 4 + REL */
	R_IA64_REL64MSB        = 0x6e, /* data 8 + REL */
	R_IA64_REL64LSB        = 0x6f, /* data 8 + REL */
	R_IA64_LTV32MSB        = 0x74, /* symbol + addend, data4 MSB */
	R_IA64_LTV32LSB        = 0x75, /* symbol + addend, data4 LSB */
	R_IA64_LTV64MSB        = 0x76, /* symbol + addend, data8 MSB */
	R_IA64_LTV64LSB        = 0x77, /* symbol + addend, data8 LSB */
	R_IA64_PCREL21BI       = 0x79, /* @pcrel(sym + add), 21bit inst */
	R_IA64_PCREL22         = 0x7a, /* @pcrel(sym + add), 22bit inst */
	R_IA64_PCREL64I        = 0x7b, /* @pcrel(sym + add), 64bit inst */
	R_IA64_IPLTMSB         = 0x80, /* dynamic reloc, imported PLT, MSB */
	R_IA64_IPLTLSB         = 0x81, /* dynamic reloc, imported PLT, LSB */
	R_IA64_COPY            = 0x84, /* copy relocation */
	R_IA64_SUB             = 0x85, /* Addend and symbol difference */
	R_IA64_LTOFF22X        = 0x86, /* LTOFF22, relaxable.  */
	R_IA64_LDXMOV          = 0x87, /* Use of LTOFF22X.  */
	R_IA64_TPREL14         = 0x91, /* @tprel(sym + add), imm14 */
	R_IA64_TPREL22         = 0x92, /* @tprel(sym + add), imm22 */
	R_IA64_TPREL64I        = 0x93, /* @tprel(sym + add), imm64 */
	R_IA64_TPREL64MSB      = 0x96, /* @tprel(sym + add), data8 MSB */
	R_IA64_TPREL64LSB      = 0x97, /* @tprel(sym + add), data8 LSB */
	R_IA64_LTOFF_TPREL22   = 0x9a, /* @ltoff(@tprel(s+a)), imm2 */
	R_IA64_DTPMOD64MSB     = 0xa6, /* @dtpmod(sym + add), data8 MSB */
	R_IA64_DTPMOD64LSB     = 0xa7, /* @dtpmod(sym + add), data8 LSB */
	R_IA64_LTOFF_DTPMOD22  = 0xaa, /* @ltoff(@dtpmod(sym + add)), imm22 */
	R_IA64_DTPREL14        = 0xb1, /* @dtprel(sym + add), imm14 */
	R_IA64_DTPREL22        = 0xb2, /* @dtprel(sym + add), imm22 */
	R_IA64_DTPREL64I       = 0xb3, /* @dtprel(sym + add), imm64 */
	R_IA64_DTPREL32MSB     = 0xb4, /* @dtprel(sym + add), data4 MSB */
	R_IA64_DTPREL32LSB     = 0xb5, /* @dtprel(sym + add), data4 LSB */
	R_IA64_DTPREL64MSB     = 0xb6, /* @dtprel(sym + add), data8 MSB */
	R_IA64_DTPREL64LSB     = 0xb7, /* @dtprel(sym + add), data8 LSB */
	R_IA64_LTOFF_DTPREL22  = 0xba, /* @ltoff(@dtprel(s+a)), imm22 */
};

/* SH specific declarations */

/* Processor specific flags for the ELF header e_flags field.  */
enum {
	EF_SH_MACH_MASK    = 0x1f,
	EF_SH_UNKNOWN      = 0x0,
	EF_SH1             = 0x1,
	EF_SH2             = 0x2,
	EF_SH3             = 0x3,
	EF_SH_DSP          = 0x4,
	EF_SH3_DSP         = 0x5,
	EF_SH4AL_DSP       = 0x6,
	EF_SH3E            = 0x8,
	EF_SH4             = 0x9,
	EF_SH2E            = 0xb,
	EF_SH4A            = 0xc,
	EF_SH2A            = 0xd,
	EF_SH4_NOFPU       = 0x10,
	EF_SH4A_NOFPU      = 0x11,
	EF_SH4_NOMMU_NOFPU = 0x12,
	EF_SH2A_NOFPU      = 0x13,
	EF_SH3_NOMMU       = 0x14,
	EF_SH2A_SH4_NOFPU  = 0x15,
	EF_SH2A_SH3_NOFPU  = 0x16,
	EF_SH2A_SH4        = 0x17,
	EF_SH2A_SH3E       = 0x18,
};

/* SH relocs.  */
enum {
	R_SH_NONE          = 0,
	R_SH_DIR32         = 1,
	R_SH_REL32         = 2,
	R_SH_DIR8WPN       = 3,
	R_SH_IND12W        = 4,
	R_SH_DIR8WPL       = 5,
	R_SH_DIR8WPZ       = 6,
	R_SH_DIR8BP        = 7,
	R_SH_DIR8W         = 8,
	R_SH_DIR8L         = 9,
	R_SH_SWITCH16      = 25,
	R_SH_SWITCH32      = 26,
	R_SH_USES          = 27,
	R_SH_COUNT         = 28,
	R_SH_ALIGN         = 29,
	R_SH_CODE          = 30,
	R_SH_DATA          = 31,
	R_SH_LABEL         = 32,
	R_SH_SWITCH8       = 33,
	R_SH_GNU_VTINHERIT = 34,
	R_SH_GNU_VTENTRY   = 35,
	R_SH_TLS_GD_32     = 144,
	R_SH_TLS_LD_32     = 145,
	R_SH_TLS_LDO_32    = 146,
	R_SH_TLS_IE_32     = 147,
	R_SH_TLS_LE_32     = 148,
	R_SH_TLS_DTPMOD32  = 149,
	R_SH_TLS_DTPOFF32  = 150,
	R_SH_TLS_TPOFF32   = 151,
	R_SH_GOT32         = 160,
	R_SH_PLT32         = 161,
	R_SH_COPY          = 162,
	R_SH_GLOB_DAT      = 163,
	R_SH_JMP_SLOT      = 164,
	R_SH_RELATIVE      = 165,
	R_SH_GOTOFF        = 166,
	R_SH_GOTPC         = 167,

	/* Keep this the last entry.  */
	R_SH_NUM = 256,
};

/* S/390 specific definitions.  */

/* Valid values for the e_flags field.  */
enum {
	EF_S390_HIGH_GPRS = 0x00000001, /* High GPRs kernel facility needed.  */
};

/* Additional s390 relocs */
enum {
	R_390_NONE        = 0,  /* No reloc.  */
	R_390_8           = 1,  /* Direct 8 bit.  */
	R_390_12          = 2,  /* Direct 12 bit.  */
	R_390_16          = 3,  /* Direct 16 bit.  */
	R_390_32          = 4,  /* Direct 32 bit.  */
	R_390_PC32        = 5,  /* PC relative 32 bit.	*/
	R_390_GOT12       = 6,  /* 12 bit GOT offset.  */
	R_390_GOT32       = 7,  /* 32 bit GOT offset.  */
	R_390_PLT32       = 8,  /* 32 bit PC relative PLT address.  */
	R_390_COPY        = 9,  /* Copy symbol at runtime.  */
	R_390_GLOB_DAT    = 10, /* Create GOT entry.  */
	R_390_JMP_SLOT    = 11, /* Create PLT entry.  */
	R_390_RELATIVE    = 12, /* Adjust by program base.  */
	R_390_GOTOFF32    = 13, /* 32 bit offset to GOT.	 */
	R_390_GOTPC       = 14, /* 32 bit PC relative offset to GOT.  */
	R_390_GOT16       = 15, /* 16 bit GOT offset.  */
	R_390_PC16        = 16, /* PC relative 16 bit.	*/
	R_390_PC16DBL     = 17, /* PC relative 16 bit shifted by 1.  */
	R_390_PLT16DBL    = 18, /* 16 bit PC rel. PLT shifted by 1.  */
	R_390_PC32DBL     = 19, /* PC relative 32 bit shifted by 1.  */
	R_390_PLT32DBL    = 20, /* 32 bit PC rel. PLT shifted by 1.  */
	R_390_GOTPCDBL    = 21, /* 32 bit PC rel. GOT shifted by 1.  */
	R_390_64          = 22, /* Direct 64 bit.  */
	R_390_PC64        = 23, /* PC relative 64 bit.	*/
	R_390_GOT64       = 24, /* 64 bit GOT offset.  */
	R_390_PLT64       = 25, /* 64 bit PC relative PLT address.  */
	R_390_GOTENT      = 26, /* 32 bit PC rel. to GOT entry >> 1. */
	R_390_GOTOFF16    = 27, /* 16 bit offset to GOT. */
	R_390_GOTOFF64    = 28, /* 64 bit offset to GOT. */
	R_390_GOTPLT12    = 29, /* 12 bit offset to jump slot.	*/
	R_390_GOTPLT16    = 30, /* 16 bit offset to jump slot.	*/
	R_390_GOTPLT32    = 31, /* 32 bit offset to jump slot.	*/
	R_390_GOTPLT64    = 32, /* 64 bit offset to jump slot.	*/
	R_390_GOTPLTENT   = 33, /* 32 bit rel. offset to jump slot.  */
	R_390_PLTOFF16    = 34, /* 16 bit offset from GOT to PLT. */
	R_390_PLTOFF32    = 35, /* 32 bit offset from GOT to PLT. */
	R_390_PLTOFF64    = 36, /* 16 bit offset from GOT to PLT. */
	R_390_TLS_LOAD    = 37, /* Tag for load insn in TLS code.  */
	R_390_TLS_GDCALL  = 38, /* Tag for function call in general dynamic TLS code. */
	R_390_TLS_LDCALL  = 39, /* Tag for function call in local dynamic TLS code. */
	R_390_TLS_GD32    = 40, /* Direct 32 bit for general dynamic thread local data.  */
	R_390_TLS_GD64    = 41, /* Direct 64 bit for general dynamic thread local data.  */
	R_390_TLS_GOTIE12 = 42, /* 12 bit GOT offset for static TLS block offset.  */
	R_390_TLS_GOTIE32 = 43, /* 32 bit GOT offset for static TLS block offset.  */
	R_390_TLS_GOTIE64 = 44, /* 64 bit GOT offset for static TLS block offset. */
	R_390_TLS_LDM32   = 45, /* Direct 32 bit for local dynamic   thread local data in LE code.  */
	R_390_TLS_LDM64   = 46, /* Direct 64 bit for local dynamic   thread local data in LE code.  */
	R_390_TLS_IE32    = 47, /* 32 bit address of GOT entry for	negated static TLS block offset.  */
	R_390_TLS_IE64    = 48, /* 64 bit address of GOT entry for	negated static TLS block offset.  */
	R_390_TLS_IEENT   = 49, /* 32 bit rel. offset to GOT entry for   negated static TLS block offset.  */
	R_390_TLS_LE32    = 50, /* 32 bit negated offset relative to static TLS block.  */
	R_390_TLS_LE64    = 51, /* 64 bit negated offset relative to static TLS block.  */
	R_390_TLS_LDO32   = 52, /* 32 bit offset relative to TLS block.  */
	R_390_TLS_LDO64   = 53, /* 64 bit offset relative to TLS block.  */
	R_390_TLS_DTPMOD  = 54, /* ID of module containing symbol.  */
	R_390_TLS_DTPOFF  = 55, /* Offset in TLS block.	 */
	R_390_TLS_TPOFF   = 56, /* Negated offset in static TLS block.  */
	R_390_20          = 57, /* Direct 20 bit.  */
	R_390_GOT20       = 58, /* 20 bit GOT offset.  */
	R_390_GOTPLT20    = 59, /* 20 bit offset to jump slot.  */
	R_390_TLS_GOTIE20 = 60, /* 20 bit GOT offset for static TLS block offset.  */

	/* Keep this the last entry.  */
	R_390_NUM = 61,
};

/* CRIS relocations.  */
enum {
	R_CRIS_NONE          = 0,
	R_CRIS_8             = 1,
	R_CRIS_16            = 2,
	R_CRIS_32            = 3,
	R_CRIS_8_PCREL       = 4,
	R_CRIS_16_PCREL      = 5,
	R_CRIS_32_PCREL      = 6,
	R_CRIS_GNU_VTINHERIT = 7,
	R_CRIS_GNU_VTENTRY   = 8,
	R_CRIS_COPY          = 9,
	R_CRIS_GLOB_DAT      = 10,
	R_CRIS_JUMP_SLOT     = 11,
	R_CRIS_RELATIVE      = 12,
	R_CRIS_16_GOT        = 13,
	R_CRIS_32_GOT        = 14,
	R_CRIS_16_GOTPLT     = 15,
	R_CRIS_32_GOTPLT     = 16,
	R_CRIS_32_GOTREL     = 17,
	R_CRIS_32_PLT_GOTREL = 18,
	R_CRIS_32_PLT_PCREL  = 19,

	R_CRIS_NUM = 20,
};

/* AMD x86-64 relocations.  */
enum {
	R_X86_64_NONE            = 0,  /* No reloc */
	R_X86_64_64              = 1,  /* Direct 64 bit  */
	R_X86_64_PC32            = 2,  /* PC relative 32 bit signed */
	R_X86_64_GOT32           = 3,  /* 32 bit GOT entry */
	R_X86_64_PLT32           = 4,  /* 32 bit PLT address */
	R_X86_64_COPY            = 5,  /* Copy symbol at runtime */
	R_X86_64_GLOB_DAT        = 6,  /* Create GOT entry */
	R_X86_64_JUMP_SLOT       = 7,  /* Create PLT entry */
	R_X86_64_RELATIVE        = 8,  /* Adjust by program base */
	R_X86_64_GOTPCREL        = 9,  /* 32 bit signed PC relative  offset to GOT */
	R_X86_64_32              = 10, /* Direct 32 bit zero extended */
	R_X86_64_32S             = 11, /* Direct 32 bit sign extended */
	R_X86_64_16              = 12, /* Direct 16 bit zero extended */
	R_X86_64_PC16            = 13, /* 16 bit sign extended pc relative */
	R_X86_64_8               = 14, /* Direct 8 bit sign extended  */
	R_X86_64_PC8             = 15, /* 8 bit sign extended pc relative */
	R_X86_64_DTPMOD64        = 16, /* ID of module containing symbol */
	R_X86_64_DTPOFF64        = 17, /* Offset in module's TLS block */
	R_X86_64_TPOFF64         = 18, /* Offset in initial TLS block */
	R_X86_64_TLSGD           = 19, /* 32 bit signed PC relative offset to two GOT entries for GD symbol */
	R_X86_64_TLSLD           = 20, /* 32 bit signed PC relative offset to two GOT entries for LD symbol */
	R_X86_64_DTPOFF32        = 21, /* Offset in TLS block */
	R_X86_64_GOTTPOFF        = 22, /* 32 bit signed PC relative offset GOT entry for IE symbol */
	R_X86_64_TPOFF32         = 23, /* Offset in initial TLS block */
	R_X86_64_PC64            = 24, /* PC relative 64 bit */
	R_X86_64_GOTOFF64        = 25, /* 64 bit offset to GOT */
	R_X86_64_GOTPC32         = 26, /* 32 bit signed pc relative offset to GOT */
	R_X86_64_GOT64           = 27, /* 64-bit GOT entry offset */
	R_X86_64_GOTPCREL64      = 28, /* 64-bit PC relative offset to GOT entry */
	R_X86_64_GOTPC64         = 29, /* 64-bit PC relative offset to GOT */
	R_X86_64_GOTPLT64        = 30, /* like GOT64, says PLT entry needed */
	R_X86_64_PLTOFF64        = 31, /* 64-bit GOT relative offset to PLT entry */
	R_X86_64_SIZE32          = 32, /* Size of symbol plus 32-bit addend */
	R_X86_64_SIZE64          = 33, /* Size of symbol plus 64-bit addend */
	R_X86_64_GOTPC32_TLSDESC = 34, /* GOT offset for TLS descriptor.  */
	R_X86_64_TLSDESC_CALL    = 35, /* Marker for call through TLS descriptor.  */
	R_X86_64_TLSDESC         = 36, /* TLS descriptor.  */
	R_X86_64_IRELATIVE       = 37, /* Adjust indirectly by program base */

	R_X86_64_NUM = 38,
};

/* AM33 relocations.  */
enum {
	R_MN10300_NONE          = 0,  /* No reloc.  */
	R_MN10300_32            = 1,  /* Direct 32 bit.  */
	R_MN10300_16            = 2,  /* Direct 16 bit.  */
	R_MN10300_8             = 3,  /* Direct 8 bit.  */
	R_MN10300_PCREL32       = 4,  /* PC-relative 32-bit.  */
	R_MN10300_PCREL16       = 5,  /* PC-relative 16-bit signed.  */
	R_MN10300_PCREL8        = 6,  /* PC-relative 8-bit signed.  */
	R_MN10300_GNU_VTINHERIT = 7,  /* Ancient C++ vtable garbage... */
	R_MN10300_GNU_VTENTRY   = 8,  /* ... collection annotation.  */
	R_MN10300_24            = 9,  /* Direct 24 bit.  */
	R_MN10300_GOTPC32       = 10, /* 32-bit PCrel offset to GOT.  */
	R_MN10300_GOTPC16       = 11, /* 16-bit PCrel offset to GOT.  */
	R_MN10300_GOTOFF32      = 12, /* 32-bit offset from GOT.  */
	R_MN10300_GOTOFF24      = 13, /* 24-bit offset from GOT.  */
	R_MN10300_GOTOFF16      = 14, /* 16-bit offset from GOT.  */
	R_MN10300_PLT32         = 15, /* 32-bit PCrel to PLT entry.  */
	R_MN10300_PLT16         = 16, /* 16-bit PCrel to PLT entry.  */
	R_MN10300_GOT32         = 17, /* 32-bit offset to GOT entry.  */
	R_MN10300_GOT24         = 18, /* 24-bit offset to GOT entry.  */
	R_MN10300_GOT16         = 19, /* 16-bit offset to GOT entry.  */
	R_MN10300_COPY          = 20, /* Copy symbol at runtime.  */
	R_MN10300_GLOB_DAT      = 21, /* Create GOT entry.  */
	R_MN10300_JMP_SLOT      = 22, /* Create PLT entry.  */
	R_MN10300_RELATIVE      = 23, /* Adjust by program base.  */

	R_MN10300_NUM = 24,
};

/* M32R relocs.  */
enum {
	R_M32R_NONE          = 0,  /* No reloc. */
	R_M32R_16            = 1,  /* Direct 16 bit. */
	R_M32R_32            = 2,  /* Direct 32 bit. */
	R_M32R_24            = 3,  /* Direct 24 bit. */
	R_M32R_10_PCREL      = 4,  /* PC relative 10 bit shifted. */
	R_M32R_18_PCREL      = 5,  /* PC relative 18 bit shifted. */
	R_M32R_26_PCREL      = 6,  /* PC relative 26 bit shifted. */
	R_M32R_HI16_ULO      = 7,  /* High 16 bit with unsigned low. */
	R_M32R_HI16_SLO      = 8,  /* High 16 bit with signed low. */
	R_M32R_LO16          = 9,  /* Low 16 bit. */
	R_M32R_SDA16         = 10, /* 16 bit offset in SDA. */
	R_M32R_GNU_VTINHERIT = 11,
	R_M32R_GNU_VTENTRY   = 12,
};

/* M32R relocs use SHT_RELA.  */
enum {
	R_M32R_16_RELA            = 33, /* Direct 16 bit. */
	R_M32R_32_RELA            = 34, /* Direct 32 bit. */
	R_M32R_24_RELA            = 35, /* Direct 24 bit. */
	R_M32R_10_PCREL_RELA      = 36, /* PC relative 10 bit shifted. */
	R_M32R_18_PCREL_RELA      = 37, /* PC relative 18 bit shifted. */
	R_M32R_26_PCREL_RELA      = 38, /* PC relative 26 bit shifted. */
	R_M32R_HI16_ULO_RELA      = 39, /* High 16 bit with unsigned low */
	R_M32R_HI16_SLO_RELA      = 40, /* High 16 bit with signed low */
	R_M32R_LO16_RELA          = 41, /* Low 16 bit */
	R_M32R_SDA16_RELA         = 42, /* 16 bit offset in SDA */
	R_M32R_RELA_GNU_VTINHERIT = 43,
	R_M32R_RELA_GNU_VTENTRY   = 44,
	R_M32R_REL32              = 45, /* PC relative 32 bit.  */

	R_M32R_GOT24         = 48, /* 24 bit GOT entry */
	R_M32R_26_PLTREL     = 49, /* 26 bit PC relative to PLT shifted */
	R_M32R_COPY          = 50, /* Copy symbol at runtime */
	R_M32R_GLOB_DAT      = 51, /* Create GOT entry */
	R_M32R_JMP_SLOT      = 52, /* Create PLT entry */
	R_M32R_RELATIVE      = 53, /* Adjust by program base */
	R_M32R_GOTOFF        = 54, /* 24 bit offset to GOT */
	R_M32R_GOTPC24       = 55, /* 24 bit PC relative offset to GOT */
	R_M32R_GOT16_HI_ULO  = 56, /* High 16 bit GOT entry with unsigned low */
	R_M32R_GOT16_HI_SLO  = 57, /* High 16 bit GOT entry with signed low */
	R_M32R_GOT16_LO      = 58, /* Low 16 bit GOT entry */
	R_M32R_GOTPC_HI_ULO  = 59, /* High 16 bit PC relative offset to GOT with unsigned low */
	R_M32R_GOTPC_HI_SLO  = 60, /* High 16 bit PC relative offset to GOT with signed low */
	R_M32R_GOTPC_LO      = 61, /* Low 16 bit PC relative offset to GOT */
	R_M32R_GOTOFF_HI_ULO = 62, /* High 16 bit offset to GOT with unsigned low */
	R_M32R_GOTOFF_HI_SLO = 63, /* High 16 bit offset to GOT with signed low */
	R_M32R_GOTOFF_LO     = 64, /* Low 16 bit offset to GOT */

	R_M32R_NUM = 256, /* Keep this the last entry. */
};

#endif
