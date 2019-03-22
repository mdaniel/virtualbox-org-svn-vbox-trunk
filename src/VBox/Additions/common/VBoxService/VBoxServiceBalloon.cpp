/* $Id$ */
/** @file
 * VBoxService - Memory Ballooning.
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


/** @page pg_vgsvc_memballoon VBoxService - Memory Ballooning
 *
 * The Memory Ballooning subservice works with VBoxGuest, PGM and GMM to
 * dynamically reallocate memory between VMs.
 *
 * Memory ballooning is typically used to deal with overcomitting memory on the
 * host.  It allowes you to borrow memory from one or more VMs and make it
 * available to others.  In theory it could also be used to make memory
 * available to the host system, however memory fragmentation typically makes
 * that difficult.
 *
 * The memory ballooning subservices talks to PGM, GMM and Main via the VMMDev.
 * It polls for change requests at an interval and executes them when they
 * arrive.  There are two ways we implement the actual ballooning, either
 * VBoxGuest allocates kernel memory and donates it to the host, or this service
 * allocates process memory which VBoxGuest then locks down and donates to the
 * host.  While we prefer the former method it is not practicable on all OS and
 * we have to use the latter.
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

#ifdef RT_OS_LINUX
# include <iprt/param.h>
# include <sys/mman.h>
# ifndef MADV_DONTFORK
#  define MADV_DONTFORK 10
# endif
#endif



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The balloon size. */
static uint32_t g_cMemBalloonChunks = 0;

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;

/** The array holding the R3 pointers of the balloon. */
static void **g_pavBalloon = NULL;

#ifdef RT_OS_LINUX
/** True = madvise(MADV_DONTFORK) works, false otherwise. */
static bool g_fSysMadviseWorks;
#endif


/**
 * Check whether madvise() works.
 */
static void vgsvcBalloonInitMadvise(void)
{
#ifdef RT_OS_LINUX
    void *pv = (void*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pv != MAP_FAILED)
    {
        g_fSysMadviseWorks = madvise(pv, PAGE_SIZE, MADV_DONTFORK) == 0;
        munmap(pv, PAGE_SIZE);
    }
#endif
}


/**
 * Allocate a chunk of the balloon. Fulfil the prerequisite that we can lock this memory
 * and protect it against fork() in R0. See also suplibOsPageAlloc().
 */
static void *VGSvcBalloonAllocChunk(void)
{
    size_t cb = VMMDEV_MEMORY_BALLOON_CHUNK_SIZE;
    char *pu8;

#ifdef RT_OS_LINUX
    if (!g_fSysMadviseWorks)
        cb += 2 * PAGE_SIZE;

    pu8 = (char*)mmap(NULL, cb, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pu8 == MAP_FAILED)
        return NULL;

    if (g_fSysMadviseWorks)
    {
        /*
         * It is not fatal if we fail here but a forked child (e.g. the ALSA sound server)
         * could crash. Linux < 2.6.16 does not implement madvise(MADV_DONTFORK) but the
         * kernel seems to split bigger VMAs and that is all that we want -- later we set the
         * VM_DONTCOPY attribute in supdrvOSLockMemOne().
         */
        madvise(pu8, cb, MADV_DONTFORK);
    }
    else
    {
        /*
         * madvise(MADV_DONTFORK) is not available (most probably Linux 2.4). Enclose any
         * mmapped region by two unmapped pages to guarantee that there is exactly one VM
         * area struct of the very same size as the mmap area.
         */
        RTMemProtect(pu8, PAGE_SIZE, RTMEM_PROT_NONE);
        RTMemProtect(pu8 + cb - PAGE_SIZE, PAGE_SIZE, RTMEM_PROT_NONE);
        pu8 += PAGE_SIZE;
    }

#else

    pu8 = (char*)RTMemPageAlloc(cb);
    if (!pu8)
        return pu8;

#endif

    memset(pu8, 0, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE);
    return pu8;
}


/**
 * Free an allocated chunk undoing VGSvcBalloonAllocChunk().
 */
