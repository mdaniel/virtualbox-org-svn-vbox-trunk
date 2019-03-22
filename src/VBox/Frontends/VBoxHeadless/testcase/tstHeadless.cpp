/** @file
 *
 * VBox frontends: VBoxHeadless frontend:
 * Testcases
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>

using namespace com;

#include <VBox/log.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>


////////////////////////////////////////////////////////////////////////////////

/**
 *  Entry point.
 */
int main(int argc, char **argv)
{
    // initialize VBox Runtime
    RTR3InitExe(argc, &argv, 0);

    // the below cannot be Bstr because on Linux Bstr doesn't work
    // until XPCOM (nsMemory) is initialized
    const char *name = NULL;
    const char *operation = NULL;

    // parse the command line
    if (argc > 1)
        name = argv [1];
    if (argc > 2)
        operation = argv [2];

    if (!name || !operation)
    {
        RTPrintf("\nUsage:\n\n"
                 "%s <machine_name> [on|off|pause|resume]\n\n",
                 argv [0]);
        return 0;
    }

    RTPrintf("\n");
    RTPrintf("tstHeadless STARTED.\n");

    RTPrintf("VM name   : {%s}\n"
             "Operation : %s\n\n",
             name, operation);

    HRESULT rc;

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
        return rc;
    }

    do
    {
        ComPtr <IVirtualBoxClient> virtualBoxClient;
        ComPtr <IVirtualBox> virtualBox;
        ComPtr <ISession> session;

        RTPrintf("Creating VirtualBox object...\n");
        rc = virtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
        if (SUCCEEDED(rc))
            rc = virtualBoxClient->COMGETTER(VirtualBox)(virtualBox.asOutParam());
        if (FAILED(rc))
            RTPrintf("ERROR: failed to create the VirtualBox object!\n");
        else
        {
            rc = session.createInprocObject(CLSID_Session);
            if (FAILED(rc))
                RTPrintf("ERROR: failed to create a session object!\n");
        }

        if (FAILED(rc))
        {
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
            {
                com::GluePrintRCMessage(rc);
                RTPrintf("Most likely, the VirtualBox COM server is not running or failed to start.\n");
            }
            else
                com::GluePrintErrorInfo(info);
            break;
        }

        ComPtr <IMachine> m;

        // find ID by name
        CHECK_ERROR_BREAK(virtualBox, FindMachine(Bstr(name).raw(),
                                                  m.asOutParam()));

        if (!strcmp(operation, "on"))
        {
            ComPtr <IProgress> progress;
            RTPrintf("Opening a new (remote) session...\n");
            CHECK_ERROR_BREAK(m,
                              LaunchVMProcess(session, Bstr("vrdp").raw(),
                                              NULL, progress.asOutParam()));

            RTPrintf("Waiting for the remote session to open...\n");
            CHECK_ERROR_BREAK(progress, WaitForCompletion(-1));

            BOOL completed;
            CHECK_ERROR_BREAK(progress, COMGETTER(Completed)(&completed));
            ASSERT(completed);

            LONG resultCode;
            CHECK_ERROR_BREAK(progress, COMGETTER(ResultCode)(&resultCode));
            if (FAILED(resultCode))
            {
                ProgressErrorInfo info(progress);
                com::GluePrintErrorInfo(info);
            }
            else
            {
                RTPrintf("Remote session has been successfully opened.\n");
            }
        }
        else
        {
            RTPrintf("Opening an existing session...\n");
            CHECK_ERROR_BREAK(m, LockMachine(session, LockType_Shared));

            ComPtr <IConsole> console;
            CHECK_ERROR_BREAK(session, COMGETTER(Console)(console.asOutParam()));

            if (!strcmp(operation, "off"))
            {
                ComPtr <IProgress> progress;
                RTPrintf("Powering the VM off...\n");
                CHECK_ERROR_BREAK(console, PowerDown(progress.asOutParam()));

                RTPrintf("Waiting for the VM to power down...\n");
                CHECK_ERROR_BREAK(progress, WaitForCompletion(-1));

                BOOL completed;
                CHECK_ERROR_BREAK(progress, COMGETTER(Completed)(&completed));
                ASSERT(completed);

                LONG resultCode;
                CHECK_ERROR_BREAK(progress, COMGETTER(ResultCode)(&resultCode));
                if (FAILED(resultCode))
                {
                    ProgressErrorInfo info(progress);
                    com::GluePrintErrorInfo(info);
                }
                else
                {
                    RTPrintf("VM is powered down.\n");
                }
            }
            else
            if (!strcmp(operation, "pause"))
            {
                RTPrintf("Pausing the VM...\n");
                CHECK_ERROR_BREAK(console, Pause());
            }
            else
            if (!strcmp(operation, "resume"))
            {
                RTPrintf("Resuming the VM...\n");
                CHECK_ERROR_BREAK(console, Resume());
            }
            else
            {
                RTPrintf("Invalid operation!\n");
            }
        }

        RTPrintf("Closing the session (may fail after power off)...\n");
        CHECK_ERROR(session, UnlockMachine());
    }
    while (0);
    RTPrintf("\n");

    com::Shutdown();

    RTPrintf("tstHeadless FINISHED.\n");

    return rc;
}

