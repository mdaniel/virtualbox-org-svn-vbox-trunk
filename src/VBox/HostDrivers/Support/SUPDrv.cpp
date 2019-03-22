/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common code.
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
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"
#ifndef PAGE_SHIFT
# include <iprt/param.h>
#endif
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/asm-math.h>
#include <iprt/cpuset.h>
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
# include <iprt/dbg.h>
#endif
#include <iprt/handletable.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/net.h>
#include <iprt/crc.h>
#include <iprt/string.h>
#include <iprt/timer.h>
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
# include <iprt/rand.h>
# include <iprt/path.h>
#endif
#include <iprt/uint128.h>
#include <iprt/x86.h>

#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/vmm/hm_vmx.h>

#if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
# include "dtrace/SUPDrv.h"
#else
# define VBOXDRV_SESSION_CREATE(pvSession, fUser) do { } while (0)
# define VBOXDRV_SESSION_CLOSE(pvSession) do { } while (0)
# define VBOXDRV_IOCTL_ENTRY(pvSession, uIOCtl, pvReqHdr) do { } while (0)
# define VBOXDRV_IOCTL_RETURN(pvSession, uIOCtl, pvReqHdr, rcRet, rcReq) do { } while (0)
#endif

/*
 * Logging assignments:
 *      Log     - useful stuff, like failures.
 *      LogFlow - program flow, except the really noisy bits.
 *      Log2    - Cleanup.
 *      Log3    - Loader flow noise.
 *      Log4    - Call VMMR0 flow noise.
 *      Log5    - Native yet-to-be-defined noise.
 *      Log6    - Native ioctl flow noise.
 *
 * Logging requires BUILD_TYPE=debug and possibly changes to the logger
 * instantiation in log-vbox.c(pp).
 */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def VBOX_SVN_REV
 * The makefile should define this if it can. */
#ifndef VBOX_SVN_REV
# define VBOX_SVN_REV 0
#endif

/** @ SUPDRV_CHECK_SMAP_SETUP
 * SMAP check setup. */
/** @def SUPDRV_CHECK_SMAP_CHECK
 * Checks that the AC flag is set if SMAP is enabled.  If AC is not set, it
 * will be logged and @a a_BadExpr is executed. */
#if defined(RT_OS_DARWIN) || defined(RT_OS_LINUX)
# define SUPDRV_CHECK_SMAP_SETUP() uint32_t const fKernelFeatures = SUPR0GetKernelFeatures()
# define SUPDRV_CHECK_SMAP_CHECK(a_pDevExt, a_BadExpr) \
    do { \
        if (fKernelFeatures & SUPKERNELFEATURES_SMAP) \
        { \
            RTCCUINTREG fEfl = ASMGetFlags(); \
            if (RT_LIKELY(fEfl & X86_EFL_AC)) \
            { /* likely */ } \
            else \
            { \
                supdrvBadContext(a_pDevExt, "SUPDrv.cpp", __LINE__, "EFLAGS.AC is 0!"); \
                a_BadExpr; \
            } \
        } \
    } while (0)
#else
# define SUPDRV_CHECK_SMAP_SETUP()                      uint32_t const fKernelFeatures = 0
# define SUPDRV_CHECK_SMAP_CHECK(a_pDevExt, a_BadExpr)  NOREF(fKernelFeatures)
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)    supdrvSessionObjHandleRetain(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser);
static DECLCALLBACK(void)   supdrvSessionObjHandleDelete(RTHANDLETABLE hHandleTable, uint32_t h, void *pvObj, void *pvCtx, void *pvUser);
static int                  supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession);
static int                  supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType);
static int                  supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN pReq);
static int                  supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD pReq);
static int                  supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE pReq);
static int                  supdrvIOCtl_LdrLockDown(PSUPDRVDEVEXT pDevExt);
static int                  supdrvIOCtl_LdrQuerySymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL pReq);
static int                  supdrvIDC_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQGETSYM pReq);
static int                  supdrvLdrSetVMMR0EPs(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0EntryFast, void *pvVMMR0EntryEx);
static void                 supdrvLdrUnsetVMMR0EPs(PSUPDRVDEVEXT pDevExt);
static int                  supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage);
static void                 supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage);
DECLINLINE(int)             supdrvLdrLock(PSUPDRVDEVEXT pDevExt);
DECLINLINE(int)             supdrvLdrUnlock(PSUPDRVDEVEXT pDevExt);
static int                  supdrvIOCtl_CallServiceModule(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPCALLSERVICE pReq);
static int                  supdrvIOCtl_LoggerSettings(PSUPLOGGERSETTINGS pReq);
static int                  supdrvIOCtl_MsrProber(PSUPDRVDEVEXT pDevExt, PSUPMSRPROBER pReq);
static int                  supdrvIOCtl_ResumeSuspendedKbds(void);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Array of the R0 SUP API.
 *
 * While making changes to these exports, make sure to update the IOC
 * minor version (SUPDRV_IOC_VERSION).
 *
 * @remarks This array is processed by SUPR0-def-pe.sed and SUPR0-def-lx.sed to
 *          produce definition files from which import libraries are generated.
 *          Take care when commenting things and especially with \#ifdef'ing.
 */
static SUPFUNC g_aFunctions[] =
{
/* SED: START */
    /* name                                     function */
        /* Entries with absolute addresses determined at runtime, fixup
           code makes ugly ASSUMPTIONS about the order here: */
    { "SUPR0AbsIs64bit",                        (void *)0 },
    { "SUPR0Abs64bitKernelCS",                  (void *)0 },
    { "SUPR0Abs64bitKernelSS",                  (void *)0 },
    { "SUPR0Abs64bitKernelDS",                  (void *)0 },
    { "SUPR0AbsKernelCS",                       (void *)0 },
    { "SUPR0AbsKernelSS",                       (void *)0 },
    { "SUPR0AbsKernelDS",                       (void *)0 },
    { "SUPR0AbsKernelES",                       (void *)0 },
    { "SUPR0AbsKernelFS",                       (void *)0 },
    { "SUPR0AbsKernelGS",                       (void *)0 },
        /* Normal function pointers: */
    { "g_pSUPGlobalInfoPage",                   (void *)&g_pSUPGlobalInfoPage },            /* SED: DATA */
    { "SUPGetGIP",                              (void *)(uintptr_t)SUPGetGIP },
    { "SUPReadTscWithDelta",                    (void *)(uintptr_t)SUPReadTscWithDelta },
    { "SUPGetTscDeltaSlow",                     (void *)(uintptr_t)SUPGetTscDeltaSlow },
    { "SUPGetCpuHzFromGipForAsyncMode",         (void *)(uintptr_t)SUPGetCpuHzFromGipForAsyncMode },
    { "SUPIsTscFreqCompatible",                 (void *)(uintptr_t)SUPIsTscFreqCompatible },
    { "SUPIsTscFreqCompatibleEx",               (void *)(uintptr_t)SUPIsTscFreqCompatibleEx },
    { "SUPR0BadContext",                        (void *)(uintptr_t)SUPR0BadContext },
    { "SUPR0ComponentDeregisterFactory",        (void *)(uintptr_t)SUPR0ComponentDeregisterFactory },
    { "SUPR0ComponentQueryFactory",             (void *)(uintptr_t)SUPR0ComponentQueryFactory },
    { "SUPR0ComponentRegisterFactory",          (void *)(uintptr_t)SUPR0ComponentRegisterFactory },
    { "SUPR0ContAlloc",                         (void *)(uintptr_t)SUPR0ContAlloc },
    { "SUPR0ContFree",                          (void *)(uintptr_t)SUPR0ContFree },
    { "SUPR0ChangeCR4",                         (void *)(uintptr_t)SUPR0ChangeCR4 },
    { "SUPR0EnableVTx",                         (void *)(uintptr_t)SUPR0EnableVTx },
    { "SUPR0SuspendVTxOnCpu",                   (void *)(uintptr_t)SUPR0SuspendVTxOnCpu },
    { "SUPR0ResumeVTxOnCpu",                    (void *)(uintptr_t)SUPR0ResumeVTxOnCpu },
    { "SUPR0GetCurrentGdtRw",                   (void *)(uintptr_t)SUPR0GetCurrentGdtRw },
    { "SUPR0GetKernelFeatures",                 (void *)(uintptr_t)SUPR0GetKernelFeatures },
    { "SUPR0GetPagingMode",                     (void *)(uintptr_t)SUPR0GetPagingMode },
    { "SUPR0GetSvmUsability",                   (void *)(uintptr_t)SUPR0GetSvmUsability },
    { "SUPR0GetVTSupport",                      (void *)(uintptr_t)SUPR0GetVTSupport },
    { "SUPR0GetVmxUsability",                   (void *)(uintptr_t)SUPR0GetVmxUsability },
    { "SUPR0GetRawModeUsability",               (void *)(uintptr_t)SUPR0GetRawModeUsability },
    { "SUPR0LockMem",                           (void *)(uintptr_t)SUPR0LockMem },
    { "SUPR0LowAlloc",                          (void *)(uintptr_t)SUPR0LowAlloc },
    { "SUPR0LowFree",                           (void *)(uintptr_t)SUPR0LowFree },
    { "SUPR0MemAlloc",                          (void *)(uintptr_t)SUPR0MemAlloc },
    { "SUPR0MemFree",                           (void *)(uintptr_t)SUPR0MemFree },
    { "SUPR0MemGetPhys",                        (void *)(uintptr_t)SUPR0MemGetPhys },
    { "SUPR0ObjAddRef",                         (void *)(uintptr_t)SUPR0ObjAddRef },
    { "SUPR0ObjAddRefEx",                       (void *)(uintptr_t)SUPR0ObjAddRefEx },
    { "SUPR0ObjRegister",                       (void *)(uintptr_t)SUPR0ObjRegister },
    { "SUPR0ObjRelease",                        (void *)(uintptr_t)SUPR0ObjRelease },
    { "SUPR0ObjVerifyAccess",                   (void *)(uintptr_t)SUPR0ObjVerifyAccess },
    { "SUPR0PageAllocEx",                       (void *)(uintptr_t)SUPR0PageAllocEx },
    { "SUPR0PageFree",                          (void *)(uintptr_t)SUPR0PageFree },
    { "SUPR0Printf",                            (void *)(uintptr_t)SUPR0Printf },
    { "SUPR0GetSessionGVM",                     (void *)(uintptr_t)SUPR0GetSessionGVM },
    { "SUPR0GetSessionVM",                      (void *)(uintptr_t)SUPR0GetSessionVM },
    { "SUPR0SetSessionVM",                      (void *)(uintptr_t)SUPR0SetSessionVM },
    { "SUPR0TscDeltaMeasureBySetIndex",         (void *)(uintptr_t)SUPR0TscDeltaMeasureBySetIndex },
    { "SUPR0TracerDeregisterDrv",               (void *)(uintptr_t)SUPR0TracerDeregisterDrv },
    { "SUPR0TracerDeregisterImpl",              (void *)(uintptr_t)SUPR0TracerDeregisterImpl },
    { "SUPR0TracerFireProbe",                   (void *)(uintptr_t)SUPR0TracerFireProbe },
    { "SUPR0TracerRegisterDrv",                 (void *)(uintptr_t)SUPR0TracerRegisterDrv },
    { "SUPR0TracerRegisterImpl",                (void *)(uintptr_t)SUPR0TracerRegisterImpl },
    { "SUPR0TracerRegisterModule",              (void *)(uintptr_t)SUPR0TracerRegisterModule },
    { "SUPR0TracerUmodProbeFire",               (void *)(uintptr_t)SUPR0TracerUmodProbeFire },
    { "SUPR0UnlockMem",                         (void *)(uintptr_t)SUPR0UnlockMem },
#ifdef RT_OS_WINDOWS
    { "SUPR0IoCtlSetupForHandle",               (void *)(uintptr_t)SUPR0IoCtlSetupForHandle },  /* only-windows */
    { "SUPR0IoCtlPerform",                      (void *)(uintptr_t)SUPR0IoCtlPerform },         /* only-windows */
    { "SUPR0IoCtlCleanup",                      (void *)(uintptr_t)SUPR0IoCtlCleanup },         /* only-windows */
#endif
    { "SUPSemEventClose",                       (void *)(uintptr_t)SUPSemEventClose },
    { "SUPSemEventCreate",                      (void *)(uintptr_t)SUPSemEventCreate },
    { "SUPSemEventGetResolution",               (void *)(uintptr_t)SUPSemEventGetResolution },
    { "SUPSemEventMultiClose",                  (void *)(uintptr_t)SUPSemEventMultiClose },
    { "SUPSemEventMultiCreate",                 (void *)(uintptr_t)SUPSemEventMultiCreate },
    { "SUPSemEventMultiGetResolution",          (void *)(uintptr_t)SUPSemEventMultiGetResolution },
    { "SUPSemEventMultiReset",                  (void *)(uintptr_t)SUPSemEventMultiReset },
    { "SUPSemEventMultiSignal",                 (void *)(uintptr_t)SUPSemEventMultiSignal },
    { "SUPSemEventMultiWait",                   (void *)(uintptr_t)SUPSemEventMultiWait },
    { "SUPSemEventMultiWaitNoResume",           (void *)(uintptr_t)SUPSemEventMultiWaitNoResume },
    { "SUPSemEventMultiWaitNsAbsIntr",          (void *)(uintptr_t)SUPSemEventMultiWaitNsAbsIntr },
    { "SUPSemEventMultiWaitNsRelIntr",          (void *)(uintptr_t)SUPSemEventMultiWaitNsRelIntr },
    { "SUPSemEventSignal",                      (void *)(uintptr_t)SUPSemEventSignal },
    { "SUPSemEventWait",                        (void *)(uintptr_t)SUPSemEventWait },
    { "SUPSemEventWaitNoResume",                (void *)(uintptr_t)SUPSemEventWaitNoResume },
    { "SUPSemEventWaitNsAbsIntr",               (void *)(uintptr_t)SUPSemEventWaitNsAbsIntr },
    { "SUPSemEventWaitNsRelIntr",               (void *)(uintptr_t)SUPSemEventWaitNsRelIntr },

    { "RTAssertAreQuiet",                       (void *)(uintptr_t)RTAssertAreQuiet },
    { "RTAssertMayPanic",                       (void *)(uintptr_t)RTAssertMayPanic },
    { "RTAssertMsg1",                           (void *)(uintptr_t)RTAssertMsg1 },
    { "RTAssertMsg2AddV",                       (void *)(uintptr_t)RTAssertMsg2AddV },
    { "RTAssertMsg2V",                          (void *)(uintptr_t)RTAssertMsg2V },
    { "RTAssertSetMayPanic",                    (void *)(uintptr_t)RTAssertSetMayPanic },
    { "RTAssertSetQuiet",                       (void *)(uintptr_t)RTAssertSetQuiet },
    { "RTCrc32",                                (void *)(uintptr_t)RTCrc32 },
    { "RTCrc32Finish",                          (void *)(uintptr_t)RTCrc32Finish },
    { "RTCrc32Process",                         (void *)(uintptr_t)RTCrc32Process },
    { "RTCrc32Start",                           (void *)(uintptr_t)RTCrc32Start },
    { "RTErrConvertFromErrno",                  (void *)(uintptr_t)RTErrConvertFromErrno },
    { "RTErrConvertToErrno",                    (void *)(uintptr_t)RTErrConvertToErrno },
    { "RTHandleTableAllocWithCtx",              (void *)(uintptr_t)RTHandleTableAllocWithCtx },
    { "RTHandleTableCreate",                    (void *)(uintptr_t)RTHandleTableCreate },
    { "RTHandleTableCreateEx",                  (void *)(uintptr_t)RTHandleTableCreateEx },
    { "RTHandleTableDestroy",                   (void *)(uintptr_t)RTHandleTableDestroy },
    { "RTHandleTableFreeWithCtx",               (void *)(uintptr_t)RTHandleTableFreeWithCtx },
    { "RTHandleTableLookupWithCtx",             (void *)(uintptr_t)RTHandleTableLookupWithCtx },
    { "RTLogDefaultInstance",                   (void *)(uintptr_t)RTLogDefaultInstance },
    { "RTLogDefaultInstanceEx",                 (void *)(uintptr_t)RTLogDefaultInstanceEx },
    { "RTLogGetDefaultInstance",                (void *)(uintptr_t)RTLogGetDefaultInstance },
    { "RTLogGetDefaultInstanceEx",              (void *)(uintptr_t)RTLogGetDefaultInstanceEx },
    { "SUPR0GetDefaultLogInstanceEx",           (void *)(uintptr_t)SUPR0GetDefaultLogInstanceEx },
    { "RTLogLoggerExV",                         (void *)(uintptr_t)RTLogLoggerExV },
    { "RTLogPrintfV",                           (void *)(uintptr_t)RTLogPrintfV },
    { "RTLogRelGetDefaultInstance",             (void *)(uintptr_t)RTLogRelGetDefaultInstance },
    { "RTLogRelGetDefaultInstanceEx",           (void *)(uintptr_t)RTLogRelGetDefaultInstanceEx },
    { "SUPR0GetDefaultLogRelInstanceEx",        (void *)(uintptr_t)SUPR0GetDefaultLogRelInstanceEx },
    { "RTLogSetDefaultInstanceThread",          (void *)(uintptr_t)RTLogSetDefaultInstanceThread },
    { "RTMemAllocExTag",                        (void *)(uintptr_t)RTMemAllocExTag },
    { "RTMemAllocTag",                          (void *)(uintptr_t)RTMemAllocTag },
    { "RTMemAllocVarTag",                       (void *)(uintptr_t)RTMemAllocVarTag },
    { "RTMemAllocZTag",                         (void *)(uintptr_t)RTMemAllocZTag },
    { "RTMemAllocZVarTag",                      (void *)(uintptr_t)RTMemAllocZVarTag },
    { "RTMemDupExTag",                          (void *)(uintptr_t)RTMemDupExTag },
    { "RTMemDupTag",                            (void *)(uintptr_t)RTMemDupTag },
    { "RTMemFree",                              (void *)(uintptr_t)RTMemFree },
    { "RTMemFreeEx",                            (void *)(uintptr_t)RTMemFreeEx },
    { "RTMemReallocTag",                        (void *)(uintptr_t)RTMemReallocTag },
    { "RTMpCpuId",                              (void *)(uintptr_t)RTMpCpuId },
    { "RTMpCpuIdFromSetIndex",                  (void *)(uintptr_t)RTMpCpuIdFromSetIndex },
    { "RTMpCpuIdToSetIndex",                    (void *)(uintptr_t)RTMpCpuIdToSetIndex },
    { "RTMpCurSetIndex",                        (void *)(uintptr_t)RTMpCurSetIndex },
    { "RTMpCurSetIndexAndId",                   (void *)(uintptr_t)RTMpCurSetIndexAndId },
    { "RTMpGetArraySize",                       (void *)(uintptr_t)RTMpGetArraySize },
    { "RTMpGetCount",                           (void *)(uintptr_t)RTMpGetCount },
    { "RTMpGetMaxCpuId",                        (void *)(uintptr_t)RTMpGetMaxCpuId },
    { "RTMpGetOnlineCount",                     (void *)(uintptr_t)RTMpGetOnlineCount },
    { "RTMpGetOnlineSet",                       (void *)(uintptr_t)RTMpGetOnlineSet },
    { "RTMpGetSet",                             (void *)(uintptr_t)RTMpGetSet },
    { "RTMpIsCpuOnline",                        (void *)(uintptr_t)RTMpIsCpuOnline },
    { "RTMpIsCpuPossible",                      (void *)(uintptr_t)RTMpIsCpuPossible },
    { "RTMpIsCpuWorkPending",                   (void *)(uintptr_t)RTMpIsCpuWorkPending },
    { "RTMpNotificationDeregister",             (void *)(uintptr_t)RTMpNotificationDeregister },
    { "RTMpNotificationRegister",               (void *)(uintptr_t)RTMpNotificationRegister },
    { "RTMpOnAll",                              (void *)(uintptr_t)RTMpOnAll },
    { "RTMpOnOthers",                           (void *)(uintptr_t)RTMpOnOthers },
    { "RTMpOnSpecific",                         (void *)(uintptr_t)RTMpOnSpecific },
    { "RTMpPokeCpu",                            (void *)(uintptr_t)RTMpPokeCpu },
    { "RTNetIPv4AddDataChecksum",               (void *)(uintptr_t)RTNetIPv4AddDataChecksum },
    { "RTNetIPv4AddTCPChecksum",                (void *)(uintptr_t)RTNetIPv4AddTCPChecksum },
    { "RTNetIPv4AddUDPChecksum",                (void *)(uintptr_t)RTNetIPv4AddUDPChecksum },
    { "RTNetIPv4FinalizeChecksum",              (void *)(uintptr_t)RTNetIPv4FinalizeChecksum },
    { "RTNetIPv4HdrChecksum",                   (void *)(uintptr_t)RTNetIPv4HdrChecksum },
    { "RTNetIPv4IsDHCPValid",                   (void *)(uintptr_t)RTNetIPv4IsDHCPValid },
    { "RTNetIPv4IsHdrValid",                    (void *)(uintptr_t)RTNetIPv4IsHdrValid },
    { "RTNetIPv4IsTCPSizeValid",                (void *)(uintptr_t)RTNetIPv4IsTCPSizeValid },
    { "RTNetIPv4IsTCPValid",                    (void *)(uintptr_t)RTNetIPv4IsTCPValid },
    { "RTNetIPv4IsUDPSizeValid",                (void *)(uintptr_t)RTNetIPv4IsUDPSizeValid },
    { "RTNetIPv4IsUDPValid",                    (void *)(uintptr_t)RTNetIPv4IsUDPValid },
    { "RTNetIPv4PseudoChecksum",                (void *)(uintptr_t)RTNetIPv4PseudoChecksum },
    { "RTNetIPv4PseudoChecksumBits",            (void *)(uintptr_t)RTNetIPv4PseudoChecksumBits },
    { "RTNetIPv4TCPChecksum",                   (void *)(uintptr_t)RTNetIPv4TCPChecksum },
    { "RTNetIPv4UDPChecksum",                   (void *)(uintptr_t)RTNetIPv4UDPChecksum },
    { "RTNetIPv6PseudoChecksum",                (void *)(uintptr_t)RTNetIPv6PseudoChecksum },
    { "RTNetIPv6PseudoChecksumBits",            (void *)(uintptr_t)RTNetIPv6PseudoChecksumBits },
    { "RTNetIPv6PseudoChecksumEx",              (void *)(uintptr_t)RTNetIPv6PseudoChecksumEx },
    { "RTNetTCPChecksum",                       (void *)(uintptr_t)RTNetTCPChecksum },
    { "RTNetUDPChecksum",                       (void *)(uintptr_t)RTNetUDPChecksum },
    { "RTPowerNotificationDeregister",          (void *)(uintptr_t)RTPowerNotificationDeregister },
    { "RTPowerNotificationRegister",            (void *)(uintptr_t)RTPowerNotificationRegister },
    { "RTProcSelf",                             (void *)(uintptr_t)RTProcSelf },
    { "RTR0AssertPanicSystem",                  (void *)(uintptr_t)RTR0AssertPanicSystem },
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
    { "RTR0DbgKrnlInfoOpen",                    (void *)(uintptr_t)RTR0DbgKrnlInfoOpen },          /* only-darwin, only-solaris, only-windows */
    { "RTR0DbgKrnlInfoQueryMember",             (void *)(uintptr_t)RTR0DbgKrnlInfoQueryMember },   /* only-darwin, only-solaris, only-windows */
# if defined(RT_OS_SOLARIS)
    { "RTR0DbgKrnlInfoQuerySize",               (void *)(uintptr_t)RTR0DbgKrnlInfoQuerySize },     /* only-solaris */
# endif
    { "RTR0DbgKrnlInfoQuerySymbol",             (void *)(uintptr_t)RTR0DbgKrnlInfoQuerySymbol },   /* only-darwin, only-solaris, only-windows */
    { "RTR0DbgKrnlInfoRelease",                 (void *)(uintptr_t)RTR0DbgKrnlInfoRelease },       /* only-darwin, only-solaris, only-windows */
    { "RTR0DbgKrnlInfoRetain",                  (void *)(uintptr_t)RTR0DbgKrnlInfoRetain },        /* only-darwin, only-solaris, only-windows */
#endif
    { "RTR0MemAreKrnlAndUsrDifferent",          (void *)(uintptr_t)RTR0MemAreKrnlAndUsrDifferent },
    { "RTR0MemKernelIsValidAddr",               (void *)(uintptr_t)RTR0MemKernelIsValidAddr },
    { "RTR0MemKernelCopyFrom",                  (void *)(uintptr_t)RTR0MemKernelCopyFrom },
    { "RTR0MemKernelCopyTo",                    (void *)(uintptr_t)RTR0MemKernelCopyTo },
    { "RTR0MemObjAddress",                      (void *)(uintptr_t)RTR0MemObjAddress },
    { "RTR0MemObjAddressR3",                    (void *)(uintptr_t)RTR0MemObjAddressR3 },
    { "RTR0MemObjAllocContTag",                 (void *)(uintptr_t)RTR0MemObjAllocContTag },
    { "RTR0MemObjAllocLowTag",                  (void *)(uintptr_t)RTR0MemObjAllocLowTag },
    { "RTR0MemObjAllocPageTag",                 (void *)(uintptr_t)RTR0MemObjAllocPageTag },
    { "RTR0MemObjAllocPhysExTag",               (void *)(uintptr_t)RTR0MemObjAllocPhysExTag },
    { "RTR0MemObjAllocPhysNCTag",               (void *)(uintptr_t)RTR0MemObjAllocPhysNCTag },
    { "RTR0MemObjAllocPhysTag",                 (void *)(uintptr_t)RTR0MemObjAllocPhysTag },
    { "RTR0MemObjEnterPhysTag",                 (void *)(uintptr_t)RTR0MemObjEnterPhysTag },
    { "RTR0MemObjFree",                         (void *)(uintptr_t)RTR0MemObjFree },
    { "RTR0MemObjGetPagePhysAddr",              (void *)(uintptr_t)RTR0MemObjGetPagePhysAddr },
    { "RTR0MemObjIsMapping",                    (void *)(uintptr_t)RTR0MemObjIsMapping },
    { "RTR0MemObjLockUserTag",                  (void *)(uintptr_t)RTR0MemObjLockUserTag },
    { "RTR0MemObjMapKernelExTag",               (void *)(uintptr_t)RTR0MemObjMapKernelExTag },
    { "RTR0MemObjMapKernelTag",                 (void *)(uintptr_t)RTR0MemObjMapKernelTag },
    { "RTR0MemObjMapUserTag",                   (void *)(uintptr_t)RTR0MemObjMapUserTag },
    { "RTR0MemObjProtect",                      (void *)(uintptr_t)RTR0MemObjProtect },
    { "RTR0MemObjSize",                         (void *)(uintptr_t)RTR0MemObjSize },
    { "RTR0MemUserCopyFrom",                    (void *)(uintptr_t)RTR0MemUserCopyFrom },
    { "RTR0MemUserCopyTo",                      (void *)(uintptr_t)RTR0MemUserCopyTo },
    { "RTR0MemUserIsValidAddr",                 (void *)(uintptr_t)RTR0MemUserIsValidAddr },
    { "RTR0ProcHandleSelf",                     (void *)(uintptr_t)RTR0ProcHandleSelf },
    { "RTSemEventCreate",                       (void *)(uintptr_t)RTSemEventCreate },
    { "RTSemEventDestroy",                      (void *)(uintptr_t)RTSemEventDestroy },
    { "RTSemEventGetResolution",                (void *)(uintptr_t)RTSemEventGetResolution },
    { "RTSemEventMultiCreate",                  (void *)(uintptr_t)RTSemEventMultiCreate },
    { "RTSemEventMultiDestroy",                 (void *)(uintptr_t)RTSemEventMultiDestroy },
    { "RTSemEventMultiGetResolution",           (void *)(uintptr_t)RTSemEventMultiGetResolution },
    { "RTSemEventMultiReset",                   (void *)(uintptr_t)RTSemEventMultiReset },
    { "RTSemEventMultiSignal",                  (void *)(uintptr_t)RTSemEventMultiSignal },
    { "RTSemEventMultiWait",                    (void *)(uintptr_t)RTSemEventMultiWait },
    { "RTSemEventMultiWaitEx",                  (void *)(uintptr_t)RTSemEventMultiWaitEx },
    { "RTSemEventMultiWaitExDebug",             (void *)(uintptr_t)RTSemEventMultiWaitExDebug },
    { "RTSemEventMultiWaitNoResume",            (void *)(uintptr_t)RTSemEventMultiWaitNoResume },
    { "RTSemEventSignal",                       (void *)(uintptr_t)RTSemEventSignal },
    { "RTSemEventWait",                         (void *)(uintptr_t)RTSemEventWait },
    { "RTSemEventWaitEx",                       (void *)(uintptr_t)RTSemEventWaitEx },
    { "RTSemEventWaitExDebug",                  (void *)(uintptr_t)RTSemEventWaitExDebug },
    { "RTSemEventWaitNoResume",                 (void *)(uintptr_t)RTSemEventWaitNoResume },
    { "RTSemFastMutexCreate",                   (void *)(uintptr_t)RTSemFastMutexCreate },
    { "RTSemFastMutexDestroy",                  (void *)(uintptr_t)RTSemFastMutexDestroy },
    { "RTSemFastMutexRelease",                  (void *)(uintptr_t)RTSemFastMutexRelease },
    { "RTSemFastMutexRequest",                  (void *)(uintptr_t)RTSemFastMutexRequest },
    { "RTSemMutexCreate",                       (void *)(uintptr_t)RTSemMutexCreate },
    { "RTSemMutexDestroy",                      (void *)(uintptr_t)RTSemMutexDestroy },
    { "RTSemMutexRelease",                      (void *)(uintptr_t)RTSemMutexRelease },
    { "RTSemMutexRequest",                      (void *)(uintptr_t)RTSemMutexRequest },
    { "RTSemMutexRequestDebug",                 (void *)(uintptr_t)RTSemMutexRequestDebug },
    { "RTSemMutexRequestNoResume",              (void *)(uintptr_t)RTSemMutexRequestNoResume },
    { "RTSemMutexRequestNoResumeDebug",         (void *)(uintptr_t)RTSemMutexRequestNoResumeDebug },
    { "RTSpinlockAcquire",                      (void *)(uintptr_t)RTSpinlockAcquire },
    { "RTSpinlockCreate",                       (void *)(uintptr_t)RTSpinlockCreate },
    { "RTSpinlockDestroy",                      (void *)(uintptr_t)RTSpinlockDestroy },
    { "RTSpinlockRelease",                      (void *)(uintptr_t)RTSpinlockRelease },
    { "RTStrCopy",                              (void *)(uintptr_t)RTStrCopy },
    { "RTStrDupTag",                            (void *)(uintptr_t)RTStrDupTag },
    { "RTStrFormat",                            (void *)(uintptr_t)RTStrFormat },
    { "RTStrFormatNumber",                      (void *)(uintptr_t)RTStrFormatNumber },
    { "RTStrFormatTypeDeregister",              (void *)(uintptr_t)RTStrFormatTypeDeregister },
    { "RTStrFormatTypeRegister",                (void *)(uintptr_t)RTStrFormatTypeRegister },
    { "RTStrFormatTypeSetUser",                 (void *)(uintptr_t)RTStrFormatTypeSetUser },
    { "RTStrFormatV",                           (void *)(uintptr_t)RTStrFormatV },
    { "RTStrFree",                              (void *)(uintptr_t)RTStrFree },
    { "RTStrNCmp",                              (void *)(uintptr_t)RTStrNCmp },
    { "RTStrPrintf",                            (void *)(uintptr_t)RTStrPrintf },
    { "RTStrPrintfEx",                          (void *)(uintptr_t)RTStrPrintfEx },
    { "RTStrPrintfExV",                         (void *)(uintptr_t)RTStrPrintfExV },
    { "RTStrPrintfV",                           (void *)(uintptr_t)RTStrPrintfV },
    { "RTThreadCreate",                         (void *)(uintptr_t)RTThreadCreate },
    { "RTThreadCtxHookIsEnabled",               (void *)(uintptr_t)RTThreadCtxHookIsEnabled },
    { "RTThreadCtxHookCreate",                  (void *)(uintptr_t)RTThreadCtxHookCreate },
    { "RTThreadCtxHookDestroy",                 (void *)(uintptr_t)RTThreadCtxHookDestroy },
    { "RTThreadCtxHookDisable",                 (void *)(uintptr_t)RTThreadCtxHookDisable },
    { "RTThreadCtxHookEnable",                  (void *)(uintptr_t)RTThreadCtxHookEnable },
    { "RTThreadGetName",                        (void *)(uintptr_t)RTThreadGetName },
    { "RTThreadGetNative",                      (void *)(uintptr_t)RTThreadGetNative },
    { "RTThreadGetType",                        (void *)(uintptr_t)RTThreadGetType },
    { "RTThreadIsInInterrupt",                  (void *)(uintptr_t)RTThreadIsInInterrupt },
    { "RTThreadNativeSelf",                     (void *)(uintptr_t)RTThreadNativeSelf },
    { "RTThreadPreemptDisable",                 (void *)(uintptr_t)RTThreadPreemptDisable },
    { "RTThreadPreemptIsEnabled",               (void *)(uintptr_t)RTThreadPreemptIsEnabled },
    { "RTThreadPreemptIsPending",               (void *)(uintptr_t)RTThreadPreemptIsPending },
    { "RTThreadPreemptIsPendingTrusty",         (void *)(uintptr_t)RTThreadPreemptIsPendingTrusty },
    { "RTThreadPreemptIsPossible",              (void *)(uintptr_t)RTThreadPreemptIsPossible },
    { "RTThreadPreemptRestore",                 (void *)(uintptr_t)RTThreadPreemptRestore },
    { "RTThreadSelf",                           (void *)(uintptr_t)RTThreadSelf },
    { "RTThreadSelfName",                       (void *)(uintptr_t)RTThreadSelfName },
    { "RTThreadSleep",                          (void *)(uintptr_t)RTThreadSleep },
    { "RTThreadUserReset",                      (void *)(uintptr_t)RTThreadUserReset },
    { "RTThreadUserSignal",                     (void *)(uintptr_t)RTThreadUserSignal },
    { "RTThreadUserWait",                       (void *)(uintptr_t)RTThreadUserWait },
    { "RTThreadUserWaitNoResume",               (void *)(uintptr_t)RTThreadUserWaitNoResume },
    { "RTThreadWait",                           (void *)(uintptr_t)RTThreadWait },
    { "RTThreadWaitNoResume",                   (void *)(uintptr_t)RTThreadWaitNoResume },
    { "RTThreadYield",                          (void *)(uintptr_t)RTThreadYield },
    { "RTTimeMilliTS",                          (void *)(uintptr_t)RTTimeMilliTS },
    { "RTTimeNanoTS",                           (void *)(uintptr_t)RTTimeNanoTS },
    { "RTTimeNow",                              (void *)(uintptr_t)RTTimeNow },
    { "RTTimerCanDoHighResolution",             (void *)(uintptr_t)RTTimerCanDoHighResolution },
    { "RTTimerChangeInterval",                  (void *)(uintptr_t)RTTimerChangeInterval },
    { "RTTimerCreate",                          (void *)(uintptr_t)RTTimerCreate },
    { "RTTimerCreateEx",                        (void *)(uintptr_t)RTTimerCreateEx },
    { "RTTimerDestroy",                         (void *)(uintptr_t)RTTimerDestroy },
    { "RTTimerGetSystemGranularity",            (void *)(uintptr_t)RTTimerGetSystemGranularity },
    { "RTTimerReleaseSystemGranularity",        (void *)(uintptr_t)RTTimerReleaseSystemGranularity },
    { "RTTimerRequestSystemGranularity",        (void *)(uintptr_t)RTTimerRequestSystemGranularity },
    { "RTTimerStart",                           (void *)(uintptr_t)RTTimerStart },
    { "RTTimerStop",                            (void *)(uintptr_t)RTTimerStop },
    { "RTTimeSystemMilliTS",                    (void *)(uintptr_t)RTTimeSystemMilliTS },
    { "RTTimeSystemNanoTS",                     (void *)(uintptr_t)RTTimeSystemNanoTS },
    { "RTUuidCompare",                          (void *)(uintptr_t)RTUuidCompare },
    { "RTUuidCompareStr",                       (void *)(uintptr_t)RTUuidCompareStr },
    { "RTUuidFromStr",                          (void *)(uintptr_t)RTUuidFromStr },
/* SED: END */
};

