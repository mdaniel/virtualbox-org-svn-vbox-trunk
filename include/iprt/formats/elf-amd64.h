/*-
 * Copyright (c) 1996-1997 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef IPRT_INCLUDED_formats_elf_amd64_h
#define IPRT_INCLUDED_formats_elf_amd64_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/*
 * ELF definitions for the AMD64 architecture.
 */

#if 0 /* later */
/*
 * Auxiliary vector entries for passing information to the interpreter.
 *
 * The i386 supplement to the SVR4 ABI specification names this "auxv_t",
 * but POSIX lays claim to all symbols ending with "_t".
 */
typedef struct {        /* Auxiliary vector entry on initial stack */
        int     a_type;                 /* Entry type. */
        union {
                int     a_val;          /* Integer value. */
        } a_un;
} Elf32_Auxinfo;


typedef struct {        /* Auxiliary vector entry on initial stack */
        long    a_type;                 /* Entry type. */
        union {
                long    a_val;          /* Integer value. */
                void    *a_ptr;         /* Address. */
                void    (*a_fcn)(void); /* Function pointer (not used). */
        } a_un;
} Elf64_Auxinfo;

__ElfType(Auxinfo);

/* Values for a_type. */
#define AT_NULL         0       /* Terminates the vector. */
#define AT_IGNORE       1       /* Ignored entry. */
#define AT_EXECFD       2       /* File descriptor of program to load. */
#define AT_PHDR         3       /* Program header of program already loaded. */
#define AT_PHENT        4       /* Size of each program header entry. */
#define AT_PHNUM        5       /* Number of program header entries. */
#define AT_PAGESZ       6       /* Page size in bytes. */
#define AT_BASE         7       /* Interpreter's base address. */
#define AT_FLAGS        8       /* Flags (unused for i386). */
#define AT_ENTRY        9       /* Where interpreter should transfer control. */

/*
 * The following non-standard values are used for passing information
 * from John Polstra's testbed program to the dynamic linker.  These
 * are expected to go away soon.
 *
 * Unfortunately, these overlap the Linux non-standard values, so they
 * must not be used in the same context.
 */
#define AT_BRK          10      /* Starting point for sbrk and brk. */
#define AT_DEBUG        11      /* Debugging level. */

/*
 * The following non-standard values are used in Linux ELF binaries.
 */
#define AT_NOTELF       10      /* Program is not ELF ?? */
#define AT_UID          11      /* Real uid. */
#define AT_EUID         12      /* Effective uid. */
#define AT_GID          13      /* Real gid. */
#define AT_EGID         14      /* Effective gid. */

#define AT_COUNT        15      /* Count of defined aux entry types. */

#endif /* later */

/*
 * Relocation types.
 */

#define R_X86_64_NONE   0       /* No relocation. */
#define R_X86_64_64     1       /* Add 64 bit symbol value. */
#define R_X86_64_PC32   2       /* PC-relative 32 bit signed sym value. */
#define R_X86_64_GOT32  3       /* PC-relative 32 bit GOT offset. */
#define R_X86_64_PLT32  4       /* PC-relative 32 bit PLT offset. */
#define R_X86_64_COPY   5       /* Copy data from shared object. */
#define R_X86_64_GLOB_DAT 6     /* Set GOT entry to data address. */
#define R_X86_64_JMP_SLOT 7     /* Set GOT entry to code address. */
#define R_X86_64_RELATIVE 8     /* Add load address of shared object. */
#define R_X86_64_GOTPCREL 9     /* Add 32 bit signed pcrel offset to GOT. */
#define R_X86_64_32     10      /* Add 32 bit zero extended symbol value */
#define R_X86_64_32S    11      /* Add 32 bit sign extended symbol value */
#define R_X86_64_16     12      /* Add 16 bit zero extended symbol value */
#define R_X86_64_PC16   13      /* Add 16 bit signed extended pc relative symbol value */
#define R_X86_64_8      14      /* Add 8 bit zero extended symbol value */
#define R_X86_64_PC8    15      /* Add 8 bit signed extended pc relative symbol value */
#define R_X86_64_DTPMOD64 16    /* ID of module containing symbol */
#define R_X86_64_DTPOFF64 17    /* Offset in TLS block */
#define R_X86_64_TPOFF64 18     /* Offset in static TLS block */
#define R_X86_64_TLSGD  19      /* PC relative offset to GD GOT entry */
#define R_X86_64_TLSLD  20      /* PC relative offset to LD GOT entry */
#define R_X86_64_DTPOFF32 21    /* Offset in TLS block */
#define R_X86_64_GOTTPOFF 22    /* PC relative offset to IE GOT entry */
#define R_X86_64_TPOFF32 23     /* Offset in static TLS block */

#define R_X86_64_COUNT  24      /* Count of defined relocation types. */

#endif /* !IPRT_INCLUDED_formats_elf_amd64_h */

