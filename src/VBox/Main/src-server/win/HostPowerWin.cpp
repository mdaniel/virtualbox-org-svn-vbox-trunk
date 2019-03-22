/* $Id$ */
/** @file
 *
 * VirtualBox interface to host's power notification service
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#include <iprt/win/windows.h>
/* Some SDK versions lack the extern "C" and thus cause linking failures.
 * This workaround isn't pretty, but there are not many options. */
extern "C" {
#include <PowrProf.h>
}

#include <VBox/com/ptr.h>
#include <iprt/errcore.h>
#include "HostPower.h"
#include "Logging.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static WCHAR gachWindowClassName[] = L"VBoxPowerNotifyClass";


HostPowerServiceWin::HostPowerServiceWin(VirtualBox *aVirtualBox) : HostPowerService(aVirtualBox), mThread(NIL_RTTHREAD)
{
    mHwnd = 0;

    int rc = RTThreadCreate(&mThread, HostPowerServiceWin::NotificationThread, this, 65536,
                            RTTHREADTYPE_GUI, RTTHREADFLAGS_WAITABLE, "MainPower");

    if (RT_FAILURE(rc))
    {
        Log(("HostPowerServiceWin::HostPowerServiceWin: RTThreadCreate failed with %Rrc\n", rc));
        return;
    }
}

HostPowerServiceWin::~HostPowerServiceWin()
{
    if (mHwnd)
    {
        Log(("HostPowerServiceWin::!HostPowerServiceWin: destroy window %x\n", mHwnd));

        /* Poke the thread out of the event loop and wait for it to clean up. */
        PostMessage(mHwnd, WM_CLOSE, 0, 0);
        RTThreadWait(mThread, 5000, NULL);
        mThread = NIL_RTTHREAD;
    }
}



DECLCALLBACK(int) HostPowerServiceWin::NotificationThread(RTTHREAD hThreadSelf, void *pInstance)
{
    RT_NOREF(hThreadSelf);
    HostPowerServiceWin *pPowerObj = (HostPowerServiceWin *)pInstance;
    HWND                 hwnd = 0;

    /* Create a window and make it a power event notification handler. */
    int rc = VINF_SUCCESS;

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);

    /* Register the Window Class. */
    WNDCLASS wc;

    wc.style         = CS_NOCLOSE;
    wc.lpfnWndProc   = HostPowerServiceWin::WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(void *);
    wc.hInstance     = hInstance;
    wc.hIcon         = NULL;
    wc.hCursor       = NULL;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = gachWindowClassName;

    ATOM atomWindowClass = RegisterClass(&wc);

    if (atomWindowClass == 0)
    {
        rc = VERR_NOT_SUPPORTED;
        Log(("HostPowerServiceWin::NotificationThread: RegisterClassA failed with %x\n", GetLastError()));
    }
    else
    {
        /* Create the window. */
        hwnd = pPowerObj->mHwnd = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                                 gachWindowClassName, gachWindowClassName,
                                                 WS_POPUPWINDOW,
                                                -200, -200, 100, 100, NULL, NULL, hInstance, NULL);

        if (hwnd == NULL)
        {
            Log(("HostPowerServiceWin::NotificationThread: CreateWindowExA failed with %x\n", GetLastError()));
            rc = VERR_NOT_SUPPORTED;
        }
        else
        {
            SetWindowLongPtr(hwnd, 0, (LONG_PTR)pPowerObj);
            SetWindowPos(hwnd, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);

            MSG msg;
            BOOL fRet;
            while ((fRet = GetMessage(&msg, NULL, 0, 0)) > 0)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            /*
            * Window procedure can return error,
            * but this is exceptional situation
            * that should be identified in testing
            */
            Assert(fRet >= 0);
        }
    }

    Log(("HostPowerServiceWin::NotificationThread: exit thread\n"));

    if (atomWindowClass != 0)
    {
        UnregisterClass(gachWindowClassName, hInstance);
        atomWindowClass = 0;
    }

    return 0;
}

LRESULT CALLBACK HostPowerServiceWin::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_POWERBROADCAST:
        {
            HostPowerServiceWin *pPowerObj;

            pPowerObj = (HostPowerServiceWin *)GetWindowLongPtr(hwnd, 0);
            if (pPowerObj)
            {
                switch(wParam)
                {
                case PBT_APMSUSPEND:
                    pPowerObj->notify(Reason_HostSuspend);
                    break;

                case PBT_APMRESUMEAUTOMATIC:
                    pPowerObj->notify(Reason_HostResume);
                    break;

                case PBT_APMPOWERSTATUSCHANGE:
                {
                    SYSTEM_POWER_STATUS SystemPowerStatus;

                    Log(("PBT_APMPOWERSTATUSCHANGE\n"));
                    if (GetSystemPowerStatus(&SystemPowerStatus) == TRUE)
                    {
                        Log(("PBT_APMPOWERSTATUSCHANGE ACLineStatus=%d BatteryFlag=%d\n", SystemPowerStatus.ACLineStatus,
                             SystemPowerStatus.BatteryFlag));

                        if (SystemPowerStatus.ACLineStatus == 0)      /* offline */
                        {
                            if (SystemPowerStatus.BatteryFlag == 2 /* low > 33% */)
                            {
                                LONG rc;
                                SYSTEM_BATTERY_STATE BatteryState;

                                rc = CallNtPowerInformation(SystemBatteryState, NULL, 0, (PVOID)&BatteryState,
                                                            sizeof(BatteryState));
#ifdef LOG_ENABLED
                                if (rc == 0 /* STATUS_SUCCESS */)
                                    Log(("CallNtPowerInformation claims %d seconds of power left\n",
                                         BatteryState.EstimatedTime));
#endif
                                if (    rc == 0 /* STATUS_SUCCESS */
                                    &&  BatteryState.EstimatedTime < 60*5)
                                {
                                    pPowerObj->notify(Reason_HostBatteryLow);
                                }
                            }
                            /* If the machine has less than 5% battery left (and is not connected
                             * to the AC), then we should save the state. */
                            else if (SystemPowerStatus.BatteryFlag == 4      /* critical battery status; less than 5% */)
                            {
                                pPowerObj->notify(Reason_HostBatteryLow);
                            }
                        }
                    }
                    break;
                }
                default:
                    return DefWindowProc(hwnd, msg, wParam, lParam);
                }
            }
            return TRUE;
        }

        case WM_DESTROY:
        {
            /* moved here. it can't work across theads */
            SetWindowLongPtr(hwnd, 0, 0);
            PostQuitMessage(0);
            return 0;
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
