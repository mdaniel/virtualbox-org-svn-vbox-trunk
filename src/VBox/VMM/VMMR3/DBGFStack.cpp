/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Call Stack Analyser.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/selm.h>
#include <VBox/vmm/mm.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/alloca.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/formats/pecoff.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Magic value for DBGFUNWINDSTATE::u32Magic (James Moody). */
#define DBGFUNWINDSTATE_MAGIC           UINT32_C(0x19250326)
/** Magic value for DBGFUNWINDSTATE::u32Magic after use. */
#define DBGFUNWINDSTATE_MAGIC_DEAD      UINT32_C(0x20101209)

/**
 * Register state.
 */
typedef struct DBGFUNWINDSTATE
{
    /** Structure magic (DBGFUNWINDSTATE_MAGIC) */
    uint32_t            u32Magic;
    /** The state architecture. */
    RTLDRARCH           enmArch;

    /** The program counter register.
     * amd64/x86: RIP/EIP/IP
     * sparc: PC
     * arm32: PC / R15
     */
    uint64_t            uPc;

    /** Return type. */
    DBGFRETRUNTYPE      enmRetType;

    /** Register state (see enmArch). */
    union
    {
        /** RTLDRARCH_AMD64, RTLDRARCH_X86_32 and RTLDRARCH_X86_16. */
        struct
        {
            /** General purpose registers indexed by X86_GREG_XXX. */
            uint64_t    auRegs[16];
            /** The frame address. */
            RTFAR64     FrameAddr;
            /** Set if we're in real or virtual 8086 mode. */
            bool        fRealOrV86;
            /** The flags register. */
            uint64_t    uRFlags;
            /** Trap error code. */
            uint64_t    uErrCd;
            /** Segment registers (indexed by X86_SREG_XXX). */
            uint16_t    auSegs[6];

            /** Bitmap tracking register we've loaded and which content can possibly be trusted. */
            union
            {
                /** For effective clearing of the bits. */
                uint32_t    fAll;
                /** Detailed view. */
                struct
                {
                    /** Bitmap indicating whether a GPR was loaded (parallel to auRegs). */
                    uint16_t    fRegs;
                    /** Bitmap indicating whether a segment register was loaded (parallel to auSegs). */
                    uint8_t     fSegs;
                    /** Set if uPc was loaded. */
                    uint8_t     fPc : 1;
                    /** Set if FrameAddr was loaded. */
                    uint8_t     fFrameAddr : 1;
                    /** Set if uRFlags was loaded. */
                    uint8_t     fRFlags : 1;
                    /** Set if uErrCd was loaded. */
                    uint8_t     fErrCd : 1;
                } s;
            } Loaded;
        } x86;

        /** @todo add ARM and others as needed. */
    } u;

    /**
     * Stack read callback.
     *
     * @returns IPRT status code.
     * @param   pThis       Pointer to this structure.
     * @param   uSp         The stack pointer address.
     * @param   cbToRead    The number of bytes to read.
     * @param   pvDst       Where to put the bytes we read.
     */
    DECLCALLBACKMEMBER(int, pfnReadStack)(struct DBGFUNWINDSTATE *pThis, RTUINTPTR uSp, size_t cbToRead, void *pvDst);
    /** User argument (usefule for pfnReadStack). */
    void               *pvUser;

} DBGFUNWINDSTATE;
typedef struct DBGFUNWINDSTATE *PDBGFUNWINDSTATE;
typedef struct DBGFUNWINDSTATE const *PCDBGFUNWINDSTATE;

static DECLCALLBACK(int) dbgfR3StackReadCallback(PDBGFUNWINDSTATE pThis, RTUINTPTR uSp, size_t cbToRead, void *pvDst);


/**
 * Unwind context.
 *
 * @note Using a constructor and destructor here for simple+safe cleanup.
 *
 * @todo Generalize and move to IPRT or some such place.
 */
typedef struct DBGFUNWINDCTX
{
    PUVM        m_pUVM;
    VMCPUID     m_idCpu;
    RTDBGAS     m_hAs;

    DBGFUNWINDSTATE m_State;

    RTDBGMOD    m_hCached;
    RTUINTPTR   m_uCachedMapping;
    RTUINTPTR   m_cbCachedMapping;
    uint8_t    *m_pbCachedInfo;
    size_t      m_cbCachedInfo;

    /** Function table for PE/AMD64 (entire m_pbCachedInfo) . */
    PCIMAGE_RUNTIME_FUNCTION_ENTRY  m_paFunctions;
    /** Number functions in m_paFunctions. */
    size_t                          m_cFunctions;

    DBGFUNWINDCTX(PUVM pUVM, VMCPUID idCpu, PCCPUMCTX pInitialCtx, RTDBGAS hAs)
    {
        m_State.u32Magic     = DBGFUNWINDSTATE_MAGIC;
        m_State.enmArch      = RTLDRARCH_AMD64;
        m_State.pfnReadStack = dbgfR3StackReadCallback;
        m_State.pvUser       = this;
        RT_ZERO(m_State.u);
        if (pInitialCtx)
        {
            m_State.u.x86.auRegs[X86_GREG_xAX] = pInitialCtx->rax;
            m_State.u.x86.auRegs[X86_GREG_xCX] = pInitialCtx->rcx;
            m_State.u.x86.auRegs[X86_GREG_xDX] = pInitialCtx->rdx;
            m_State.u.x86.auRegs[X86_GREG_xBX] = pInitialCtx->rbx;
            m_State.u.x86.auRegs[X86_GREG_xSP] = pInitialCtx->rsp;
            m_State.u.x86.auRegs[X86_GREG_xBP] = pInitialCtx->rbp;
            m_State.u.x86.auRegs[X86_GREG_xSI] = pInitialCtx->rsi;
            m_State.u.x86.auRegs[X86_GREG_xDI] = pInitialCtx->rdi;
            m_State.u.x86.auRegs[X86_GREG_x8 ] = pInitialCtx->r8;
            m_State.u.x86.auRegs[X86_GREG_x9 ] = pInitialCtx->r9;
            m_State.u.x86.auRegs[X86_GREG_x10] = pInitialCtx->r10;
            m_State.u.x86.auRegs[X86_GREG_x11] = pInitialCtx->r11;
            m_State.u.x86.auRegs[X86_GREG_x12] = pInitialCtx->r12;
            m_State.u.x86.auRegs[X86_GREG_x13] = pInitialCtx->r13;
            m_State.u.x86.auRegs[X86_GREG_x14] = pInitialCtx->r14;
            m_State.u.x86.auRegs[X86_GREG_x15] = pInitialCtx->r15;
            m_State.uPc                        = pInitialCtx->rip;
            m_State.u.x86.uRFlags              = pInitialCtx->rflags.u;
            m_State.u.x86.auSegs[X86_SREG_ES]  = pInitialCtx->es.Sel;
            m_State.u.x86.auSegs[X86_SREG_CS]  = pInitialCtx->cs.Sel;
            m_State.u.x86.auSegs[X86_SREG_SS]  = pInitialCtx->ss.Sel;
            m_State.u.x86.auSegs[X86_SREG_DS]  = pInitialCtx->ds.Sel;
            m_State.u.x86.auSegs[X86_SREG_GS]  = pInitialCtx->gs.Sel;
            m_State.u.x86.auSegs[X86_SREG_FS]  = pInitialCtx->fs.Sel;
            m_State.u.x86.fRealOrV86           = CPUMIsGuestInRealOrV86ModeEx(pInitialCtx);
        }

        m_pUVM            = pUVM;
        m_idCpu           = idCpu;
        m_hAs             = DBGFR3AsResolveAndRetain(pUVM, hAs);

        m_hCached         = NIL_RTDBGMOD;
        m_uCachedMapping  = 0;
        m_cbCachedMapping = 0;
        m_pbCachedInfo    = NULL;
        m_cbCachedInfo    = 0;
        m_paFunctions     = NULL;
        m_cFunctions      = 0;
    }

    ~DBGFUNWINDCTX();

} DBGFUNWINDCTX;
/** Pointer to unwind context. */
typedef DBGFUNWINDCTX *PDBGFUNWINDCTX;


static void dbgfR3UnwindCtxFlushCache(PDBGFUNWINDCTX pUnwindCtx)
{
    if (pUnwindCtx->m_hCached != NIL_RTDBGMOD)
    {
        RTDbgModRelease(pUnwindCtx->m_hCached);
        pUnwindCtx->m_hCached = NIL_RTDBGMOD;
    }
    if (pUnwindCtx->m_pbCachedInfo)
    {
        RTMemFree(pUnwindCtx->m_pbCachedInfo);
        pUnwindCtx->m_pbCachedInfo = NULL;
    }
    pUnwindCtx->m_cbCachedInfo = 0;
    pUnwindCtx->m_paFunctions  = NULL;
    pUnwindCtx->m_cFunctions   = 0;
}


