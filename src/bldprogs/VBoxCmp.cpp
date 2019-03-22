/* $Id$ */
/** @file
 * File Compare - Compares two files byte by byte.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <iprt/string.h>
#include <iprt/stdarg.h>


/**
 * Writes an error message.
 *
 * @returns RTEXITCODE_FAILURE.
 * @param   pcszFormat          Error message.
 * @param   ...                 Format argument referenced in the message.
 */
static RTEXITCODE printErr(const char *pcszFormat, ...)
{
    va_list va;

    fprintf(stderr, "VBoxCmp: ");
    va_start(va, pcszFormat);
    vfprintf(stderr, pcszFormat, va);
    va_end(va);

    return RTEXITCODE_FAILURE;
}


static FILE *openFile(const char *pszFile)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    FILE *pFile = fopen(pszFile, "rb");
#else
    FILE *pFile = fopen(pszFile, "r");
#endif
    if (!pFile)
        printErr("Failed to open '%s': %s\n", pszFile, strerror(errno));
    return pFile;
}


static RTEXITCODE compareFiles(FILE *pFile1, FILE *pFile2)
{
    if (!pFile1 || !pFile2)
        return RTEXITCODE_FAILURE;

    uint32_t    cMismatches = 1;
    RTEXITCODE  rcRet       = RTEXITCODE_SUCCESS;
    uint64_t    off         = 0;
    for (;;)
    {
        uint8_t b1;
        size_t cb1 = fread(&b1, sizeof(b1), 1, pFile1);
        uint8_t b2;
        size_t cb2 = fread(&b2, sizeof(b2), 1, pFile2);
        if (cb1 != 1 || cb2 != 1)
            break;
        if (b1 != b2)
        {
            printErr("0x%x%08x: %#04x (%3d) != %#04x (%3d)\n", (uint32_t)(off >> 32), (uint32_t)off, b1, b1, b2, b2);
            rcRet = RTEXITCODE_FAILURE;
            cMismatches++;
            if (cMismatches > 128)
            {
                printErr("Too many mismatches, giving up\n");
                return rcRet;
            }
        }
        off++;
    }

    if (!feof(pFile1) || !feof(pFile2))
    {
        if (!feof(pFile1) && ferror(pFile1))
            rcRet = printErr("Read error on file #1.\n");
        else if (!feof(pFile2) && ferror(pFile2))
            rcRet = printErr("Read error on file #2.\n");
        else if (!feof(pFile2))
            rcRet = printErr("0x%x%08x: file #1 ends before file #2\n", (uint32_t)(off >> 32), (uint32_t)off);
        else
            rcRet = printErr("0x%x%08x: file #2 ends before file #1\n", (uint32_t)(off >> 32), (uint32_t)off);
    }

    return rcRet;
}


int main(int argc, char *argv[])
{
    RTEXITCODE rcExit;

    if (argc == 3)
    {
        const char *pszFile1 = argv[1];
        const char *pszFile2 = argv[2];
        FILE       *pFile1 = openFile(pszFile1);
        FILE       *pFile2 = openFile(pszFile2);
        rcExit = compareFiles(pFile1, pFile2);
        if (pFile1)
            fclose(pFile1);
        if (pFile2)
            fclose(pFile2);
    }
    else
        rcExit = printErr("Syntax error: usage: VBoxCmp <file1> <file2>\n");
    return rcExit;
}

