/** @file
 * VM - The Virtual Machine, API.
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

#ifndef ___VBox_vmm_vmapi_h
#define ___VBox_vmm_vmapi_h

#include <VBox/types.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/cfgm.h>

#include <iprt/stdarg.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_vm_apis   VM All Contexts API
 * @ingroup grp_vm
 * @{ */

/** @def VM_RC_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * raw-mode address.
 *
 * @returns raw-mode virtual address.
 * @param   pVM     The cross context VM structure.
 * @param   pvInVM  CC Pointer within the VM.
 */
#ifdef IN_RING3
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)((RTRCUINTPTR)pVM->pVMRC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#elif defined(IN_RING0)
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)((RTRCUINTPTR)pVM->pVMRC + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR0)) )
#else
# define VM_RC_ADDR(pVM, pvInVM)        ( (RTRCPTR)(pvInVM) )
#endif

/** @def VM_R3_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * ring-3 host address.
 *
 * @returns host virtual address.
 * @param   pVM     The cross context VM structure.
 * @param   pvInVM  CC pointer within the VM.
 */
#ifdef IN_RC
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMRC)) )
#elif defined(IN_RING0)
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)((RTR3UINTPTR)pVM->pVMR3 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR0)) )
#else
# define VM_R3_ADDR(pVM, pvInVM)       ( (RTR3PTR)(pvInVM) )
#endif


/** @def VM_R0_ADDR
 * Converts a current context address of data within the VM structure to the equivalent
 * ring-0 host address.
 *
 * @returns host virtual address.
 * @param   pVM     The cross context VM structure.
 * @param   pvInVM  CC pointer within the VM.
 */
#ifdef IN_RC
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMRC)) )
#elif defined(IN_RING3)
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)((RTR0UINTPTR)pVM->pVMR0 + (uint32_t)((uintptr_t)(pvInVM) - (uintptr_t)pVM->pVMR3)) )
#else
# define VM_R0_ADDR(pVM, pvInVM)       ( (RTR0PTR)(pvInVM) )
#endif



/**
 * VM error callback function.
 *
 * @param   pUVM            The user mode VM handle.  Can be NULL if an error
 *                          occurred before successfully creating a VM.
 * @param   pvUser          The user argument.
 * @param   rc              VBox status code.
 * @param   SRC_POS         The source position arguments. See RT_SRC_POS and RT_SRC_POS_ARGS.
 * @param   pszFormat       Error message format string.
 * @param   args            Error message arguments.
 */
typedef DECLCALLBACK(void) FNVMATERROR(PUVM pUVM, void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszError, va_list args);
/** Pointer to a VM error callback. */
typedef FNVMATERROR *PFNVMATERROR;

