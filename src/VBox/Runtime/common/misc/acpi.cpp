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
#include <iprt/uuid.h>

#include <iprt/formats/acpi-aml.h>
#include <iprt/formats/acpi-resources.h>


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
    /** Pointer to the table buffer memory where the PkgLength object starts. */
    uint8_t                     *pbPkgLength;
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
 * @param pbPkgBuf              The Start of the package buffer.
 */
static int rtAcpiTblPkgAppendEx(PRTACPITBLINT pThis, uint8_t bOp, uint8_t *pbPkgBuf)
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
    pStackElem->pbPkgLength = pbPkgBuf;
    pStackElem->cbPkg       = 0;
    pStackElem->bOp         = bOp;
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
    return rtAcpiTblPkgAppendEx(pThis, bOp, pbPkg + 1);
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
    return rtAcpiTblPkgAppendEx(pThis, bOp, pbPkg + 2);
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
    uint8_t  *pbPkgLength = pPkgElem->pbPkgLength;
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
 * Appends the given namestring to the ACPI table, updating the package length of the current package
 * and padding the name with _ if too short.
 *
 * @param pThis                 The ACPI table instance.
 * @param pszName               The name to append, maximum is 4 bytes (or 5 if \\ is the first character).
 */
DECLINLINE(void) rtAcpiTblAppendNameString(PRTACPITBLINT pThis, const char *pszName)
{
    uint32_t cbName = *pszName == '\\' ? 5 : 4;
    uint8_t *pb = rtAcpiTblBufEnsureSpace(pThis, cbName);
    if (pb)
    {
        rtAcpiTblCopyStringPadWith(pb, cbName, pszName, '_');
        rtAcpiTblUpdatePkgLength(pThis, cbName);
    }
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
                pStackElem->pbPkgLength        = pThis->pbTblBuf; /* Starts with the header. */
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


RTDECL(int) RTAcpiTblDumpToVfsIoStrm(RTACPITBL hAcpiTbl, RTVFSIOSTREAM hVfsIos)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertRCReturn(pThis->rcErr, 0);

    return RTVfsIoStrmWrite(hVfsIos, pThis->pbTblBuf, pThis->paPkgStack[0].cbPkg,
                            true /*fBlocking*/, NULL /*pcbWritten*/);
}


RTDECL(int) RTAcpiTblDumpToFile(RTACPITBL hAcpiTbl, const char *pszFilename)
{
    RTVFSIOSTREAM hVfsIos = NIL_RTVFSIOSTREAM;
    int rc = RTVfsChainOpenIoStream(pszFilename, RTFILE_O_WRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE,
                                    &hVfsIos, NULL /*poffError*/, NULL);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTAcpiTblDumpToVfsIoStrm(hAcpiTbl, hVfsIos);
    RTVfsIoStrmRelease(hVfsIos);
    return rc;
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
    char szName[5];
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


RTDECL(int) RTAcpiTblStringAppend(RTACPITBL hAcpiTbl, const char *psz)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    rtAcpiTblAppendByte(pThis, ACPI_AML_BYTE_CODE_PREFIX_STRING);
    rtAcpiTblAppendData(pThis, psz, (uint32_t)strlen(psz) + 1);
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
    switch (enmStmt)
    {
        case kAcpiStmt_Return:     bOp = ACPI_AML_BYTE_CODE_OP_RETURN;      break;
        case kAcpiStmt_Breakpoint: bOp = ACPI_AML_BYTE_CODE_OP_BREAK_POINT; break;
        case kAcpiStmt_Nop:        bOp = ACPI_AML_BYTE_CODE_OP_NOOP;        break;
        case kAcpiStmt_Break:      bOp = ACPI_AML_BYTE_CODE_OP_BREAK;       break;
        case kAcpiStmt_Continue:   bOp = ACPI_AML_BYTE_CODE_OP_CONTINUE;    break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
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

    /* Makes only sense inside an IfOp package. */
    AssertReturn(pThis->paPkgStack[pThis->idxPkgStackElem].bOp == ACPI_AML_BYTE_CODE_OP_IF, VERR_INVALID_STATE);

    rtAcpiTblPkgStartExt(pThis, ACPI_AML_BYTE_CODE_OP_ELSE);
    return pThis->rcErr;
}


RTDECL(int) RTAcpiTblElseFinalize(RTACPITBL hAcpiTbl)
{
    PRTACPITBLINT pThis = hAcpiTbl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    return rtAcpiTblPkgFinish(pThis, ACPI_AML_BYTE_CODE_OP_ELSE);
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
 */
static int rtAcpiResourceAddQWordAddressRange(PRTACPIRESINT pThis, uint8_t bType, uint32_t fAddrSpace, uint8_t fType,
                                              uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                              uint64_t u64Granularity, uint64_t u64Length)
{
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 43);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_QWORD_ADDR_SPACE;         /* Tag          */
    pb[1] = 43;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = bType;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE);
    pb[5] = fType;

    pb = rtAcpiResEncode64BitInteger(&pb[6], u64Granularity);
    pb = rtAcpiResEncode64BitInteger(pb,     u64AddrMin);
    pb = rtAcpiResEncode64BitInteger(pb,     u64AddrMax);
    pb = rtAcpiResEncode64BitInteger(pb,     u64OffTrans);
         rtAcpiResEncode64BitInteger(pb,     u64Length);
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
 */
static int rtAcpiResourceAddDWordAddressRange(PRTACPIRESINT pThis, uint8_t bType, uint32_t fAddrSpace, uint8_t fType,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length)
{
    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 23);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_DWORD_ADDR_SPACE;         /* Tag          */
    pb[1] = 23;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = bType;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE);
    pb[5] = fType;

    pb = rtAcpiResEncode32BitInteger(&pb[6], u32Granularity);
    pb = rtAcpiResEncode32BitInteger(pb,     u32AddrMin);
    pb = rtAcpiResEncode32BitInteger(pb,     u32AddrMax);
    pb = rtAcpiResEncode32BitInteger(pb,     u32OffTrans);
         rtAcpiResEncode32BitInteger(pb,     u32Length);
    return VINF_SUCCESS;
}


