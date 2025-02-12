/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_ACPI
#include <iprt/acpi.h>
#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/utf16.h>
#include <iprt/uuid.h>

#include <iprt/formats/acpi-aml.h>
#include <iprt/formats/acpi-resources.h>

#include "internal/acpi.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Package stack element.
 */
typedef struct RTACPITBLSTACKELEM
{
    /** Offset into the table buffer memory where the PkgLength object starts. */
    uint32_t                    offPkgLength;
    /** Current size of the package in bytes, without the PkgLength object. */
    uint32_t                    cbPkg;
    /** The operator creating the package, UINT8_MAX denotes the special root operator. */
    uint8_t                     bOp;
} RTACPITBLSTACKELEM;
/** Pointer to a package stack element. */
typedef RTACPITBLSTACKELEM *PRTACPITBLSTACKELEM;
/** Pointer to a const package stack element. */
typedef const RTACPITBLSTACKELEM *PCRTACPITBLSTACKELEM;


/**
 * ACPI table generator instance.
 */
typedef struct RTACPITBLINT
{
    /** Pointer to the ACPI table header, needed when finalizing the table. */
    PACPITBLHDR                 pHdr;
    /** Byte buffer holding the actual table. */
    uint8_t                     *pbTblBuf;
    /** Size of the table buffer. */
    uint32_t                    cbTblBuf;
    /** Current offset into the table buffer. */
    uint32_t                    offTblBuf;
    /** Flag whether the table is finalized. */
    bool                        fFinalized;
    /** First error code encountered. */
    int                         rcErr;
    /** Pointer to the package element stack. */
    PRTACPITBLSTACKELEM         paPkgStack;
    /** Number of elements the package stack can hold. */
    uint32_t                    cPkgStackElems;
    /** Index of the current package in the package stack. */
    uint32_t                    idxPkgStackElem;
} RTACPITBLINT;
/** Pointer to an ACPI table generator instance. */
typedef RTACPITBLINT *PRTACPITBLINT;


/**
 * ACPI resource builder instance.
 */
typedef struct RTACPIRESINT
{
    /** Byte buffer holding the resource. */
    uint8_t                     *pbResBuf;
    /** Size of the resource buffer. */
    size_t                      cbResBuf;
    /** Current offset into the resource buffer. */
    uint32_t                    offResBuf;
    /** Flag whether the resource is sealed. */
    bool                        fSealed;
    /** First error code encountered. */
    int                         rcErr;
} RTACPIRESINT;
/** Pointer to an ACPI resource builder instance. */
typedef RTACPIRESINT *PRTACPIRESINT;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/


/**
 * Copies the given string into the given buffer padding the remainder with the given character.
 *
 * @param    pbId           The destination to copy the string to.
 * @param    cbId           Size of the buffer in bytes.
 * @param    pszStr         The string to copy.
 * @param    chPad          The character to pad with.
 */
static void rtAcpiTblCopyStringPadWith(uint8_t *pbId, size_t cbId, const char *pszStr, char chPad)
{
    Assert(strlen(pszStr) <= cbId);

    uint32_t idx = 0;
    while (*pszStr != '\0')
        pbId[idx++] = (uint8_t)*pszStr++;

    while (idx < cbId)
        pbId[idx++] = chPad;
}


/**
 * Updates the package length of the current package in the stack
 *
 * @param pThis                 The ACPI table instance.
 * @param cbAdd                 How many bytes to add to the package length.
 */
DECL_FORCE_INLINE(void) rtAcpiTblUpdatePkgLength(PRTACPITBLINT pThis, uint32_t cbAdd)
{
    PRTACPITBLSTACKELEM pPkgElem = &pThis->paPkgStack[pThis->idxPkgStackElem];
    pPkgElem->cbPkg += cbAdd;
}


/**
 * Ensures there is the given amount of room in the ACPI table buffer returning the pointer.
 *
 * @returns The pointer to the free space on success or NULL if out of memory.
 * @param pThis                 The ACPI table instance.
 * @param cbReq                 Amount of bytes requested.
 */
static uint8_t *rtAcpiTblBufEnsureSpace(PRTACPITBLINT pThis, uint32_t cbReq)
{
    if (RT_LIKELY(pThis->cbTblBuf - pThis->offTblBuf >= cbReq))
    {
        uint8_t *pb = &pThis->pbTblBuf[pThis->offTblBuf];
        pThis->offTblBuf += cbReq;
        return pb;
    }

    uint32_t const cbNew = RT_ALIGN_32(pThis->cbTblBuf + cbReq, _4K);
    uint8_t *pbNew = (uint8_t *)RTMemRealloc(pThis->pbTblBuf, cbNew);
    if (RT_UNLIKELY(!pbNew))
    {
        pThis->rcErr = VERR_NO_MEMORY;
        return NULL;
    }

    pThis->pbTblBuf = pbNew;
    pThis->cbTblBuf = cbNew;
    pThis->pHdr     = (PACPITBLHDR)pbNew;

    uint8_t *pb = &pThis->pbTblBuf[pThis->offTblBuf];
    pThis->offTblBuf += cbReq;
    return pb;
}


/**
 * Appends a new package in the given ACPI table instance package stack.
 *
 * @returns IPRT status code.
 * @retval VERR_NO_MEMORY if allocating additional resources to hold the new package failed.
 * @param pThis                 The ACPI table instance.
 * @param bOp                   The opcode byte the package starts with (for verification purposes when finalizing the package).
 * @param offPkgBuf             Offset of the start of the package buffer.
 */
static int rtAcpiTblPkgAppendEx(PRTACPITBLINT pThis, uint8_t bOp, uint32_t offPkgBuf)
{
    /* Get a new stack element. */
    if (pThis->idxPkgStackElem + 1 == pThis->cPkgStackElems)
    {
        uint32_t const cPkgElemsNew = pThis->cPkgStackElems + 8;
        PRTACPITBLSTACKELEM paPkgStackNew = (PRTACPITBLSTACKELEM)RTMemRealloc(pThis->paPkgStack, cPkgElemsNew * sizeof(*paPkgStackNew));
        if (!paPkgStackNew)
        {
            pThis->rcErr = VERR_NO_MEMORY;
            return VERR_NO_MEMORY;
        }

        pThis->paPkgStack     = paPkgStackNew;
        pThis->cPkgStackElems = cPkgElemsNew;
    }

    PRTACPITBLSTACKELEM pStackElem = &pThis->paPkgStack[++pThis->idxPkgStackElem];
    pStackElem->offPkgLength = offPkgBuf;
    pStackElem->cbPkg        = 0;
    pStackElem->bOp          = bOp;
    return VINF_SUCCESS;
}


/**
 * Starts a new ACPI package in the given ACPI table instance.
 *
 * @returns IPRT status code.
 * @retval VERR_NO_MEMORY if allocating additional resources to hold the new package failed.
 * @param pThis                 The ACPI table instance.
 * @param bOp                   The opcode byte identifying the package content.
 */
static int rtAcpiTblPkgStart(PRTACPITBLINT pThis, uint8_t bOp)
{
    /*
     * Allocate 1 byte for opcode + always 4 bytes for the PkgLength, as we don't know how much we will need upfront.
     * This will be corrected when the package is finalized.
     */
    uint8_t *pbPkg = rtAcpiTblBufEnsureSpace(pThis, 5);
    if (!pbPkg)
    {
        pThis->rcErr = VERR_NO_MEMORY;
        return VERR_NO_MEMORY;
    }

    *pbPkg = bOp;
    /*
     * Update the package length of the outer package for the opcode,
     * the PkgLength object's final length will be added in rtAcpiTblPkgFinish().
     */
    rtAcpiTblUpdatePkgLength(pThis, sizeof(bOp));
    return rtAcpiTblPkgAppendEx(pThis, bOp, (pbPkg + 1) - pThis->pbTblBuf);
}


/**
 * Starts a new ACPI package in the given ACPI table instance. This is for opcodes prefixed with
 * ACPI_AML_BYTE_CODE_PREFIX_EXT_O, which will be added automatically.
 *
 * @returns IPRT status code.
 * @retval VERR_NO_MEMORY if allocating additional resources to hold the new package failed.
 * @param pThis                 The ACPI table instance.
 * @param bOp                   The opcode byte identifying the package content.
 */
static int rtAcpiTblPkgStartExt(PRTACPITBLINT pThis, uint8_t bOp)
{
    /*
     * Allocate 2 bytes for ExtOpPrefix opcode + always 4 bytes for the PkgLength, as we don't know how much we will need upfront.
     * This will be corrected when the package is finalized.
     */
    uint8_t *pbPkg = rtAcpiTblBufEnsureSpace(pThis, 6);
    if (!pbPkg)
    {
        pThis->rcErr = VERR_NO_MEMORY;
        return VERR_NO_MEMORY;
    }

    pbPkg[0] = ACPI_AML_BYTE_CODE_PREFIX_EXT_OP;
    pbPkg[1] = bOp;

    /*
     * Update the package length of the outer package for the opcode,
     * the PkgLength object's final length will be added in rtAcpiTblPkgFinish().
     */
    rtAcpiTblUpdatePkgLength(pThis, sizeof(uint8_t) + sizeof(bOp));
    return rtAcpiTblPkgAppendEx(pThis, bOp, (pbPkg + 2) - pThis->pbTblBuf);
}


/**
 * Finishes the current package on the top of the package stack, setting the
 * package length accordingly.
 *
 * @returns IPRT status code.
 * @retval VERR_INVALID_STATE if bOp doesn't match the opcode the package was started with (asserted in debug builds).
 * @retval VERR_BUFFER_OVERFLOW if the package length exceeds what can be encoded in the package length field.
 * @param pThis                 The ACPI table instance.
 * @param bOp                   The opcode byte identifying the package content the package was started with.
 */
