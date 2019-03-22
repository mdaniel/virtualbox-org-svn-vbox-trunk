/* $Id$ */
/** @file
 * IPRT - Fake asm.h routines for use early in a new port.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include <iprt/asm.h>
#include "internal/iprt.h"

#include <iprt/string.h>
#include <iprt/param.h>


RTDECL(uint8_t) ASMAtomicXchgU8(volatile uint8_t *pu8, uint8_t u8)
{
    uint8_t u8Ret = *pu8;
    *pu8 = u8;
    return u8Ret;
}

RTDECL(uint16_t) ASMAtomicXchgU16(volatile uint16_t *pu16, uint16_t u16)
{
    uint16_t u16Ret = *pu16;
    *pu16 = u16;
    return u16Ret;
}

RTDECL(uint32_t) ASMAtomicXchgU32(volatile uint32_t *pu32, uint32_t u32)
{
    uint32_t u32Ret = *pu32;
    *pu32 = u32;
    return u32Ret;
}

RTDECL(uint64_t) ASMAtomicXchgU64(volatile uint64_t *pu64, uint64_t u64)
{
    uint64_t u64Ret = *pu64;
    *pu64 = u64;
    return u64Ret;
}

RTDECL(bool) ASMAtomicCmpXchgU8(volatile uint8_t *pu8, const uint8_t u8New, const uint8_t u8Old)
{
    if (*pu8 == u8Old)
    {
        *pu8 = u8New;
        return true;
    }
    return false;
}

RTDECL(bool) ASMAtomicCmpXchgU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old)
{
    if (*pu32 == u32Old)
    {
        *pu32 = u32New;
        return true;
    }
    return false;
}

RTDECL(bool) ASMAtomicCmpXchgU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old)
{
    if (*pu64 == u64Old)
    {
        *pu64 = u64New;
        return true;
    }
    return false;
}

RTDECL(bool) ASMAtomicCmpXchgExU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old, uint32_t *pu32Old)
{
    uint32_t u32Cur = *pu32;
    if (u32Cur == u32Old)
    {
        *pu32 = u32New;
        *pu32Old = u32Old;
        return true;
    }
    *pu32Old = u32Cur;
    return false;
}

RTDECL(bool) ASMAtomicCmpXchgExU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old, uint64_t *pu64Old)
{
    uint64_t u64Cur = *pu64;
    if (u64Cur == u64Old)
    {
        *pu64 = u64New;
        *pu64Old = u64Old;
        return true;
    }
    *pu64Old = u64Cur;
    return false;
}

RTDECL(uint32_t) ASMAtomicAddU32(uint32_t volatile *pu32, uint32_t u32)
{
    uint32_t u32Old = *pu32;
    *pu32 = u32Old + u32;
    return u32Old;
}

RTDECL(uint64_t) ASMAtomicAddU64(uint64_t volatile *pu64, uint64_t u64)
{
    uint64_t u64Old = *pu64;
    *pu64 = u64Old + u64;
    return u64Old;
}

RTDECL(uint32_t) ASMAtomicIncU32(uint32_t volatile *pu32)
{
    return *pu32 += 1;
}

RTDECL(uint32_t) ASMAtomicDecU32(uint32_t volatile *pu32)
{
    return *pu32 -= 1;
}

RTDECL(uint64_t) ASMAtomicIncU64(uint64_t volatile *pu64)
{
    return *pu64 += 1;
}

RTDECL(uint64_t) ASMAtomicDecU64(uint64_t volatile *pu64)
{
    return *pu64 -= 1;
}

RTDECL(void) ASMAtomicOrU32(uint32_t volatile *pu32, uint32_t u32)
{
    *pu32 |= u32;
}

RTDECL(void) ASMAtomicAndU32(uint32_t volatile *pu32, uint32_t u32)
{
    *pu32 &= u32;
}

RTDECL(void) ASMAtomicOrU64(uint64_t volatile *pu64, uint64_t u64)
{
    *pu64 |= u64;
}

RTDECL(void) ASMAtomicAndU64(uint64_t volatile *pu64, uint64_t u64)
{
    *pu64 &= u64;
}

RTDECL(void) ASMSerializeInstruction(void)
{

}

RTDECL(uint64_t) ASMAtomicReadU64(volatile uint64_t *pu64)
{
    return *pu64;
}

RTDECL(uint64_t) ASMAtomicUoReadU64(volatile uint64_t *pu64)
{
    return *pu64;
}

RTDECL(void) ASMMemZeroPage(volatile void *pv)
{
    uintptr_t volatile *puPtr = (uintptr_t volatile *)pv;
    uint32_t            cbLeft = PAGE_SIZE / sizeof(uintptr_t);
    while (cbLeft-- > 0)
        *puPtr++ = 0;
}

RTDECL(void) ASMMemZero32(volatile void *pv, size_t cb)
{
    uint32_t volatile *pu32   = (uint32_t volatile *)pv;
    uint32_t           cbLeft = cb / sizeof(uint32_t);
    while (cbLeft-- > 0)
        *pu32++ = 0;
}

RTDECL(void) ASMMemFill32(volatile void *pv, size_t cb, uint32_t u32)
{
    uint32_t volatile *pu32 = (uint32_t volatile *)pv;
    while (cb > 0)
    {
        *pu32 = u32;
        cb -= sizeof(uint32_t);
        pu32++;
    }
}

RTDECL(uint8_t) ASMProbeReadByte(const void *pvByte)
{
    return *(volatile uint8_t *)pvByte;
}

#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
RTDECL(void) ASMNopPause(void)
{
}
#endif

RTDECL(void) ASMBitSet(volatile void *pvBitmap, int32_t iBit)
{
    uint8_t volatile *pau8Bitmap = (uint8_t volatile *)pvBitmap;
    pau8Bitmap[iBit / 8] |= (uint8_t)RT_BIT_32(iBit & 7);
}

RTDECL(void) ASMAtomicBitSet(volatile void *pvBitmap, int32_t iBit)
{
    ASMBitSet(pvBitmap, iBit);
}

RTDECL(void) ASMBitClear(volatile void *pvBitmap, int32_t iBit)
{
    uint8_t volatile *pau8Bitmap = (uint8_t volatile *)pvBitmap;
    pau8Bitmap[iBit / 8] &= ~((uint8_t)RT_BIT_32(iBit & 7));
}

RTDECL(void) ASMAtomicBitClear(volatile void *pvBitmap, int32_t iBit)
{
    ASMBitClear(pvBitmap, iBit);
}

RTDECL(void) ASMBitToggle(volatile void *pvBitmap, int32_t iBit)
{
    uint8_t volatile *pau8Bitmap = (uint8_t volatile *)pvBitmap;
    pau8Bitmap[iBit / 8] ^= (uint8_t)RT_BIT_32(iBit & 7);
}

RTDECL(void) ASMAtomicBitToggle(volatile void *pvBitmap, int32_t iBit)
{
    ASMBitToggle(pvBitmap, iBit);
}

RTDECL(bool) ASMBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    if (ASMBitTest(pvBitmap, iBit))
        return true;
    ASMBitSet(pvBitmap, iBit);
    return false;
}

RTDECL(bool) ASMAtomicBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    return ASMBitTestAndSet(pvBitmap, iBit);
}

RTDECL(bool) ASMBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    if (!ASMBitTest(pvBitmap, iBit))
        return false;
    ASMBitClear(pvBitmap, iBit);
    return true;
}

RTDECL(bool) ASMAtomicBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    return ASMBitTestAndClear(pvBitmap, iBit);
}

RTDECL(bool) ASMBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    bool fRet = ASMBitTest(pvBitmap, iBit);
    ASMBitToggle(pvBitmap, iBit);
    return fRet;
}

RTDECL(bool) ASMAtomicBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    return ASMBitTestAndToggle(pvBitmap, iBit);
}

RTDECL(bool) ASMBitTest(const volatile void *pvBitmap, int32_t iBit)
{
    uint8_t volatile *pau8Bitmap = (uint8_t volatile *)pvBitmap;
    return  pau8Bitmap[iBit / 8] & (uint8_t)RT_BIT_32(iBit & 7) ? true : false;
}

RTDECL(int) ASMBitFirstClear(const volatile void *pvBitmap, uint32_t cBits)
{
    uint32_t           iBit = 0;
    uint8_t volatile *pu8 = (uint8_t volatile *)pvBitmap;

    while (iBit < cBits)
    {
        uint8_t u8 = *pu8;
        if (u8 != UINT8_MAX)
        {
            while (u8 & 1)
            {
                u8 >>= 1;
                iBit++;
            }
            if (iBit >= cBits)
                return -1;
            return iBit;
        }

        iBit += 8;
        pu8++;
    }
    return -1;
}

RTDECL(int) ASMBitNextClear(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    const volatile uint8_t *pau8Bitmap = (const volatile uint8_t *)pvBitmap;
    int                      iBit = ++iBitPrev & 7;
    if (iBit)
    {
        /*
         * Inspect the byte containing the unaligned bit.
         */
        uint8_t u8 = ~pau8Bitmap[iBitPrev / 8] >> iBit;
        if (u8)
        {
            iBit = 0;
            while (!(u8 & 1))
            {
                u8 >>= 1;
                iBit++;
            }
            return iBitPrev + iBit;
        }

        /*
         * Skip ahead and see if there is anything left to search.
         */
        iBitPrev |= 7;
        iBitPrev++;
        if (cBits <= iBitPrev)
            return -1;
    }

    /*
     * Byte search, let ASMBitFirstClear do the dirty work.
     */
    iBit = ASMBitFirstClear(&pau8Bitmap[iBitPrev / 8], cBits - iBitPrev);
    if (iBit >= 0)
        iBit += iBitPrev;
    return iBit;
}