#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
/**
 * Drag in the rest of IRPT since we share it with the
 * rest of the kernel modules on darwin.
 */
PFNRT g_apfnVBoxDrvIPRTDeps[] =
{
    /* VBoxNetAdp */
    (PFNRT)RTRandBytes,
    /* VBoxUSB */
    (PFNRT)RTPathStripFilename,
    (PFNRT)RTHandleTableAlloc,
#if !defined(RT_OS_FREEBSD)
    (PFNRT)RTStrPurgeEncoding,
#endif
    NULL
};
#endif  /* RT_OS_DARWIN || RT_OS_SOLARIS || RT_OS_SOLARIS */


/**
 * Initializes the device extentsion structure.
 *
 * @returns IPRT status code.
 * @param   pDevExt     The device extension to initialize.
 * @param   cbSession   The size of the session structure.  The size of
 *                      SUPDRVSESSION may be smaller when SUPDRV_AGNOSTIC is
 *                      defined because we're skipping the OS specific members
 *                      then.
 */
int VBOXCALL supdrvInitDevExt(PSUPDRVDEVEXT pDevExt, size_t cbSession)
{
    int rc;

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    /*
     * Create the release log.
     */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    PRTLOGGER pRelLogger;
    rc = RTLogCreate(&pRelLogger, 0 /* fFlags */, "all",
                     "VBOX_RELEASE_LOG", RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER, NULL);
    if (RT_SUCCESS(rc))
        RTLogRelSetDefaultInstance(pRelLogger);
    /** @todo Add native hook for getting logger config parameters and setting
     *        them. On linux we should use the module parameter stuff... */
#endif

#if (defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)) && !defined(VBOX_WITH_OLD_CPU_SUPPORT)
    /*
     * Require SSE2 to be present.
     */
    if (!(ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_SSE2))
    {
        SUPR0Printf("vboxdrv: Requires SSE2 (cpuid(0).EDX=%#x)\n", ASMCpuId_EDX(1));
        return VERR_UNSUPPORTED_CPU;
    }
#endif

    /*
     * Initialize it.
     */
    memset(pDevExt, 0, sizeof(*pDevExt)); /* Does not wipe OS specific tail section of the structure. */
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    pDevExt->mtxLdr = NIL_RTSEMMUTEX;
#else
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
#endif
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    pDevExt->mtxGip = NIL_RTSEMMUTEX;
    pDevExt->mtxTscDelta = NIL_RTSEMMUTEX;
#else
    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
    pDevExt->mtxTscDelta = NIL_RTSEMFASTMUTEX;
#endif

    rc = RTSpinlockCreate(&pDevExt->Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvDevExt");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->hGipSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvGip");
    if (RT_SUCCESS(rc))
        rc = RTSpinlockCreate(&pDevExt->hSessionHashTabSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_SAFE, "SUPDrvSession");

    if (RT_SUCCESS(rc))
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
        rc = RTSemMutexCreate(&pDevExt->mtxLdr);
#else
        rc = RTSemFastMutexCreate(&pDevExt->mtxLdr);
#endif
    if (RT_SUCCESS(rc))
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
        rc = RTSemMutexCreate(&pDevExt->mtxTscDelta);
#else
        rc = RTSemFastMutexCreate(&pDevExt->mtxTscDelta);
#endif
    if (RT_SUCCESS(rc))
    {
        rc = RTSemFastMutexCreate(&pDevExt->mtxComponentFactory);
        if (RT_SUCCESS(rc))
        {
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
            rc = RTSemMutexCreate(&pDevExt->mtxGip);
#else
            rc = RTSemFastMutexCreate(&pDevExt->mtxGip);
#endif
            if (RT_SUCCESS(rc))
            {
                rc = supdrvGipCreate(pDevExt);
                if (RT_SUCCESS(rc))
                {
                    rc = supdrvTracerInit(pDevExt);
                    if (RT_SUCCESS(rc))
                    {
                        pDevExt->pLdrInitImage  = NULL;
                        pDevExt->hLdrInitThread = NIL_RTNATIVETHREAD;
                        pDevExt->u32Cookie      = BIRD;  /** @todo make this random? */
                        pDevExt->cbSession      = (uint32_t)cbSession;

                        /*
                         * Fixup the absolute symbols.
                         *
                         * Because of the table indexing assumptions we'll have a little #ifdef orgy
                         * here rather than distributing this to OS specific files. At least for now.
                         */
#ifdef RT_OS_DARWIN
# if ARCH_BITS == 32
                        if (SUPR0GetPagingMode() >= SUPPAGINGMODE_AMD64)
                        {
                            g_aFunctions[0].pfn = (void *)1;                    /* SUPR0AbsIs64bit */
                            g_aFunctions[1].pfn = (void *)0x80;                 /* SUPR0Abs64bitKernelCS - KERNEL64_CS, seg.h */
                            g_aFunctions[2].pfn = (void *)0x88;                 /* SUPR0Abs64bitKernelSS - KERNEL64_SS, seg.h */
                            g_aFunctions[3].pfn = (void *)0x88;                 /* SUPR0Abs64bitKernelDS - KERNEL64_SS, seg.h */
                        }
                        else
                            g_aFunctions[0].pfn = g_aFunctions[1].pfn = g_aFunctions[2].pfn = g_aFunctions[3].pfn = (void *)0;
                        g_aFunctions[4].pfn = (void *)0x08;                     /* SUPR0AbsKernelCS - KERNEL_CS, seg.h */
                        g_aFunctions[5].pfn = (void *)0x10;                     /* SUPR0AbsKernelSS - KERNEL_DS, seg.h */
                        g_aFunctions[6].pfn = (void *)0x10;                     /* SUPR0AbsKernelDS - KERNEL_DS, seg.h */
                        g_aFunctions[7].pfn = (void *)0x10;                     /* SUPR0AbsKernelES - KERNEL_DS, seg.h */
                        g_aFunctions[8].pfn = (void *)0x10;                     /* SUPR0AbsKernelFS - KERNEL_DS, seg.h */
                        g_aFunctions[9].pfn = (void *)0x48;                     /* SUPR0AbsKernelGS - CPU_DATA_GS, seg.h */
# else /* 64-bit darwin: */
                        g_aFunctions[0].pfn = (void *)1;                        /* SUPR0AbsIs64bit */
                        g_aFunctions[1].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0Abs64bitKernelCS */
                        g_aFunctions[2].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0Abs64bitKernelSS */
                        g_aFunctions[3].pfn = (void *)0;                        /* SUPR0Abs64bitKernelDS */
                        g_aFunctions[4].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0AbsKernelCS */
                        g_aFunctions[5].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0AbsKernelSS */
                        g_aFunctions[6].pfn = (void *)0;                        /* SUPR0AbsKernelDS */
                        g_aFunctions[7].pfn = (void *)0;                        /* SUPR0AbsKernelES */
                        g_aFunctions[8].pfn = (void *)0;                        /* SUPR0AbsKernelFS */
                        g_aFunctions[9].pfn = (void *)0;                        /* SUPR0AbsKernelGS */

# endif
#else  /* !RT_OS_DARWIN */
# if ARCH_BITS == 64
                        g_aFunctions[0].pfn = (void *)1;                        /* SUPR0AbsIs64bit */
                        g_aFunctions[1].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0Abs64bitKernelCS */
                        g_aFunctions[2].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0Abs64bitKernelSS */
                        g_aFunctions[3].pfn = (void *)(uintptr_t)ASMGetDS();    /* SUPR0Abs64bitKernelDS */
# else
                        g_aFunctions[0].pfn = g_aFunctions[1].pfn = g_aFunctions[2].pfn = g_aFunctions[3].pfn = (void *)0;
# endif
                        g_aFunctions[4].pfn = (void *)(uintptr_t)ASMGetCS();    /* SUPR0AbsKernelCS */
                        g_aFunctions[5].pfn = (void *)(uintptr_t)ASMGetSS();    /* SUPR0AbsKernelSS */
                        g_aFunctions[6].pfn = (void *)(uintptr_t)ASMGetDS();    /* SUPR0AbsKernelDS */
                        g_aFunctions[7].pfn = (void *)(uintptr_t)ASMGetES();    /* SUPR0AbsKernelES */
                        g_aFunctions[8].pfn = (void *)(uintptr_t)ASMGetFS();    /* SUPR0AbsKernelFS */
                        g_aFunctions[9].pfn = (void *)(uintptr_t)ASMGetGS();    /* SUPR0AbsKernelGS */
#endif /* !RT_OS_DARWIN */
                        return VINF_SUCCESS;
                    }

                    supdrvGipDestroy(pDevExt);
                }

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
                RTSemMutexDestroy(pDevExt->mtxGip);
                pDevExt->mtxGip = NIL_RTSEMMUTEX;
#else
                RTSemFastMutexDestroy(pDevExt->mtxGip);
                pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
#endif
            }
            RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
            pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;
        }
    }

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexDestroy(pDevExt->mtxTscDelta);
    pDevExt->mtxTscDelta = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxTscDelta);
    pDevExt->mtxTscDelta = NIL_RTSEMFASTMUTEX;
#endif
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    RTSemMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
#endif
    RTSpinlockDestroy(pDevExt->Spinlock);
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    RTSpinlockDestroy(pDevExt->hGipSpinlock);
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;
    RTSpinlockDestroy(pDevExt->hSessionHashTabSpinlock);
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif

    return rc;
}


/**
 * Delete the device extension (e.g. cleanup members).
 *
 * @param   pDevExt     The device extension to delete.
 */
void VBOXCALL supdrvDeleteDevExt(PSUPDRVDEVEXT pDevExt)
{
    PSUPDRVOBJ          pObj;
    PSUPDRVUSAGE        pUsage;

    /*
     * Kill mutexes and spinlocks.
     */
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMMUTEX;
    RTSemMutexDestroy(pDevExt->mtxTscDelta);
    pDevExt->mtxTscDelta = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxGip);
    pDevExt->mtxGip = NIL_RTSEMFASTMUTEX;
    RTSemFastMutexDestroy(pDevExt->mtxTscDelta);
    pDevExt->mtxTscDelta = NIL_RTSEMFASTMUTEX;
#endif
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    RTSemMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMMUTEX;
#else
    RTSemFastMutexDestroy(pDevExt->mtxLdr);
    pDevExt->mtxLdr = NIL_RTSEMFASTMUTEX;
#endif
    RTSpinlockDestroy(pDevExt->Spinlock);
    pDevExt->Spinlock = NIL_RTSPINLOCK;
    RTSemFastMutexDestroy(pDevExt->mtxComponentFactory);
    pDevExt->mtxComponentFactory = NIL_RTSEMFASTMUTEX;
    RTSpinlockDestroy(pDevExt->hSessionHashTabSpinlock);
    pDevExt->hSessionHashTabSpinlock = NIL_RTSPINLOCK;

    /*
     * Free lists.
     */
    /* objects. */
    pObj = pDevExt->pObjs;
    Assert(!pObj);                      /* (can trigger on forced unloads) */
    pDevExt->pObjs = NULL;
    while (pObj)
    {
        void *pvFree = pObj;
        pObj = pObj->pNext;
        RTMemFree(pvFree);
    }

    /* usage records. */
    pUsage = pDevExt->pUsageFree;
    pDevExt->pUsageFree = NULL;
    while (pUsage)
    {
        void *pvFree = pUsage;
        pUsage = pUsage->pNext;
        RTMemFree(pvFree);
    }

    /* kill the GIP. */
    supdrvGipDestroy(pDevExt);
    RTSpinlockDestroy(pDevExt->hGipSpinlock);
    pDevExt->hGipSpinlock = NIL_RTSPINLOCK;

    supdrvTracerTerm(pDevExt);

#ifdef SUPDRV_WITH_RELEASE_LOGGER
    /* destroy the loggers. */
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
    RTLogDestroy(RTLogSetDefaultInstance(NULL));
#endif
}


/**
 * Create session.
 *
 * @returns IPRT status code.
 * @param   pDevExt         Device extension.
 * @param   fUser           Flag indicating whether this is a user or kernel
 *                          session.
 * @param   fUnrestricted   Unrestricted access (system) or restricted access
 *                          (user)?
 * @param   ppSession       Where to store the pointer to the session data.
 */
int VBOXCALL supdrvCreateSession(PSUPDRVDEVEXT pDevExt, bool fUser, bool fUnrestricted, PSUPDRVSESSION *ppSession)
{
    int             rc;
    PSUPDRVSESSION  pSession;

    if (!SUP_IS_DEVEXT_VALID(pDevExt))
        return VERR_INVALID_PARAMETER;

    /*
     * Allocate memory for the session data.
     */
    pSession = *ppSession = (PSUPDRVSESSION)RTMemAllocZ(pDevExt->cbSession);
    if (pSession)
    {
        /* Initialize session data. */
        rc = RTSpinlockCreate(&pSession->Spinlock, RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, "SUPDrvSession");
        if (!rc)
        {
            rc = RTHandleTableCreateEx(&pSession->hHandleTable,
                                       RTHANDLETABLE_FLAGS_LOCKED_IRQ_SAFE | RTHANDLETABLE_FLAGS_CONTEXT,
                                       1 /*uBase*/, 32768 /*cMax*/, supdrvSessionObjHandleRetain, pSession);
            if (RT_SUCCESS(rc))
            {
                Assert(pSession->Spinlock != NIL_RTSPINLOCK);
                pSession->pDevExt           = pDevExt;
                pSession->u32Cookie         = BIRD_INV;
                pSession->fUnrestricted     = fUnrestricted;
                /*pSession->fInHashTable      = false; */
                pSession->cRefs             = 1;
                /*pSession->pCommonNextHash   = NULL;
                pSession->ppOsSessionPtr    = NULL; */
                if (fUser)
                {
                    pSession->Process       = RTProcSelf();
                    pSession->R0Process     = RTR0ProcHandleSelf();
                }
                else
                {
                    pSession->Process       = NIL_RTPROCESS;
                    pSession->R0Process     = NIL_RTR0PROCESS;
                }
                /*pSession->pLdrUsage         = NULL;
                pSession->pVM               = NULL;
                pSession->pUsage            = NULL;
                pSession->pGip              = NULL;
                pSession->fGipReferenced    = false;
                pSession->Bundle.cUsed      = 0; */
                pSession->Uid               = NIL_RTUID;
                pSession->Gid               = NIL_RTGID;
                /*pSession->uTracerData       = 0;*/
                pSession->hTracerCaller     = NIL_RTNATIVETHREAD;
                RTListInit(&pSession->TpProviders);
                /*pSession->cTpProviders      = 0;*/
                /*pSession->cTpProbesFiring   = 0;*/
                RTListInit(&pSession->TpUmods);
                /*RT_ZERO(pSession->apTpLookupTable);*/

                VBOXDRV_SESSION_CREATE(pSession, fUser);
                LogFlow(("Created session %p initial cookie=%#x\n", pSession, pSession->u32Cookie));
                return VINF_SUCCESS;
            }

            RTSpinlockDestroy(pSession->Spinlock);
        }
        RTMemFree(pSession);
        *ppSession = NULL;
        Log(("Failed to create spinlock, rc=%d!\n", rc));
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


/**
 * Cleans up the session in the context of the process to which it belongs, the
 * caller will free the session and the session spinlock.
 *
 * This should normally occur when the session is closed or as the process
 * exits.  Careful reference counting in the OS specfic code makes sure that
 * there cannot be any races between process/handle cleanup callbacks and
 * threads doing I/O control calls.
 *
 * @param   pDevExt     The device extension.
 * @param   pSession    Session data.
 */
static void supdrvCleanupSession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    int                 rc;
    PSUPDRVBUNDLE       pBundle;
    LogFlow(("supdrvCleanupSession: pSession=%p\n", pSession));

    Assert(!pSession->fInHashTable);
    Assert(!pSession->ppOsSessionPtr);
    AssertLogRelMsg(pSession->R0Process == RTR0ProcHandleSelf() || pSession->R0Process == NIL_RTR0PROCESS,
                    ("R0Process=%p cur=%p; curpid=%u\n",
                    pSession->R0Process, RTR0ProcHandleSelf(), RTProcSelf()));

    /*
     * Remove logger instances related to this session.
     */
    RTLogSetDefaultInstanceThread(NULL, (uintptr_t)pSession);

    /*
     * Destroy the handle table.
     */
    rc = RTHandleTableDestroy(pSession->hHandleTable, supdrvSessionObjHandleDelete, pSession);
    AssertRC(rc);
    pSession->hHandleTable = NIL_RTHANDLETABLE;

    /*
     * Release object references made in this session.
     * In theory there should be noone racing us in this session.
     */
    Log2(("release objects - start\n"));
    if (pSession->pUsage)
    {
        PSUPDRVUSAGE    pUsage;
        RTSpinlockAcquire(pDevExt->Spinlock);

        while ((pUsage = pSession->pUsage) != NULL)
        {
            PSUPDRVOBJ  pObj = pUsage->pObj;
            pSession->pUsage = pUsage->pNext;

            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage < pObj->cUsage)
            {
                pObj->cUsage -= pUsage->cUsage;
                RTSpinlockRelease(pDevExt->Spinlock);
            }
            else
            {
                /* Destroy the object and free the record. */
                if (pDevExt->pObjs == pObj)
                    pDevExt->pObjs = pObj->pNext;
                else
                {
                    PSUPDRVOBJ pObjPrev;
                    for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                        if (pObjPrev->pNext == pObj)
                        {
                            pObjPrev->pNext = pObj->pNext;
                            break;
                        }
                    Assert(pObjPrev);
                }
                RTSpinlockRelease(pDevExt->Spinlock);

                Log(("supdrvCleanupSession: destroying %p/%d (%p/%p) cpid=%RTproc pid=%RTproc dtor=%p\n",
                     pObj, pObj->enmType, pObj->pvUser1, pObj->pvUser2, pObj->CreatorProcess, RTProcSelf(), pObj->pfnDestructor));
                if (pObj->pfnDestructor)
                    pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
                RTMemFree(pObj);
            }

            /* free it and continue. */
            RTMemFree(pUsage);

            RTSpinlockAcquire(pDevExt->Spinlock);
        }

        RTSpinlockRelease(pDevExt->Spinlock);
        AssertMsg(!pSession->pUsage, ("Some buster reregistered an object during desturction!\n"));
    }
    Log2(("release objects - done\n"));

    /*
     * Make sure the associated VM pointers are NULL.
     */
    if (pSession->pSessionGVM || pSession->pSessionVM || pSession->pFastIoCtrlVM)
    {
        SUPR0Printf("supdrvCleanupSession: VM not disassociated! pSessionGVM=%p pSessionVM=%p pFastIoCtrlVM=%p\n",
                    pSession->pSessionGVM, pSession->pSessionVM, pSession->pFastIoCtrlVM);
        pSession->pSessionGVM   = NULL;
        pSession->pSessionVM    = NULL;
        pSession->pFastIoCtrlVM = NULL;
    }

    /*
     * Do tracer cleanups related to this session.
     */
    Log2(("release tracer stuff - start\n"));
    supdrvTracerCleanupSession(pDevExt, pSession);
    Log2(("release tracer stuff - end\n"));

    /*
     * Release memory allocated in the session.
     *
     * We do not serialize this as we assume that the application will
     * not allocated memory while closing the file handle object.
     */
    Log2(("freeing memory:\n"));
    pBundle = &pSession->Bundle;
    while (pBundle)
    {
        PSUPDRVBUNDLE   pToFree;
        unsigned        i;

        /*
         * Check and unlock all entries in the bundle.
         */
        for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
        {
            if (pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ)
            {
                Log2(("eType=%d pvR0=%p pvR3=%p cb=%ld\n", pBundle->aMem[i].eType, RTR0MemObjAddress(pBundle->aMem[i].MemObj),
                      (void *)RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3), (long)RTR0MemObjSize(pBundle->aMem[i].MemObj)));
                if (pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ)
                {
                    rc = RTR0MemObjFree(pBundle->aMem[i].MapObjR3, false);
                    AssertRC(rc); /** @todo figure out how to handle this. */
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                }
                rc = RTR0MemObjFree(pBundle->aMem[i].MemObj, true /* fFreeMappings */);
                AssertRC(rc); /** @todo figure out how to handle this. */
                pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
            }
        }

        /*
         * Advance and free previous bundle.
         */
        pToFree = pBundle;
        pBundle = pBundle->pNext;

        pToFree->pNext = NULL;
        pToFree->cUsed = 0;
        if (pToFree != &pSession->Bundle)
            RTMemFree(pToFree);
    }
    Log2(("freeing memory - done\n"));

    /*
     * Deregister component factories.
     */
    RTSemFastMutexRequest(pDevExt->mtxComponentFactory);
    Log2(("deregistering component factories:\n"));
    if (pDevExt->pComponentFactoryHead)
    {
        PSUPDRVFACTORYREG pPrev = NULL;
        PSUPDRVFACTORYREG pCur = pDevExt->pComponentFactoryHead;
        while (pCur)
        {
            if (pCur->pSession == pSession)
            {
                /* unlink it */
                PSUPDRVFACTORYREG pNext = pCur->pNext;
                if (pPrev)
                    pPrev->pNext = pNext;
                else
                    pDevExt->pComponentFactoryHead = pNext;

                /* free it */
                pCur->pNext = NULL;
                pCur->pSession = NULL;
                pCur->pFactory = NULL;
                RTMemFree(pCur);

                /* next */
                pCur = pNext;
            }
            else
            {
                /* next */
                pPrev = pCur;
                pCur = pCur->pNext;
            }
        }
    }
    RTSemFastMutexRelease(pDevExt->mtxComponentFactory);
    Log2(("deregistering component factories - done\n"));

    /*
     * Loaded images needs to be dereferenced and possibly freed up.
     */
    supdrvLdrLock(pDevExt);
    Log2(("freeing images:\n"));
    if (pSession->pLdrUsage)
    {
        PSUPDRVLDRUSAGE pUsage = pSession->pLdrUsage;
        pSession->pLdrUsage = NULL;
        while (pUsage)
        {
            void           *pvFree = pUsage;
            PSUPDRVLDRIMAGE pImage = pUsage->pImage;
            if (pImage->cUsage > pUsage->cUsage)
                pImage->cUsage -= pUsage->cUsage;
            else
                supdrvLdrFree(pDevExt, pImage);
            pUsage->pImage = NULL;
            pUsage = pUsage->pNext;
            RTMemFree(pvFree);
        }
    }
    supdrvLdrUnlock(pDevExt);
    Log2(("freeing images - done\n"));

    /*
     * Unmap the GIP.
     */
    Log2(("umapping GIP:\n"));
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
    {
        SUPR0GipUnmap(pSession);
        pSession->fGipReferenced = 0;
    }
    Log2(("umapping GIP - done\n"));
}