DBGFUNWINDCTX::~DBGFUNWINDCTX()
{
    dbgfR3UnwindCtxFlushCache(this);
    if (m_hAs != NIL_RTDBGAS)
    {
        RTDbgAsRelease(m_hAs);
        m_hAs = NIL_RTDBGAS;
    }
}


/**
 * @interface_method_impl{DBGFUNWINDSTATE,pfnReadStack}
 */
static DECLCALLBACK(int) dbgfR3StackReadCallback(PDBGFUNWINDSTATE pThis, RTUINTPTR uSp, size_t cbToRead, void *pvDst)
{
    Assert(   pThis->enmArch == RTLDRARCH_AMD64
           || pThis->enmArch == RTLDRARCH_X86_32);

    PDBGFUNWINDCTX pUnwindCtx = (PDBGFUNWINDCTX)pThis->pvUser;
    DBGFADDRESS SrcAddr;
    int rc = VINF_SUCCESS;
    if (   pThis->enmArch == RTLDRARCH_X86_32
        || pThis->enmArch == RTLDRARCH_X86_16)
    {
        if (!pThis->u.x86.fRealOrV86)
            rc = DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &SrcAddr, pThis->u.x86.auSegs[X86_SREG_SS], uSp);
        else
            DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &SrcAddr, uSp + ((uint32_t)pThis->u.x86.auSegs[X86_SREG_SS] << 4));
    }
    else
        DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &SrcAddr, uSp);
    if (RT_SUCCESS(rc))
        rc = DBGFR3MemRead(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &SrcAddr, pvDst, cbToRead);
    return rc;
}


/**
 * Sets PC and SP.
 *
 * @returns true.
 * @param   pUnwindCtx          The unwind context.
 * @param   pAddrPC             The program counter (PC) value to set.
 * @param   pAddrStack          The stack pointer (SP) value to set.
 */
static bool dbgfR3UnwindCtxSetPcAndSp(PDBGFUNWINDCTX pUnwindCtx, PCDBGFADDRESS pAddrPC, PCDBGFADDRESS pAddrStack)
{
    Assert(   pUnwindCtx->m_State.enmArch == RTLDRARCH_AMD64
           || pUnwindCtx->m_State.enmArch == RTLDRARCH_X86_32);

    if (!DBGFADDRESS_IS_FAR(pAddrPC))
        pUnwindCtx->m_State.uPc = pAddrPC->FlatPtr;
    else
    {
        pUnwindCtx->m_State.uPc                       = pAddrPC->off;
        pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_CS] = pAddrPC->Sel;
    }
    if (!DBGFADDRESS_IS_FAR(pAddrStack))
        pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xSP] = pAddrStack->FlatPtr;
    else
    {
        pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xSP] = pAddrStack->off;
        pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_SS]  = pAddrStack->Sel;
    }
    return true;
}


/**
 * Try read a 16-bit value off the stack.
 *
 * @returns pfnReadStack result.
 * @param   pUnwindCtx      The unwind context.
 * @param   uSrcAddr        The stack address.
 * @param   puDst           The read destination.
 */
DECLINLINE(int) dbgUnwindLoadStackU16(PDBGFUNWINDSTATE pThis, uint64_t uSrcAddr, uint16_t *puDst)
{
    return pThis->pfnReadStack(pThis, uSrcAddr, sizeof(*puDst), puDst);
}


/**
 * Try read a 64-bit value off the stack.
 *
 * @returns pfnReadStack result.
 * @param   pUnwindCtx      The unwind context.
 * @param   uSrcAddr        The stack address.
 * @param   puDst           The read destination.
 */
DECLINLINE(int) dbgUnwindLoadStackU64(PDBGFUNWINDSTATE pThis, uint64_t uSrcAddr, uint64_t *puDst)
{
    return pThis->pfnReadStack(pThis, uSrcAddr, sizeof(*puDst), puDst);
}


/**
 * Binary searches the lookup table.
 *
 * @returns RVA of unwind info on success, UINT32_MAX on failure.
 * @param   paFunctions     The table to lookup @a uRva in.
 * @param   iEnd            Size of the table.
 * @param   uRva            The RVA of the function we want.
 */
DECLINLINE(PCIMAGE_RUNTIME_FUNCTION_ENTRY)
dbgfR3UnwindCtxLookupUnwindInfoRva(PCIMAGE_RUNTIME_FUNCTION_ENTRY paFunctions, size_t iEnd, uint32_t uRva)
{
    size_t iBegin = 0;
    while (iBegin < iEnd)
    {
        size_t const i = iBegin  + (iEnd - iBegin) / 2;
        PCIMAGE_RUNTIME_FUNCTION_ENTRY pEntry = &paFunctions[i];
        if (uRva < pEntry->BeginAddress)
            iEnd = i;
        else if (uRva > pEntry->EndAddress)
            iBegin = i + 1;
        else
            return pEntry;
    }
    return NULL;
}


/**
 * Processes an IRET frame.
 *
 * @returns true.
 * @param   pThis           The unwind state being worked.
 * @param   fErrCd          Non-zero if there is an error code on the stack.
 */
static bool dbgUnwindPeAmd64DoOneIRet(PDBGFUNWINDSTATE pThis, uint8_t fErrCd)
{
    Assert(fErrCd <= 1);
    if (!fErrCd)
        pThis->u.x86.Loaded.s.fErrCd = 0;
    else
    {
        pThis->u.x86.uErrCd = 0;
        pThis->u.x86.Loaded.s.fErrCd = 1;
        dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.uErrCd);
        pThis->u.x86.auRegs[X86_GREG_xSP] += 8;
    }

    pThis->enmRetType = DBGFRETURNTYPE_IRET64;
    pThis->u.x86.FrameAddr.off = pThis->u.x86.auRegs[X86_GREG_xSP] - /* pretend rbp is pushed on the stack */ 8;
    pThis->u.x86.FrameAddr.sel = pThis->u.x86.auSegs[X86_SREG_SS];

    dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->uPc);
    pThis->u.x86.auRegs[X86_GREG_xSP] += 8; /* RIP */

    dbgUnwindLoadStackU16(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.auSegs[X86_SREG_CS]);
    pThis->u.x86.auRegs[X86_GREG_xSP] += 8; /* CS */

    dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.uRFlags);
    pThis->u.x86.auRegs[X86_GREG_xSP] += 8; /* EFLAGS */

    uint64_t uNewRsp = (pThis->u.x86.auRegs[X86_GREG_xSP] - 8) & ~(uint64_t)15;
    dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &uNewRsp);
    pThis->u.x86.auRegs[X86_GREG_xSP] += 8; /* RSP */

    dbgUnwindLoadStackU16(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.auSegs[X86_SREG_SS]);
    pThis->u.x86.auRegs[X86_GREG_xSP] += 8; /* SS */

    pThis->u.x86.auRegs[X86_GREG_xSP] = uNewRsp;

    pThis->u.x86.Loaded.s.fRegs        |= RT_BIT(X86_GREG_xSP);
    pThis->u.x86.Loaded.s.fSegs        |= RT_BIT(X86_SREG_CS) | RT_BIT(X86_SREG_SS);
    pThis->u.x86.Loaded.s.fPc           = 1;
    pThis->u.x86.Loaded.s.fFrameAddr    = 1;
    pThis->u.x86.Loaded.s.fRFlags       = 1;
    return true;
}


/**
 * Unwinds one frame using cached module info.
 *
 * @returns true on success, false on failure.
 * @param   hMod            The debug module to retrieve unwind info from.
 * @param   paFunctions     The table to lookup @a uRvaRip in.
 * @param   cFunctions      Size of the lookup table.
 * @param   uRvaRip         The RVA of the RIP.
 *
 * @todo Move this down to IPRT in the ldrPE.cpp / dbgmodcodeview.cpp area.
 */
