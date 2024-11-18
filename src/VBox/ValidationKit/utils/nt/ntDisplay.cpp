/* $Id$ */
/** @file
 * Test cases for Display device and DirectX 3D rendering - NT.
 */

/*
 * Copyright (C) 2007-2024 Oracle and/or its affiliates.
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

#include <d3d11.h>

#include <iprt/win/setupapi.h>
#include <devguid.h>

#include <iprt/initterm.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/errcore.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** How chatty we should be. */
static uint32_t  g_cVerbosity = 0;
static HINSTANCE g_hInstance;

NTSTATUS SetDisplayDeviceState(bool bEnable)
{
    HDEVINFO hDevs = NULL;
    SP_DEVINFO_DATA DevInfo;
    NTSTATUS rcNt = (NTSTATUS)0;

    hDevs = SetupDiGetClassDevs(&GUID_DEVCLASS_DISPLAY, NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE);

    if (hDevs == INVALID_HANDLE_VALUE)
    {
        rcNt = GetLastError();
        RTMsgError("SetupDiGetClassDevs failed: %#x\n", rcNt);
        return rcNt;
    }

    RT_ZERO(DevInfo);
    DevInfo.cbSize = sizeof(SP_DEVINFO_DATA);

    if (SetupDiEnumDeviceInfo(hDevs, 0, &DevInfo))
    {
        SP_PROPCHANGE_PARAMS PropChangeParams;

        RT_ZERO(PropChangeParams);
        PropChangeParams.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
        PropChangeParams.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        PropChangeParams.StateChange = bEnable ? DICS_ENABLE : DICS_DISABLE;
        PropChangeParams.Scope = DICS_FLAG_CONFIGSPECIFIC;
        PropChangeParams.HwProfile = 0;

        if (SetupDiSetClassInstallParams(hDevs, &DevInfo, &PropChangeParams.ClassInstallHeader, sizeof(PropChangeParams)))
        {
            if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, hDevs, &DevInfo))
            {
                if (g_cVerbosity >= 1)
                    RTMsgInfo("debug: device %s\n", bEnable ? "enabled" : "disabled");
            }
            else
            {
                rcNt = GetLastError();
                RTMsgError("SetupDiCallClassInstaller failed: %#x\n", rcNt);
            }
        }
        else
        {
            rcNt = GetLastError();
            RTMsgError("SetupDiSetClassInstallParams failed: %#x\n", rcNt);
        }
    }

    SetupDiDestroyDeviceInfoList(hDevs);

    return rcNt;
}

bool CheckDXFeatureLevel()
{
    IDXGIAdapter *pAdapter = NULL; /* Default adapter. */
    static const D3D_FEATURE_LEVEL aFeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };
    UINT Flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL FeatureLevel = D3D_FEATURE_LEVEL_9_1;
    ID3D11Device *pDevice = NULL;
    ID3D11DeviceContext *pImmediateContext = NULL;
    bool fResult = false;
    HRESULT hr = S_OK;

    hr = D3D11CreateDevice(pAdapter,
                           D3D_DRIVER_TYPE_HARDWARE,
                           NULL,
                           Flags,
                           aFeatureLevels,
                           RT_ELEMENTS(aFeatureLevels),
                           D3D11_SDK_VERSION,
                           &pDevice,
                           &FeatureLevel,
                           &pImmediateContext);

    if (FAILED(hr))
    {
        RTMsgError("D3D11CreateDevice failed with 0x%X\n", hr);
        return false;
    }

    if (FeatureLevel == D3D_FEATURE_LEVEL_11_1)
    {
        RTMsgInfo("D3D_FEATURE_LEVEL_11_1 is supported\n");
        fResult = true;
    }
    else
    {
        RTMsgError("D3D_FEATURE_LEVEL_11_1 is not supported, only 0x%X\n", FeatureLevel);
    }

    pDevice->Release();
    pImmediateContext->Release();

    return fResult;
}

#define WM_PRIV_RECREATE_ALL_WINDOWS WM_USER

#define DISPLAYS_NUM_MAX 64
static int    g_cHWnd = 0;
static HWND   g_aHWnd[DISPLAYS_NUM_MAX];
static HWND   g_hWndPrimary = NULL;
static HBRUSH g_ahBrush[3];
static WNDCLASSEX g_WindowClass;

LRESULT CALLBACK WindowProcGDIFullScreen(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
    case WM_DISPLAYCHANGE:
        RTPrintf("WM_DISPLAYCHANGE: bpp %d, w %d, h %d\n", wParam, LOWORD(lParam), HIWORD(lParam));
        if (g_hWndPrimary == hWnd)
            PostMessage(g_hWndPrimary, WM_PRIV_RECREATE_ALL_WINDOWS, 0, 0);
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc;

        hdc = BeginPaint(hWnd, &ps);

        if (RT_LIKELY(hdc))
        {
            RECT r;

            SelectObject(hdc, g_ahBrush[0]);
            r = ps.rcPaint;
            PatBlt(hdc, r.left, r.top, r.right - r.left, r.bottom - r.top, PATCOPY);

            EndPaint(hWnd, &ps);
        }
    }
    break;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            PostQuitMessage(0);
    break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}


