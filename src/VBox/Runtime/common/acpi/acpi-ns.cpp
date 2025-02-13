/* $Id$ */
/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) namespace handling.
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
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "internal/acpi.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

static void rtAcpiNsEntryDestroy(PRTACPINSENTRY pNsEntry)
{
    PRTACPINSENTRY pIt, pItNext;
    RTListForEachSafe(&pNsEntry->LstNsEntries, pIt, pItNext, RTACPINSENTRY, NdNs)
    {
        RTListNodeRemove(&pIt->NdNs);
        rtAcpiNsEntryDestroy(pIt);
    }
    RTMemFree(pNsEntry);
}


/**
 * Worker for looking up the entry in the given namespace for a given namestring.
 *
 * @returns Pointer to the namespace entry if found or NULL if not.
 * @param   pNsRoot             The namespace to search in.
 * @param   pszNameString       The namestring to search.
 * @param   fExcludeLast        Flag whether to exclude the last name segment from the search and return
 *                              the pointer to the second to last entry.
 * @param   ppszNameSegLast     Where to store the pointe to the last name segment on success (if fExcludeLast is true).
 */
static PRTACPINSENTRY rtAcpiNsLookupWorker(PRTACPINSROOT pNsRoot, const char *pszNameString, bool fExcludeLast,
                                           const char **ppszNameSegLast)
{
    AssertReturn(*pszNameString != '\0', NULL);

    /* Find the starting position. */
    PRTACPINSENTRY pNsEntry;
    const char *pszCur = pszNameString;
    if (*pszCur == '\\')
    {
        /* Search from the root of the namespace. */
        pNsEntry = &pNsRoot->RootEntry;
        pszCur++;
    }
    else if (*pszCur == '^')
    {
        /* Go up the tree. */
        pNsEntry = pNsRoot->aNsStack[pNsRoot->idxNsStack];
        while (*pszCur == '^')
        {
            if (!pNsEntry->pParent) /* Too many levels up. */
                return NULL;
            pNsEntry = pNsEntry->pParent;
            pszCur++;
        }
    }
    else
        pNsEntry = pNsRoot->aNsStack[pNsRoot->idxNsStack];

    /* This ASSUMES the namestring has always full 4 character name segments and is well formed. */
    do
    {
        Assert(pszCur[0] != '\0' && pszCur[1] != '\0' && pszCur[2] != '\0' && pszCur[3] != '\0');

        if (fExcludeLast && pszCur[4] == '\0')
            break;

        PRTACPINSENTRY pIt;
        bool fFound = false;
        RTListForEach(&pNsEntry->LstNsEntries, pIt, RTACPINSENTRY, NdNs)
        {
            if (!memcmp(&pIt->achNameSeg[0], pszCur, sizeof(pIt->achNameSeg[0])))
            {
                pNsEntry = pIt;
                fFound = true;
                break;
            }
        }

        /* The name path is invalid. */
        if (!fFound)
            return NULL;

        pszCur += 4;
    } while (*pszCur++ == '.');
    AssertReturn(   *pszCur == '\0'
                 || (   fExcludeLast
                     && pszCur[4] == '\0'),
                 NULL);

    if (ppszNameSegLast)
        *ppszNameSegLast = pszCur;
    return pNsEntry;
}


/**
 * Adds a new entry in the given namespace under the given path.
 *
 * @returns IPRT status code.
 * @param   pNsRoot             The namespace to add the new entry to.
 * @param   pszNameString       The namestring to add.
 * @param   fSwitchTo           Flag whether to switch to the new entry.
 * @param   ppNsEntry           Where to store the pointer to the created entry on success.
 */
