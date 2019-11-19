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

#ifndef ELF_HEADER_H_20121007_
#define ELF_HEADER_H_20121007_

#include "elf_types.h"

// The ELF file header.  This appears at the start of every ELF file.
enum { EI_NIDENT = 16 };

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */
enum {
	EI_MAG0 = 0, /* File identification byte 0 index */
	EI_MAG1 = 1, /* File identification byte 1 index */
	EI_MAG2 = 2, /* File identification byte 2 index */
	EI_MAG3 = 3, /* File identification byte 3 index */
};

#define ELFMAG0 0x7f /* Magic number byte 0 */
#define ELFMAG1 'E'  /* Magic number byte 1 */
#define ELFMAG2 'L'  /* Magic number byte 2 */
#define ELFMAG3 'F'  /* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define ELFMAG "\177ELF"
#define SELFMAG 4

enum {
	EI_CLASS     = 4, /* File class byte index */
	ELFCLASSNONE = 0, /* Invalid class */
	ELFCLASS32   = 1, /* 32-bit objects */
	ELFCLASS64   = 2, /* 64-bit objects */
	ELFCLASSNUM  = 3,

	EI_DATA     = 5, /* Data encoding byte index */
	ELFDATANONE = 0, /* Invalid data encoding */
	ELFDATA2LSB = 1, /* 2's complement, little endian */
	ELFDATA2MSB = 2, /* 2's complement, big endian */
	ELFDATANUM  = 3,

	EI_VERSION = 6, /* File version byte index */
					/* Value must be EV_CURRENT */
};

/* Legal values for e_version (version).  */
enum {
	EV_NONE    = 0, /* Invalid ELF version */
	EV_CURRENT = 1, /* Current version */
	EV_NUM     = 2,
};

enum {
	EI_OSABI            = 7,            /* OS ABI identification */
	ELFOSABI_NONE       = 0,            /* UNIX System V ABI */
	ELFOSABI_SYSV       = 0,            /* Alias.  */
	ELFOSABI_HPUX       = 1,            /* HP-UX */
	ELFOSABI_NETBSD     = 2,            /* NetBSD.  */
	ELFOSABI_GNU        = 3,            /* Object uses GNU ELF extensions.  */
	ELFOSABI_LINUX      = ELFOSABI_GNU, /* Compatibility alias.  */
	ELFOSABI_SOLARIS    = 6,            /* Sun Solaris.  */
	ELFOSABI_AIX        = 7,            /* IBM AIX.  */
	ELFOSABI_IRIX       = 8,            /* SGI Irix.  */
	ELFOSABI_FREEBSD    = 9,            /* FreeBSD.  */
	ELFOSABI_TRU64      = 10,           /* Compaq TRU64 UNIX.  */
	ELFOSABI_MODESTO    = 11,           /* Novell Modesto.  */
	ELFOSABI_OPENBSD    = 12,           /* OpenBSD.  */
	ELFOSABI_ARM_AEABI  = 64,           /* ARM EABI */
	ELFOSABI_ARM        = 97,           /* ARM */
	ELFOSABI_STANDALONE = 255,          /* Standalone (embedded) application */

	EI_ABIVERSION = 8, /* ABI version */

	EI_PAD = 9, /* Byte index of padding bytes */
};

/* Legal values for e_type (object file type).  */
enum {
	ET_NONE   = 0,      /* No file type */
	ET_REL    = 1,      /* Relocatable file */
	ET_EXEC   = 2,      /* Executable file */
	ET_DYN    = 3,      /* Shared object file */
	ET_CORE   = 4,      /* Core file */
	ET_NUM    = 5,      /* Number of defined types */
	ET_LOOS   = 0xfe00, /* OS-specific range start */
	ET_HIOS   = 0xfeff, /* OS-specific range end */
	ET_LOPROC = 0xff00, /* Processor-specific range start */
	ET_HIPROC = 0xffff, /* Processor-specific range end */
};

/* Legal values for e_machine (architecture).  */
enum {
	EM_NONE        = 0,  /* No machine */
	EM_M32         = 1,  /* AT&T WE 32100 */
	EM_SPARC       = 2,  /* SUN SPARC */
	EM_386         = 3,  /* Intel 80386 */
	EM_68K         = 4,  /* Motorola m68k family */
	EM_88K         = 5,  /* Motorola m88k family */
	EM_860         = 7,  /* Intel 80860 */
	EM_MIPS        = 8,  /* MIPS R3000 big-endian */
	EM_S370        = 9,  /* IBM System/370 */
	EM_MIPS_RS3_LE = 10, /* MIPS R3000 little-endian */

	EM_PARISC      = 15, /* HPPA */
	EM_VPP500      = 17, /* Fujitsu VPP500 */
	EM_SPARC32PLUS = 18, /* Sun's "v8plus" */
	EM_960         = 19, /* Intel 80960 */
	EM_PPC         = 20, /* PowerPC */
	EM_PPC64       = 21, /* PowerPC 64-bit */
	EM_S390        = 22, /* IBM S390 */