static bool dbgUnwindPeAmd64DoOne(RTDBGMOD hMod, PCIMAGE_RUNTIME_FUNCTION_ENTRY paFunctions, size_t cFunctions,
                                  PDBGFUNWINDSTATE pThis, uint32_t uRvaRip)
{
    /*
     * Lookup the unwind info RVA and try read it.
     */
    PCIMAGE_RUNTIME_FUNCTION_ENTRY pEntry = dbgfR3UnwindCtxLookupUnwindInfoRva(paFunctions, cFunctions, uRvaRip);
    if (pEntry)
    {
        IMAGE_RUNTIME_FUNCTION_ENTRY ChainedEntry;
        unsigned iFrameReg   = ~0U;
        unsigned offFrameReg = 0;

        int      fInEpilog = -1; /* -1: not-determined-assume-false;  0: false;  1: true. */
        uint8_t  cbEpilog  = 0;
        uint8_t  offEpilog = UINT8_MAX;
        for (unsigned cChainLoops = 0; ; cChainLoops++)
        {
            /*
             * Get the info.
             */
            union
            {
                uint32_t uRva;
                uint8_t  ab[  RT_OFFSETOF(IMAGE_UNWIND_INFO, aOpcodes)
                            + sizeof(IMAGE_UNWIND_CODE) * 256
                            + sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY)];
            } uBuf;

            uBuf.uRva = pEntry->UnwindInfoAddress;
            size_t cbBuf = sizeof(uBuf);
            int rc = RTDbgModImageQueryProp(hMod, RTLDRPROP_UNWIND_INFO, &uBuf, cbBuf, &cbBuf);
            if (RT_FAILURE(rc))
                return false;

            /*
             * Check the info.
             */
            ASMCompilerBarrier(); /* we're aliasing */
            PCIMAGE_UNWIND_INFO pInfo = (PCIMAGE_UNWIND_INFO)&uBuf;

            if (pInfo->Version != 1 && pInfo->Version != 2)
                return false;

            /*
             * Execute the opcodes.
             */
            unsigned const cOpcodes = pInfo->CountOfCodes;
            unsigned       iOpcode  = 0;

            /*
             * Check for epilog opcodes at the start and see if we're in an epilog.
             */
            if (   pInfo->Version >= 2
                && iOpcode < cOpcodes
                && pInfo->aOpcodes[iOpcode].u.UnwindOp == IMAGE_AMD64_UWOP_EPILOG)
            {
                if (fInEpilog == -1)
                {
                    cbEpilog = pInfo->aOpcodes[iOpcode].u.CodeOffset;
                    Assert(cbEpilog > 0);

                    uint32_t uRvaEpilog = pEntry->EndAddress - cbEpilog;
                    iOpcode++;
                    if (   (pInfo->aOpcodes[iOpcode - 1].u.OpInfo & 1)
                        && uRvaRip >= uRvaEpilog)
                    {
                        offEpilog = uRvaRip - uRvaEpilog;
                        fInEpilog = 1;
                    }
                    else
                    {
                        fInEpilog = 0;
                        while (iOpcode < cOpcodes && pInfo->aOpcodes[iOpcode].u.UnwindOp == IMAGE_AMD64_UWOP_EPILOG)
                        {
                            uRvaEpilog = pEntry->EndAddress
                                       - (pInfo->aOpcodes[iOpcode].u.CodeOffset + (pInfo->aOpcodes[iOpcode].u.OpInfo << 8));
                            iOpcode++;
                            if (uRvaRip - uRvaEpilog < cbEpilog)
                            {
                                offEpilog = uRvaRip - uRvaEpilog;
                                fInEpilog = 1;
                                break;
                            }
                        }
                    }
                }
                while (iOpcode < cOpcodes && pInfo->aOpcodes[iOpcode].u.UnwindOp == IMAGE_AMD64_UWOP_EPILOG)
                    iOpcode++;
            }
            if (fInEpilog != 1)
            {
                /*
                 * Skip opcodes that doesn't apply to us if we're in the prolog.
                 */
                uint32_t offPc = uRvaRip - pEntry->BeginAddress;
                if (offPc < pInfo->SizeOfProlog)
                    while (iOpcode < cOpcodes && pInfo->aOpcodes[iOpcode].u.CodeOffset > offPc)
                        iOpcode++;

                /*
                 * Execute the opcodes.
                 */
                if (pInfo->FrameRegister != 0)
                {
                    iFrameReg   = pInfo->FrameRegister;
                    offFrameReg = pInfo->FrameOffset * 16;
                }
                while (iOpcode < cOpcodes)
                {
                    Assert(pInfo->aOpcodes[iOpcode].u.CodeOffset <= offPc);
                    uint8_t const uOpInfo   = pInfo->aOpcodes[iOpcode].u.OpInfo;
                    uint8_t const uUnwindOp = pInfo->aOpcodes[iOpcode].u.UnwindOp;
                    switch (uUnwindOp)
                    {
                        case IMAGE_AMD64_UWOP_PUSH_NONVOL:
                            pThis->u.x86.auRegs[X86_GREG_xSP] += 8;
                            dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.auRegs[uOpInfo]);
                            pThis->u.x86.Loaded.s.fRegs |= RT_BIT(uOpInfo);
                            iOpcode++;
                            break;

                        case IMAGE_AMD64_UWOP_ALLOC_LARGE:
                            if (uOpInfo == 0)
                            {
                                iOpcode += 2;
                                AssertBreak(iOpcode <= cOpcodes);
                                pThis->u.x86.auRegs[X86_GREG_xSP] += pInfo->aOpcodes[iOpcode - 1].FrameOffset * 8;
                            }
                            else
                            {
                                iOpcode += 3;
                                AssertBreak(iOpcode <= cOpcodes);
                                pThis->u.x86.auRegs[X86_GREG_xSP] += RT_MAKE_U32(pInfo->aOpcodes[iOpcode - 2].FrameOffset,
                                                                                  pInfo->aOpcodes[iOpcode - 1].FrameOffset);
                            }
                            break;

                        case IMAGE_AMD64_UWOP_ALLOC_SMALL:
                            AssertBreak(iOpcode <= cOpcodes);
                            pThis->u.x86.auRegs[X86_GREG_xSP] += uOpInfo * 8 + 8;
                            iOpcode++;
                            break;

                        case IMAGE_AMD64_UWOP_SET_FPREG:
                            iFrameReg = uOpInfo;
                            offFrameReg = pInfo->FrameOffset * 16;
                            iOpcode++;
                            break;

                        case IMAGE_AMD64_UWOP_SAVE_NONVOL:
                        case IMAGE_AMD64_UWOP_SAVE_NONVOL_FAR:
                        {
                            uint32_t off = 0;
                            iOpcode++;
                            if (iOpcode < cOpcodes)
                            {
                                off = pInfo->aOpcodes[iOpcode].FrameOffset;
                                iOpcode++;
                                if (uUnwindOp == IMAGE_AMD64_UWOP_SAVE_NONVOL_FAR && iOpcode < cOpcodes)
                                {
                                    off |= (uint32_t)pInfo->aOpcodes[iOpcode].FrameOffset << 16;
                                    iOpcode++;
                                }
                            }
                            off *= 8;
                            dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP] + off, &pThis->u.x86.auRegs[uOpInfo]);
                            pThis->u.x86.Loaded.s.fRegs |= RT_BIT(uOpInfo);
                            break;
                        }

                        case IMAGE_AMD64_UWOP_SAVE_XMM128:
                            iOpcode += 2;
                            break;

                        case IMAGE_AMD64_UWOP_SAVE_XMM128_FAR:
                            iOpcode += 3;
                            break;

                        case IMAGE_AMD64_UWOP_PUSH_MACHFRAME:
                            return dbgUnwindPeAmd64DoOneIRet(pThis, uOpInfo);

                        case IMAGE_AMD64_UWOP_EPILOG:
                            iOpcode += 1;
                            break;

                        case IMAGE_AMD64_UWOP_RESERVED_7:
                            AssertFailedReturn(false);

                        default:
                            AssertMsgFailedReturn(("%u\n", uUnwindOp), false);
                    }
                }
            }
            else
            {
                /*
                 * We're in the POP sequence of an epilog.  The POP sequence should
                 * mirror the PUSH sequence exactly.
                 *
                 * Note! We should only end up here for the initial frame (just consider
                 *       RSP, stack allocations, non-volatile register restores, ++).
                 */
                while (iOpcode < cOpcodes)
                {
                    uint8_t const uOpInfo   = pInfo->aOpcodes[iOpcode].u.OpInfo;
                    uint8_t const uUnwindOp = pInfo->aOpcodes[iOpcode].u.UnwindOp;
                    switch (uUnwindOp)
                    {
                        case IMAGE_AMD64_UWOP_PUSH_NONVOL:
                            pThis->u.x86.auRegs[X86_GREG_xSP] += 8;
                            if (offEpilog == 0)
                            {
                                dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->u.x86.auRegs[uOpInfo]);
                                pThis->u.x86.Loaded.s.fRegs |= RT_BIT(uOpInfo);
                            }
                            else
                            {
                                /* Decrement offEpilog by estimated POP instruction length. */
                                offEpilog -= 1;
                                if (offEpilog > 0 && uOpInfo >= 8)
                                    offEpilog -= 1;
                            }
                            iOpcode++;
                            break;

                        case IMAGE_AMD64_UWOP_PUSH_MACHFRAME: /* Must terminate an epilog, so always execute this. */
                            return dbgUnwindPeAmd64DoOneIRet(pThis, uOpInfo);

                        case IMAGE_AMD64_UWOP_ALLOC_SMALL:
                        case IMAGE_AMD64_UWOP_SET_FPREG:
                        case IMAGE_AMD64_UWOP_EPILOG:
                            iOpcode++;
                            break;
                        case IMAGE_AMD64_UWOP_SAVE_NONVOL:
                        case IMAGE_AMD64_UWOP_SAVE_XMM128:
                            iOpcode += 2;
                            break;
                        case IMAGE_AMD64_UWOP_ALLOC_LARGE:
                        case IMAGE_AMD64_UWOP_SAVE_NONVOL_FAR:
                        case IMAGE_AMD64_UWOP_SAVE_XMM128_FAR:
                            iOpcode += 3;
                            break;

                        default:
                            AssertMsgFailedReturn(("%u\n", uUnwindOp), false);
                    }
                }
            }

            /*
             * Chained stuff?
             */
            if (!(pInfo->Flags & IMAGE_UNW_FLAGS_CHAININFO))
                break;
            ChainedEntry = *(PCIMAGE_RUNTIME_FUNCTION_ENTRY)&pInfo->aOpcodes[(cOpcodes + 1) & ~1];
            pEntry = &ChainedEntry;
            AssertReturn(cChainLoops < 32, false);
        }

        /*
         * RSP should now give us the return address, so perform a RET.
         */
        pThis->enmRetType = DBGFRETURNTYPE_NEAR64;

        pThis->u.x86.FrameAddr.off = pThis->u.x86.auRegs[X86_GREG_xSP] - /* pretend rbp is pushed on the stack */ 8;
        pThis->u.x86.FrameAddr.sel = pThis->u.x86.auSegs[X86_SREG_SS];
        pThis->u.x86.Loaded.s.fFrameAddr = 1;

        dbgUnwindLoadStackU64(pThis, pThis->u.x86.auRegs[X86_GREG_xSP], &pThis->uPc);
        pThis->u.x86.auRegs[X86_GREG_xSP] += 8;
        pThis->u.x86.Loaded.s.fPc = 1;
        return true;
    }

    return false;
}