static int rtAcpiNsAddEntryWorker(PRTACPINSROOT pNsRoot, const char *pszNameString, bool fSwitchTo, PRTACPINSENTRY *ppNsEntry)
{
    AssertReturn(   !fSwitchTo
                 || pNsRoot->idxNsStack < RT_ELEMENTS(pNsRoot->aNsStack),
                 VERR_INVALID_STATE);

    int rc;
    const char *pszNameSegLast = NULL;
    PRTACPINSENTRY pNsEntryParent = rtAcpiNsLookupWorker(pNsRoot, pszNameString, true /*fExcludeLast*/, &pszNameSegLast);
    if (pNsEntryParent)
    {
        PRTACPINSENTRY pNsEntry = (PRTACPINSENTRY)RTMemAllocZ(sizeof(*pNsEntry));
        if (pNsEntry)
        {
            pNsEntry->pParent = pNsEntryParent;
            RTListInit(&pNsEntry->LstNsEntries);

            memcpy(&pNsEntry->achNameSeg[0], pszNameSegLast, sizeof(pNsEntry->achNameSeg));

            RTListAppend(&pNsEntryParent->LstNsEntries, &pNsEntry->NdNs);
            *ppNsEntry = pNsEntry;
            if (fSwitchTo)
                pNsRoot->aNsStack[++pNsRoot->idxNsStack] = pNsEntry;
            return VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_NOT_FOUND;

    return rc;
}


DECLHIDDEN(PRTACPINSROOT) rtAcpiNsCreate(void)
{
    PRTACPINSROOT pNsRoot = (PRTACPINSROOT)RTMemAllocZ(sizeof(*pNsRoot));
    if (pNsRoot)
    {
        RTListInit(&pNsRoot->RootEntry.LstNsEntries);
        pNsRoot->RootEntry.pParent = NULL;
        pNsRoot->idxNsStack        = 0;
        pNsRoot->aNsStack[pNsRoot->idxNsStack] = &pNsRoot->RootEntry;
    }
    return pNsRoot;
}


DECLHIDDEN(void) rtAcpiNsDestroy(PRTACPINSROOT pNsRoot)
{
    PRTACPINSENTRY pIt, pItNext;
    RTListForEachSafe(&pNsRoot->RootEntry.LstNsEntries, pIt, pItNext, RTACPINSENTRY, NdNs)
    {
        RTListNodeRemove(&pIt->NdNs);
        rtAcpiNsEntryDestroy(pIt);
    }
    RTMemFree(pNsRoot);
}


DECLHIDDEN(int) rtAcpiNsAddEntryAstNode(PRTACPINSROOT pNsRoot, const char *pszNameString, PCRTACPIASTNODE pAstNd, bool fSwitchTo)
{
    PRTACPINSENTRY pNsEntry = NULL;
    int rc = rtAcpiNsAddEntryWorker(pNsRoot, pszNameString, fSwitchTo, &pNsEntry);
    if (RT_SUCCESS(rc))
    {
        pNsEntry->fAstNd = true;
        pNsEntry->pAstNd = pAstNd;
    }

    return rc;
}


DECLHIDDEN(int) rtAcpiNsAddEntryRsrcField(PRTACPINSROOT pNsRoot, const char *pszNameString, uint32_t offBits, uint32_t cBits)
{
    PRTACPINSENTRY pNsEntry = NULL;
    int rc = rtAcpiNsAddEntryWorker(pNsRoot, pszNameString, false /*fSwitchTo*/, &pNsEntry);
    if (RT_SUCCESS(rc))
    {
        pNsEntry->fAstNd  = false;
        pNsEntry->pAstNd  = NULL;
        pNsEntry->offBits = offBits;
        pNsEntry->cBits   = cBits;
    }

    return rc;
}


DECLHIDDEN(int) rtAcpiNsAddEntryExternal(PRTACPINSROOT pNsRoot, const char *pszNameString, PCRTACPIASLEXTERNAL pExternal)
{
    PRTACPINSENTRY pNsEntry = NULL;
    int rc = rtAcpiNsAddEntryWorker(pNsRoot, pszNameString, false /*fSwitchTo*/, &pNsEntry);
    if (RT_SUCCESS(rc))
    {
        pNsEntry->fAstNd    = false;
        pNsEntry->pExternal = pExternal;
    }

    return rc;
}


DECLHIDDEN(int) rtAcpiNsQueryNamePathForNameString(PRTACPINSROOT pNsRoot, const char *pszNameString, char *pachNamePath, size_t *pcchNamePath)
{
    AssertReturn(!pachNamePath || *pcchNamePath >= 6, VERR_INVALID_PARAMETER); /* Needs to support at least \XXXX and the zero terminator. */

    const char *pszNameSegLast = NULL;
    PCRTACPINSENTRY pNsEntry = rtAcpiNsLookupWorker(pNsRoot, pszNameString, true /*fExcludeLast*/, &pszNameSegLast);
    if (pNsEntry)
    {
        int rc = VERR_BUFFER_OVERFLOW;
        size_t cchNamePath = 1; /* For the root prefix. */

        if (!pachNamePath)
        {
            /* Calculate the name path size based on the number of segments. */
            uint32_t cEntries = 0;
            do
            {
                cEntries++;
                pNsEntry = pNsEntry->pParent;
            } while (pNsEntry);

            cchNamePath += cEntries * (4 + 1) - 1; /* XXXX., except for the last one. */
        }
        else
        {
            uint32_t idxEntry = 0;
            PCRTACPINSENTRY aNsEntries[255]; /* Maximum amount of name segments possible. */
            do
            {
                aNsEntries[idxEntry++] = pNsEntry;
                pNsEntry = pNsEntry->pParent;
            } while (pNsEntry);

            char *pch = pachNamePath;
            *pch++ = '\\';
            *pch   = '\0';

            /* The last entry must be the root entry. */
            idxEntry--;
            Assert(!aNsEntries[idxEntry]->pParent);

            while (idxEntry)
            {
                pNsEntry = aNsEntries[--idxEntry];
                if (cchNamePath + 5 < *pcchNamePath)
                {
                    pch[0] = pNsEntry->achNameSeg[0];
                    pch[1] = pNsEntry->achNameSeg[1];
                    pch[2] = pNsEntry->achNameSeg[2];
                    pch[3] = pNsEntry->achNameSeg[3];
                    pch[4] = '.';
                    pch += 5;
                }
                cchNamePath += 5;
            }

            /* Append the last name segment. */
            if (cchNamePath + 5 < *pcchNamePath)
            {
                pch[0] = pszNameSegLast[0];
                pch[1] = pszNameSegLast[1];
                pch[2] = pszNameSegLast[2];
                pch[3] = pszNameSegLast[3];
                pch[4] = '\0';
                cchNamePath += 4;
            }

            if (cchNamePath <= *pcchNamePath)
                rc = VINF_SUCCESS;
        }

        *pcchNamePath = cchNamePath;
        return rc;
    }

    *pcchNamePath = 0;
    return VERR_NOT_FOUND;
}


DECLHIDDEN(int) rtAcpiNsPop(PRTACPINSROOT pNsRoot)
{
    AssertReturn(pNsRoot->idxNsStack, VERR_INVALID_STATE); /* The root can't be popped from the stack. */
    pNsRoot->idxNsStack--;
    return VINF_SUCCESS;
}


DECLHIDDEN(PCRTACPINSENTRY) rtAcpiNsLookup(PRTACPINSROOT pNsRoot, const char *pszNameString)
{
    return rtAcpiNsLookupWorker(pNsRoot, pszNameString, false /*fExcludeLast*/, NULL /*ppszNameSegLast*/);
}