/**
 * Common code for freeing a session when the reference count reaches zero.
 *
 * @param   pDevExt     Device extension.
 * @param   pSession    Session data.
 *                      This data will be freed by this routine.
 */
static void supdrvDestroySession(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    VBOXDRV_SESSION_CLOSE(pSession);

    /*
     * Cleanup the session first.
     */
    supdrvCleanupSession(pDevExt, pSession);
    supdrvOSCleanupSession(pDevExt, pSession);

    /*
     * Free the rest of the session stuff.
     */
    RTSpinlockDestroy(pSession->Spinlock);
    pSession->Spinlock = NIL_RTSPINLOCK;
    pSession->pDevExt = NULL;
    RTMemFree(pSession);
    LogFlow(("supdrvDestroySession: returns\n"));
}


/**
 * Inserts the session into the global hash table.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_WRONG_ORDER if the session was already inserted (asserted).
 * @retval  VERR_INVALID_PARAMETER if the session handle is invalid or a ring-0
 *          session (asserted).
 * @retval  VERR_DUPLICATE if there is already a session for that pid.
 *
 * @param   pDevExt         The device extension.
 * @param   pSession        The session.
 * @param   ppOsSessionPtr  Pointer to the OS session pointer, if any is
 *                          available and used.  This will set to point to the
 *                          session while under the protection of the session
 *                          hash table spinlock.  It will also be kept in
 *                          PSUPDRVSESSION::ppOsSessionPtr for lookup and
 *                          cleanup use.
 * @param   pvUser          Argument for supdrvOSSessionHashTabInserted.
 */
int VBOXCALL supdrvSessionHashTabInsert(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVSESSION *ppOsSessionPtr,
                                        void *pvUser)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process != NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(pSession->Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * If there are a collisions, we need to carefully check if we got a
     * duplicate.  There can only be one open session per process.
     */
    pCur = pDevExt->apSessionHashTab[iHash];
    if (pCur)
    {
        while (pCur && pCur->Process != pSession->Process)
            pCur = pCur->pCommonNextHash;

        if (pCur)
        {
            RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
            if (pCur == pSession)
            {
                Assert(pSession->fInHashTable);
                AssertFailed();
                return VERR_WRONG_ORDER;
            }
            Assert(!pSession->fInHashTable);
            if (pCur->R0Process == pSession->R0Process)
                return VERR_RESOURCE_IN_USE;
            return VERR_DUPLICATE;
        }
    }
    Assert(!pSession->fInHashTable);
    Assert(!pSession->ppOsSessionPtr);

    /*
     * Insert it, doing a callout to the OS specific code in case it has
     * anything it wishes to do while we're holding the spinlock.
     */
    pSession->pCommonNextHash = pDevExt->apSessionHashTab[iHash];
    pDevExt->apSessionHashTab[iHash] = pSession;
    pSession->fInHashTable    = true;
    ASMAtomicIncS32(&pDevExt->cSessions);

    pSession->ppOsSessionPtr = ppOsSessionPtr;
    if (ppOsSessionPtr)
        ASMAtomicWritePtr(ppOsSessionPtr, pSession);

    supdrvOSSessionHashTabInserted(pDevExt, pSession, pvUser);

    /*
     * Retain a reference for the pointer in the session table.
     */
    ASMAtomicIncU32(&pSession->cRefs);

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
    return VINF_SUCCESS;
}


/**
 * Removes the session from the global hash table.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_FOUND if the session was already removed (asserted).
 * @retval  VERR_INVALID_PARAMETER if the session handle is invalid or a ring-0
 *          session (asserted).
 *
 * @param   pDevExt     The device extension.
 * @param   pSession    The session. The caller is expected to have a reference
 *                      to this so it won't croak on us when we release the hash
 *                      table reference.
 * @param   pvUser      OS specific context value for the
 *                      supdrvOSSessionHashTabInserted callback.
 */
int VBOXCALL supdrvSessionHashTabRemove(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, void *pvUser)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;
    int32_t         cRefs;

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process != NIL_RTR0PROCESS, VERR_INVALID_PARAMETER);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(pSession->Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * Unlink it.
     */
    pCur = pDevExt->apSessionHashTab[iHash];
    if (pCur == pSession)
        pDevExt->apSessionHashTab[iHash] = pSession->pCommonNextHash;
    else
    {
        PSUPDRVSESSION pPrev = pCur;
        while (pCur && pCur != pSession)
        {
            pPrev = pCur;
            pCur  = pCur->pCommonNextHash;
        }
        if (pCur)
            pPrev->pCommonNextHash = pCur->pCommonNextHash;
        else
        {
            Assert(!pSession->fInHashTable);
            RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);
            return VERR_NOT_FOUND;
        }
    }

    pSession->pCommonNextHash = NULL;
    pSession->fInHashTable    = false;

    ASMAtomicDecS32(&pDevExt->cSessions);

    /*
     * Clear OS specific session pointer if available and do the OS callback.
     */
    if (pSession->ppOsSessionPtr)
    {
        ASMAtomicCmpXchgPtr(pSession->ppOsSessionPtr, NULL, pSession);
        pSession->ppOsSessionPtr = NULL;
    }

    supdrvOSSessionHashTabRemoved(pDevExt, pSession, pvUser);

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);

    /*
     * Drop the reference the hash table had to the session.  This shouldn't
     * be the last reference!
     */
    cRefs = ASMAtomicDecU32(&pSession->cRefs);
    Assert(cRefs > 0 && cRefs < _1M);
    if (cRefs == 0)
        supdrvDestroySession(pDevExt, pSession);

    return VINF_SUCCESS;
}


/**
 * Looks up the session for the current process in the global hash table or in
 * OS specific pointer.
 *
 * @returns Pointer to the session with a reference that the caller must
 *          release.  If no valid session was found, NULL is returned.
 *
 * @param   pDevExt         The device extension.
 * @param   Process         The process ID.
 * @param   R0Process       The ring-0 process handle.
 * @param   ppOsSessionPtr  The OS session pointer if available.  If not NULL,
 *                          this is used instead of the hash table.  For
 *                          additional safety it must then be equal to the
 *                          SUPDRVSESSION::ppOsSessionPtr member.
 *                          This can be NULL even if the OS has a session
 *                          pointer.
 */
PSUPDRVSESSION VBOXCALL supdrvSessionHashTabLookup(PSUPDRVDEVEXT pDevExt, RTPROCESS Process, RTR0PROCESS R0Process,
                                                   PSUPDRVSESSION *ppOsSessionPtr)
{
    PSUPDRVSESSION  pCur;
    unsigned        iHash;

    /*
     * Validate input.
     */
    AssertReturn(R0Process != NIL_RTR0PROCESS, NULL);

    /*
     * Calculate the hash table index and acquire the spinlock.
     */
    iHash = SUPDRV_SESSION_HASH(Process);

    RTSpinlockAcquire(pDevExt->hSessionHashTabSpinlock);

    /*
     * If an OS session pointer is provided, always use it.
     */
    if (ppOsSessionPtr)
    {
        pCur = *ppOsSessionPtr;
        if (   pCur
            && (   pCur->ppOsSessionPtr != ppOsSessionPtr
                || pCur->Process        != Process
                || pCur->R0Process      != R0Process) )
            pCur = NULL;
    }
    else
    {
        /*
         * Otherwise, do the hash table lookup.
         */
        pCur = pDevExt->apSessionHashTab[iHash];
        while (   pCur
               && (   pCur->Process   != Process
                   || pCur->R0Process != R0Process) )
            pCur = pCur->pCommonNextHash;
    }

    /*
     * Retain the session.
     */
    if (pCur)
    {
        uint32_t cRefs = ASMAtomicIncU32(&pCur->cRefs);
        NOREF(cRefs);
        Assert(cRefs > 1 && cRefs < _1M);
    }

    RTSpinlockRelease(pDevExt->hSessionHashTabSpinlock);

    return pCur;
}


/**
 * Retain a session to make sure it doesn't go away while it is in use.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   pSession    Session data.
 */
uint32_t VBOXCALL supdrvSessionRetain(PSUPDRVSESSION pSession)
{
    uint32_t cRefs;
    AssertPtrReturn(pSession, UINT32_MAX);
    AssertReturn(SUP_IS_SESSION_VALID(pSession), UINT32_MAX);

    cRefs = ASMAtomicIncU32(&pSession->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pSession));
    return cRefs;
}


/**
 * Releases a given session.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   pSession    Session data.
 */
uint32_t VBOXCALL supdrvSessionRelease(PSUPDRVSESSION pSession)
{
    uint32_t cRefs;
    AssertPtrReturn(pSession, UINT32_MAX);
    AssertReturn(SUP_IS_SESSION_VALID(pSession), UINT32_MAX);

    cRefs = ASMAtomicDecU32(&pSession->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pSession));
    if (cRefs == 0)
        supdrvDestroySession(pSession->pDevExt, pSession);
    return cRefs;
}


/**
 * RTHandleTableDestroy callback used by supdrvCleanupSession.
 *
 * @returns IPRT status code, see SUPR0ObjAddRef.
 * @param   hHandleTable    The handle table handle. Ignored.
 * @param   pvObj           The object pointer.
 * @param   pvCtx           Context, the handle type. Ignored.
 * @param   pvUser          Session pointer.
 */
static DECLCALLBACK(int) supdrvSessionObjHandleRetain(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser)
{
    NOREF(pvCtx);
    NOREF(hHandleTable);
    return SUPR0ObjAddRefEx(pvObj, (PSUPDRVSESSION)pvUser, true /*fNoBlocking*/);
}


/**
 * RTHandleTableDestroy callback used by supdrvCleanupSession.
 *
 * @param   hHandleTable    The handle table handle. Ignored.
 * @param   h               The handle value. Ignored.
 * @param   pvObj           The object pointer.
 * @param   pvCtx           Context, the handle type. Ignored.
 * @param   pvUser          Session pointer.
 */
static DECLCALLBACK(void) supdrvSessionObjHandleDelete(RTHANDLETABLE hHandleTable, uint32_t h, void *pvObj, void *pvCtx, void *pvUser)
{
    NOREF(pvCtx);
    NOREF(h);
    NOREF(hHandleTable);
    SUPR0ObjRelease(pvObj, (PSUPDRVSESSION)pvUser);
}


/**
 * Fast path I/O Control worker.
 *
 * @returns VBox status code that should be passed down to ring-3 unchanged.
 * @param   uOperation  SUP_VMMR0_DO_XXX (not the I/O control number!).
 * @param   idCpu       VMCPU id.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 */
int VBOXCALL supdrvIOCtlFast(uintptr_t uOperation, VMCPUID idCpu, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession)
{
    /*
     * Validate input and check that the VM has a session.
     */
    if (RT_LIKELY(RT_VALID_PTR(pSession)))
    {
        PVM  pVM  = pSession->pSessionVM;
        PGVM pGVM = pSession->pSessionGVM;
        if (RT_LIKELY(   pGVM != NULL
                      && pVM  != NULL
                      && pVM  == pSession->pFastIoCtrlVM))
        {
            if (RT_LIKELY(pDevExt->pfnVMMR0EntryFast))
            {
                /*
                 * Make the call.
                 */
                pDevExt->pfnVMMR0EntryFast(pGVM, pVM, idCpu, uOperation);
                return VINF_SUCCESS;
            }

            SUPR0Printf("supdrvIOCtlFast: pfnVMMR0EntryFast is NULL\n");
        }
        else
            SUPR0Printf("supdrvIOCtlFast: Misconfig session: pGVM=%p pVM=%p pFastIoCtrlVM=%p\n",
                        pGVM, pVM, pSession->pFastIoCtrlVM);
    }
    else
        SUPR0Printf("supdrvIOCtlFast: Bad session pointer %p\n", pSession);
    return VERR_INTERNAL_ERROR;
}


/**
 * Helper for supdrvIOCtl used to validate module names passed to SUP_IOCTL_LDR_OPEN.
 *
 * Check if pszStr contains any character of pszChars.  We would use strpbrk
 * here if this function would be contained in the RedHat kABI white list, see
 * http://www.kerneldrivers.org/RHEL5.
 *
 * @returns  true if fine, false if not.
 * @param    pszName        The module name to check.
 */
static bool supdrvIsLdrModuleNameValid(const char *pszName)
{
    int chCur;
    while ((chCur = *pszName++) != '\0')
    {
        static const char s_szInvalidChars[] = ";:()[]{}/\\|&*%#@!~`\"'";
        unsigned offInv = RT_ELEMENTS(s_szInvalidChars);
        while (offInv-- > 0)
            if (s_szInvalidChars[offInv] == chCur)
                return false;
    }
    return true;
}



/**
 * I/O Control inner worker (tracing reasons).
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
static int supdrvIOCtlInnerUnrestricted(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
    /*
     * Validation macros
     */
