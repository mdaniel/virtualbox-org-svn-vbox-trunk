/* $Id$ */
/** @file
 * Small tool to (un)install the VBoxGuest device driver.
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
#include <iprt/win/windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <VBox/VBoxGuest.h> /* for VBOXGUEST_SERVICE_NAME */


//#define TESTMODE



static int installDriver(bool fStartIt)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    SC_HANDLE   hSMgrCreate = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    if (!hSMgrCreate)
    {
        printf("OpenSCManager(,,create) failed rc=%d\n", GetLastError());
        return -1;
    }

    const char * const pszSlashName = (GetVersion() & 0xff) < 4 ? "\\VBoxGuestNT3.sys" : "\\VBoxGuestNT.sys";
    char szDriver[MAX_PATH * 2];
    GetCurrentDirectory(MAX_PATH, szDriver);
    strcat(szDriver, pszSlashName);
    if (GetFileAttributesA(szDriver) == INVALID_FILE_ATTRIBUTES)
    {
        GetSystemDirectory(szDriver, sizeof(szDriver));
        strcat(strcat(szDriver, "\\drivers"), pszSlashName);
    }

    SC_HANDLE hService = CreateService(hSMgrCreate,
                                       VBOXGUEST_SERVICE_NAME,
                                       "VBoxGuest Support Driver",
                                       SERVICE_QUERY_STATUS | (fStartIt ? SERVICE_START : 0),
                                       SERVICE_KERNEL_DRIVER,
                                       SERVICE_BOOT_START,
                                       SERVICE_ERROR_NORMAL,
                                       szDriver,
                                       "System",
                                       NULL, NULL, NULL, NULL);
    if (hService)
    {
        printf("Successfully created service '%s' for driver '%s'.\n", VBOXGUEST_SERVICE_NAME, szDriver);
        if (fStartIt)
        {
            if (StartService(hService, 0, NULL))
                printf("successfully started driver '%s'\n", szDriver);
            else
                printf("StartService failed: %d\n", GetLastError(), szDriver);
        }
        CloseServiceHandle(hService);
    }
    else
        printf("CreateService failed! lasterr=%d (szDriver=%s)\n", GetLastError(), szDriver);
    CloseServiceHandle(hSMgrCreate);
    return hService ? 0 : -1;
}

static int uninstallDriver(void)
{
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    if (!hSMgr)
    {
        printf("OpenSCManager(,,delete) failed rc=%d\n", GetLastError());
        return -1;
    }
    SC_HANDLE hService = OpenService(hSMgr, VBOXGUEST_SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);
    if (hService)
    {
        /*
         * Try stop it if it's running.
         */
        SERVICE_STATUS  Status = { 0, 0, 0, 0, 0, 0, 0 };
        QueryServiceStatus(hService, &Status);
        if (Status.dwCurrentState == SERVICE_STOPPED)
            rc = VINF_SUCCESS;
        else if (ControlService(hService, SERVICE_CONTROL_STOP, &Status))
        {
            int iWait = 100;
            while (Status.dwCurrentState == SERVICE_STOP_PENDING && iWait-- > 0)
            {
                Sleep(100);
                QueryServiceStatus(hService, &Status);
            }
            if (Status.dwCurrentState == SERVICE_STOPPED)
                rc = VINF_SUCCESS;
            else
            {
                printf("Failed to stop service. status=%d (%#x)\n", Status.dwCurrentState, Status.dwCurrentState);
                rc = VERR_GENERAL_FAILURE;
            }
        }
        else
        {
            DWORD dwErr = GetLastError();
            if (   Status.dwCurrentState == SERVICE_STOP_PENDING
                && dwErr == ERROR_SERVICE_CANNOT_ACCEPT_CTRL)
                rc = VERR_RESOURCE_BUSY;    /* better than VERR_GENERAL_FAILURE */
            else
            {
                printf("ControlService failed with dwErr=%u. status=%d (%#x)\n",
                       dwErr, Status.dwCurrentState, Status.dwCurrentState);
                rc = -1;
            }
        }

        /*
         * Delete the service.
         */
        if (RT_SUCCESS(rc))
        {
            if (DeleteService(hService))
                rc = 0;
            else
            {
                printf("DeleteService failed lasterr=%d\n", GetLastError());
                rc = -1;
            }
        }
        CloseServiceHandle(hService);
    }
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
        rc = 0;
    else
        printf("OpenService failed lasterr=%d\n", GetLastError());
    CloseServiceHandle(hSMgr);
    return rc;
}

#ifdef TESTMODE

static HANDLE openDriver(void)
{
    HANDLE hDevice;

    hDevice = CreateFile(VBOXGUEST_DEVICE_NAME, // Win2k+: VBOXGUEST_DEVICE_NAME_GLOBAL
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile did not work. GetLastError() 0x%x\n", GetLastError());
    }
    return hDevice;
}

static int closeDriver(HANDLE hDevice)
{
    CloseHandle(hDevice);
    return 0;
}

static int performTest(void)
{
    int rc = 0;

    HANDLE hDevice = openDriver();

    if (hDevice != INVALID_HANDLE_VALUE)
        closeDriver(hDevice);
    else
        printf("openDriver failed!\n");

    return rc;
}

#endif /* TESTMODE */

static int usage(char *programName)
{
    printf("error, syntax: %s [install|uninstall]\n", programName);
    return 1;
}

int main(int argc, char **argv)
{
    bool installMode;
#ifdef TESTMODE
    bool testMode = false;
#endif

    if (argc != 2)
        return usage(argv[0]);

    if (strcmp(argv[1], "install") == 0)
        installMode = true;
    else if (strcmp(argv[1], "uninstall") == 0)
        installMode = false;
#ifdef TESTMODE
    else if (strcmp(argv[1], "test") == 0)
        testMode = true;
#endif
    else
        return usage(argv[0]);


    int rc;
#ifdef TESTMODE
    if (testMode)
        rc = performTest();
    else
#endif
    if (installMode)
        rc = installDriver(true);
    else
        rc = uninstallDriver();

    if (rc == 0)
        printf("operation completed successfully!\n");
    else
        printf("error: operation failed with status code %d\n", rc);

    return rc;
}