/**
 * Converts the given cacheability, range type and R/W flag to the ACPI resource flags.
 *
 * @returns Converted ACPI resource flags.
 * @param   enmCacheability     The cacheability enum to convert.
 * @param   enmType             THe memory range type enum to convert.
 * @param   fRw                 The read/write flag.
 */
DECLINLINE(uint8_t) rtAcpiResourceMemoryRangeToTypeFlags(RTACPIRESMEMRANGECACHEABILITY enmCacheability, RTACPIRESMEMRANGETYPE enmType,
                                                         bool fRw)
{
    uint8_t fType = fRw ? ACPI_RSRCS_ADDR_SPACE_MEM_F_RW : ACPI_RSRCS_ADDR_SPACE_MEM_F_RO;

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
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmCacheability != kAcpiResMemRangeCacheability_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmType != kAcpiResMemType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceMemoryRangeToTypeFlags(enmCacheability, enmType, fRw);
    return rtAcpiResourceAddQWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_MEMORY, fAddrSpace, fType,
                                              u64AddrMin, u64AddrMax, u64OffTrans, u64Granularity, u64Length);
}


RTDECL(int) RTAcpiResourceAddDWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(enmCacheability != kAcpiResMemRangeCacheability_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(enmType != kAcpiResMemType_Invalid, VERR_INVALID_PARAMETER);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t fType = rtAcpiResourceMemoryRangeToTypeFlags(enmCacheability, enmType, fRw);
    return rtAcpiResourceAddDWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_MEMORY, fAddrSpace, fType,
                                              u32AddrMin, u32AddrMax, u32OffTrans, u32Granularity, u32Length);
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
            AssertFailedReturn(VERR_INVALID_PARAMETER);
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
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    return rtAcpiResourceAddQWordAddressRange(pThis, ACPI_RSRCS_ADDR_SPACE_TYPE_IO, fAddrSpace, fType,
                                              u64AddrMin, u64AddrMax, u64OffTrans, u64Granularity, u64Length);
}


RTDECL(int) RTAcpiResourceAddWordBusNumber(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                           uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length)
{
    PRTACPIRESINT pThis = hAcpiRes;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!(fAddrSpace & ~RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!pThis->fSealed, VERR_INVALID_STATE);
    AssertRCReturn(pThis->rcErr, pThis->rcErr);

    uint8_t *pb = rtAcpiResBufEnsureSpace(pThis, 3 + 13);
    if (!pb)
        return VERR_NO_MEMORY;

    pb[0] = ACPI_RSRCS_LARGE_TYPE | ACPI_RSRCS_ITEM_WORD_ADDR_SPACE;          /* Tag          */
    pb[1] = 13;                                                               /* Length[7:0]  */
    pb[2] = 0;                                                                /* Length[15:8] */
    pb[3] = ACPI_RSRCS_ADDR_SPACE_TYPE_BUS_NUM_RANGE;
    pb[4] =   (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB ? ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB : ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE)
            | (fAddrSpace & RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED  ? ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED  : ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE);
    pb[5] = 0;

    pb = rtAcpiResEncode16BitInteger(&pb[6], u16Granularity);
    pb = rtAcpiResEncode16BitInteger(pb,     u16BusMin);
    pb = rtAcpiResEncode16BitInteger(pb,     u16BusMax);
    pb = rtAcpiResEncode16BitInteger(pb,     u16OffTrans);
         rtAcpiResEncode16BitInteger(pb,     u16Length);
    return VINF_SUCCESS;

}