static int rtAcpiTblPkgFinish(PRTACPITBLINT pThis, uint8_t bOp)
{
    /* Ensure the op matches what is current on the top of the stack. */
    AssertReturn(pThis->paPkgStack[pThis->idxPkgStackElem].bOp == bOp, VERR_INVALID_STATE);

    /* Pop the topmost stack element from the stack. */
    PRTACPITBLSTACKELEM pPkgElem = &pThis->paPkgStack[pThis->idxPkgStackElem--];

    /*
     * Determine how many bytes we actually need for the PkgLength and re-arrange the ACPI table.
     *
     * Note! PkgLength will also include its own length.
     */
    uint8_t  *pbPkgLength = &pThis->pbTblBuf[pPkgElem->offPkgLength];
    uint32_t cbThisPkg    = pPkgElem->cbPkg;
    if (cbThisPkg + 1 <= 63)
    {
        /* Remove the gap. */
        memmove(pbPkgLength + 1, pbPkgLength + 4, cbThisPkg);
        pThis->offTblBuf -= 3;

        /* PkgLength only consists of the package lead byte. */
        cbThisPkg += 1;
        *pbPkgLength = (cbThisPkg & 0x3f);
    }
    else if (cbThisPkg + 2 < RT_BIT_32(12))
    {
        /* Remove the gap. */
        memmove(pbPkgLength + 2, pbPkgLength + 4, cbThisPkg);
        pThis->offTblBuf -= 2;

        cbThisPkg += 2;
        pbPkgLength[0] = (1 << 6) | (cbThisPkg & 0xf);
        pbPkgLength[1] = (cbThisPkg >> 4)  & 0xff;
    }
    else if (cbThisPkg + 3 < RT_BIT_32(20))
    {
        /* Remove the gap. */
        memmove(pbPkgLength + 3, pbPkgLength + 4, cbThisPkg);
        pThis->offTblBuf -= 1;

        cbThisPkg += 3;
        pbPkgLength[0] = (2 << 6) | (cbThisPkg & 0xf);
        pbPkgLength[1] = (cbThisPkg >> 4)  & 0xff;
        pbPkgLength[2] = (cbThisPkg >> 12) & 0xff;
    }
    else if (cbThisPkg + 4 < RT_BIT_32(28))
    {
        cbThisPkg += 4;
        pbPkgLength[0] = (3 << 6) | (cbThisPkg & 0xf);
        pbPkgLength[1] = (cbThisPkg >> 4)  & 0xff;
        pbPkgLength[2] = (cbThisPkg >> 12) & 0xff;
        pbPkgLength[3] = (cbThisPkg >> 20) & 0xff;
    }
    else
        return VERR_BUFFER_OVERFLOW;

    /* Update the size of the outer package. */
    pThis->paPkgStack[pThis->idxPkgStackElem].cbPkg += cbThisPkg;

    return VINF_SUCCESS;
}


/**
 * Appends the given byte to the ACPI table, updating the package length of the current package.
 *
 * @param pThis                 The ACPI table instance.
 * @param bData                 The byte data to append.
 */
DECLINLINE(void) rtAcpiTblAppendByte(PRTACPITBLINT pThis, uint8_t bData)
{
    uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, sizeof(bData));
    if (pb)
    {
        *pb = bData;
        rtAcpiTblUpdatePkgLength(pThis, sizeof(bData));
    }
}


/**
 * Appends the given double word to the ACPI table, updating the package length of the current package.
 *
 * @param pThis                 The ACPI table instance.
 * @param u32                   The data to append.
 */
DECLINLINE(void) rtAcpiTblAppendDword(PRTACPITBLINT pThis, uint32_t u32)
{
    uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, sizeof(u32));
    if (pb)
    {
        pb[0] = (uint8_t)u32;
        pb[1] = (uint8_t)(u32 >>  8);
        pb[2] = (uint8_t)(u32 >> 16);
        pb[3] = (uint8_t)(u32 >> 24);
        rtAcpiTblUpdatePkgLength(pThis, sizeof(u32));
    }
}


/**
 * Appends the given date to the ACPI table, updating the package length of the current package.
 *
 * @param pThis                 The ACPI table instance.
 * @param pvData                The data to append.
 * @param cbData                Size of the data in bytes.
 */
DECLINLINE(void) rtAcpiTblAppendData(PRTACPITBLINT pThis, const void *pvData, uint32_t cbData)
{
    uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, cbData);
    if (pb)
    {
        memcpy(pb, pvData, cbData);
        rtAcpiTblUpdatePkgLength(pThis, cbData);
    }
}


/**
 * Appends the given name segment to the destination padding the segment with '_' if the
 * name segment is shorter than 4 characters.
 *
 * @returns Pointer to the character after the given name segment.
 * @param   pbDst               Where to store the name segment.
 * @param   pachNameSeg         The name segment to append.
 */
DECLINLINE(const char *) rtAcpiTblAppendNameSeg(uint8_t *pbDst, const char *pachNameSeg)
{
    Assert(pachNameSeg[0] != '.' && pachNameSeg[0] != '\0');

    uint8_t cch = 1;
    pbDst[0] = pachNameSeg[0];

    for (uint8_t i = 1; i < 4; i++)
    {
        if (   pachNameSeg[cch] != '.'
            && pachNameSeg[cch] != '\0')
        {
            pbDst[i] = pachNameSeg[cch];
            cch++;
        }
        else
            pbDst[i] = '_';
    }

    return &pachNameSeg[cch];
}


/**
 * Appends the given namestring to the ACPI table, updating the package length of the current package
 * and padding the name with _ if too short.
 *
 * @param pThis                 The ACPI table instance.
 * @param pszName               The name string to append.
 */
static void rtAcpiTblAppendNameString(PRTACPITBLINT pThis, const char *pszName)
{
    if (*pszName == '\\')
    {
        /* Root prefix. */
        rtAcpiTblAppendByte(pThis, '\\');
        pszName++;
    }
    else if (*pszName == '^')
    {
        /* PrefixPath */
        do
        {
            rtAcpiTblAppendByte(pThis, '^');
            pszName++;
        }
        while (*pszName == '^');
    }

    /*
     * We need to count the number of segments to decide whether a
     * NameSeg, DualNamePath or MultiNamePath is needed.
     */
    uint8_t cSegments = 1;
    const char *pszTmp = pszName;
    while (*pszTmp != '\0')
    {
        if (*pszTmp++ == '.')
            cSegments++;
    }

    uint32_t cbReq = cSegments * 4 * sizeof(uint8_t);
    if (cSegments == 2)
        cbReq++; /* DualName Prefix */
    else if (cSegments != 1)
        cbReq += 2; /* MultiName prefix + segment count */
    uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, cbReq);
    if (pb)
    {
        if (cSegments == 1)
        {
            rtAcpiTblAppendNameSeg(pb, pszName);
            rtAcpiTblUpdatePkgLength(pThis, 4);
        }
        else if (cSegments == 2)
        {
            *pb++ = ACPI_AML_BYTE_CODE_PREFIX_DUAL_NAME;
            pszName = rtAcpiTblAppendNameSeg(pb, pszName);
            pb += 4;
            Assert(*pszName == '.');
            pszName++;
            pszName = rtAcpiTblAppendNameSeg(pb, pszName);
            Assert(*pszName == '\0'); RT_NOREF(pszName);
            rtAcpiTblUpdatePkgLength(pThis, 1 + 8);
        }
        else
        {
            *pb++ = ACPI_AML_BYTE_CODE_PREFIX_MULTI_NAME;
            *pb++ = cSegments;
            for (uint8_t i = 0; i < cSegments; i++)
            {
                pszName = rtAcpiTblAppendNameSeg(pb, pszName);
                Assert(*pszName == '.' || *pszName == '\0');
                pb += 4;
                pszName++;
            }
            rtAcpiTblUpdatePkgLength(pThis, 2 + cSegments * 4);
        }
    }
}


/**
 * Appends a name segment or the NullName to the given ACPI table.
 *
 * @returns nothing.
 * @param pThis                 The ACPI table instance.
 * @param pszName               The name to append, maximum is 4 chracters. If less than 4 characters
 *                              anything left is padded with _. NULL means append the NullName.
 */
DECLINLINE(void) rtAcpiTblAppendNameSegOrNullName(PRTACPITBLINT pThis, const char *pszName)
{
    if (!pszName)
    {
        uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, 1);
        if (pb)
        {
            *pb = ACPI_AML_BYTE_CODE_PREFIX_NULL_NAME;
            rtAcpiTblUpdatePkgLength(pThis, 1);
        }
    }
    else
    {
        AssertReturnVoidStmt(strlen(pszName) <= 4, pThis->rcErr = VERR_INVALID_PARAMETER);
        uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, 4);
        if (pb)
        {
            rtAcpiTblCopyStringPadWith(pb, 4, pszName, '_');
            rtAcpiTblUpdatePkgLength(pThis, 4);
        }
    }
}


/**
 * Encodes a PkgLength item for the given number.
 *
 * @returns IPRT status code.
 * @param pThis                 The ACPI table instance.
 * @param u64Length             The length to encode.
 */
