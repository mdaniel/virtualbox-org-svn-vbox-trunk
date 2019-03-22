/* $Id$ */
/** @file
 * Windows Driver Manipulation API.
 */

/*
 * Copyright (C) 2011-2017 Oracle Corporation
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

#ifndef ___VBox_VBoxDrvCfg_win_h
#define ___VBox_VBoxDrvCfg_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/win/windows.h>
#include <VBox/cdefs.h>

RT_C_DECLS_BEGIN

#if 0
/* enable this in case we include this in a dll*/
# ifdef IN_VBOXDRVCFG
#  define VBOXDRVCFG_DECL(a_Type) DECLEXPORT(a_Type)
# else
#  define VBOXDRVCFG_DECL(a_Type) DECLIMPORT(a_Type)
# endif
#else
/*enable this in case we include this in a static lib*/
# define VBOXDRVCFG_DECL(a_Type) a_Type VBOXCALL
#endif

typedef enum
{
    VBOXDRVCFG_LOG_SEVERITY_FLOW = 1,
    VBOXDRVCFG_LOG_SEVERITY_REGULAR,
    VBOXDRVCFG_LOG_SEVERITY_REL
} VBOXDRVCFG_LOG_SEVERITY;

typedef DECLCALLBACK(void) FNVBOXDRVCFG_LOG(VBOXDRVCFG_LOG_SEVERITY enmSeverity, char *pszMsg, void *pvContext);
typedef FNVBOXDRVCFG_LOG *PFNVBOXDRVCFG_LOG;

VBOXDRVCFG_DECL(void) VBoxDrvCfgLoggerSet(PFNVBOXDRVCFG_LOG pfnLog, void *pvLog);

typedef DECLCALLBACK(void) FNVBOXDRVCFG_PANIC(void * pvPanic);
typedef FNVBOXDRVCFG_PANIC *PFNVBOXDRVCFG_PANIC;
VBOXDRVCFG_DECL(void) VBoxDrvCfgPanicSet(PFNVBOXDRVCFG_PANIC pfnPanic, void *pvPanic);

/* Driver package API*/
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfInstall(IN LPCWSTR lpszInfPath);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstall(IN LPCWSTR lpszInfPath, IN DWORD fFlags);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstallAllSetupDi(IN const GUID * pGuidClass, IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD fFlags);
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgInfUninstallAllF(IN LPCWSTR lpszClassName, IN LPCWSTR lpszPnPId, IN DWORD fFlags);

/* Service API */
VBOXDRVCFG_DECL(HRESULT) VBoxDrvCfgSvcStart(LPCWSTR lpszSvcName);

HRESULT VBoxDrvCfgDrvUpdate(LPCWSTR pcszwHwId, LPCWSTR pcsxwInf, BOOL *pbRebootRequired);

RT_C_DECLS_END

#endif

