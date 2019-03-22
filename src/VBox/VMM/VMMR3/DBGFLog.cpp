/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Log Manager.
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
#include <VBox/vmm/vmapi.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/uvm.h>
#include <VBox/vmm/vm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>


/**
 * Checkes for logger prefixes and selects the right logger.
 *
 * @returns Target logger.
 * @param   ppsz                Pointer to the string pointer.
 */
static PRTLOGGER dbgfR3LogResolvedLogger(const char **ppsz)
{
    PRTLOGGER   pLogger;
    const char *psz = *ppsz;
    if (!strncmp(psz, RT_STR_TUPLE("release:")))
    {
        *ppsz += sizeof("release:") - 1;
        pLogger = RTLogRelGetDefaultInstance();
    }
    else
    {
        if (!strncmp(psz, RT_STR_TUPLE("debug:")))
            *ppsz += sizeof("debug:") - 1;
        pLogger = RTLogDefaultInstance();
    }
    return pLogger;
}


/**
 * EMT worker for DBGFR3LogModifyGroups.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 */
static DECLCALLBACK(int) dbgfR3LogModifyGroups(PUVM pUVM, const char *pszGroupSettings)
{
    PRTLOGGER pLogger = dbgfR3LogResolvedLogger(&pszGroupSettings);
    if (!pLogger)
        return VINF_SUCCESS;

    int rc = RTLogGroupSettings(pLogger, pszGroupSettings);
    if (RT_SUCCESS(rc) && pUVM->pVM)
    {
        VM_ASSERT_VALID_EXT_RETURN(pUVM->pVM, VERR_INVALID_VM_HANDLE);
        rc = VMMR3UpdateLoggers(pUVM->pVM);
    }
    return rc;
}


/**
 * Changes the logger group settings.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszGroupSettings    The group settings string. (VBOX_LOG)
 *                              By prefixing the string with \"release:\" the
 *                              changes will be applied to the release log
 *                              instead of the debug log.  The prefix \"debug:\"
 *                              is also recognized.
 */
VMMR3DECL(int) DBGFR3LogModifyGroups(PUVM pUVM, const char *pszGroupSettings)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pszGroupSettings, VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWaitU(pUVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyGroups, 2, pUVM, pszGroupSettings);
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 */
static DECLCALLBACK(int) dbgfR3LogModifyFlags(PUVM pUVM, const char *pszFlagSettings)
{
    PRTLOGGER pLogger = dbgfR3LogResolvedLogger(&pszFlagSettings);
    if (!pLogger)
        return VINF_SUCCESS;

    int rc = RTLogFlags(pLogger, pszFlagSettings);
    if (RT_SUCCESS(rc) && pUVM->pVM)
    {
        VM_ASSERT_VALID_EXT_RETURN(pUVM->pVM, VERR_INVALID_VM_HANDLE);
        rc = VMMR3UpdateLoggers(pUVM->pVM);
    }
    return rc;
}


/**
 * Changes the logger flag settings.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszFlagSettings     The group settings string. (VBOX_LOG_FLAGS)
 *                              By prefixing the string with \"release:\" the
 *                              changes will be applied to the release log
 *                              instead of the debug log.  The prefix \"debug:\"
 *                              is also recognized.
 */
VMMR3DECL(int) DBGFR3LogModifyFlags(PUVM pUVM, const char *pszFlagSettings)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pszFlagSettings, VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWaitU(pUVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyFlags, 2, pUVM, pszFlagSettings);
}


/**
 * EMT worker for DBGFR3LogModifyFlags.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 */
static DECLCALLBACK(int) dbgfR3LogModifyDestinations(PUVM pUVM, const char *pszDestSettings)
{
    PRTLOGGER pLogger = dbgfR3LogResolvedLogger(&pszDestSettings);
    if (!pLogger)
        return VINF_SUCCESS;

    int rc = RTLogDestinations(NULL, pszDestSettings);
    if (RT_SUCCESS(rc) && pUVM->pVM)
    {
        VM_ASSERT_VALID_EXT_RETURN(pUVM->pVM, VERR_INVALID_VM_HANDLE);
        rc = VMMR3UpdateLoggers(pUVM->pVM);
    }
    return rc;
}


/**
 * Changes the logger destination settings.
 *
 * @returns VBox status code.
 * @param   pUVM                The user mode VM handle.
 * @param   pszDestSettings     The destination settings string. (VBOX_LOG_DEST)
 *                              By prefixing the string with \"release:\" the
 *                              changes will be applied to the release log
 *                              instead of the debug log.  The prefix \"debug:\"
 *                              is also recognized.
 */
VMMR3DECL(int) DBGFR3LogModifyDestinations(PUVM pUVM, const char *pszDestSettings)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    AssertPtrReturn(pszDestSettings, VERR_INVALID_POINTER);

    return VMR3ReqPriorityCallWaitU(pUVM, VMCPUID_ANY, (PFNRT)dbgfR3LogModifyDestinations, 2, pUVM, pszDestSettings);
}

