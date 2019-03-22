/* $Id$ */
/** @file
 * BS3Kit - Bs3InitMemory
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define BS3_USE_RM_TEXT_SEG 1
#include "bs3kit-template-header.h"
#include "bs3-cmn-memory.h"
#include <iprt/asm.h>
#include <VBox/VMMDevTesting.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

typedef struct INT15E820ENTRY
{
    uint64_t    uBaseAddr;
    uint64_t    cbRange;
    /** Memory type this entry describes, see INT15E820_TYPE_XXX. */
    uint32_t    uType;
    uint32_t    fAcpi3;
} INT15E820ENTRY;
AssertCompileSize(INT15E820ENTRY,24);


/** @name INT15E820_TYPE_XXX - Memory types returned by int 15h function 0xe820.
 * @{ */
#define INT15E820_TYPE_USABLE               1 /**< Usable RAM. */
#define INT15E820_TYPE_RESERVED             2 /**< Reserved by the system, unusable. */
#define INT15E820_TYPE_ACPI_RECLAIMABLE     3 /**< ACPI reclaimable memory, whatever that means. */
#define INT15E820_TYPE_ACPI_NVS             4 /**< ACPI non-volatile storage? */
#define INT15E820_TYPE_BAD                  5 /**< Bad memory, unusable. */
/** @} */


/**
 * Performs a int 15h function 0xe820 call.
 *
 * @returns Continuation value on success, 0 on failure.
 *          (Because of the way the API works, EBX should never be zero when
 *          data is returned.)
 * @param   pEntry              The return buffer.
 * @param   cbEntry             The size of the buffer (min 20 bytes).
 * @param   uContinuationValue  Zero the first time, the return value from the
 *                              previous call after that.
 */
BS3_DECL(uint32_t) Bs3BiosInt15hE820(INT15E820ENTRY BS3_FAR *pEntry, size_t cbEntry, uint32_t uContinuationValue);
#pragma aux Bs3BiosInt15hE820 = \
    ".386" \
    "shl    ebx, 10h" \
    "mov    bx, ax" /* ebx = continutation */ \
    "movzx  ecx, cx" \
    "movzx  edi, di" \
    "mov    edx, 0534d4150h" /*SMAP*/ \
    "mov    eax, 0xe820" \
    "int    15h" \
    "jc     failed" \
    "cmp    eax, 0534d4150h" \
    "jne    failed" \
    "cmp    cx, 20" \
    "jb     failed" \
    "mov    ax, bx" \
    "shr    ebx, 10h" /* ax:bx = continuation */ \
    "jmp    done" \
    "failed:" \
    "xor    ax, ax" \
    "xor    bx, bx" \
    "done:" \
    parm [es di] [cx] [ax bx] \
    value [ax bx] \
    modify exact [ax bx cx dx di es];

/**
 * Performs a int 15h function 0x88 call.
 *
 * @returns UINT32_MAX on failure, number of KBs above 1MB otherwise.
 */
BS3_DECL(uint32_t) Bs3BiosInt15h88(void);
#pragma aux Bs3BiosInt15h88 = \
    ".286" \
    "clc" \
    "mov    ax, 08800h" \
    "int    15h" \
    "jc     failed" \
    "xor    dx, dx" \
    "jmp    done" \
    "failed:" \
    "xor    ax, ax" \
    "dec    ax" \
    "mov    dx, ax" \
    "done:" \
    value [ax dx] \
    modify exact [ax bx cx dx es];


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Slab control structure for the 4K management of low memory (< 1MB). */
BS3SLABCTLLOW           g_Bs3Mem4KLow;
/** Slab control structure for the 4K management of tiled upper memory,
 *  between 1 MB and 16MB. */
BS3SLABCTLUPPERTILED    g_Bs3Mem4KUpperTiled;


/** Translates a power of two request size to an slab list index. */
uint8_t const           g_aiBs3SlabListsByPowerOfTwo[12] =
{
    /* 2^0  =    1 */  0,
    /* 2^1  =    2 */  0,
    /* 2^2  =    4 */  0,
    /* 2^3  =    8 */  0,
    /* 2^4  =   16 */  0,
    /* 2^5  =   32 */  1,
    /* 2^6  =   64 */  2,
    /* 2^7  =  128 */  3,
    /* 2^8  =  256 */  4,
    /* 2^9  =  512 */  5,
    /* 2^10 = 1024 */  -1
    /* 2^11 = 2048 */  -1
};

/** The slab list chunk sizes. */
uint16_t const          g_acbBs3SlabLists[BS3_MEM_SLAB_LIST_COUNT] =
{
    16,
    32,
    64,
    128,
    256,
    512,
};

/** Low memory slab lists, sizes given by g_acbBs3SlabLists. */
BS3SLABHEAD             g_aBs3LowSlabLists[BS3_MEM_SLAB_LIST_COUNT];
/** Upper tiled memory slab lists, sizes given by g_acbBs3SlabLists. */
BS3SLABHEAD             g_aBs3UpperTiledSlabLists[BS3_MEM_SLAB_LIST_COUNT];

