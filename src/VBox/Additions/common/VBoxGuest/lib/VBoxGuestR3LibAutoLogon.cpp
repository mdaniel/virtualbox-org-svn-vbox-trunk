/* $Id$ */
/** @file
 * VBoxGuestR3LibAutoLogon - Ring-3 utility functions for auto-logon modules
 *                           (VBoxGINA / VBoxCredProv / pam_vbox).
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
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
#ifdef RT_OS_WINDOWS
# include <iprt/win/windows.h>
#endif

#include "VBoxGuestR3LibInternal.h"
#include <iprt/errcore.h>


/**
 * Reports the current auto-logon status to the host.
 *
 * This makes sure that the Failed state is sticky.
 *
 * @return  IPRT status code.
 * @param   enmStatus               Status to report to the host.
 */
VBGLR3DECL(int) VbglR3AutoLogonReportStatus(VBoxGuestFacilityStatus enmStatus)
{
    /*
     * VBoxGuestFacilityStatus_Failed is sticky.
     */
    static VBoxGuestFacilityStatus s_enmLastStatus = VBoxGuestFacilityStatus_Inactive;
    if (s_enmLastStatus != VBoxGuestFacilityStatus_Failed)
    {
        int rc = VbglR3ReportAdditionsStatus(VBoxGuestFacilityType_AutoLogon, enmStatus, 0 /* Flags */);
        if (rc == VERR_NOT_SUPPORTED)
        {
            /*
             * To maintain backwards compatibility to older hosts which don't have
             * VMMDevReportGuestStatus implemented we set the appropriate status via
             * guest property to have at least something.
             */
#ifdef VBOX_WITH_GUEST_PROPS
            HGCMCLIENTID idClient = 0;
            rc = VbglR3GuestPropConnect(&idClient);
            if (RT_SUCCESS(rc))
            {
                const char *pszStatus;
                switch (enmStatus)
                {
                    case VBoxGuestFacilityStatus_Inactive:      pszStatus = "Inactive"; break;
                    case VBoxGuestFacilityStatus_Paused:        pszStatus = "Disabled"; break;
                    case VBoxGuestFacilityStatus_PreInit:       pszStatus = "PreInit"; break;
                    case VBoxGuestFacilityStatus_Init:          pszStatus = "Init"; break;
                    case VBoxGuestFacilityStatus_Active:        pszStatus = "Active"; break;
                    case VBoxGuestFacilityStatus_Terminating:   pszStatus = "Terminating"; break;
                    case VBoxGuestFacilityStatus_Terminated:    pszStatus = "Terminated"; break;
                    case VBoxGuestFacilityStatus_Failed:        pszStatus = "Failed"; break;
                    default:                                    pszStatus = NULL;
                }
                if (pszStatus)
                {
                    /*
                     * Use TRANSRESET when possible, fall back to TRANSIENT
                     * (generally sufficient unless the guest misbehaves).
                     */
                    static const char s_szPath[] = "/VirtualBox/GuestInfo/OS/AutoLogonStatus";
                    rc = VbglR3GuestPropWrite(idClient, s_szPath, pszStatus, "TRANSRESET");
                    if (rc == VERR_PARSE_ERROR)
                        rc = VbglR3GuestPropWrite(idClient, s_szPath, pszStatus, "TRANSIENT");
                }
                else
                    rc = VERR_INVALID_PARAMETER;

                VbglR3GuestPropDisconnect(idClient);
            }
#endif
        }

        s_enmLastStatus = enmStatus;
    }
    return VINF_SUCCESS;
}


/**
 * Detects whether our process is running in a remote session or not.
 *
 * @return  bool        true if running in a remote session, false if not.
 */
VBGLR3DECL(bool) VbglR3AutoLogonIsRemoteSession(void)
{
#ifdef RT_OS_WINDOWS
    return GetSystemMetrics(SM_REMOTESESSION) != 0 ? true : false;
#else
    return false; /* Not implemented. */
#endif
}

