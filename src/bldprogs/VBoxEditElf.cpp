/* $Id$ */
/** @file
 * VBoxEditElf - Simple ELF binary file editor.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/types.h>
#include <iprt/formats/elf64.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

typedef struct
{
    Elf64_Half      vd_version;
    Elf64_Half      vd_flags;
    Elf64_Half      vd_ndx;
    Elf64_Half      vd_cnt;
    Elf64_Word      vd_hash;
    Elf64_Word      vd_aux;
    Elf64_Word      vd_next;
} Elf64_Verdef;


typedef struct
{
    Elf64_Word      vda_name;
    Elf64_Word      vda_next;
} Elf64_Verdaux;


#define SHT_GNU_versym   UINT32_C(0x6fffffff)
#define SHT_GNU_verdef   UINT32_C(0x6ffffffd)
#define SHT_GNU_verneed  UINT32_C(0x6ffffffe)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name Options
 * @{ */
static enum
{
    kVBoxEditElfAction_Nothing,
    kVBoxEditElfAction_DeleteRunpath,
    kVBoxEditElfAction_ChangeRunpath,
    kVBoxEditElfAction_CreateLinkerStub
}                 g_enmAction = kVBoxEditElfAction_Nothing;
static const char *g_pszInput = NULL;
/** Verbosity level. */
static int        g_cVerbosity = 0;
/** New runpath. */
static const char *g_pszRunpath = NULL;
/** The output path for the stub library. */
static const char *g_pszLinkerStub = NULL;
/** @} */

static const char s_achShStrTab[] = "\0.shstrtab\0.dynsym\0.dynstr\0.gnu.version\0.gnu.version_d";


static void verbose(const char *pszFmt, ...)
{
    if (g_cVerbosity == 0)
        return;

    va_list Args;
    va_start(Args, pszFmt);
    RTMsgInfoV(pszFmt, Args);
    va_end(Args);
}


