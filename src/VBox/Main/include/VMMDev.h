/* $Id$ */
/** @file
 * VirtualBox Driver interface to VMM device
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

#ifndef ____H_VMMDEV
#define ____H_VMMDEV
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "VirtualBoxBase.h"
#include <VBox/vmm/pdmdrv.h>
#include <VBox/hgcmsvc.h>
#include <iprt/asm.h>

class Console;

class VMMDevMouseInterface
{
public:
    virtual PPDMIVMMDEVPORT getVMMDevPort() = 0;
};

class VMMDev : public VMMDevMouseInterface
{
public:
    VMMDev(Console *console);
    virtual ~VMMDev();
    static const PDMDRVREG  DrvReg;
    /** Pointer to the associated VMMDev driver. */
    struct DRVMAINVMMDEV *mpDrv;

    bool fSharedFolderActive;
    bool isShFlActive()
    {
        return fSharedFolderActive;
    }

    Console *getParent()
    {
        return mParent;
    }

    int WaitCredentialsJudgement (uint32_t u32Timeout, uint32_t *pu32GuestFlags);
    int SetCredentialsJudgementResult (uint32_t u32Flags);

    PPDMIVMMDEVPORT getVMMDevPort();

#ifdef VBOX_WITH_HGCM
    int hgcmLoadService (const char *pszServiceLibrary, const char *pszServiceName);
    int hgcmHostCall (const char *pszServiceName, uint32_t u32Function, uint32_t cParms, PVBOXHGCMSVCPARM paParms);
# ifdef VBOX_WITH_CRHGSMI
    int hgcmHostSvcHandleCreate (const char *pszServiceName, HGCMCVSHANDLE * phSvc);
    int hgcmHostSvcHandleDestroy (HGCMCVSHANDLE hSvc);
    int hgcmHostFastCallAsync (HGCMCVSHANDLE hSvc, uint32_t function, PVBOXHGCMSVCPARM pParm, PHGCMHOSTFASTCALLCB pfnCompletion, void *pvCompletion);
# endif
    void hgcmShutdown(bool fUvmIsInvalid = false);

    bool hgcmIsActive (void) { return ASMAtomicReadBool(&m_fHGCMActive); }
#endif /* VBOX_WITH_HGCM */

private:
#ifdef VBOX_WITH_HGCM
# ifdef VBOX_WITH_GUEST_PROPS
    void i_guestPropSetMultiple(void *names, void *values, void *timestamps, void *flags);
    void i_guestPropSet(const char *pszName, const char *pszValue, const char *pszFlags);
    int  i_guestPropSetGlobalPropertyFlags(uint32_t fFlags);
    int  i_guestPropLoadAndConfigure();
# endif
#endif
    static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
    static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
    static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(void)   drvReset(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(void)   drvPowerOn(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(void)   drvPowerOff(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(void)   drvSuspend(PPDMDRVINS pDrvIns);
    static DECLCALLBACK(void)   drvResume(PPDMDRVINS pDrvIns);

    Console * const         mParent;

    RTSEMEVENT mCredentialsEvent;
    uint32_t mu32CredentialsFlags;

#ifdef VBOX_WITH_HGCM
    bool volatile m_fHGCMActive;
#endif /* VBOX_WITH_HGCM */
};

#endif // !____H_VMMDEV
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
