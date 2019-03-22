/** @file
 * tstShflSize - Testcase for shared folder structure sizes.
 * Run this on Linux and Windows, then compare.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#include <VBox/shflsvc.h>
#include <iprt/string.h>
#include <stdio.h>

#define STRUCT(t, size)   \
    do { \
        if (fPrintChecks) \
            printf("    STRUCT(" #t ", %d);\n", (int)sizeof(t)); \
        else if ((size) != sizeof(t)) \
        { \
            printf("%30s: %d expected %d!\n", #t, (int)sizeof(t), (size)); \
            cErrors++; \
        } \
        else if (!fQuiet)\
            printf("%30s: %d\n", #t, (int)sizeof(t)); \
    } while (0)


int main(int argc, char **argv)
{
    unsigned cErrors = 0;

    /*
     * Prints the code below if any argument was giving.
     */
    bool fQuiet = argc == 2 && !strcmp(argv[1], "quiet");
    bool fPrintChecks = !fQuiet && argc != 1;

    printf("tstShflSizes: TESTING\n");

    /*
     * The checks.
     */
    STRUCT(SHFLROOT, 4);
    STRUCT(SHFLHANDLE, 8);
    STRUCT(SHFLSTRING, 6);
    STRUCT(SHFLCREATERESULT, 4);
    STRUCT(SHFLCREATEPARMS, 108);
    STRUCT(SHFLMAPPING, 8);
    STRUCT(SHFLDIRINFO, 128);
    STRUCT(SHFLVOLINFO, 40);
    STRUCT(SHFLFSOBJATTR, 44);
    STRUCT(SHFLFSOBJINFO, 92);
#ifdef VBOX_WITH_64_BITS_GUESTS
/* The size of the guest structures depends on the current architecture bit count (ARCH_BITS)
 * because the HGCMFunctionParameter structure differs in 32 and 64 bit guests.
 * The host VMMDev device takes care about this.
 *
 * Therefore this testcase verifies whether structure sizes are correct for the current ARCH_BITS.
 */
# if ARCH_BITS == 64
    STRUCT(VBoxSFQueryMappings, 88);
    STRUCT(VBoxSFQueryMapName, 72);
    STRUCT(VBoxSFMapFolder_Old, 88);
    STRUCT(VBoxSFMapFolder, 104);
    STRUCT(VBoxSFUnmapFolder, 56);
    STRUCT(VBoxSFCreate, 88);
    STRUCT(VBoxSFClose, 72);
    STRUCT(VBoxSFRead, 120);
    STRUCT(VBoxSFWrite, 120);
    STRUCT(VBoxSFLock, 120);
    STRUCT(VBoxSFFlush, 72);
    STRUCT(VBoxSFList, 168);
    STRUCT(VBoxSFInformation, 120);
    STRUCT(VBoxSFRemove, 88);
    STRUCT(VBoxSFRename, 104);
# elif ARCH_BITS == 32
    STRUCT(VBoxSFQueryMappings, 24+52);
    STRUCT(VBoxSFQueryMapName, 24+40); /* this was changed from 52 in 21976 after VBox-1.4. */
    STRUCT(VBoxSFMapFolder_Old, 24+52);
    STRUCT(VBoxSFMapFolder, 24+64);
    STRUCT(VBoxSFUnmapFolder, 24+28);
    STRUCT(VBoxSFCreate, 24+52);
    STRUCT(VBoxSFClose, 24+40);
    STRUCT(VBoxSFRead, 24+76);
    STRUCT(VBoxSFWrite, 24+76);
    STRUCT(VBoxSFLock, 24+76);
    STRUCT(VBoxSFFlush, 24+40);
    STRUCT(VBoxSFList, 24+112);
    STRUCT(VBoxSFInformation, 24+76);
    STRUCT(VBoxSFRemove, 24+52);
    STRUCT(VBoxSFRename, 24+64);
# else
#  error "Unsupported ARCH_BITS"
# endif /* ARCH_BITS */
#else
    STRUCT(VBoxSFQueryMappings, 24+52);
    STRUCT(VBoxSFQueryMapName, 24+40); /* this was changed from 52 in 21976 after VBox-1.4. */
    STRUCT(VBoxSFMapFolder_Old, 24+52);
    STRUCT(VBoxSFMapFolder, 24+64);
    STRUCT(VBoxSFUnmapFolder, 24+28);
    STRUCT(VBoxSFCreate, 24+52);
    STRUCT(VBoxSFClose, 24+40);
    STRUCT(VBoxSFRead, 24+76);
    STRUCT(VBoxSFWrite, 24+76);
    STRUCT(VBoxSFLock, 24+76);
    STRUCT(VBoxSFFlush, 24+40);
    STRUCT(VBoxSFList, 24+112);
    STRUCT(VBoxSFInformation, 24+76);
    STRUCT(VBoxSFRemove, 24+52);
    STRUCT(VBoxSFRename, 24+64);
#endif /* VBOX_WITH_64_BITS_GUESTS */

    /*
     * The summary.
     */
    if (!cErrors)
        printf("tstShflSizes: SUCCESS\n");
    else
        printf("tstShflSizes: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