#define REQ_CHECK_SIZES_EX(Name, cbInExpect, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect) || pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld. cbOut=%ld expected %ld.\n", \
                        (long)pReqHdr->cbIn, (long)(cbInExpect), (long)pReqHdr->cbOut, (long)(cbOutExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZES(Name) REQ_CHECK_SIZES_EX(Name, Name ## _SIZE_IN, Name ## _SIZE_OUT)

#define REQ_CHECK_SIZE_IN(Name, cbInExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbIn != (cbInExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbIn=%ld expected %ld.\n", \
                        (long)pReqHdr->cbIn, (long)(cbInExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_SIZE_OUT(Name, cbOutExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cbOut != (cbOutExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cbOut=%ld expected %ld.\n", \
                        (long)pReqHdr->cbOut, (long)(cbOutExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR(Name, expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT(( #Name ": %s\n", #expr)); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

#define REQ_CHECK_EXPR_FMT(expr, fmt) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            OSDBGPRINT( fmt ); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

    /*
     * The switch.
     */
    switch (SUP_CTL_CODE_NO_SIZE(uIOCtl))
    {
        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_COOKIE):
        {
            PSUPCOOKIE pReq = (PSUPCOOKIE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_COOKIE);
            if (strncmp(pReq->u.In.szMagic, SUPCOOKIE_MAGIC, sizeof(pReq->u.In.szMagic)))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: invalid magic %.16s\n", pReq->u.In.szMagic));
                pReq->Hdr.rc = VERR_INVALID_MAGIC;
                return 0;
            }

#if 0
            /*
             * Call out to the OS specific code and let it do permission checks on the
             * client process.
             */
            if (!supdrvOSValidateClientProcess(pDevExt, pSession))
            {
                pReq->u.Out.u32Cookie         = 0xffffffff;
                pReq->u.Out.u32SessionCookie  = 0xffffffff;
                pReq->u.Out.u32SessionVersion = 0xffffffff;
                pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
                pReq->u.Out.pSession          = NULL;
                pReq->u.Out.cFunctions        = 0;
                pReq->Hdr.rc = VERR_PERMISSION_DENIED;
                return 0;
            }
#endif

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pReq->u.In.u32MinVersion > SUPDRV_IOC_VERSION
                ||  (pReq->u.In.u32MinVersion & 0xffff0000) != (SUPDRV_IOC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n",
                            pReq->u.In.u32ReqVersion, pReq->u.In.u32MinVersion, SUPDRV_IOC_VERSION));
                pReq->u.Out.u32Cookie         = 0xffffffff;
                pReq->u.Out.u32SessionCookie  = 0xffffffff;
                pReq->u.Out.u32SessionVersion = 0xffffffff;
                pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
                pReq->u.Out.pSession          = NULL;
                pReq->u.Out.cFunctions        = 0;
                pReq->Hdr.rc = VERR_VERSION_MISMATCH;
                return 0;
            }

            /*
             * Fill in return data and be gone.
             * N.B. The first one to change SUPDRV_IOC_VERSION shall makes sure that
             *      u32SessionVersion <= u32ReqVersion!
             */
            /** @todo Somehow validate the client and negotiate a secure cookie... */
            pReq->u.Out.u32Cookie         = pDevExt->u32Cookie;
            pReq->u.Out.u32SessionCookie  = pSession->u32Cookie;
            pReq->u.Out.u32SessionVersion = SUPDRV_IOC_VERSION;
            pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
            pReq->u.Out.pSession          = pSession;
            pReq->u.Out.cFunctions        = sizeof(g_aFunctions) / sizeof(g_aFunctions[0]);
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_QUERY_FUNCS(0)):
        {
            /* validate */
            PSUPQUERYFUNCS pReq = (PSUPQUERYFUNCS)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_QUERY_FUNCS, SUP_IOCTL_QUERY_FUNCS_SIZE_IN, SUP_IOCTL_QUERY_FUNCS_SIZE_OUT(RT_ELEMENTS(g_aFunctions)));

            /* execute */
            pReq->u.Out.cFunctions = RT_ELEMENTS(g_aFunctions);
            memcpy(&pReq->u.Out.aFunctions[0], g_aFunctions, sizeof(g_aFunctions));
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_LOCK):
        {
            /* validate */
            PSUPPAGELOCK pReq = (PSUPPAGELOCK)pReqHdr;
            REQ_CHECK_SIZE_IN(SUP_IOCTL_PAGE_LOCK, SUP_IOCTL_PAGE_LOCK_SIZE_IN);
            REQ_CHECK_SIZE_OUT(SUP_IOCTL_PAGE_LOCK, SUP_IOCTL_PAGE_LOCK_SIZE_OUT(pReq->u.In.cPages));
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_LOCK, pReq->u.In.cPages > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_LOCK, pReq->u.In.pvR3 >= PAGE_SIZE);

            /* execute */
            pReq->Hdr.rc = SUPR0LockMem(pSession, pReq->u.In.pvR3, pReq->u.In.cPages, &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_UNLOCK):
        {
            /* validate */
            PSUPPAGEUNLOCK pReq = (PSUPPAGEUNLOCK)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_UNLOCK);

            /* execute */
            pReq->Hdr.rc = SUPR0UnlockMem(pSession, pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CONT_ALLOC):
        {
            /* validate */
            PSUPCONTALLOC pReq = (PSUPCONTALLOC)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_CONT_ALLOC);

            /* execute */
            pReq->Hdr.rc = SUPR0ContAlloc(pSession, pReq->u.In.cPages, &pReq->u.Out.pvR0, &pReq->u.Out.pvR3, &pReq->u.Out.HCPhys);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CONT_FREE):
        {
            /* validate */
            PSUPCONTFREE pReq = (PSUPCONTFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_CONT_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0ContFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_OPEN):
        {
            /* validate */
            PSUPLDROPEN pReq = (PSUPLDROPEN)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_OPEN);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageWithTabs > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageWithTabs < 16*_1M);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits > 0);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.cbImageBits < pReq->u.In.cbImageWithTabs);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, pReq->u.In.szName[0]);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)));
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, supdrvIsLdrModuleNameValid(pReq->u.In.szName));
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_OPEN, RTStrEnd(pReq->u.In.szFilename, sizeof(pReq->u.In.szFilename)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrOpen(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_LOAD):
        {
            /* validate */
            PSUPLDRLOAD pReq = (PSUPLDRLOAD)pReqHdr;
            REQ_CHECK_EXPR(Name, pReq->Hdr.cbIn >= SUP_IOCTL_LDR_LOAD_SIZE_IN(32));
            REQ_CHECK_SIZES_EX(SUP_IOCTL_LDR_LOAD, SUP_IOCTL_LDR_LOAD_SIZE_IN(pReq->u.In.cbImageWithTabs), SUP_IOCTL_LDR_LOAD_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_LOAD, pReq->u.In.cSymbols <= 16384);
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cSymbols
                               ||   (   pReq->u.In.offSymbols < pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.offSymbols + pReq->u.In.cSymbols * sizeof(SUPLDRSYM) <= pReq->u.In.cbImageWithTabs),
                               ("SUP_IOCTL_LDR_LOAD: offSymbols=%#lx cSymbols=%#lx cbImageWithTabs=%#lx\n", (long)pReq->u.In.offSymbols,
                                (long)pReq->u.In.cSymbols, (long)pReq->u.In.cbImageWithTabs));
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.cbStrTab
                               ||   (   pReq->u.In.offStrTab < pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.offStrTab + pReq->u.In.cbStrTab <= pReq->u.In.cbImageWithTabs
                                     && pReq->u.In.cbStrTab <= pReq->u.In.cbImageWithTabs),
                               ("SUP_IOCTL_LDR_LOAD: offStrTab=%#lx cbStrTab=%#lx cbImageWithTabs=%#lx\n", (long)pReq->u.In.offStrTab,
                                (long)pReq->u.In.cbStrTab, (long)pReq->u.In.cbImageWithTabs));

            if (pReq->u.In.cSymbols)
            {
                uint32_t i;
                PSUPLDRSYM paSyms = (PSUPLDRSYM)&pReq->u.In.abImage[pReq->u.In.offSymbols];
                for (i = 0; i < pReq->u.In.cSymbols; i++)
                {
                    REQ_CHECK_EXPR_FMT(paSyms[i].offSymbol < pReq->u.In.cbImageWithTabs,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: symb off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offSymbol, (long)pReq->u.In.cbImageWithTabs));
                    REQ_CHECK_EXPR_FMT(paSyms[i].offName < pReq->u.In.cbStrTab,
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: name off %#lx (max=%#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImageWithTabs));
                    REQ_CHECK_EXPR_FMT(RTStrEnd((char const *)&pReq->u.In.abImage[pReq->u.In.offStrTab + paSyms[i].offName],
                                                pReq->u.In.cbStrTab - paSyms[i].offName),
                                       ("SUP_IOCTL_LDR_LOAD: sym #%ld: unterminated name! (%#lx / %#lx)\n", (long)i, (long)paSyms[i].offName, (long)pReq->u.In.cbImageWithTabs));
                }
            }

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrLoad(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_FREE):
        {
            /* validate */
            PSUPLDRFREE pReq = (PSUPLDRFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_FREE);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrFree(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_LOCK_DOWN):
        {
            /* validate */
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_LOCK_DOWN);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_LdrLockDown(pDevExt);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LDR_GET_SYMBOL):
        {
            /* validate */
            PSUPLDRGETSYMBOL pReq = (PSUPLDRGETSYMBOL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LDR_GET_SYMBOL);
            REQ_CHECK_EXPR(SUP_IOCTL_LDR_GET_SYMBOL, RTStrEnd(pReq->u.In.szSymbol, sizeof(pReq->u.In.szSymbol)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LdrQuerySymbol(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_VMMR0_NO_SIZE()):
        {
            /* validate */
            PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)pReqHdr;
            Log4(("SUP_IOCTL_CALL_VMMR0: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_VMMR0_SIZE(0))
            {
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0, SUP_IOCTL_CALL_VMMR0_SIZE_IN(0), SUP_IOCTL_CALL_VMMR0_SIZE_OUT(0));

                /* execute */
                if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
                {
                    if (pReq->u.In.pVMR0 == NULL)
                        pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(NULL, NULL, pReq->u.In.idCpu,
                                                                pReq->u.In.uOperation, NULL, pReq->u.In.u64Arg, pSession);
                    else if (pReq->u.In.pVMR0 == pSession->pSessionVM)
                        pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pSession->pSessionGVM, pSession->pSessionVM, pReq->u.In.idCpu,
                                                                pReq->u.In.uOperation, NULL, pReq->u.In.u64Arg, pSession);
                    else
                        pReq->Hdr.rc = VERR_INVALID_VM_HANDLE;
                }
                else
                    pReq->Hdr.rc = VERR_WRONG_ORDER;
            }
            else
            {
                PSUPVMMR0REQHDR pVMMReq = (PSUPVMMR0REQHDR)&pReq->abReqPkt[0];
                REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_VMMR0_SIZE(sizeof(SUPVMMR0REQHDR)),
                                   ("SUP_IOCTL_CALL_VMMR0: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_VMMR0_SIZE(sizeof(SUPVMMR0REQHDR))));
                REQ_CHECK_EXPR(SUP_IOCTL_CALL_VMMR0, pVMMReq->u32Magic == SUPVMMR0REQHDR_MAGIC);
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0, SUP_IOCTL_CALL_VMMR0_SIZE_IN(pVMMReq->cbReq), SUP_IOCTL_CALL_VMMR0_SIZE_OUT(pVMMReq->cbReq));

                /* execute */
                if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
                {
                    if (pReq->u.In.pVMR0 == NULL)
                        pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(NULL, NULL, pReq->u.In.idCpu,
                                                                pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
                    else if (pReq->u.In.pVMR0 == pSession->pSessionVM)
                        pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pSession->pSessionGVM, pSession->pSessionVM, pReq->u.In.idCpu,
                                                                pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
                    else
                        pReq->Hdr.rc = VERR_INVALID_VM_HANDLE;
                }
                else
                    pReq->Hdr.rc = VERR_WRONG_ORDER;
            }

            if (    RT_FAILURE(pReq->Hdr.rc)
                &&  pReq->Hdr.rc != VERR_INTERRUPTED
                &&  pReq->Hdr.rc != VERR_TIMEOUT)
                Log(("SUP_IOCTL_CALL_VMMR0: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                     pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            else
                Log4(("SUP_IOCTL_CALL_VMMR0: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                      pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_VMMR0_BIG):
        {
            /* validate */
            PSUPCALLVMMR0 pReq = (PSUPCALLVMMR0)pReqHdr;
            PSUPVMMR0REQHDR pVMMReq;
            Log4(("SUP_IOCTL_CALL_VMMR0_BIG: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            pVMMReq = (PSUPVMMR0REQHDR)&pReq->abReqPkt[0];
            REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_VMMR0_BIG_SIZE(sizeof(SUPVMMR0REQHDR)),
                               ("SUP_IOCTL_CALL_VMMR0_BIG: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_VMMR0_BIG_SIZE(sizeof(SUPVMMR0REQHDR))));
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_VMMR0_BIG, pVMMReq->u32Magic == SUPVMMR0REQHDR_MAGIC);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_VMMR0_BIG, SUP_IOCTL_CALL_VMMR0_BIG_SIZE_IN(pVMMReq->cbReq), SUP_IOCTL_CALL_VMMR0_BIG_SIZE_OUT(pVMMReq->cbReq));

            /* execute */
            if (RT_LIKELY(pDevExt->pfnVMMR0EntryEx))
            {
                if (pReq->u.In.pVMR0 == NULL)
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(NULL, NULL, pReq->u.In.idCpu, pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
                else if (pReq->u.In.pVMR0 == pSession->pSessionVM)
                    pReq->Hdr.rc = pDevExt->pfnVMMR0EntryEx(pSession->pSessionGVM, pSession->pSessionVM, pReq->u.In.idCpu,
                                                            pReq->u.In.uOperation, pVMMReq, pReq->u.In.u64Arg, pSession);
                else
                    pReq->Hdr.rc = VERR_INVALID_VM_HANDLE;
            }
            else
                pReq->Hdr.rc = VERR_WRONG_ORDER;

            if (    RT_FAILURE(pReq->Hdr.rc)
                &&  pReq->Hdr.rc != VERR_INTERRUPTED
                &&  pReq->Hdr.rc != VERR_TIMEOUT)
                Log(("SUP_IOCTL_CALL_VMMR0_BIG: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                     pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            else
                Log4(("SUP_IOCTL_CALL_VMMR0_BIG: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                      pReq->Hdr.rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GET_PAGING_MODE):
        {
            /* validate */
            PSUPGETPAGINGMODE pReq = (PSUPGETPAGINGMODE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GET_PAGING_MODE);

            /* execute */
            pReq->Hdr.rc = VINF_SUCCESS;
            pReq->u.Out.enmMode = SUPR0GetPagingMode();
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOW_ALLOC):
        {
            /* validate */
            PSUPLOWALLOC pReq = (PSUPLOWALLOC)pReqHdr;
            REQ_CHECK_EXPR(SUP_IOCTL_LOW_ALLOC, pReq->Hdr.cbIn <= SUP_IOCTL_LOW_ALLOC_SIZE_IN);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_LOW_ALLOC, SUP_IOCTL_LOW_ALLOC_SIZE_IN, SUP_IOCTL_LOW_ALLOC_SIZE_OUT(pReq->u.In.cPages));

            /* execute */
            pReq->Hdr.rc = SUPR0LowAlloc(pSession, pReq->u.In.cPages, &pReq->u.Out.pvR0, &pReq->u.Out.pvR3, &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOW_FREE):
        {
            /* validate */
            PSUPLOWFREE pReq = (PSUPLOWFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_LOW_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0LowFree(pSession, (RTHCUINTPTR)pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_MAP):
        {
            /* validate */
            PSUPGIPMAP pReq = (PSUPGIPMAP)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_MAP);

            /* execute */
            pReq->Hdr.rc = SUPR0GipMap(pSession, &pReq->u.Out.pGipR3, &pReq->u.Out.HCPhysGip);
            if (RT_SUCCESS(pReq->Hdr.rc))
                pReq->u.Out.pGipR0 = pDevExt->pGip;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_UNMAP):
        {
            /* validate */
            PSUPGIPUNMAP pReq = (PSUPGIPUNMAP)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_UNMAP);

            /* execute */
            pReq->Hdr.rc = SUPR0GipUnmap(pSession);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SET_VM_FOR_FAST):
        {
            /* validate */
            PSUPSETVMFORFAST pReq = (PSUPSETVMFORFAST)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_SET_VM_FOR_FAST);
            REQ_CHECK_EXPR_FMT(     !pReq->u.In.pVMR0
                               ||   (   VALID_PTR(pReq->u.In.pVMR0)
                                     && !((uintptr_t)pReq->u.In.pVMR0 & (PAGE_SIZE - 1))),
                               ("SUP_IOCTL_SET_VM_FOR_FAST: pVMR0=%p!\n", pReq->u.In.pVMR0));

            /* execute */
            RTSpinlockAcquire(pDevExt->Spinlock);
            if (pSession->pSessionVM == pReq->u.In.pVMR0)
            {
                if (pSession->pFastIoCtrlVM == NULL)
                {
                    pSession->pFastIoCtrlVM = pSession->pSessionVM;
                    RTSpinlockRelease(pDevExt->Spinlock);
                    pReq->Hdr.rc = VINF_SUCCESS;
                }
                else
                {
                    RTSpinlockRelease(pDevExt->Spinlock);
                    OSDBGPRINT(("SUP_IOCTL_SET_VM_FOR_FAST: pSession->pFastIoCtrlVM=%p! (pVMR0=%p)\n",
                                pSession->pFastIoCtrlVM, pReq->u.In.pVMR0));
                    pReq->Hdr.rc = VERR_ALREADY_EXISTS;
                }
            }
            else
            {
                RTSpinlockRelease(pDevExt->Spinlock);
                OSDBGPRINT(("SUP_IOCTL_SET_VM_FOR_FAST: pSession->pSessionVM=%p vs pVMR0=%p)\n",
                            pSession->pSessionVM, pReq->u.In.pVMR0));
                pReq->Hdr.rc = pSession->pSessionVM ? VERR_ACCESS_DENIED : VERR_WRONG_ORDER;
            }
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_ALLOC_EX):
        {
            /* validate */
            PSUPPAGEALLOCEX pReq = (PSUPPAGEALLOCEX)pReqHdr;
            REQ_CHECK_EXPR(SUP_IOCTL_PAGE_ALLOC_EX, pReq->Hdr.cbIn <= SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN);
            REQ_CHECK_SIZES_EX(SUP_IOCTL_PAGE_ALLOC_EX, SUP_IOCTL_PAGE_ALLOC_EX_SIZE_IN, SUP_IOCTL_PAGE_ALLOC_EX_SIZE_OUT(pReq->u.In.cPages));
            REQ_CHECK_EXPR_FMT(pReq->u.In.fKernelMapping || pReq->u.In.fUserMapping,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: No mapping requested!\n"));
            REQ_CHECK_EXPR_FMT(pReq->u.In.fUserMapping,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: Must have user mapping!\n"));
            REQ_CHECK_EXPR_FMT(!pReq->u.In.fReserved0 && !pReq->u.In.fReserved1,
                               ("SUP_IOCTL_PAGE_ALLOC_EX: fReserved0=%d fReserved1=%d\n", pReq->u.In.fReserved0, pReq->u.In.fReserved1));

            /* execute */
            pReq->Hdr.rc = SUPR0PageAllocEx(pSession, pReq->u.In.cPages, 0 /* fFlags */,
                                            pReq->u.In.fUserMapping   ? &pReq->u.Out.pvR3 : NULL,
                                            pReq->u.In.fKernelMapping ? &pReq->u.Out.pvR0 : NULL,
                                            &pReq->u.Out.aPages[0]);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_MAP_KERNEL):
        {
            /* validate */
            PSUPPAGEMAPKERNEL pReq = (PSUPPAGEMAPKERNEL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_MAP_KERNEL);
            REQ_CHECK_EXPR_FMT(!pReq->u.In.fFlags, ("SUP_IOCTL_PAGE_MAP_KERNEL: fFlags=%#x! MBZ\n", pReq->u.In.fFlags));
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.offSub & PAGE_OFFSET_MASK), ("SUP_IOCTL_PAGE_MAP_KERNEL: offSub=%#x\n", pReq->u.In.offSub));
            REQ_CHECK_EXPR_FMT(pReq->u.In.cbSub && !(pReq->u.In.cbSub & PAGE_OFFSET_MASK),
                               ("SUP_IOCTL_PAGE_MAP_KERNEL: cbSub=%#x\n", pReq->u.In.cbSub));

            /* execute */
            pReq->Hdr.rc = SUPR0PageMapKernel(pSession, pReq->u.In.pvR3, pReq->u.In.offSub, pReq->u.In.cbSub,
                                              pReq->u.In.fFlags, &pReq->u.Out.pvR0);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_PROTECT):
        {
            /* validate */
            PSUPPAGEPROTECT pReq = (PSUPPAGEPROTECT)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_PROTECT);
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC | RTMEM_PROT_NONE)),
                               ("SUP_IOCTL_PAGE_PROTECT: fProt=%#x!\n", pReq->u.In.fProt));
            REQ_CHECK_EXPR_FMT(!(pReq->u.In.offSub & PAGE_OFFSET_MASK), ("SUP_IOCTL_PAGE_PROTECT: offSub=%#x\n", pReq->u.In.offSub));
            REQ_CHECK_EXPR_FMT(pReq->u.In.cbSub && !(pReq->u.In.cbSub & PAGE_OFFSET_MASK),
                               ("SUP_IOCTL_PAGE_PROTECT: cbSub=%#x\n", pReq->u.In.cbSub));

            /* execute */
            pReq->Hdr.rc = SUPR0PageProtect(pSession, pReq->u.In.pvR3, pReq->u.In.pvR0, pReq->u.In.offSub, pReq->u.In.cbSub, pReq->u.In.fProt);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_PAGE_FREE):
        {
            /* validate */
            PSUPPAGEFREE pReq = (PSUPPAGEFREE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_PAGE_FREE);

            /* execute */
            pReq->Hdr.rc = SUPR0PageFree(pSession, pReq->u.In.pvR3);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_CALL_SERVICE_NO_SIZE()):
        {
            /* validate */
            PSUPCALLSERVICE pReq = (PSUPCALLSERVICE)pReqHdr;
            Log4(("SUP_IOCTL_CALL_SERVICE: op=%u in=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
                  pReq->u.In.uOperation, pReq->Hdr.cbIn, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));

            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_SERVICE_SIZE(0))
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_SERVICE, SUP_IOCTL_CALL_SERVICE_SIZE_IN(0), SUP_IOCTL_CALL_SERVICE_SIZE_OUT(0));
            else
            {
                PSUPR0SERVICEREQHDR pSrvReq = (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0];
                REQ_CHECK_EXPR_FMT(pReq->Hdr.cbIn >= SUP_IOCTL_CALL_SERVICE_SIZE(sizeof(SUPR0SERVICEREQHDR)),
                                   ("SUP_IOCTL_CALL_SERVICE: cbIn=%#x < %#lx\n", pReq->Hdr.cbIn, SUP_IOCTL_CALL_SERVICE_SIZE(sizeof(SUPR0SERVICEREQHDR))));
                REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, pSrvReq->u32Magic == SUPR0SERVICEREQHDR_MAGIC);
                REQ_CHECK_SIZES_EX(SUP_IOCTL_CALL_SERVICE, SUP_IOCTL_CALL_SERVICE_SIZE_IN(pSrvReq->cbReq), SUP_IOCTL_CALL_SERVICE_SIZE_OUT(pSrvReq->cbReq));
            }
            REQ_CHECK_EXPR(SUP_IOCTL_CALL_SERVICE, RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)));

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_CallServiceModule(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_LOGGER_SETTINGS_NO_SIZE()):
        {
            /* validate */
            PSUPLOGGERSETTINGS pReq = (PSUPLOGGERSETTINGS)pReqHdr;
            size_t cbStrTab;
            REQ_CHECK_SIZE_OUT(SUP_IOCTL_LOGGER_SETTINGS, SUP_IOCTL_LOGGER_SETTINGS_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->Hdr.cbIn >= SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(1));
            cbStrTab = pReq->Hdr.cbIn - SUP_IOCTL_LOGGER_SETTINGS_SIZE_IN(0);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offGroups      < cbStrTab);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offFlags       < cbStrTab);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.offDestination < cbStrTab);
            REQ_CHECK_EXPR_FMT(pReq->u.In.szStrings[cbStrTab - 1] == '\0',
                               ("SUP_IOCTL_LOGGER_SETTINGS: cbIn=%#x cbStrTab=%#zx LastChar=%d\n",
                                pReq->Hdr.cbIn, cbStrTab, pReq->u.In.szStrings[cbStrTab - 1]));
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.fWhich <= SUPLOGGERSETTINGS_WHICH_RELEASE);
            REQ_CHECK_EXPR(SUP_IOCTL_LOGGER_SETTINGS, pReq->u.In.fWhat  <= SUPLOGGERSETTINGS_WHAT_DESTROY);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_LoggerSettings(pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SEM_OP2):
        {
            /* validate */
            PSUPSEMOP2 pReq = (PSUPSEMOP2)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_SEM_OP2, SUP_IOCTL_SEM_OP2_SIZE_IN, SUP_IOCTL_SEM_OP2_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP2, pReq->u.In.uReserved == 0);

            /* execute */
            switch (pReq->u.In.uType)
            {
                case SUP_SEM_TYPE_EVENT:
                {
                    SUPSEMEVENT hEvent = (SUPSEMEVENT)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP2_WAIT_MS_REL:
                            pReq->Hdr.rc = SUPSemEventWaitNoResume(pSession, hEvent, pReq->u.In.uArg.cRelMsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_ABS:
                            pReq->Hdr.rc = SUPSemEventWaitNsAbsIntr(pSession, hEvent, pReq->u.In.uArg.uAbsNsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_REL:
                            pReq->Hdr.rc = SUPSemEventWaitNsRelIntr(pSession, hEvent, pReq->u.In.uArg.cRelNsTimeout);
                            break;
                        case SUPSEMOP2_SIGNAL:
                            pReq->Hdr.rc = SUPSemEventSignal(pSession, hEvent);
                            break;
                        case SUPSEMOP2_CLOSE:
                            pReq->Hdr.rc = SUPSemEventClose(pSession, hEvent);
                            break;
                        case SUPSEMOP2_RESET:
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                case SUP_SEM_TYPE_EVENT_MULTI:
                {
                    SUPSEMEVENTMULTI hEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP2_WAIT_MS_REL:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNoResume(pSession, hEventMulti, pReq->u.In.uArg.cRelMsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_ABS:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNsAbsIntr(pSession, hEventMulti, pReq->u.In.uArg.uAbsNsTimeout);
                            break;
                        case SUPSEMOP2_WAIT_NS_REL:
                            pReq->Hdr.rc = SUPSemEventMultiWaitNsRelIntr(pSession, hEventMulti, pReq->u.In.uArg.cRelNsTimeout);
                            break;
                        case SUPSEMOP2_SIGNAL:
                            pReq->Hdr.rc = SUPSemEventMultiSignal(pSession, hEventMulti);
                            break;
                        case SUPSEMOP2_CLOSE:
                            pReq->Hdr.rc = SUPSemEventMultiClose(pSession, hEventMulti);
                            break;
                        case SUPSEMOP2_RESET:
                            pReq->Hdr.rc = SUPSemEventMultiReset(pSession, hEventMulti);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                default:
                    pReq->Hdr.rc = VERR_INVALID_PARAMETER;
                    break;
            }
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_SEM_OP3):
        {
            /* validate */
            PSUPSEMOP3 pReq = (PSUPSEMOP3)pReqHdr;
            REQ_CHECK_SIZES_EX(SUP_IOCTL_SEM_OP3, SUP_IOCTL_SEM_OP3_SIZE_IN, SUP_IOCTL_SEM_OP3_SIZE_OUT);
            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, pReq->u.In.u32Reserved == 0 && pReq->u.In.u64Reserved == 0);

            /* execute */
            switch (pReq->u.In.uType)
            {
                case SUP_SEM_TYPE_EVENT:
                {
                    SUPSEMEVENT hEvent = (SUPSEMEVENT)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP3_CREATE:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEvent == NIL_SUPSEMEVENT);
                            pReq->Hdr.rc = SUPSemEventCreate(pSession, &hEvent);
                            pReq->u.Out.hSem = (uint32_t)(uintptr_t)hEvent;
                            break;
                        case SUPSEMOP3_GET_RESOLUTION:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEvent == NIL_SUPSEMEVENT);
                            pReq->Hdr.rc = VINF_SUCCESS;
                            pReq->Hdr.cbOut = sizeof(*pReq);
                            pReq->u.Out.cNsResolution = SUPSemEventGetResolution(pSession);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                case SUP_SEM_TYPE_EVENT_MULTI:
                {
                    SUPSEMEVENTMULTI hEventMulti = (SUPSEMEVENTMULTI)(uintptr_t)pReq->u.In.hSem;
                    switch (pReq->u.In.uOp)
                    {
                        case SUPSEMOP3_CREATE:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEventMulti == NIL_SUPSEMEVENTMULTI);
                            pReq->Hdr.rc = SUPSemEventMultiCreate(pSession, &hEventMulti);
                            pReq->u.Out.hSem = (uint32_t)(uintptr_t)hEventMulti;
                            break;
                        case SUPSEMOP3_GET_RESOLUTION:
                            REQ_CHECK_EXPR(SUP_IOCTL_SEM_OP3, hEventMulti == NIL_SUPSEMEVENTMULTI);
                            pReq->Hdr.rc = VINF_SUCCESS;
                            pReq->u.Out.cNsResolution = SUPSemEventMultiGetResolution(pSession);
                            break;
                        default:
                            pReq->Hdr.rc = VERR_INVALID_FUNCTION;
                            break;
                    }
                    break;
                }

                default:
                    pReq->Hdr.rc = VERR_INVALID_PARAMETER;
                    break;
            }
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_VT_CAPS):
        {
            /* validate */
            PSUPVTCAPS pReq = (PSUPVTCAPS)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_VT_CAPS);

            /* execute */
            pReq->Hdr.rc = SUPR0QueryVTCaps(pSession, &pReq->u.Out.fCaps);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_OPEN):
        {
            /* validate */
            PSUPTRACEROPEN pReq = (PSUPTRACEROPEN)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_OPEN);

            /* execute */
            pReq->Hdr.rc = supdrvIOCtl_TracerOpen(pDevExt, pSession, pReq->u.In.uCookie, pReq->u.In.uArg);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_CLOSE):
        {
            /* validate */
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_CLOSE);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerClose(pDevExt, pSession);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_IOCTL):
        {
            /* validate */
            PSUPTRACERIOCTL pReq = (PSUPTRACERIOCTL)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_IOCTL);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerIOCtl(pDevExt, pSession, pReq->u.In.uCmd, pReq->u.In.uArg, &pReq->u.Out.iRetVal);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_REG):
        {
            /* validate */
            PSUPTRACERUMODREG pReq = (PSUPTRACERUMODREG)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_REG);
            if (!RTStrEnd(pReq->u.In.szName, sizeof(pReq->u.In.szName)))
                return VERR_INVALID_PARAMETER;

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerUmodRegister(pDevExt, pSession,
                                                         pReq->u.In.R3PtrVtgHdr, pReq->u.In.uVtgHdrAddr,
                                                         pReq->u.In.R3PtrStrTab, pReq->u.In.cbStrTab,
                                                         pReq->u.In.szName, pReq->u.In.fFlags);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_DEREG):
        {
            /* validate */
            PSUPTRACERUMODDEREG pReq = (PSUPTRACERUMODDEREG)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_DEREG);

            /* execute */
            pReqHdr->rc = supdrvIOCtl_TracerUmodDeregister(pDevExt, pSession, pReq->u.In.pVtgHdr);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TRACER_UMOD_FIRE_PROBE):
        {
            /* validate */
            PSUPTRACERUMODFIREPROBE pReq = (PSUPTRACERUMODFIREPROBE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TRACER_UMOD_FIRE_PROBE);

            supdrvIOCtl_TracerUmodProbeFire(pDevExt, pSession, &pReq->u.In);
            pReqHdr->rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_MSR_PROBER):
        {
            /* validate */
            PSUPMSRPROBER pReq = (PSUPMSRPROBER)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_MSR_PROBER);
            REQ_CHECK_EXPR(SUP_IOCTL_MSR_PROBER,
                           pReq->u.In.enmOp > SUPMSRPROBEROP_INVALID && pReq->u.In.enmOp < SUPMSRPROBEROP_END);

            pReqHdr->rc = supdrvIOCtl_MsrProber(pDevExt, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_RESUME_SUSPENDED_KBDS):
        {
            /* validate */
            REQ_CHECK_SIZES(SUP_IOCTL_RESUME_SUSPENDED_KBDS);

            pReqHdr->rc = supdrvIOCtl_ResumeSuspendedKbds();
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TSC_DELTA_MEASURE):
        {
            /* validate */
            PSUPTSCDELTAMEASURE pReq = (PSUPTSCDELTAMEASURE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TSC_DELTA_MEASURE);

            pReqHdr->rc = supdrvIOCtl_TscDeltaMeasure(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_TSC_READ):
        {
            /* validate */
            PSUPTSCREAD pReq = (PSUPTSCREAD)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_TSC_READ);

            pReqHdr->rc = supdrvIOCtl_TscRead(pDevExt, pSession, pReq);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_GIP_SET_FLAGS):
        {
            /* validate */
            PSUPGIPSETFLAGS pReq = (PSUPGIPSETFLAGS)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_GIP_SET_FLAGS);

            pReqHdr->rc = supdrvIOCtl_GipSetFlags(pDevExt, pSession, pReq->u.In.fOrMask, pReq->u.In.fAndMask);
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_UCODE_REV):
        {
            /* validate */
            PSUPUCODEREV pReq = (PSUPUCODEREV)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_UCODE_REV);

            /* execute */
            pReq->Hdr.rc = SUPR0QueryUcodeRev(pSession, &pReq->u.Out.MicrocodeRev);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    return VERR_GENERAL_FAILURE;
}


/**
 * I/O Control inner worker for the restricted operations.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
static int supdrvIOCtlInnerRestricted(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr)
{
    /*
     * The switch.
     */
    switch (SUP_CTL_CODE_NO_SIZE(uIOCtl))
    {
        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_COOKIE):
        {
            PSUPCOOKIE pReq = (PSUPCOOKIE)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_COOKIE);
            if (strncmp(pReq->u.In.szMagic, SUPCOOKIE_MAGIC, sizeof(pReq->u.In.szMagic)))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: invalid magic %.16s\n", pReq->u.In.szMagic));
                pReq->Hdr.rc = VERR_INVALID_MAGIC;
                return 0;
            }

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pReq->u.In.u32MinVersion > SUPDRV_IOC_VERSION
                ||  (pReq->u.In.u32MinVersion & 0xffff0000) != (SUPDRV_IOC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUP_IOCTL_COOKIE: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n",
                            pReq->u.In.u32ReqVersion, pReq->u.In.u32MinVersion, SUPDRV_IOC_VERSION));
                pReq->u.Out.u32Cookie         = 0xffffffff;
                pReq->u.Out.u32SessionCookie  = 0xffffffff;
                pReq->u.Out.u32SessionVersion = 0xffffffff;
                pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
                pReq->u.Out.pSession          = NULL;
                pReq->u.Out.cFunctions        = 0;
                pReq->Hdr.rc = VERR_VERSION_MISMATCH;
                return 0;
            }

            /*
             * Fill in return data and be gone.
             * N.B. The first one to change SUPDRV_IOC_VERSION shall makes sure that
             *      u32SessionVersion <= u32ReqVersion!
             */
            /** @todo Somehow validate the client and negotiate a secure cookie... */
            pReq->u.Out.u32Cookie         = pDevExt->u32Cookie;
            pReq->u.Out.u32SessionCookie  = pSession->u32Cookie;
            pReq->u.Out.u32SessionVersion = SUPDRV_IOC_VERSION;
            pReq->u.Out.u32DriverVersion  = SUPDRV_IOC_VERSION;
            pReq->u.Out.pSession          = pSession;
            pReq->u.Out.cFunctions        = 0;
            pReq->Hdr.rc = VINF_SUCCESS;
            return 0;
        }

        case SUP_CTL_CODE_NO_SIZE(SUP_IOCTL_VT_CAPS):
        {
            /* validate */
            PSUPVTCAPS pReq = (PSUPVTCAPS)pReqHdr;
            REQ_CHECK_SIZES(SUP_IOCTL_VT_CAPS);

            /* execute */
            pReq->Hdr.rc = SUPR0QueryVTCaps(pSession, &pReq->u.Out.fCaps);
            if (RT_FAILURE(pReq->Hdr.rc))
                pReq->Hdr.cbOut = sizeof(pReq->Hdr);
            return 0;
        }

        default:
            Log(("Unknown IOCTL %#lx\n", (long)uIOCtl));
            break;
    }
    return VERR_GENERAL_FAILURE;
}


/**
 * I/O Control worker.
 *
 * @returns IPRT status code.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 *
 * @param   uIOCtl      Function number.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 * @param   cbReq       The size of the request buffer.
 */
int VBOXCALL supdrvIOCtl(uintptr_t uIOCtl, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPREQHDR pReqHdr, size_t cbReq)
{
    int rc;
    VBOXDRV_IOCTL_ENTRY(pSession, uIOCtl, pReqHdr);

    /*
     * Validate the request.
     */
    if (RT_UNLIKELY(cbReq < sizeof(*pReqHdr)))
    {
        OSDBGPRINT(("vboxdrv: Bad ioctl request size; cbReq=%#lx\n", (long)cbReq));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(   (pReqHdr->fFlags & SUPREQHDR_FLAGS_MAGIC_MASK) != SUPREQHDR_FLAGS_MAGIC
                    || pReqHdr->cbIn < sizeof(*pReqHdr)
                    || pReqHdr->cbIn > cbReq
                    || pReqHdr->cbOut < sizeof(*pReqHdr)
                    || pReqHdr->cbOut > cbReq))
    {
        OSDBGPRINT(("vboxdrv: Bad ioctl request header; cbIn=%#lx cbOut=%#lx fFlags=%#lx\n",
                    (long)pReqHdr->cbIn, (long)pReqHdr->cbOut, (long)pReqHdr->fFlags));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(!RT_VALID_PTR(pSession)))
    {
        OSDBGPRINT(("vboxdrv: Invalid pSession value %p (ioctl=%p)\n", pSession, (void *)uIOCtl));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }
    if (RT_UNLIKELY(uIOCtl == SUP_IOCTL_COOKIE))
    {
        if (pReqHdr->u32Cookie != SUPCOOKIE_INITIAL_COOKIE)
        {
            OSDBGPRINT(("SUP_IOCTL_COOKIE: bad cookie %#lx\n", (long)pReqHdr->u32Cookie));
            VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
            return VERR_INVALID_PARAMETER;
        }
    }
    else if (RT_UNLIKELY(    pReqHdr->u32Cookie != pDevExt->u32Cookie
                         ||  pReqHdr->u32SessionCookie != pSession->u32Cookie))
    {
        OSDBGPRINT(("vboxdrv: bad cookie %#lx / %#lx.\n", (long)pReqHdr->u32Cookie, (long)pReqHdr->u32SessionCookie));
        VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, VERR_INVALID_PARAMETER, VINF_SUCCESS);
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Hand it to an inner function to avoid lots of unnecessary return tracepoints.
     */
    if (pSession->fUnrestricted)
        rc = supdrvIOCtlInnerUnrestricted(uIOCtl, pDevExt, pSession, pReqHdr);
    else
        rc = supdrvIOCtlInnerRestricted(uIOCtl, pDevExt, pSession, pReqHdr);

    VBOXDRV_IOCTL_RETURN(pSession, uIOCtl, pReqHdr, pReqHdr->rc, rc);
    return rc;
}


/**
 * Inter-Driver Communication (IDC) worker.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_PARAMETER if the request is invalid.
 * @retval  VERR_NOT_SUPPORTED if the request isn't supported.
 *
 * @param   uReq        The request (function) code.
 * @param   pDevExt     Device extention.
 * @param   pSession    Session data.
 * @param   pReqHdr     The request header.
 */
int VBOXCALL supdrvIDC(uintptr_t uReq, PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQHDR pReqHdr)
{
    /*
     * The OS specific code has already validated the pSession
     * pointer, and the request size being greater or equal to
     * size of the header.
     *
     * So, just check that pSession is a kernel context session.
     */
    if (RT_UNLIKELY(    pSession
                    &&  pSession->R0Process != NIL_RTR0PROCESS))
        return VERR_INVALID_PARAMETER;

/*
 * Validation macro.
 */
#define REQ_CHECK_IDC_SIZE(Name, cbExpect) \
    do { \
        if (RT_UNLIKELY(pReqHdr->cb != (cbExpect))) \
        { \
            OSDBGPRINT(( #Name ": Invalid input/output sizes. cb=%ld expected %ld.\n", \
                        (long)pReqHdr->cb, (long)(cbExpect))); \
            return pReqHdr->rc = VERR_INVALID_PARAMETER; \
        } \
    } while (0)

    switch (uReq)
    {
        case SUPDRV_IDC_REQ_CONNECT:
        {
            PSUPDRVIDCREQCONNECT pReq = (PSUPDRVIDCREQCONNECT)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_CONNECT, sizeof(*pReq));

            /*
             * Validate the cookie and other input.
             */
            if (pReq->Hdr.pSession != NULL)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: Hdr.pSession=%p expected NULL!\n", pReq->Hdr.pSession));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }
            if (pReq->u.In.u32MagicCookie != SUPDRVIDCREQ_CONNECT_MAGIC_COOKIE)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: u32MagicCookie=%#x expected %#x!\n",
                            (unsigned)pReq->u.In.u32MagicCookie, (unsigned)SUPDRVIDCREQ_CONNECT_MAGIC_COOKIE));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }
            if (    pReq->u.In.uMinVersion > pReq->u.In.uReqVersion
                ||  (pReq->u.In.uMinVersion & UINT32_C(0xffff0000)) != (pReq->u.In.uReqVersion & UINT32_C(0xffff0000)))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: uMinVersion=%#x uMaxVersion=%#x doesn't match!\n",
                            pReq->u.In.uMinVersion, pReq->u.In.uReqVersion));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }
            if (pSession != NULL)
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: pSession=%p expected NULL!\n", pSession));
                return pReqHdr->rc = VERR_INVALID_PARAMETER;
            }

            /*
             * Match the version.
             * The current logic is very simple, match the major interface version.
             */
            if (    pReq->u.In.uMinVersion > SUPDRV_IDC_VERSION
                ||  (pReq->u.In.uMinVersion & 0xffff0000) != (SUPDRV_IDC_VERSION & 0xffff0000))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: Version mismatch. Requested: %#x  Min: %#x  Current: %#x\n",
                            pReq->u.In.uReqVersion, pReq->u.In.uMinVersion, (unsigned)SUPDRV_IDC_VERSION));
                pReq->u.Out.pSession        = NULL;
                pReq->u.Out.uSessionVersion = 0xffffffff;
                pReq->u.Out.uDriverVersion  = SUPDRV_IDC_VERSION;
                pReq->u.Out.uDriverRevision = VBOX_SVN_REV;
                pReq->Hdr.rc = VERR_VERSION_MISMATCH;
                return VINF_SUCCESS;
            }

            pReq->u.Out.pSession        = NULL;
            pReq->u.Out.uSessionVersion = SUPDRV_IDC_VERSION;
            pReq->u.Out.uDriverVersion  = SUPDRV_IDC_VERSION;
            pReq->u.Out.uDriverRevision = VBOX_SVN_REV;

            pReq->Hdr.rc = supdrvCreateSession(pDevExt, false /* fUser */, true /*fUnrestricted*/, &pSession);
            if (RT_FAILURE(pReq->Hdr.rc))
            {
                OSDBGPRINT(("SUPDRV_IDC_REQ_CONNECT: failed to create session, rc=%d\n", pReq->Hdr.rc));
                return VINF_SUCCESS;
            }

            pReq->u.Out.pSession = pSession;
            pReq->Hdr.pSession = pSession;

            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_DISCONNECT:
        {
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_DISCONNECT, sizeof(*pReqHdr));

            supdrvSessionRelease(pSession);
            return pReqHdr->rc = VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_GET_SYMBOL:
        {
            PSUPDRVIDCREQGETSYM pReq = (PSUPDRVIDCREQGETSYM)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_GET_SYMBOL, sizeof(*pReq));

            pReq->Hdr.rc = supdrvIDC_LdrGetSymbol(pDevExt, pSession, pReq);
            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_COMPONENT_REGISTER_FACTORY:
        {
            PSUPDRVIDCREQCOMPREGFACTORY pReq = (PSUPDRVIDCREQCOMPREGFACTORY)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_COMPONENT_REGISTER_FACTORY, sizeof(*pReq));

            pReq->Hdr.rc = SUPR0ComponentRegisterFactory(pSession, pReq->u.In.pFactory);
            return VINF_SUCCESS;
        }

        case SUPDRV_IDC_REQ_COMPONENT_DEREGISTER_FACTORY:
        {
            PSUPDRVIDCREQCOMPDEREGFACTORY pReq = (PSUPDRVIDCREQCOMPDEREGFACTORY)pReqHdr;
            REQ_CHECK_IDC_SIZE(SUPDRV_IDC_REQ_COMPONENT_DEREGISTER_FACTORY, sizeof(*pReq));

            pReq->Hdr.rc = SUPR0ComponentDeregisterFactory(pSession, pReq->u.In.pFactory);
            return VINF_SUCCESS;
        }

        default:
            Log(("Unknown IDC %#lx\n", (long)uReq));
            break;
    }