DECLINLINE(int) rtAcpiTblEncodePkgLength(PRTACPITBLINT pThis, uint64_t u64Length)
{
    AssertReturn(u64Length < RT_BIT_32(28), VERR_BUFFER_OVERFLOW);

    if (u64Length <= 63)
    {
        /* PkgLength only consists of the package lead byte. */
        rtAcpiTblAppendByte(pThis, (u64Length & 0x3f));
    }
    else if (u64Length < RT_BIT_32(12))
    {
        uint8_t abData[2];
        abData[0] = (1 << 6) | (u64Length & 0xf);
        abData[1] = (u64Length >> 4) & 0xff;
        rtAcpiTblAppendData(pThis, &abData[0], sizeof(abData));
    }
    else if (u64Length < RT_BIT_32(20))
    {
        uint8_t abData[3];
        abData[0] = (2 << 6) | (u64Length & 0xf);
        abData[1] = (u64Length >> 4)  & 0xff;
        abData[2] = (u64Length >> 12) & 0xff;
        rtAcpiTblAppendData(pThis, &abData[0], sizeof(abData));
    }
    else if (u64Length < RT_BIT_32(28))
    {
        uint8_t abData[4];
        abData[0] = (3 << 6) | (u64Length & 0xf);
        abData[1] = (u64Length >> 4)  & 0xff;
        abData[2] = (u64Length >> 12) & 0xff;
        abData[3] = (u64Length >> 20) & 0xff;
        rtAcpiTblAppendData(pThis, &abData[0], sizeof(abData));
    }
    else
        AssertReleaseFailed();

    return VINF_SUCCESS;
}


RTDECL(uint8_t) RTAcpiChecksumGenerate(const void *pvData, size_t cbData)
{
    uint8_t const *pbSrc = (uint8_t const *)pvData;
    uint8_t bSum = 0;
    for (size_t i = 0; i < cbData; ++i)
        bSum += pbSrc[i];

    return -bSum;
}


RTDECL(void) RTAcpiTblHdrChecksumGenerate(PACPITBLHDR pTbl, size_t cbTbl)
{
    pTbl->bChkSum = 0;
    pTbl->bChkSum = RTAcpiChecksumGenerate(pTbl, cbTbl);
}


RTDECL(int) RTAcpiTblCreate(PRTACPITBL phAcpiTbl, uint32_t u32TblSig, uint8_t bRevision, const char *pszOemId,
                            const char *pszOemTblId, uint32_t u32OemRevision, const char *pszCreatorId,
                            uint32_t u32CreatorRevision)
{
    AssertPtrReturn(phAcpiTbl,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszOemId,    VERR_INVALID_POINTER);
    AssertPtrReturn(pszOemTblId, VERR_INVALID_POINTER);
    AssertReturn(strlen(pszOemId) <= 6, VERR_INVALID_PARAMETER);
    AssertReturn(strlen(pszOemTblId) <= 8, VERR_INVALID_PARAMETER);
    AssertReturn(!pszCreatorId || strlen(pszCreatorId) <= 4, VERR_INVALID_PARAMETER);

    PRTACPITBLINT pThis = (PRTACPITBLINT)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->pbTblBuf = (uint8_t *)RTMemAlloc(_4K);
        if (pThis->pbTblBuf)
        {
            pThis->pHdr            = (PACPITBLHDR)pThis->pbTblBuf;
            pThis->offTblBuf       = sizeof(*pThis->pHdr);
            pThis->cbTblBuf        = _4K;
            pThis->fFinalized      = false;
            pThis->rcErr           = VINF_SUCCESS;
            pThis->paPkgStack      = NULL;
            pThis->cPkgStackElems  = 0;
            pThis->idxPkgStackElem = 0;

            /* Add the root stack element for the table, aka DefinitionBlock() in ASL. */
            uint32_t const cPkgElemsInitial = 8;
            pThis->paPkgStack = (PRTACPITBLSTACKELEM)RTMemAlloc(cPkgElemsInitial * sizeof(*pThis->paPkgStack));
            if (pThis->paPkgStack)
            {
                pThis->cPkgStackElems = cPkgElemsInitial;

                PRTACPITBLSTACKELEM pStackElem = &pThis->paPkgStack[pThis->idxPkgStackElem];
                pStackElem->offPkgLength       = 0; /* Starts with the header. */
                pStackElem->cbPkg              = sizeof(*pThis->pHdr);
                pStackElem->bOp                = UINT8_MAX;

                /* Init the table header with static things. */
                pThis->pHdr->u32Signature       = u32TblSig;
                pThis->pHdr->bRevision          = bRevision;
                pThis->pHdr->u32OemRevision     = RT_H2LE_U32(u32OemRevision);
                pThis->pHdr->u32CreatorRevision = RT_H2LE_U32(u32CreatorRevision);

                rtAcpiTblCopyStringPadWith(&pThis->pHdr->abOemId[0],     sizeof(pThis->pHdr->abOemId),     pszOemId,    ' ');
                rtAcpiTblCopyStringPadWith(&pThis->pHdr->abOemTblId[0],  sizeof(pThis->pHdr->abOemTblId),  pszOemTblId, ' ');
                rtAcpiTblCopyStringPadWith(&pThis->pHdr->abCreatorId[0], sizeof(pThis->pHdr->abCreatorId),
                                           pszCreatorId ? pszCreatorId : "IPRT", ' ');

                *phAcpiTbl = pThis;
                return VINF_SUCCESS;
            }

            RTMemFree(pThis->pbTblBuf);
        }

        RTMemFree(pThis);
    }

    return VERR_NO_MEMORY;
}


RTDECL(void) RTAcpiTblDestroy(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturnVoid(pThis);

    RTMemFree(pThis->paPkgStack);
    RTMemFree(pThis->pbTblBuf);
    pThis->pHdr            = NULL;
    pThis->pbTblBuf        = NULL;
    pThis->cbTblBuf        = 0;
    pThis->offTblBuf       = 0;
    pThis->paPkgStack      = NULL;
    pThis->cPkgStackElems  = 0;
    pThis->idxPkgStackElem = 0;
    RTMemFree(pThis);
}


RTDECL(int) RTAcpiTblFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);
    AssertReturn(!pThis->fFinalized, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->idxPkgStackElem == 0, VERR_INVALID_STATE); /** @todo Better status code. */
    AssertReturn(pThis->paPkgStack[0].bOp == UINT8_MAX, VERR_INVALID_STATE);

    pThis->pHdr->cbTbl = RT_H2LE_U32(pThis->paPkgStack[0].cbPkg);
    RTAcpiTblHdrChecksumGenerate(pThis->pHdr, pThis->paPkgStack[0].cbPkg);

    pThis->fFinalized = true;
    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTAcpiTblGetSize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, 0);
    AssertRCReturn(pThis->rcErr, 0);
    AssertReturn(pThis->fFinalized, 0);

    return pThis->paPkgStack[0].cbPkg;
}


RTDECL(int) RTAcpiTblDumpToVfsIoStrm(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, RTVFSIOSTREAM hVfsIos)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertRCReturn(pThis->rcErr, 0);
    AssertReturn(enmOutType == RTACPITBLTYPE_AML, VERR_NOT_SUPPORTED);

    return RTVfsIoStrmWrite(hVfsIos, pThis->pbTblBuf, pThis->paPkgStack[0].cbPkg,
                            true /*fBlocking*/, NULL /*pcbWritten*/);
}


RTDECL(int) RTAcpiTblDumpToFile(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, const char *pszFilename)
{
    RTVFSIOSTREAM hVfsIos = NIL_RTVFSIOSTREAM;
    int rc = RTVfsChainOpenIoStream(pszFilename, RTFILE_O_WRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE,
                                    &hVfsIos, NULL /*poffError*/, NULL);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTAcpiTblDumpToVfsIoStrm(hAcpiTbl, enmOutType, hVfsIos);
    RTVfsIoStrmRelease(hVfsIos);
    return rc;
}


RTDECL(int) RTAcpiTblDumpToBufferA(RTACPITBL hAcpiTbl, RTACPITBLTYPE enmOutType, uint8_t **ppbAcpiTbl, size_t *pcbAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(ppbAcpiTbl, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbAcpiTbl, VERR_INVALID_POINTER);
    AssertRCReturn(pThis->rcErr, 0);
    AssertReturn(pThis->fFinalized, VERR_INVALID_STATE);
    AssertReturn(enmOutType == RTACPITBLTYPE_AML, VERR_NOT_SUPPORTED);

    *ppbAcpiTbl = (uint8_t *)RTMemDup(pThis->pbTblBuf, pThis->paPkgStack[0].cbPkg);
    *pcbAcpiTbl = pThis->paPkgStack[0].cbPkg;
    return *ppbAcpiTbl != NULL ? VINF_SUCCESS : VERR_NO_MEMORY;
}


RTDECL(int) RTAcpiTblScopeFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_SCOPE);
}


RTDECL(int) RTAcpiTblScopeStart(RTACPITBL hAcpiTbl, const char *pszName)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_SCOPE);
    rtAcpiTblAppendNameString(pThis, pszName);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblPackageStart(RTACPITBL hAcpiTbl, uint8_t cElements)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_PACKAGE);
    rtAcpiTblAppendByte(pThis, cElements);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblPackageFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_PACKAGE);
}


RTDECL(int) RTAcpiTblDeviceStart(RTACPITBL hAcpiTbl, const char *pszName)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStartExt(pThis, ACPI_AML_BYTE_CODE_EXT_OP_DEVICE);
    rtAcpiTblAppendNameString(pThis, pszName);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblDeviceStartF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTAcpiTblDeviceStartV(hAcpiTbl, pszNameFmt, va);
    va_end(va);
    return rc;
}


RTDECL(int) RTAcpiTblDeviceStartV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va)
{
    char szName[128];
    ssize_t cch = RTStrPrintf2V(&szName[0], sizeof(szName), pszNameFmt, va);
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    return RTAcpiTblDeviceStart(hAcpiTbl, &szName[0]);
}


RTDECL(int) RTAcpiTblDeviceFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_EXT_OP_DEVICE);
}


RTDECL(int) RTAcpiTblProcessorStart(RTACPITBL hAcpiTbl, const char *pszName, uint8_t bProcId, uint32_t u32PBlkAddr,
                                    uint8_t cbPBlk)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStartExt(pThis, ACPI_AML_BYTE_CODE_EXT_OP_PROCESSOR);
    rtAcpiTblAppendNameString(pThis, pszName);
    rtAcpiTblAppendByte(pThis, bProcId);
    rtAcpiTblAppendDword(pThis, u32PBlkAddr);
    rtAcpiTblAppendByte(pThis, cbPBlk);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblProcessorStartF(RTACPITBL hAcpiTbl, uint8_t bProcId, uint32_t u32PBlkAddr, uint8_t cbPBlk,
                                     const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTAcpiTblProcessorStartV(hAcpiTbl, bProcId, u32PBlkAddr, cbPBlk, pszNameFmt, va);
    va_end(va);
    return rc;
}


RTDECL(int) RTAcpiTblProcessorStartV(RTACPITBL hAcpiTbl, uint8_t bProcId, uint32_t u32PBlkAddr, uint8_t cbPBlk,
                                     const char *pszNameFmt, va_list va)
{
    char szName[128];
    ssize_t cch = RTStrPrintf2V(&szName[0], sizeof(szName), pszNameFmt, va);
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    return RTAcpiTblProcessorStart(hAcpiTbl, &szName[0], bProcId, u32PBlkAddr, cbPBlk);
}


RTDECL(int) RTAcpiTblProcessorFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_EXT_OP_PROCESSOR);
}


RTDECL(int) RTAcpiTblMethodStart(RTACPITBL hAcpiTbl, const char *pszName, uint8_t cArgs, uint32_t fFlags, uint8_t uSyncLvl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cArgs < 8, VERR_INVALID_PARAMETER);
    AssertReturn(uSyncLvl < 0x10, VERR_INVALID_PARAMETER);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_METHOD);
    rtAcpiTblAppendNameString(pThis, pszName);

    uint8_t bFlags = cArgs;
    bFlags |= fFlags & RTACPI_METHOD_F_SERIALIZED ? RT_BIT(3) : 0;
    bFlags |= uSyncLvl << 4;

    rtAcpiTblAppendByte(pThis, bFlags);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblMethodFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_METHOD);
}


RTDECL(int) RTAcpiTblNameAppend(RTACPITBL hAcpiTbl, const char *pszName)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_NAME);
    rtAcpiTblAppendNameString(pThis, pszName);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblNullNameAppend(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblAppendByte(pThis, 0x00);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblNameStringAppend(RTACPITBL hAcpiTbl, const char *pszName)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblAppendNameString(pThis, pszName);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblNameStringAppendF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTAcpiTblNameStringAppendV(hAcpiTbl, pszNameFmt, va);
    va_end(va);
    return rc;
}


RTDECL(int) RTAcpiTblNameStringAppendV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va)
{
    char szName[512];
    ssize_t cch = RTStrPrintf2V(&szName[0], sizeof(szName), pszNameFmt, va);
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    return RTAcpiTblNameStringAppend(hAcpiTbl, &szName[0]);
}


RTDECL(int) RTAcpiTblStringAppend(RTACPITBL hAcpiTbl, const char *psz)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_STRING);
    rtAcpiTblAppendData(pThis, psz, (uint32_t)strlen(psz) + 1);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblStringAppendF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...)
{
    va_list va;
    va_start(va, pszNameFmt);
    int rc = RTAcpiTblStringAppendV(hAcpiTbl, pszNameFmt, va);
    va_end(va);
    return rc;
}


RTDECL(int) RTAcpiTblStringAppendV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va)
{
    char szName[512];
    ssize_t cch = RTStrPrintf2V(&szName[0], sizeof(szName), pszNameFmt, va);
    if (cch <= 0)
        return VERR_BUFFER_OVERFLOW;

    return RTAcpiTblStringAppend(hAcpiTbl, &szName[0]);
}


RTDECL(int) RTAcpiTblStringAppendAsUtf16(RTACPITBL hAcpiTbl, const char *psz)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    PRTUTF16 pwsz = NULL;
    size_t cwc = 0;
    int rc = RTStrToUtf16Ex(psz, RTSTR_MAX, &pwsz, 0, &cwc);
    if (RT_SUCCESS(rc))
    {
        RTAcpiTblBufferAppend(hAcpiTbl, pwsz, (cwc + 1) * sizeof(*pwsz));
        RTUtf16Free(pwsz);
    }
    else
        pThis->rcErr = rc;
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblIntegerAppend(RTACPITBL hAcpiTbl, uint64_t u64)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    if (!u64)
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_ZERO);
    else if (u64 == 1)
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_ONE);
    else if (u64 <= UINT8_MAX)
    {
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_BYTE);
        rtAcpiTblAppendByte(pThis, (uint8_t)u64);
    }
    else if (u64 <= UINT16_MAX)
    {
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_WORD);
        rtAcpiTblAppendByte(pThis, (uint8_t)u64);
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 8));
    }
    else if (u64 <= UINT32_MAX)
    {
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_DWORD);
        rtAcpiTblAppendByte(pThis, (uint8_t)u64);
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >>  8));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 16));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 24));
    }
    else
    {
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_QWORD);
        rtAcpiTblAppendByte(pThis, (uint8_t)u64);
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >>  8));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 16));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 24));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 32));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 40));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 48));
        rtAcpiTblAppendByte(pThis, (uint8_t)(u64 >> 56));
    }
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblBufferAppend(RTACPITBL hAcpiTbl, const void *pvBuf, size_t cbBuf)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!cbBuf || RT_VALID_PTR(pvBuf), VERR_INVALID_PARAMETER);
    AssertReturn(cbBuf <= UINT32_MAX, VERR_BUFFER_OVERFLOW);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_BUFFER);
    RTAcpiTblIntegerAppend(hAcpiTbl, cbBuf);
    if (pvBuf)
        rtAcpiTblAppendData(pThis, pvBuf, (uint32_t)cbBuf);
    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_BUFFER);
}


RTDECL(int) RTAcpiTblResourceAppend(RTACPITBL hAcpiTbl, RTACPIRES hAcpiRes)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    const void *pvRes = NULL;
    size_t cbRes = 0;
    int rc = RTAcpiResourceQueryBuffer(hAcpiRes, &pvRes, &cbRes);
    if (RT_SUCCESS(rc))
        rc = RTAcpiTblBufferAppend(pThis, pvRes, cbRes);

    return rc;
}


RTDECL(int) RTAcpiTblStmtSimpleAppend(RTACPITBL hAcpiTbl, RTACPISTMT enmStmt)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    uint8_t bOp;
    bool fExtOp = false;
    switch (enmStmt)
    {
        case kAcpiStmt_Return:     bOp = ACPI_AML_BYTE_CODE_OP_RETURN;      break;
        case kAcpiStmt_Breakpoint: bOp = ACPI_AML_BYTE_CODE_OP_BREAK_POINT; break;
        case kAcpiStmt_Nop:        bOp = ACPI_AML_BYTE_CODE_OP_NOOP;        break;
        case kAcpiStmt_Break:      bOp = ACPI_AML_BYTE_CODE_OP_BREAK;       break;
        case kAcpiStmt_Continue:   bOp = ACPI_AML_BYTE_CODE_OP_CONTINUE;    break;
        case kAcpiStmt_Add:        bOp = ACPI_AML_BYTE_CODE_OP_ADD;         break;
        case kAcpiStmt_Subtract:   bOp = ACPI_AML_BYTE_CODE_OP_SUBTRACT;    break;
        case kAcpiStmt_Multiply:   bOp = ACPI_AML_BYTE_CODE_OP_MULTIPLY;    break;
        case kAcpiStmt_And:        bOp = ACPI_AML_BYTE_CODE_OP_AND;         break;
        case kAcpiStmt_Nand:       bOp = ACPI_AML_BYTE_CODE_OP_NAND;        break;
        case kAcpiStmt_Or:         bOp = ACPI_AML_BYTE_CODE_OP_OR;          break;
        case kAcpiStmt_Xor:        bOp = ACPI_AML_BYTE_CODE_OP_XOR;         break;
        case kAcpiStmt_ShiftLeft:  bOp = ACPI_AML_BYTE_CODE_OP_SHIFT_LEFT;  break;
        case kAcpiStmt_ShiftRight: bOp = ACPI_AML_BYTE_CODE_OP_SHIFT_RIGHT; break;
        case kAcpiStmt_Not:        bOp = ACPI_AML_BYTE_CODE_OP_NOT;         break;
        case kAcpiStmt_Store:      bOp = ACPI_AML_BYTE_CODE_OP_STORE;       break;
        case kAcpiStmt_Index:      bOp = ACPI_AML_BYTE_CODE_OP_INDEX;       break;
        case kAcpiStmt_DerefOf:    bOp = ACPI_AML_BYTE_CODE_OP_DEREF_OF;    break;
        case kAcpiStmt_Notify:     bOp = ACPI_AML_BYTE_CODE_OP_NOTIFY;      break;
        case kAcpiStmt_SizeOf:     bOp = ACPI_AML_BYTE_CODE_OP_SIZE_OF;     break;
        case kAcpiStmt_Increment:  bOp = ACPI_AML_BYTE_CODE_OP_INCREMENT;   break;
        case kAcpiStmt_Decrement:  bOp = ACPI_AML_BYTE_CODE_OP_INCREMENT;   break;
        case kAcpiStmt_CondRefOf:  bOp = ACPI_AML_BYTE_CODE_EXT_OP_COND_REF_OF; fExtOp = true; break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
    if (fExtOp)
        rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_EXT_OP);
    rtAcpiTblAppendByte(pThis, bOp);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblIfStart(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_IF);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblIfFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_IF);
}


