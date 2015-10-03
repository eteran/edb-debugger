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

#ifndef ELF_PHDR_20121007_H_
#define ELF_PHDR_20121007_H_

#include "elf/elf_types.h"

/* Program segment header.  */

struct elf32_phdr {
	elf32_word	p_type;			/* Segment type */
	elf32_off	p_offset;		/* Segment file offset */
	elf32_addr	p_vaddr;		/* Segment virtual address */
	elf32_addr	p_paddr;		/* Segment physical address */
	elf32_word	p_filesz;		/* Segment size in file */
	elf32_word	p_memsz;		/* Segment size in memory */
	elf32_word	p_flags;		/* Segment flags */
	elf32_word	p_align;		/* Segment alignment */
};

struct elf64_phdr {
	elf64_word	p_type;			/* Segment type */
	elf64_word	p_flags;		/* Segment flags */
	elf64_off	p_offset;		/* Segment file offset */
	elf64_addr	p_vaddr;		/* Segment virtual address */
	elf64_addr	p_paddr;		/* Segment physical address */
	elf64_xword	p_filesz;		/* Segment size in file */
	elf64_xword	p_memsz;		/* Segment size in memory */
	elf64_xword	p_align;		/* Segment alignment */
};

/* Special value for e_phnum.  This indicates that the real number of
   program headers is too large to fit into e_phnum.  Instead the real
   value is in the field sh_info of section 0.  */

#define PN_XNUM		0xffff

/* Legal values for p_type (segment type).  */

#define	PT_NULL		0		/* Program header table entry unused */
#define PT_LOAD		1		/* Loadable program segment */
#define PT_DYNAMIC	2		/* Dynamic linking information */
#define PT_INTERP	3		/* Program interpreter */
#define PT_NOTE		4		/* Auxiliary information */
#define PT_SHLIB	5		/* Reserved */
#define PT_PHDR		6		/* Entry for header table itself */
#define PT_TLS		7		/* Thread-local storage segment */
#define	PT_NUM		8		/* Number of defined types */
#define PT_LOOS		0x60000000	/* Start of OS-specific */
#define PT_GNU_EH_FRAME	0x6474e550	/* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK	0x6474e551	/* Indicates stack executability */
#define PT_GNU_RELRO	0x6474e552	/* Read-only after relocation */
#define PT_PAX_FLAGS	0x65041580	/* Indicates PaX flag markings */
#define PT_LOSUNW	0x6ffffffa
#define PT_SUNWBSS	0x6ffffffa	/* Sun Specific segment */
#define PT_SUNWSTACK	0x6ffffffb	/* Stack segment */
#define PT_HISUNW	0x6fffffff
#define PT_HIOS		0x6fffffff	/* End of OS-specific */
#define PT_LOPROC	0x70000000	/* Start of processor-specific */
#define PT_HIPROC	0x7fffffff	/* End of processor-specific */

/* Legal values for p_flags (segment flags).  */

#define PF_X		(1 << 0)	/* Segment is executable */
#define PF_W		(1 << 1)	/* Segment is writable */
#define PF_R		(1 << 2)	/* Segment is readable */
#define PF_PAGEEXEC	(1 << 4)	/* Enable  PAGEEXEC */
#define PF_NOPAGEEXEC	(1 << 5)	/* Disable PAGEEXEC */
#define PF_SEGMEXEC	(1 << 6)	/* Enable  SEGMEXEC */
#define PF_NOSEGMEXEC	(1 << 7)	/* Disable SEGMEXEC */
#define PF_MPROTECT	(1 << 8)	/* Enable  MPROTECT */
#define PF_NOMPROTECT	(1 << 9)	/* Disable MPROTECT */
#define PF_RANDEXEC	(1 << 10)	/* Enable  RANDEXEC */
#define PF_NORANDEXEC	(1 << 11)	/* Disable RANDEXEC */
#define PF_EMUTRAMP	(1 << 12)	/* Enable  EMUTRAMP */
#define PF_NOEMUTRAMP	(1 << 13)	/* Disable EMUTRAMP */
#define PF_RANDMMAP	(1 << 14)	/* Enable  RANDMMAP */
#define PF_NORANDMMAP	(1 << 15)	/* Disable RANDMMAP */
#define PF_MASKOS	0x0ff00000	/* OS-specific */
#define PF_MASKPROC	0xf0000000	/* Processor-specific */

/* Legal values for note segment descriptor types for core files. */

#define NT_PRSTATUS	1		/* Contains copy of prstatus struct */
#define NT_FPREGSET	2		/* Contains copy of fpregset struct */
#define NT_PRPSINFO	3		/* Contains copy of prpsinfo struct */
#define NT_PRXREG	4		/* Contains copy of prxregset struct */
#define NT_TASKSTRUCT	4		/* Contains copy of task structure */
#define NT_PLATFORM	5		/* String from sysinfo(SI_PLATFORM) */
#define NT_AUXV		6		/* Contains copy of auxv array */
#define NT_GWINDOWS	7		/* Contains copy of gwindows struct */
#define NT_ASRS		8		/* Contains copy of asrset struct */
#define NT_PSTATUS	10		/* Contains copy of pstatus struct */
#define NT_PSINFO	13		/* Contains copy of psinfo struct */
#define NT_PRCRED	14		/* Contains copy of prcred struct */
#define NT_UTSNAME	15		/* Contains copy of utsname struct */
#define NT_LWPSTATUS	16		/* Contains copy of lwpstatus struct */
#define NT_LWPSINFO	17		/* Contains copy of lwpinfo struct */
#define NT_PRFPXREG	20		/* Contains copy of fprxregset struct */
#define NT_PRXFPREG	0x46e62b7f	/* Contains copy of user_fxsr_struct */
#define NT_PPC_VMX	0x100		/* PowerPC Altivec/VMX registers */
#define NT_PPC_SPE	0x101		/* PowerPC SPE/EVR registers */
#define NT_PPC_VSX	0x102		/* PowerPC VSX registers */
#define NT_386_TLS	0x200		/* i386 TLS slots (struct user_desc) */
#define NT_386_IOPERM	0x201		/* x86 io permission bitmap (1=deny) */
#define NT_X86_XSTATE	0x202		/* x86 extended state using xsave */

/* Legal values for the note segment descriptor types for object files.  */

#define NT_VERSION	1		/* Contains a version string.  */


#endif
