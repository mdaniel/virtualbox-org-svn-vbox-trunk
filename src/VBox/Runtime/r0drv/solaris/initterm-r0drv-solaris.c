/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include "internal/initterm.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Kernel debug info handle. */
RTDBGKRNLINFO                   g_hKrnlDbgInfo;
/** Indicates that the spl routines (and therefore a bunch of other ones too)
 * will set EFLAGS::IF and break code that disables interrupts.  */
bool g_frtSolSplSetsEIF = false;
/** timeout_generic address. */
PFNSOL_timeout_generic          g_pfnrtR0Sol_timeout_generic = NULL;
/** untimeout_generic address. */
PFNSOL_untimeout_generic        g_pfnrtR0Sol_untimeout_generic = NULL;
/** cyclic_reprogram address. */
PFNSOL_cyclic_reprogram         g_pfnrtR0Sol_cyclic_reprogram = NULL;
/** page_noreloc_supported address. */
PFNSOL_page_noreloc_supported   g_pfnrtR0Sol_page_noreloc_supported = NULL;
/** Whether to use the kernel page freelist. */
bool                            g_frtSolUseKflt = false;
/** Whether we've completed R0 initialization. */
bool                            g_frtSolInitDone = false;
/** Whether to use old-style xc_call interface. */
bool                            g_frtSolOldIPI = false;
/** Whether to use old-style xc_call interface using one ulong_t as the CPU set
 *  representation. */
bool                            g_frtSolOldIPIUlong = false;
/** The xc_call callout table structure. */
RTR0FNSOLXCCALL                 g_rtSolXcCall;
/** Whether to use the old-style installctx()/removectx() routines. */
bool                            g_frtSolOldThreadCtx = false;
/** The thread-context hooks callout table structure. */
RTR0FNSOLTHREADCTX              g_rtSolThreadCtx;
/** Thread preemption offset in the thread structure. */
size_t                          g_offrtSolThreadPreempt;
/** Thread ID offset in the thread structure. */
size_t                          g_offrtSolThreadId;
/** The interrupt (pinned) thread pointer offset in the thread structure.  */
size_t                          g_offrtSolThreadIntrThread;
/** The dispatcher lock pointer offset in the thread structure. */
size_t                          g_offrtSolThreadLock;
/** The process pointer offset in the thread structure. */
size_t                          g_offrtSolThreadProc;
/** Host scheduler preemption offset. */
size_t                          g_offrtSolCpuPreempt;
/** Host scheduler force preemption offset. */
size_t                          g_offrtSolCpuForceKernelPreempt;
/* Resolve using dl_lookup (remove if no longer relevant for supported S10 versions) */
extern void contig_free(void *addr, size_t size);
#pragma weak contig_free
/** contig_free address. */
PFNSOL_contig_free              g_pfnrtR0Sol_contig_free = contig_free;