RTDECL(int) RTAcpiTblElseStart(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_ELSE);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblElseFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_ELSE);
}


RTDECL(int) RTAcpiTblWhileStart(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStart(pThis, ACPI_AML_BYTE_CODE_OP_WHILE);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblWhileFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_WHILE);
}


RTDECL(int) RTAcpiTblBinaryOpAppend(RTACPITBL hAcpiTbl, RTACPIBINARYOP enmBinaryOp)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    uint8_t bOp;
    switch (enmBinaryOp)
    {
        case kAcpiBinaryOp_LAnd:            bOp = ACPI_AML_BYTE_CODE_OP_LAND;     break;
        case kAcpiBinaryOp_LEqual:          bOp = ACPI_AML_BYTE_CODE_OP_LEQUAL;   break;
        case kAcpiBinaryOp_LGreater:        bOp = ACPI_AML_BYTE_CODE_OP_LGREATER; break;
        case kAcpiBinaryOp_LLess:           bOp = ACPI_AML_BYTE_CODE_OP_LLESS;    break;
        case kAcpiBinaryOp_LGreaterEqual:
        case kAcpiBinaryOp_LLessEqual:
        case kAcpiBinaryOp_LNotEqual:
            bOp = ACPI_AML_BYTE_CODE_OP_LNOT;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
    rtAcpiTblAppendByte(pThis, bOp);
    switch (enmBinaryOp)
    {
        case kAcpiBinaryOp_LGreaterEqual:   bOp = ACPI_AML_BYTE_CODE_OP_LLESS;    break;
        case kAcpiBinaryOp_LLessEqual:      bOp = ACPI_AML_BYTE_CODE_OP_LGREATER; break;
        case kAcpiBinaryOp_LNotEqual:       bOp = ACPI_AML_BYTE_CODE_OP_LEQUAL;   break;
        default:
            bOp = 0x00;
    }
    if (bOp != 0x00)
        rtAcpiTblAppendByte(pThis, bOp);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblArgOpAppend(RTACPITBL hAcpiTbl, uint8_t idArg)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(idArg <= 6, VERR_INVALID_PARAMETER);

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_ARG_0 + idArg);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblLocalOpAppend(RTACPITBL hAcpiTbl, uint8_t idLocal)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(idLocal <= 7, VERR_INVALID_PARAMETER);

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_LOCAL_0 + idLocal);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblUuidAppend(RTACPITBL hAcpiTbl, PCRTUUID pUuid)
{
    /* UUIDs are stored as a buffer object. */
    /** @todo Needs conversion on big endian machines. */
    return RTAcpiTblBufferAppend(hAcpiTbl, &pUuid->au8[0], sizeof(*pUuid));
}


RTDECL(int) RTAcpiTblUuidAppendFromStr(RTACPITBL hAcpiTbl, const char *pszUuid)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    RTUUID Uuid;
    pThis->rcErr = RTUuidFromStr(&Uuid, pszUuid);
    if (RT_SUCCESS(pThis->rcErr))
        return RTAcpiTblUuidAppend(pThis, &Uuid);

    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblOpRegionAppendEx(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOPREGIONSPACE enmSpace)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    uint8_t abOp[2] = { ACPI_AML_BYTE_CODE_PREFIX_EXT_OP, ACPI_AML_BYTE_CODE_EXT_OP_OP_REGION };
    rtAcpiTblAppendData(pThis, &abOp[0], sizeof(abOp));
    rtAcpiTblAppendNameString(pThis, pszName);

    uint8_t bRegionSpace = 0xff;
    switch (enmSpace)
    {
        case kAcpiOperationRegionSpace_SystemMemory:        bRegionSpace = 0x00; break;
        case kAcpiOperationRegionSpace_SystemIo:            bRegionSpace = 0x01; break;
        case kAcpiOperationRegionSpace_PciConfig:           bRegionSpace = 0x02; break;
        case kAcpiOperationRegionSpace_EmbeddedControl:     bRegionSpace = 0x03; break;
        case kAcpiOperationRegionSpace_SmBus:               bRegionSpace = 0x04; break;
        case kAcpiOperationRegionSpace_SystemCmos:          bRegionSpace = 0x05; break;
        case kAcpiOperationRegionSpace_PciBarTarget:        bRegionSpace = 0x06; break;
        case kAcpiOperationRegionSpace_Ipmi:                bRegionSpace = 0x07; break;
        case kAcpiOperationRegionSpace_Gpio:                bRegionSpace = 0x08; break;
        case kAcpiOperationRegionSpace_GenericSerialBus:    bRegionSpace = 0x09; break;
        case kAcpiOperationRegionSpace_Pcc:                 bRegionSpace = 0x0a; break;
        default:
            pThis->rcErr = VERR_INVALID_PARAMETER;
            AssertFailedReturn(pThis->rcErr);
    }
    rtAcpiTblAppendByte(pThis, bRegionSpace);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblOpRegionAppend(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOPREGIONSPACE enmSpace,
                                    uint64_t offRegion, uint64_t cbRegion)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    int rc = RTAcpiTblOpRegionAppendEx(pThis, pszName, enmSpace);
    if (RT_FAILURE(rc))
        return rc;

    RTAcpiTblIntegerAppend(pThis, offRegion);
    RTAcpiTblIntegerAppend(pThis, cbRegion);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblFieldAppend(RTACPITBL hAcpiTbl, const char *pszNameRef, RTACPIFIELDACC enmAcc,
                                 bool fLock, RTACPIFIELDUPDATE enmUpdate, PCRTACPIFIELDENTRY paFields,
                                 uint32_t cFields)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblPkgStartExt(pThis, ACPI_AML_BYTE_CODE_EXT_OP_FIELD);
    rtAcpiTblAppendNameString(pThis, pszNameRef);

    uint8_t fFlags = 0;
    switch (enmAcc)
    {
        case kAcpiFieldAcc_Any:    fFlags = 0; break;
        case kAcpiFieldAcc_Byte:   fFlags = 1; break;
        case kAcpiFieldAcc_Word:   fFlags = 2; break;
        case kAcpiFieldAcc_DWord:  fFlags = 3; break;
        case kAcpiFieldAcc_QWord:  fFlags = 4; break;
        case kAcpiFieldAcc_Buffer: fFlags = 5; break;
        default:
            pThis->rcErr = VERR_INVALID_PARAMETER;
            AssertFailedReturn(pThis->rcErr);
    }
    if (fLock)
        fFlags |= RT_BIT(4);
    switch (enmUpdate)
    {
        case kAcpiFieldUpdate_Preserve:      fFlags |= 0 << 5; break;
        case kAcpiFieldUpdate_WriteAsOnes:   fFlags |= 1 << 5; break;
        case kAcpiFieldUpdate_WriteAsZeroes: fFlags |= 2 << 5; break;
        default:
            pThis->rcErr = VERR_INVALID_PARAMETER;
            AssertFailedReturn(pThis->rcErr);
    }
    rtAcpiTblAppendByte(pThis, fFlags);

    for (uint32_t i = 0; i < cFields; i++)
    {
        rtAcpiTblAppendNameSegOrNullName(pThis, paFields[i].pszName);
        rtAcpiTblEncodePkgLength(pThis, paFields[i].cBits);
    }

    rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_EXT_OP_FIELD);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblExternalAppend(RTACPITBL hAcpiTbl, const char *pszName, RTACPIOBJTYPE enmObjType, uint8_t cArgs)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(cArgs <= 7, VERR_INVALID_PARAMETER);

    uint8_t bObjType;
    switch (enmObjType)
    {
        case kAcpiObjType_Unknown:     bObjType = ACPI_AML_OBJECT_TYPE_UNINIT; break;
        case kAcpiObjType_Int:         bObjType = ACPI_AML_OBJECT_TYPE_INTEGER; break;
        case kAcpiObjType_Str:         bObjType = ACPI_AML_OBJECT_TYPE_STRING; break;
        case kAcpiObjType_Buff:        bObjType = ACPI_AML_OBJECT_TYPE_BUFFER; break;
        case kAcpiObjType_Pkg:         bObjType = ACPI_AML_OBJECT_TYPE_PACKAGE; break;
        case kAcpiObjType_FieldUnit:   bObjType = ACPI_AML_OBJECT_TYPE_FIELD_UNIT; break;
        case kAcpiObjType_Device:      bObjType = ACPI_AML_OBJECT_TYPE_DEVICE; break;
        case kAcpiObjType_Event:       bObjType = ACPI_AML_OBJECT_TYPE_EVENT; break;
        case kAcpiObjType_Method:      bObjType = ACPI_AML_OBJECT_TYPE_METHOD; break;
        case kAcpiObjType_MutexObj:    bObjType = ACPI_AML_OBJECT_TYPE_MUTEX; break;
        case kAcpiObjType_OpRegion:    bObjType = ACPI_AML_OBJECT_TYPE_OPERATION_REGION; break;
        case kAcpiObjType_PowerRes:    bObjType = ACPI_AML_OBJECT_TYPE_POWER_RESOURCE; break;
        case kAcpiObjType_ThermalZone: bObjType = ACPI_AML_OBJECT_TYPE_THERMAL_ZONE; break;
        case kAcpiObjType_BuffField:   bObjType = ACPI_AML_OBJECT_TYPE_BUFFER_FIELD; break;
        case kAcpiObjType_Processor:   bObjType = ACPI_AML_OBJECT_TYPE_PROCESSOR; break;
        default:
            pThis->rcErr = VERR_INVALID_PARAMETER;
            AssertFailedReturn(pThis->rcErr);
    }

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_OP_EXTERNAL);
    rtAcpiTblAppendNameString(pThis, pszName);
    rtAcpiTblAppendByte(pThis, bObjType);
    rtAcpiTblAppendByte(pThis, cArgs);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblCreateFromVfsIoStrm(PRTACPITBL phAcpiTbl, RTVFSIOSTREAM hVfsIos, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phAcpiTbl, VERR_INVALID_POINTER);
    AssertReturn(hVfsIos != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);

    RT_NOREF(pErrInfo, enmInType);