/**
 * Tries to unwind one frame using unwind info.
 *
 * @returns true on success, false on failure.
 * @param   pUnwindCtx      The unwind context.
 */
static bool dbgfR3UnwindCtxDoOneFrame(PDBGFUNWINDCTX pUnwindCtx)
{
    /*
     * Hope for the same module as last time around.
     */
    RTUINTPTR offCache = pUnwindCtx->m_State.uPc - pUnwindCtx->m_uCachedMapping;
    if (offCache < pUnwindCtx->m_cbCachedMapping)
        return dbgUnwindPeAmd64DoOne(pUnwindCtx->m_hCached, pUnwindCtx->m_paFunctions, pUnwindCtx->m_cFunctions,
                                     &pUnwindCtx->m_State, offCache);

    /*
     * Try locate the module.
     */
    RTDBGMOD        hDbgMod = NIL_RTDBGMOD;
    RTUINTPTR       uBase   = 0;
    RTDBGSEGIDX     idxSeg  = NIL_RTDBGSEGIDX;
    int rc = RTDbgAsModuleByAddr(pUnwindCtx->m_hAs, pUnwindCtx->m_State.uPc, &hDbgMod, &uBase, &idxSeg);
    if (RT_SUCCESS(rc))
    {
        /* We cache the module regardless of unwind info. */
        dbgfR3UnwindCtxFlushCache(pUnwindCtx);
        pUnwindCtx->m_hCached         = hDbgMod;
        pUnwindCtx->m_uCachedMapping  = uBase;
        pUnwindCtx->m_cbCachedMapping = idxSeg == NIL_RTDBGSEGIDX ? RTDbgModImageSize(hDbgMod)
                                      : RTDbgModSegmentSize(hDbgMod, idxSeg);

        /* Play simple for now. */
        if (   idxSeg == NIL_RTDBGSEGIDX
            && RTDbgModImageGetFormat(hDbgMod) == RTLDRFMT_PE
            && RTDbgModImageGetArch(hDbgMod)   == RTLDRARCH_AMD64)
        {
            /*
             * Try query the unwind data.
             */
            uint32_t uDummy;
            size_t cbNeeded = 0;
            rc = RTDbgModImageQueryProp(hDbgMod, RTLDRPROP_UNWIND_TABLE, &uDummy, 0, &cbNeeded);
            if (   rc == VERR_BUFFER_OVERFLOW
                && cbNeeded >= sizeof(*pUnwindCtx->m_paFunctions)
                && cbNeeded < _64M)
            {
                void *pvBuf = RTMemAllocZ(cbNeeded + 32);
                if (pvBuf)
                {
                    rc = RTDbgModImageQueryProp(hDbgMod, RTLDRPROP_UNWIND_TABLE, pvBuf, cbNeeded + 32, &cbNeeded);
                    if (RT_SUCCESS(rc))
                    {
                        pUnwindCtx->m_pbCachedInfo = (uint8_t *)pvBuf;
                        pUnwindCtx->m_cbCachedInfo = cbNeeded;
                        pUnwindCtx->m_paFunctions  = (PCIMAGE_RUNTIME_FUNCTION_ENTRY)pvBuf;
                        pUnwindCtx->m_cFunctions   = cbNeeded / sizeof(*pUnwindCtx->m_paFunctions);

                        return dbgUnwindPeAmd64DoOne(pUnwindCtx->m_hCached, pUnwindCtx->m_paFunctions, pUnwindCtx->m_cFunctions,
                                                     &pUnwindCtx->m_State, pUnwindCtx->m_State.uPc - pUnwindCtx->m_uCachedMapping);
                    }
                    RTMemFree(pvBuf);
                }
            }
        }
    }
    return false;
}


/**
 * Read stack memory, will init entire buffer.
 */
DECLINLINE(int) dbgfR3StackRead(PUVM pUVM, VMCPUID idCpu, void *pvBuf, PCDBGFADDRESS pSrcAddr, size_t cb, size_t *pcbRead)
{
    int rc = DBGFR3MemRead(pUVM, idCpu, pSrcAddr, pvBuf, cb);
    if (RT_FAILURE(rc))
    {
        /* fallback: byte by byte and zero the ones we fail to read. */
        size_t cbRead;
        for (cbRead = 0; cbRead < cb; cbRead++)
        {
            DBGFADDRESS Addr = *pSrcAddr;
            rc = DBGFR3MemRead(pUVM, idCpu, DBGFR3AddrAdd(&Addr, cbRead), (uint8_t *)pvBuf + cbRead, 1);
            if (RT_FAILURE(rc))
                break;
        }
        if (cbRead)
            rc = VINF_SUCCESS;
        memset((char *)pvBuf + cbRead, 0, cb - cbRead);
        *pcbRead = cbRead;
    }
    else
        *pcbRead = cb;
    return rc;
}

