/* $Id$ */
/** @file
 * VBox WDDM Miniport registry related functions
 */

/*
 * Copyright (C) 2011-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "common/VBoxMPCommon.h"

VP_STATUS VBoxMPCmnRegInit(IN PVBOXMP_DEVEXT pExt, OUT VBOXMPCMNREGISTRY *pReg)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vboxWddmRegQueryDrvKeyName(pExt, cbBuf, Buf, &cbBuf);
    AssertNtStatusSuccess(Status);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmRegOpenKey(pReg, Buf, GENERIC_READ | GENERIC_WRITE);
        AssertNtStatusSuccess(Status);
        if(Status == STATUS_SUCCESS)
            return NO_ERROR;
    }

    /* fall-back to make the subsequent VBoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *pReg = NULL;

    return ERROR_INVALID_PARAMETER;
}

VP_STATUS VBoxMPCmnRegFini(IN VBOXMPCMNREGISTRY Reg)
{
    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = ZwClose(Reg);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VBoxMPCmnRegQueryDword(IN VBOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t *pVal)
{
    /* seems like the new code assumes the Reg functions zeroes up the value on failure */
    *pVal = 0;

    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = vboxWddmRegQueryValueDword(Reg, pName, (PDWORD)pVal);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VBoxMPCmnRegSetDword(IN VBOXMPCMNREGISTRY Reg, PWSTR pName, uint32_t Val)
{
    if (!Reg)
    {
        return ERROR_INVALID_PARAMETER;
    }

    NTSTATUS Status = vboxWddmRegSetValueDword(Reg, pName, Val);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}