#if 0
    if (enmInType == RTACPITBLTYPE_AML)
        return rtAcpiTblLoadAml(phAcpiTbl, hVfsIos, pErrInfo);
#endif

    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTAcpiTblConvertFromVfsIoStrm(RTVFSIOSTREAM hVfsIosOut, RTACPITBLTYPE enmOutType,
                                          RTVFSIOSTREAM hVfsIosIn, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo)
{
    AssertReturn(hVfsIosOut != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);
    AssertReturn(hVfsIosIn  != NIL_RTVFSIOSTREAM, VERR_INVALID_HANDLE);

    if (enmInType == RTACPITBLTYPE_AML && enmOutType == RTACPITBLTYPE_ASL)
        return rtAcpiTblConvertFromAmlToAsl(hVfsIosOut, hVfsIosIn, pErrInfo);
    else if (enmInType == RTACPITBLTYPE_ASL && enmOutType == RTACPITBLTYPE_AML)
        return rtAcpiTblConvertFromAslToAml(hVfsIosOut, hVfsIosIn, pErrInfo);

    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTAcpiTblCreateFromFile(PRTACPITBL phAcpiTbl, const char *pszFilename, RTACPITBLTYPE enmInType, PRTERRINFO pErrInfo)
{
    RTVFSIOSTREAM hVfsIos = NIL_RTVFSIOSTREAM;
    int rc = RTVfsChainOpenIoStream(pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE,
                                    &hVfsIos, NULL /*poffError*/, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTAcpiTblCreateFromVfsIoStrm(phAcpiTbl, hVfsIos, enmInType, pErrInfo);
    RTVfsIoStrmRelease(hVfsIos);
    return rc;
}


/**
 * Ensures there is at least the given amount of space in the given ACPI resource.
 *
 * @returns Pointer to the free buffer space or NULL if out of memory.
 * @param   pThis               The ACPI resource instance.
 * @param   cbReq               Number of free bytes required.
 */
static uint8_t *rtAcpiResBufEnsureSpace(PRTACPIRESINT pThis, uint32_t cbReq)
{
    if (RT_LIKELY(pThis->cbResBuf - pThis->offResBuf >= cbReq))
    {
        uint8_t *pb = &pThis->pbResBuf[pThis->offResBuf];
        pThis->offResBuf += cbReq;
        return pb;
    }

    size_t const cbNew = RT_ALIGN_Z(pThis->cbResBuf + cbReq, _4K);
    uint8_t *pbNew = (uint8_t *)RTMemRealloc(pThis->pbResBuf, cbNew);
    if (RT_UNLIKELY(!pbNew))
    {
        pThis->rcErr = VERR_NO_MEMORY;
        return NULL;
    }

    pThis->pbResBuf = pbNew;
    pThis->cbResBuf = cbNew;

    uint8_t *pb = &pThis->pbResBuf[pThis->offResBuf];
    pThis->offResBuf += cbReq;
    return pb;
}


/**
 * Encodes an ACPI 16-bit integer in the given byte buffer.
 *
 * @returns Pointer to after the encoded integer.
 * @param   pb                  Where to encode the integer into.
 * @param   u16                 The 16-bit unsigned integere to encode.
 */
DECLINLINE(uint8_t *) rtAcpiResEncode16BitInteger(uint8_t *pb, uint16_t u16)
{
    *pb++ = (uint8_t)u16;
    *pb++ = (uint8_t)(u16 >>  8);
    return pb;
}


/**
 * Encodes an ACPI 32-bit integer in the given byte buffer.
 *
 * @returns Pointer to after the encoded integer.
 * @param   pb                  Where to encode the integer into.
 * @param   u32                 The 32-bit unsigned integere to encode.
 */
DECLINLINE(uint8_t *) rtAcpiResEncode32BitInteger(uint8_t *pb, uint32_t u32)
{
    *pb++ = (uint8_t)u32;
    *pb++ = (uint8_t)(u32 >>  8);
    *pb++ = (uint8_t)(u32 >> 16);
    *pb++ = (uint8_t)(u32 >> 24);
    return pb;
}

/**
 * Encodes an ACPI 64-bit integer in the given byte buffer.
 *
 * @returns Pointer to after the encoded integer.
 * @param   pb                  Where to encode the integer into.
 * @param   u64                 The 64-bit unsigned integere to encode.
 */

DECLINLINE(uint8_t *) rtAcpiResEncode64BitInteger(uint8_t *pb, uint64_t u64)
{
    *pb++ = (uint8_t)u64;
    *pb++ = (uint8_t)(u64 >>  8);
    *pb++ = (uint8_t)(u64 >> 16);
    *pb++ = (uint8_t)(u64 >> 24);
    *pb++ = (uint8_t)(u64 >> 32);
    *pb++ = (uint8_t)(u64 >> 40);
    *pb++ = (uint8_t)(u64 >> 48);
    *pb++ = (uint8_t)(u64 >> 56);
    return pb;
}


RTDECL(int) RTAcpiResourceCreate(PRTACPIRES phAcpiRes)
{
    AssertPtrReturn(phAcpiRes, VERR_INVALID_POINTER);

    PRTACPIRESINT pThis = (PRTACPIRESINT)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->pbResBuf = (uint8_t *)RTMemAlloc(64);
        if (pThis->pbResBuf)
        {
            pThis->offResBuf = 0;
            pThis->cbResBuf  = 64;
            pThis->fSealed   = false;
            pThis->rcErr     = VINF_SUCCESS;

            *phAcpiRes = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
    }

    return VERR_NO_MEMORY;
}


RTDECL(void) RTAcpiResourceDestroy(RTACPIRES hAcpiRes)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturnVoid(pThis);

    RTMemFree(pThis->pbResBuf);
    pThis->pbResBuf        = NULL;
    pThis->cbResBuf        = 0;
    pThis->offResBuf       = 0;
    RTMemFree(pThis);
}


RTDECL(void) RTAcpiResourceReset(RTACPIRES hAcpiRes)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturnVoid(pThis);

    pThis->offResBuf = 0;
    pThis->fSealed   = false;
    pThis->rcErr     = VINF_SUCCESS;
}


RTDECL(uint32_t) RTAcpiResourceGetOffset(RTACPIRES hAcpiRes)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertReturn(pThis, UINT32_MAX);
    AssertRCReturn(pThis->rcErr, UINT32_MAX);

    return pThis->offResBuf;
}


RTDECL(int) RTAcpiResourceSeal(RTACPIRES hAcpiRes)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    /* Add the end tag. */
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 2);
    if (!pb)
        return VERR_NO_MEMORY;

    *pb++ = ACPI_RSRCS_TAG_END;
    /*
     * Generate checksum, we could just write 0 here which will be treated as checksum operation succeeded,
     * but having this might catch some bugs.
     *
     * Checksum algorithm is the same as with the ACPI tables.
     */
    *pb = RTAcpiChecksumGenerate(pThis->pbResBuf, pThis->offResBuf - 1); /* Exclude the checksum field. */

    pThis->fSealed = true;
    return VINF_SUCCESS;
}


RTDECL(int) RTAcpiResourceQueryBuffer(RTACPIRES hAcpiRes, const void **ppvRes, size_t *pcbRes)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    *ppvRes = pThis->pbResBuf;
    *pcbRes = pThis->offResBuf;
    return VINF_SUCCESS;
}


RTDECL(int) RTAcpiResourceAdd32BitFixedMemoryRange(RTACPIRES hAcpiRes, uint32_t u32AddrBase, uint32_t cbRange,
                                                   bool fRw)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 12);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_32BIT_FIXED_MEMORY_RANGE; /* Tag          */
    pb[1] = 9;                                                                /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = fRw ? 1 : 0;                                                      /* Information  */
    rtAcpiResEncode32BitInteger(&pb[4], u32AddrBase);
    rtAcpiResEncode32BitInteger(&pb[8], cbRange);
    return VINF_SUCCESS;
}