#undef REQ_CHECK_IDC_SIZE
    return VERR_NOT_SUPPORTED;
}


/**
 * Register a object for reference counting.
 * The object is registered with one reference in the specified session.
 *
 * @returns Unique identifier on success (pointer).
 *          All future reference must use this identifier.
 * @returns NULL on failure.
 * @param   pSession        The caller's session.
 * @param   enmType         The object type.
 * @param   pfnDestructor   The destructore function which will be called when the reference count reaches 0.
 * @param   pvUser1         The first user argument.
 * @param   pvUser2         The second user argument.
 */
SUPR0DECL(void *) SUPR0ObjRegister(PSUPDRVSESSION pSession, SUPDRVOBJTYPE enmType, PFNSUPDRVDESTRUCTOR pfnDestructor, void *pvUser1, void *pvUser2)
{
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), NULL);
    AssertReturn(enmType > SUPDRVOBJTYPE_INVALID && enmType < SUPDRVOBJTYPE_END, NULL);
    AssertPtrReturn(pfnDestructor, NULL);

    /*
     * Allocate and initialize the object.
     */
    pObj = (PSUPDRVOBJ)RTMemAlloc(sizeof(*pObj));
    if (!pObj)
        return NULL;
    pObj->u32Magic      = SUPDRVOBJ_MAGIC;
    pObj->enmType       = enmType;
    pObj->pNext         = NULL;
    pObj->cUsage        = 1;
    pObj->pfnDestructor = pfnDestructor;
    pObj->pvUser1       = pvUser1;
    pObj->pvUser2       = pvUser2;
    pObj->CreatorUid    = pSession->Uid;
    pObj->CreatorGid    = pSession->Gid;
    pObj->CreatorProcess= pSession->Process;
    supdrvOSObjInitCreator(pObj, pSession);

    /*
     * Allocate the usage record.
     * (We keep freed usage records around to simplify SUPR0ObjAddRefEx().)
     */
    RTSpinlockAcquire(pDevExt->Spinlock);

    pUsage = pDevExt->pUsageFree;
    if (pUsage)
        pDevExt->pUsageFree = pUsage->pNext;
    else
    {
        RTSpinlockRelease(pDevExt->Spinlock);
        pUsage = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsage));
        if (!pUsage)
        {
            RTMemFree(pObj);
            return NULL;
        }
        RTSpinlockAcquire(pDevExt->Spinlock);
    }

    /*
     * Insert the object and create the session usage record.
     */
    /* The object. */
    pObj->pNext         = pDevExt->pObjs;
    pDevExt->pObjs      = pObj;

    /* The session record. */
    pUsage->cUsage      = 1;
    pUsage->pObj        = pObj;
    pUsage->pNext       = pSession->pUsage;
    /* Log2(("SUPR0ObjRegister: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext)); */
    pSession->pUsage    = pUsage;

    RTSpinlockRelease(pDevExt->Spinlock);

    Log(("SUPR0ObjRegister: returns %p (pvUser1=%p, pvUser=%p)\n", pObj, pvUser1, pvUser2));
    return pObj;
}


/**
 * Increment the reference counter for the object associating the reference
 * with the specified session.
 *
 * @returns IPRT status code.
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 *
 * @remarks The caller should not own any spinlocks and must carefully protect
 *          itself against potential race with the destructor so freed memory
 *          isn't accessed here.
 */
SUPR0DECL(int) SUPR0ObjAddRef(void *pvObj, PSUPDRVSESSION pSession)
{
    return SUPR0ObjAddRefEx(pvObj, pSession, false /* fNoBlocking */);
}


/**
 * Increment the reference counter for the object associating the reference
 * with the specified session.
 *
 * @returns IPRT status code.
 * @retval  VERR_TRY_AGAIN if fNoBlocking was set and a new usage record
 *          couldn't be allocated. (If you see this you're not doing the right
 *          thing and it won't ever work reliably.)
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 * @param   fNoBlocking     Set if it's not OK to block. Never try to make the
 *                          first reference to an object in a session with this
 *                          argument set.
 *
 * @remarks The caller should not own any spinlocks and must carefully protect
 *          itself against potential race with the destructor so freed memory
 *          isn't accessed here.
 */
SUPR0DECL(int) SUPR0ObjAddRefEx(void *pvObj, PSUPDRVSESSION pSession, bool fNoBlocking)
{
    PSUPDRVDEVEXT   pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ      pObj        = (PSUPDRVOBJ)pvObj;
    int             rc          = VINF_SUCCESS;
    PSUPDRVUSAGE    pUsagePre;
    PSUPDRVUSAGE    pUsage;

    /*
     * Validate the input.
     * Be ready for the destruction race (someone might be stuck in the
     * destructor waiting a lock we own).
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pObj, VERR_INVALID_POINTER);
    AssertMsgReturn(pObj->u32Magic == SUPDRVOBJ_MAGIC || pObj->u32Magic == SUPDRVOBJ_MAGIC_DEAD,
                    ("Invalid pvObj=%p magic=%#x (expected %#x or %#x)\n", pvObj, pObj->u32Magic, SUPDRVOBJ_MAGIC, SUPDRVOBJ_MAGIC_DEAD),
                    VERR_INVALID_PARAMETER);

    RTSpinlockAcquire(pDevExt->Spinlock);

    if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
    {
        RTSpinlockRelease(pDevExt->Spinlock);

        AssertMsgFailed(("pvObj=%p magic=%#x\n", pvObj, pObj->u32Magic));
        return VERR_WRONG_ORDER;
    }

    /*
     * Preallocate the usage record if we can.
     */
    pUsagePre = pDevExt->pUsageFree;
    if (pUsagePre)
        pDevExt->pUsageFree = pUsagePre->pNext;
    else if (!fNoBlocking)
    {
        RTSpinlockRelease(pDevExt->Spinlock);
        pUsagePre = (PSUPDRVUSAGE)RTMemAlloc(sizeof(*pUsagePre));
        if (!pUsagePre)
            return VERR_NO_MEMORY;

        RTSpinlockAcquire(pDevExt->Spinlock);
        if (RT_UNLIKELY(pObj->u32Magic != SUPDRVOBJ_MAGIC))
        {
            RTSpinlockRelease(pDevExt->Spinlock);

            AssertMsgFailed(("pvObj=%p magic=%#x\n", pvObj, pObj->u32Magic));
            return VERR_WRONG_ORDER;
        }
    }

    /*
     * Reference the object.
     */
    pObj->cUsage++;

    /*
     * Look for the session record.
     */
    for (pUsage = pSession->pUsage; pUsage; pUsage = pUsage->pNext)
    {
        /*Log(("SUPR0AddRef: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));*/
        if (pUsage->pObj == pObj)
            break;
    }
    if (pUsage)
        pUsage->cUsage++;
    else if (pUsagePre)
    {
        /* create a new session record. */
        pUsagePre->cUsage   = 1;
        pUsagePre->pObj     = pObj;
        pUsagePre->pNext    = pSession->pUsage;
        pSession->pUsage    = pUsagePre;
        /*Log(("SUPR0AddRef: pUsagePre=%p:{.pObj=%p, .pNext=%p}\n", pUsagePre, pUsagePre->pObj, pUsagePre->pNext));*/

        pUsagePre = NULL;
    }
    else
    {
        pObj->cUsage--;
        rc = VERR_TRY_AGAIN;
    }

    /*
     * Put any unused usage record into the free list..
     */
    if (pUsagePre)
    {
        pUsagePre->pNext = pDevExt->pUsageFree;
        pDevExt->pUsageFree = pUsagePre;
    }

    RTSpinlockRelease(pDevExt->Spinlock);

    return rc;
}


/**
 * Decrement / destroy a reference counter record for an object.
 *
 * The object is uniquely identified by pfnDestructor+pvUser1+pvUser2.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if not destroyed.
 * @retval  VINF_OBJECT_DESTROYED if it's destroyed by this release call.
 * @retval  VERR_INVALID_PARAMETER if the object isn't valid. Will assert in
 *          string builds.
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which is referencing the object.
 */
SUPR0DECL(int) SUPR0ObjRelease(void *pvObj, PSUPDRVSESSION pSession)
{
    PSUPDRVDEVEXT       pDevExt     = pSession->pDevExt;
    PSUPDRVOBJ          pObj        = (PSUPDRVOBJ)pvObj;
    int                 rc          = VERR_INVALID_PARAMETER;
    PSUPDRVUSAGE        pUsage;
    PSUPDRVUSAGE        pUsagePrev;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pObj)&& pObj->u32Magic == SUPDRVOBJ_MAGIC,
                    ("Invalid pvObj=%p magic=%#x (expected %#x)\n", pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC),
                    VERR_INVALID_PARAMETER);

    /*
     * Acquire the spinlock and look for the usage record.
     */
    RTSpinlockAcquire(pDevExt->Spinlock);

    for (pUsagePrev = NULL, pUsage = pSession->pUsage;
         pUsage;
         pUsagePrev = pUsage, pUsage = pUsage->pNext)
    {
        /*Log2(("SUPR0ObjRelease: pUsage=%p:{.pObj=%p, .pNext=%p}\n", pUsage, pUsage->pObj, pUsage->pNext));*/
        if (pUsage->pObj == pObj)
        {
            rc = VINF_SUCCESS;
            AssertMsg(pUsage->cUsage >= 1 && pObj->cUsage >= pUsage->cUsage, ("glob %d; sess %d\n", pObj->cUsage, pUsage->cUsage));
            if (pUsage->cUsage > 1)
            {
                pObj->cUsage--;
                pUsage->cUsage--;
            }
            else
            {
                /*
                 * Free the session record.
                 */
                if (pUsagePrev)
                    pUsagePrev->pNext = pUsage->pNext;
                else
                    pSession->pUsage = pUsage->pNext;
                pUsage->pNext = pDevExt->pUsageFree;
                pDevExt->pUsageFree = pUsage;

                /* What about the object? */
                if (pObj->cUsage > 1)
                    pObj->cUsage--;
                else
                {
                    /*
                     * Object is to be destroyed, unlink it.
                     */
                    pObj->u32Magic = SUPDRVOBJ_MAGIC_DEAD;
                    rc = VINF_OBJECT_DESTROYED;
                    if (pDevExt->pObjs == pObj)
                        pDevExt->pObjs = pObj->pNext;
                    else
                    {
                        PSUPDRVOBJ pObjPrev;
                        for (pObjPrev = pDevExt->pObjs; pObjPrev; pObjPrev = pObjPrev->pNext)
                            if (pObjPrev->pNext == pObj)
                            {
                                pObjPrev->pNext = pObj->pNext;
                                break;
                            }
                        Assert(pObjPrev);
                    }
                }
            }
            break;
        }
    }

    RTSpinlockRelease(pDevExt->Spinlock);

    /*
     * Call the destructor and free the object if required.
     */
    if (rc == VINF_OBJECT_DESTROYED)
    {
        Log(("SUPR0ObjRelease: destroying %p/%d (%p/%p) cpid=%RTproc pid=%RTproc dtor=%p\n",
             pObj, pObj->enmType, pObj->pvUser1, pObj->pvUser2, pObj->CreatorProcess, RTProcSelf(), pObj->pfnDestructor));
        if (pObj->pfnDestructor)
            pObj->pfnDestructor(pObj, pObj->pvUser1, pObj->pvUser2);
        RTMemFree(pObj);
    }

    AssertMsg(pUsage, ("pvObj=%p\n", pvObj));
    return rc;
}


/**
 * Verifies that the current process can access the specified object.
 *
 * @returns The following IPRT status code:
 * @retval  VINF_SUCCESS if access was granted.
 * @retval  VERR_PERMISSION_DENIED if denied access.
 * @retval  VERR_INVALID_PARAMETER if invalid parameter.
 *
 * @param   pvObj           The identifier returned by SUPR0ObjRegister().
 * @param   pSession        The session which wishes to access the object.
 * @param   pszObjName      Object string name. This is optional and depends on the object type.
 *
 * @remark  The caller is responsible for making sure the object isn't removed while
 *          we're inside this function. If uncertain about this, just call AddRef before calling us.
 */
SUPR0DECL(int) SUPR0ObjVerifyAccess(void *pvObj, PSUPDRVSESSION pSession, const char *pszObjName)
{
    PSUPDRVOBJ  pObj = (PSUPDRVOBJ)pvObj;
    int         rc;

    /*
     * Validate the input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pObj) && pObj->u32Magic == SUPDRVOBJ_MAGIC,
                    ("Invalid pvObj=%p magic=%#x (exepcted %#x)\n", pvObj, pObj ? pObj->u32Magic : 0, SUPDRVOBJ_MAGIC),
                    VERR_INVALID_PARAMETER);

    /*
     * Check access. (returns true if a decision has been made.)
     */
    rc = VERR_INTERNAL_ERROR;
    if (supdrvOSObjCanAccess(pObj, pSession, pszObjName, &rc))
        return rc;

    /*
     * Default policy is to allow the user to access his own
     * stuff but nothing else.
     */
    if (pObj->CreatorUid == pSession->Uid)
        return VINF_SUCCESS;
    return VERR_PERMISSION_DENIED;
}


/**
 * API for the VMMR0 module to get the SUPDRVSESSION::pSessionVM member.
 *
 * @returns The associated VM pointer.
 * @param   pSession    The session of the current thread.
 */
SUPR0DECL(PVM) SUPR0GetSessionVM(PSUPDRVSESSION pSession)
{
    AssertReturn(SUP_IS_SESSION_VALID(pSession), NULL);
    return pSession->pSessionVM;
}


/**
 * API for the VMMR0 module to get the SUPDRVSESSION::pSessionGVM member.
 *
 * @returns The associated GVM pointer.
 * @param   pSession    The session of the current thread.
 */
SUPR0DECL(PGVM) SUPR0GetSessionGVM(PSUPDRVSESSION pSession)
{
    AssertReturn(SUP_IS_SESSION_VALID(pSession), NULL);
    return pSession->pSessionGVM;
}


/**
 * API for the VMMR0 module to work the SUPDRVSESSION::pSessionVM member.
 *
 * This will fail if there is already a VM associated with the session and pVM
 * isn't NULL.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_ALREADY_EXISTS if there already is a VM associated with the
 *          session.
 * @retval  VERR_INVALID_PARAMETER if only one of the parameters are NULL or if
 *          the session is invalid.
 *
 * @param   pSession    The session of the current thread.
 * @param   pGVM        The GVM to associate with the session.  Pass NULL to
 *                      dissassociate.
 * @param   pVM         The VM to associate with the session.  Pass NULL to
 *                      dissassociate.
 */
SUPR0DECL(int) SUPR0SetSessionVM(PSUPDRVSESSION pSession, PGVM pGVM, PVM pVM)
{
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn((pGVM != NULL) == (pVM != NULL), VERR_INVALID_PARAMETER);

    RTSpinlockAcquire(pSession->pDevExt->Spinlock);
    if (pGVM)
    {
        if (!pSession->pSessionGVM)
        {
            pSession->pSessionGVM   = pGVM;
            pSession->pSessionVM    = pVM;
            pSession->pFastIoCtrlVM = NULL;
        }
        else
        {
            RTSpinlockRelease(pSession->pDevExt->Spinlock);
            SUPR0Printf("SUPR0SetSessionVM: Unable to associated GVM/VM %p/%p with session %p as it has %p/%p already!\n",
                        pGVM, pVM, pSession, pSession->pSessionGVM, pSession->pSessionVM);
            return VERR_ALREADY_EXISTS;
        }
    }
    else
    {
        pSession->pSessionGVM   = NULL;
        pSession->pSessionVM    = NULL;
        pSession->pFastIoCtrlVM = NULL;
    }
    RTSpinlockRelease(pSession->pDevExt->Spinlock);
    return VINF_SUCCESS;
}


/** @copydoc RTLogGetDefaultInstanceEx
 * @remarks To allow overriding RTLogGetDefaultInstanceEx locally. */
SUPR0DECL(struct RTLOGGER *) SUPR0GetDefaultLogInstanceEx(uint32_t fFlagsAndGroup)
{
    return RTLogGetDefaultInstanceEx(fFlagsAndGroup);
}


/** @copydoc RTLogRelGetDefaultInstanceEx
 * @remarks To allow overriding RTLogRelGetDefaultInstanceEx locally. */
SUPR0DECL(struct RTLOGGER *) SUPR0GetDefaultLogRelInstanceEx(uint32_t fFlagsAndGroup)
{
    return RTLogRelGetDefaultInstanceEx(fFlagsAndGroup);
}


/**
 * Lock pages.
 *
 * @returns IPRT status code.
 * @param   pSession    Session to which the locked memory should be associated.
 * @param   pvR3        Start of the memory range to lock.
 *                      This must be page aligned.
 * @param   cPages      Number of pages to lock.
 * @param   paPages     Where to put the physical addresses of locked memory.
 */
SUPR0DECL(int) SUPR0LockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t cPages, PRTHCPHYS paPages)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    const size_t    cb = (size_t)cPages << PAGE_SHIFT;
    LogFlow(("SUPR0LockMem: pSession=%p pvR3=%p cPages=%d paPages=%p\n", pSession, (void *)pvR3, cPages, paPages));

    /*
     * Verify input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(paPages, VERR_INVALID_PARAMETER);
    if (    RT_ALIGN_R3PT(pvR3, PAGE_SIZE, RTR3PTR) != pvR3
        ||  !pvR3)
    {
        Log(("pvR3 (%p) must be page aligned and not NULL!\n", (void *)pvR3));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Let IPRT do the job.
     */
    Mem.eType = MEMREF_TYPE_LOCKED;
    rc = RTR0MemObjLockUser(&Mem.MemObj, pvR3, cb, RTMEM_PROT_READ | RTMEM_PROT_WRITE, NIL_RTR0PROCESS);
    if (RT_SUCCESS(rc))
    {
        uint32_t iPage = cPages;
        AssertMsg(RTR0MemObjAddressR3(Mem.MemObj) == pvR3, ("%p == %p\n", RTR0MemObjAddressR3(Mem.MemObj), pvR3));
        AssertMsg(RTR0MemObjSize(Mem.MemObj) == cb, ("%x == %x\n", RTR0MemObjSize(Mem.MemObj), cb));

        while (iPage-- > 0)
        {
            paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
            if (RT_UNLIKELY(paPages[iPage] == NIL_RTCCPHYS))
            {
                AssertMsgFailed(("iPage=%d\n", iPage));
                rc = VERR_INTERNAL_ERROR;
                break;
            }
        }
        if (RT_SUCCESS(rc))
            rc = supdrvMemAdd(&Mem, pSession);
        if (RT_FAILURE(rc))
        {
            int rc2 = RTR0MemObjFree(Mem.MemObj, false);
            AssertRC(rc2);
        }
    }

    return rc;
}


/**
 * Unlocks the memory pointed to by pv.
 *
 * @returns IPRT status code.
 * @param   pSession    Session to which the memory was locked.
 * @param   pvR3        Memory to unlock.
 */
SUPR0DECL(int) SUPR0UnlockMem(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    LogFlow(("SUPR0UnlockMem: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, (RTHCUINTPTR)pvR3, MEMREF_TYPE_LOCKED);
}


/**
 * Allocates a chunk of page aligned memory with contiguous and fixed physical
 * backing.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping the allocated memory.
 * @param   pHCPhys     Where to put the physical address of allocated memory.
 */
SUPR0DECL(int) SUPR0ContAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0ContAlloc: pSession=%p cPages=%d ppvR0=%p ppvR3=%p pHCPhys=%p\n", pSession, cPages, ppvR0, ppvR3, pHCPhys));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (!ppvR3 || !ppvR0 || !pHCPhys)
    {
        Log(("Null pointer. All of these should be set: pSession=%p ppvR0=%p ppvR3=%p pHCPhys=%p\n",
             pSession, ppvR0, ppvR3, pHCPhys));
        return VERR_INVALID_PARAMETER;

    }
    if (cPages < 1 || cPages >= 256)
    {
        Log(("Illegal request cPages=%d, must be greater than 0 and smaller than 256.\n", cPages));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the job.
     */
    rc = RTR0MemObjAllocCont(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable R0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, NIL_RTR0PROCESS);
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_CONT;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                *pHCPhys = RTR0MemObjGetPagePhysAddr(Mem.MemObj, 0);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }
        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Frees memory allocated using SUPR0ContAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0ContFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0ContFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_CONT);
}


/**
 * Allocates a chunk of page aligned memory with fixed physical backing below 4GB.
 *
 * The memory isn't zeroed.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   cPages      Number of pages to allocate.
 * @param   ppvR0       Where to put the address of Ring-0 mapping of the allocated memory.
 * @param   ppvR3       Where to put the address of Ring-3 mapping of the allocated memory.
 * @param   paPages     Where to put the physical addresses of allocated memory.
 */
SUPR0DECL(int) SUPR0LowAlloc(PSUPDRVSESSION pSession, uint32_t cPages, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS paPages)
{
    unsigned        iPage;
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0LowAlloc: pSession=%p cPages=%d ppvR3=%p ppvR0=%p paPages=%p\n", pSession, cPages, ppvR3, ppvR0, paPages));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    if (!ppvR3 || !ppvR0 || !paPages)
    {
        Log(("Null pointer. All of these should be set: pSession=%p ppvR3=%p ppvR0=%p paPages=%p\n",
             pSession, ppvR3, ppvR0, paPages));
        return VERR_INVALID_PARAMETER;

    }
    if (cPages < 1 || cPages >= 256)
    {
        Log(("Illegal request cPages=%d, must be greater than 0 and smaller than 256.\n", cPages));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocLow(&Mem.MemObj, cPages << PAGE_SHIFT, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, NIL_RTR0PROCESS);
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_LOW;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                for (iPage = 0; iPage < cPages; iPage++)
                {
                    paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MemObj, iPage);
                    AssertMsg(!(paPages[iPage] & (PAGE_SIZE - 1)), ("iPage=%d Phys=%RHp\n", paPages[iPage]));
                }
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                return 0;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Frees memory allocated using SUPR0LowAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to which the memory was allocated.
 * @param   uPtr        Pointer to the memory (ring-3 or ring-0).
 */
SUPR0DECL(int) SUPR0LowFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0LowFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_LOW);
}



/**
 * Allocates a chunk of memory with both R0 and R3 mappings.
 * The memory is fixed and it's possible to query the physical addresses using SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cb          Number of bytes to allocate.
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 */
SUPR0DECL(int) SUPR0MemAlloc(PSUPDRVSESSION pSession, uint32_t cb, PRTR0PTR ppvR0, PRTR3PTR ppvR3)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0MemAlloc: pSession=%p cb=%d ppvR0=%p ppvR3=%p\n", pSession, cb, ppvR0, ppvR3));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvR0, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvR3, VERR_INVALID_POINTER);
    if (cb < 1 || cb >= _4M)
    {
        Log(("Illegal request cb=%u; must be greater than 0 and smaller than 4MB.\n", cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Let IPRT do the work.
     */
    rc = RTR0MemObjAllocPage(&Mem.MemObj, cb, true /* executable ring-0 mapping */);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0,
                               RTMEM_PROT_EXEC | RTMEM_PROT_WRITE | RTMEM_PROT_READ, NIL_RTR0PROCESS);
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_MEM;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                return VINF_SUCCESS;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }

    return rc;
}


/**
 * Get the physical addresses of memory allocated using SUPR0MemAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession        The session to which the memory was allocated.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 * @param   paPages         Where to store the physical addresses.
 */
SUPR0DECL(int) SUPR0MemGetPhys(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, PSUPPAGE paPages) /** @todo switch this bugger to RTHCPHYS */
{
    PSUPDRVBUNDLE pBundle;
    LogFlow(("SUPR0MemGetPhys: pSession=%p uPtr=%p paPages=%p\n", pSession, (void *)uPtr, paPages));

    /*
     * Validate input.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(paPages, VERR_INVALID_POINTER);
    AssertReturn(uPtr, VERR_INVALID_PARAMETER);

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == MEMREF_TYPE_MEM
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == uPtr)
                        )
                   )
                {
                    const size_t cPages = RTR0MemObjSize(pBundle->aMem[i].MemObj) >> PAGE_SHIFT;
                    size_t iPage;
                    for (iPage = 0; iPage < cPages; iPage++)
                    {
                        paPages[iPage].Phys = RTR0MemObjGetPagePhysAddr(pBundle->aMem[i].MemObj, iPage);
                        paPages[iPage].uReserved = 0;
                    }
                    RTSpinlockRelease(pSession->Spinlock);
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock);
    Log(("Failed to find %p!!!\n", (void *)uPtr));
    return VERR_INVALID_PARAMETER;
}


/**
 * Free memory allocated by SUPR0MemAlloc().
 *
 * @returns IPRT status code.
 * @param   pSession        The session owning the allocation.
 * @param   uPtr            The Ring-0 or Ring-3 address returned by SUPR0MemAlloc().
 */
SUPR0DECL(int) SUPR0MemFree(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr)
{
    LogFlow(("SUPR0MemFree: pSession=%p uPtr=%p\n", pSession, (void *)uPtr));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, uPtr, MEMREF_TYPE_MEM);
}


/**
 * Allocates a chunk of memory with a kernel or/and a user mode mapping.
 *
 * The memory is fixed and it's possible to query the physical addresses using
 * SUPR0MemGetPhys().
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   cPages      The number of pages to allocate.
 * @param   fFlags      Flags, reserved for the future. Must be zero.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 *                      NULL if no ring-3 mapping.
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 *                      NULL if no ring-0 mapping.
 * @param   paPages     Where to store the addresses of the pages. Optional.
 */
SUPR0DECL(int) SUPR0PageAllocEx(PSUPDRVSESSION pSession, uint32_t cPages, uint32_t fFlags, PRTR3PTR ppvR3, PRTR0PTR ppvR0, PRTHCPHYS paPages)
{
    int             rc;
    SUPDRVMEMREF    Mem = { NIL_RTR0MEMOBJ, NIL_RTR0MEMOBJ, MEMREF_TYPE_UNUSED };
    LogFlow(("SUPR0PageAlloc: pSession=%p cb=%d ppvR3=%p\n", pSession, cPages, ppvR3));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppvR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(ppvR0, VERR_INVALID_POINTER);
    AssertReturn(ppvR3 || ppvR0, VERR_INVALID_PARAMETER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    if (cPages < 1 || cPages > VBOX_MAX_ALLOC_PAGE_COUNT)
    {
        Log(("SUPR0PageAlloc: Illegal request cb=%u; must be greater than 0 and smaller than %uMB (VBOX_MAX_ALLOC_PAGE_COUNT pages).\n", cPages, VBOX_MAX_ALLOC_PAGE_COUNT * (_1M / _4K)));
        return VERR_PAGE_COUNT_OUT_OF_RANGE;
    }

    /*
     * Let IPRT do the work.
     */
    if (ppvR0)
        rc = RTR0MemObjAllocPage(&Mem.MemObj, (size_t)cPages * PAGE_SIZE, true /* fExecutable */);
    else
        rc = RTR0MemObjAllocPhysNC(&Mem.MemObj, (size_t)cPages * PAGE_SIZE, NIL_RTHCPHYS);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        if (ppvR3)
            rc = RTR0MemObjMapUser(&Mem.MapObjR3, Mem.MemObj, (RTR3PTR)-1, 0, RTMEM_PROT_WRITE | RTMEM_PROT_READ, NIL_RTR0PROCESS);
        else
            Mem.MapObjR3 = NIL_RTR0MEMOBJ;
        if (RT_SUCCESS(rc))
        {
            Mem.eType = MEMREF_TYPE_PAGE;
            rc = supdrvMemAdd(&Mem, pSession);
            if (!rc)
            {
                if (ppvR3)
                    *ppvR3 = RTR0MemObjAddressR3(Mem.MapObjR3);
                if (ppvR0)
                    *ppvR0 = RTR0MemObjAddress(Mem.MemObj);
                if (paPages)
                {
                    uint32_t iPage = cPages;
                    while (iPage-- > 0)
                    {
                        paPages[iPage] = RTR0MemObjGetPagePhysAddr(Mem.MapObjR3, iPage);
                        Assert(paPages[iPage] != NIL_RTHCPHYS);
                    }
                }
                return VINF_SUCCESS;
            }

            rc2 = RTR0MemObjFree(Mem.MapObjR3, false);
            AssertRC(rc2);
        }

        rc2 = RTR0MemObjFree(Mem.MemObj, false);
        AssertRC(rc2);
    }
    return rc;
}