static int dbgfR3StackWalkCollectRegisterChanges(PUVM pUVM, PDBGFSTACKFRAME pFrame, PDBGFUNWINDSTATE pState)
{
    pFrame->cSureRegs  = 0;
    pFrame->paSureRegs = NULL;

    if (   pState->enmArch == RTLDRARCH_AMD64
        || pState->enmArch == RTLDRARCH_X86_32
        || pState->enmArch == RTLDRARCH_X86_16)
    {
        if (pState->u.x86.Loaded.fAll)
        {
            /*
             * Count relevant registers.
             */
            uint32_t cRegs = 0;
            if (pState->u.x86.Loaded.s.fRegs)
                for (uint32_t f = 1; f < RT_BIT_32(RT_ELEMENTS(pState->u.x86.auRegs)); f <<= 1)
                    if (pState->u.x86.Loaded.s.fRegs & f)
                        cRegs++;
            if (pState->u.x86.Loaded.s.fSegs)
                for (uint32_t f = 1; f < RT_BIT_32(RT_ELEMENTS(pState->u.x86.auSegs)); f <<= 1)
                    if (pState->u.x86.Loaded.s.fSegs & f)
                        cRegs++;
            if (pState->u.x86.Loaded.s.fRFlags)
                cRegs++;
            if (pState->u.x86.Loaded.s.fErrCd)
                cRegs++;
            if (cRegs > 0)
            {
                /*
                 * Allocate the arrays.
                 */
                PDBGFREGVALEX paSureRegs = (PDBGFREGVALEX)MMR3HeapAllocZU(pUVM, MM_TAG_DBGF_STACK, sizeof(DBGFREGVALEX) * cRegs);
                AssertReturn(paSureRegs, VERR_NO_MEMORY);
                pFrame->paSureRegs = paSureRegs;
                pFrame->cSureRegs  = cRegs;

                /*
                 * Popuplate the arrays.
                 */
                uint32_t iReg = 0;
                if (pState->u.x86.Loaded.s.fRegs)
                    for (uint32_t i = 1; i < RT_ELEMENTS(pState->u.x86.auRegs); i++)
                        if (pState->u.x86.Loaded.s.fRegs & RT_BIT(i))
                        {
                            paSureRegs[iReg].Value.u64 = pState->u.x86.auRegs[i];
                            paSureRegs[iReg].enmType   = DBGFREGVALTYPE_U64;
                            paSureRegs[iReg].enmReg    = (DBGFREG)(DBGFREG_RAX + i);
                            iReg++;
                        }

                if (pState->u.x86.Loaded.s.fSegs)
                    for (uint32_t i = 1; i < RT_ELEMENTS(pState->u.x86.auSegs); i++)
                        if (pState->u.x86.Loaded.s.fSegs & RT_BIT(i))
                        {
                            paSureRegs[iReg].Value.u16 = pState->u.x86.auSegs[i];
                            paSureRegs[iReg].enmType   = DBGFREGVALTYPE_U16;
                            switch (i)
                            {
                                case X86_SREG_ES: paSureRegs[iReg].enmReg = DBGFREG_ES; break;
                                case X86_SREG_CS: paSureRegs[iReg].enmReg = DBGFREG_CS; break;
                                case X86_SREG_SS: paSureRegs[iReg].enmReg = DBGFREG_SS; break;
                                case X86_SREG_DS: paSureRegs[iReg].enmReg = DBGFREG_DS; break;
                                case X86_SREG_FS: paSureRegs[iReg].enmReg = DBGFREG_FS; break;
                                case X86_SREG_GS: paSureRegs[iReg].enmReg = DBGFREG_GS; break;
                                default:          AssertFailedBreak();
                            }
                            iReg++;
                        }

                if (iReg < cRegs)
                {
                    if (pState->u.x86.Loaded.s.fRFlags)
                    {
                        paSureRegs[iReg].Value.u64 = pState->u.x86.uRFlags;
                        paSureRegs[iReg].enmType   = DBGFREGVALTYPE_U64;
                        paSureRegs[iReg].enmReg    = DBGFREG_RFLAGS;
                        iReg++;
                    }
                    if (pState->u.x86.Loaded.s.fErrCd)
                    {
                        paSureRegs[iReg].Value.u64 = pState->u.x86.uErrCd;
                        paSureRegs[iReg].enmType   = DBGFREGVALTYPE_U64;
                        paSureRegs[iReg].enmReg    = DBGFREG_END;
                        paSureRegs[iReg].pszName   = "trap-errcd";
                        iReg++;
                    }
                }
                Assert(iReg == cRegs);
            }
        }
    }

    return VINF_SUCCESS;
}


/**
 * Internal worker routine.
 *
 * On x86 the typical stack frame layout is like this:
 *     ..  ..
 *     16  parameter 2
 *     12  parameter 1
 *      8  parameter 0
 *      4  return address
 *      0  old ebp; current ebp points here
 */
