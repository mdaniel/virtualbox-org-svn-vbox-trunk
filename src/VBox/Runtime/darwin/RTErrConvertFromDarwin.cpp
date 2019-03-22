/* $Id$ */
/** @file
 * IPRT - Convert Darwin Mach returns codes to iprt status codes.
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
#include <mach/kern_return.h>
#include <IOKit/IOReturn.h>

#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/assert.h>


RTDECL(int) RTErrConvertFromDarwin(int iNativeCode)
{
    /*
     * 'optimized' success case.
     */
    if (iNativeCode == KERN_SUCCESS)
        return VINF_SUCCESS;

    switch (iNativeCode)
    {
        /*
         * Mach.
         */
        case KERN_INVALID_ADDRESS:      return VERR_INVALID_POINTER;
        case KERN_PROTECTION_FAILURE:   return VERR_PERMISSION_DENIED;
        //case KERN_NO_SPACE:
        case KERN_INVALID_ARGUMENT:     return VERR_INVALID_PARAMETER;
        //case KERN_FAILURE:
        //case KERN_RESOURCE_SHORTAGE:
        //case KERN_NOT_RECEIVER:
        case KERN_NO_ACCESS:            return VERR_ACCESS_DENIED;
        //case KERN_MEMORY_FAILURE:
        //case KERN_MEMORY_ERROR:
        //case KERN_ALREADY_IN_SET:
        //case KERN_NOT_IN_SET:
        //case KERN_NAME_EXISTS:
        //case KERN_ABORTED:
        //case KERN_INVALID_NAME:
        //case KERN_INVALID_TASK:
        //case KERN_INVALID_RIGHT:
        //case KERN_INVALID_VALUE:
        //case KERN_UREFS_OVERFLOW:
        //case KERN_INVALID_CAPABILITY:
        //case KERN_RIGHT_EXISTS:
        //case KERN_INVALID_HOST:
        //case KERN_MEMORY_PRESENT:
        //case KERN_MEMORY_DATA_MOVED:
        //case KERN_MEMORY_RESTART_COPY:
        //case KERN_INVALID_PROCESSOR_SET:
        //case KERN_POLICY_LIMIT:
        //case KERN_INVALID_POLICY:
        //case KERN_INVALID_OBJECT:
        //case KERN_ALREADY_WAITING:
        //case KERN_DEFAULT_SET:
        //case KERN_EXCEPTION_PROTECTED:
        //case KERN_INVALID_LEDGER:
        //case KERN_INVALID_MEMORY_CONTROL:
        //case KERN_INVALID_SECURITY:
        //case KERN_NOT_DEPRESSED:
        //case KERN_TERMINATED:
        //case KERN_LOCK_SET_DESTROYED:
        //case KERN_LOCK_UNSTABLE:
        case KERN_LOCK_OWNED:           return VERR_SEM_BUSY;
        //case KERN_LOCK_OWNED_SELF:
        case KERN_SEMAPHORE_DESTROYED:  return VERR_SEM_DESTROYED;
        //case KERN_RPC_SERVER_TERMINATED:
        //case KERN_RPC_TERMINATE_ORPHAN:
        //case KERN_RPC_CONTINUE_ORPHAN:
        case KERN_NOT_SUPPORTED:        return VERR_NOT_SUPPORTED;
        //case KERN_NODE_DOWN:
        //case KERN_NOT_WAITING:
        case KERN_OPERATION_TIMED_OUT:  return VERR_TIMEOUT;


        /*
         * I/O Kit.
         */
        case kIOReturnNoDevice:         return VERR_IO_BAD_UNIT;
        case kIOReturnUnsupported:      return VERR_NOT_SUPPORTED;
        case kIOReturnInternalError:    return VERR_INTERNAL_ERROR;
        case kIOReturnNoResources:      return VERR_OUT_OF_RESOURCES;
        case kIOReturnBadArgument:      return VERR_INVALID_PARAMETER;
        case kIOReturnCannotWire:       return VERR_LOCK_FAILED;

#ifdef IN_RING3
        /*
         * CoreFoundation COM (may overlap with I/O Kit and Mach).
         */
        default:
            if (    (unsigned)iNativeCode >= 0x80000000U
                &&  (unsigned)iNativeCode <= 0x8000FFFFU)
                return RTErrConvertFromDarwinCOM(iNativeCode);
            break;
#endif /* IN_RING3 */
    }

    /* unknown error. */
    AssertLogRelMsgFailed(("Unhandled error %#x\n", iNativeCode));
    return VERR_UNRESOLVED_ERROR;
}