/**
 * Maps a chunk of memory previously allocated by SUPR0PageAllocEx into kernel
 * space.
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   pvR3        The ring-3 address returned by SUPR0PageAllocEx.
 * @param   offSub      Where to start mapping. Must be page aligned.
 * @param   cbSub       How much to map. Must be page aligned.
 * @param   fFlags      Flags, MBZ.
 * @param   ppvR0       Where to return the address of the ring-0 mapping on
 *                      success.
 */
SUPR0DECL(int) SUPR0PageMapKernel(PSUPDRVSESSION pSession, RTR3PTR pvR3, uint32_t offSub, uint32_t cbSub,
                                  uint32_t fFlags, PRTR0PTR ppvR0)
{
    int             rc;
    PSUPDRVBUNDLE   pBundle;
    RTR0MEMOBJ      hMemObj = NIL_RTR0MEMOBJ;
    LogFlow(("SUPR0PageMapKernel: pSession=%p pvR3=%p offSub=%#x cbSub=%#x\n", pSession, pvR3, offSub, cbSub));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppvR0, VERR_INVALID_POINTER);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertReturn(!(offSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(cbSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cbSub, VERR_INVALID_PARAMETER);

    /*
     * Find the memory object.
     */
    RTSpinlockAcquire(pSession->Spinlock);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    (   pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                         && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                         && pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                         && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3)
                    ||  (   pBundle->aMem[i].eType == MEMREF_TYPE_LOCKED
                         && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                         && pBundle->aMem[i].MapObjR3 == NIL_RTR0MEMOBJ
                         && RTR0MemObjAddressR3(pBundle->aMem[i].MemObj) == pvR3))
                {
                    hMemObj = pBundle->aMem[i].MemObj;
                    break;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock);

    rc = VERR_INVALID_PARAMETER;
    if (hMemObj != NIL_RTR0MEMOBJ)
    {
        /*
         * Do some further input validations before calling IPRT.
         * (Cleanup is done indirectly by telling RTR0MemObjFree to include mappings.)
         */
        size_t cbMemObj = RTR0MemObjSize(hMemObj);
        if (    offSub < cbMemObj
            &&  cbSub <= cbMemObj
            &&  offSub + cbSub <= cbMemObj)
        {
            RTR0MEMOBJ hMapObj;
            rc = RTR0MemObjMapKernelEx(&hMapObj, hMemObj, (void *)-1, 0,
                                       RTMEM_PROT_READ | RTMEM_PROT_WRITE, offSub, cbSub);
            if (RT_SUCCESS(rc))
                *ppvR0 = RTR0MemObjAddress(hMapObj);
        }
        else
            SUPR0Printf("SUPR0PageMapKernel: cbMemObj=%#x offSub=%#x cbSub=%#x\n", cbMemObj, offSub, cbSub);

    }
    return rc;
}


/**
 * Changes the page level protection of one or more pages previously allocated
 * by SUPR0PageAllocEx.
 *
 * @returns IPRT status code.
 * @param   pSession    The session to associated the allocation with.
 * @param   pvR3        The ring-3 address returned by SUPR0PageAllocEx.
 *                      NIL_RTR3PTR if the ring-3 mapping should be unaffected.
 * @param   pvR0        The ring-0 address returned by SUPR0PageAllocEx.
 *                      NIL_RTR0PTR if the ring-0 mapping should be unaffected.
 * @param   offSub      Where to start changing. Must be page aligned.
 * @param   cbSub       How much to change. Must be page aligned.
 * @param   fProt       The new page level protection, see RTMEM_PROT_*.
 */
SUPR0DECL(int) SUPR0PageProtect(PSUPDRVSESSION pSession, RTR3PTR pvR3, RTR0PTR pvR0, uint32_t offSub, uint32_t cbSub, uint32_t fProt)
{
    int             rc;
    PSUPDRVBUNDLE   pBundle;
    RTR0MEMOBJ      hMemObjR0 = NIL_RTR0MEMOBJ;
    RTR0MEMOBJ      hMemObjR3 = NIL_RTR0MEMOBJ;
    LogFlow(("SUPR0PageProtect: pSession=%p pvR3=%p pvR0=%p offSub=%#x cbSub=%#x fProt-%#x\n", pSession, pvR3, pvR0, offSub, cbSub, fProt));

    /*
     * Validate input. The allowed allocation size must be at least equal to the maximum guest VRAM size.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(!(fProt & ~(RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC | RTMEM_PROT_NONE)), VERR_INVALID_PARAMETER);
    AssertReturn(!(offSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(!(cbSub & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cbSub, VERR_INVALID_PARAMETER);

    /*
     * Find the memory object.
     */
    RTSpinlockAcquire(pSession->Spinlock);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (   pBundle->aMem[i].eType == MEMREF_TYPE_PAGE
                    && pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    && (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                        || pvR3 == NIL_RTR3PTR)
                    && (   pvR0 == NIL_RTR0PTR
                        || RTR0MemObjAddress(pBundle->aMem[i].MemObj) == pvR0)
                    && (   pvR3 == NIL_RTR3PTR
                        || RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == pvR3))
                {
                    if (pvR0 != NIL_RTR0PTR)
                        hMemObjR0 = pBundle->aMem[i].MemObj;
                    if (pvR3 != NIL_RTR3PTR)
                        hMemObjR3 = pBundle->aMem[i].MapObjR3;
                    break;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock);

    rc = VERR_INVALID_PARAMETER;
    if (    hMemObjR0 != NIL_RTR0MEMOBJ
        ||  hMemObjR3 != NIL_RTR0MEMOBJ)
    {
        /*
         * Do some further input validations before calling IPRT.
         */
        size_t cbMemObj = hMemObjR0 != NIL_RTR0PTR ? RTR0MemObjSize(hMemObjR0) : RTR0MemObjSize(hMemObjR3);
        if (    offSub < cbMemObj
            &&  cbSub <= cbMemObj
            &&  offSub + cbSub <= cbMemObj)
        {
            rc = VINF_SUCCESS;
            if (hMemObjR3 != NIL_RTR0PTR)
                rc = RTR0MemObjProtect(hMemObjR3, offSub, cbSub, fProt);
            if (hMemObjR0 != NIL_RTR0PTR && RT_SUCCESS(rc))
                rc = RTR0MemObjProtect(hMemObjR0, offSub, cbSub, fProt);
        }
        else
            SUPR0Printf("SUPR0PageMapKernel: cbMemObj=%#x offSub=%#x cbSub=%#x\n", cbMemObj, offSub, cbSub);

    }
    return rc;

}


/**
 * Free memory allocated by SUPR0PageAlloc() and SUPR0PageAllocEx().
 *
 * @returns IPRT status code.
 * @param   pSession        The session owning the allocation.
 * @param   pvR3             The Ring-3 address returned by SUPR0PageAlloc() or
 *                           SUPR0PageAllocEx().
 */
SUPR0DECL(int) SUPR0PageFree(PSUPDRVSESSION pSession, RTR3PTR pvR3)
{
    LogFlow(("SUPR0PageFree: pSession=%p pvR3=%p\n", pSession, (void *)pvR3));
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    return supdrvMemRelease(pSession, (RTHCUINTPTR)pvR3, MEMREF_TYPE_PAGE);
}


/**
 * Reports a bad context, currenctly that means EFLAGS.AC is 0 instead of 1.
 *
 * @param   pDevExt         The device extension.
 * @param   pszFile         The source file where the caller detected the bad
 *                          context.
 * @param   uLine           The line number in @a pszFile.
 * @param   pszExtra        Optional additional message to give further hints.
 */
void VBOXCALL supdrvBadContext(PSUPDRVDEVEXT pDevExt, const char *pszFile, uint32_t uLine, const char *pszExtra)
{
    uint32_t cCalls;

    /*
     * Shorten the filename before displaying the message.
     */
    for (;;)
    {
        const char *pszTmp = strchr(pszFile, '/');
        if (!pszTmp)
            pszTmp = strchr(pszFile, '\\');
        if (!pszTmp)
            break;
        pszFile = pszTmp + 1;
    }
    if (RT_VALID_PTR(pszExtra) && *pszExtra)
        SUPR0Printf("vboxdrv: Bad CPU context error at line %u in %s: %s\n", uLine, pszFile, pszExtra);
    else
        SUPR0Printf("vboxdrv: Bad CPU context error at line %u in %s!\n", uLine, pszFile);

    /*
     * Record the incident so that we stand a chance of blocking I/O controls
     * before panicing the system.
     */
    cCalls = ASMAtomicIncU32(&pDevExt->cBadContextCalls);
    if (cCalls > UINT32_MAX - _1K)
        ASMAtomicWriteU32(&pDevExt->cBadContextCalls, UINT32_MAX - _1K);
}


/**
 * Reports a bad context, currenctly that means EFLAGS.AC is 0 instead of 1.
 *
 * @param   pSession        The session of the caller.
 * @param   pszFile         The source file where the caller detected the bad
 *                          context.
 * @param   uLine           The line number in @a pszFile.
 * @param   pszExtra        Optional additional message to give further hints.
 */
SUPR0DECL(void) SUPR0BadContext(PSUPDRVSESSION pSession, const char *pszFile, uint32_t uLine, const char *pszExtra)
{
    PSUPDRVDEVEXT pDevExt;

    AssertReturnVoid(SUP_IS_SESSION_VALID(pSession));
    pDevExt = pSession->pDevExt;

    supdrvBadContext(pDevExt, pszFile, uLine, pszExtra);
}


/**
 * Gets the paging mode of the current CPU.
 *
 * @returns Paging mode, SUPPAGEINGMODE_INVALID on error.
 */
SUPR0DECL(SUPPAGINGMODE) SUPR0GetPagingMode(void)
{
    SUPPAGINGMODE enmMode;

    RTR0UINTREG cr0 = ASMGetCR0();
    if ((cr0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
        enmMode = SUPPAGINGMODE_INVALID;
    else
    {
        RTR0UINTREG cr4 = ASMGetCR4();
        uint32_t fNXEPlusLMA = 0;
        if (cr4 & X86_CR4_PAE)
        {
            uint32_t fExtFeatures = ASMCpuId_EDX(0x80000001);
            if (fExtFeatures & (X86_CPUID_EXT_FEATURE_EDX_NX | X86_CPUID_EXT_FEATURE_EDX_LONG_MODE))
            {
                uint64_t efer = ASMRdMsr(MSR_K6_EFER);
                if ((fExtFeatures & X86_CPUID_EXT_FEATURE_EDX_NX)        && (efer & MSR_K6_EFER_NXE))
                    fNXEPlusLMA |= RT_BIT(0);
                if ((fExtFeatures & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE) && (efer & MSR_K6_EFER_LMA))
                    fNXEPlusLMA |= RT_BIT(1);
            }
        }

        switch ((cr4 & (X86_CR4_PAE | X86_CR4_PGE)) | fNXEPlusLMA)
        {
            case 0:
                enmMode = SUPPAGINGMODE_32_BIT;
                break;

            case X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;
                break;

            case X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_PAE;
                break;

            case X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_NX;
                break;

            case X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_AMD64;
                break;

            case RT_BIT(1) | X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_NX;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL_NX;
                break;

            default:
                AssertMsgFailed(("Cannot happen! cr4=%#x fNXEPlusLMA=%d\n", cr4, fNXEPlusLMA));
                enmMode = SUPPAGINGMODE_INVALID;
                break;
        }
    }
    return enmMode;
}


/**
 * Change CR4 and take care of the kernel CR4 shadow if applicable.
 *
 * CR4 shadow handling is required for Linux >= 4.0. Calling this function
 * instead of ASMSetCR4() is only necessary for semi-permanent CR4 changes
 * for code with interrupts enabled.
 *
 * @returns the old CR4 value.
 *
 * @param   fOrMask         bits to be set in CR4.
 * @param   fAndMask        bits to be cleard in CR4.
 *
 * @remarks Must be called with preemption/interrupts disabled.
 */
SUPR0DECL(RTCCUINTREG) SUPR0ChangeCR4(RTCCUINTREG fOrMask, RTCCUINTREG fAndMask)
{
#ifdef RT_OS_LINUX
    return supdrvOSChangeCR4(fOrMask, fAndMask);
#else
    RTCCUINTREG uOld = ASMGetCR4();
    RTCCUINTREG uNew = (uOld & fAndMask) | fOrMask;
    if (uNew != uOld)
        ASMSetCR4(uNew);
    return uOld;
#endif
}


/**
 * Enables or disabled hardware virtualization extensions using native OS APIs.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if not supported by the native OS.
 *
 * @param   fEnable         Whether to enable or disable.
 */
SUPR0DECL(int) SUPR0EnableVTx(bool fEnable)
{
#ifdef RT_OS_DARWIN
    return supdrvOSEnableVTx(fEnable);
#else
    RT_NOREF1(fEnable);
    return VERR_NOT_SUPPORTED;
#endif
}


/**
 * Suspends hardware virtualization extensions using the native OS API.
 *
 * This is called prior to entering raw-mode context.
 *
 * @returns @c true if suspended, @c false if not.
 */
SUPR0DECL(bool) SUPR0SuspendVTxOnCpu(void)
{
#ifdef RT_OS_DARWIN
    return supdrvOSSuspendVTxOnCpu();
#else
    return false;
#endif
}


/**
 * Resumes hardware virtualization extensions using the native OS API.
 *
 * This is called after to entering raw-mode context.
 *
 * @param   fSuspended      The return value of SUPR0SuspendVTxOnCpu.
 */
SUPR0DECL(void) SUPR0ResumeVTxOnCpu(bool fSuspended)
{
#ifdef RT_OS_DARWIN
    supdrvOSResumeVTxOnCpu(fSuspended);
#else
    RT_NOREF1(fSuspended);
    Assert(!fSuspended);
#endif
}


SUPR0DECL(int) SUPR0GetCurrentGdtRw(RTHCUINTPTR *pGdtRw)
{
#ifdef RT_OS_LINUX
    return supdrvOSGetCurrentGdtRw(pGdtRw);
#else
    NOREF(pGdtRw);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Checks if raw-mode is usable on this system.
 *
 * The reasons why raw-mode isn't safe to use are host specific.  For example on
 * Windows the Hyper-V root partition may perhapse not allow important bits in
 * CR4 to be changed, which would make it impossible to do a world switch.
 *
 * @returns VBox status code.
 */
SUPR0DECL(int) SUPR0GetRawModeUsability(void)
{
#ifdef RT_OS_WINDOWS
    return supdrvOSGetRawModeUsability();
#else
    return VINF_SUCCESS;
#endif
}


/**
 * Gets AMD-V and VT-x support for the calling CPU.
 *
 * @returns VBox status code.
 * @param   pfCaps          Where to store whether VT-x (SUPVTCAPS_VT_X) or AMD-V
 *                          (SUPVTCAPS_AMD_V) is supported.
 */
SUPR0DECL(int) SUPR0GetVTSupport(uint32_t *pfCaps)
{
    Assert(pfCaps);
    *pfCaps = 0;

    /* Check if the CPU even supports CPUID (extremely ancient CPUs). */
    if (ASMHasCpuId())
    {
        /* Check the range of standard CPUID leafs. */
        uint32_t uMaxLeaf, uVendorEbx, uVendorEcx, uVendorEdx;
        ASMCpuId(0, &uMaxLeaf, &uVendorEbx, &uVendorEcx, &uVendorEdx);
        if (ASMIsValidStdRange(uMaxLeaf))
        {
            /* Query the standard CPUID leaf. */
            uint32_t fFeatEcx, fFeatEdx, uDummy;
            ASMCpuId(1, &uDummy, &uDummy, &fFeatEcx, &fFeatEdx);

            /* Check if the vendor is Intel (or compatible). */
            if (   ASMIsIntelCpuEx(uVendorEbx, uVendorEcx, uVendorEdx)
                || ASMIsViaCentaurCpuEx(uVendorEbx, uVendorEcx, uVendorEdx))
            {
                /* Check VT-x support. In addition, VirtualBox requires MSR and FXSAVE/FXRSTOR to function. */
                if (   (fFeatEcx & X86_CPUID_FEATURE_ECX_VMX)
                    && (fFeatEdx & X86_CPUID_FEATURE_EDX_MSR)
                    && (fFeatEdx & X86_CPUID_FEATURE_EDX_FXSR))
                {
                    *pfCaps = SUPVTCAPS_VT_X;
                    return VINF_SUCCESS;
                }
                return VERR_VMX_NO_VMX;
            }

            /* Check if the vendor is AMD (or compatible). */
            if (ASMIsAmdCpuEx(uVendorEbx, uVendorEcx, uVendorEdx))
            {
                uint32_t fExtFeatEcx, uExtMaxId;
                ASMCpuId(0x80000000, &uExtMaxId, &uDummy, &uDummy, &uDummy);
                ASMCpuId(0x80000001, &uDummy, &uDummy, &fExtFeatEcx, &uDummy);

                /* Check AMD-V support. In addition, VirtualBox requires MSR and FXSAVE/FXRSTOR to function. */
                if (   ASMIsValidExtRange(uExtMaxId)
                    && uExtMaxId >= 0x8000000a
                    && (fExtFeatEcx & X86_CPUID_AMD_FEATURE_ECX_SVM)
                    && (fFeatEdx    & X86_CPUID_FEATURE_EDX_MSR)
                    && (fFeatEdx    & X86_CPUID_FEATURE_EDX_FXSR))
                {
                    *pfCaps = SUPVTCAPS_AMD_V;
                    return VINF_SUCCESS;
                }
                return VERR_SVM_NO_SVM;
            }
        }
    }
    return VERR_UNSUPPORTED_CPU;
}


/**
 * Checks if Intel VT-x feature is usable on this CPU.
 *
 * @returns VBox status code.
 * @param   pfIsSmxModeAmbiguous  Where to return whether the SMX mode causes
 *                                ambiguity that makes us unsure whether we
 *                                really can use VT-x or not.
 *
 * @remarks Must be called with preemption disabled.
 *          The caller is also expected to check that the CPU is an Intel (or
 *          VIA) CPU -and- that it supports VT-x.  Otherwise, this function
 *          might throw a \#GP fault as it tries to read/write MSRs that may not
 *          be present!
 */
SUPR0DECL(int) SUPR0GetVmxUsability(bool *pfIsSmxModeAmbiguous)
{
    uint64_t   fFeatMsr;
    bool       fMaybeSmxMode;
    bool       fMsrLocked;
    bool       fSmxVmxAllowed;
    bool       fVmxAllowed;
    bool       fIsSmxModeAmbiguous;
    int        rc;

    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    fFeatMsr            = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
    fMaybeSmxMode       = RT_BOOL(ASMGetCR4() & X86_CR4_SMXE);
    fMsrLocked          = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_LOCK);
    fSmxVmxAllowed      = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_SMX_VMXON);
    fVmxAllowed         = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_VMXON);
    fIsSmxModeAmbiguous = false;
    rc                  = VERR_INTERNAL_ERROR_5;

    /* Check if the LOCK bit is set but excludes the required VMXON bit. */
    if (fMsrLocked)
    {
        if (fVmxAllowed && fSmxVmxAllowed)
            rc = VINF_SUCCESS;
        else if (!fVmxAllowed && !fSmxVmxAllowed)
            rc = VERR_VMX_MSR_ALL_VMX_DISABLED;
        else if (!fMaybeSmxMode)
        {
            if (fVmxAllowed)
                rc = VINF_SUCCESS;
            else
                rc = VERR_VMX_MSR_VMX_DISABLED;
        }
        else
        {
            /*
             * CR4.SMXE is set but this doesn't mean the CPU is necessarily in SMX mode. We shall assume
             * that it is -not- and that it is a stupid BIOS/OS setting CR4.SMXE for no good reason.
             * See @bugref{6873}.
             */
            Assert(fMaybeSmxMode == true);
            fIsSmxModeAmbiguous = true;
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * MSR is not yet locked; we can change it ourselves here. Once the lock bit is set,
         * this MSR can no longer be modified.
         *
         * Set both the VMX and SMX_VMX bits (if supported) as we can't determine SMX mode
         * accurately. See @bugref{6873}.
         *
         * We need to check for SMX hardware support here, before writing the MSR as
         * otherwise we will #GP fault on CPUs that do not support it. Callers do not check
         * for it.
         */
        uint32_t fFeaturesECX, uDummy;
#ifdef VBOX_STRICT
        /* Callers should have verified these at some point. */
        uint32_t uMaxId, uVendorEBX, uVendorECX, uVendorEDX;
        ASMCpuId(0, &uMaxId, &uVendorEBX, &uVendorECX, &uVendorEDX);
        Assert(ASMIsValidStdRange(uMaxId));
        Assert(   ASMIsIntelCpuEx(     uVendorEBX, uVendorECX, uVendorEDX)
               || ASMIsViaCentaurCpuEx(uVendorEBX, uVendorECX, uVendorEDX));
#endif
        ASMCpuId(1, &uDummy, &uDummy, &fFeaturesECX, &uDummy);
        bool fSmxVmxHwSupport = false;
        if (   (fFeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
            && (fFeaturesECX & X86_CPUID_FEATURE_ECX_SMX))
            fSmxVmxHwSupport = true;

        fFeatMsr |= MSR_IA32_FEATURE_CONTROL_LOCK
                 |  MSR_IA32_FEATURE_CONTROL_VMXON;
        if (fSmxVmxHwSupport)
            fFeatMsr |= MSR_IA32_FEATURE_CONTROL_SMX_VMXON;

        /*
         * Commit.
         */
        ASMWrMsr(MSR_IA32_FEATURE_CONTROL, fFeatMsr);

        /*
         * Verify.
         */
        fFeatMsr = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
        fMsrLocked = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_LOCK);
        if (fMsrLocked)
        {
            fSmxVmxAllowed = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_SMX_VMXON);
            fVmxAllowed    = RT_BOOL(fFeatMsr & MSR_IA32_FEATURE_CONTROL_VMXON);
            if (   fVmxAllowed
                && (   !fSmxVmxHwSupport
                    || fSmxVmxAllowed))
                rc = VINF_SUCCESS;
            else
                rc = !fSmxVmxHwSupport ? VERR_VMX_MSR_VMX_ENABLE_FAILED : VERR_VMX_MSR_SMX_VMX_ENABLE_FAILED;
        }
        else
            rc = VERR_VMX_MSR_LOCKING_FAILED;
    }

    if (pfIsSmxModeAmbiguous)
        *pfIsSmxModeAmbiguous = fIsSmxModeAmbiguous;

    return rc;
}


/**
 * Checks if AMD-V SVM feature is usable on this CPU.
 *
 * @returns VBox status code.
 * @param   fInitSvm    If usable, try to initialize SVM on this CPU.
 *
 * @remarks Must be called with preemption disabled.
 */
SUPR0DECL(int) SUPR0GetSvmUsability(bool fInitSvm)
{
    int      rc;
    uint64_t fVmCr;
    uint64_t fEfer;

    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    fVmCr = ASMRdMsr(MSR_K8_VM_CR);
    if (!(fVmCr & MSR_K8_VM_CR_SVM_DISABLE))
    {
        rc = VINF_SUCCESS;
        if (fInitSvm)
        {
            /* Turn on SVM in the EFER MSR. */
            fEfer = ASMRdMsr(MSR_K6_EFER);
            if (fEfer & MSR_K6_EFER_SVME)
                rc = VERR_SVM_IN_USE;
            else
            {
                ASMWrMsr(MSR_K6_EFER, fEfer | MSR_K6_EFER_SVME);

                /* Paranoia. */
                fEfer = ASMRdMsr(MSR_K6_EFER);
                if (fEfer & MSR_K6_EFER_SVME)
                {
                    /* Restore previous value. */
                    ASMWrMsr(MSR_K6_EFER, fEfer & ~MSR_K6_EFER_SVME);
                }
                else
                    rc = VERR_SVM_ILLEGAL_EFER_MSR;
            }
        }
    }
    else
        rc = VERR_SVM_DISABLED;
    return rc;
}


/**
 * Queries the AMD-V and VT-x capabilities of the calling CPU.
 *
 * @returns VBox status code.
 * @retval  VERR_VMX_NO_VMX
 * @retval  VERR_VMX_MSR_ALL_VMX_DISABLED
 * @retval  VERR_VMX_MSR_VMX_DISABLED
 * @retval  VERR_VMX_MSR_LOCKING_FAILED
 * @retval  VERR_VMX_MSR_VMX_ENABLE_FAILED
 * @retval  VERR_VMX_MSR_SMX_VMX_ENABLE_FAILED
 * @retval  VERR_SVM_NO_SVM
 * @retval  VERR_SVM_DISABLED
 * @retval  VERR_UNSUPPORTED_CPU if not identifiable as an AMD, Intel or VIA
 *          (centaur) CPU.
 *
 * @param   pfCaps          Where to store the capabilities.
 */
int VBOXCALL supdrvQueryVTCapsInternal(uint32_t *pfCaps)
{
    int  rc = VERR_UNSUPPORTED_CPU;
    bool fIsSmxModeAmbiguous = false;
    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;

    /*
     * Input validation.
     */
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);
    *pfCaps = 0;

    /* We may modify MSRs and re-read them, disable preemption so we make sure we don't migrate CPUs. */
    RTThreadPreemptDisable(&PreemptState);

    /* Check if VT-x/AMD-V is supported. */
    rc = SUPR0GetVTSupport(pfCaps);
    if (RT_SUCCESS(rc))
    {
        /* Check if VT-x is supported. */
        if (*pfCaps & SUPVTCAPS_VT_X)
        {
            /* Check if VT-x is usable. */
            rc = SUPR0GetVmxUsability(&fIsSmxModeAmbiguous);
            if (RT_SUCCESS(rc))
            {
                /* Query some basic VT-x capabilities (mainly required by our GUI). */
                VMXCTLSMSR vtCaps;
                vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                if (vtCaps.n.allowed1 & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
                {
                    vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                    if (vtCaps.n.allowed1 & VMX_PROC_CTLS2_EPT)
                        *pfCaps |= SUPVTCAPS_NESTED_PAGING;
                    if (vtCaps.n.allowed1 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)
                        *pfCaps |= SUPVTCAPS_VTX_UNRESTRICTED_GUEST;
                }
            }
        }
        /* Check if AMD-V is supported. */
        else if (*pfCaps & SUPVTCAPS_AMD_V)
        {
            /* Check is SVM is usable. */
            rc = SUPR0GetSvmUsability(false /* fInitSvm */);
            if (RT_SUCCESS(rc))
            {
                /* Query some basic AMD-V capabilities (mainly required by our GUI). */
                uint32_t uDummy, fSvmFeatures;
                ASMCpuId(0x8000000a, &uDummy, &uDummy, &uDummy, &fSvmFeatures);
                if (fSvmFeatures & X86_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                    *pfCaps |= SUPVTCAPS_NESTED_PAGING;
            }
        }
    }

    /* Restore preemption. */
    RTThreadPreemptRestore(&PreemptState);

    /* After restoring preemption, if we may be in SMX mode, print a warning as it's difficult to debug such problems. */
    if (fIsSmxModeAmbiguous)
        SUPR0Printf(("WARNING! CR4 hints SMX mode but your CPU is too secretive. Proceeding anyway... We wish you good luck!\n"));

    return rc;
}


/**
 * Queries the AMD-V and VT-x capabilities of the calling CPU.
 *
 * @returns VBox status code.
 * @retval  VERR_VMX_NO_VMX
 * @retval  VERR_VMX_MSR_ALL_VMX_DISABLED
 * @retval  VERR_VMX_MSR_VMX_DISABLED
 * @retval  VERR_VMX_MSR_LOCKING_FAILED
 * @retval  VERR_VMX_MSR_VMX_ENABLE_FAILED
 * @retval  VERR_VMX_MSR_SMX_VMX_ENABLE_FAILED
 * @retval  VERR_SVM_NO_SVM
 * @retval  VERR_SVM_DISABLED
 * @retval  VERR_UNSUPPORTED_CPU if not identifiable as an AMD, Intel or VIA
 *          (centaur) CPU.
 *
 * @param   pSession        The session handle.
 * @param   pfCaps          Where to store the capabilities.
 */
SUPR0DECL(int) SUPR0QueryVTCaps(PSUPDRVSESSION pSession, uint32_t *pfCaps)
{
    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);

    /*
     * Call common worker.
     */
    return supdrvQueryVTCapsInternal(pfCaps);
}