	EM_V800       = 36, /* NEC V800 series */
	EM_FR20       = 37, /* Fujitsu FR20 */
	EM_RH32       = 38, /* TRW RH-32 */
	EM_RCE        = 39, /* Motorola RCE */
	EM_ARM        = 40, /* ARM */
	EM_FAKE_ALPHA = 41, /* Digital Alpha */
	EM_SH         = 42, /* Hitachi SH */
	EM_SPARCV9    = 43, /* SPARC v9 64-bit */
	EM_TRICORE    = 44, /* Siemens Tricore */
	EM_ARC        = 45, /* Argonaut RISC Core */
	EM_H8_300     = 46, /* Hitachi H8/300 */
	EM_H8_300H    = 47, /* Hitachi H8/300H */
	EM_H8S        = 48, /* Hitachi H8S */
	EM_H8_500     = 49, /* Hitachi H8/500 */
	EM_IA_64      = 50, /* Intel Merced */
	EM_MIPS_X     = 51, /* Stanford MIPS-X */
	EM_COLDFIRE   = 52, /* Motorola Coldfire */
	EM_68HC12     = 53, /* Motorola M68HC12 */
	EM_MMA        = 54, /* Fujitsu MMA Multimedia Accelerator*/
	EM_PCP        = 55, /* Siemens PCP */
	EM_NCPU       = 56, /* Sony nCPU embeeded RISC */
	EM_NDR1       = 57, /* Denso NDR1 microprocessor */
	EM_STARCORE   = 58, /* Motorola Start*Core processor */
	EM_ME16       = 59, /* Toyota ME16 processor */
	EM_ST100      = 60, /* STMicroelectronic ST100 processor */
	EM_TINYJ      = 61, /* Advanced Logic Corp. Tinyj emb.fam*/
	EM_X86_64     = 62, /* AMD x86-64 architecture */
	EM_PDSP       = 63, /* Sony DSP Processor */

	EM_FX66     = 66, /* Siemens FX66 microcontroller */
	EM_ST9PLUS  = 67, /* STMicroelectronics ST9+ 8/16 mc */
	EM_ST7      = 68, /* STmicroelectronics ST7 8 bit mc */
	EM_68HC16   = 69, /* Motorola MC68HC16 microcontroller */
	EM_68HC11   = 70, /* Motorola MC68HC11 microcontroller */
	EM_68HC08   = 71, /* Motorola MC68HC08 microcontroller */
	EM_68HC05   = 72, /* Motorola MC68HC05 microcontroller */
	EM_SVX      = 73, /* Silicon Graphics SVx */
	EM_ST19     = 74, /* STMicroelectronics ST19 8 bit mc */
	EM_VAX      = 75, /* Digital VAX */
	EM_CRIS     = 76, /* Axis Communications 32-bit embedded processor */
	EM_JAVELIN  = 77, /* Infineon Technologies 32-bit embedded processor */
	EM_FIREPATH = 78, /* Element 14 64-bit DSP Processor */
	EM_ZSP      = 79, /* LSI Logic 16-bit DSP Processor */
	EM_MMIX     = 80, /* Donald Knuth's educational 64-bit processor */
	EM_HUANY    = 81, /* Harvard University machine-independent object files */
	EM_PRISM    = 82, /* SiTera Prism */
	EM_AVR      = 83, /* Atmel AVR 8-bit microcontroller */
	EM_FR30     = 84, /* Fujitsu FR30 */
	EM_D10V     = 85, /* Mitsubishi D10V */
	EM_D30V     = 86, /* Mitsubishi D30V */
	EM_V850     = 87, /* NEC v850 */
	EM_M32R     = 88, /* Mitsubishi M32R */
	EM_MN10300  = 89, /* Matsushita MN10300 */
	EM_MN10200  = 90, /* Matsushita MN10200 */
	EM_PJ       = 91, /* picoJava */
	EM_OPENRISC = 92, /* OpenRISC 32-bit embedded processor */
	EM_ARC_A5   = 93, /* ARC Cores Tangent-A5 */
	EM_XTENSA   = 94, /* Tensilica Xtensa Architecture */
	EM_NUM      = 95,

	/* If it is necessary to assign new unofficial EM_* values, please
	   pick large random numbers (0x8523, 0xa7f2, etc.) to minimize the
	   chances of collision with official or non-GNU unofficial values.  */
	EM_ALPHA = 0x9026,
};

struct elf32_phdr;
struct elf32_header {
	using elf_phdr = elf32_phdr;
	enum { ELFCLASS = ELFCLASS32 };

	uint8_t e_ident[EI_NIDENT]; /* Magic number and other info */
	elf32_half e_type;          /* Object file type */
	elf32_half e_machine;       /* Architecture */
	elf32_word e_version;       /* Object file version */
	elf32_addr e_entry;         /* Entry point virtual address */
	elf32_off e_phoff;          /* Program header table file offset */
	elf32_off e_shoff;          /* Section header table file offset */
	elf32_word e_flags;         /* Processor-specific flags */
	elf32_half e_ehsize;        /* ELF header size in bytes */
	elf32_half e_phentsize;     /* Program header table entry size */
	elf32_half e_phnum;         /* Program header table entry count */
	elf32_half e_shentsize;     /* Section header table entry size */
	elf32_half e_shnum;         /* Section header table entry count */
	elf32_half e_shstrndx;      /* Section header string table index */
};

struct elf64_phdr;
struct elf64_header {
	using elf_phdr = elf64_phdr;
	enum { ELFCLASS = ELFCLASS64 };

	uint8_t e_ident[EI_NIDENT]; /* Magic number and other info */
	elf64_half e_type;          /* Object file type */
	elf64_half e_machine;       /* Architecture */
	elf64_word e_version;       /* Object file version */
	elf64_addr e_entry;         /* Entry point virtual address */
	elf64_off e_phoff;          /* Program header table file offset */
	elf64_off e_shoff;          /* Section header table file offset */
	elf64_word e_flags;         /* Processor-specific flags */
	elf64_half e_ehsize;        /* ELF header size in bytes */
	elf64_half e_phentsize;     /* Program header table entry size */
	elf64_half e_phnum;         /* Program header table entry count */
	elf64_half e_shentsize;     /* Section header table entry size */
	elf64_half e_shnum;         /* Section header table entry count */
	elf64_half e_shstrndx;      /* Section header string table index */
};

#endif