RTDECL(int) RTAcpiResourceAddExtendedInterrupt(RTACPIRES hAcpiRes, bool fConsumer, bool fEdgeTriggered, bool fActiveLow, bool fShared,
                                               bool fWakeCapable, uint8_t cIntrs, uint32_t *pau32Intrs)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 2 + cIntrs * sizeof(uint32_t));
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_EXTENDED_INTERRUPT;       /* Tag          */
    rtAcpiResEncode16BitInteger(&pb[1], 2 + cIntrs * sizeof(uint32_t));       /* Length[15:0] */
    pb[3] =   (fConsumer      ? ACPI_RSRCS_EXT_INTR_VEC_F_CONSUMER       : ACPI_RSRCS_EXT_INTR_VEC_F_PRODUCER)
            | (fEdgeTriggered ? ACPI_RSRCS_EXT_INTR_VEC_F_EDGE_TRIGGERED : ACPI_RSRCS_EXT_INTR_VEC_F_LEVEL_TRIGGERED)
            | (fActiveLow     ? ACPI_RSRCS_EXT_INTR_VEC_F_ACTIVE_LOW     : ACPI_RSRCS_EXT_INTR_VEC_F_ACTIVE_HIGH)
            | (fShared        ? ACPI_RSRCS_EXT_INTR_VEC_F_SHARED         : ACPI_RSRCS_EXT_INTR_VEC_F_EXCLUSIVE)
            | (fWakeCapable   ? ACPI_RSRCS_EXT_INTR_VEC_F_WAKE_CAP       : ACPI_RSRCS_EXT_INTR_VEC_F_NOT_WAKE_CAP);
    pb[4] = cIntrs;
    pb = &pb[5];
    for (uint32_t i = 0; i < cIntrs; i++)
        pb = rtAcpiResEncode32BitInteger(pb, pau32Intrs[i]);

    return VINF_SUCCESS;
}


/**
 * Common worker for encoding a new quad word (64-bit) address range.
 *
 * @returns IPRT status code
 * @retval  VERR_NO_MEMORY if not enough memory could be reserved in the ACPI resource descriptor.
 * @param   pThis               The ACPI resource instance.
 * @param   bType               The ACPI address range type.
 * @param   fAddrSpace          Combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX.
 * @param   fType               The range flags returned from rtAcpiResourceMemoryRangeToTypeFlags().
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
static int rtAcpiResourceAddQWordAddressRange(PRTACPIRESINT pThis, uint8_t bType, uint32_t fAddrSpace, uint8_t fType,
                                              uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                              uint64_t u64Granularity, uint64_t u64Length,
                                              const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    uint32_t cchRsrcSrc = pszRsrcSrc ? (uint32_t)strlen(pszRsrcSrc) + 2 : 0;
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 43 + cchRsrcSrc);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_QWORD_ADDR_SPACE;         /* Tag          */
    pb[1] = 43;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = bType;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_PRODUCER        ? ACPI_RSRCS_ADDR_SPACE_F_PRODUCER        : ACPI_RSRCS_ADDR_SPACE_F_CONSUMER);
    pb[5] = fType;

    pb = rtAcpiResEncode64BitInteger(&pb[6], u64Granularity);
    pb = rtAcpiResEncode64BitInteger(pb,     u64AddrMin);
    pb = rtAcpiResEncode64BitInteger(pb,     u64AddrMax);
    pb = rtAcpiResEncode64BitInteger(pb,     u64OffTrans);
    pb = rtAcpiResEncode64BitInteger(pb,     u64Length);
    if (pszRsrcSrc)
    {
        *pb++ = bRsrcIndex;
        memcpy(pb, pszRsrcSrc, cchRsrcSrc + 1);
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for encoding a new double word (32-bit) address range.
 *
 * @returns IPRT status code
 * @retval  VERR_NO_MEMORY if not enough memory could be reserved in the ACPI resource descriptor.
 * @param   pThis               The ACPI resource instance.
 * @param   bType               The ACPI address range type.
 * @param   fAddrSpace          Combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX.
 * @param   fType               The range flags returned from rtAcpiResourceMemoryRangeToTypeFlags().
 * @param   u32AddrMin          The start address of the memory range.
 * @param   u32AddrMax          Last valid address of the range.
 * @param   u32OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u32Granularity      The access granularity of the range in bytes.
 * @param   u32Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
static int rtAcpiResourceAddDWordAddressRange(PRTACPIRESINT pThis, uint8_t bType, uint32_t fAddrSpace, uint8_t fType,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length,
                                              const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    uint32_t cchRsrcSrc = pszRsrcSrc ? (uint32_t)strlen(pszRsrcSrc) + 2 : 0;
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 23);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_DWORD_ADDR_SPACE;         /* Tag          */
    pb[1] = 23;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = bType;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_PRODUCER        ? ACPI_RSRCS_ADDR_SPACE_F_PRODUCER        : ACPI_RSRCS_ADDR_SPACE_F_CONSUMER);
    pb[5] = fType;

    pb = rtAcpiResEncode32BitInteger(&pb[6], u32Granularity);
    pb = rtAcpiResEncode32BitInteger(pb,     u32AddrMin);
    pb = rtAcpiResEncode32BitInteger(pb,     u32AddrMax);
    pb = rtAcpiResEncode32BitInteger(pb,     u32OffTrans);
    pb = rtAcpiResEncode32BitInteger(pb,     u32Length);
    if (pszRsrcSrc)
    {
        *pb++ = bRsrcIndex;
        memcpy(pb, pszRsrcSrc, cchRsrcSrc + 1);
    }
    return VINF_SUCCESS;
}


/**
 * Common worker for encoding a new word (16-bit) address range.
 *
 * @returns IPRT status code
 * @retval  VERR_NO_MEMORY if not enough memory could be reserved in the ACPI resource descriptor.
 * @param   pThis               The ACPI resource instance.
 * @param   bType               The ACPI address range type.
 * @param   fAddrSpace          Combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX.
 * @param   fType               The range flags returned from rtAcpiResourceMemoryRangeToTypeFlags().
 * @param   u16AddrMin          The start address of the memory range.
 * @param   u16AddrMax          Last valid address of the range.
 * @param   u16OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u16Granularity      The access granularity of the range in bytes.
 * @param   u16Length           Length of the memory range in bytes.
 * @param   pszRsrcSrc          Name of the device object that produces the the descriptor consumed by the device, optional.
 *                              NULL means the device consumes the resource out of a global pool.
 * @param   bRsrcIndex          Index into the resource descriptor this device consumes from. Ignored if pszRsrcSrc is NULL.
 */
static int rtAcpiResourceAddWordAddressRange(PRTACPIRESINT pThis, uint8_t bType, uint32_t fAddrSpace, uint8_t fType,
                                              uint16_t u16AddrMin, uint16_t u16AddrMax, uint16_t u16OffTrans,
                                              uint16_t u16Granularity, uint16_t u16Length,
                                              const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    uint32_t cchRsrcSrc = pszRsrcSrc ? (uint32_t)strlen(pszRsrcSrc) + 2 : 0;
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 13 + cchRsrcSrc);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_WORD_ADDR_SPACE;          /* Tag          */
    pb[1] = 13;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = bType;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_PRODUCER        ? ACPI_RSRCS_ADDR_SPACE_F_PRODUCER        : ACPI_RSRCS_ADDR_SPACE_F_CONSUMER);
    pb[5] = fType;

    pb = rtAcpiResEncode16BitInteger(&pb[6], u16Granularity);
    pb = rtAcpiResEncode16BitInteger(pb,     u16AddrMin);
    pb = rtAcpiResEncode16BitInteger(pb,     u16AddrMax);
    pb = rtAcpiResEncode16BitInteger(pb,     u16OffTrans);
    pb = rtAcpiResEncode16BitInteger(pb,     u16Length);
    if (pszRsrcSrc)
    {
        *pb++ = bRsrcIndex;
        memcpy(pb, pszRsrcSrc, cchRsrcSrc + 1);
    }

    return VINF_SUCCESS;
}


/**
 * Converts the given cacheability, range type and R/W flag to the ACPI resource flags.
 *
 * @returns Converted ACPI resource flags.
 * @param   enmCacheability     The cacheability enum to convert.
 * @param   enmType             THe memory range type enum to convert.
 * @param   fRw                 The read/write flag.
 * @param   fStatic             Static/Translation type flag.
 */
DECLINLINE(uint8_t) rtAcpiResourceMemoryRangeToTypeFlags(RTACPIRESMEMRANGECACHEABILITY enmCacheability, RTACPIRESMEMRANGETYPE enmType,
                                                         bool fRw, bool fStatic)
{
    uint8_t fType =   (fRw ? ACPI_RSRCS_ADDR_SPACE_MEM_F_RW : ACPI_RSRCS_ADDR_SPACE_MEM_F_RO)
                    | (fStatic ? ACPI_RSRCS_ADDR_SPACE_MEM_F_TYPE_STATIC : ACPI_RSRCS_ADDR_SPACE_MEM_F_TYPE_TRANSLATION);

    switch (enmCacheability)
    {
        case kAcpiResMemRangeCacheability_NonCacheable:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_NON_CACHEABLE;
            break;
        case kAcpiResMemRangeCacheability_Cacheable:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE;
            break;
        case kAcpiResMemRangeCacheability_CacheableWriteCombining:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE_WR_COMB;
            break;
        case kAcpiResMemRangeCacheability_CacheablePrefetchable:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE_PREFETCHABLE;
            break;
        case kAcpiResMemRangeCacheability_Invalid:
        default:
            AssertFailedReturn(0);
    }

    switch (enmType)
    {
        case kAcpiResMemType_Memory:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_MEMORY;
            break;
        case kAcpiResMemType_Reserved:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_RESERVED;
            break;
        case kAcpiResMemType_Acpi:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_ACPI;
            break;
        case kAcpiResMemType_Nvs:
            fType |= ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_NVS;
            break;
        case kAcpiResMemType_Invalid:
        default:
            AssertFailedReturn(0);
    }

    return fType;
}


RTDECL(int) RTAcpiResourceAddQWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                              uint64_t u64Granularity, uint64_t u64Length)
{
    return RTAcpiResourceAddQWordMemoryRangeEx(hAcpiRes,  enmCacheability, enmType,  fRw, true /*fStatic*/, fAddrSpace,
                                               u64AddrMin, u64AddrMax, u64OffTrans, u64Granularity, u64Length,
                                               NULL /*pszRsrcSrc*/, 0 /*bRsrcIndex*/);
}