DECL_NO_INLINE(static, int) dbgfR3StackWalk(PDBGFUNWINDCTX pUnwindCtx, PDBGFSTACKFRAME pFrame, bool fFirst)
{
    /*
     * Stop if we got a read error in the previous run.
     */
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_LAST)
        return VERR_NO_MORE_FILES;

    /*
     * Advance the frame (except for the first).
     */
    if (!fFirst) /** @todo we can probably eliminate this fFirst business... */
    {
        /* frame, pc and stack is taken from the existing frames return members. */
        pFrame->AddrFrame = pFrame->AddrReturnFrame;
        pFrame->AddrPC    = pFrame->AddrReturnPC;
        pFrame->pSymPC    = pFrame->pSymReturnPC;
        pFrame->pLinePC   = pFrame->pLineReturnPC;

        /* increment the frame number. */
        pFrame->iFrame++;

        /* UNWIND_INFO_RET -> USED_UNWIND; return type */
        if (!(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_UNWIND_INFO_RET))
            pFrame->fFlags &= ~DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO;
        else
        {
            pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO;
            pFrame->fFlags &= ~DBGFSTACKFRAME_FLAGS_UNWIND_INFO_RET;
            if (pFrame->enmReturnFrameReturnType != DBGFRETURNTYPE_INVALID)
            {
                pFrame->enmReturnType = pFrame->enmReturnFrameReturnType;
                pFrame->enmReturnFrameReturnType = DBGFRETURNTYPE_INVALID;
            }
        }
    }

    /*
     * Enagage the OS layer and collect register changes.
     */
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO)
    {
        /** @todo engage the OS buggers to identify trap frames and update unwindctx accordingly. */
        if (!fFirst)
        {
            int rc = dbgfR3StackWalkCollectRegisterChanges(pUnwindCtx->m_pUVM, pFrame, &pUnwindCtx->m_State);
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    /*
     * Figure the return address size and use the old PC to guess stack item size.
     */
    /** @todo this is bogus... */
    unsigned cbRetAddr = DBGFReturnTypeSize(pFrame->enmReturnType);
    unsigned cbStackItem;
    switch (pFrame->AddrPC.fFlags & DBGFADDRESS_FLAGS_TYPE_MASK)
    {
        case DBGFADDRESS_FLAGS_FAR16: cbStackItem = 2; break;
        case DBGFADDRESS_FLAGS_FAR32: cbStackItem = 4; break;
        case DBGFADDRESS_FLAGS_FAR64: cbStackItem = 8; break;
        case DBGFADDRESS_FLAGS_RING0: cbStackItem = sizeof(RTHCUINTPTR); break;
        default:
            switch (pFrame->enmReturnType)
            {
                case DBGFRETURNTYPE_FAR16:
                case DBGFRETURNTYPE_IRET16:
                case DBGFRETURNTYPE_IRET32_V86:
                case DBGFRETURNTYPE_NEAR16: cbStackItem = 2; break;

                case DBGFRETURNTYPE_FAR32:
                case DBGFRETURNTYPE_IRET32:
                case DBGFRETURNTYPE_IRET32_PRIV:
                case DBGFRETURNTYPE_NEAR32: cbStackItem = 4; break;

                case DBGFRETURNTYPE_FAR64:
                case DBGFRETURNTYPE_IRET64:
                case DBGFRETURNTYPE_NEAR64: cbStackItem = 8; break;

                default:
                    AssertMsgFailed(("%d\n", pFrame->enmReturnType));
                    cbStackItem = 4;
                    break;
            }
    }

    /*
     * Read the raw frame data.
     * We double cbRetAddr in case we have a far return.
     */
    union
    {
        uint64_t *pu64;
        uint32_t *pu32;
        uint16_t *pu16;
        uint8_t  *pb;
        void     *pv;
    } u, uRet, uArgs, uBp;
    size_t cbRead = cbRetAddr*2 + cbStackItem + sizeof(pFrame->Args);
    u.pv = alloca(cbRead);
    uBp = u;
    uRet.pb = u.pb + cbStackItem;
    uArgs.pb = u.pb + cbStackItem + cbRetAddr;

    Assert(DBGFADDRESS_IS_VALID(&pFrame->AddrFrame));
    int rc = dbgfR3StackRead(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, u.pv, &pFrame->AddrFrame, cbRead, &cbRead);
    if (   RT_FAILURE(rc)
        || cbRead < cbRetAddr + cbStackItem)
        pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_LAST;

    /*
     * Return Frame address.
     *
     * If we used unwind info to get here, the unwind register context will be
     * positioned after the return instruction has been executed.  We start by
     * picking up the rBP register here for return frame and will try improve
     * on it further down by using unwind info.
     */
    pFrame->AddrReturnFrame = pFrame->AddrFrame;
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO)
    {
        if (   pFrame->enmReturnType == DBGFRETURNTYPE_IRET32_PRIV
            || pFrame->enmReturnType == DBGFRETURNTYPE_IRET64)
            DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnFrame,
                                 pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_SS], pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xBP]);
        else if (pFrame->enmReturnType == DBGFRETURNTYPE_IRET32_V86)
            DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &pFrame->AddrReturnFrame,
                                 ((uint32_t)pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_SS] << 4)
                               + pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xBP]);
        else
        {
            pFrame->AddrReturnFrame.off      = pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xBP];
            pFrame->AddrReturnFrame.FlatPtr += pFrame->AddrReturnFrame.off - pFrame->AddrFrame.off;
        }
    }
    else
    {
        switch (cbStackItem)
        {
            case 2:     pFrame->AddrReturnFrame.off = *uBp.pu16; break;
            case 4:     pFrame->AddrReturnFrame.off = *uBp.pu32; break;
            case 8:     pFrame->AddrReturnFrame.off = *uBp.pu64; break;
            default:    AssertMsgFailedReturn(("cbStackItem=%d\n", cbStackItem), VERR_DBGF_STACK_IPE_1);
        }

        /* Watcom tries to keep the frame pointer odd for far returns. */
        if (   cbStackItem <= 4
            && !(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO))
        {
            if (pFrame->AddrReturnFrame.off & 1)
            {
                pFrame->AddrReturnFrame.off &= ~(RTGCUINTPTR)1;
                if (pFrame->enmReturnType == DBGFRETURNTYPE_NEAR16)
                {
                    pFrame->fFlags       |= DBGFSTACKFRAME_FLAGS_USED_ODD_EVEN;
                    pFrame->enmReturnType = DBGFRETURNTYPE_FAR16;
                    cbRetAddr = 4;
                }
                else if (pFrame->enmReturnType == DBGFRETURNTYPE_NEAR32)
                {
                    pFrame->fFlags       |= DBGFSTACKFRAME_FLAGS_USED_ODD_EVEN;
                    pFrame->enmReturnType = DBGFRETURNTYPE_FAR32;
                    cbRetAddr = 8;
                }
            }
            else if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_ODD_EVEN)
            {
                if (pFrame->enmReturnType == DBGFRETURNTYPE_FAR16)
                {
                    pFrame->enmReturnType = DBGFRETURNTYPE_NEAR16;
                    cbRetAddr = 2;
                }
                else if (pFrame->enmReturnType == DBGFRETURNTYPE_NEAR32)
                {
                    pFrame->enmReturnType = DBGFRETURNTYPE_FAR32;
                    cbRetAddr = 4;
                }
                pFrame->fFlags &= ~DBGFSTACKFRAME_FLAGS_USED_ODD_EVEN;
            }
            uArgs.pb = u.pb + cbStackItem + cbRetAddr;
        }

        pFrame->AddrReturnFrame.FlatPtr += pFrame->AddrReturnFrame.off - pFrame->AddrFrame.off;
    }

    /*
     * Return Stack Address.
     */
    pFrame->AddrReturnStack = pFrame->AddrReturnFrame;
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO)
    {
        if (   pFrame->enmReturnType == DBGFRETURNTYPE_IRET32_PRIV
            || pFrame->enmReturnType == DBGFRETURNTYPE_IRET64)
            DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnStack,
                                 pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_SS], pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xSP]);
        else if (pFrame->enmReturnType == DBGFRETURNTYPE_IRET32_V86)
            DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &pFrame->AddrReturnStack,
                                 ((uint32_t)pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_SS] << 4)
                               + pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xSP]);
        else
        {
            pFrame->AddrReturnStack.off      = pUnwindCtx->m_State.u.x86.auRegs[X86_GREG_xSP];
            pFrame->AddrReturnStack.FlatPtr += pFrame->AddrReturnStack.off - pFrame->AddrStack.off;
        }
    }
    else
    {
        pFrame->AddrReturnStack.off     += cbStackItem + cbRetAddr;
        pFrame->AddrReturnStack.FlatPtr += cbStackItem + cbRetAddr;
    }

    /*
     * Return PC.
     */
    pFrame->AddrReturnPC = pFrame->AddrPC;
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO)
    {
        if (DBGFReturnTypeIsNear(pFrame->enmReturnType))
        {
            pFrame->AddrReturnPC.off      = pUnwindCtx->m_State.uPc;
            pFrame->AddrReturnPC.FlatPtr += pFrame->AddrReturnPC.off - pFrame->AddrPC.off;
        }
        else
            DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC,
                                 pUnwindCtx->m_State.u.x86.auSegs[X86_SREG_CS], pUnwindCtx->m_State.uPc);
    }
    else
        switch (pFrame->enmReturnType)
        {
            case DBGFRETURNTYPE_NEAR16:
                if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
                {
                    pFrame->AddrReturnPC.FlatPtr += *uRet.pu16 - pFrame->AddrReturnPC.off;
                    pFrame->AddrReturnPC.off      = *uRet.pu16;
                }
                else
                    DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &pFrame->AddrReturnPC, *uRet.pu16);
                break;
            case DBGFRETURNTYPE_NEAR32:
                if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
                {
                    pFrame->AddrReturnPC.FlatPtr += *uRet.pu32 - pFrame->AddrReturnPC.off;
                    pFrame->AddrReturnPC.off      = *uRet.pu32;
                }
                else
                    DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &pFrame->AddrReturnPC, *uRet.pu32);
                break;
            case DBGFRETURNTYPE_NEAR64:
                if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
                {
                    pFrame->AddrReturnPC.FlatPtr += *uRet.pu64 - pFrame->AddrReturnPC.off;
                    pFrame->AddrReturnPC.off      = *uRet.pu64;
                }
                else
                    DBGFR3AddrFromFlat(pUnwindCtx->m_pUVM, &pFrame->AddrReturnPC, *uRet.pu64);
                break;
            case DBGFRETURNTYPE_FAR16:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[1], uRet.pu16[0]);
                break;
            case DBGFRETURNTYPE_FAR32:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
                break;
            case DBGFRETURNTYPE_FAR64:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[4], uRet.pu64[0]);
                break;
            case DBGFRETURNTYPE_IRET16:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[1], uRet.pu16[0]);
                break;
            case DBGFRETURNTYPE_IRET32:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
                break;
            case DBGFRETURNTYPE_IRET32_PRIV:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
                break;
            case DBGFRETURNTYPE_IRET32_V86:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
                break;
            case DBGFRETURNTYPE_IRET64:
                DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &pFrame->AddrReturnPC, uRet.pu16[4], uRet.pu64[0]);
                break;
            default:
                AssertMsgFailed(("enmReturnType=%d\n", pFrame->enmReturnType));
                return VERR_INVALID_PARAMETER;
        }


    pFrame->pSymReturnPC  = DBGFR3AsSymbolByAddrA(pUnwindCtx->m_pUVM, pUnwindCtx->m_hAs, &pFrame->AddrReturnPC,
                                                  RTDBGSYMADDR_FLAGS_LESS_OR_EQUAL | RTDBGSYMADDR_FLAGS_SKIP_ABS_IN_DEFERRED,
                                                  NULL /*poffDisp*/, NULL /*phMod*/);
    pFrame->pLineReturnPC = DBGFR3AsLineByAddrA(pUnwindCtx->m_pUVM, pUnwindCtx->m_hAs, &pFrame->AddrReturnPC,
                                                NULL /*poffDisp*/, NULL /*phMod*/);

    /*
     * Frame bitness flag.
     */
    /** @todo use previous return type for this? */
    pFrame->fFlags &= ~(DBGFSTACKFRAME_FLAGS_16BIT | DBGFSTACKFRAME_FLAGS_32BIT | DBGFSTACKFRAME_FLAGS_64BIT);
    switch (cbStackItem)
    {
        case 2: pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_16BIT; break;
        case 4: pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_32BIT; break;
        case 8: pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_64BIT; break;
        default:    AssertMsgFailedReturn(("cbStackItem=%d\n", cbStackItem), VERR_DBGF_STACK_IPE_2);
    }

    /*
     * The arguments.
     */
    memcpy(&pFrame->Args, uArgs.pv, sizeof(pFrame->Args));

    /*
     * Try use unwind information to locate the return frame pointer (for the
     * next loop iteration).
     */
    Assert(!(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_UNWIND_INFO_RET));
    pFrame->enmReturnFrameReturnType = DBGFRETURNTYPE_INVALID;
    if (!(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_LAST))
    {
        /* Set PC and SP if we didn't unwind our way here (context will then point
           and the return PC and SP already). */
        if (!(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO))
        {
            dbgfR3UnwindCtxSetPcAndSp(pUnwindCtx, &pFrame->AddrReturnPC, &pFrame->AddrReturnStack);
        }
        /** @todo Reevaluate CS if the previous frame return type isn't near. */
        if (   pUnwindCtx->m_State.enmArch == RTLDRARCH_AMD64
            || pUnwindCtx->m_State.enmArch == RTLDRARCH_X86_32
            || pUnwindCtx->m_State.enmArch == RTLDRARCH_X86_16)
            pUnwindCtx->m_State.u.x86.Loaded.fAll = 0;
        else
            AssertFailed();
        if (dbgfR3UnwindCtxDoOneFrame(pUnwindCtx))
        {
            DBGFADDRESS AddrReturnFrame = pFrame->AddrReturnFrame;
            rc = DBGFR3AddrFromSelOff(pUnwindCtx->m_pUVM, pUnwindCtx->m_idCpu, &AddrReturnFrame,
                                      pUnwindCtx->m_State.u.x86.FrameAddr.sel, pUnwindCtx->m_State.u.x86.FrameAddr.off);
            if (RT_SUCCESS(rc))
                pFrame->AddrReturnFrame      = AddrReturnFrame;
            pFrame->enmReturnFrameReturnType = pUnwindCtx->m_State.enmRetType;
            pFrame->fFlags                  |= DBGFSTACKFRAME_FLAGS_UNWIND_INFO_RET;
        }
    }

    return VINF_SUCCESS;
}