VMMDECL(int)    VMSetError(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(6, 7);
VMMDECL(int)    VMSetErrorV(PVM pVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list args) RT_IPRT_FORMAT_ATTR(6, 7);

/** @def VM_SET_ERROR
 * Macro for setting a simple VM error message.
 * Don't use '%' in the message!
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @endcode
 * @param   pVM             The cross context VM structure.
 * @param   rc              VBox status code.
 * @param   pszMessage      Error message string.
 * @thread  Any
 */
#define VM_SET_ERROR(pVM, rc, pszMessage)   (VMSetError(pVM, rc, RT_SRC_POS, pszMessage))

/** @def VM_SET_ERROR
 * Macro for setting a simple VM error message.
 * Don't use '%' in the message!
 *
 * @returns rc. Meaning you can do:
 *    @code
 *    return VM_SET_ERROR(pVM, VERR_OF_YOUR_CHOICE, "descriptive message");
 *    @endcode
 * @param   pVM             The cross context VM structure.
 * @param   rc              VBox status code.
 * @param   pszMessage      Error message string.
 * @thread  Any
 */
#define VM_SET_ERROR_U(a_pUVM, a_rc, a_pszMessage)   (VMR3SetError(a_pUVM, a_rc, RT_SRC_POS, a_pszMessage))


/**
 * VM runtime error callback function.
 *
 * See VMSetRuntimeError for the detailed description of parameters.
 *
 * @param   pUVM            The user mode VM handle.
 * @param   pvUser          The user argument.
 * @param   fFlags          The error flags.
 * @param   pszErrorId      Error ID string.
 * @param   pszFormat       Error message format string.
 * @param   va              Error message arguments.
 */
typedef DECLCALLBACK(void) FNVMATRUNTIMEERROR(PUVM pUVM, void *pvUser, uint32_t fFlags, const char *pszErrorId,
                                              const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(5, 0);
/** Pointer to a VM runtime error callback. */
typedef FNVMATRUNTIMEERROR *PFNVMATRUNTIMEERROR;

VMMDECL(int) VMSetRuntimeError(PVM pVM, uint32_t fFlags, const char *pszErrorId,
                               const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(4, 5);
VMMDECL(int) VMSetRuntimeErrorV(PVM pVM, uint32_t fFlags, const char *pszErrorId,
                                const char *pszFormat, va_list args) RT_IPRT_FORMAT_ATTR(4, 0);

/** @name VMSetRuntimeError fFlags
 * When no flags are given the VM will continue running and it's up to the front
 * end to take action on the error condition.
 *
 * @{ */
/** The error is fatal.
 * The VM is not in a state where it can be saved and will enter a state
 * where it can no longer execute code. The caller <b>must</b> propagate status
 * codes. */
#define VMSETRTERR_FLAGS_FATAL      RT_BIT_32(0)
/** Suspend the VM after, or if possible before, raising the error on EMT. The
 * caller <b>must</b> propagate status codes. */
#define VMSETRTERR_FLAGS_SUSPEND    RT_BIT_32(1)
/** Don't wait for the EMT to handle the request.
 * Only valid when on a worker thread and there is a high risk of a dead
 * lock. Be careful not to flood the user with errors. */
#define VMSETRTERR_FLAGS_NO_WAIT    RT_BIT_32(2)
/** @} */

/**
 * VM state change callback function.
 *
 * You are not allowed to call any function which changes the VM state from a
 * state callback, except VMR3Destroy().
 *
 * @param   pUVM        The user mode VM handle.
 * @param   enmState    The new state.
 * @param   enmOldState The old state.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACK(void) FNVMATSTATE(PUVM pUVM, VMSTATE enmState, VMSTATE enmOldState, void *pvUser);
/** Pointer to a VM state callback. */
typedef FNVMATSTATE *PFNVMATSTATE;

VMMDECL(const char *) VMGetStateName(VMSTATE enmState);


/**
 * Request type.
 */
typedef enum VMREQTYPE
{
    /** Invalid request. */
    VMREQTYPE_INVALID = 0,
    /** VM: Internal. */
    VMREQTYPE_INTERNAL,
    /** Maximum request type (exclusive). Used for validation. */
    VMREQTYPE_MAX
} VMREQTYPE;

/**
 * Request state.
 */
typedef enum VMREQSTATE
{
    /** The state is invalid. */
    VMREQSTATE_INVALID = 0,
    /** The request have been allocated and is in the process of being filed. */
    VMREQSTATE_ALLOCATED,
    /** The request is queued by the requester. */
    VMREQSTATE_QUEUED,
    /** The request is begin processed. */
    VMREQSTATE_PROCESSING,
    /** The request is completed, the requester is begin notified. */
    VMREQSTATE_COMPLETED,
    /** The request packet is in the free chain. (The requester */
    VMREQSTATE_FREE
} VMREQSTATE;

/**
 * Request flags.
 */
typedef enum VMREQFLAGS
{
    /** The request returns a VBox status code. */
    VMREQFLAGS_VBOX_STATUS  = 0,
    /** The request is a void request and have no status code. */
    VMREQFLAGS_VOID         = 1,
    /** Return type mask. */
    VMREQFLAGS_RETURN_MASK  = 1,
    /** Caller does not wait on the packet, EMT will free it. */
    VMREQFLAGS_NO_WAIT      = 2,
    /** Poke the destination EMT(s) if executing guest code. Use with care. */
    VMREQFLAGS_POKE         = 4,
    /** Priority request that can safely be processed while doing async
     *  suspend and power off. */
    VMREQFLAGS_PRIORITY     = 8
} VMREQFLAGS;


/**
 * VM Request packet.
 *
 * This is used to request an action in the EMT. Usually the requester is
 * another thread, but EMT can also end up being the requester in which case
 * it's carried out synchronously.
 */
typedef struct VMREQ
{
    /** Pointer to the next request in the chain. */
    struct VMREQ * volatile pNext;
    /** Pointer to ring-3 VM structure which this request belongs to. */
    PUVM                    pUVM;
    /** Request state. */
    volatile VMREQSTATE     enmState;
    /** VBox status code for the completed request. */
    volatile int32_t        iStatus;
    /** Requester event sem.
     * The request can use this event semaphore to wait/poll for completion
     * of the request.
     */
    RTSEMEVENT              EventSem;
    /** Set if the event semaphore is clear. */
    volatile bool           fEventSemClear;
    /** Flags, VMR3REQ_FLAGS_*. */
    unsigned                fFlags;
    /** Request type. */
    VMREQTYPE               enmType;
    /** Request destination. */
    VMCPUID                 idDstCpu;
    /** Request specific data. */
    union VMREQ_U
    {
        /** VMREQTYPE_INTERNAL. */
        struct
        {
            /** Pointer to the function to be called. */
            PFNRT               pfn;
            /** Number of arguments. */
            unsigned            cArgs;
            /** Array of arguments. */
            uintptr_t           aArgs[64];
        } Internal;
    } u;
} VMREQ;
/** Pointer to a VM request packet. */
typedef VMREQ *PVMREQ;

/** @} */


#ifndef IN_RC
/** @defgroup grp_vmm_apis_hc  VM Host Context API
 * @ingroup grp_vm
 * @{ */

/** @} */
#endif


#ifdef IN_RING3
/** @defgroup grp_vmm_apis_r3  VM Host Context Ring 3 API
 * This interface is a _draft_!
 * @ingroup grp_vm
 * @{ */

/**
 * Completion notification codes.
 */
typedef enum VMINITCOMPLETED
{
    /** The ring-3 init is completed. */
    VMINITCOMPLETED_RING3 = 1,
    /** The ring-0 init is completed. */
    VMINITCOMPLETED_RING0,
    /** The hardware accelerated virtualization init is completed.
     * Used to make decisision depending on HM* bits being completely
     * initialized. */
    VMINITCOMPLETED_HM,
    /** The RC init is completed. */
    VMINITCOMPLETED_RC
} VMINITCOMPLETED;


/** Reason for VM resume. */
typedef enum VMRESUMEREASON
{
    VMRESUMEREASON_INVALID = 0,
    /** User decided to do so. */
    VMRESUMEREASON_USER,
    /** VM reconfiguration (like changing DVD). */
    VMRESUMEREASON_RECONFIG,
    /** The host resumed. */
    VMRESUMEREASON_HOST_RESUME,
    /** Restored state. */
    VMRESUMEREASON_STATE_RESTORED,
    /** Snapshot / saved state. */
    VMRESUMEREASON_STATE_SAVED,
    /** Teleported to a new box / instance. */
    VMRESUMEREASON_TELEPORTED,
    /** Teleportation failed. */
    VMRESUMEREASON_TELEPORT_FAILED,
    /** FTM temporarily suspended the VM. */
    VMRESUMEREASON_FTM_SYNC,
    /** End of valid reasons. */
    VMRESUMEREASON_END,
    /** Blow the type up to 32-bits. */
    VMRESUMEREASON_32BIT_HACK = 0x7fffffff
} VMRESUMEREASON;

/** Reason for VM suspend. */
typedef enum VMSUSPENDREASON
{
    VMSUSPENDREASON_INVALID = 0,
    /** User decided to do so. */
    VMSUSPENDREASON_USER,
    /** VM reconfiguration (like changing DVD). */
    VMSUSPENDREASON_RECONFIG,
    /** The VM is suspending itself. */
    VMSUSPENDREASON_VM,
    /** The Vm is suspending because of a runtime error. */
    VMSUSPENDREASON_RUNTIME_ERROR,
    /** The host was suspended. */
    VMSUSPENDREASON_HOST_SUSPEND,
    /** The host is running low on battery power. */
    VMSUSPENDREASON_HOST_BATTERY_LOW,
    /** FTM is temporarily suspending the VM. */
    VMSUSPENDREASON_FTM_SYNC,
    /** End of valid reasons. */
    VMSUSPENDREASON_END,
    /** Blow the type up to 32-bits. */
    VMSUSPENDREASON_32BIT_HACK = 0x7fffffff
} VMSUSPENDREASON;


/**
 * Progress callback.
 *
 * This will report the completion percentage of an operation.
 *
 * @returns VINF_SUCCESS.
 * @returns Error code to cancel the operation with.
 * @param   pUVM        The user mode VM handle.
 * @param   uPercent    Completion percentage (0-100).
 * @param   pvUser      User specified argument.
 */
typedef DECLCALLBACK(int) FNVMPROGRESS(PUVM pUVM, unsigned uPercent, void *pvUser);
/** Pointer to a FNVMPROGRESS function. */
typedef FNVMPROGRESS *PFNVMPROGRESS;


VMMR3DECL(int)          VMR3Create(uint32_t cCpus, PCVMM2USERMETHODS pVm2UserCbs,
                                   PFNVMATERROR pfnVMAtError, void *pvUserVM,
                                   PFNCFGMCONSTRUCTOR pfnCFGMConstructor, void *pvUserCFGM,
                                   PVM *ppVM, PUVM *ppUVM);
VMMR3DECL(int)          VMR3PowerOn(PUVM pUVM);
VMMR3DECL(int)          VMR3Suspend(PUVM pUVM, VMSUSPENDREASON enmReason);
VMMR3DECL(VMSUSPENDREASON) VMR3GetSuspendReason(PUVM);
VMMR3DECL(int)          VMR3Resume(PUVM pUVM, VMRESUMEREASON enmReason);
VMMR3DECL(VMRESUMEREASON) VMR3GetResumeReason(PUVM);
VMMR3DECL(int)          VMR3Reset(PUVM pUVM);
VMMR3_INT_DECL(VBOXSTRICTRC) VMR3ResetFF(PVM pVM);
VMMR3_INT_DECL(VBOXSTRICTRC) VMR3ResetTripleFault(PVM pVM);
VMMR3DECL(int)          VMR3Save(PUVM pUVM, const char *pszFilename, bool fContinueAfterwards, PFNVMPROGRESS pfnProgress, void *pvUser, bool *pfSuspended);
VMMR3_INT_DECL(int)     VMR3SaveFT(PUVM pUVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser, bool *pfSuspended, bool fSkipStateChanges);
VMMR3DECL(int)          VMR3Teleport(PUVM pUVM, uint32_t cMsDowntime, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser, PFNVMPROGRESS pfnProgress, void *pvProgressUser, bool *pfSuspended);
VMMR3DECL(int)          VMR3LoadFromFile(PUVM pUVM, const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);
VMMR3DECL(int)          VMR3LoadFromStream(PUVM pUVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser,
                                           PFNVMPROGRESS pfnProgress, void *pvProgressUser);
VMMR3_INT_DECL(int)     VMR3LoadFromStreamFT(PUVM pUVM, PCSSMSTRMOPS pStreamOps, void *pvStreamOpsUser);

VMMR3DECL(int)          VMR3PowerOff(PUVM pUVM);
VMMR3DECL(int)          VMR3Destroy(PUVM pUVM);
VMMR3_INT_DECL(void)    VMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

VMMR3DECL(PVM)          VMR3GetVM(PUVM pUVM);
VMMR3DECL(PUVM)         VMR3GetUVM(PVM pVM);
VMMR3DECL(uint32_t)     VMR3RetainUVM(PUVM pUVM);
VMMR3DECL(uint32_t)     VMR3ReleaseUVM(PUVM pUVM);
VMMR3DECL(const char *) VMR3GetName(PUVM pUVM);
VMMR3DECL(PRTUUID)      VMR3GetUuid(PUVM pUVM, PRTUUID pUuid);
VMMR3DECL(VMSTATE)      VMR3GetState(PVM pVM);
VMMR3DECL(VMSTATE)      VMR3GetStateU(PUVM pUVM);
VMMR3DECL(const char *) VMR3GetStateName(VMSTATE enmState);
VMMR3DECL(int)          VMR3AtStateRegister(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser);
VMMR3DECL(int)          VMR3AtStateDeregister(PUVM pUVM, PFNVMATSTATE pfnAtState, void *pvUser);
VMMR3_INT_DECL(bool)    VMR3SetGuruMeditation(PVM pVM);
VMMR3_INT_DECL(bool)    VMR3TeleportedAndNotFullyResumedYet(PVM pVM);
VMMR3DECL(int)          VMR3AtErrorRegister(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser);
VMMR3DECL(int)          VMR3AtErrorDeregister(PUVM pUVM, PFNVMATERROR pfnAtError, void *pvUser);
VMMR3DECL(int)          VMR3SetError(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(6, 7);
VMMR3DECL(int)          VMR3SetErrorV(PUVM pUVM, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(6, 0);
VMMR3_INT_DECL(void)    VMR3SetErrorWorker(PVM pVM);
VMMR3_INT_DECL(uint32_t) VMR3GetErrorCount(PUVM pUVM);
VMMR3DECL(int)          VMR3AtRuntimeErrorRegister(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
VMMR3DECL(int)          VMR3AtRuntimeErrorDeregister(PUVM pUVM, PFNVMATRUNTIMEERROR pfnAtRuntimeError, void *pvUser);
VMMR3_INT_DECL(int)     VMR3SetRuntimeErrorWorker(PVM pVM);
VMMR3_INT_DECL(uint32_t) VMR3GetRuntimeErrorCount(PUVM pUVM);

VMMR3DECL(int)          VMR3ReqCallU(PUVM pUVM, VMCPUID idDstCpu, PVMREQ *ppReq, RTMSINTERVAL cMillies, uint32_t fFlags, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallVU(PUVM pUVM, VMCPUID idDstCpu, PVMREQ *ppReq, RTMSINTERVAL cMillies, uint32_t fFlags, PFNRT pfnFunction, unsigned cArgs, va_list Args);
VMMR3_INT_DECL(int)     VMR3ReqCallWait(PVM pVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallWaitU(PUVM pUVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallNoWait(PVM pVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallNoWaitU(PUVM pUVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3_INT_DECL(int)     VMR3ReqCallVoidWait(PVM pVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallVoidWaitU(PUVM pUVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqCallVoidNoWait(PVM pVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqPriorityCallWait(PVM pVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqPriorityCallWaitU(PUVM pUVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqPriorityCallVoidWaitU(PUVM pUVM, VMCPUID idDstCpu, PFNRT pfnFunction, unsigned cArgs, ...);
VMMR3DECL(int)          VMR3ReqAlloc(PUVM pUVM, PVMREQ *ppReq, VMREQTYPE enmType, VMCPUID idDstCpu);
VMMR3DECL(int)          VMR3ReqFree(PVMREQ pReq);
VMMR3DECL(int)          VMR3ReqQueue(PVMREQ pReq, RTMSINTERVAL cMillies);
VMMR3DECL(int)          VMR3ReqWait(PVMREQ pReq, RTMSINTERVAL cMillies);
VMMR3_INT_DECL(int)     VMR3ReqProcessU(PUVM pUVM, VMCPUID idDstCpu, bool fPriorityOnly);

/** @name Flags for VMR3NotifyCpuFFU and VMR3NotifyGlobalFFU.
 * @{ */
/** Whether we've done REM or not. */
#define VMNOTIFYFF_FLAGS_DONE_REM   RT_BIT_32(0)
/** Whether we should poke the CPU if it's executing guest code. */
#define VMNOTIFYFF_FLAGS_POKE       RT_BIT_32(1)
/** @} */
VMMR3_INT_DECL(void)        VMR3NotifyGlobalFFU(PUVM pUVM, uint32_t fFlags);
VMMR3_INT_DECL(void)        VMR3NotifyCpuFFU(PUVMCPU pUVMCpu, uint32_t fFlags);
VMMR3DECL(int)              VMR3NotifyCpuDeviceReady(PVM pVM, VMCPUID idCpu);
VMMR3_INT_DECL(int)         VMR3WaitHalted(PVM pVM, PVMCPU pVCpu, bool fIgnoreInterrupts);
VMMR3_INT_DECL(int)         VMR3WaitU(PUVMCPU pUVMCpu);
VMMR3DECL(int)              VMR3WaitForDeviceReady(PVM pVM, VMCPUID idCpu);
VMMR3_INT_DECL(int)         VMR3AsyncPdmNotificationWaitU(PUVMCPU pUVCpu);
VMMR3_INT_DECL(void)        VMR3AsyncPdmNotificationWakeupU(PUVM pUVM);
VMMR3_INT_DECL(RTCPUID)     VMR3GetVMCPUId(PVM pVM);
VMMR3_INT_DECL(bool)        VMR3IsLongModeAllowed(PVM pVM);
VMMR3DECL(RTTHREAD)         VMR3GetVMCPUThread(PUVM pUVM);
VMMR3DECL(RTNATIVETHREAD)   VMR3GetVMCPUNativeThread(PVM pVM);
VMMR3DECL(RTNATIVETHREAD)   VMR3GetVMCPUNativeThreadU(PUVM pUVM);
VMMR3DECL(int)              VMR3GetCpuCoreAndPackageIdFromCpuId(PUVM pUVM, VMCPUID idCpu, uint32_t *pidCpuCore, uint32_t *pidCpuPackage);
VMMR3_INT_DECL(uint32_t)    VMR3GetActiveEmts(PUVM pUVM);
VMMR3DECL(int)              VMR3HotUnplugCpu(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(int)              VMR3HotPlugCpu(PUVM pUVM, VMCPUID idCpu);
VMMR3DECL(int)              VMR3SetCpuExecutionCap(PUVM pUVM, uint32_t uCpuExecutionCap);
VMMR3DECL(int)              VMR3SetPowerOffInsteadOfReset(PUVM pUVM, bool fPowerOffInsteadOfReset);
/** @} */
#endif /* IN_RING3 */


#ifdef IN_RC
/** @defgroup grp_vmm_apis_gc  VM Guest Context APIs
 * @ingroup grp_vm
 * @{ */

/** @} */
#endif

RT_C_DECLS_END

/** @} */

#endif

