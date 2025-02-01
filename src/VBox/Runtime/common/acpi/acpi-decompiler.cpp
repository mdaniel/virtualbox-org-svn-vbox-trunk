/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
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
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>
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
 * AML object type.
 */
typedef enum RTACPITBLAMLOBJTYPE
{
    /** Invalid object type. */
    kAcpiAmlObjType_Invalid = 0,
    /** Unknown object type. */
    kAcpiAmlObjType_Unknown,
    /** Method object type. */
    kAcpiAmlObjType_Method,
    /** 32bit hack. */
    kAcpiAmlObjType_32Bit_Hack = 0x7fffffff
} RTACPITBLAMLOBJTYPE;


/**
 * Known object in namespace.
 */
typedef struct RTACPITBLAMLOBJ
{
    /** List node. */
    RTLISTNODE          NdObjs;
    /** Object Type. */
    RTACPITBLAMLOBJTYPE enmType;
    /** Additional data depending on the type. */
    union
    {
        /** Method object argument count. */
        uint32_t        cMethodArgs;
    } u;
    /** Zero terminated object name - variable in size. */
    char                szName[1];
} RTACPITBLAMLOBJ;
typedef RTACPITBLAMLOBJ *PRTACPITBLAMLOBJ;
typedef const RTACPITBLAMLOBJ *PCRTACPITBLAMLOBJ;



/**
 * ACPI AML -> ASL decoder state.
 */
typedef struct RTACPITBLAMLDECODE
{
    /** Pointe to the raw table data. */
    const uint8_t *pbTbl;
    /** Size of the table. */
    uint32_t      cbTbl;
    /** Offset into the table. */
    uint32_t      offTbl;
    /** Current stack level. */
    uint32_t      iLvl;
    /** Number of entries in the package stack. */
    uint32_t      cPkgStackMax;
    /** Stack of package lengths. */
    size_t        *pacbPkgLeft;
    /** Stack of original package lengths. */
    size_t        *pacbPkg;
    /** Flag whether to indent. */
    bool          fIndent;
    /** List of known objects. */
    RTLISTANCHOR  LstObjs;
} RTACPITBLAMLDECODE;
/** Pointer to a ACPI AML -> ASL decoder state. */
typedef RTACPITBLAMLDECODE *PRTACPITBLAMLDECODE;


/**
 * ACPI AML -> ASL decode callback
 *
 * @returns IPRT status code.
 * @param   pThis               ACPI table decoder state.
 * @param   hVfsIosOut          VFS I/O stream output handle.
 * @param   bOp                 The opcode.
 * @param   pErrInfo            Where to return additional error information.
 */
typedef DECLCALLBACKTYPE(int, FNRTACPITBLAMLOPCDECODE,(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo));
/** Pointer to a ACPI AML -> ASL decode callback. */
typedef FNRTACPITBLAMLOPCDECODE *PFNRTACPITBLAMLOPCDECODE;


typedef enum ACPIAMLOPCTYPE
{
    kAcpiAmlOpcType_Invalid = 0,
    kAcpiAmlOpcType_Byte,
    kAcpiAmlOpcType_Word,
    kAcpiAmlOpcType_DWord,
    kAcpiAmlOpcType_NameString,
    kAcpiAmlOpcType_TermArg,
    kAcpiAmlOpcType_SuperName,
    kAcpiAmlOpcType_32BitHack = 0x7fffffff
} ACPIAMLOPCTYPE;


typedef struct RTACPIAMLOPC
{
    /** Name of the opcode. */
    const char               *pszOpc;
    /** Flags for the opcode. */
    uint32_t                 fFlags;
    /** Opcode type for the fields following. */
    ACPIAMLOPCTYPE           aenmTypes[5];
    /** Optional decoder callback. */
    PFNRTACPITBLAMLOPCDECODE pfnDecode;
} RTACPIAMLOPC;
typedef RTACPIAMLOPC *PRTACPIAMLOPC;
typedef const RTACPIAMLOPC *PCRTACPIAMLOPC;

#define RTACPI_AML_OPC_F_NONE           0
#define RTACPI_AML_OPC_F_HAS_PKG_LENGTH RT_BIT_32(0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static int rtAcpiTblAmlDecodeTerminal(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, PRTERRINFO pErrInfo);


DECLINLINE(int) rtAcpiTblAmlDecodeReadU8(PRTACPITBLAMLDECODE pThis, uint8_t *pb, PRTERRINFO pErrInfo)
{
    if (pThis->offTbl < pThis->cbTbl)
    { /* probable */ }
    else
        return RTErrInfoSetF(pErrInfo, VERR_EOF, "AML stream ended prematurely at offset '%#x' trying to read a byte", pThis->offTbl);

    if (!pThis->pacbPkgLeft[pThis->iLvl])
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Data overflows current package limitation");

    pThis->pacbPkgLeft[pThis->iLvl]--;

    *pb = pThis->pbTbl[pThis->offTbl++];
    return VINF_SUCCESS;
}


#if 0
DECLINLINE(int) rtAcpiTblAmlDecodeSkipU8IfEqual(PRTACPITBLAMLDECODE pThis, uint8_t ch, bool *fSkipped, PRTERRINFO pErrInfo)
{
    if (pThis->offTbl < pThis->cbTbl)
    { /* probable */ }
    else
        return RTErrInfoSetF(pErrInfo, VERR_EOF, "AML stream ended prematurely at offset '%#x' trying to read a byte", pThis->offTbl);

    if (pThis->pbTbl[pThis->offTbl] == ch)
    {
        pThis->offTbl++;
        *pfSkipped = true;
    }
    else
        *pfSkipped = false;
    return VINF_SUCCESS;
}
#endif


DECLINLINE(int) rtAcpiTblAmlDecodeReadU16(PRTACPITBLAMLDECODE pThis, uint16_t *pu16, PRTERRINFO pErrInfo)
{
    if (pThis->offTbl <= pThis->cbTbl + sizeof(uint16_t))
    { /* probable */ }
    else
        return RTErrInfoSetF(pErrInfo, VERR_EOF, "AML stream ended prematurely at offset '%#x' trying to read two bytes", pThis->offTbl);

    if (pThis->pacbPkgLeft[pThis->iLvl] < sizeof(uint16_t))
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Data overflows current package limitation");

    pThis->pacbPkgLeft[pThis->iLvl] -= sizeof(uint16_t);

    *pu16 =  pThis->pbTbl[pThis->offTbl++];
    *pu16 |= (uint16_t)pThis->pbTbl[pThis->offTbl++] << 8;
    return VINF_SUCCESS;
}


DECLINLINE(int) rtAcpiTblAmlDecodeReadU32(PRTACPITBLAMLDECODE pThis, uint32_t *pu32, PRTERRINFO pErrInfo)
{
    if (pThis->offTbl <= pThis->cbTbl + sizeof(uint32_t))
    { /* probable */ }
    else
        return RTErrInfoSetF(pErrInfo, VERR_EOF, "AML stream ended prematurely at offset '%#x' trying to read four bytes", pThis->offTbl);

    if (pThis->pacbPkgLeft[pThis->iLvl] < sizeof(uint32_t))
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Data overflows current package limitation");

    pThis->pacbPkgLeft[pThis->iLvl] -= sizeof(uint32_t);

    *pu32 =  pThis->pbTbl[pThis->offTbl++];
    *pu32 |= (uint32_t)pThis->pbTbl[pThis->offTbl++] <<  8;
    *pu32 |= (uint32_t)pThis->pbTbl[pThis->offTbl++] << 16;
    *pu32 |= (uint32_t)pThis->pbTbl[pThis->offTbl++] << 24;
    return VINF_SUCCESS;
}