/**
 * Walks the entire stack allocating memory as we walk.
 */
static DECLCALLBACK(int) dbgfR3StackWalkCtxFull(PUVM pUVM, VMCPUID idCpu, PCCPUMCTX pCtx, RTDBGAS hAs,
                                                DBGFCODETYPE enmCodeType,
                                                PCDBGFADDRESS pAddrFrame,
                                                PCDBGFADDRESS pAddrStack,
                                                PCDBGFADDRESS pAddrPC,
                                                DBGFRETURNTYPE enmReturnType,
                                                PCDBGFSTACKFRAME *ppFirstFrame)
{
    DBGFUNWINDCTX UnwindCtx(pUVM, idCpu, pCtx, hAs);

    /* alloc first frame. */
    PDBGFSTACKFRAME pCur = (PDBGFSTACKFRAME)MMR3HeapAllocZU(pUVM, MM_TAG_DBGF_STACK, sizeof(*pCur));
    if (!pCur)
        return VERR_NO_MEMORY;

    /*
     * Initialize the frame.
     */
    pCur->pNextInternal = NULL;
    pCur->pFirstInternal = pCur;

    int rc = VINF_SUCCESS;
    if (pAddrPC)
        pCur->AddrPC = *pAddrPC;
    else if (enmCodeType != DBGFCODETYPE_GUEST)
        DBGFR3AddrFromFlat(pUVM, &pCur->AddrPC, pCtx->rip);
    else
        rc = DBGFR3AddrFromSelOff(pUVM, idCpu, &pCur->AddrPC, pCtx->cs.Sel, pCtx->rip);
    if (RT_SUCCESS(rc))
    {
        uint64_t fAddrMask;
        if (enmCodeType == DBGFCODETYPE_RING0)
            fAddrMask = HC_ARCH_BITS == 64 ? UINT64_MAX : UINT32_MAX;
        else if (enmCodeType == DBGFCODETYPE_HYPER)
            fAddrMask = UINT32_MAX;
        else if (DBGFADDRESS_IS_FAR16(&pCur->AddrPC))
            fAddrMask = UINT16_MAX;
        else if (DBGFADDRESS_IS_FAR32(&pCur->AddrPC))
            fAddrMask = UINT32_MAX;
        else if (DBGFADDRESS_IS_FAR64(&pCur->AddrPC))
            fAddrMask = UINT64_MAX;
        else
        {
            PVMCPU pVCpu = VMMGetCpuById(pUVM->pVM, idCpu);
            CPUMMODE enmCpuMode = CPUMGetGuestMode(pVCpu);
            if (enmCpuMode == CPUMMODE_REAL)
            {
                fAddrMask = UINT16_MAX;
                if (enmReturnType == DBGFRETURNTYPE_INVALID)
                    pCur->enmReturnType = DBGFRETURNTYPE_NEAR16;
            }
            else if (   enmCpuMode == CPUMMODE_PROTECTED
                     || !CPUMIsGuestIn64BitCode(pVCpu))
            {
                fAddrMask = UINT32_MAX;
                if (enmReturnType == DBGFRETURNTYPE_INVALID)
                    pCur->enmReturnType = DBGFRETURNTYPE_NEAR32;
            }
            else
            {
                fAddrMask = UINT64_MAX;
                if (enmReturnType == DBGFRETURNTYPE_INVALID)
                    pCur->enmReturnType = DBGFRETURNTYPE_NEAR64;
            }
        }

        if (enmReturnType == DBGFRETURNTYPE_INVALID)
            switch (pCur->AddrPC.fFlags & DBGFADDRESS_FLAGS_TYPE_MASK)
            {
                case DBGFADDRESS_FLAGS_FAR16: pCur->enmReturnType = DBGFRETURNTYPE_NEAR16; break;
                case DBGFADDRESS_FLAGS_FAR32: pCur->enmReturnType = DBGFRETURNTYPE_NEAR32; break;
                case DBGFADDRESS_FLAGS_FAR64: pCur->enmReturnType = DBGFRETURNTYPE_NEAR64; break;
                case DBGFADDRESS_FLAGS_RING0:
                    pCur->enmReturnType = HC_ARCH_BITS == 64 ? DBGFRETURNTYPE_NEAR64 : DBGFRETURNTYPE_NEAR32;
                    break;
                default:
                    pCur->enmReturnType = DBGFRETURNTYPE_NEAR32;
                    break;
            }


        if (pAddrStack)
            pCur->AddrStack = *pAddrStack;
        else if (enmCodeType != DBGFCODETYPE_GUEST)
            DBGFR3AddrFromFlat(pUVM, &pCur->AddrStack, pCtx->rsp & fAddrMask);
        else
            rc = DBGFR3AddrFromSelOff(pUVM, idCpu, &pCur->AddrStack, pCtx->ss.Sel, pCtx->rsp & fAddrMask);

        Assert(!(pCur->fFlags & DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO));
        if (pAddrFrame)
            pCur->AddrFrame = *pAddrFrame;
        else if (   RT_SUCCESS(rc)
                 && dbgfR3UnwindCtxSetPcAndSp(&UnwindCtx, &pCur->AddrPC, &pCur->AddrStack)
                 && dbgfR3UnwindCtxDoOneFrame(&UnwindCtx))
        {
            pCur->enmReturnType = UnwindCtx.m_State.enmRetType;
            pCur->fFlags |= DBGFSTACKFRAME_FLAGS_USED_UNWIND_INFO;
            rc = DBGFR3AddrFromSelOff(UnwindCtx.m_pUVM, UnwindCtx.m_idCpu, &pCur->AddrFrame,
                                      UnwindCtx.m_State.u.x86.FrameAddr.sel, UnwindCtx.m_State.u.x86.FrameAddr.off);
        }
        else if (enmCodeType != DBGFCODETYPE_GUEST)
            DBGFR3AddrFromFlat(pUVM, &pCur->AddrFrame, pCtx->rbp & fAddrMask);
        else if (RT_SUCCESS(rc))
            rc = DBGFR3AddrFromSelOff(pUVM, idCpu, &pCur->AddrFrame, pCtx->ss.Sel, pCtx->rbp & fAddrMask);

        /*
         * The first frame.
         */
        if (RT_SUCCESS(rc))
        {
            if (DBGFADDRESS_IS_VALID(&pCur->AddrPC))
            {
                pCur->pSymPC  = DBGFR3AsSymbolByAddrA(pUVM, hAs, &pCur->AddrPC,
                                                      RTDBGSYMADDR_FLAGS_LESS_OR_EQUAL | RTDBGSYMADDR_FLAGS_SKIP_ABS_IN_DEFERRED,
                                                      NULL /*poffDisp*/, NULL /*phMod*/);
                pCur->pLinePC = DBGFR3AsLineByAddrA(pUVM, hAs, &pCur->AddrPC, NULL /*poffDisp*/, NULL /*phMod*/);
            }

            rc = dbgfR3StackWalk(&UnwindCtx, pCur, true /*fFirst*/);
        }
    }
    else
        pCur->enmReturnType = enmReturnType;
    if (RT_FAILURE(rc))
    {
        DBGFR3StackWalkEnd(pCur);
        return rc;
    }

    /*
     * The other frames.
     */
    DBGFSTACKFRAME Next = *pCur;
    while (!(pCur->fFlags & (DBGFSTACKFRAME_FLAGS_LAST | DBGFSTACKFRAME_FLAGS_MAX_DEPTH | DBGFSTACKFRAME_FLAGS_LOOP)))
    {
        Next.cSureRegs  = 0;
        Next.paSureRegs = NULL;

        /* try walk. */
        rc = dbgfR3StackWalk(&UnwindCtx, &Next, false /*fFirst*/);
        if (RT_FAILURE(rc))
            break;

        /* add the next frame to the chain. */
        PDBGFSTACKFRAME pNext = (PDBGFSTACKFRAME)MMR3HeapAllocU(pUVM, MM_TAG_DBGF_STACK, sizeof(*pNext));
        if (!pNext)
        {
            DBGFR3StackWalkEnd(pCur);
            return VERR_NO_MEMORY;
        }
        *pNext = Next;
        pCur->pNextInternal = pNext;
        pCur = pNext;
        Assert(pCur->pNextInternal == NULL);

        /* check for loop */
        for (PCDBGFSTACKFRAME pLoop = pCur->pFirstInternal;
             pLoop && pLoop != pCur;
             pLoop = pLoop->pNextInternal)
            if (pLoop->AddrFrame.FlatPtr == pCur->AddrFrame.FlatPtr)
            {
                pCur->fFlags |= DBGFSTACKFRAME_FLAGS_LOOP;
                break;
            }

        /* check for insane recursion */
        if (pCur->iFrame >= 2048)
            pCur->fFlags |= DBGFSTACKFRAME_FLAGS_MAX_DEPTH;
    }

    *ppFirstFrame = pCur->pFirstInternal;
    return rc;
}