DECLHIDDEN(int) rtR0InitNative(void)
{
    /*
     * IPRT has not yet been initialized at this point, so use Solaris' native cmn_err() for logging.
     */
    int rc = RTR0DbgKrnlInfoOpen(&g_hKrnlDbgInfo, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
    {
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
        /*
         * Detect whether spl*() is preserving the interrupt flag or not.
         * This is a problem on S10.
         */
        RTCCUINTREG uOldFlags = ASMIntDisableFlags();
        int iOld = splr(DISP_LEVEL);
        if (ASMIntAreEnabled())
            g_frtSolSplSetsEIF = true;
        splx(iOld);
        if (ASMIntAreEnabled())
            g_frtSolSplSetsEIF = true;
        ASMSetFlags(uOldFlags);
#else
        /* PORTME: See if the amd64/x86 problem applies to this architecture. */
#endif
        /*
         * Mandatory: Preemption offsets.
         */
        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "cpu_t", "cpu_runrun", &g_offrtSolCpuPreempt);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find cpu_t::cpu_runrun!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "cpu_t", "cpu_kprunrun", &g_offrtSolCpuForceKernelPreempt);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find cpu_t::cpu_kprunrun!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "kthread_t", "t_preempt", &g_offrtSolThreadPreempt);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find kthread_t::t_preempt!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "kthread_t", "t_did", &g_offrtSolThreadId);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find kthread_t::t_did!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "kthread_t", "t_intr", &g_offrtSolThreadIntrThread);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find kthread_t::t_intr!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "kthread_t", "t_lockp", &g_offrtSolThreadLock);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find kthread_t::t_lockp!\n");
            goto errorbail;
        }

        rc = RTR0DbgKrnlInfoQueryMember(g_hKrnlDbgInfo, NULL, "kthread_t", "t_procp", &g_offrtSolThreadProc);
        if (RT_FAILURE(rc))
        {
            cmn_err(CE_NOTE, "Failed to find kthread_t::t_procp!\n");
            goto errorbail;
        }
        cmn_err(CE_CONT, "!cpu_t::cpu_runrun @ 0x%lx (%ld)\n",    g_offrtSolCpuPreempt, g_offrtSolCpuPreempt);
        cmn_err(CE_CONT, "!cpu_t::cpu_kprunrun @ 0x%lx (%ld)\n",  g_offrtSolCpuForceKernelPreempt, g_offrtSolCpuForceKernelPreempt);
        cmn_err(CE_CONT, "!kthread_t::t_preempt @ 0x%lx (%ld)\n", g_offrtSolThreadPreempt, g_offrtSolThreadPreempt);
        cmn_err(CE_CONT, "!kthread_t::t_did @ 0x%lx (%ld)\n",     g_offrtSolThreadId, g_offrtSolThreadId);
        cmn_err(CE_CONT, "!kthread_t::t_intr @ 0x%lx (%ld)\n",    g_offrtSolThreadIntrThread, g_offrtSolThreadIntrThread);
        cmn_err(CE_CONT, "!kthread_t::t_lockp @ 0x%lx (%ld)\n",   g_offrtSolThreadLock, g_offrtSolThreadLock);
        cmn_err(CE_CONT, "!kthread_t::t_procp @ 0x%lx (%ld)\n",   g_offrtSolThreadProc, g_offrtSolThreadProc);

        /*
         * Mandatory: CPU cross call infrastructure. Refer the-solaris-kernel.h for details.
         */
        rc = RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "xc_init_cpu", NULL /* ppvSymbol */);
        if (RT_SUCCESS(rc))
        {
            if (ncpus > IPRT_SOL_NCPUS)
            {
                cmn_err(CE_NOTE, "rtR0InitNative: CPU count mismatch! ncpus=%d IPRT_SOL_NCPUS=%d\n", ncpus, IPRT_SOL_NCPUS);
                rc = VERR_NOT_SUPPORTED;
                goto errorbail;
            }
            g_rtSolXcCall.u.pfnSol_xc_call = (void *)xc_call;
        }
        else
        {
            g_frtSolOldIPI = true;
            g_rtSolXcCall.u.pfnSol_xc_call_old = (void *)xc_call;
            if (max_cpuid + 1 == sizeof(ulong_t) * 8)
            {
                g_frtSolOldIPIUlong = true;
                g_rtSolXcCall.u.pfnSol_xc_call_old_ulong = (void *)xc_call;
            }
            else if (max_cpuid + 1 != IPRT_SOL_NCPUS)
            {
                cmn_err(CE_NOTE, "rtR0InitNative: cpuset_t size mismatch! max_cpuid=%d IPRT_SOL_NCPUS=%d\n", max_cpuid,
                        IPRT_SOL_NCPUS);
                rc = VERR_NOT_SUPPORTED;
                goto errorbail;
            }
        }

        /*
         * Mandatory: Thread-context hooks.
         */
        rc = RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "exitctx",  NULL /* ppvSymbol */);
        if (RT_SUCCESS(rc))
        {
            g_rtSolThreadCtx.Install.pfnSol_installctx = (void *)installctx;
            g_rtSolThreadCtx.Remove.pfnSol_removectx   = (void *)removectx;
        }
        else
        {
            g_frtSolOldThreadCtx = true;
            g_rtSolThreadCtx.Install.pfnSol_installctx_old = (void *)installctx;
            g_rtSolThreadCtx.Remove.pfnSol_removectx_old   = (void *)removectx;
        }

        /*
         * Optional: Timeout hooks.
         */
        RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "timeout_generic",
                                   (void **)&g_pfnrtR0Sol_timeout_generic);
        RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "untimeout_generic",
                                   (void **)&g_pfnrtR0Sol_untimeout_generic);
        if ((g_pfnrtR0Sol_timeout_generic == NULL) != (g_pfnrtR0Sol_untimeout_generic == NULL))
        {
            static const char *s_apszFn[2] = { "timeout_generic", "untimeout_generic" };
            bool iMissingFn = g_pfnrtR0Sol_timeout_generic == NULL;
            cmn_err(CE_NOTE, "rtR0InitNative: Weird! Found %s but not %s!\n", s_apszFn[!iMissingFn], s_apszFn[iMissingFn]);
            g_pfnrtR0Sol_timeout_generic   = NULL;
            g_pfnrtR0Sol_untimeout_generic = NULL;
        }
        RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "cyclic_reprogram",
                                   (void **)&g_pfnrtR0Sol_cyclic_reprogram);

        /*
         * Optional: Querying page no-relocation support.
         */
        RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /*pszModule */, "page_noreloc_supported",
                                   (void **)&g_pfnrtR0Sol_page_noreloc_supported);

        /*
         * Weak binding failures: contig_free
         */
        if (g_pfnrtR0Sol_contig_free == NULL)
        {
            rc = RTR0DbgKrnlInfoQuerySymbol(g_hKrnlDbgInfo, NULL /* pszModule */, "contig_free",
                                            (void **)&g_pfnrtR0Sol_contig_free);
            if (RT_FAILURE(rc))
            {
                cmn_err(CE_NOTE, "rtR0InitNative: failed to find contig_free!\n");
                goto errorbail;
            }
        }

        g_frtSolInitDone = true;
        return VINF_SUCCESS;
    }
    else
    {
        cmn_err(CE_NOTE, "RTR0DbgKrnlInfoOpen failed. rc=%d\n", rc);
        return rc;
    }

errorbail:
    RTR0DbgKrnlInfoRelease(g_hKrnlDbgInfo);
    return rc;
}


DECLHIDDEN(void) rtR0TermNative(void)
{
    RTR0DbgKrnlInfoRelease(g_hKrnlDbgInfo);
    g_frtSolInitDone = false;
}