static RTEXITCODE deleteRunpath(const char *pszInput)
{
    RT_NOREF(pszInput);

    RTFILE hFileElf = NIL_RTFILE;
    int rc = RTFileOpen(&hFileElf, pszInput, RTFILE_O_OPEN | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Filed to open file '%s': %Rrc\n", pszInput, rc);

    /* Only support for 64-bit ELF files currently. */
    Elf64_Ehdr Hdr;
    rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
    {
        RTFileClose(hFileElf);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);
    }

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFCLASS64)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf64_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf64_Ehdr));
    if (    sizeof(Elf64_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf64_Phdr));
    if (sizeof(Elf64_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf64_Shdr));

    /* Find dynamic section. */
    Elf64_Phdr Phdr; RT_ZERO(Phdr);
    bool fFound = false;
    for (uint32_t i = 0; i < Hdr.e_phnum; i++)
    {
        rc = RTFileReadAt(hFileElf, Hdr.e_phoff + i * sizeof(Phdr), &Phdr, sizeof(Phdr), NULL);
        if (RT_FAILURE(rc))
        {
            RTFileClose(hFileElf);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);
        }
        if (Phdr.p_type == PT_DYNAMIC)
        {
            if (!Phdr.p_filesz)
            {
                RTFileClose(hFileElf);
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Dynmic section in '%s' is empty\n", pszInput);
            }
            fFound = true;
            break;
        }
    }

    if (!fFound)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' doesn't contain dynamic section\n", pszInput);

    Elf64_Dyn *paDynSh = (Elf64_Dyn *)RTMemAllocZ(Phdr.p_filesz);
    if (!paDynSh)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for dynamic section of '%s'\n", Phdr.p_filesz, pszInput);

    rc = RTFileReadAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);

    /* Remove all DT_RUNPATH entries and padd the remainder with DT_NULL. */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < Phdr.p_filesz / sizeof(Elf64_Dyn); i++)
    {
        paDynSh[idx] = paDynSh[i];
        if (paDynSh[i].d_tag != DT_RPATH && paDynSh[i].d_tag != DT_RUNPATH)
            idx++;
    }

    while (idx < Phdr.p_filesz / sizeof(Elf64_Dyn))
    {
        paDynSh[idx].d_tag = DT_NULL;
        paDynSh[idx].d_un.d_val = 0;
        idx++;
    }

    /* Write the result. */
    rc = RTFileWriteAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write updated ELF dynamic section for '%s': %Rrc\n", pszInput, rc);

    RTMemFree(paDynSh);
    RTFileClose(hFileElf);
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE changeRunpathEntry(RTFILE hFileElf, const char *pszInput, Elf64_Ehdr *pHdr, Elf64_Xword offInStrTab, const char *pszRunpath)
{
    /* Read section headers to find the string table. */
    size_t const cbShdrs = pHdr->e_shnum * sizeof(Elf64_Shdr);
    Elf64_Shdr *paShdrs = (Elf64_Shdr *)RTMemAlloc(cbShdrs);
    if (!paShdrs)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for section headers of '%s'\n", cbShdrs, pszInput);

    int rc = RTFileReadAt(hFileElf, pHdr->e_shoff, paShdrs, cbShdrs, NULL);
    if (RT_FAILURE(rc))
    {
        RTMemFree(paShdrs);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read %zu bytes of section headers from '%s': %Rrc\n", cbShdrs, pszInput, rc);
    }

    uint32_t idx;
    for (idx = 0; idx < pHdr->e_shnum; idx++)
    {
        if (paShdrs[idx].sh_type == SHT_STRTAB)
            break;
    }

    if (idx == pHdr->e_shnum)
    {
        RTMemFree(paShdrs);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' does not contain a string table\n", pszInput);
    }

    size_t const cbStrTab  = paShdrs[idx].sh_size;
    RTFOFF const offStrTab = paShdrs[idx].sh_offset;
    RTMemFree(paShdrs);

    if (offInStrTab >= cbStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table offset of runpath entry is out of bounds: got %#RX64, maximum is %zu\n", offInStrTab, cbStrTab - 1);

    /* Read the string table. */
    char *pbStrTab = (char *)RTMemAllocZ(cbStrTab + 1); /* Force a zero terminator. */
    if (!pbStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for string table of '%s'\n", cbStrTab + 1, pszInput);

    rc = RTFileReadAt(hFileElf, offStrTab, pbStrTab, cbStrTab, NULL);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pbStrTab);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read %zu bytes of the string table from '%s': %Rrc\n", cbStrTab, pszInput, rc);
    }

    /* Calculate the maximum number of characters we can replace. */
    char *pbStr = &pbStrTab[offInStrTab];
    size_t cchMax = strlen(pbStr);
    while (   &pbStr[cchMax + 1] < &pbStrTab[cbStrTab]
           && pbStr[cchMax] == '\0')
        cchMax++;

    size_t const cchNewRunpath = strlen(pszRunpath);
    if (cchMax < cchNewRunpath)
    {
        RTMemFree(pbStrTab);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "New runpath '%s' is too long to overwrite current one, maximum length is: %zu\n", cchNewRunpath, cchMax);
    }

    memcpy(&pbStr[cchMax], pszRunpath, cchNewRunpath);
    rc = RTFileReadAt(hFileElf, offStrTab, pbStrTab, cbStrTab, NULL);
    RTMemFree(pbStrTab);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Writing altered string table failed: %Rrc\n", rc);

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE changeRunpath(const char *pszInput, const char *pszRunpath)
{
    RTFILE hFileElf = NIL_RTFILE;
    int rc = RTFileOpen(&hFileElf, pszInput, RTFILE_O_OPEN | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Filed to open file '%s': %Rrc\n", pszInput, rc);

    /* Only support for 64-bit ELF files currently. */
    Elf64_Ehdr Hdr;
    rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
    {
        RTFileClose(hFileElf);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);
    }

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFCLASS64)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf64_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf64_Ehdr));
    if (    sizeof(Elf64_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf64_Phdr));
    if (sizeof(Elf64_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf64_Shdr));

    /* Find dynamic section. */
    Elf64_Phdr Phdr; RT_ZERO(Phdr);
    bool fFound = false;
    for (uint32_t i = 0; i < Hdr.e_phnum; i++)
    {
        rc = RTFileReadAt(hFileElf, Hdr.e_phoff + i * sizeof(Phdr), &Phdr, sizeof(Phdr), NULL);
        if (RT_FAILURE(rc))
        {
            RTFileClose(hFileElf);
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);
        }
        if (Phdr.p_type == PT_DYNAMIC)
        {
            if (!Phdr.p_filesz)
            {
                RTFileClose(hFileElf);
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Dynmic section in '%s' is empty\n", pszInput);
            }
            fFound = true;
            break;
        }
    }

    if (!fFound)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF binary '%s' doesn't contain dynamic section\n", pszInput);

    Elf64_Dyn *paDynSh = (Elf64_Dyn *)RTMemAllocZ(Phdr.p_filesz);
    if (!paDynSh)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes of memory for dynamic section of '%s'\n", Phdr.p_filesz, pszInput);

    rc = RTFileReadAt(hFileElf, Phdr.p_offset, paDynSh, Phdr.p_filesz, NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF program header header from '%s': %Rrc\n", pszInput, rc);

    /* Look for the first DT_RUNPATH entry and rewrite it. */
    for (uint32_t i = 0; i < Phdr.p_filesz / sizeof(Elf64_Dyn); i++)
    {
        if (   paDynSh[i].d_tag == DT_RPATH
            || paDynSh[i].d_tag == DT_RUNPATH)
        {
            RTEXITCODE rcExit = changeRunpathEntry(hFileElf, pszInput, &Hdr, paDynSh[i].d_un.d_val, pszRunpath);
            RTMemFree(paDynSh);
            RTFileClose(hFileElf);
            return rcExit;
        }
    }

    RTMemFree(paDynSh);
    RTFileClose(hFileElf);
    return RTMsgErrorExit(RTEXITCODE_FAILURE, "No DT_RPATH or DT_RUNPATH entry found in '%s'\n", pszInput);
}