RTDECL(int) ASMBitFirstSet(const volatile void *pvBitmap, uint32_t cBits)
{
    uint32_t           iBit = 0;
    uint8_t volatile *pu8 = (uint8_t volatile *)pvBitmap;
    while (iBit < cBits)
    {
        uint8_t u8 = *pu8;
        if (u8 != 0)
        {
            while (!(u8 & 1))
            {
                u8 >>= 1;
                iBit++;
            }
            if (iBit >= cBits)
                return -1;
            return iBit;
        }

        iBit += 8;
        pu8++;
    }
    return -1;
}

RTDECL(int) ASMBitNextSet(const volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    const volatile uint8_t *pau8Bitmap = (const volatile uint8_t *)pvBitmap;
    int                      iBit = ++iBitPrev & 7;
    if (iBit)
    {
        /*
         * Inspect the byte containing the unaligned bit.
         */
        uint8_t u8 = pau8Bitmap[iBitPrev / 8] >> iBit;
        if (u8)
        {
            iBit = 0;
            while (!(u8 & 1))
            {
                u8 >>= 1;
                iBit++;
            }
            return iBitPrev + iBit;
        }

        /*
         * Skip ahead and see if there is anything left to search.
         */
        iBitPrev |= 7;
        iBitPrev++;
        if (cBits <= iBitPrev)
            return -1;
    }

    /*
     * Byte search, let ASMBitFirstSet do the dirty work.
     */
    iBit = ASMBitFirstSet(&pau8Bitmap[iBitPrev / 8], cBits - iBitPrev);
    if (iBit >= 0)
        iBit += iBitPrev;
    return iBit;
}

RTDECL(unsigned) ASMBitFirstSetU32(uint32_t u32)
{
    uint32_t iBit;
    for (iBit = 0; iBit < 32; iBit++)
        if (u32 & RT_BIT_32(iBit))
            return iBit + 1;
    return 0;
}

RTDECL(unsigned) ASMBitLastSetU32(uint32_t u32)
{
    int32_t iBit = 32;
    while (iBit-- > 0)
        if (u32 & RT_BIT_32(iBit))
            return iBit + 1;
    return 0;
}

RTDECL(uint16_t) ASMByteSwapU16(uint16_t u16)
{
    return RT_MAKE_U16(RT_HIBYTE(u16), RT_LOBYTE(u16));
}

RTDECL(uint32_t) ASMByteSwapU32(uint32_t u32)
{
    return RT_MAKE_U32_FROM_U8(RT_BYTE4(u32), RT_BYTE3(u32), RT_BYTE2(u32), RT_BYTE1(u32));
}