void CreateWindowForEachDisplay()
{
    DISPLAY_DEVICE dev;
    HWND hWndPrimaryNew = NULL;
    int k;

    RT_ZERO(g_aHWnd);
    g_cHWnd = 0;

    dev.cb = sizeof(dev);
    for(k = 0; k < DISPLAYS_NUM_MAX && EnumDisplayDevices(NULL, k, &dev, 0); k++, g_cHWnd++)
    {
        RTPrintf("%d: %s 0x%x %s%s\n", k + 1, dev.DeviceName, dev.StateFlags,
            (dev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) ? "PRIMARY" : "",
            (dev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) ? " ATTACHED" : "");

        DEVMODE mode = {};
        if (EnumDisplaySettings(dev.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
        {
            int x, y, w, h;
            HWND hWnd;

            x = mode.dmPosition.x;
            y = mode.dmPosition.y;
            w = mode.dmPelsWidth;
            h = mode.dmPelsHeight;
            RTPrintf("%s: (%d,%d)-(%dx%d)\n", mode.dmDeviceName, x, y, w, h);

            hWnd = CreateWindowEx(WS_EX_TOPMOST,
                TEXT("WindowClassGDIFullScreen"),
                TEXT("Fullscreen GDI test"),
                WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                x, y, w, h,
                NULL,
                NULL,
                g_hInstance,
                NULL);

            g_aHWnd[k] = hWnd;

            RTPrintf("DISPLAY%d window 0x%x\n", k + 1, hWnd);

            if (dev.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                hWndPrimaryNew = hWnd;
            }
        }
        else
        {
            g_aHWnd[k] = NULL;
            RTPrintf("DISPLAY%d is disabled, skipped\n", k + 1);
        }
    }

    g_hWndPrimary = hWndPrimaryNew;

    for (k = 0; k < g_cHWnd; k++)
    {
        if (g_aHWnd[k])
        {
            RTPrintf("DISPLAY%d Show & Update for Window 0x%x\n", k, g_aHWnd[k]);
            ShowWindow(g_aHWnd[k], TRUE);
            UpdateWindow(g_aHWnd[k]);
        }
    }
}

bool ShowFullScreenWindows()
{
    g_WindowClass.cbSize = sizeof(WNDCLASSEX);
    g_WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    g_WindowClass.lpfnWndProc = WindowProcGDIFullScreen;
    g_WindowClass.hInstance = g_hInstance;
    g_WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    g_WindowClass.hbrBackground = NULL;
    g_WindowClass.lpszClassName = TEXT("WindowClassGDIFullScreen");

    RegisterClassEx(&g_WindowClass);

    g_ahBrush[0] = CreateSolidBrush(RGB(0, 0, 255));
    g_ahBrush[1] = CreateSolidBrush(RGB(0, 255, 0));
    g_ahBrush[2] = CreateSolidBrush(RGB(255, 0, 0));

    PostMessage(NULL, WM_PRIV_RECREATE_ALL_WINDOWS, 0, 0);

    while(TRUE)
    {
        MSG msg;

        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_PRIV_RECREATE_ALL_WINDOWS)
            {
                g_hWndPrimary = NULL;

                for(int k = 0; k < g_cHWnd; k++)
                {
                    if (RT_LIKELY(g_aHWnd[k]))
                    {
                        DestroyWindow(g_aHWnd[k]);
                        g_aHWnd[k] = NULL;
                    }
                }

                CreateWindowForEachDisplay();
                continue;
            }

            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
//      RenderFrame();
    }

    return true;
}

int main(int argc, char **argv)
{
    /*
     * Init IPRT.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    g_hInstance = GetModuleHandle(NULL);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--enable",       'e',  RTGETOPT_REQ_UINT32  },
        { "--quiet",        'q',  RTGETOPT_REQ_NOTHING },
        { "--verbose",      'v',  RTGETOPT_REQ_NOTHING },
    };

    RTGETOPTSTATE State;
    RTGetOptInit(&State, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    RTGETOPTUNION ValueUnion;
    int chOpt;
    while ((chOpt = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (chOpt)
        {
            case 'e': SetDisplayDeviceState(RT_BOOL(ValueUnion.u32)); break;
            case 'q': g_cVerbosity = 0; break;
            case 'v': g_cVerbosity += 1; break;
            case 'h':
                RTPrintf("usage: ntDisplay.exe [-e|--enable <0 or 1>]\n");
                return 0;

            default:
                return RTGetOptPrintError(chOpt, &ValueUnion);
        }
    }

    ShowFullScreenWindows();

    return !CheckDXFeatureLevel();
}
