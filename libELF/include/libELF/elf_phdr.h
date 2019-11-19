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

#ifndef ELF_PHDR_H_20121007_
#define ELF_PHDR_H_20121007_

#include "elf_types.h"

/* Program segment header.  */
struct elf32_phdr {
	elf32_word p_type;   /* Segment type */
	elf32_off p_offset;  /* Segment file offset */
	elf32_addr p_vaddr;  /* Segment virtual address */
	elf32_addr p_paddr;  /* Segment physical address */
	elf32_word p_filesz; /* Segment size in file */
	elf32_word p_memsz;  /* Segment size in memory */
	elf32_word p_flags;  /* Segment flags */
	elf32_word p_align;  /* Segment alignment */
};

struct elf64_phdr {
	elf64_word p_type;    /* Segment type */
	elf64_word p_flags;   /* Segment flags */
	elf64_off p_offset;   /* Segment file offset */
	elf64_addr p_vaddr;   /* Segment virtual address */
	elf64_addr p_paddr;   /* Segment physical address */
	elf64_xword p_filesz; /* Segment size in file */
	elf64_xword p_memsz;  /* Segment size in memory */
	elf64_xword p_align;  /* Segment alignment */
};

/* Special value for e_phnum.  This indicates that the real number of
   program headers is too large to fit into e_phnum.  Instead the real
   value is in the field sh_info of section 0.  */
enum {
	PN_XNUM = 0xffff,
};

/* Legal values for p_type (segment type).  */
enum {
	PT_NULL         = 0,          /* Program header table entry unused */
	PT_LOAD         = 1,          /* Loadable program segment */
	PT_DYNAMIC      = 2,          /* Dynamic linking information */
	PT_INTERP       = 3,          /* Program interpreter */
	PT_NOTE         = 4,          /* Auxiliary information */
	PT_SHLIB        = 5,          /* Reserved */
	PT_PHDR         = 6,          /* Entry for header table itself */
	PT_TLS          = 7,          /* Thread-local storage segment */
	PT_NUM          = 8,          /* Number of defined types */
	PT_LOOS         = 0x60000000, /* Start of OS-specific */
	PT_GNU_EH_FRAME = 0x6474e550, /* GCC .eh_frame_hdr segment */
	PT_GNU_STACK    = 0x6474e551, /* Indicates stack executability */
	PT_GNU_RELRO    = 0x6474e552, /* Read-only after relocation */
	PT_PAX_FLAGS    = 0x65041580, /* Indicates PaX flag markings */
	PT_LOSUNW       = 0x6ffffffa,
	PT_SUNWBSS      = 0x6ffffffa, /* Sun Specific segment */
	PT_SUNWSTACK    = 0x6ffffffb, /* Stack segment */
	PT_HISUNW       = 0x6fffffff,
	PT_HIOS         = 0x6fffffff, /* End of OS-specific */
	PT_LOPROC       = 0x70000000, /* Start of processor-specific */
	PT_HIPROC       = 0x7fffffff, /* End of processor-specific */
};

/* Legal values for p_flags (segment flags).  */
enum {
	PF_X          = (1 << 0),   /* Segment is executable */
	PF_W          = (1 << 1),   /* Segment is writable */
	PF_R          = (1 << 2),   /* Segment is readable */
	PF_PAGEEXEC   = (1 << 4),   /* Enable  PAGEEXEC */
	PF_NOPAGEEXEC = (1 << 5),   /* Disable PAGEEXEC */
	PF_SEGMEXEC   = (1 << 6),   /* Enable  SEGMEXEC */
	PF_NOSEGMEXEC = (1 << 7),   /* Disable SEGMEXEC */
	PF_MPROTECT   = (1 << 8),   /* Enable  MPROTECT */
	PF_NOMPROTECT = (1 << 9),   /* Disable MPROTECT */
	PF_RANDEXEC   = (1 << 10),  /* Enable  RANDEXEC */
	PF_NORANDEXEC = (1 << 11),  /* Disable RANDEXEC */
	PF_EMUTRAMP   = (1 << 12),  /* Enable  EMUTRAMP */
	PF_NOEMUTRAMP = (1 << 13),  /* Disable EMUTRAMP */
	PF_RANDMMAP   = (1 << 14),  /* Enable  RANDMMAP */
	PF_NORANDMMAP = (1 << 15),  /* Disable RANDMMAP */
	PF_MASKOS     = 0x0ff00000, /* OS-specific */
	PF_MASKPROC   = 0xf0000000, /* Processor-specific */
};

/* Legal values for note segment descriptor types for core files. */
enum {
	NT_PRSTATUS   = 1,          /* Contains copy of prstatus struct */
	NT_FPREGSET   = 2,          /* Contains copy of fpregset struct */
	NT_PRPSINFO   = 3,          /* Contains copy of prpsinfo struct */
	NT_PRXREG     = 4,          /* Contains copy of prxregset struct */
	NT_TASKSTRUCT = 4,          /* Contains copy of task structure */
	NT_PLATFORM   = 5,          /* String from sysinfo(SI_PLATFORM) */
	NT_AUXV       = 6,          /* Contains copy of auxv array */
	NT_GWINDOWS   = 7,          /* Contains copy of gwindows struct */
	NT_ASRS       = 8,          /* Contains copy of asrset struct */
	NT_PSTATUS    = 10,         /* Contains copy of pstatus struct */
	NT_PSINFO     = 13,         /* Contains copy of psinfo struct */
	NT_PRCRED     = 14,         /* Contains copy of prcred struct */
	NT_UTSNAME    = 15,         /* Contains copy of utsname struct */
	NT_LWPSTATUS  = 16,         /* Contains copy of lwpstatus struct */
	NT_LWPSINFO   = 17,         /* Contains copy of lwpinfo struct */
	NT_PRFPXREG   = 20,         /* Contains copy of fprxregset struct */
	NT_PRXFPREG   = 0x46e62b7f, /* Contains copy of user_fxsr_struct */
	NT_PPC_VMX    = 0x100,      /* PowerPC Altivec/VMX registers */
	NT_PPC_SPE    = 0x101,      /* PowerPC SPE/EVR registers */
	NT_PPC_VSX    = 0x102,      /* PowerPC VSX registers */
	NT_386_TLS    = 0x200,      /* i386 TLS slots (struct user_desc) */
	NT_386_IOPERM = 0x201,      /* x86 io permission bitmap (1=deny) */
	NT_X86_XSTATE = 0x202,      /* x86 extended state using xsave */
};

/* Legal values for the note segment descriptor types for object files.  */
enum {
	NT_VERSION = 1, /* Contains a version string.  */
};

#endif