static void vgsvcBalloonFreeChunk(void *pv)
{
    char *pu8 = (char*)pv;
    size_t cb = VMMDEV_MEMORY_BALLOON_CHUNK_SIZE;

#ifdef RT_OS_LINUX

    if (!g_fSysMadviseWorks)
    {
        cb += 2 * PAGE_SIZE;
        pu8 -= PAGE_SIZE;
        /* This is not really necessary */
        RTMemProtect(pu8, PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
        RTMemProtect(pu8 + cb - PAGE_SIZE, PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
    }
    munmap(pu8, cb);

#else

    RTMemPageFree(pu8, cb);

#endif
}


/**
 * Adapt the R0 memory balloon by granting/reclaiming 1MB chunks to/from R0.
 *
 * returns IPRT status code.
 * @param   cNewChunks     The new number of 1MB chunks in the balloon.
 */
static int vgsvcBalloonSetUser(uint32_t cNewChunks)
{
    if (cNewChunks == g_cMemBalloonChunks)
        return VINF_SUCCESS;

    VGSvcVerbose(3, "vgsvcBalloonSetUser: cNewChunks=%u g_cMemBalloonChunks=%u\n", cNewChunks, g_cMemBalloonChunks);
    int rc = VINF_SUCCESS;
    if (cNewChunks > g_cMemBalloonChunks)
    {
        /* inflate */
        g_pavBalloon = (void**)RTMemRealloc(g_pavBalloon, cNewChunks * sizeof(void*));
        uint32_t i;
        for (i = g_cMemBalloonChunks; i < cNewChunks; i++)
        {
            void *pv = VGSvcBalloonAllocChunk();
            if (!pv)
                break;
            rc = VbglR3MemBalloonChange(pv, /* inflate=*/ true);
            if (RT_SUCCESS(rc))
            {
                g_pavBalloon[i] = pv;
#ifndef RT_OS_SOLARIS
                /*
                 * Protect against access by dangling pointers (ignore errors as it may fail).
                 * On Solaris it corrupts the address space leaving the process unkillable. This
                 * could perhaps be related to what the underlying segment driver does; currently
                 * just disable it.
                 */
                RTMemProtect(pv, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, RTMEM_PROT_NONE);
#endif
                g_cMemBalloonChunks++;
            }
            else
            {
                vgsvcBalloonFreeChunk(pv);
                break;
            }
        }
        VGSvcVerbose(3, "vgsvcBalloonSetUser: inflation complete. chunks=%u rc=%d\n", i, rc);
    }
    else
    {
        /* deflate */
        uint32_t i;
        for (i = g_cMemBalloonChunks; i-- > cNewChunks;)
        {
            void *pv = g_pavBalloon[i];
            rc = VbglR3MemBalloonChange(pv, /* inflate=*/ false);
            if (RT_SUCCESS(rc))
            {
#ifndef RT_OS_SOLARIS
                /* unprotect */
                RTMemProtect(pv, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
#endif
                vgsvcBalloonFreeChunk(pv);
                g_pavBalloon[i] = NULL;
                g_cMemBalloonChunks--;
            }
            else
                break;
            VGSvcVerbose(3, "vgsvcBalloonSetUser: deflation complete. chunks=%u rc=%d\n", i, rc);
        }
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnInit}
 */
static DECLCALLBACK(int) vgsvcBalloonInit(void)
{
    VGSvcVerbose(3, "vgsvcBalloonInit\n");

    int rc = RTSemEventMultiCreate(&g_MemBalloonEvent);
    AssertRCReturn(rc, rc);

    vgsvcBalloonInitMadvise();

    g_cMemBalloonChunks = 0;
    uint32_t cNewChunks = 0;
    bool fHandleInR3;

    /* Check balloon size */
    rc = VbglR3MemBalloonRefresh(&cNewChunks, &fHandleInR3);
    if (RT_SUCCESS(rc))
    {
        VGSvcVerbose(3, "MemBalloon: New balloon size %d MB (%s memory)\n", cNewChunks, fHandleInR3 ? "R3" : "R0");
        if (fHandleInR3)
            rc = vgsvcBalloonSetUser(cNewChunks);
        else
            g_cMemBalloonChunks = cNewChunks;
    }
    if (RT_FAILURE(rc))
    {
        /* If the service was not found, we disable this service without
           causing VBoxService to fail. */
        if (   rc == VERR_NOT_IMPLEMENTED
#ifdef RT_OS_WINDOWS /** @todo r=bird: Windows kernel driver should return VERR_NOT_IMPLEMENTED,
                      *  VERR_INVALID_PARAMETER has too many other uses. */
            || rc == VERR_INVALID_PARAMETER
#endif
            )
        {
            VGSvcVerbose(0, "MemBalloon: Memory ballooning support is not available\n");
            rc = VERR_SERVICE_DISABLED;
        }
        else
        {
            VGSvcVerbose(3, "MemBalloon: VbglR3MemBalloonRefresh failed with %Rrc\n", rc);
            rc = VERR_SERVICE_DISABLED; /** @todo Playing safe for now, figure out the exact status codes here. */
        }
        RTSemEventMultiDestroy(g_MemBalloonEvent);
        g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}


/**
 * Query the size of the memory balloon, given as a page count.
 *
 * @returns Number of pages.
 * @param   cbPage          The page size.
 */
uint32_t VGSvcBalloonQueryPages(uint32_t cbPage)
{
    Assert(cbPage > 0);
    return g_cMemBalloonChunks * (VMMDEV_MEMORY_BALLOON_CHUNK_SIZE / cbPage);
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnWorker}
 */
static DECLCALLBACK(int) vgsvcBalloonWorker(bool volatile *pfShutdown)
{
    /* Start monitoring of the stat event change event. */
    int rc = VbglR3CtlFilterMask(VMMDEV_EVENT_BALLOON_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        VGSvcVerbose(3, "vgsvcBalloonInit: VbglR3CtlFilterMask failed with %Rrc\n", rc);
        return rc;
    }

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        uint32_t fEvents = 0;

        /* Check if an update interval change is pending. */
        rc = VbglR3WaitEvent(VMMDEV_EVENT_BALLOON_CHANGE_REQUEST, 0 /* no wait */, &fEvents);
        if (    RT_SUCCESS(rc)
            &&  (fEvents & VMMDEV_EVENT_BALLOON_CHANGE_REQUEST))
        {
            uint32_t cNewChunks;
            bool fHandleInR3;
            rc = VbglR3MemBalloonRefresh(&cNewChunks, &fHandleInR3);
            if (RT_SUCCESS(rc))
            {
                VGSvcVerbose(3, "vgsvcBalloonInit: new balloon size %d MB (%s memory)\n", cNewChunks, fHandleInR3 ? "R3" : "R0");
                if (fHandleInR3)
                {
                    rc = vgsvcBalloonSetUser(cNewChunks);
                    if (RT_FAILURE(rc))
                    {
                        VGSvcVerbose(3, "vgsvcBalloonInit: failed to set balloon size %d MB (%s memory)\n",
                                     cNewChunks, fHandleInR3 ? "R3" : "R0");
                    }
                    else
                        VGSvcVerbose(3, "vgsvcBalloonInit: successfully set requested balloon size %d.\n", cNewChunks);
                }
                else
                    g_cMemBalloonChunks = cNewChunks;
            }
            else
                VGSvcVerbose(3, "vgsvcBalloonInit: VbglR3MemBalloonRefresh failed with %Rrc\n", rc);
        }

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_MemBalloonEvent, 5000);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VGSvcError("vgsvcBalloonInit: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    /* Cancel monitoring of the memory balloon change event. */
    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_BALLOON_CHANGE_REQUEST);
    if (RT_FAILURE(rc))
        VGSvcVerbose(3, "vgsvcBalloonInit: VbglR3CtlFilterMask failed with %Rrc\n", rc);

    VGSvcVerbose(3, "vgsvcBalloonInit: finished mem balloon change request thread\n");
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnStop}
 */
static DECLCALLBACK(void) vgsvcBalloonStop(void)
{
    RTSemEventMultiSignal(g_MemBalloonEvent);
}


/**
 * @interface_method_impl{VBOXSERVICE,pfnTerm}
 */
static DECLCALLBACK(void) vgsvcBalloonTerm(void)
{
    if (g_MemBalloonEvent != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(g_MemBalloonEvent);
        g_MemBalloonEvent = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * The 'memballoon' service description.
 */
VBOXSERVICE g_MemBalloon =
{
    /* pszName. */
    "memballoon",
    /* pszDescription. */
    "Memory Ballooning",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VGSvcDefaultPreInit,
    VGSvcDefaultOption,
    vgsvcBalloonInit,
    vgsvcBalloonWorker,
    vgsvcBalloonStop,
    vgsvcBalloonTerm
};
