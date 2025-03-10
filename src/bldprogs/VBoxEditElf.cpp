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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @name Options
 * @{ */
static enum
{
    kVBoxEditElfAction_Nothing,
    kVBoxEditElfAction_DeleteRunpath,
    kVBoxEditElfAction_ChangeRunpath
}                 g_enmAction = kVBoxEditElfAction_Nothing;
static const char *g_pszInput = NULL;
/** Verbosity level. */
static int        g_cVerbosity = 0;
/** New runpath. */
static const char *g_pszRunpath = NULL;
/** @} */



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
            "  --verbose                        Noisier.\n"
            "  --quiet                          Quiet execution.\n"
            "  --delete-runpath                 Deletes all DT_RUNPATH entries.\n"
            "  --change-runpath <new runpath>   Changes the first DT_RUNPATH entry to the new one.\n"
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
        /* dtrace w/ long options */
        { "--input",                            'i', RTGETOPT_REQ_STRING  },
        { "--verbose",                          'v', RTGETOPT_REQ_NOTHING },
        /* our stuff */
        { "--delete-runpath",                   'd', RTGETOPT_REQ_NOTHING },
        { "--change-runpath",                   'c', RTGETOPT_REQ_STRING  },
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
        return RTMsgErrorExit(RTEXITCODE_SYNTAX, "No action specified (--delete-runpath or --change-runpath)");
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
    }

    return rcExit;
}