DECLINLINE(int) rtAcpiTblAmlDecodeReadU64(PRTACPITBLAMLDECODE pThis, uint64_t *pu64, PRTERRINFO pErrInfo)
{
    if (pThis->offTbl <= pThis->cbTbl + sizeof(uint64_t))
    { /* probable */ }
    else
        return RTErrInfoSetF(pErrInfo, VERR_EOF, "AML stream ended prematurely at offset '%#x' trying to read eight bytes", pThis->offTbl);

    if (pThis->pacbPkgLeft[pThis->iLvl] < sizeof(uint64_t))
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Data overflows current package limitation");

    pThis->pacbPkgLeft[pThis->iLvl] -= sizeof(uint64_t);

    *pu64 =  pThis->pbTbl[pThis->offTbl++];
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] <<  8;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 16;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 24;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 32;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 40;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 48;
    *pu64 |= (uint64_t)pThis->pbTbl[pThis->offTbl++] << 54;
    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeNameSeg(PRTACPITBLAMLDECODE pThis, char *pszNameString, PRTERRINFO pErrInfo)
{
    uint8_t abNameSeg[4];
    for (uint8_t i = 0; i < sizeof(abNameSeg); i++)
    {
        int rc = rtAcpiTblAmlDecodeReadU8(pThis, &abNameSeg[i], pErrInfo);
        if (RT_FAILURE(rc)) return rc;
    }

    /* LeadNameChar */
    if (   abNameSeg[0] != '_'
        && !RTLocCIsUpper(abNameSeg[0]))
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "AML stream contains invalid lead name character '%#02RX8'", abNameSeg[0]);

    for (uint8_t i = 1; i < sizeof(abNameSeg); i++)
    {
        if (   abNameSeg[i] != '_'
            && !RTLocCIsUpper(abNameSeg[i])
            && !RTLocCIsDigit(abNameSeg[i]))
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "AML stream contains invalid name character '%#02RX8", abNameSeg[i]);
    }

    pszNameString[0] = (char)abNameSeg[0];
    pszNameString[1] = (char)abNameSeg[1];
    pszNameString[2] = (char)abNameSeg[2];
    pszNameString[3] = (char)abNameSeg[3];
    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeNameSegWithoutLeadChar(PRTACPITBLAMLDECODE pThis, uint8_t bLeadChar, char *pszNameString, PRTERRINFO pErrInfo)
{
    uint8_t abNameSeg[3];
    for (uint8_t i = 0; i < sizeof(abNameSeg); i++)
    {
        int rc = rtAcpiTblAmlDecodeReadU8(pThis, &abNameSeg[i], pErrInfo);
        if (RT_FAILURE(rc)) return rc;
    }

    /* LeadNameChar */
    if (   bLeadChar != '_'
        && !RTLocCIsUpper(bLeadChar))
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "AML stream contains invalid lead name character '%#02RX8'", bLeadChar);

    for (uint8_t i = 1; i < sizeof(abNameSeg); i++)
    {
        if (   abNameSeg[i] != '_'
            && !RTLocCIsUpper(abNameSeg[i])
            && !RTLocCIsDigit(abNameSeg[i]))
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "AML stream contains invalid name character '%#02RX8'", abNameSeg[i]);
    }

    pszNameString[0] = (char)bLeadChar;
    pszNameString[1] = (char)abNameSeg[0];
    pszNameString[2] = (char)abNameSeg[1];
    pszNameString[3] = (char)abNameSeg[2];
    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeNameStringWithLead(PRTACPITBLAMLDECODE pThis, uint8_t bLeadChar, char *pszNameString, size_t cchNameString, size_t *pcbNameString, PRTERRINFO pErrInfo)
{
    AssertReturn(cchNameString >= 5, VERR_INVALID_PARAMETER); /* One name segment is at least 4 bytes (+ terminator). */

    /* Check for a root path. */
    int rc = VINF_SUCCESS;
    uint8_t bTmp = bLeadChar;
    size_t idxName = 0;
    if (bTmp == '\\')
    {
        pszNameString[idxName++] = '\\';

        rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
        if (RT_FAILURE(rc)) return rc;
    }
    else if (bTmp == '^')
    {
        /* Prefix path, can have multiple ^ prefixes. */
        pszNameString[idxName++] = '^';

        for (;;)
        {
            rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
            if (RT_FAILURE(rc)) return rc;

            if (bTmp != '^')
                break;

            if (idxName == cchNameString - 1)
                return RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "PrefixPath in AML byte stream is too long to fit into a %zu byte buffer",
                                     cchNameString - 1);

            pszNameString[idxName++] = '^';
        }
    }

    if (bTmp == ACPI_AML_BYTE_CODE_PREFIX_DUAL_NAME)
    {
        if (idxName + 8 < cchNameString)
        {
            rc = rtAcpiTblAmlDecodeNameSeg(pThis, &pszNameString[idxName], pErrInfo);
            if (RT_FAILURE(rc)) return rc;

            rc = rtAcpiTblAmlDecodeNameSeg(pThis, &pszNameString[idxName + 4], pErrInfo);
            if (RT_FAILURE(rc)) return rc;

            idxName += 8;
            pszNameString[idxName] = '\0';
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "DualNamePrefix string in AML byte stream is too long to fit into a %zu byte buffer",
                               cchNameString - 1);
    }
    else if (bTmp == ACPI_AML_BYTE_CODE_PREFIX_MULTI_NAME)
    {
        uint8_t cSegs = 0;
        rc = rtAcpiTblAmlDecodeReadU8(pThis, &cSegs, pErrInfo);
        if (RT_FAILURE(rc)) return rc;

        if (idxName + cSegs * 4 < cchNameString)
        {
            for (uint8_t i = 0; i < cSegs; i++)
            {
                rc = rtAcpiTblAmlDecodeNameSeg(pThis, &pszNameString[idxName + i * 4], pErrInfo);
                if (RT_FAILURE(rc)) return rc;
            }

            idxName += cSegs * 4;
            pszNameString[idxName] = '\0';
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "MultiNamePrefix string in AML byte stream is too long to fit into a %zu byte buffer",
                               cchNameString - 1);
    }
    else if (bTmp == ACPI_AML_BYTE_CODE_PREFIX_NULL_NAME)
        pszNameString[idxName] = '\0';
    else
    {
        if (idxName + 4 < cchNameString)
        {
            rc = rtAcpiTblAmlDecodeNameSegWithoutLeadChar(pThis, bTmp, &pszNameString[idxName], pErrInfo);
            if (RT_FAILURE(rc)) return rc;

            idxName += 4;
            pszNameString[idxName] = '\0';
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "Name string in AML byte stream is too long to fit into a %zu byte buffer",
                               cchNameString - 1);
    }

    *pcbNameString = idxName;
    return rc;
}


static int rtAcpiTblAmlDecodeNameString(PRTACPITBLAMLDECODE pThis, char *pszNameString, size_t cchNameString, size_t *pcbNameString, PRTERRINFO pErrInfo)
{
    AssertReturn(cchNameString >= 5, VERR_INVALID_PARAMETER); /* One name segment is at least 4 bytes (+ terminator). */

    uint8_t bLead = 0; /* shut up gcc */
    int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bLead, pErrInfo);
    if (RT_FAILURE(rc)) return rc;

    return rtAcpiTblAmlDecodeNameStringWithLead( pThis, bLead, pszNameString, cchNameString, pcbNameString, pErrInfo);
}


/**
 * Adds the proper indentation before a new line.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle to dump the ASL to.
 * @param   uIndentLvl      The level of indentation.
 */