/** Slab control structure sizes for the slab lists.
 * This is to help the allocator when growing a list. */
uint16_t const          g_cbBs3SlabCtlSizesforLists[BS3_MEM_SLAB_LIST_COUNT] =
{
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 16  / 8 /*=32*/), 16),
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 32  / 8 /*=16*/), 32),
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 64  / 8 /*=8*/),  64),
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 128 / 8 /*=4*/), 128),
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 256 / 8 /*=2*/), 256),
    RT_ALIGN(sizeof(BS3SLABCTL) - 4 + (4096 / 512 / 8 /*=1*/), 512),
};


/** The last RAM address below 4GB (approximately). */
uint32_t                g_uBs3EndOfRamBelow4G = 0;



/**
 * Adds a range of memory to the tiled slabs.
 *
 * @param   uRange      Start of range.
 * @param   cbRange     Size of range.
 */
static void bs3InitMemoryAddRange32(uint32_t uRange, uint32_t cbRange)
{
    uint32_t uRangeEnd = uRange + cbRange;
    if (uRangeEnd < uRange)
        uRangeEnd = UINT32_MAX;

    /* Raise the end-of-ram-below-4GB marker? */
    if (uRangeEnd > g_uBs3EndOfRamBelow4G)
        g_uBs3EndOfRamBelow4G = uRangeEnd;

    /* Applicable to tiled memory? */
    if (   uRange < BS3_SEL_TILED_AREA_SIZE
        && (   uRange >= _1M
            || uRangeEnd >= _1M))
    {
        uint16_t cPages;

        /* Adjust the start of the range such that it's at or above 1MB and page aligned.  */
        if (uRange < _1M)
        {
            cbRange -= _1M - uRange;
            uRange   = _1M;
        }
        else if (uRange & (_4K - 1U))
        {
            cbRange -= uRange & (_4K - 1U);
            uRange   = RT_ALIGN_32(uRange, _4K);
        }

        /* Adjust the end/size of the range such that it's page aligned and not beyond the tiled area. */
        if (uRangeEnd > BS3_SEL_TILED_AREA_SIZE)
        {
            cbRange  -= uRangeEnd - BS3_SEL_TILED_AREA_SIZE;
            uRangeEnd = BS3_SEL_TILED_AREA_SIZE;
        }
        else if (uRangeEnd & (_4K - 1U))
        {
            cbRange   -= uRangeEnd & (_4K - 1U);
            uRangeEnd &= ~(uint32_t)(_4K - 1U);
        }

        /* If there is still something, enable it.
           (We're a bit paranoid here don't trust the BIOS to only report a page once.)  */
        cPages = cbRange >> 12; /*div 4K*/
        if (cPages)
        {
            unsigned i;
            uRange -= _1M;
            i = uRange >> 12; /*div _4K*/
            while (cPages-- > 0)
            {
                uint16_t uLineToLong = ASMBitTestAndClear(g_Bs3Mem4KUpperTiled.Core.bmAllocated, i);
                g_Bs3Mem4KUpperTiled.Core.cFreeChunks += uLineToLong;
                i++;
            }
        }
    }
}


