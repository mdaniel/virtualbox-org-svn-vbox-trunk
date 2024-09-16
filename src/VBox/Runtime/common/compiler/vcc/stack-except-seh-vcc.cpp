/* $Id$ */
/** @file
 * IPRT - Visual C++ Compiler - Stack Checking, __GSHandlerCheck_SEH.
 */

/*
 * Copyright (C) 2022-2024 Oracle and/or its affiliates.
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
#include "internal/nocrt.h"

#include "except-vcc.h"


#if !defined(RT_ARCH_AMD64)
# error "This file is for AMD64 (and probably ARM, but needs porting)"
#endif



/**
 * Check the stack cookie before calling the exception handler.
 *
 * This is to prevent attackers from bypassing stack cookie checking by
 * triggering an exception.
 *
 * This is called for windows' structured exception handling (SEH), i.e. the
 * __try/__except/__finally stuff in Visual C++, for which the compiler
 * generates somewhat different strctures compared to the plain __GSHanderCheck
 * scenario.
 *
 * @returns Exception disposition.
 * @param   pXcptRec    The exception record.
 * @param   pXcptRegRec The exception registration record, taken to be the frame
 *                      address.
 * @param   pCpuCtx     The CPU context for the exception.
 * @param   pDispCtx    Dispatcher context.
 */
extern "C" __declspec(guard(suppress))
EXCEPTION_DISPOSITION __GSHandlerCheck_SEH(PEXCEPTION_RECORD pXcptRec, PEXCEPTION_REGISTRATION_RECORD pXcptRegRec,
                                           PCONTEXT pCpuCtx, PDISPATCHER_CONTEXT pDispCtx)
{
    /*
     * The HandlerData points to a scope table, which is then followed by GS_HANDLER_DATA.
     *
     * Sample offCookie values: 0521H (tst.cpp), 02caH (installNetLwf), and 0502H (installNetFlt).
     */
    SCOPE_TABLE const *pScopeTable  = (SCOPE_TABLE const *)pDispCtx->HandlerData;
    PCGS_HANDLER_DATA  pHandlerData = (PCGS_HANDLER_DATA)&pScopeTable->ScopeRecord[pScopeTable->Count];

    /*
     * Locate the stack cookie and call the regular stack cookie checker routine.
     * (Same code as in __GSHandlerCheck, fixes applies both places.)
     */
    /* Calculate the cookie address and read it. */
    uintptr_t uPtrFrame = (uintptr_t)pXcptRegRec;
    uint32_t  offCookie = pHandlerData->u.offCookie;
    if (offCookie & GS_HANDLER_OFF_COOKIE_HAS_ALIGNMENT)
    {
        uPtrFrame += pHandlerData->offAlignedBase;
        uPtrFrame &= ~(uintptr_t)pHandlerData->uAlignmentMask;
    }
    uintptr_t uCookie = *(uintptr_t const *)(uPtrFrame + (int32_t)(offCookie & GS_HANDLER_OFF_COOKIE_MASK));

    /* The stored cookie is xor'ed with the frame / registration record address
       or with the frame pointer register if one is being used.  In the latter
       case, we have to add the frame offset to get the correct address. */
    uintptr_t uXorAddr = (uintptr_t)pXcptRegRec;
    PCIMAGE_UNWIND_INFO pUnwindInfo = (PCIMAGE_UNWIND_INFO)(pDispCtx->ImageBase + pDispCtx->FunctionEntry->UnwindInfoAddress);
    if (pUnwindInfo->FrameRegister != 0)
        uXorAddr += pUnwindInfo->FrameOffset << 4;

    /* This call will not return on failure. */
    __security_check_cookie(uCookie ^ uXorAddr);


    /*
     * Now call the handler if the GS handler data indicates that we ought to.
     */
    if (  (IS_UNWINDING(pXcptRec->ExceptionFlags) ? GS_HANDLER_OFF_COOKIE_IS_UHANDLER : GS_HANDLER_OFF_COOKIE_IS_EHANDLER)
        & pHandlerData->u.offCookie)
        return __C_specific_handler(pXcptRec, pXcptRegRec, pCpuCtx, pDispCtx);

    return ExceptionContinueSearch;
}

