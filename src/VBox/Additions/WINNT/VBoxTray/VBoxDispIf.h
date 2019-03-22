/* $Id$ */
/** @file
 * VBoxTray - Display Settings Interface abstraction for XPDM & WDDM
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

#include <iprt/cdefs.h>

#ifdef VBOX_WITH_WDDM
# define D3DKMDT_SPECIAL_MULTIPLATFORM_TOOL
# include <d3dkmthk.h>
# include <VBoxDispKmt.h>
#endif

#include <VBoxDisplay.h>

typedef enum
{
    VBOXDISPIF_MODE_UNKNOWN  = 0,
    VBOXDISPIF_MODE_XPDM_NT4 = 1,
    VBOXDISPIF_MODE_XPDM
#ifdef VBOX_WITH_WDDM
    , VBOXDISPIF_MODE_WDDM
    , VBOXDISPIF_MODE_WDDM_W7
#endif
} VBOXDISPIF_MODE;
/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us
 * NOTE: DispIf makes no checks whether the display driver is actually a VBox driver,
 * it just switches between using different backend OS API based on the VBoxDispIfSwitchMode call
 * It's caller's responsibility to initiate it to work in the correct mode */
typedef struct VBOXDISPIF
{
    VBOXDISPIF_MODE enmMode;
    /* with WDDM the approach is to call into WDDM miniport driver via PFND3DKMT API provided by the GDI,
     * The PFND3DKMT is supposed to be used by the OpenGL ICD according to MSDN, so this approach is a bit hacky */
    union
    {
        struct
        {
            LONG (WINAPI *pfnChangeDisplaySettingsEx)(LPCSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
        } xpdm;
#ifdef VBOX_WITH_WDDM
        struct
        {
            /* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
            LONG (WINAPI *pfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
            /* EnumDisplayDevices does not exist in NT. isVBoxDisplayDriverActive et al. are using these functions. */
            BOOL (WINAPI *pfnEnumDisplayDevices)(IN LPCSTR lpDevice, IN DWORD iDevNum, OUT PDISPLAY_DEVICEA lpDisplayDevice, IN DWORD dwFlags);

            VBOXDISPKMT_CALLBACKS KmtCallbacks;
        } wddm;
#endif
    } modeData;
} VBOXDISPIF, *PVBOXDISPIF;
typedef const struct VBOXDISPIF *PCVBOXDISPIF;

/* initializes the DispIf
 * Initially the DispIf is configured to work in XPDM mode
 * call VBoxDispIfSwitchMode to switch the mode to WDDM */
DWORD VBoxDispIfInit(PVBOXDISPIF pIf);
DWORD VBoxDispIfSwitchMode(PVBOXDISPIF pIf, VBOXDISPIF_MODE enmMode, VBOXDISPIF_MODE *penmOldMode);
DECLINLINE(VBOXDISPIF_MODE) VBoxDispGetMode(PVBOXDISPIF pIf) { return pIf->enmMode; }
DWORD VBoxDispIfTerm(PVBOXDISPIF pIf);
DWORD VBoxDispIfEscape(PCVBOXDISPIF const pIf, PVBOXDISPIFESCAPE pEscape, int cbData);
DWORD VBoxDispIfEscapeInOut(PCVBOXDISPIF const pIf, PVBOXDISPIFESCAPE pEscape, int cbData);
DWORD VBoxDispIfResizeModes(PCVBOXDISPIF const pIf, UINT iChangedMode, BOOL fEnable, BOOL fExtDispSup, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes);
DWORD VBoxDispIfCancelPendingResize(PCVBOXDISPIF const pIf);
DWORD VBoxDispIfResizeStarted(PCVBOXDISPIF const pIf);

typedef struct VBOXDISPIF_SEAMLESS
{
    PCVBOXDISPIF pIf;

    union
    {
#ifdef VBOX_WITH_WDDM
        struct
        {
            VBOXDISPKMT_ADAPTER Adapter;
# ifdef VBOX_DISPIF_WITH_OPCONTEXT
            VBOXDISPKMT_DEVICE Device;
            VBOXDISPKMT_CONTEXT Context;
# endif
        } wddm;
#endif
    } modeData;
} VBOXDISPIF_SEAMLESS;

DECLINLINE(bool) VBoxDispIfSeamlesIsValid(VBOXDISPIF_SEAMLESS *pSeamless)
{
    return !!pSeamless->pIf;
}

DWORD VBoxDispIfSeamlessCreate(PCVBOXDISPIF const pIf, VBOXDISPIF_SEAMLESS *pSeamless, HANDLE hEvent);
DWORD VBoxDispIfSeamlessTerm(VBOXDISPIF_SEAMLESS *pSeamless);
DWORD VBoxDispIfSeamlessSubmit(VBOXDISPIF_SEAMLESS *pSeamless, VBOXDISPIFESCAPE *pData, int cbData);