BS3_DECL(void) BS3_FAR_CODE Bs3InitMemory_rm_far(void)
{
    uint16_t            i;
    uint16_t            cPages;
    uint32_t            u32;
    INT15E820ENTRY      Entry;
    uint32_t BS3_FAR   *pu32Mmio;

    /*
     * Enable the A20 gate.
     */
    Bs3A20Enable();

    /*
     * Low memory (4K chunks).
     *      - 0x00000 to 0x004ff - Interrupt Vector table, BIOS data area.
     *      - 0x01000 to 0x0ffff - Stacks.
     *      - 0x10000 to 0x1yyyy - BS3TEXT16
     *      - 0x20000 to 0x26fff - BS3SYSTEM16
     *      - 0x29000 to 0xzzzzz - BS3DATA16, BS3TEXT32, BS3TEXT64, BS3DATA32, BS3DATA64 (in that order).
     *      - 0xzzzzZ to 0x9fdff - Free conventional memory.
     *      - 0x9fc00 to 0x9ffff - Extended BIOS data area (exact start may vary).
     *      - 0xa0000 to 0xbffff - VGA MMIO
     *      - 0xc0000 to 0xc7fff - VGA BIOS
     *      - 0xc8000 to 0xeffff - ROMs, tables, unusable.
     *      - 0xf0000 to 0xfffff - PC BIOS.
     */
    Bs3SlabInit(&g_Bs3Mem4KLow.Core, sizeof(g_Bs3Mem4KLow), 0 /*uFlatSlabPtr*/, 0xA0000 /* 640 KB*/, _4K);

    /* Mark the stacks and whole image as allocated. */
    cPages  = (Bs3TotalImageSize + _4K - 1U) >> 12;
    ASMBitSetRange(g_Bs3Mem4KLow.Core.bmAllocated, 0, 0x10 + cPages);

    /* Mark any unused pages between BS3TEXT16 and BS3SYSTEM16 as free. */
    cPages = (Bs3Text16_Size + (uint32_t)_4K - 1U) >> 12;
    ASMBitClearRange(g_Bs3Mem4KLow.Core.bmAllocated, 0x10U + cPages, 0x20U);

    /* In case the system has less than 640KB of memory, check the BDA variable for it. */
    cPages = *(uint16_t BS3_FAR *)BS3_FP_MAKE(0x0000, 0x0413); /* KB of low memory */
    if (cPages < 640)
    {
        cPages = 640 - cPages;
        cPages = RT_ALIGN(cPages, 4);
        cPages >>= 2;
        ASMBitSetRange(g_Bs3Mem4KLow.Core.bmAllocated, 0xA0 - cPages, 0xA0);
    }
    else
        ASMBitSet(g_Bs3Mem4KLow.Core.bmAllocated, 0x9F);

    /* Recalc free pages. */
    cPages = 0;
    i = g_Bs3Mem4KLow.Core.cChunks;
    while (i-- > 0)
        cPages += !ASMBitTest(g_Bs3Mem4KLow.Core.bmAllocated, i);
    g_Bs3Mem4KLow.Core.cFreeChunks = cPages;

    /*
     * First 16 MB of memory above 1MB.  We start out by marking it all allocated.
     */
    Bs3SlabInit(&g_Bs3Mem4KUpperTiled.Core, sizeof(g_Bs3Mem4KUpperTiled), _1M, BS3_SEL_TILED_AREA_SIZE - _1M, _4K);

    ASMBitSetRange(g_Bs3Mem4KUpperTiled.Core.bmAllocated, 0, g_Bs3Mem4KUpperTiled.Core.cChunks);
    g_Bs3Mem4KUpperTiled.Core.cFreeChunks = 0;

    /* Ask the BIOS about where there's memory, and make pages in between 1MB
       and BS3_SEL_TILED_AREA_SIZE present.  This means we're only interested
       in entries describing usable memory, ASSUMING of course no overlaps. */
    if (   (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386
        && Bs3BiosInt15hE820(&Entry, sizeof(Entry), 0) != 0)
    {
        uint32_t uCont = 0;
        i = 0;
        while (   (uCont = Bs3BiosInt15hE820(&Entry, sizeof(Entry), uCont)) != 0
               && i++ < 2048)
            if (Entry.uType == INT15E820_TYPE_USABLE)
                if (!(Entry.uBaseAddr >> 32))
                    /* Convert from 64-bit to 32-bit value and record it. */
                    bs3InitMemoryAddRange32((uint32_t)Entry.uBaseAddr,
                                            (Entry.cbRange >> 32) ? UINT32_C(0xfffff000) : (uint32_t)Entry.cbRange);
    }
    /* Try the 286+ API for getting memory above 1MB and (usually) below 16MB. */
    else if (   (g_uBs3CpuDetected & BS3CPU_TYPE_MASK) >= BS3CPU_80386
             && (u32 = Bs3BiosInt15h88()) != UINT32_MAX
             && u32 > 0)
        bs3InitMemoryAddRange32(_1M, u32 * _1K);

    /*
     * Check if we've got the VMMDev MMIO testing memory mapped above 1MB.
     */
    pu32Mmio = (uint32_t BS3_FAR *)BS3_FP_MAKE(VMMDEV_TESTING_MMIO_RM_SEL,
                                               VMMDEV_TESTING_MMIO_RM_OFF2(VMMDEV_TESTING_MMIO_OFF_NOP));
    if (*pu32Mmio == VMMDEV_TESTING_NOP_RET)
    {
        Bs3Printf("Memory: Found VMMDev MMIO testing region\n");
        if (!ASMBitTestAndSet(g_Bs3Mem4KUpperTiled.Core.bmAllocated, 1))
            g_Bs3Mem4KUpperTiled.Core.cFreeChunks--;

    }

    /*
     * Initialize the slab lists.
     */
    for (i = 0; i < BS3_MEM_SLAB_LIST_COUNT; i++)
    {
        Bs3SlabListInit(&g_aBs3LowSlabLists[i], g_acbBs3SlabLists[i]);
        Bs3SlabListInit(&g_aBs3UpperTiledSlabLists[i], g_acbBs3SlabLists[i]);
    }

#if 0
    /*
     * For debugging.
     */
    Bs3Printf("Memory-low: %u/%u chunks bmAllocated[]=", g_Bs3Mem4KLow.Core.cFreeChunks, g_Bs3Mem4KLow.Core.cChunks);
    for (i = 0; i < 20; i++)
        Bs3Printf("%02x ", g_Bs3Mem4KLow.Core.bmAllocated[i]);
    Bs3Printf("\n");
    Bs3Printf("Memory-upt: %u/%u chunks bmAllocated[]=", g_Bs3Mem4KUpperTiled.Core.cFreeChunks, g_Bs3Mem4KUpperTiled.Core.cChunks);
    for (i = 0; i < 32; i++)
        Bs3Printf("%02x ", g_Bs3Mem4KUpperTiled.Core.bmAllocated[i]);
    Bs3Printf("...\n");
#endif
}