static int rtAcpiTblAmlDecodeIndent(RTVFSIOSTREAM hVfsIos, uint32_t uIndentLvl)
{
    ssize_t cch = RTVfsIoStrmPrintf(hVfsIos, "\n");
    if (cch != 1)
        return cch < 0 ? (int)cch : VERR_BUFFER_UNDERFLOW;

    while (uIndentLvl--)
    {
        cch = RTVfsIoStrmPrintf(hVfsIos, "    ");
        if (cch != 4)
            return cch < 0 ? (int)cch : VERR_BUFFER_UNDERFLOW;
    }

    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeFormat(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIos, const char *pszFmt, ...)
{
    int rc = VINF_SUCCESS;
    if (pThis->fIndent)
        rc = rtAcpiTblAmlDecodeIndent(hVfsIos, pThis->iLvl);
    if (RT_SUCCESS(rc))
    {
        va_list VaArgs;
        va_start(VaArgs, pszFmt);
        ssize_t cch = RTVfsIoStrmPrintfV(hVfsIos, pszFmt, VaArgs);
        va_end(VaArgs);
        if (cch <= 0)
            rc = cch < 0 ? (int)cch : VERR_NO_MEMORY;
    }

    return rc;
}


static int rtAcpiTblAmlDecodePkgLength(PRTACPITBLAMLDECODE pThis, size_t *pcbPkg, size_t *pcbPkgLength, PRTERRINFO pErrInfo)
{
    uint8_t bTmp = 0; /* shut up gcc */
    int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    /* High 2 bits give the remaining bytes following to form the final package length. */
    uint8_t cBytesRemaining = (bTmp >> 6) & 0x3;
    *pcbPkgLength = 1 + cBytesRemaining;

    if (cBytesRemaining)
    {
        size_t cbPkg = bTmp & 0xf;
        for (uint8_t i = 0; i < cBytesRemaining; i++)
        {
            rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            cbPkg |= (size_t)bTmp << (i * 8 + 4);
            *pcbPkg = cbPkg;
        }
    }
    else
        *pcbPkg = bTmp & 0x3f;

    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodePkgPush(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, size_t cbPkg, PRTERRINFO pErrInfo)
{
    /* Get a new stack element. */
    if (pThis->iLvl == pThis->cPkgStackMax)
    {
        uint32_t const cPkgElemsNew = pThis->cPkgStackMax + 8;
        size_t *pacbPkgLeftNew = (size_t *)RTMemRealloc(pThis->pacbPkgLeft, 2 * cPkgElemsNew * sizeof(*pacbPkgLeftNew));
        if (!pacbPkgLeftNew)
            return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Out of memory pushing a new package onto the stack");

        pThis->pacbPkgLeft  = pacbPkgLeftNew;
        pThis->pacbPkg      = pacbPkgLeftNew + cPkgElemsNew;
        pThis->cPkgStackMax = cPkgElemsNew;
    }

    uint32_t const iLvlNew = pThis->iLvl + 1;
    pThis->pacbPkgLeft[iLvlNew] = cbPkg;
    pThis->pacbPkg[iLvlNew]     = cbPkg;
    int rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "{");
    pThis->iLvl = iLvlNew;
    return rc;
}


DECLINLINE(int) rtAcpiTblAmlDecodePkgPop(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, PRTERRINFO pErrInfo)
{
    while (!pThis->pacbPkgLeft[pThis->iLvl])
    {
        size_t cbPkg = pThis->pacbPkg[pThis->iLvl];
        pThis->iLvl--;

        if (pThis->pacbPkgLeft[pThis->iLvl] < cbPkg)
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "AML contains invalid package length encoding");

        pThis->pacbPkgLeft[pThis->iLvl] -= cbPkg;
        int rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "}");
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeIntegerFromPrefix(PRTACPITBLAMLDECODE pThis, uint8_t bPrefix, uint64_t *pu64, size_t cbDecodeMax, size_t *pcbDecoded, PRTERRINFO pErrInfo)
{
    switch (bPrefix)
    {
        case ACPI_AML_BYTE_CODE_PREFIX_BYTE:
        {
            if (!cbDecodeMax)
                return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Not enough data left to decode byte integer in AML stream");

            uint8_t bInt = 0;
            int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bInt, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            *pu64       = bInt;
            *pcbDecoded = 2;
            break;
        }
        case ACPI_AML_BYTE_CODE_PREFIX_WORD:
        {
            if (cbDecodeMax < 2)
                return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Not enough data left to decode word integer in AML stream");

            uint16_t u16 = 0;
            int rc = rtAcpiTblAmlDecodeReadU16(pThis, &u16, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            *pu64       = u16;
            *pcbDecoded = 3;
            break;
        }
        case ACPI_AML_BYTE_CODE_PREFIX_DWORD:
        {
            if (cbDecodeMax < 4)
                return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Not enough data left to decode double word integer in AML stream");

            uint32_t u32 = 0;
            int rc = rtAcpiTblAmlDecodeReadU32(pThis, &u32, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            *pu64       = u32;
            *pcbDecoded = 5;
            break;
        }
        case ACPI_AML_BYTE_CODE_PREFIX_QWORD:
        {
            if (cbDecodeMax < 8)
                return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Not enough data left to decode quad word integer in AML stream");

            uint64_t u64 = 0;
            int rc = rtAcpiTblAmlDecodeReadU64(pThis, &u64, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            *pu64       = u64;
            *pcbDecoded = 9;
            break;
        }
        default:
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Invalid integer prefix '%#02RX8'", bPrefix);
    }

    return VINF_SUCCESS;
}


static int rtAcpiTblAmlDecodeIntegerWorker(PRTACPITBLAMLDECODE pThis, uint64_t *pu64, size_t cbDecodeMax, size_t *pcbDecoded, PRTERRINFO pErrInfo)
{
    AssertReturn(cbDecodeMax >= 1, VERR_INVALID_PARAMETER);

    uint8_t bPrefix = 0; /* shut up gcc */
    int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bPrefix, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    cbDecodeMax--;
    return rtAcpiTblAmlDecodeIntegerFromPrefix(pThis, bPrefix, pu64, cbDecodeMax, pcbDecoded, pErrInfo);
}


static DECLCALLBACK(int) rtAcpiTblAmlDecodeNameObject(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo)
{
    char szName[512];
    size_t cbName = 0;

    int rc = rtAcpiTblAmlDecodeNameStringWithLead(pThis, bOp, &szName[0], sizeof(szName), &cbName, pErrInfo);
    if (RT_FAILURE(rc)) return rc;

    PRTACPITBLAMLOBJ pIt;
    bool fFound = false;
    RTListForEach(&pThis->LstObjs, pIt, RTACPITBLAMLOBJ, NdObjs)
    {
        if (!strcmp(pIt->szName, szName))
        {
            fFound = true;
            break;
        }
    }

    if (fFound)
    {
        if (pIt->enmType == kAcpiAmlObjType_Method)
        {
            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s(", szName);
            if (RT_FAILURE(rc)) return rc;

            bool fIndentOld = pThis->fIndent;
            pThis->fIndent = false;
            for (uint32_t iArg = 0; iArg < pIt->u.cMethodArgs; iArg++)
            {
                rc = rtAcpiTblAmlDecodeTerminal(pThis, hVfsIosOut, pErrInfo);
                if (RT_FAILURE(rc)) return rc;

                if (iArg < pIt->u.cMethodArgs - 1)
                {
                    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ", ", szName);
                    if (RT_FAILURE(rc)) return rc;
                }
            }
            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ")");
            pThis->fIndent = fIndentOld;
        }
        else
            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s", szName);
    }
    else
        rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s", szName);

    return rc;
}


static DECLCALLBACK(int) rtAcpiTblAmlDecodeString(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo)
{
    RT_NOREF(bOp);

    char szStr[512];
    uint32_t i = 0;
    for (;;)
    {
        uint8_t bTmp = 0; /* shut up gcc */
        int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;

        if (bTmp >= 0x1 && bTmp <= 0x7f)
            szStr[i++] = (char)bTmp;
        else if (bTmp == 0x00)
        {
            szStr[i++] = '\0';
            break;
        }
        else
            return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Invalid ASCII string character %#x in string", bTmp);

        if (i == sizeof(szStr))
            return RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "ASCII string is out of bounds");
    }

    return rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "\"%s\"", szStr);
}


static DECLCALLBACK(int) rtAcpiTblAmlDecodeBuffer(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo)
{
    RT_NOREF(bOp);

    size_t cbPkg = 0;
    size_t cbPkgLength = 0;
    int rc = rtAcpiTblAmlDecodePkgLength(pThis, &cbPkg, &cbPkgLength, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    cbPkg -= cbPkgLength;
    uint64_t u64 = 0;
    size_t cbInt = 0;
    rc = rtAcpiTblAmlDecodeIntegerWorker(pThis, &u64, cbPkg, &cbInt, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    cbPkg -= cbInt;

    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "Buffer (%RU64) {", u64);
    if (RT_FAILURE(rc))
        return rc;

    /* Decode remaining bytes. */
    while (cbPkg--)
    {
        uint8_t bTmp = 0;
        rc = rtAcpiTblAmlDecodeReadU8(pThis, &bTmp, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;

        rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%#02RX8%s", bTmp, cbPkg ? "," : "");
        if (RT_FAILURE(rc))
            return rc;
    }

    return rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "}");
}


static DECLCALLBACK(int) rtAcpiTblAmlDecodeInteger(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo)
{
    uint64_t u64 = 0;
    size_t cbDecoded = 0;
    int rc = rtAcpiTblAmlDecodeIntegerFromPrefix(pThis, bOp, &u64, sizeof(uint64_t), &cbDecoded, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    return rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%#RX64", u64);
}


static DECLCALLBACK(int) rtAcpiTblAmlDecodeMethod(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, uint8_t bOp, PRTERRINFO pErrInfo)
{
    RT_NOREF(bOp);

    size_t cbPkg = 0;
    size_t cbPkgLength = 0;
    int rc = rtAcpiTblAmlDecodePkgLength(pThis, &cbPkg, &cbPkgLength, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    size_t cbPkgConsumed = cbPkgLength;
    char szName[512]; RT_ZERO(szName);
    size_t cchName = 0;
    rc = rtAcpiTblAmlDecodeNameString(pThis, &szName[0], sizeof(szName), &cchName, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    cbPkgConsumed += cchName;

    uint8_t bMethod = 0; /* shut up gcc */
    rc = rtAcpiTblAmlDecodeReadU8(pThis, &bMethod, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    cbPkgConsumed++;

    if (cbPkg < cbPkgConsumed)
        return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Number of bytes consumed for the current package exceeds package length (%zu vs %zu)",
                             cbPkgConsumed, cbPkg);

    PRTACPITBLAMLOBJ pObj = (PRTACPITBLAMLOBJ)RTMemAllocZ(RT_UOFFSETOF_DYN(RTACPITBLAMLOBJ, szName[cchName + 1]));
    if (!pObj)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate %zu bytes for method object %s",
                             RT_UOFFSETOF_DYN(RTACPITBLAMLOBJ, szName[cchName + 1]), szName);

    pObj->enmType       = kAcpiAmlObjType_Method;
    pObj->u.cMethodArgs = bMethod & 0x7;
    memcpy(&pObj->szName[0], &szName[0], cchName);
    RTListAppend(&pThis->LstObjs, &pObj->NdObjs);

    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "Method(%s, %u, %s, %u)",
                                  pObj->szName, pObj->u.cMethodArgs, bMethod & RT_BIT(3) ? "Serialized" : "NotSerialized",
                                  bMethod >> 4);
    if (RT_FAILURE(rc))
        return rc;
    return rtAcpiTblAmlDecodePkgPush(pThis, hVfsIosOut, cbPkg - cbPkgConsumed, pErrInfo);
}


/**
 * AML Opcode -> ASL decoder array.
 */
static const RTACPIAMLOPC g_aAmlOpcodeDecode[] =
{
#define RTACPI_AML_OPC_INVALID \
    { NULL,     RTACPI_AML_OPC_F_NONE,  { kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_0(a_pszOpc, a_fFlags) \
    { a_pszOpc, a_fFlags,               { kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_1(a_pszOpc, a_fFlags, a_enmType0) \
    { a_pszOpc, a_fFlags,               { a_enmType0,              kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_2(a_pszOpc, a_fFlags, a_enmType0, a_enmType1) \
    { a_pszOpc, a_fFlags,               { a_enmType0,              a_enmType1,              kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_3(a_pszOpc, a_fFlags, a_enmType0, a_enmType1, a_enmType2) \
    { a_pszOpc, a_fFlags,               { a_enmType0,              a_enmType1,              a_enmType2,              kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_4(a_pszOpc, a_fFlags, a_enmType0, a_enmType1, a_enmType2, a_enmType3) \
    { a_pszOpc, a_fFlags,               { a_enmType0,              a_enmType1,              a_enmType2,              a_enmType3,              kAcpiAmlOpcType_Invalid }, NULL }
#define RTACPI_AML_OPC_SIMPLE_5(a_pszOpc, a_fFlags, a_enmType0, a_enmType1, a_enmType2, a_enmType3, a_enmType4) \
    { a_pszOpc, a_fFlags,               { a_enmType0,              a_enmType1,              a_enmType2,              a_enmType3,              a_enmType4              }, NULL }
#define RTACPI_AML_OPC_HANDLER(a_pszOpc, a_pfnHandler) \
    { a_pszOpc, RTACPI_AML_OPC_F_NONE, { kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid, kAcpiAmlOpcType_Invalid }, a_pfnHandler }

    /* 0x00 */ RTACPI_AML_OPC_SIMPLE_0("Zero", RTACPI_AML_OPC_F_NONE),
    /* 0x01 */ RTACPI_AML_OPC_SIMPLE_0("One",  RTACPI_AML_OPC_F_NONE),
    /* 0x02 */ RTACPI_AML_OPC_INVALID,
    /* 0x03 */ RTACPI_AML_OPC_INVALID,
    /* 0x04 */ RTACPI_AML_OPC_INVALID,
    /* 0x05 */ RTACPI_AML_OPC_INVALID,
    /* 0x06 */ RTACPI_AML_OPC_INVALID,
    /* 0x07 */ RTACPI_AML_OPC_INVALID,
    /* 0x08 */ RTACPI_AML_OPC_SIMPLE_2("Name",   0,     kAcpiAmlOpcType_NameString,  kAcpiAmlOpcType_TermArg),
    /* 0x09 */ RTACPI_AML_OPC_INVALID,
    /* 0x0a */ RTACPI_AML_OPC_HANDLER( "ByteInteger",   rtAcpiTblAmlDecodeInteger),
    /* 0x0b */ RTACPI_AML_OPC_HANDLER( "WordInteger",   rtAcpiTblAmlDecodeInteger),
    /* 0x0c */ RTACPI_AML_OPC_HANDLER( "DWordInteger",  rtAcpiTblAmlDecodeInteger),
    /* 0x0d */ RTACPI_AML_OPC_HANDLER( "StringPrefix",  rtAcpiTblAmlDecodeString),
    /* 0x0e */ RTACPI_AML_OPC_HANDLER( "QWordInteger",  rtAcpiTblAmlDecodeInteger),
    /* 0x0f */ RTACPI_AML_OPC_INVALID,

    /* 0x10 */ RTACPI_AML_OPC_SIMPLE_1("Scope",    RTACPI_AML_OPC_F_HAS_PKG_LENGTH,  kAcpiAmlOpcType_NameString),
    /* 0x11 */ RTACPI_AML_OPC_HANDLER( "Buffer",   rtAcpiTblAmlDecodeBuffer),
    /* 0x12 */ RTACPI_AML_OPC_INVALID,
    /* 0x13 */ RTACPI_AML_OPC_INVALID,
    /* 0x14 */ RTACPI_AML_OPC_HANDLER( "Method",        rtAcpiTblAmlDecodeMethod),
    /* 0x15 */ RTACPI_AML_OPC_SIMPLE_3("External", RTACPI_AML_OPC_F_NONE,            kAcpiAmlOpcType_NameString, kAcpiAmlOpcType_Byte, kAcpiAmlOpcType_Byte),
    /* 0x16 */ RTACPI_AML_OPC_INVALID,
    /* 0x17 */ RTACPI_AML_OPC_INVALID,
    /* 0x18 */ RTACPI_AML_OPC_INVALID,
    /* 0x19 */ RTACPI_AML_OPC_INVALID,
    /* 0x1a */ RTACPI_AML_OPC_INVALID,
    /* 0x1b */ RTACPI_AML_OPC_INVALID,
    /* 0x1c */ RTACPI_AML_OPC_INVALID,
    /* 0x1d */ RTACPI_AML_OPC_INVALID,
    /* 0x1e */ RTACPI_AML_OPC_INVALID,
    /* 0x1f */ RTACPI_AML_OPC_INVALID,

    /* 0x20 */ RTACPI_AML_OPC_INVALID,
    /* 0x21 */ RTACPI_AML_OPC_INVALID,
    /* 0x22 */ RTACPI_AML_OPC_INVALID,
    /* 0x23 */ RTACPI_AML_OPC_INVALID,
    /* 0x24 */ RTACPI_AML_OPC_INVALID,
    /* 0x25 */ RTACPI_AML_OPC_INVALID,
    /* 0x26 */ RTACPI_AML_OPC_INVALID,
    /* 0x27 */ RTACPI_AML_OPC_INVALID,
    /* 0x28 */ RTACPI_AML_OPC_INVALID,
    /* 0x29 */ RTACPI_AML_OPC_INVALID,
    /* 0x2a */ RTACPI_AML_OPC_INVALID,
    /* 0x2b */ RTACPI_AML_OPC_INVALID,
    /* 0x2c */ RTACPI_AML_OPC_INVALID,
    /* 0x2d */ RTACPI_AML_OPC_INVALID,
    /* 0x2e */ RTACPI_AML_OPC_INVALID,
    /* 0x2f */ RTACPI_AML_OPC_INVALID,

    /* 0x30 */ RTACPI_AML_OPC_INVALID,
    /* 0x31 */ RTACPI_AML_OPC_INVALID,
    /* 0x32 */ RTACPI_AML_OPC_INVALID,
    /* 0x33 */ RTACPI_AML_OPC_INVALID,
    /* 0x34 */ RTACPI_AML_OPC_INVALID,
    /* 0x35 */ RTACPI_AML_OPC_INVALID,
    /* 0x36 */ RTACPI_AML_OPC_INVALID,
    /* 0x37 */ RTACPI_AML_OPC_INVALID,
    /* 0x38 */ RTACPI_AML_OPC_INVALID,
    /* 0x39 */ RTACPI_AML_OPC_INVALID,
    /* 0x3a */ RTACPI_AML_OPC_INVALID,
    /* 0x3b */ RTACPI_AML_OPC_INVALID,
    /* 0x3c */ RTACPI_AML_OPC_INVALID,
    /* 0x3d */ RTACPI_AML_OPC_INVALID,
    /* 0x3e */ RTACPI_AML_OPC_INVALID,
    /* 0x3f */ RTACPI_AML_OPC_INVALID,

    /* 0x40 */ RTACPI_AML_OPC_INVALID,
    /* 0x41 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x42 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x43 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x44 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x45 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x46 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x47 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x48 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x49 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4a */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4b */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4c */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4d */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4e */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x4f */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),

    /* 0x50 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x51 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x52 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x53 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x54 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x55 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x56 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x57 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x58 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x59 */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x5a */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x5b */ RTACPI_AML_OPC_INVALID,
    /* 0x5c */ RTACPI_AML_OPC_HANDLER("RootChar",         rtAcpiTblAmlDecodeNameObject),
    /* 0x5d */ RTACPI_AML_OPC_INVALID,
    /* 0x5e */ RTACPI_AML_OPC_HANDLER("ParentPrefixChar", rtAcpiTblAmlDecodeNameObject),
    /* 0x5f */ RTACPI_AML_OPC_HANDLER("NameChar",         rtAcpiTblAmlDecodeNameObject),

    /* 0x60 */ RTACPI_AML_OPC_SIMPLE_0("Local0", RTACPI_AML_OPC_F_NONE),
    /* 0x61 */ RTACPI_AML_OPC_SIMPLE_0("Local1", RTACPI_AML_OPC_F_NONE),
    /* 0x62 */ RTACPI_AML_OPC_SIMPLE_0("Local2", RTACPI_AML_OPC_F_NONE),
    /* 0x63 */ RTACPI_AML_OPC_SIMPLE_0("Local3", RTACPI_AML_OPC_F_NONE),
    /* 0x64 */ RTACPI_AML_OPC_SIMPLE_0("Local4", RTACPI_AML_OPC_F_NONE),
    /* 0x65 */ RTACPI_AML_OPC_SIMPLE_0("Local5", RTACPI_AML_OPC_F_NONE),
    /* 0x66 */ RTACPI_AML_OPC_SIMPLE_0("Local6", RTACPI_AML_OPC_F_NONE),
    /* 0x67 */ RTACPI_AML_OPC_SIMPLE_0("Local7", RTACPI_AML_OPC_F_NONE),
    /* 0x68 */ RTACPI_AML_OPC_SIMPLE_0("Arg0",   RTACPI_AML_OPC_F_NONE),
    /* 0x69 */ RTACPI_AML_OPC_SIMPLE_0("Arg1",   RTACPI_AML_OPC_F_NONE),
    /* 0x6a */ RTACPI_AML_OPC_SIMPLE_0("Arg2",   RTACPI_AML_OPC_F_NONE),
    /* 0x6b */ RTACPI_AML_OPC_SIMPLE_0("Arg3",   RTACPI_AML_OPC_F_NONE),
    /* 0x6c */ RTACPI_AML_OPC_SIMPLE_0("Arg4",   RTACPI_AML_OPC_F_NONE),
    /* 0x6d */ RTACPI_AML_OPC_SIMPLE_0("Arg5",   RTACPI_AML_OPC_F_NONE),
    /* 0x6e */ RTACPI_AML_OPC_SIMPLE_0("Arg6",   RTACPI_AML_OPC_F_NONE),
    /* 0x6f */ RTACPI_AML_OPC_INVALID,

    /* 0x70 */ RTACPI_AML_OPC_SIMPLE_2("Store",   0,     kAcpiAmlOpcType_TermArg, kAcpiAmlOpcType_SuperName),
    /* 0x71 */ RTACPI_AML_OPC_INVALID,
    /* 0x72 */ RTACPI_AML_OPC_INVALID,
    /* 0x73 */ RTACPI_AML_OPC_INVALID,
    /* 0x74 */ RTACPI_AML_OPC_INVALID,
    /* 0x75 */ RTACPI_AML_OPC_INVALID,
    /* 0x76 */ RTACPI_AML_OPC_INVALID,
    /* 0x77 */ RTACPI_AML_OPC_INVALID,
    /* 0x78 */ RTACPI_AML_OPC_INVALID,
    /* 0x79 */ RTACPI_AML_OPC_INVALID,
    /* 0x7a */ RTACPI_AML_OPC_INVALID,
    /* 0x7b */ RTACPI_AML_OPC_INVALID,
    /* 0x7c */ RTACPI_AML_OPC_INVALID,
    /* 0x7d */ RTACPI_AML_OPC_INVALID,
    /* 0x7e */ RTACPI_AML_OPC_INVALID,
    /* 0x7f */ RTACPI_AML_OPC_INVALID,

    /* 0x80 */ RTACPI_AML_OPC_INVALID,
    /* 0x81 */ RTACPI_AML_OPC_INVALID,
    /* 0x82 */ RTACPI_AML_OPC_INVALID,
    /* 0x83 */ RTACPI_AML_OPC_INVALID,
    /* 0x84 */ RTACPI_AML_OPC_INVALID,
    /* 0x85 */ RTACPI_AML_OPC_INVALID,
    /* 0x86 */ RTACPI_AML_OPC_SIMPLE_2("Notify",                                0,     kAcpiAmlOpcType_SuperName, kAcpiAmlOpcType_TermArg),
    /* 0x87 */ RTACPI_AML_OPC_INVALID,
    /* 0x88 */ RTACPI_AML_OPC_SIMPLE_3("Index",                                 0,     kAcpiAmlOpcType_TermArg,   kAcpiAmlOpcType_TermArg, kAcpiAmlOpcType_SuperName),
    /* 0x89 */ RTACPI_AML_OPC_INVALID,
    /* 0x8a */ RTACPI_AML_OPC_INVALID,
    /* 0x8b */ RTACPI_AML_OPC_INVALID,
    /* 0x8c */ RTACPI_AML_OPC_INVALID,
    /* 0x8d */ RTACPI_AML_OPC_INVALID,
    /* 0x8e */ RTACPI_AML_OPC_INVALID,
    /* 0x8f */ RTACPI_AML_OPC_INVALID,

    /* 0x90 */ RTACPI_AML_OPC_INVALID,
    /* 0x91 */ RTACPI_AML_OPC_INVALID,
    /* 0x92 */ RTACPI_AML_OPC_INVALID,
    /* 0x93 */ RTACPI_AML_OPC_SIMPLE_2("LEqual",                                0,     kAcpiAmlOpcType_TermArg, kAcpiAmlOpcType_TermArg),
    /* 0x94 */ RTACPI_AML_OPC_INVALID,
    /* 0x95 */ RTACPI_AML_OPC_INVALID,
    /* 0x96 */ RTACPI_AML_OPC_INVALID,
    /* 0x97 */ RTACPI_AML_OPC_INVALID,
    /* 0x98 */ RTACPI_AML_OPC_INVALID,
    /* 0x99 */ RTACPI_AML_OPC_INVALID,
    /* 0x9a */ RTACPI_AML_OPC_INVALID,
    /* 0x9b */ RTACPI_AML_OPC_INVALID,
    /* 0x9c */ RTACPI_AML_OPC_INVALID,
    /* 0x9d */ RTACPI_AML_OPC_INVALID,
    /* 0x9e */ RTACPI_AML_OPC_INVALID,
    /* 0x9f */ RTACPI_AML_OPC_INVALID,

    /* 0xa0 */ RTACPI_AML_OPC_SIMPLE_1("If",      RTACPI_AML_OPC_F_HAS_PKG_LENGTH,     kAcpiAmlOpcType_TermArg),
    /* 0xa1 */ RTACPI_AML_OPC_SIMPLE_0("Else",    RTACPI_AML_OPC_F_HAS_PKG_LENGTH),
    /* 0xa2 */ RTACPI_AML_OPC_INVALID,
    /* 0xa3 */ RTACPI_AML_OPC_INVALID,
    /* 0xa4 */ RTACPI_AML_OPC_SIMPLE_1("Return",                                0,     kAcpiAmlOpcType_TermArg),
    /* 0xa5 */ RTACPI_AML_OPC_INVALID,
    /* 0xa6 */ RTACPI_AML_OPC_INVALID,
    /* 0xa7 */ RTACPI_AML_OPC_INVALID,
    /* 0xa8 */ RTACPI_AML_OPC_INVALID,
    /* 0xa9 */ RTACPI_AML_OPC_INVALID,
    /* 0xaa */ RTACPI_AML_OPC_INVALID,
    /* 0xab */ RTACPI_AML_OPC_INVALID,
    /* 0xac */ RTACPI_AML_OPC_INVALID,
    /* 0xad */ RTACPI_AML_OPC_INVALID,
    /* 0xae */ RTACPI_AML_OPC_INVALID,
    /* 0xaf */ RTACPI_AML_OPC_INVALID,

    /* 0xb0 */ RTACPI_AML_OPC_INVALID,
    /* 0xb1 */ RTACPI_AML_OPC_INVALID,
    /* 0xb2 */ RTACPI_AML_OPC_INVALID,
    /* 0xb3 */ RTACPI_AML_OPC_INVALID,
    /* 0xb4 */ RTACPI_AML_OPC_INVALID,
    /* 0xb5 */ RTACPI_AML_OPC_INVALID,
    /* 0xb6 */ RTACPI_AML_OPC_INVALID,
    /* 0xb7 */ RTACPI_AML_OPC_INVALID,
    /* 0xb8 */ RTACPI_AML_OPC_INVALID,
    /* 0xb9 */ RTACPI_AML_OPC_INVALID,
    /* 0xba */ RTACPI_AML_OPC_INVALID,
    /* 0xbb */ RTACPI_AML_OPC_INVALID,
    /* 0xbc */ RTACPI_AML_OPC_INVALID,
    /* 0xbd */ RTACPI_AML_OPC_INVALID,
    /* 0xbe */ RTACPI_AML_OPC_INVALID,
    /* 0xbf */ RTACPI_AML_OPC_INVALID,

    /* 0xc0 */ RTACPI_AML_OPC_INVALID,
    /* 0xc1 */ RTACPI_AML_OPC_INVALID,
    /* 0xc2 */ RTACPI_AML_OPC_INVALID,
    /* 0xc3 */ RTACPI_AML_OPC_INVALID,
    /* 0xc4 */ RTACPI_AML_OPC_INVALID,
    /* 0xc5 */ RTACPI_AML_OPC_INVALID,
    /* 0xc6 */ RTACPI_AML_OPC_INVALID,
    /* 0xc7 */ RTACPI_AML_OPC_INVALID,
    /* 0xc8 */ RTACPI_AML_OPC_INVALID,
    /* 0xc9 */ RTACPI_AML_OPC_INVALID,
    /* 0xca */ RTACPI_AML_OPC_INVALID,
    /* 0xcb */ RTACPI_AML_OPC_INVALID,
    /* 0xcc */ RTACPI_AML_OPC_INVALID,
    /* 0xcd */ RTACPI_AML_OPC_INVALID,
    /* 0xce */ RTACPI_AML_OPC_INVALID,
    /* 0xcf */ RTACPI_AML_OPC_INVALID,

    /* 0xd0 */ RTACPI_AML_OPC_INVALID,
    /* 0xd1 */ RTACPI_AML_OPC_INVALID,
    /* 0xd2 */ RTACPI_AML_OPC_INVALID,
    /* 0xd3 */ RTACPI_AML_OPC_INVALID,
    /* 0xd4 */ RTACPI_AML_OPC_INVALID,
    /* 0xd5 */ RTACPI_AML_OPC_INVALID,
    /* 0xd6 */ RTACPI_AML_OPC_INVALID,
    /* 0xd7 */ RTACPI_AML_OPC_INVALID,
    /* 0xd8 */ RTACPI_AML_OPC_INVALID,
    /* 0xd9 */ RTACPI_AML_OPC_INVALID,
    /* 0xda */ RTACPI_AML_OPC_INVALID,
    /* 0xdb */ RTACPI_AML_OPC_INVALID,
    /* 0xdc */ RTACPI_AML_OPC_INVALID,
    /* 0xdd */ RTACPI_AML_OPC_INVALID,
    /* 0xde */ RTACPI_AML_OPC_INVALID,
    /* 0xdf */ RTACPI_AML_OPC_INVALID,

    /* 0xe0 */ RTACPI_AML_OPC_INVALID,
    /* 0xe1 */ RTACPI_AML_OPC_INVALID,
    /* 0xe2 */ RTACPI_AML_OPC_INVALID,
    /* 0xe3 */ RTACPI_AML_OPC_INVALID,
    /* 0xe4 */ RTACPI_AML_OPC_INVALID,
    /* 0xe5 */ RTACPI_AML_OPC_INVALID,
    /* 0xe6 */ RTACPI_AML_OPC_INVALID,
    /* 0xe7 */ RTACPI_AML_OPC_INVALID,
    /* 0xe8 */ RTACPI_AML_OPC_INVALID,
    /* 0xe9 */ RTACPI_AML_OPC_INVALID,
    /* 0xea */ RTACPI_AML_OPC_INVALID,
    /* 0xeb */ RTACPI_AML_OPC_INVALID,
    /* 0xec */ RTACPI_AML_OPC_INVALID,
    /* 0xed */ RTACPI_AML_OPC_INVALID,
    /* 0xee */ RTACPI_AML_OPC_INVALID,
    /* 0xef */ RTACPI_AML_OPC_INVALID,

    /* 0xf0 */ RTACPI_AML_OPC_INVALID,
    /* 0xf1 */ RTACPI_AML_OPC_INVALID,
    /* 0xf2 */ RTACPI_AML_OPC_INVALID,
    /* 0xf3 */ RTACPI_AML_OPC_INVALID,
    /* 0xf4 */ RTACPI_AML_OPC_INVALID,
    /* 0xf5 */ RTACPI_AML_OPC_INVALID,
    /* 0xf6 */ RTACPI_AML_OPC_INVALID,
    /* 0xf7 */ RTACPI_AML_OPC_INVALID,
    /* 0xf8 */ RTACPI_AML_OPC_INVALID,
    /* 0xf9 */ RTACPI_AML_OPC_INVALID,
    /* 0xfa */ RTACPI_AML_OPC_INVALID,
    /* 0xfb */ RTACPI_AML_OPC_INVALID,
    /* 0xfc */ RTACPI_AML_OPC_INVALID,
    /* 0xfd */ RTACPI_AML_OPC_INVALID,
    /* 0xfe */ RTACPI_AML_OPC_INVALID,
    /* 0xff */ RTACPI_AML_OPC_INVALID
};


/**
 * AML extended opcode -> ASL decoder array.
 */
static const RTACPIAMLOPC g_aAmlExtOpcodeDecode[] =
{
    /* 0x00 */ RTACPI_AML_OPC_INVALID,
    /* 0x01 */ RTACPI_AML_OPC_INVALID,
    /* 0x02 */ RTACPI_AML_OPC_INVALID,
    /* 0x03 */ RTACPI_AML_OPC_INVALID,
    /* 0x04 */ RTACPI_AML_OPC_INVALID,
    /* 0x05 */ RTACPI_AML_OPC_INVALID,
    /* 0x06 */ RTACPI_AML_OPC_INVALID,
    /* 0x07 */ RTACPI_AML_OPC_INVALID,
    /* 0x08 */ RTACPI_AML_OPC_INVALID,
    /* 0x09 */ RTACPI_AML_OPC_INVALID,
    /* 0x0a */ RTACPI_AML_OPC_INVALID,
    /* 0x0b */ RTACPI_AML_OPC_INVALID,
    /* 0x0c */ RTACPI_AML_OPC_INVALID,
    /* 0x0d */ RTACPI_AML_OPC_INVALID,
    /* 0x0e */ RTACPI_AML_OPC_INVALID,
    /* 0x0f */ RTACPI_AML_OPC_INVALID,

    /* 0x10 */ RTACPI_AML_OPC_INVALID,
    /* 0x11 */ RTACPI_AML_OPC_INVALID,
    /* 0x12 */ RTACPI_AML_OPC_INVALID,
    /* 0x13 */ RTACPI_AML_OPC_INVALID,
    /* 0x14 */ RTACPI_AML_OPC_INVALID,
    /* 0x15 */ RTACPI_AML_OPC_INVALID,
    /* 0x16 */ RTACPI_AML_OPC_INVALID,
    /* 0x17 */ RTACPI_AML_OPC_INVALID,
    /* 0x18 */ RTACPI_AML_OPC_INVALID,
    /* 0x19 */ RTACPI_AML_OPC_INVALID,
    /* 0x1a */ RTACPI_AML_OPC_INVALID,
    /* 0x1b */ RTACPI_AML_OPC_INVALID,
    /* 0x1c */ RTACPI_AML_OPC_INVALID,
    /* 0x1d */ RTACPI_AML_OPC_INVALID,
    /* 0x1e */ RTACPI_AML_OPC_INVALID,
    /* 0x1f */ RTACPI_AML_OPC_INVALID,

    /* 0x20 */ RTACPI_AML_OPC_INVALID,
    /* 0x21 */ RTACPI_AML_OPC_INVALID,
    /* 0x22 */ RTACPI_AML_OPC_INVALID,
    /* 0x23 */ RTACPI_AML_OPC_INVALID,
    /* 0x24 */ RTACPI_AML_OPC_INVALID,
    /* 0x25 */ RTACPI_AML_OPC_INVALID,
    /* 0x26 */ RTACPI_AML_OPC_INVALID,
    /* 0x27 */ RTACPI_AML_OPC_INVALID,
    /* 0x28 */ RTACPI_AML_OPC_INVALID,
    /* 0x29 */ RTACPI_AML_OPC_INVALID,
    /* 0x2a */ RTACPI_AML_OPC_INVALID,
    /* 0x2b */ RTACPI_AML_OPC_INVALID,
    /* 0x2c */ RTACPI_AML_OPC_INVALID,
    /* 0x2d */ RTACPI_AML_OPC_INVALID,
    /* 0x2e */ RTACPI_AML_OPC_INVALID,
    /* 0x2f */ RTACPI_AML_OPC_INVALID,

    /* 0x30 */ RTACPI_AML_OPC_INVALID,
    /* 0x31 */ RTACPI_AML_OPC_SIMPLE_0("Debug", RTACPI_AML_OPC_F_NONE),
    /* 0x32 */ RTACPI_AML_OPC_INVALID,
    /* 0x33 */ RTACPI_AML_OPC_INVALID,
    /* 0x34 */ RTACPI_AML_OPC_INVALID,
    /* 0x35 */ RTACPI_AML_OPC_INVALID,
    /* 0x36 */ RTACPI_AML_OPC_INVALID,
    /* 0x37 */ RTACPI_AML_OPC_INVALID,
    /* 0x38 */ RTACPI_AML_OPC_INVALID,
    /* 0x39 */ RTACPI_AML_OPC_INVALID,
    /* 0x3a */ RTACPI_AML_OPC_INVALID,
    /* 0x3b */ RTACPI_AML_OPC_INVALID,
    /* 0x3c */ RTACPI_AML_OPC_INVALID,
    /* 0x3d */ RTACPI_AML_OPC_INVALID,
    /* 0x3e */ RTACPI_AML_OPC_INVALID,
    /* 0x3f */ RTACPI_AML_OPC_INVALID,

    /* 0x40 */ RTACPI_AML_OPC_INVALID,
    /* 0x41 */ RTACPI_AML_OPC_INVALID,
    /* 0x42 */ RTACPI_AML_OPC_INVALID,
    /* 0x43 */ RTACPI_AML_OPC_INVALID,
    /* 0x44 */ RTACPI_AML_OPC_INVALID,
    /* 0x45 */ RTACPI_AML_OPC_INVALID,
    /* 0x46 */ RTACPI_AML_OPC_INVALID,
    /* 0x47 */ RTACPI_AML_OPC_INVALID,
    /* 0x48 */ RTACPI_AML_OPC_INVALID,
    /* 0x49 */ RTACPI_AML_OPC_INVALID,
    /* 0x4a */ RTACPI_AML_OPC_INVALID,
    /* 0x4b */ RTACPI_AML_OPC_INVALID,
    /* 0x4c */ RTACPI_AML_OPC_INVALID,
    /* 0x4d */ RTACPI_AML_OPC_INVALID,
    /* 0x4e */ RTACPI_AML_OPC_INVALID,
    /* 0x4f */ RTACPI_AML_OPC_INVALID,

    /* 0x50 */ RTACPI_AML_OPC_INVALID,
    /* 0x51 */ RTACPI_AML_OPC_INVALID,
    /* 0x52 */ RTACPI_AML_OPC_INVALID,
    /* 0x53 */ RTACPI_AML_OPC_INVALID,
    /* 0x54 */ RTACPI_AML_OPC_INVALID,
    /* 0x55 */ RTACPI_AML_OPC_INVALID,
    /* 0x56 */ RTACPI_AML_OPC_INVALID,
    /* 0x57 */ RTACPI_AML_OPC_INVALID,
    /* 0x58 */ RTACPI_AML_OPC_INVALID,
    /* 0x59 */ RTACPI_AML_OPC_INVALID,
    /* 0x5a */ RTACPI_AML_OPC_INVALID,
    /* 0x5b */ RTACPI_AML_OPC_INVALID,
    /* 0x5c */ RTACPI_AML_OPC_INVALID,
    /* 0x5d */ RTACPI_AML_OPC_INVALID,
    /* 0x5e */ RTACPI_AML_OPC_INVALID,
    /* 0x5f */ RTACPI_AML_OPC_INVALID,

    /* 0x60 */ RTACPI_AML_OPC_INVALID,
    /* 0x61 */ RTACPI_AML_OPC_INVALID,
    /* 0x62 */ RTACPI_AML_OPC_INVALID,
    /* 0x63 */ RTACPI_AML_OPC_INVALID,
    /* 0x64 */ RTACPI_AML_OPC_INVALID,
    /* 0x65 */ RTACPI_AML_OPC_INVALID,
    /* 0x66 */ RTACPI_AML_OPC_INVALID,
    /* 0x67 */ RTACPI_AML_OPC_INVALID,
    /* 0x68 */ RTACPI_AML_OPC_INVALID,
    /* 0x69 */ RTACPI_AML_OPC_INVALID,
    /* 0x6a */ RTACPI_AML_OPC_INVALID,
    /* 0x6b */ RTACPI_AML_OPC_INVALID,
    /* 0x6c */ RTACPI_AML_OPC_INVALID,
    /* 0x6d */ RTACPI_AML_OPC_INVALID,
    /* 0x6e */ RTACPI_AML_OPC_INVALID,
    /* 0x6f */ RTACPI_AML_OPC_INVALID,

    /* 0x70 */ RTACPI_AML_OPC_INVALID,
    /* 0x71 */ RTACPI_AML_OPC_INVALID,
    /* 0x72 */ RTACPI_AML_OPC_INVALID,
    /* 0x73 */ RTACPI_AML_OPC_INVALID,
    /* 0x74 */ RTACPI_AML_OPC_INVALID,
    /* 0x75 */ RTACPI_AML_OPC_INVALID,
    /* 0x76 */ RTACPI_AML_OPC_INVALID,
    /* 0x77 */ RTACPI_AML_OPC_INVALID,
    /* 0x78 */ RTACPI_AML_OPC_INVALID,
    /* 0x79 */ RTACPI_AML_OPC_INVALID,
    /* 0x7a */ RTACPI_AML_OPC_INVALID,
    /* 0x7b */ RTACPI_AML_OPC_INVALID,
    /* 0x7c */ RTACPI_AML_OPC_INVALID,
    /* 0x7d */ RTACPI_AML_OPC_INVALID,
    /* 0x7e */ RTACPI_AML_OPC_INVALID,
    /* 0x7f */ RTACPI_AML_OPC_INVALID,

    /* 0x80 */ RTACPI_AML_OPC_SIMPLE_4("OpRegion",                                0, kAcpiAmlOpcType_NameString, kAcpiAmlOpcType_Byte, kAcpiAmlOpcType_TermArg, kAcpiAmlOpcType_TermArg),
    /* 0x81 */ RTACPI_AML_OPC_INVALID,
    /* 0x82 */ RTACPI_AML_OPC_SIMPLE_1("Device",    RTACPI_AML_OPC_F_HAS_PKG_LENGTH, kAcpiAmlOpcType_NameString),
    /* 0x83 */ RTACPI_AML_OPC_SIMPLE_4("Processor", RTACPI_AML_OPC_F_HAS_PKG_LENGTH, kAcpiAmlOpcType_NameString, kAcpiAmlOpcType_Byte, kAcpiAmlOpcType_DWord,   kAcpiAmlOpcType_Byte),
    /* 0x84 */ RTACPI_AML_OPC_INVALID,
    /* 0x85 */ RTACPI_AML_OPC_INVALID,
    /* 0x86 */ RTACPI_AML_OPC_INVALID,
    /* 0x87 */ RTACPI_AML_OPC_INVALID,
    /* 0x88 */ RTACPI_AML_OPC_INVALID,
    /* 0x89 */ RTACPI_AML_OPC_INVALID,
    /* 0x8a */ RTACPI_AML_OPC_INVALID,
    /* 0x8b */ RTACPI_AML_OPC_INVALID,
    /* 0x8c */ RTACPI_AML_OPC_INVALID,
    /* 0x8d */ RTACPI_AML_OPC_INVALID,
    /* 0x8e */ RTACPI_AML_OPC_INVALID,
    /* 0x8f */ RTACPI_AML_OPC_INVALID,

    /* 0x90 */ RTACPI_AML_OPC_INVALID,
    /* 0x91 */ RTACPI_AML_OPC_INVALID,
    /* 0x92 */ RTACPI_AML_OPC_INVALID,
    /* 0x93 */ RTACPI_AML_OPC_INVALID,
    /* 0x94 */ RTACPI_AML_OPC_INVALID,
    /* 0x95 */ RTACPI_AML_OPC_INVALID,
    /* 0x96 */ RTACPI_AML_OPC_INVALID,
    /* 0x97 */ RTACPI_AML_OPC_INVALID,
    /* 0x98 */ RTACPI_AML_OPC_INVALID,
    /* 0x99 */ RTACPI_AML_OPC_INVALID,
    /* 0x9a */ RTACPI_AML_OPC_INVALID,
    /* 0x9b */ RTACPI_AML_OPC_INVALID,
    /* 0x9c */ RTACPI_AML_OPC_INVALID,
    /* 0x9d */ RTACPI_AML_OPC_INVALID,
    /* 0x9e */ RTACPI_AML_OPC_INVALID,
    /* 0x9f */ RTACPI_AML_OPC_INVALID,

    /* 0xa0 */ RTACPI_AML_OPC_INVALID,
    /* 0xa1 */ RTACPI_AML_OPC_INVALID,
    /* 0xa2 */ RTACPI_AML_OPC_INVALID,
    /* 0xa3 */ RTACPI_AML_OPC_INVALID,
    /* 0xa4 */ RTACPI_AML_OPC_INVALID,
    /* 0xa5 */ RTACPI_AML_OPC_INVALID,
    /* 0xa6 */ RTACPI_AML_OPC_INVALID,
    /* 0xa7 */ RTACPI_AML_OPC_INVALID,
    /* 0xa8 */ RTACPI_AML_OPC_INVALID,
    /* 0xa9 */ RTACPI_AML_OPC_INVALID,
    /* 0xaa */ RTACPI_AML_OPC_INVALID,
    /* 0xab */ RTACPI_AML_OPC_INVALID,
    /* 0xac */ RTACPI_AML_OPC_INVALID,
    /* 0xad */ RTACPI_AML_OPC_INVALID,
    /* 0xae */ RTACPI_AML_OPC_INVALID,
    /* 0xaf */ RTACPI_AML_OPC_INVALID,

    /* 0xb0 */ RTACPI_AML_OPC_INVALID,
    /* 0xb1 */ RTACPI_AML_OPC_INVALID,
    /* 0xb2 */ RTACPI_AML_OPC_INVALID,
    /* 0xb3 */ RTACPI_AML_OPC_INVALID,
    /* 0xb4 */ RTACPI_AML_OPC_INVALID,
    /* 0xb5 */ RTACPI_AML_OPC_INVALID,
    /* 0xb6 */ RTACPI_AML_OPC_INVALID,
    /* 0xb7 */ RTACPI_AML_OPC_INVALID,
    /* 0xb8 */ RTACPI_AML_OPC_INVALID,
    /* 0xb9 */ RTACPI_AML_OPC_INVALID,
    /* 0xba */ RTACPI_AML_OPC_INVALID,
    /* 0xbb */ RTACPI_AML_OPC_INVALID,
    /* 0xbc */ RTACPI_AML_OPC_INVALID,
    /* 0xbd */ RTACPI_AML_OPC_INVALID,
    /* 0xbe */ RTACPI_AML_OPC_INVALID,
    /* 0xbf */ RTACPI_AML_OPC_INVALID,

    /* 0xc0 */ RTACPI_AML_OPC_INVALID,
    /* 0xc1 */ RTACPI_AML_OPC_INVALID,
    /* 0xc2 */ RTACPI_AML_OPC_INVALID,
    /* 0xc3 */ RTACPI_AML_OPC_INVALID,
    /* 0xc4 */ RTACPI_AML_OPC_INVALID,
    /* 0xc5 */ RTACPI_AML_OPC_INVALID,
    /* 0xc6 */ RTACPI_AML_OPC_INVALID,
    /* 0xc7 */ RTACPI_AML_OPC_INVALID,
    /* 0xc8 */ RTACPI_AML_OPC_INVALID,
    /* 0xc9 */ RTACPI_AML_OPC_INVALID,
    /* 0xca */ RTACPI_AML_OPC_INVALID,
    /* 0xcb */ RTACPI_AML_OPC_INVALID,
    /* 0xcc */ RTACPI_AML_OPC_INVALID,
    /* 0xcd */ RTACPI_AML_OPC_INVALID,
    /* 0xce */ RTACPI_AML_OPC_INVALID,
    /* 0xcf */ RTACPI_AML_OPC_INVALID,

    /* 0xd0 */ RTACPI_AML_OPC_INVALID,
    /* 0xd1 */ RTACPI_AML_OPC_INVALID,
    /* 0xd2 */ RTACPI_AML_OPC_INVALID,
    /* 0xd3 */ RTACPI_AML_OPC_INVALID,
    /* 0xd4 */ RTACPI_AML_OPC_INVALID,
    /* 0xd5 */ RTACPI_AML_OPC_INVALID,
    /* 0xd6 */ RTACPI_AML_OPC_INVALID,
    /* 0xd7 */ RTACPI_AML_OPC_INVALID,
    /* 0xd8 */ RTACPI_AML_OPC_INVALID,
    /* 0xd9 */ RTACPI_AML_OPC_INVALID,
    /* 0xda */ RTACPI_AML_OPC_INVALID,
    /* 0xdb */ RTACPI_AML_OPC_INVALID,
    /* 0xdc */ RTACPI_AML_OPC_INVALID,
    /* 0xdd */ RTACPI_AML_OPC_INVALID,
    /* 0xde */ RTACPI_AML_OPC_INVALID,
    /* 0xdf */ RTACPI_AML_OPC_INVALID,

    /* 0xe0 */ RTACPI_AML_OPC_INVALID,
    /* 0xe1 */ RTACPI_AML_OPC_INVALID,
    /* 0xe2 */ RTACPI_AML_OPC_INVALID,
    /* 0xe3 */ RTACPI_AML_OPC_INVALID,
    /* 0xe4 */ RTACPI_AML_OPC_INVALID,
    /* 0xe5 */ RTACPI_AML_OPC_INVALID,
    /* 0xe6 */ RTACPI_AML_OPC_INVALID,
    /* 0xe7 */ RTACPI_AML_OPC_INVALID,
    /* 0xe8 */ RTACPI_AML_OPC_INVALID,
    /* 0xe9 */ RTACPI_AML_OPC_INVALID,
    /* 0xea */ RTACPI_AML_OPC_INVALID,
    /* 0xeb */ RTACPI_AML_OPC_INVALID,
    /* 0xec */ RTACPI_AML_OPC_INVALID,
    /* 0xed */ RTACPI_AML_OPC_INVALID,
    /* 0xee */ RTACPI_AML_OPC_INVALID,
    /* 0xef */ RTACPI_AML_OPC_INVALID,

    /* 0xf0 */ RTACPI_AML_OPC_INVALID,
    /* 0xf1 */ RTACPI_AML_OPC_INVALID,
    /* 0xf2 */ RTACPI_AML_OPC_INVALID,
    /* 0xf3 */ RTACPI_AML_OPC_INVALID,
    /* 0xf4 */ RTACPI_AML_OPC_INVALID,
    /* 0xf5 */ RTACPI_AML_OPC_INVALID,
    /* 0xf6 */ RTACPI_AML_OPC_INVALID,
    /* 0xf7 */ RTACPI_AML_OPC_INVALID,
    /* 0xf8 */ RTACPI_AML_OPC_INVALID,
    /* 0xf9 */ RTACPI_AML_OPC_INVALID,
    /* 0xfa */ RTACPI_AML_OPC_INVALID,
    /* 0xfb */ RTACPI_AML_OPC_INVALID,
    /* 0xfc */ RTACPI_AML_OPC_INVALID,
    /* 0xfd */ RTACPI_AML_OPC_INVALID,
    /* 0xfe */ RTACPI_AML_OPC_INVALID,
    /* 0xff */ RTACPI_AML_OPC_INVALID
};


static int rtAcpiTblAmlDecodeSimple(PRTACPITBLAMLDECODE pThis, PCRTACPIAMLOPC pAmlOpc, uint8_t bOpc, RTVFSIOSTREAM hVfsIosOut, PRTERRINFO pErrInfo)
{
    RT_NOREF(bOpc);

    int rc;

    /* Decode any package length field first. */
    size_t cbPkg         = 0;
    size_t cbPkgLength   = 0;
    size_t cbPkgConsumed = 0;
    if (pAmlOpc->fFlags & RTACPI_AML_OPC_F_HAS_PKG_LENGTH)
    {
        rc = rtAcpiTblAmlDecodePkgLength(pThis, &cbPkg, &cbPkgLength, pErrInfo);
        if (RT_FAILURE(rc)) return rc;

        cbPkgConsumed += cbPkgLength;
    }

    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s", pAmlOpc->pszOpc);
    if (RT_FAILURE(rc)) return rc;

    /* Any arguments? */
    if (pAmlOpc->aenmTypes[0] != kAcpiAmlOpcType_Invalid)
    {
        bool fOld = pThis->fIndent;
        pThis->fIndent = false;

        rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, " (");
        if (RT_FAILURE(rc)) return rc;

        for (uint32_t i = 0; i < RT_ELEMENTS(pAmlOpc->aenmTypes); i++)
        {
            if (pAmlOpc->aenmTypes[i] == kAcpiAmlOpcType_Invalid)
                break; /* End of arguments. */

            if (i > 0)
            {
                rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ", ");
                if (RT_FAILURE(rc)) return rc;
            }

            switch (pAmlOpc->aenmTypes[i])
            {
                case kAcpiAmlOpcType_Byte:
                {
                    uint8_t bVal = 0; /* shut up gcc */
                    rc = rtAcpiTblAmlDecodeReadU8(pThis, &bVal, pErrInfo);
                    if (RT_FAILURE(rc)) return rc;

                    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%RU8", bVal);
                    if (RT_FAILURE(rc)) return rc;

                    cbPkgConsumed++;
                    break;
                }
                case kAcpiAmlOpcType_Word:
                {
                    uint16_t u16Val = 0; /* shut up gcc */
                    rc = rtAcpiTblAmlDecodeReadU16(pThis, &u16Val, pErrInfo);
                    if (RT_FAILURE(rc)) return rc;

                    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%RX16", u16Val);
                    if (RT_FAILURE(rc)) return rc;

                    cbPkgConsumed += sizeof(uint16_t);
                    break;
                }
                case kAcpiAmlOpcType_DWord:
                {
                    uint32_t u32Val = 0; /* shut up gcc */
                    rc = rtAcpiTblAmlDecodeReadU32(pThis, &u32Val, pErrInfo);
                    if (RT_FAILURE(rc)) return rc;

                    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%RX32", u32Val);
                    if (RT_FAILURE(rc)) return rc;

                    cbPkgConsumed += sizeof(uint32_t);
                    break;
                }
                case kAcpiAmlOpcType_NameString:
                {
                    char szName[512];
                    size_t cbName = 0;
                    rc = rtAcpiTblAmlDecodeNameString(pThis, &szName[0], sizeof(szName), &cbName, pErrInfo);
                    if (RT_FAILURE(rc)) return rc;

                    PRTACPITBLAMLOBJ pIt;
                    bool fFound = false;
                    RTListForEach(&pThis->LstObjs, pIt, RTACPITBLAMLOBJ, NdObjs)
                    {
                        if (!strcmp(pIt->szName, szName))
                        {
                            fFound = true;
                            break;
                        }
                    }

                    if (fFound)
                    {
                        if (pIt->enmType == kAcpiAmlObjType_Method)
                        {
                            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s(", szName);
                            if (RT_FAILURE(rc)) return rc;

                            bool fIndentOld = pThis->fIndent;
                            pThis->fIndent = false;

                            size_t offTblOrig = pThis->offTbl;
                            for (uint32_t iArg = 0; iArg < pIt->u.cMethodArgs; iArg++)
                            {
                                rc = rtAcpiTblAmlDecodeTerminal(pThis, hVfsIosOut, pErrInfo);
                                if (RT_FAILURE(rc)) return rc;

                                if (iArg < pIt->u.cMethodArgs - 1)
                                {
                                    rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ", ", szName);
                                    if (RT_FAILURE(rc)) return rc;
                                }
                            }
                            cbPkgConsumed += pThis->offTbl - offTblOrig;

                            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ")");
                            pThis->fIndent = fIndentOld;
                        }
                        else
                            rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s", szName);
                    }
                    else
                        rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, "%s", szName);
                    if (RT_FAILURE(rc)) return rc;


                    cbPkgConsumed += cbName;
                    break;
                }
                case kAcpiAmlOpcType_SuperName:
                case kAcpiAmlOpcType_TermArg:
                {
                    /** @todo SuperName has limited allowed arguments. */
                    size_t offTblOrig = pThis->offTbl;

                    rc = rtAcpiTblAmlDecodeTerminal(pThis, hVfsIosOut, pErrInfo);
                    if (RT_FAILURE(rc)) return rc;

                    cbPkgConsumed += pThis->offTbl - offTblOrig;
                    break;
                }
                case kAcpiAmlOpcType_Invalid:
                default:
                    AssertReleaseFailed();
            }
        }

        rc = rtAcpiTblAmlDecodeFormat(pThis, hVfsIosOut, ")");
        if (RT_FAILURE(rc)) return rc;

        pThis->fIndent = fOld;
    }

    if (pAmlOpc->fFlags & RTACPI_AML_OPC_F_HAS_PKG_LENGTH)
    {
        if (cbPkg < cbPkgConsumed)
            return RTErrInfoSetF(pErrInfo, VERR_BUFFER_OVERFLOW, "Opcode arguments consumed more than the package length indicated (%zu vs %zu)", cbPkg, cbPkgConsumed);
        rc = rtAcpiTblAmlDecodePkgPush(pThis, hVfsIosOut, cbPkg - cbPkgConsumed, pErrInfo);
    }

    return rc;
}

static int rtAcpiTblAmlDecode(PRTACPITBLAMLDECODE pThis, PCRTACPIAMLOPC pAmlOpc, uint8_t bOpc, RTVFSIOSTREAM hVfsIosOut, PRTERRINFO pErrInfo)
{
    if (pAmlOpc->pszOpc)
    {
        if (pAmlOpc->pfnDecode)
            return pAmlOpc->pfnDecode(pThis, hVfsIosOut, bOpc, pErrInfo);
        return rtAcpiTblAmlDecodeSimple(pThis, pAmlOpc, bOpc, hVfsIosOut, pErrInfo);
    }

    return RTErrInfoSetF(pErrInfo, VERR_INVALID_STATE, "Invalid opcode %#x in ACPI table at offset %u", bOpc, pThis->offTbl);
}


static int rtAcpiTblAmlDecodeTerminal(PRTACPITBLAMLDECODE pThis, RTVFSIOSTREAM hVfsIosOut, PRTERRINFO pErrInfo)
{
    PCRTACPIAMLOPC pAmlOpc = NULL;
    uint8_t bOpc = 0; /* shut up gcc */
    int rc = rtAcpiTblAmlDecodeReadU8(pThis, &bOpc, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (bOpc == ACPI_AML_BYTE_CODE_PREFIX_EXT_OP)
        {
            rc = rtAcpiTblAmlDecodeReadU8(pThis, &bOpc, pErrInfo);
            if (RT_FAILURE(rc))
                return rc;

            pAmlOpc = &g_aAmlExtOpcodeDecode[bOpc];
        }
        else
            pAmlOpc = &g_aAmlOpcodeDecode[bOpc];

        return rtAcpiTblAmlDecode(pThis, pAmlOpc, bOpc, hVfsIosOut, pErrInfo);
    }

    return rc;
}


DECLHIDDEN(int) rtAcpiTblConvertFromAmlToAsl(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo)
{
    ACPITBLHDR Hdr;
    int rc = RTVfsIoStrmRead(hVfsIosIn, &Hdr, sizeof(Hdr), true /*fBlocking*/, NULL /*pcbRead*/);
    if (RT_SUCCESS(rc))
    {
        Hdr.u32Signature       = RT_LE2H_U32(Hdr.u32Signature);
        Hdr.cbTbl              = RT_LE2H_U32(Hdr.cbTbl);
        Hdr.u32OemRevision     = RT_LE2H_U32(Hdr.u32OemRevision);
        Hdr.u32CreatorRevision = RT_LE2H_U32(Hdr.u32CreatorRevision);

        if (   Hdr.u32Signature == ACPI_TABLE_HDR_SIGNATURE_SSDT
            || Hdr.u32Signature == ACPI_TABLE_HDR_SIGNATURE_DSDT)
        {
            uint8_t *pbTbl = (uint8_t *)RTMemAlloc(Hdr.cbTbl);
            if (pbTbl)
            {
                rc = RTVfsIoStrmRead(hVfsIosIn, pbTbl, Hdr.cbTbl - sizeof(Hdr), true /*fBlocking*/, NULL /*pcbRead*/);
                if (RT_SUCCESS(rc))
                {
                    /** @todo Verify checksum */
                    ssize_t cch = RTVfsIoStrmPrintf(hVfsIosOut, "DefinitionBlock(\"\", \"%s\", %u, \"%.6s\", \"%.8s\", %u)",
                                                    Hdr.u32Signature == ACPI_TABLE_HDR_SIGNATURE_SSDT ? "SSDT" : "DSDT",
                                                    1, &Hdr.abOemId[0], &Hdr.abOemTblId[0], Hdr.u32OemRevision);
                    if (cch > 0)
                    {
                        RTACPITBLAMLDECODE AmlDecode;
                        AmlDecode.pbTbl        = pbTbl;
                        AmlDecode.cbTbl        = Hdr.cbTbl - sizeof(Hdr);
                        AmlDecode.offTbl       = 0;
                        AmlDecode.iLvl         = 0;
                        AmlDecode.cPkgStackMax = 0;
                        AmlDecode.pacbPkgLeft  = NULL;
                        AmlDecode.fIndent      = true;
                        RTListInit(&AmlDecode.LstObjs);
                        rc = rtAcpiTblAmlDecodePkgPush(&AmlDecode, hVfsIosOut, AmlDecode.cbTbl, pErrInfo);
                        while (   RT_SUCCESS(rc)
                               && AmlDecode.offTbl < Hdr.cbTbl - sizeof(Hdr))
                        {
                            rc = rtAcpiTblAmlDecodeTerminal(&AmlDecode, hVfsIosOut, pErrInfo);
                            if (RT_SUCCESS(rc))
                                rc = rtAcpiTblAmlDecodePkgPop(&AmlDecode, hVfsIosOut, pErrInfo);
                        }
                        if (AmlDecode.pacbPkgLeft)
                            RTMemFree(AmlDecode.pacbPkgLeft);

                        PRTACPITBLAMLOBJ pIt, pItNext;
                        RTListForEachSafe(&AmlDecode.LstObjs, pIt, pItNext, RTACPITBLAMLOBJ, NdObjs)
                        {
                            RTListNodeRemove(&pIt->NdObjs);
                            RTMemFree(pIt);
                        } 

                        if (RT_SUCCESS(rc))
                        {
                            cch = RTVfsIoStrmPrintf(hVfsIosOut, "}\n");
                            if (cch <= 0)
                                rc = RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : (int)cch, "Failed to emit closing definition block");
                        }
                    }
                    else
                        rc = RTErrInfoSetF(pErrInfo, cch == 0 ? VERR_NO_MEMORY : (int)cch, "Failed to emit DefinitionBlock()");
                }
                else
                    rc = RTErrInfoSetF(pErrInfo, rc, "Reading %u bytes of the ACPI table failed", Hdr.cbTbl);

                RTMemFree(pbTbl);
                pbTbl = NULL;
            }
            else
                rc = RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Allocating memory for the ACPI table failed");
        }
        else
            rc = RTErrInfoSetF(pErrInfo, VERR_NOT_SUPPORTED, "Only DSDT and SSDT ACPI tables are supported");
    }
    else
        rc = RTErrInfoSetF(pErrInfo, rc, "Reading the ACPI table header failed with %Rrc", rc);

    return rc;
}