/**
 * Queries the CPU microcode revision.
 *
 * @returns VBox status code.
 * @retval  VERR_UNSUPPORTED_CPU if not identifiable as a processor with
 *          readable microcode rev.
 *
 * @param   puRevision      Where to store the microcode revision.
 */
int VBOXCALL supdrvQueryUcodeRev(uint32_t *puRevision)
{
    int  rc = VERR_UNSUPPORTED_CPU;
    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;

    /*
     * Input validation.
     */
    AssertPtrReturn(puRevision, VERR_INVALID_POINTER);

    *puRevision = 0;

    /* Disable preemption so we make sure we don't migrate CPUs, just in case. */
    /* NB: We assume that there aren't mismatched microcode revs in the system. */
    RTThreadPreemptDisable(&PreemptState);

    if (ASMHasCpuId())
    {
        uint32_t uDummy, uTFMSEAX;
        uint32_t uMaxId, uVendorEBX, uVendorECX, uVendorEDX;

        ASMCpuId(0, &uMaxId, &uVendorEBX, &uVendorECX, &uVendorEDX);
        ASMCpuId(1, &uTFMSEAX, &uDummy, &uDummy, &uDummy);

        if (ASMIsValidStdRange(uMaxId))
        {
            uint64_t    uRevMsr;
            if (ASMIsIntelCpuEx(uVendorEBX, uVendorECX, uVendorEDX))
            {
                /* Architectural MSR available on Pentium Pro and later. */
                if (ASMGetCpuFamily(uTFMSEAX) >= 6)
                {
                    /* Revision is in the high dword. */
                    uRevMsr = ASMRdMsr(MSR_IA32_BIOS_SIGN_ID);
                    *puRevision = RT_HIDWORD(uRevMsr);
                    rc = VINF_SUCCESS;
                }
            }
            else if (ASMIsAmdCpuEx(uVendorEBX, uVendorECX, uVendorEDX))
            {
                /* Not well documented, but at least all AMD64 CPUs support this. */
                if (ASMGetCpuFamily(uTFMSEAX) >= 15)
                {
                    /* Revision is in the low dword. */
                    uRevMsr = ASMRdMsr(MSR_IA32_BIOS_SIGN_ID);  /* Same MSR as Intel. */
                    *puRevision = RT_LODWORD(uRevMsr);
                    rc = VINF_SUCCESS;
                }
            }
        }
    }

    RTThreadPreemptRestore(&PreemptState);

    return rc;
}

/**
 * Queries the CPU microcode revision.
 *
 * @returns VBox status code.
 * @retval  VERR_UNSUPPORTED_CPU if not identifiable as a processor with
 *          readable microcode rev.
 *
 * @param   pSession        The session handle.
 * @param   puRevision      Where to store the microcode revision.
 */
SUPR0DECL(int) SUPR0QueryUcodeRev(PSUPDRVSESSION pSession, uint32_t *puRevision)
{
    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(puRevision, VERR_INVALID_POINTER);

    /*
     * Call common worker.
     */
    return supdrvQueryUcodeRev(puRevision);
}


/**
 * Register a component factory with the support driver.
 *
 * This is currently restricted to kernel sessions only.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NO_MEMORY if we're out of memory.
 * @retval  VERR_ALREADY_EXISTS if the factory has already been registered.
 * @retval  VERR_ACCESS_DENIED if it isn't a kernel session.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 *
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure.
 *
 * @remarks This interface is also available via SUPR0IdcComponentRegisterFactory.
 */
SUPR0DECL(int) SUPR0ComponentRegisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory)
{
    PSUPDRVFACTORYREG pNewReg;
    const char *psz;
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_ACCESS_DENIED);
    AssertPtrReturn(pFactory, VERR_INVALID_POINTER);
    AssertPtrReturn(pFactory->pfnQueryFactoryInterface, VERR_INVALID_POINTER);
    psz = RTStrEnd(pFactory->szName, sizeof(pFactory->szName));
    AssertReturn(psz, VERR_INVALID_PARAMETER);

    /*
     * Allocate and initialize a new registration structure.
     */
    pNewReg = (PSUPDRVFACTORYREG)RTMemAlloc(sizeof(SUPDRVFACTORYREG));
    if (pNewReg)
    {
        pNewReg->pNext = NULL;
        pNewReg->pFactory = pFactory;
        pNewReg->pSession = pSession;
        pNewReg->cchName = psz - &pFactory->szName[0];

        /*
         * Add it to the tail of the list after checking for prior registration.
         */
        rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
        if (RT_SUCCESS(rc))
        {
            PSUPDRVFACTORYREG pPrev = NULL;
            PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
            while (pCur && pCur->pFactory != pFactory)
            {
                pPrev = pCur;
                pCur = pCur->pNext;
            }
            if (!pCur)
            {
                if (pPrev)
                    pPrev->pNext = pNewReg;
                else
                    pSession->pDevExt->pComponentFactoryHead = pNewReg;
                rc = VINF_SUCCESS;
            }
            else
                rc = VERR_ALREADY_EXISTS;

            RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);
        }

        if (RT_FAILURE(rc))
            RTMemFree(pNewReg);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 * Deregister a component factory.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_FOUND if the factory wasn't registered.
 * @retval  VERR_ACCESS_DENIED if it isn't a kernel session.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 *
 * @param   pSession        The SUPDRV session (must be a ring-0 session).
 * @param   pFactory        Pointer to the component factory registration structure
 *                          previously passed SUPR0ComponentRegisterFactory().
 *
 * @remarks This interface is also available via SUPR0IdcComponentDeregisterFactory.
 */
SUPR0DECL(int) SUPR0ComponentDeregisterFactory(PSUPDRVSESSION pSession, PCSUPDRVFACTORY pFactory)
{
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertReturn(pSession->R0Process == NIL_RTR0PROCESS, VERR_ACCESS_DENIED);
    AssertPtrReturn(pFactory, VERR_INVALID_POINTER);

    /*
     * Take the lock and look for the registration record.
     */
    rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
    if (RT_SUCCESS(rc))
    {
        PSUPDRVFACTORYREG pPrev = NULL;
        PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
        while (pCur && pCur->pFactory != pFactory)
        {
            pPrev = pCur;
            pCur = pCur->pNext;
        }
        if (pCur)
        {
            if (!pPrev)
                pSession->pDevExt->pComponentFactoryHead = pCur->pNext;
            else
                pPrev->pNext = pCur->pNext;

            pCur->pNext = NULL;
            pCur->pFactory = NULL;
            pCur->pSession = NULL;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NOT_FOUND;

        RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);

        RTMemFree(pCur);
    }
    return rc;
}


/**
 * Queries a component factory.
 *
 * @returns VBox status code.
 * @retval  VERR_INVALID_PARAMETER on invalid parameter.
 * @retval  VERR_INVALID_POINTER on invalid pointer parameter.
 * @retval  VERR_SUPDRV_COMPONENT_NOT_FOUND if the component factory wasn't found.
 * @retval  VERR_SUPDRV_INTERFACE_NOT_SUPPORTED if the interface wasn't supported.
 *
 * @param   pSession            The SUPDRV session.
 * @param   pszName             The name of the component factory.
 * @param   pszInterfaceUuid    The UUID of the factory interface (stringified).
 * @param   ppvFactoryIf        Where to store the factory interface.
 */
SUPR0DECL(int) SUPR0ComponentQueryFactory(PSUPDRVSESSION pSession, const char *pszName, const char *pszInterfaceUuid, void **ppvFactoryIf)
{
    const char *pszEnd;
    size_t cchName;
    int rc;

    /*
     * Validate parameters.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);

    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    pszEnd = RTStrEnd(pszName, RT_SIZEOFMEMB(SUPDRVFACTORY, szName));
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cchName = pszEnd - pszName;

    AssertPtrReturn(pszInterfaceUuid, VERR_INVALID_POINTER);
    pszEnd = RTStrEnd(pszInterfaceUuid, RTUUID_STR_LENGTH);
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);

    AssertPtrReturn(ppvFactoryIf, VERR_INVALID_POINTER);
    *ppvFactoryIf = NULL;

    /*
     * Take the lock and try all factories by this name.
     */
    rc = RTSemFastMutexRequest(pSession->pDevExt->mtxComponentFactory);
    if (RT_SUCCESS(rc))
    {
        PSUPDRVFACTORYREG pCur = pSession->pDevExt->pComponentFactoryHead;
        rc = VERR_SUPDRV_COMPONENT_NOT_FOUND;
        while (pCur)
        {
            if (    pCur->cchName == cchName
                &&  !memcmp(pCur->pFactory->szName, pszName, cchName))
            {
                void *pvFactory = pCur->pFactory->pfnQueryFactoryInterface(pCur->pFactory, pSession, pszInterfaceUuid);
                if (pvFactory)
                {
                    *ppvFactoryIf = pvFactory;
                    rc = VINF_SUCCESS;
                    break;
                }
                rc = VERR_SUPDRV_INTERFACE_NOT_SUPPORTED;
            }

            /* next */
            pCur = pCur->pNext;
        }

        RTSemFastMutexRelease(pSession->pDevExt->mtxComponentFactory);
    }
    return rc;
}


/**
 * Adds a memory object to the session.
 *
 * @returns IPRT status code.
 * @param   pMem        Memory tracking structure containing the
 *                      information to track.
 * @param   pSession    The session.
 */
static int supdrvMemAdd(PSUPDRVMEMREF pMem, PSUPDRVSESSION pSession)
{
    PSUPDRVBUNDLE pBundle;

    /*
     * Find free entry and record the allocation.
     */
    RTSpinlockAcquire(pSession->Spinlock);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed < RT_ELEMENTS(pBundle->aMem))
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (pBundle->aMem[i].MemObj == NIL_RTR0MEMOBJ)
                {
                    pBundle->cUsed++;
                    pBundle->aMem[i] = *pMem;
                    RTSpinlockRelease(pSession->Spinlock);
                    return VINF_SUCCESS;
                }
            }
            AssertFailed();             /* !!this can't be happening!!! */
        }
    }
    RTSpinlockRelease(pSession->Spinlock);

    /*
     * Need to allocate a new bundle.
     * Insert into the last entry in the bundle.
     */
    pBundle = (PSUPDRVBUNDLE)RTMemAllocZ(sizeof(*pBundle));
    if (!pBundle)
        return VERR_NO_MEMORY;

    /* take last entry. */
    pBundle->cUsed++;
    pBundle->aMem[RT_ELEMENTS(pBundle->aMem) - 1] = *pMem;

    /* insert into list. */
    RTSpinlockAcquire(pSession->Spinlock);
    pBundle->pNext = pSession->Bundle.pNext;
    pSession->Bundle.pNext = pBundle;
    RTSpinlockRelease(pSession->Spinlock);

    return VINF_SUCCESS;
}


/**
 * Releases a memory object referenced by pointer and type.
 *
 * @returns IPRT status code.
 * @param   pSession    Session data.
 * @param   uPtr        Pointer to memory. This is matched against both the R0 and R3 addresses.
 * @param   eType       Memory type.
 */
static int supdrvMemRelease(PSUPDRVSESSION pSession, RTHCUINTPTR uPtr, SUPDRVMEMREFTYPE eType)
{
    PSUPDRVBUNDLE pBundle;

    /*
     * Validate input.
     */
    if (!uPtr)
    {
        Log(("Illegal address %p\n", (void *)uPtr));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Search for the address.
     */
    RTSpinlockAcquire(pSession->Spinlock);
    for (pBundle = &pSession->Bundle; pBundle; pBundle = pBundle->pNext)
    {
        if (pBundle->cUsed > 0)
        {
            unsigned i;
            for (i = 0; i < RT_ELEMENTS(pBundle->aMem); i++)
            {
                if (    pBundle->aMem[i].eType == eType
                    &&  pBundle->aMem[i].MemObj != NIL_RTR0MEMOBJ
                    &&  (   (RTHCUINTPTR)RTR0MemObjAddress(pBundle->aMem[i].MemObj) == uPtr
                         || (   pBundle->aMem[i].MapObjR3 != NIL_RTR0MEMOBJ
                             && RTR0MemObjAddressR3(pBundle->aMem[i].MapObjR3) == uPtr))
                   )
                {
                    /* Make a copy of it and release it outside the spinlock. */
                    SUPDRVMEMREF Mem = pBundle->aMem[i];
                    pBundle->aMem[i].eType = MEMREF_TYPE_UNUSED;
                    pBundle->aMem[i].MemObj = NIL_RTR0MEMOBJ;
                    pBundle->aMem[i].MapObjR3 = NIL_RTR0MEMOBJ;
                    RTSpinlockRelease(pSession->Spinlock);

                    if (Mem.MapObjR3 != NIL_RTR0MEMOBJ)
                    {
                        int rc = RTR0MemObjFree(Mem.MapObjR3, false);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    if (Mem.MemObj != NIL_RTR0MEMOBJ)
                    {
                        int rc = RTR0MemObjFree(Mem.MemObj, true /* fFreeMappings */);
                        AssertRC(rc); /** @todo figure out how to handle this. */
                    }
                    return VINF_SUCCESS;
                }
            }
        }
    }
    RTSpinlockRelease(pSession->Spinlock);
    Log(("Failed to find %p!!! (eType=%d)\n", (void *)uPtr, eType));
    return VERR_INVALID_PARAMETER;
}


/**
 * Opens an image. If it's the first time it's opened the call must upload
 * the bits using the supdrvIOCtl_LdrLoad() / SUPDRV_IOCTL_LDR_LOAD function.
 *
 * This is the 1st step of the loading.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The open request.
 */
static int supdrvIOCtl_LdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDROPEN pReq)
{
    int             rc;
    PSUPDRVLDRIMAGE pImage;
    void           *pv;
    size_t          cchName = strlen(pReq->u.In.szName); /* (caller checked < 32). */
    SUPDRV_CHECK_SMAP_SETUP();
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    LogFlow(("supdrvIOCtl_LdrOpen: szName=%s cbImageWithTabs=%d\n", pReq->u.In.szName, pReq->u.In.cbImageWithTabs));

    /*
     * Check if we got an instance of the image already.
     */
    supdrvLdrLock(pDevExt);
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
    {
        if (    pImage->szName[cchName] == '\0'
            &&  !memcmp(pImage->szName, pReq->u.In.szName, cchName))
        {
            if (RT_LIKELY(pImage->cUsage < UINT32_MAX / 2U))
            {
                /** @todo check cbImageBits and cbImageWithTabs here, if they differs that indicates that the images are different. */
                pImage->cUsage++;
                pReq->u.Out.pvImageBase   = pImage->pvImage;
                pReq->u.Out.fNeedsLoading = pImage->uState == SUP_IOCTL_LDR_OPEN;
                pReq->u.Out.fNativeLoader = pImage->fNative;
                supdrvLdrAddUsage(pSession, pImage);
                supdrvLdrUnlock(pDevExt);
                SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
                return VINF_SUCCESS;
            }
            supdrvLdrUnlock(pDevExt);
            Log(("supdrvIOCtl_LdrOpen: To many existing references to '%s'!\n", pReq->u.In.szName));
            return VERR_INTERNAL_ERROR_3; /** @todo add VERR_TOO_MANY_REFERENCES */
        }
    }
    /* (not found - add it!) */

    /* If the loader interface is locked down, make userland fail early */
    if (pDevExt->fLdrLockedDown)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("supdrvIOCtl_LdrOpen: Not adding '%s' to image list, loader interface is locked down!\n", pReq->u.In.szName));
        return VERR_PERMISSION_DENIED;
    }

    /*
     * Allocate memory.
     */
    Assert(cchName < sizeof(pImage->szName));
    pv = RTMemAlloc(sizeof(SUPDRVLDRIMAGE));
    if (!pv)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("supdrvIOCtl_LdrOpen: RTMemAlloc() failed\n"));
        return /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_2;
    }
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);

    /*
     * Setup and link in the LDR stuff.
     */
    pImage = (PSUPDRVLDRIMAGE)pv;
    pImage->pvImage         = NULL;
    pImage->pvImageAlloc    = NULL;
    pImage->cbImageWithTabs = pReq->u.In.cbImageWithTabs;
    pImage->cbImageBits     = pReq->u.In.cbImageBits;
    pImage->cSymbols        = 0;
    pImage->paSymbols       = NULL;
    pImage->pachStrTab      = NULL;
    pImage->cbStrTab        = 0;
    pImage->pfnModuleInit   = NULL;
    pImage->pfnModuleTerm   = NULL;
    pImage->pfnServiceReqHandler = NULL;
    pImage->uState          = SUP_IOCTL_LDR_OPEN;
    pImage->cUsage          = 1;
    pImage->pDevExt         = pDevExt;
    memcpy(pImage->szName, pReq->u.In.szName, cchName + 1);

    /*
     * Try load it using the native loader, if that isn't supported, fall back
     * on the older method.
     */
    pImage->fNative         = true;
    rc = supdrvOSLdrOpen(pDevExt, pImage, pReq->u.In.szFilename);
    if (rc == VERR_NOT_SUPPORTED)
    {
        pImage->pvImageAlloc = RTMemExecAlloc(pImage->cbImageBits + 31);
        pImage->pvImage     = RT_ALIGN_P(pImage->pvImageAlloc, 32);
        pImage->fNative     = false;
        rc = pImage->pvImageAlloc ? VINF_SUCCESS : VERR_NO_EXEC_MEMORY;
        SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    }
    if (RT_FAILURE(rc))
    {
        supdrvLdrUnlock(pDevExt);
        RTMemFree(pImage);
        Log(("supdrvIOCtl_LdrOpen(%s): failed - %Rrc\n", pReq->u.In.szName, rc));
        return rc;
    }
    Assert(VALID_PTR(pImage->pvImage) || RT_FAILURE(rc));

    /*
     * Link it.
     */
    pImage->pNext           = pDevExt->pLdrImages;
    pDevExt->pLdrImages     = pImage;

    supdrvLdrAddUsage(pSession, pImage);

    pReq->u.Out.pvImageBase   = pImage->pvImage;
    pReq->u.Out.fNeedsLoading = true;
    pReq->u.Out.fNativeLoader = pImage->fNative;
    supdrvOSLdrNotifyOpened(pDevExt, pImage, pReq->u.In.szFilename);

    supdrvLdrUnlock(pDevExt);
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    return VINF_SUCCESS;
}


/**
 * Worker that validates a pointer to an image entrypoint.
 *
 * @returns IPRT status code.
 * @param   pDevExt         The device globals.
 * @param   pImage          The loader image.
 * @param   pv              The pointer into the image.
 * @param   fMayBeNull      Whether it may be NULL.
 * @param   fCheckNative    Whether to check with the native loaders.
 * @param   pszSymbol       The entrypoint name or log name.  If the symbol
 *                          capitalized it signifies a specific symbol, otherwise it
 *                          for logging.
 * @param   pbImageBits     The image bits prepared by ring-3.
 *
 * @remarks Will leave the lock on failure.
 */
static int supdrvLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, void *pv, bool fMayBeNull,
                                    bool fCheckNative, const uint8_t *pbImageBits, const char *pszSymbol)
{
    if (!fMayBeNull || pv)
    {
        if ((uintptr_t)pv - (uintptr_t)pImage->pvImage >= pImage->cbImageBits)
        {
            supdrvLdrUnlock(pDevExt);
            Log(("Out of range (%p LB %#x): %s=%p\n", pImage->pvImage, pImage->cbImageBits, pszSymbol, pv));
            return VERR_INVALID_PARAMETER;
        }

        if (pImage->fNative && fCheckNative)
        {
            int rc = supdrvOSLdrValidatePointer(pDevExt, pImage, pv, pbImageBits, pszSymbol);
            if (RT_FAILURE(rc))
            {
                supdrvLdrUnlock(pDevExt);
                Log(("Bad entry point address: %s=%p (rc=%Rrc)\n", pszSymbol, pv, rc));
                return rc;
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Formats a load error message.
 *
 * @returns @a rc
 * @param   rc                  Return code.
 * @param   pReq                The request.
 * @param   pszFormat           The error message format string.
 * @param   ...                 Argument to the format string.
 */
int VBOXCALL supdrvLdrLoadError(int rc, PSUPLDRLOAD pReq, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pReq->u.Out.uErrorMagic = SUPLDRLOAD_ERROR_MAGIC;
    RTStrPrintfV(pReq->u.Out.szError, sizeof(pReq->u.Out.szError), pszFormat, va);
    va_end(va);
    Log(("SUP_IOCTL_LDR_LOAD: %s [rc=%Rrc]\n", pReq->u.Out.szError, rc));
    return rc;
}


/**
 * Loads the image bits.
 *
 * This is the 2nd step of the loading.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRLOAD pReq)
{
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    int             rc;
    SUPDRV_CHECK_SMAP_SETUP();
    LogFlow(("supdrvIOCtl_LdrLoad: pvImageBase=%p cbImageWithBits=%d\n", pReq->u.In.pvImageBase, pReq->u.In.cbImageWithTabs));
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);

    /*
     * Find the ldr image.
     */
    supdrvLdrLock(pDevExt);
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);

    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
        return supdrvLdrLoadError(VERR_INVALID_HANDLE, pReq, "Image not found");
    }
    pImage = pUsage->pImage;

    /*
     * Validate input.
     */
    if (   pImage->cbImageWithTabs != pReq->u.In.cbImageWithTabs
        || pImage->cbImageBits     != pReq->u.In.cbImageBits)
    {
        supdrvLdrUnlock(pDevExt);
        return supdrvLdrLoadError(VERR_INVALID_HANDLE, pReq, "Image size mismatch found: %d(prep) != %d(load) or %d != %d",
                                  pImage->cbImageWithTabs, pReq->u.In.cbImageWithTabs, pImage->cbImageBits, pReq->u.In.cbImageBits);
    }

    if (pImage->uState != SUP_IOCTL_LDR_OPEN)
    {
        unsigned uState = pImage->uState;
        supdrvLdrUnlock(pDevExt);
        if (uState != SUP_IOCTL_LDR_LOAD)
            AssertMsgFailed(("SUP_IOCTL_LDR_LOAD: invalid image state %d (%#x)!\n", uState, uState));
        pReq->u.Out.uErrorMagic = 0;
        return VERR_ALREADY_LOADED;
    }

    /* If the loader interface is locked down, don't load new images */
    if (pDevExt->fLdrLockedDown)
    {
        supdrvLdrUnlock(pDevExt);
        return supdrvLdrLoadError(VERR_PERMISSION_DENIED, pReq, "Loader is locked down");
    }

    switch (pReq->u.In.eEPType)
    {
        case SUPLDRLOADEP_NOTHING:
            break;

        case SUPLDRLOADEP_VMMR0:
            rc = supdrvLdrValidatePointer(    pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0,          false, false, pReq->u.In.abImage, "pvVMMR0");
            if (RT_SUCCESS(rc))
                rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, false,  true, pReq->u.In.abImage, "VMMR0EntryFast");
            if (RT_SUCCESS(rc))
                rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx,   false,  true, pReq->u.In.abImage, "VMMR0EntryEx");
            if (RT_FAILURE(rc))
                return supdrvLdrLoadError(rc, pReq, "Invalid VMMR0 pointer");
            break;

        case SUPLDRLOADEP_SERVICE:
            rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.EP.Service.pfnServiceReq, false,  true, pReq->u.In.abImage, "pfnServiceReq");
            if (RT_FAILURE(rc))
                return supdrvLdrLoadError(rc, pReq, "Invalid pfnServiceReq pointer: %p", pReq->u.In.EP.Service.pfnServiceReq);
            if (    pReq->u.In.EP.Service.apvReserved[0] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[1] != NIL_RTR0PTR
                ||  pReq->u.In.EP.Service.apvReserved[2] != NIL_RTR0PTR)
            {
                supdrvLdrUnlock(pDevExt);
                return supdrvLdrLoadError(VERR_INVALID_PARAMETER, pReq,
                                          "Out of range (%p LB %#x): apvReserved={%p,%p,%p} MBZ!",
                                          pImage->pvImage, pReq->u.In.cbImageWithTabs,
                                          pReq->u.In.EP.Service.apvReserved[0],
                                          pReq->u.In.EP.Service.apvReserved[1],
                                          pReq->u.In.EP.Service.apvReserved[2]);
            }
            break;

        default:
            supdrvLdrUnlock(pDevExt);
            return supdrvLdrLoadError(VERR_INVALID_PARAMETER, pReq, "Invalid eEPType=%d", pReq->u.In.eEPType);
    }

    rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.pfnModuleInit, true, true, pReq->u.In.abImage, "ModuleInit");
    if (RT_FAILURE(rc))
        return supdrvLdrLoadError(rc, pReq, "Invalid pfnModuleInit pointer: %p", pReq->u.In.pfnModuleInit);
    rc = supdrvLdrValidatePointer(pDevExt, pImage, pReq->u.In.pfnModuleTerm, true, true, pReq->u.In.abImage, "ModuleTerm");
    if (RT_FAILURE(rc))
        return supdrvLdrLoadError(rc, pReq, "Invalid pfnModuleTerm pointer: %p", pReq->u.In.pfnModuleTerm);
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);

    /*
     * Allocate and copy the tables if non-native.
     * (No need to do try/except as this is a buffered request.)
     */
    if (!pImage->fNative)
    {
        pImage->cbStrTab = pReq->u.In.cbStrTab;
        if (pImage->cbStrTab)
        {
            pImage->pachStrTab = (char *)RTMemAlloc(pImage->cbStrTab);
            if (pImage->pachStrTab)
                memcpy(pImage->pachStrTab, &pReq->u.In.abImage[pReq->u.In.offStrTab], pImage->cbStrTab);
            else
                rc = supdrvLdrLoadError(VERR_NO_MEMORY, pReq, "Out of memory for string table: %#x", pImage->cbStrTab);
            SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
        }

        pImage->cSymbols = pReq->u.In.cSymbols;
        if (RT_SUCCESS(rc) && pImage->cSymbols)
        {
            size_t  cbSymbols = pImage->cSymbols * sizeof(SUPLDRSYM);
            pImage->paSymbols = (PSUPLDRSYM)RTMemAlloc(cbSymbols);
            if (pImage->paSymbols)
                memcpy(pImage->paSymbols, &pReq->u.In.abImage[pReq->u.In.offSymbols], cbSymbols);
            else
                rc = supdrvLdrLoadError(VERR_NO_MEMORY, pReq, "Out of memory for symbol table: %#x", cbSymbols);
            SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
        }
    }

    /*
     * Copy the bits / complete native loading.
     */
    if (RT_SUCCESS(rc))
    {
        pImage->uState = SUP_IOCTL_LDR_LOAD;
        pImage->pfnModuleInit = (PFNR0MODULEINIT)(uintptr_t)pReq->u.In.pfnModuleInit;
        pImage->pfnModuleTerm = (PFNR0MODULETERM)(uintptr_t)pReq->u.In.pfnModuleTerm;

        if (pImage->fNative)
            rc = supdrvOSLdrLoad(pDevExt, pImage, pReq->u.In.abImage, pReq);
        else
        {
            memcpy(pImage->pvImage, &pReq->u.In.abImage[0], pImage->cbImageBits);
            Log(("vboxdrv: Loaded '%s' at %p\n", pImage->szName, pImage->pvImage));
        }
        SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    }

    /*
     * Update any entry points.
     */
    if (RT_SUCCESS(rc))
    {
        switch (pReq->u.In.eEPType)
        {
            default:
            case SUPLDRLOADEP_NOTHING:
                rc = VINF_SUCCESS;
                break;
            case SUPLDRLOADEP_VMMR0:
                rc = supdrvLdrSetVMMR0EPs(pDevExt, pReq->u.In.EP.VMMR0.pvVMMR0,
                                          pReq->u.In.EP.VMMR0.pvVMMR0EntryFast, pReq->u.In.EP.VMMR0.pvVMMR0EntryEx);
                break;
            case SUPLDRLOADEP_SERVICE:
                pImage->pfnServiceReqHandler = (PFNSUPR0SERVICEREQHANDLER)(uintptr_t)pReq->u.In.EP.Service.pfnServiceReq;
                rc = VINF_SUCCESS;
                break;
        }
    }

    /*
     * On success call the module initialization.
     */
    LogFlow(("supdrvIOCtl_LdrLoad: pfnModuleInit=%p\n", pImage->pfnModuleInit));
    if (RT_SUCCESS(rc) && pImage->pfnModuleInit)
    {
        Log(("supdrvIOCtl_LdrLoad: calling pfnModuleInit=%p\n", pImage->pfnModuleInit));
        pDevExt->pLdrInitImage  = pImage;
        pDevExt->hLdrInitThread = RTThreadNativeSelf();
        SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
        rc = pImage->pfnModuleInit(pImage);
        SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
        pDevExt->pLdrInitImage  = NULL;
        pDevExt->hLdrInitThread = NIL_RTNATIVETHREAD;
        if (RT_FAILURE(rc))
        {
            if (pDevExt->pvVMMR0 == pImage->pvImage)
                supdrvLdrUnsetVMMR0EPs(pDevExt);
            supdrvLdrLoadError(rc, pReq, "ModuleInit failed: %Rrc", rc);
        }
    }
    if (RT_SUCCESS(rc))
    {
        SUPR0Printf("vboxdrv: %RKv %s\n", pImage->pvImage, pImage->szName);
        pReq->u.Out.uErrorMagic = 0;
        pReq->u.Out.szError[0]  = '\0';
    }
    else
    {
        /* Inform the tracing component in case ModuleInit registered TPs. */
        supdrvTracerModuleUnloading(pDevExt, pImage);

        pImage->uState              = SUP_IOCTL_LDR_OPEN;
        pImage->pfnModuleInit       = NULL;
        pImage->pfnModuleTerm       = NULL;
        pImage->pfnServiceReqHandler= NULL;
        pImage->cbStrTab            = 0;
        RTMemFree(pImage->pachStrTab);
        pImage->pachStrTab          = NULL;
        RTMemFree(pImage->paSymbols);
        pImage->paSymbols           = NULL;
        pImage->cSymbols            = 0;
    }

    supdrvLdrUnlock(pDevExt);
    SUPDRV_CHECK_SMAP_CHECK(pDevExt, RT_NOTHING);
    return rc;
}