RTDECL(int) RTAcpiResourceAddQWordMemoryRangeEx(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                                RTACPIRESMEMRANGETYPE enmType, bool fRw, bool fStatic, uint32_t fAddrSpace,
                                                uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                                uint64_t u64Granularity, uint64_t u64Length,
                                                const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmCacheability != kAcpiResMemRangeCacheability_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmType != kAcpiResMemType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceMemoryRangeToTypeFlags(enmCacheability, enmType, fRw, fStatic);
    return rtAcpiResourceAddQWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_MEMORY, fAddrSpace, fType,
                                              u64AddrMin, u64AddrMax, u64OffTrans, u64Granularity, u64Length,
                                              pszRsrcSrc, bRsrcIndex);
}


RTDECL(int) RTAcpiResourceAddDWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length)
{
    return RTAcpiResourceAddDWordMemoryRangeEx(hAcpiRes, enmCacheability, enmType, fRw, true /*fStatic*/, fAddrSpace,
                                               u32AddrMin, u32AddrMax, u32OffTrans, u32Granularity, u32Length,
                                               NULL /*pszRsrcSrc*/, 0 /*bRsrcIndex*/);
}


RTDECL(int) RTAcpiResourceAddDWordMemoryRangeEx(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                                RTACPIRESMEMRANGETYPE enmType, bool fRw, bool fStatic, uint32_t fAddrSpace,
                                                uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                                uint32_t u32Granularity, uint32_t u32Length,
                                                const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmCacheability != kAcpiResMemRangeCacheability_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmType != kAcpiResMemType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceMemoryRangeToTypeFlags(enmCacheability, enmType, fRw, fStatic);
    return rtAcpiResourceAddDWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_MEMORY, fAddrSpace, fType,
                                              u32AddrMin, u32AddrMax, u32OffTrans, u32Granularity, u32Length,
                                              pszRsrcSrc, bRsrcIndex);
}


/**
 * Converts the given I/O type and range flag to the ACPI resource flags.
 *
 * @returns Converted ACPI resource flags.
 * @param   enmIoType           The I/O type enum to convert.
 * @param   enmIoRange          The I/O range enum to convert.
 */
DECLINLINE(uint8_t) rtAcpiResourceIoRangeToTypeFlags(RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange)
{
    uint8_t fType = 0;
    switch (enmIoType)
    {
        case kAcpiResIoRangeType_Static:
            fType = ACPI_RSRCS_ADDR_SPACE_IO_F_TYPE_STATIC;
            break;
        case kAcpiResIoRangeType_Translation_Sparse:
            fType = ACPI_RSRCS_ADDR_SPACE_IO_F_TYPE_TRANSLATION | ACPI_RSRCS_ADDR_SPACE_IO_F_TRANSLATION_SPARSE;
            break;
        case kAcpiResIoRangeType_Translation_Dense:
            fType = ACPI_RSRCS_ADDR_SPACE_IO_F_TYPE_TRANSLATION | ACPI_RSRCS_ADDR_SPACE_IO_F_TRANSLATION_DENSE;
            break;
        case kAcpiResIoRangeType_Invalid:
        default:
            AssertFailedReturn(0);
    }

    switch (enmIoRange)
    {
        case kAcpiResIoRange_NonIsaOnly:
            fType |= ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_NON_ISA_ONLY;
            break;
        case kAcpiResIoRange_IsaOnly:
            fType |= ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_ISA_ONLY;
            break;
        case kAcpiResIoRange_Whole:
            fType |= ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_WHOLE;
            break;
        case kAcpiResIoRange_Invalid:
        default:
            AssertFailedReturn(0);
    }

    return fType;
}


RTDECL(int) RTAcpiResourceAddQWordIoRange(RTACPIRES hAcpiRes, RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange,
                                          uint32_t fAddrSpace, uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                          uint64_t u64Granularity, uint64_t u64Length)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmIoType != kAcpiResIoRangeType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmIoRange != kAcpiResIoRange_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceIoRangeToTypeFlags(enmIoType, enmIoRange);
    return rtAcpiResourceAddQWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_IO, fAddrSpace, fType,
                                              u64AddrMin, u64AddrMax, u64OffTrans, u64Granularity, u64Length,
                                              NULL /*pszRsrcSrc*/, 0 /*bRsrcIndex*/);
}


RTDECL(int) RTAcpiResourceAddWordIoRangeEx(RTACPIRES hAcpiRes, RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange,
                                           uint32_t fAddrSpace, uint16_t u16AddrMin, uint16_t u16AddrMax, uint64_t u16OffTrans,
                                           uint16_t u16Granularity, uint16_t u16Length, const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmIoType != kAcpiResIoRangeType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmIoRange != kAcpiResIoRange_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceIoRangeToTypeFlags(enmIoType, enmIoRange);
    return rtAcpiResourceAddWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_IO, fAddrSpace, fType,
                                             u16AddrMin, u16AddrMax, u16OffTrans, u16Granularity, u16Length,
                                             pszRsrcSrc, bRsrcIndex);
}


RTDECL(int) RTAcpiResourceAddWordBusNumber(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                           uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length)
{
    return RTAcpiResourceAddWordBusNumberEx(hAcpiRes, fAddrSpace, u16BusMin, u16BusMax, u16OffTrans, u16Granularity, u16Length,
                                            NULL /*pszRsrcSrc*/, 0 /*bRsrcIndex*/);
}


RTDECL(int) RTAcpiResourceAddWordBusNumberEx(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                             uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length,
                                             const char *pszRsrcSrc, uint8_t bRsrcIndex)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    return rtAcpiResourceAddWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_BUS_NUM_RANGE, fAddrSpace, 0 /*fType*/,
                                             u16BusMin, u16BusMax, u16OffTrans, u16Granularity, u16Length,
                                             pszRsrcSrc, bRsrcIndex);
}


RTDECL(int) RTAcpiResourceAddIo(RTACPIRES hAcpiRes, RTACPIRESIODECODETYPE enmDecode, uint16_t u16AddrMin, uint16_t u16AddrMax,
                                uint8_t u8AddrAlignment, uint8_t u8RangeLength)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 8);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_SMALL_TYPE | (ACPI_RSRCS_ITEM_IO << 3) | 7; /* Tag */
    pb[1] = enmDecode == kAcpiResIoDecodeType_Decode10 ? 0 : 1;
    rtAcpiResEncode16BitInteger(&pb[2], u16AddrMin);
    rtAcpiResEncode16BitInteger(&pb[4], u16AddrMax);
    pb[6] = u8AddrAlignment;
    pb[7] = u8RangeLength;

    return VINF_SUCCESS;
}


RTDECL(int) RTAcpiResourceAddIrq(RTACPIRES hAcpiRes, bool fEdgeTriggered, bool fActiveLow, bool fShared,
                                 bool fWakeCapable, uint16_t bmIntrs)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    bool fDefaultCfg = fEdgeTriggered && !fActiveLow && !fShared && !fWakeCapable;
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 2 + (fDefaultCfg ? 0 : 1));
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_SMALL_TYPE | (ACPI_RSRCS_ITEM_IRQ << 3) | (fDefaultCfg ? 2 : 3); /* Tag */
    rtAcpiResEncode16BitInteger(&pb[1], bmIntrs);
    if (!fDefaultCfg)
        pb[3] =   (fEdgeTriggered ? ACPI_RSRCS_IRQ_F_EDGE_TRIGGERED : ACPI_RSRCS_IRQ_F_LEVEL_TRIGGERED)
                | (fActiveLow     ? ACPI_RSRCS_IRQ_F_ACTIVE_LOW     : ACPI_RSRCS_IRQ_F_ACTIVE_HIGH)
                | (fShared        ? ACPI_RSRCS_IRQ_F_SHARED         : ACPI_RSRCS_IRQ_F_EXCLUSIVE)
                | (fWakeCapable   ? ACPI_RSRCS_IRQ_F_WAKE_CAP       : ACPI_RSRCS_IRQ_F_NOT_WAKE_CAP);

    return VINF_SUCCESS;
}


RTDECL(int) RTAcpiResourceAddDma(RTACPIRES hAcpiRes, RTACPIRESDMACHANSPEED enmChanSpeed, bool fBusMaster,
                                 RTACPIRESDMATRANSFERTYPE enmTransferType, uint8_t bmChannels)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmChanSpeed != kAcpiResDmaChanSpeed_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmTransferType != kAcpiResDmaTransferType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3);
    if (!pb)
        return VERR_NO_MEMORY;

    uint8_t fSpeed = 0;
    switch (enmChanSpeed)
    {
        case kAcpiResDmaChanSpeed_Compatibility: fSpeed = 0; break;
        case kAcpiResDmaChanSpeed_TypeA:         fSpeed = 1; break;
        case kAcpiResDmaChanSpeed_TypeB:         fSpeed = 2; break;
        case kAcpiResDmaChanSpeed_TypeF:         fSpeed = 3; break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    uint8_t fTransferType = 0;
    switch (enmTransferType)
    {
        case kAcpiResDmaTransferType_8Bit:       fTransferType = 0; break;
        case kAcpiResDmaTransferType_8Bit_16Bit: fTransferType = 1; break;
        case kAcpiResDmaTransferType_16Bit:      fTransferType = 2; break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    pb[0] = ACPI_RSRCS_SMALL_TYPE | (ACPI_RSRCS_ITEM_DMA << 3) | 2; /* Tag */
    pb[1] = bmChannels;
    pb[2] =   fSpeed << 5
            | (fBusMaster ? RT_BIT(2) : 0)
            | fTransferType;

    return VINF_SUCCESS;
}
