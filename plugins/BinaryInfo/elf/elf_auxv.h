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

#ifndef ELF_AUXV_20121007_H_
#define ELF_AUXV_20121007_H_

#include "elf/elf_types.h"

/* Auxiliary vector.  */

/* This vector is normally only used by the program interpreter.  The
   usual definition in an ABI supplement uses the name auxv_t.  The
   vector is not usually defined in a standard <elf.h> file, but it
   can't hurt.  We rename it to avoid conflicts.  The sizes of these
   types are an arrangement between the exec server and the program
   interpreter, so we don't fully specify them here.  */

struct elf32_auxv_t {
	uint32_t a_type;    /* Entry type */
	union {
		uint32_t a_val; /* Integer value */
		/*
		We use to have pointer elements added here.  We cannot do that,
		though, since it does not work when using 32-bit definitions
		on 64-bit platforms and vice versa.
		*/
	} a_un;
};

struct elf64_auxv_t {
	uint64_t a_type;    /* Entry type */
	union {
		uint64_t a_val; /* Integer value */
		/*
		We use to have pointer elements added here.  We cannot do that,
		though, since it does not work when using 32-bit definitions
		on 64-bit platforms and vice versa.
		*/
	} a_un;
};

// Legal values for a_type (entry type).  
enum {
	AT_NULL           = 0,  // End of vector 
	AT_IGNORE         = 1,  // Entry should be ignored 
	AT_EXECFD         = 2,  // File descriptor of program 
	AT_PHDR           = 3,  // Program headers for program 
	AT_PHENT          = 4,  // Size of program header entry 
	AT_PHNUM          = 5,  // Number of program headers 
	AT_PAGESZ         = 6,  // System page size 
	AT_BASE           = 7,  // Base address of interpreter 
	AT_FLAGS          = 8,  // Flags 
	AT_ENTRY          = 9,  // Entry point of program 
	AT_NOTELF         = 10, // Program is not ELF 
	AT_UID            = 11, // Real uid 
	AT_EUID           = 12, // Effective uid 
	AT_GID            = 13, // Real gid 
	AT_EGID           = 14, // Effective gid 
	AT_CLKTCK         = 17, // Frequency of times() 

	// Some more special a_type values describing the hardware.  
	AT_PLATFORM       = 15, // String identifying platform.  
	AT_HWCAP          = 16, // Machine dependent hints about processor capabilities.

	// This entry gives some information about the FPU initialization
	// performed by the kernel.
	AT_FPUCW          = 18, // Used FPU control word.  

	// Cache block sizes.  
	AT_DCACHEBSIZE    = 19, // Data cache block size.  
	AT_ICACHEBSIZE    = 20, // Instruction cache block size.  
	AT_UCACHEBSIZE    = 21, // Unified cache block size.  

	// A special ignored value for PPC, used by the kernel to control the
	// interpretation of the AUXV. Must be > 16.
	AT_IGNOREPPC     = 22, // Entry should be ignored.  
	AT_SECURE        = 23, // Boolean, was exec setuid-like?  
	AT_BASE_PLATFORM = 24, // String identifying real platforms.
	AT_RANDOM        = 25, // Address of 16 random bytes.  
	AT_EXECFN        = 31, // Filename of executable.  

	// Pointer to the global system page used for system calls and other
	// nice things.
	AT_SYSINFO        = 32,
	AT_SYSINFO_EHDR   = 33,

	// Shapes of the caches.  Bits 0-3 contains associativity; bits 4-7 contains
	// log2 of line size; mask those to get cache size.
	AT_L1I_CACHESHAPE = 34,
	AT_L1D_CACHESHAPE = 35,
	AT_L2_CACHESHAPE  = 36,
	AT_L3_CACHESHAPE  = 37
};

#endif