/**
 * Frees a previously loaded (prep'ed) image.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRFREE pReq)
{
    int             rc;
    PSUPDRVLDRUSAGE pUsagePrev;
    PSUPDRVLDRUSAGE pUsage;
    PSUPDRVLDRIMAGE pImage;
    LogFlow(("supdrvIOCtl_LdrFree: pvImageBase=%p\n", pReq->u.In.pvImageBase));

    /*
     * Find the ldr image.
     */
    supdrvLdrLock(pDevExt);
    pUsagePrev = NULL;
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
    {
        pUsagePrev = pUsage;
        pUsage = pUsage->pNext;
    }
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_FREE: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if we can remove anything.
     */
    rc = VINF_SUCCESS;
    pImage = pUsage->pImage;
    if (pImage->cUsage <= 1 || pUsage->cUsage <= 1)
    {
        /*
         * Check if there are any objects with destructors in the image, if
         * so leave it for the session cleanup routine so we get a chance to
         * clean things up in the right order and not leave them all dangling.
         */
        RTSpinlockAcquire(pDevExt->Spinlock);
        if (pImage->cUsage <= 1)
        {
            PSUPDRVOBJ pObj;
            for (pObj = pDevExt->pObjs; pObj; pObj = pObj->pNext)
                if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
                {
                    rc = VERR_DANGLING_OBJECTS;
                    break;
                }
        }
        else
        {
            PSUPDRVUSAGE pGenUsage;
            for (pGenUsage = pSession->pUsage; pGenUsage; pGenUsage = pGenUsage->pNext)
                if (RT_UNLIKELY((uintptr_t)pGenUsage->pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
                {
                    rc = VERR_DANGLING_OBJECTS;
                    break;
                }
        }
        RTSpinlockRelease(pDevExt->Spinlock);
        if (rc == VINF_SUCCESS)
        {
            /* unlink it */
            if (pUsagePrev)
                pUsagePrev->pNext = pUsage->pNext;
            else
                pSession->pLdrUsage = pUsage->pNext;

            /* free it */
            pUsage->pImage = NULL;
            pUsage->pNext = NULL;
            RTMemFree(pUsage);

            /*
             * Dereference the image.
             */
            if (pImage->cUsage <= 1)
                supdrvLdrFree(pDevExt, pImage);
            else
                pImage->cUsage--;
        }
        else
        {
            Log(("supdrvIOCtl_LdrFree: Dangling objects in %p/%s!\n", pImage->pvImage, pImage->szName));
            rc = VINF_SUCCESS; /** @todo BRANCH-2.1: remove this after branching. */
        }
    }
    else
    {
        /*
         * Dereference both image and usage.
         */
        pImage->cUsage--;
        pUsage->cUsage--;
    }

    supdrvLdrUnlock(pDevExt);
    return rc;
}


/**
 * Lock down the image loader interface.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 */
static int supdrvIOCtl_LdrLockDown(PSUPDRVDEVEXT pDevExt)
{
    LogFlow(("supdrvIOCtl_LdrLockDown:\n"));

    supdrvLdrLock(pDevExt);
    if (!pDevExt->fLdrLockedDown)
    {
        pDevExt->fLdrLockedDown = true;
        Log(("supdrvIOCtl_LdrLockDown: Image loader interface locked down\n"));
    }
    supdrvLdrUnlock(pDevExt);

    return VINF_SUCCESS;
}


/**
 * Queries the address of a symbol in an open image.
 *
 * @returns IPRT status code.
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request buffer.
 */
static int supdrvIOCtl_LdrQuerySymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPLDRGETSYMBOL pReq)
{
    PSUPDRVLDRIMAGE pImage;
    PSUPDRVLDRUSAGE pUsage;
    uint32_t        i;
    PSUPLDRSYM      paSyms;
    const char     *pchStrings;
    const size_t    cbSymbol = strlen(pReq->u.In.szSymbol) + 1;
    void           *pvSymbol = NULL;
    int             rc = VERR_SYMBOL_NOT_FOUND;
    Log3(("supdrvIOCtl_LdrQuerySymbol: pvImageBase=%p szSymbol=\"%s\"\n", pReq->u.In.pvImageBase, pReq->u.In.szSymbol));

    /*
     * Find the ldr image.
     */
    supdrvLdrLock(pDevExt);
    pUsage = pSession->pLdrUsage;
    while (pUsage && pUsage->pImage->pvImage != pReq->u.In.pvImageBase)
        pUsage = pUsage->pNext;
    if (!pUsage)
    {
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: couldn't find image!\n"));
        return VERR_INVALID_HANDLE;
    }
    pImage = pUsage->pImage;
    if (pImage->uState != SUP_IOCTL_LDR_LOAD)
    {
        unsigned uState = pImage->uState;
        supdrvLdrUnlock(pDevExt);
        Log(("SUP_IOCTL_LDR_GET_SYMBOL: invalid image state %d (%#x)!\n", uState, uState)); NOREF(uState);
        return VERR_ALREADY_LOADED;
    }

    /*
     * Search the image exports / symbol strings.
     *
     * Note! The int32_t is for native loading on solaris where the data
     *       and text segments are in very different places.
     */
    if (pImage->fNative)
        rc = supdrvOSLdrQuerySymbol(pDevExt, pImage, pReq->u.In.szSymbol, cbSymbol - 1, &pvSymbol);
    else
    {
        pchStrings = pImage->pachStrTab;
        paSyms     = pImage->paSymbols;
        for (i = 0; i < pImage->cSymbols; i++)
        {
            if (    paSyms[i].offName + cbSymbol <= pImage->cbStrTab
                &&  !memcmp(pchStrings + paSyms[i].offName, pReq->u.In.szSymbol, cbSymbol))
            {
                pvSymbol = (uint8_t *)pImage->pvImage + (int32_t)paSyms[i].offSymbol;
                rc = VINF_SUCCESS;
                break;
            }
        }
    }
    supdrvLdrUnlock(pDevExt);
    pReq->u.Out.pvSymbol = pvSymbol;
    return rc;
}


/**
 * Gets the address of a symbol in an open image or the support driver.
 *
 * @returns VINF_SUCCESS on success.
 * @returns
 * @param   pDevExt     Device globals.
 * @param   pSession    Session data.
 * @param   pReq        The request buffer.
 */
static int supdrvIDC_LdrGetSymbol(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPDRVIDCREQGETSYM pReq)
{
    int             rc = VINF_SUCCESS;
    const char     *pszSymbol = pReq->u.In.pszSymbol;
    const char     *pszModule = pReq->u.In.pszModule;
    size_t          cbSymbol;
    char const     *pszEnd;
    uint32_t        i;

    /*
     * Input validation.
     */
    AssertPtrReturn(pszSymbol, VERR_INVALID_POINTER);
    pszEnd = RTStrEnd(pszSymbol, 512);
    AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    cbSymbol = pszEnd - pszSymbol + 1;

    if (pszModule)
    {
        AssertPtrReturn(pszModule, VERR_INVALID_POINTER);
        pszEnd = RTStrEnd(pszModule, 64);
        AssertReturn(pszEnd, VERR_INVALID_PARAMETER);
    }
    Log3(("supdrvIDC_LdrGetSymbol: pszModule=%p:{%s} pszSymbol=%p:{%s}\n", pszModule, pszModule, pszSymbol, pszSymbol));


    if (    !pszModule
        ||  !strcmp(pszModule, "SupDrv"))
    {
        /*
         * Search the support driver export table.
         */
        for (i = 0; i < RT_ELEMENTS(g_aFunctions); i++)
            if (!strcmp(g_aFunctions[i].szName, pszSymbol))
            {
                pReq->u.Out.pfnSymbol = (PFNRT)(uintptr_t)g_aFunctions[i].pfn;
                break;
            }
    }
    else
    {
        /*
         * Find the loader image.
         */
        PSUPDRVLDRIMAGE pImage;

        supdrvLdrLock(pDevExt);

        for (pImage = pDevExt->pLdrImages; pImage; pImage = pImage->pNext)
            if (!strcmp(pImage->szName, pszModule))
                break;
        if (pImage && pImage->uState == SUP_IOCTL_LDR_LOAD)
        {
            /*
             * Search the image exports / symbol strings.
             */
            if (pImage->fNative)
            {
                rc = supdrvOSLdrQuerySymbol(pDevExt, pImage, pszSymbol, cbSymbol - 1, (void **)&pReq->u.Out.pfnSymbol);
                if (RT_SUCCESS(rc))
                    rc = supdrvLdrAddUsage(pSession, pImage);
            }
            else
            {
                const char *pchStrings = pImage->pachStrTab;
                PCSUPLDRSYM paSyms     = pImage->paSymbols;
                rc = VERR_SYMBOL_NOT_FOUND;
                for (i = 0; i < pImage->cSymbols; i++)
                {
                    if (    paSyms[i].offName + cbSymbol <= pImage->cbStrTab
                        &&  !memcmp(pchStrings + paSyms[i].offName, pszSymbol, cbSymbol))
                    {
                        /*
                         * Found it! Calc the symbol address and add a reference to the module.
                         */
                        pReq->u.Out.pfnSymbol = (PFNRT)((uintptr_t)pImage->pvImage + (int32_t)paSyms[i].offSymbol);
                        rc = supdrvLdrAddUsage(pSession, pImage);
                        break;
                    }
                }
            }
        }
        else
            rc = pImage ? VERR_WRONG_ORDER : VERR_MODULE_NOT_FOUND;

        supdrvLdrUnlock(pDevExt);
    }
    return rc;
}


/**
 * Looks up a symbol in g_aFunctions
 *
 * @returns VINF_SUCCESS on success, VERR_SYMBOL_NOT_FOUND on failure.
 * @param   pszSymbol   The symbol to look up.
 * @param   puValue     Where to return the value.
 */
int VBOXCALL supdrvLdrGetExportedSymbol(const char *pszSymbol, uintptr_t *puValue)
{
    uint32_t i;
    for (i = 0; i < RT_ELEMENTS(g_aFunctions); i++)
        if (!strcmp(g_aFunctions[i].szName, pszSymbol))
        {
            *puValue = (uintptr_t)g_aFunctions[i].pfn;
            return VINF_SUCCESS;
        }

    if (!strcmp(pszSymbol, "g_SUPGlobalInfoPage"))
    {
        *puValue = (uintptr_t)g_pSUPGlobalInfoPage;
        return VINF_SUCCESS;
    }

    return VERR_SYMBOL_NOT_FOUND;
}


/**
 * Updates the VMMR0 entry point pointers.
 *
 * @returns IPRT status code.
 * @param   pDevExt             Device globals.
 * @param   pvVMMR0             VMMR0 image handle.
 * @param   pvVMMR0EntryFast    VMMR0EntryFast address.
 * @param   pvVMMR0EntryEx      VMMR0EntryEx address.
 * @remark  Caller must own the loader mutex.
 */
static int supdrvLdrSetVMMR0EPs(PSUPDRVDEVEXT pDevExt, void *pvVMMR0, void *pvVMMR0EntryFast, void *pvVMMR0EntryEx)
{
    int rc = VINF_SUCCESS;
    LogFlow(("supdrvLdrSetR0EP pvVMMR0=%p pvVMMR0EntryFast=%p\n", pvVMMR0, pvVMMR0EntryFast));


    /*
     * Check if not yet set.
     */
    if (!pDevExt->pvVMMR0)
    {
        pDevExt->pvVMMR0 = pvVMMR0;
        *(void **)&pDevExt->pfnVMMR0EntryFast = pvVMMR0EntryFast;
        *(void **)&pDevExt->pfnVMMR0EntryEx   = pvVMMR0EntryEx;
        ASMCompilerBarrier(); /* the above isn't nice, so be careful... */
    }
    else
    {
        /*
         * Return failure or success depending on whether the values match or not.
         */
        if (    pDevExt->pvVMMR0 != pvVMMR0
            ||  (uintptr_t)pDevExt->pfnVMMR0EntryFast  != (uintptr_t)pvVMMR0EntryFast
            ||  (uintptr_t)pDevExt->pfnVMMR0EntryEx    != (uintptr_t)pvVMMR0EntryEx)
        {
            AssertMsgFailed(("SUP_IOCTL_LDR_SETR0EP: Already set pointing to a different module!\n"));
            rc = VERR_INVALID_PARAMETER;
        }
    }
    return rc;
}


/**
 * Unsets the VMMR0 entry point installed by supdrvLdrSetR0EP.
 *
 * @param   pDevExt     Device globals.
 */
static void supdrvLdrUnsetVMMR0EPs(PSUPDRVDEVEXT pDevExt)
{
    pDevExt->pvVMMR0            = NULL;
    pDevExt->pfnVMMR0EntryFast  = NULL;
    pDevExt->pfnVMMR0EntryEx    = NULL;
}


/**
 * Adds a usage reference in the specified session of an image.
 *
 * Called while owning the loader semaphore.
 *
 * @returns VINF_SUCCESS on success and VERR_NO_MEMORY on failure.
 * @param   pSession    Session in question.
 * @param   pImage      Image which the session is using.
 */
static int supdrvLdrAddUsage(PSUPDRVSESSION pSession, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRUSAGE pUsage;
    LogFlow(("supdrvLdrAddUsage: pImage=%p\n", pImage));

    /*
     * Referenced it already?
     */
    pUsage = pSession->pLdrUsage;
    while (pUsage)
    {
        if (pUsage->pImage == pImage)
        {
            pUsage->cUsage++;
            return VINF_SUCCESS;
        }
        pUsage = pUsage->pNext;
    }

    /*
     * Allocate new usage record.
     */
    pUsage = (PSUPDRVLDRUSAGE)RTMemAlloc(sizeof(*pUsage));
    AssertReturn(pUsage, /*VERR_NO_MEMORY*/ VERR_INTERNAL_ERROR_5);
    pUsage->cUsage = 1;
    pUsage->pImage = pImage;
    pUsage->pNext  = pSession->pLdrUsage;
    pSession->pLdrUsage = pUsage;
    return VINF_SUCCESS;
}


/**
 * Frees a load image.
 *
 * @param   pDevExt     Pointer to device extension.
 * @param   pImage      Pointer to the image we're gonna free.
 *                      This image must exit!
 * @remark  The caller MUST own SUPDRVDEVEXT::mtxLdr!
 */
static void supdrvLdrFree(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    PSUPDRVLDRIMAGE pImagePrev;
    LogFlow(("supdrvLdrFree: pImage=%p\n", pImage));

    /*
     * Warn if we're releasing images while the image loader interface is
     * locked down -- we won't be able to reload them!
     */
    if (pDevExt->fLdrLockedDown)
        Log(("supdrvLdrFree: Warning: unloading '%s' image, while loader interface is locked down!\n", pImage->szName));

    /* find it - arg. should've used doubly linked list. */
    Assert(pDevExt->pLdrImages);
    pImagePrev = NULL;
    if (pDevExt->pLdrImages != pImage)
    {
        pImagePrev = pDevExt->pLdrImages;
        while (pImagePrev->pNext != pImage)
            pImagePrev = pImagePrev->pNext;
        Assert(pImagePrev->pNext == pImage);
    }

    /* unlink */
    if (pImagePrev)
        pImagePrev->pNext = pImage->pNext;
    else
        pDevExt->pLdrImages = pImage->pNext;

    /* check if this is VMMR0.r0 unset its entry point pointers. */
    if (pDevExt->pvVMMR0 == pImage->pvImage)
        supdrvLdrUnsetVMMR0EPs(pDevExt);

    /* check for objects with destructors in this image. (Shouldn't happen.) */
    if (pDevExt->pObjs)
    {
        unsigned        cObjs = 0;
        PSUPDRVOBJ      pObj;
        RTSpinlockAcquire(pDevExt->Spinlock);
        for (pObj = pDevExt->pObjs; pObj; pObj = pObj->pNext)
            if (RT_UNLIKELY((uintptr_t)pObj->pfnDestructor - (uintptr_t)pImage->pvImage < pImage->cbImageBits))
            {
                pObj->pfnDestructor = NULL;
                cObjs++;
            }
        RTSpinlockRelease(pDevExt->Spinlock);
        if (cObjs)
            OSDBGPRINT(("supdrvLdrFree: Image '%s' has %d dangling objects!\n", pImage->szName, cObjs));
    }

    /* call termination function if fully loaded. */
    if (    pImage->pfnModuleTerm
        &&  pImage->uState == SUP_IOCTL_LDR_LOAD)
    {
        LogFlow(("supdrvIOCtl_LdrLoad: calling pfnModuleTerm=%p\n", pImage->pfnModuleTerm));
        pImage->pfnModuleTerm(pImage);
    }

    /* Inform the tracing component. */
    supdrvTracerModuleUnloading(pDevExt, pImage);

    /* Do native unload if appropriate, then inform the native code about the
       unloading (mainly for non-native loading case). */
    if (pImage->fNative)
        supdrvOSLdrUnload(pDevExt, pImage);
    supdrvOSLdrNotifyUnloaded(pDevExt, pImage);

    /* free the image */
    pImage->cUsage  = 0;
    pImage->pDevExt = NULL;
    pImage->pNext   = NULL;
    pImage->uState  = SUP_IOCTL_LDR_FREE;
    RTMemExecFree(pImage->pvImageAlloc, pImage->cbImageBits + 31);
    pImage->pvImageAlloc = NULL;
    RTMemFree(pImage->pachStrTab);
    pImage->pachStrTab = NULL;
    RTMemFree(pImage->paSymbols);
    pImage->paSymbols = NULL;
    RTMemFree(pImage);
}


/**
 * Acquires the loader lock.
 *
 * @returns IPRT status code.
 * @param   pDevExt         The device extension.
 */
DECLINLINE(int) supdrvLdrLock(PSUPDRVDEVEXT pDevExt)
{
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    int rc = RTSemMutexRequest(pDevExt->mtxLdr, RT_INDEFINITE_WAIT);
#else
    int rc = RTSemFastMutexRequest(pDevExt->mtxLdr);
#endif
    AssertRC(rc);
    return rc;
}


/**
 * Releases the loader lock.
 *
 * @returns IPRT status code.
 * @param   pDevExt         The device extension.
 */
DECLINLINE(int) supdrvLdrUnlock(PSUPDRVDEVEXT pDevExt)
{
#ifdef SUPDRV_USE_MUTEX_FOR_LDR
    return RTSemMutexRelease(pDevExt->mtxLdr);
#else
    return RTSemFastMutexRelease(pDevExt->mtxLdr);
#endif
}


/**
 * Implements the service call request.
 *
 * @returns VBox status code.
 * @param   pDevExt         The device extension.
 * @param   pSession        The calling session.
 * @param   pReq            The request packet, valid.
 */
static int supdrvIOCtl_CallServiceModule(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPCALLSERVICE pReq)
{
#if !defined(RT_OS_WINDOWS) || defined(RT_ARCH_AMD64) || defined(DEBUG)
    int rc;

    /*
     * Find the module first in the module referenced by the calling session.
     */
    rc = supdrvLdrLock(pDevExt);
    if (RT_SUCCESS(rc))
    {
        PFNSUPR0SERVICEREQHANDLER   pfnServiceReqHandler = NULL;
        PSUPDRVLDRUSAGE             pUsage;

        for (pUsage = pSession->pLdrUsage; pUsage; pUsage = pUsage->pNext)
            if (    pUsage->pImage->pfnServiceReqHandler
                &&  !strcmp(pUsage->pImage->szName, pReq->u.In.szName))
            {
                pfnServiceReqHandler = pUsage->pImage->pfnServiceReqHandler;
                break;
            }
        supdrvLdrUnlock(pDevExt);

        if (pfnServiceReqHandler)
        {
            /*
             * Call it.
             */
            if (pReq->Hdr.cbIn == SUP_IOCTL_CALL_SERVICE_SIZE(0))
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, NULL);
            else
                rc = pfnServiceReqHandler(pSession, pReq->u.In.uOperation, pReq->u.In.u64Arg, (PSUPR0SERVICEREQHDR)&pReq->abReqPkt[0]);
        }
        else
            rc = VERR_SUPDRV_SERVICE_NOT_FOUND;
    }

    /* log it */
    if (    RT_FAILURE(rc)
        &&  rc != VERR_INTERRUPTED
        &&  rc != VERR_TIMEOUT)
        Log(("SUP_IOCTL_CALL_SERVICE: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
             rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
    else
        Log4(("SUP_IOCTL_CALL_SERVICE: rc=%Rrc op=%u out=%u arg=%RX64 p/t=%RTproc/%RTthrd\n",
              rc, pReq->u.In.uOperation, pReq->Hdr.cbOut, pReq->u.In.u64Arg, RTProcSelf(), RTThreadNativeSelf()));
    return rc;
#else  /* RT_OS_WINDOWS && !RT_ARCH_AMD64 && !DEBUG */
    RT_NOREF3(pDevExt, pSession, pReq);
    return VERR_NOT_IMPLEMENTED;
#endif /* RT_OS_WINDOWS && !RT_ARCH_AMD64 && !DEBUG */
}


/**
 * Implements the logger settings request.
 *
 * @returns VBox status code.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_LoggerSettings(PSUPLOGGERSETTINGS pReq)
{
    const char *pszGroup = &pReq->u.In.szStrings[pReq->u.In.offGroups];
    const char *pszFlags = &pReq->u.In.szStrings[pReq->u.In.offFlags];
    const char *pszDest  = &pReq->u.In.szStrings[pReq->u.In.offDestination];
    PRTLOGGER   pLogger  = NULL;
    int         rc;

    /*
     * Some further validation.
     */
    switch (pReq->u.In.fWhat)
    {
        case SUPLOGGERSETTINGS_WHAT_SETTINGS:
        case SUPLOGGERSETTINGS_WHAT_CREATE:
            break;

        case SUPLOGGERSETTINGS_WHAT_DESTROY:
            if (*pszGroup || *pszFlags || *pszDest)
                return VERR_INVALID_PARAMETER;
            if (pReq->u.In.fWhich == SUPLOGGERSETTINGS_WHICH_RELEASE)
                return VERR_ACCESS_DENIED;
            break;

        default:
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Get the logger.
     */
    switch (pReq->u.In.fWhich)
    {
        case SUPLOGGERSETTINGS_WHICH_DEBUG:
            pLogger = RTLogGetDefaultInstance();
            break;

        case SUPLOGGERSETTINGS_WHICH_RELEASE:
            pLogger = RTLogRelGetDefaultInstance();
            break;

        default:
            return VERR_INTERNAL_ERROR;
    }

    /*
     * Do the job.
     */
    switch (pReq->u.In.fWhat)
    {
        case SUPLOGGERSETTINGS_WHAT_SETTINGS:
            if (pLogger)
            {
                rc = RTLogFlags(pLogger, pszFlags);
                if (RT_SUCCESS(rc))
                    rc = RTLogGroupSettings(pLogger, pszGroup);
                NOREF(pszDest);
            }
            else
                rc = VERR_NOT_FOUND;
            break;

        case SUPLOGGERSETTINGS_WHAT_CREATE:
        {
            if (pLogger)
                rc = VERR_ALREADY_EXISTS;
            else
            {
                static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;

                rc = RTLogCreate(&pLogger,
                                 0 /* fFlags */,
                                 pszGroup,
                                 pReq->u.In.fWhich == SUPLOGGERSETTINGS_WHICH_DEBUG
                                 ? "VBOX_LOG"
                                 : "VBOX_RELEASE_LOG",
                                 RT_ELEMENTS(s_apszGroups),
                                 s_apszGroups,
                                 RTLOGDEST_STDOUT | RTLOGDEST_DEBUGGER,
                                 NULL);
                if (RT_SUCCESS(rc))
                {
                    rc = RTLogFlags(pLogger, pszFlags);
                    NOREF(pszDest);
                    if (RT_SUCCESS(rc))
                    {
                        switch (pReq->u.In.fWhich)
                        {
                            case SUPLOGGERSETTINGS_WHICH_DEBUG:
                                pLogger = RTLogSetDefaultInstance(pLogger);
                                break;
                            case SUPLOGGERSETTINGS_WHICH_RELEASE:
                                pLogger = RTLogRelSetDefaultInstance(pLogger);
                                break;
                        }
                    }
                    RTLogDestroy(pLogger);
                }
            }
            break;
        }

        case SUPLOGGERSETTINGS_WHAT_DESTROY:
            switch (pReq->u.In.fWhich)
            {
                case SUPLOGGERSETTINGS_WHICH_DEBUG:
                    pLogger = RTLogSetDefaultInstance(NULL);
                    break;
                case SUPLOGGERSETTINGS_WHICH_RELEASE:
                    pLogger = RTLogRelSetDefaultInstance(NULL);
                    break;
            }
            rc = RTLogDestroy(pLogger);
            break;

        default:
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }

    return rc;
}


/**
 * Implements the MSR prober operations.
 *
 * @returns VBox status code.
 * @param   pDevExt     The device extension.
 * @param   pReq        The request.
 */
static int supdrvIOCtl_MsrProber(PSUPDRVDEVEXT pDevExt, PSUPMSRPROBER pReq)
{
#ifdef SUPDRV_WITH_MSR_PROBER
    RTCPUID const idCpu = pReq->u.In.idCpu == UINT32_MAX ? NIL_RTCPUID : pReq->u.In.idCpu;
    int rc;

    switch (pReq->u.In.enmOp)
    {
        case SUPMSRPROBEROP_READ:
        {
            uint64_t uValue;
            rc = supdrvOSMsrProberRead(pReq->u.In.uMsr, idCpu, &uValue);
            if (RT_SUCCESS(rc))
            {
                pReq->u.Out.uResults.Read.uValue = uValue;
                pReq->u.Out.uResults.Read.fGp    = false;
            }
            else if (rc == VERR_ACCESS_DENIED)
            {
                pReq->u.Out.uResults.Read.uValue = 0;
                pReq->u.Out.uResults.Read.fGp    = true;
                rc  = VINF_SUCCESS;
            }
            break;
        }

        case SUPMSRPROBEROP_WRITE:
            rc = supdrvOSMsrProberWrite(pReq->u.In.uMsr, idCpu, pReq->u.In.uArgs.Write.uToWrite);
            if (RT_SUCCESS(rc))
                pReq->u.Out.uResults.Write.fGp   = false;
            else if (rc == VERR_ACCESS_DENIED)
            {
                pReq->u.Out.uResults.Write.fGp   = true;
                rc  = VINF_SUCCESS;
            }
            break;

        case SUPMSRPROBEROP_MODIFY:
        case SUPMSRPROBEROP_MODIFY_FASTER:
            rc = supdrvOSMsrProberModify(idCpu, pReq);
            break;

        default:
            return VERR_INVALID_FUNCTION;
    }
    RT_NOREF1(pDevExt);
    return rc;
#else
    RT_NOREF2(pDevExt, pReq);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Resume built-in keyboard on MacBook Air and Pro hosts.
 * If there is no built-in keyboard device, return success anyway.
 *
 * @returns 0 on Mac OS X platform, VERR_NOT_IMPLEMENTED on the other ones.
 */
static int supdrvIOCtl_ResumeSuspendedKbds(void)
{
#if defined(RT_OS_DARWIN)
    return supdrvDarwinResumeSuspendedKbds();
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