/**
 * Common worker for DBGFR3StackWalkBeginGuestEx, DBGFR3StackWalkBeginHyperEx,
 * DBGFR3StackWalkBeginGuest and DBGFR3StackWalkBeginHyper.
 */
static int dbgfR3StackWalkBeginCommon(PUVM pUVM,
                                      VMCPUID idCpu,
                                      DBGFCODETYPE enmCodeType,
                                      PCDBGFADDRESS pAddrFrame,
                                      PCDBGFADDRESS pAddrStack,
                                      PCDBGFADDRESS pAddrPC,
                                      DBGFRETURNTYPE enmReturnType,
                                      PCDBGFSTACKFRAME *ppFirstFrame)
{
    /*
     * Validate parameters.
     */
    *ppFirstFrame = NULL;
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, VERR_INVALID_VM_HANDLE);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, VERR_INVALID_VM_HANDLE);
    AssertReturn(idCpu < pVM->cCpus, VERR_INVALID_CPU_ID);
    if (pAddrFrame)
        AssertReturn(DBGFR3AddrIsValid(pUVM, pAddrFrame), VERR_INVALID_PARAMETER);
    if (pAddrStack)
        AssertReturn(DBGFR3AddrIsValid(pUVM, pAddrStack), VERR_INVALID_PARAMETER);
    if (pAddrPC)
        AssertReturn(DBGFR3AddrIsValid(pUVM, pAddrPC), VERR_INVALID_PARAMETER);
    AssertReturn(enmReturnType >= DBGFRETURNTYPE_INVALID && enmReturnType < DBGFRETURNTYPE_END, VERR_INVALID_PARAMETER);

    /*
     * Get the CPUM context pointer and pass it on the specified EMT.
     */
    RTDBGAS     hAs;
    PCCPUMCTX   pCtx;
    switch (enmCodeType)
    {
        case DBGFCODETYPE_GUEST:
            pCtx = CPUMQueryGuestCtxPtr(VMMGetCpuById(pVM, idCpu));
            hAs  = DBGF_AS_GLOBAL;
            break;
        case DBGFCODETYPE_HYPER:
            pCtx = CPUMQueryGuestCtxPtr(VMMGetCpuById(pVM, idCpu));
            hAs  = DBGF_AS_RC_AND_GC_GLOBAL;
            break;
        case DBGFCODETYPE_RING0:
            pCtx = NULL;    /* No valid context present. */
            hAs  = DBGF_AS_R0;
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
    return VMR3ReqPriorityCallWaitU(pUVM, idCpu, (PFNRT)dbgfR3StackWalkCtxFull, 10,
                                    pUVM, idCpu, pCtx, hAs, enmCodeType,
                                    pAddrFrame, pAddrStack, pAddrPC, enmReturnType, ppFirstFrame);
}


/**
 * Begins a guest stack walk, extended version.
 *
 * This will walk the current stack, constructing a list of info frames which is
 * returned to the caller. The caller uses DBGFR3StackWalkNext to traverse the
 * list and DBGFR3StackWalkEnd to release it.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_NO_MEMORY if we're out of memory.
 *
 * @param   pUVM            The user mode VM handle.
 * @param   idCpu           The ID of the virtual CPU which stack we want to walk.
 * @param   enmCodeType     Code type
 * @param   pAddrFrame      Frame address to start at. (Optional)
 * @param   pAddrStack      Stack address to start at. (Optional)
 * @param   pAddrPC         Program counter to start at. (Optional)
 * @param   enmReturnType   The return address type. (Optional)
 * @param   ppFirstFrame    Where to return the pointer to the first info frame.
 */
VMMR3DECL(int) DBGFR3StackWalkBeginEx(PUVM pUVM,
                                      VMCPUID idCpu,
                                      DBGFCODETYPE enmCodeType,
                                      PCDBGFADDRESS pAddrFrame,
                                      PCDBGFADDRESS pAddrStack,
                                      PCDBGFADDRESS pAddrPC,
                                      DBGFRETURNTYPE enmReturnType,
                                      PCDBGFSTACKFRAME *ppFirstFrame)
{
    return dbgfR3StackWalkBeginCommon(pUVM, idCpu, enmCodeType, pAddrFrame, pAddrStack, pAddrPC, enmReturnType, ppFirstFrame);
}


/**
 * Begins a guest stack walk.
 *
 * This will walk the current stack, constructing a list of info frames which is
 * returned to the caller. The caller uses DBGFR3StackWalkNext to traverse the
 * list and DBGFR3StackWalkEnd to release it.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_NO_MEMORY if we're out of memory.
 *
 * @param   pUVM            The user mode VM handle.
 * @param   idCpu           The ID of the virtual CPU which stack we want to walk.
 * @param   enmCodeType     Code type
 * @param   ppFirstFrame    Where to return the pointer to the first info frame.
 */
VMMR3DECL(int) DBGFR3StackWalkBegin(PUVM pUVM, VMCPUID idCpu, DBGFCODETYPE enmCodeType, PCDBGFSTACKFRAME *ppFirstFrame)
{
    return dbgfR3StackWalkBeginCommon(pUVM, idCpu, enmCodeType, NULL, NULL, NULL, DBGFRETURNTYPE_INVALID, ppFirstFrame);
}

/**
 * Gets the next stack frame.
 *
 * @returns Pointer to the info for the next stack frame.
 *          NULL if no more frames.
 *
 * @param   pCurrent    Pointer to the current stack frame.
 *
 */
VMMR3DECL(PCDBGFSTACKFRAME) DBGFR3StackWalkNext(PCDBGFSTACKFRAME pCurrent)
{
    return pCurrent
         ? pCurrent->pNextInternal
         : NULL;
}


/**
 * Ends a stack walk process.
 *
 * This *must* be called after a successful first call to any of the stack
 * walker functions. If not called we will leak memory or other resources.
 *
 * @param   pFirstFrame     The frame returned by one of the begin functions.
 */
VMMR3DECL(void) DBGFR3StackWalkEnd(PCDBGFSTACKFRAME pFirstFrame)
{
    if (    !pFirstFrame
        ||  !pFirstFrame->pFirstInternal)
        return;

    PDBGFSTACKFRAME pFrame = (PDBGFSTACKFRAME)pFirstFrame->pFirstInternal;
    while (pFrame)
    {
        PDBGFSTACKFRAME pCur = pFrame;
        pFrame = (PDBGFSTACKFRAME)pCur->pNextInternal;
        if (pFrame)
        {
            if (pCur->pSymReturnPC == pFrame->pSymPC)
                pFrame->pSymPC = NULL;
            if (pCur->pSymReturnPC == pFrame->pSymReturnPC)
                pFrame->pSymReturnPC = NULL;

            if (pCur->pSymPC == pFrame->pSymPC)
                pFrame->pSymPC = NULL;
            if (pCur->pSymPC == pFrame->pSymReturnPC)
                pFrame->pSymReturnPC = NULL;

            if (pCur->pLineReturnPC == pFrame->pLinePC)
                pFrame->pLinePC = NULL;
            if (pCur->pLineReturnPC == pFrame->pLineReturnPC)
                pFrame->pLineReturnPC = NULL;

            if (pCur->pLinePC == pFrame->pLinePC)
                pFrame->pLinePC = NULL;
            if (pCur->pLinePC == pFrame->pLineReturnPC)
                pFrame->pLineReturnPC = NULL;
        }

        RTDbgSymbolFree(pCur->pSymPC);
        RTDbgSymbolFree(pCur->pSymReturnPC);
        RTDbgLineFree(pCur->pLinePC);
        RTDbgLineFree(pCur->pLineReturnPC);

        if (pCur->paSureRegs)
        {
            MMR3HeapFree(pCur->paSureRegs);
            pCur->paSureRegs = NULL;
            pCur->cSureRegs = 0;
        }

        pCur->pNextInternal = NULL;
        pCur->pFirstInternal = NULL;
        pCur->fFlags = 0;
        MMR3HeapFree(pCur);
    }
}