static RTEXITCODE createLinkerStubFrom(const char *pszInput, const char *pszStubPath)
{
    RTFILE hFileElf = NIL_RTFILE;
    int rc = RTFileOpen(&hFileElf, pszInput, RTFILE_O_OPEN | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Filed to open file '%s': %Rrc\n", pszInput, rc);

    /* Only support for 64-bit ELF files currently. */
    Elf64_Ehdr Hdr;
    rc = RTFileReadAt(hFileElf, 0, &Hdr, sizeof(Hdr), NULL);
    if (RT_FAILURE(rc))
    {
        RTFileClose(hFileElf);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read ELF header from '%s': %Rrc\n", pszInput, rc);
    }

    if (    Hdr.e_ident[EI_MAG0] != ELFMAG0
        ||  Hdr.e_ident[EI_MAG1] != ELFMAG1
        ||  Hdr.e_ident[EI_MAG2] != ELFMAG2
        ||  Hdr.e_ident[EI_MAG3] != ELFMAG3)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF magic (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_CLASS] != ELFCLASS64)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Invalid ELF class (%.*Rhxs)", sizeof(Hdr.e_ident), Hdr.e_ident);
    if (Hdr.e_ident[EI_DATA] != ELFDATA2LSB)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF endian %x is unsupported", Hdr.e_ident[EI_DATA]);
    if (Hdr.e_version != EV_CURRENT)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "ELF version %x is unsupported", Hdr.e_version);

    if (sizeof(Elf64_Ehdr) != Hdr.e_ehsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_ehsize is %d expected %d!", Hdr.e_ehsize, sizeof(Elf64_Ehdr));
    if (    sizeof(Elf64_Phdr) != Hdr.e_phentsize
        &&  (   Hdr.e_phnum != 0
             || Hdr.e_type == ET_DYN
             || Hdr.e_type == ET_EXEC))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_phentsize is %d expected %d!", Hdr.e_phentsize, sizeof(Elf64_Phdr));
    if (sizeof(Elf64_Shdr) != Hdr.e_shentsize)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Elf header e_shentsize is %d expected %d!", Hdr.e_shentsize, sizeof(Elf64_Shdr));

    /* Find all the dynamic sections we need. */
    uint32_t idStrTab = UINT32_MAX;
    char *pachStrTab = NULL; size_t cbStrTab = 0;
    Elf64_Sym *paDynSyms = NULL; size_t cbDynSyms = 0;
    uint16_t *pu16GnuVerSym = NULL; size_t cbGnuVerSym = 0;
    uint8_t *pbGnuVerDef = NULL; size_t cbGnuVerDef = 0;

    for (uint32_t i = 0; i < Hdr.e_shnum; i++)
    {
        Elf64_Shdr Shdr; RT_ZERO(Shdr);
        rc = RTFileReadAt(hFileElf, Hdr.e_shoff + i * sizeof(Shdr), &Shdr, sizeof(Shdr), NULL /*pcbRead*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read section header at %RX64 from '%s'\n", Hdr.e_shoff + i * sizeof(Shdr), pszInput);

        verbose("Section header %u:\n"
                "    sh_name:      %RU32\n"
                "    sh_type:      %RX32\n"
                "    sh_flags:     %#RX64\n"
                "    sh_addr:      %#RX64\n"
                "    sh_offset:    %RU64\n"
                "    sh_size:      %RU64\n"
                "    sh_link:      %RU16\n"
                "    sh_info:      %RU16\n"
                "    sh_addralign: %#RX64\n"
                "    sh_entsize:   %#RX64\n", i,
                Shdr.sh_name, Shdr.sh_type, Shdr.sh_flags,
                Shdr.sh_addr, Shdr.sh_offset, Shdr.sh_size,
                Shdr.sh_link, Shdr.sh_info, Shdr.sh_addralign,
                Shdr.sh_entsize);

        switch (Shdr.sh_type)
        {
            case SHT_DYNSYM:
            {
                cbDynSyms = Shdr.sh_size;
                paDynSyms = (Elf64_Sym *)RTMemAllocZ(cbDynSyms);
                if (!paDynSyms)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .dynsym section in '%s'\n", cbDynSyms, pszInput);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, paDynSyms, cbDynSyms, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64 from '%s'\n", Shdr.sh_offset, pszInput);

                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u in '%s'\n", Shdr.sh_link, idStrTab, pszInput);
                break;
            }
            case SHT_GNU_versym:
            {
                cbGnuVerSym = Shdr.sh_size;
                pu16GnuVerSym = (uint16_t *)RTMemAllocZ(cbGnuVerSym);
                if (!pu16GnuVerSym)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version section in '%s'\n", cbGnuVerSym, pszInput);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pu16GnuVerSym, cbGnuVerSym, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64 from '%s'\n", Shdr.sh_offset, pszInput);

                /** @todo It should link to the .dynsym table. */
                break;
            }
            case SHT_GNU_verdef:
            {
                cbGnuVerDef = Shdr.sh_size;
                pbGnuVerDef = (uint8_t *)RTMemAllocZ(cbGnuVerDef);
                if (!pbGnuVerDef)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .gnu.version section in '%s'\n", cbGnuVerDef, pszInput);

                rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pbGnuVerDef, cbGnuVerDef, NULL /*pcbRead*/);
                if (RT_FAILURE(rc))
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .dynsym section at %RX64 from '%s'\n", Shdr.sh_offset, pszInput);

                /* It should link to the string table. */
                if (idStrTab == UINT32_MAX)
                    idStrTab = Shdr.sh_link;
                else if (idStrTab != Shdr.sh_link)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index %u doesn't match %u in '%s'\n", Shdr.sh_link, idStrTab, pszInput);
                break;
            }
            default:
                break; /* Ignored. */
        }
    }

    if (idStrTab == UINT32_MAX)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "String table index not found in '%s'\n", pszInput);

    if (pbGnuVerDef && !pu16GnuVerSym)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Versioned library '%s' misses .gnu.version\n", pszInput);

    /* Read the string table section header. */
    Elf64_Shdr Shdr; RT_ZERO(Shdr);
    rc = RTFileReadAt(hFileElf, Hdr.e_shoff + idStrTab * sizeof(Shdr), &Shdr, sizeof(Shdr), NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .strtab section header at %RX64 from '%s': %Rrc\n",
                              Hdr.e_shoff + idStrTab * sizeof(Shdr), pszInput, rc);

    cbStrTab = Shdr.sh_size;
    pachStrTab = (char *)RTMemAllocZ(cbStrTab + 1); /* Force termination */
    if (!pachStrTab)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to allocate %zu bytes for .strtab section in '%s'\n", cbStrTab, pszInput);

    rc = RTFileReadAt(hFileElf, Shdr.sh_offset, pachStrTab, cbStrTab, NULL /*pcbRead*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to read .strtab section at %RX64 from '%s'\n", Shdr.sh_offset, pszInput);

    RTFileClose(hFileElf);

    verbose("Processing symbol table\n");

    /* Remove all undefined entries from .dynsym and rewrite all exposed symbols to point to the first section header in the stub. */
    uint32_t cDynSymsExport = 0;
    for (uint32_t i = 0; i < cbDynSyms / sizeof(*paDynSyms); i++)
    {
        if (paDynSyms[i].st_shndx)
        {
            paDynSyms[cDynSymsExport] = paDynSyms[i];
            paDynSyms[cDynSymsExport].st_shndx = 1;
            paDynSyms[cDynSymsExport].st_value = 0;
            if (pu16GnuVerSym)
                pu16GnuVerSym[cDynSymsExport] = pu16GnuVerSym[i];

            cDynSymsExport++;
        }
        else
            verbose("Nuked symbol entry %u\n", i);
    }

    /* Figure out the number of .gnu.version entries. */
    uint32_t cVerdefEntries = 0;
    if (pbGnuVerDef)
    {
        cVerdefEntries = 1;
        Elf64_Verdef *pVerDef = (Elf64_Verdef *)pbGnuVerDef;
        for (;;)
        {
            if (!pVerDef->vd_next)
                break;

            cVerdefEntries++;
            pVerDef = (Elf64_Verdef *)((uint8_t *)pVerDef + pVerDef->vd_next);
            if ((uintptr_t)pVerDef >= (uintptr_t)pbGnuVerDef + cbGnuVerDef)
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Version definition next entry points outside .gnu.version section '%s': %Rrc\n", pszInput);
        }
    }

    /* Start writing the output ELF file. */

    verbose("Writing stub binary\n");

    RTFILE hFileOut = NIL_RTFILE;
    rc = RTFileOpen(&hFileOut, pszStubPath, RTFILE_O_CREATE_REPLACE | RTFILE_O_READWRITE | RTFILE_O_DENY_NONE);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create file '%s': %Rrc\n", pszStubPath, rc);

    /* Rewrite the header. */
    Hdr.e_phoff = 0;
    Hdr.e_shoff = sizeof(Hdr);
    Hdr.e_phnum = 0;
    Hdr.e_shnum = pbGnuVerDef ? 6 : 4; /* NULL + .dynsym + .dynstr + (.gnu.version + gnu.version_d) + .shstrtab */
    Hdr.e_shstrndx = 5;

    rc = RTFileWriteAt(hFileOut, 0, &Hdr, sizeof(Hdr), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write ELF header to '%s': %Rrc\n", pszStubPath, rc);

    Elf64_Shdr aShdrs[6]; RT_ZERO(aShdrs);
    uint32_t idx = 1;
    /* NULL header */
    /* .dynsym */
    aShdrs[idx].sh_name      = 11;
    aShdrs[idx].sh_type      = SHT_DYNSYM;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = sizeof(Hdr) + Hdr.e_shnum * sizeof(aShdrs[0]);
    aShdrs[idx].sh_size      = cDynSymsExport * sizeof(*paDynSyms);
    aShdrs[idx].sh_link      = 2;
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(uint64_t);
    aShdrs[idx].sh_entsize   = sizeof(*paDynSyms);
    idx++;

    /* .dynstr */
    aShdrs[idx].sh_name      = 19;
    aShdrs[idx].sh_type      = SHT_STRTAB;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size;
    aShdrs[idx].sh_size      = cbStrTab;
    aShdrs[idx].sh_link      = 0;
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(uint8_t);
    aShdrs[idx].sh_entsize   = 0;
    idx++;

    if (pbGnuVerDef)
    {
        /* .gnu.version */
        aShdrs[idx].sh_name      = 27;
        aShdrs[idx].sh_type      = SHT_GNU_versym;
        aShdrs[idx].sh_flags     = SHF_ALLOC;
        aShdrs[idx].sh_addr      = 0;
        aShdrs[idx].sh_offset    = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(uint16_t));
        aShdrs[idx].sh_size      = cDynSymsExport * sizeof(uint16_t);
        aShdrs[idx].sh_link      = 1; /* .dynsym */
        aShdrs[idx].sh_info      = 0;
        aShdrs[idx].sh_addralign = sizeof(uint16_t);
        aShdrs[idx].sh_entsize   = 2;
        idx++;

        /* .gnu.version_d */
        aShdrs[idx].sh_name      = 40;
        aShdrs[idx].sh_type      = SHT_GNU_verdef;
        aShdrs[idx].sh_flags     = SHF_ALLOC;
        aShdrs[idx].sh_addr      = 0;
        aShdrs[idx].sh_offset    = RT_ALIGN_64(aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size, sizeof(uint64_t));
        aShdrs[idx].sh_size      = cbGnuVerDef;
        aShdrs[idx].sh_link      = 2; /* .dynstr */
        aShdrs[idx].sh_info      = cVerdefEntries;
        aShdrs[idx].sh_addralign = sizeof(uint64_t);
        aShdrs[idx].sh_entsize   = 0;
        idx++;
    }

    /* .shstrtab */
    aShdrs[idx].sh_name      = 1;
    aShdrs[idx].sh_type      = SHT_STRTAB;
    aShdrs[idx].sh_flags     = SHF_ALLOC;
    aShdrs[idx].sh_addr      = 0;
    aShdrs[idx].sh_offset    = aShdrs[idx - 1].sh_offset + aShdrs[idx - 1].sh_size;
    aShdrs[idx].sh_size      = sizeof(s_achShStrTab);
    aShdrs[idx].sh_link      = 0;
    aShdrs[idx].sh_info      = 0;
    aShdrs[idx].sh_addralign = sizeof(uint8_t);
    aShdrs[idx].sh_entsize   = 0;
    idx++;

    rc = RTFileWriteAt(hFileOut, sizeof(Hdr), &aShdrs[0], idx * sizeof(aShdrs[0]), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write section headers to '%s': %Rrc\n", pszStubPath, rc);

    idx = 1;
    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, paDynSyms, cDynSymsExport * sizeof(*paDynSyms), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .dynsym section to '%s': %Rrc\n", pszStubPath, rc);

    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pachStrTab, cbStrTab, NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .dynstr section to '%s': %Rrc\n", pszStubPath, rc);

    if (pbGnuVerDef)
    {
        rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pu16GnuVerSym, cDynSymsExport * sizeof(uint16_t), NULL /*pcbWritten*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .gnu.version section to '%s': %Rrc\n", pszStubPath, rc);

        rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, pbGnuVerDef, cbGnuVerDef, NULL /*pcbWritten*/);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .gnu.version section to '%s': %Rrc\n", pszStubPath, rc);
    }

    rc = RTFileWriteAt(hFileOut, aShdrs[idx++].sh_offset, s_achShStrTab, sizeof(s_achShStrTab), NULL /*pcbWritten*/);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to write .gnu.version section to '%s': %Rrc\n", pszStubPath, rc);

    RTFileClose(hFileOut);
    return RTEXITCODE_SUCCESS;
}


/**
 * Display usage
 *
 * @returns success if stdout, syntax error if stderr.
 */
static RTEXITCODE usage(FILE *pOut, const char *argv0)
{
    fprintf(pOut,
            "usage: %s --input <input binary> [options and operations]\n"
            "\n"
            "Operations and Options (processed in place):\n"
            "  --verbose                                Noisier.\n"
            "  --quiet                                  Quiet execution.\n"
            "  --delete-runpath                         Deletes all DT_RUNPATH entries.\n"
            "  --change-runpath <new runpath>           Changes the first DT_RUNPATH entry to the new one.\n"
            "  --create-linker-stub <path/to/stub>      Creates a stub library used for linking.\n"
            , argv0);
    return pOut == stdout ? RTEXITCODE_SUCCESS : RTEXITCODE_SYNTAX;
}


/**
 * Parses the arguments.
 */
static RTEXITCODE parseArguments(int argc,  char **argv)
{
    /*
     * Option config.
     */
    static RTGETOPTDEF const s_aOpts[] =
    {
        { "--input",                            'i', RTGETOPT_REQ_STRING  },
        { "--verbose",                          'v', RTGETOPT_REQ_NOTHING },
        { "--delete-runpath",                   'd', RTGETOPT_REQ_NOTHING },
        { "--change-runpath",                   'c', RTGETOPT_REQ_STRING  },
        { "--create-stub-library",              's', RTGETOPT_REQ_STRING  },
    };

    RTGETOPTUNION   ValueUnion;
    RTGETOPTSTATE   GetOptState;
    int rc = RTGetOptInit(&GetOptState, argc, argv, &s_aOpts[0], RT_ELEMENTS(s_aOpts), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertReleaseRCReturn(rc, RTEXITCODE_FAILURE);

    /*
     * Process the options.
     */
    while ((rc = RTGetOpt(&GetOptState, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'h':
                return usage(stdout, argv[0]);

            case 'i':
                if (g_pszInput)
                    return RTMsgErrorExit(RTEXITCODE_SYNTAX, "Input file is already set to '%s'", g_pszInput);
                g_pszInput = ValueUnion.psz;
                break;

            case 'v':
                g_cVerbosity++;
                break;

            case 'd':
                g_enmAction = kVBoxEditElfAction_DeleteRunpath;
                break;

            case 'c':
                g_enmAction = kVBoxEditElfAction_ChangeRunpath;
                g_pszRunpath = ValueUnion.psz;
                break;

            case 's':
                g_enmAction = kVBoxEditElfAction_CreateLinkerStub;
                g_pszLinkerStub = ValueUnion.psz;
                break;

            case 'V':
            {
                /* The following is assuming that svn does it's job here. */
                static const char s_szRev[] = "$Revision$";
                const char *psz = RTStrStripL(strchr(s_szRev, ' '));
                RTPrintf("r%.*s\n", strchr(psz, ' ') - psz, psz);
                return RTEXITCODE_SUCCESS;
            }

            /*
             * Errors and bugs.
             */
            default:
                return RTGetOptPrintError(rc, &ValueUnion);
        }
    }

    /*
     * Check that we've got all we need.
     */
    if (g_enmAction == kVBoxEditElfAction_Nothing)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No action specified (--delete-runpath, --change-runpath or --create-stub-library)");
    if (!g_pszInput)
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No input file specified (--input)");

    return RTEXITCODE_SUCCESS;
}


int main(int argc, char **argv)
{
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return 1;

    RTEXITCODE rcExit = parseArguments(argc, argv);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Take action.
         */
        if (g_enmAction == kVBoxEditElfAction_DeleteRunpath)
            rcExit = deleteRunpath(g_pszInput);
        else if (g_enmAction == kVBoxEditElfAction_ChangeRunpath)
            rcExit = changeRunpath(g_pszInput, g_pszRunpath);
        else if (g_enmAction == kVBoxEditElfAction_CreateLinkerStub)
            rcExit = createLinkerStubFrom(g_pszInput, g_pszLinkerStub);
    }

    return rcExit;
}

