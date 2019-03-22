/* $Id$ */
/** @file
 * IPRT - Ring-0 Memory Objects, Darwin.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#define RTMEM_NO_WRAP_TO_EF_APIS /* circular dependency otherwise. */
#include "the-darwin-kernel.h"
#include "internal/iprt.h"
#include <iprt/memobj.h>

#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/x86.h>
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include "internal/memobj.h"

/*#define USE_VM_MAP_WIRE - may re-enable later when non-mapped allocations are added. */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The Darwin version of the memory object structure.
 */
typedef struct RTR0MEMOBJDARWIN
{
    /** The core structure. */
    RTR0MEMOBJINTERNAL  Core;
    /** Pointer to the memory descriptor created for allocated and locked memory. */
    IOMemoryDescriptor *pMemDesc;
    /** Pointer to the memory mapping object for mapped memory. */
    IOMemoryMap        *pMemMap;
} RTR0MEMOBJDARWIN, *PRTR0MEMOBJDARWIN;


/**
 * Touch the pages to force the kernel to create or write-enable the page table
 * entries.
 *
 * This is necessary since the kernel gets upset if we take a page fault when
 * preemption is disabled and/or we own a simple lock (same thing).  It has no
 * problems with us disabling interrupts when taking the traps, weird stuff.
 *
 * (This is basically a way of invoking vm_fault on a range of pages.)
 *
 * @param  pv           Pointer to the first page.
 * @param  cb           The number of bytes.
 */
static void rtR0MemObjDarwinTouchPages(void *pv, size_t cb)
{
    uint32_t volatile  *pu32 = (uint32_t volatile *)pv;
    for (;;)
    {
        ASMAtomicCmpXchgU32(pu32, 0xdeadbeef, 0xdeadbeef);
        if (cb <= PAGE_SIZE)
            break;
        cb -= PAGE_SIZE;
        pu32 += PAGE_SIZE / sizeof(uint32_t);
    }
}


/**
 * Read (sniff) every page in the range to make sure there are some page tables
 * entries backing it.
 *
 * This is just to be sure vm_protect didn't remove stuff without re-adding it
 * if someone should try write-protect something.
 *
 * @param  pv           Pointer to the first page.
 * @param  cb           The number of bytes.
 */
static void rtR0MemObjDarwinSniffPages(void const *pv, size_t cb)
{
    uint32_t volatile  *pu32 = (uint32_t volatile *)pv;
    uint32_t volatile   u32Counter = 0;
    for (;;)
    {
        u32Counter += *pu32;

        if (cb <= PAGE_SIZE)
            break;
        cb -= PAGE_SIZE;
        pu32 += PAGE_SIZE / sizeof(uint32_t);
    }
}


/**
 * Gets the virtual memory map the specified object is mapped into.
 *
 * @returns VM map handle on success, NULL if no map.
 * @param   pMem                The memory object.
 */
DECLINLINE(vm_map_t) rtR0MemObjDarwinGetMap(PRTR0MEMOBJINTERNAL pMem)
{
    switch (pMem->enmType)
    {
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_CONT:
            return kernel_map;

        case RTR0MEMOBJTYPE_PHYS:
        case RTR0MEMOBJTYPE_PHYS_NC:
            return NULL; /* pretend these have no mapping atm. */

        case RTR0MEMOBJTYPE_LOCK:
            return pMem->u.Lock.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : get_task_map((task_t)pMem->u.Lock.R0Process);

        case RTR0MEMOBJTYPE_RES_VIRT:
            return pMem->u.ResVirt.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : get_task_map((task_t)pMem->u.ResVirt.R0Process);

        case RTR0MEMOBJTYPE_MAPPING:
            return pMem->u.Mapping.R0Process == NIL_RTR0PROCESS
                ? kernel_map
                : get_task_map((task_t)pMem->u.Mapping.R0Process);

        default:
            return NULL;
    }
}

#if 0 /* not necessary after all*/
/* My vm_map mockup. */
struct my_vm_map
{
    struct { char pad[8]; } lock;
    struct my_vm_map_header
    {
        struct vm_map_links
        {
            void            *prev;
            void            *next;
            vm_map_offset_t start;
            vm_map_offset_t end;
        }                   links;
        int                 nentries;
        boolean_t           entries_pageable;
    }                       hdr;
    pmap_t                  pmap;
    vm_map_size_t           size;
};


/**
 * Gets the minimum map address, this is similar to get_map_min.
 *
 * @returns The start address of the map.
 * @param   pMap                The map.
 */
static vm_map_offset_t rtR0MemObjDarwinGetMapMin(vm_map_t pMap)
{
    /* lazy discovery of the correct offset. The apple guys is a wonderfully secretive bunch. */
    static int32_t volatile s_offAdjust = INT32_MAX;
    int32_t                 off         = s_offAdjust;
    if (off == INT32_MAX)
    {
        for (off = 0; ; off += sizeof(pmap_t))
        {
            if (*(pmap_t *)((uint8_t *)kernel_map + off) == kernel_pmap)
                break;
            AssertReturn(off <= RT_MAX(RT_OFFSETOF(struct my_vm_map, pmap) * 4, 1024), 0x1000);
        }
        ASMAtomicWriteS32(&s_offAdjust, off - RT_OFFSETOF(struct my_vm_map, pmap));
    }

    /* calculate it. */
    struct my_vm_map *pMyMap = (struct my_vm_map *)((uint8_t *)pMap + off);
    return pMyMap->hdr.links.start;
}
#endif /* unused */

#ifdef RT_STRICT
# if 0 /* unused */

/**
 * Read from a physical page.
 *
 * @param   HCPhys      The address to start reading at.
 * @param   cb          How many bytes to read.
 * @param   pvDst       Where to put the bytes. This is zero'd on failure.
 */
static void rtR0MemObjDarwinReadPhys(RTHCPHYS HCPhys, size_t cb, void *pvDst)
{
    memset(pvDst, '\0', cb);

    IOAddressRange      aRanges[1]  = { { (mach_vm_address_t)HCPhys, RT_ALIGN_Z(cb, PAGE_SIZE) } };
    IOMemoryDescriptor *pMemDesc    = IOMemoryDescriptor::withAddressRanges(&aRanges[0], RT_ELEMENTS(aRanges),
                                                                            kIODirectionIn, NULL /*task*/);
    if (pMemDesc)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
        IOMemoryMap *pMemMap = pMemDesc->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapDefaultCache);
#else
        IOMemoryMap *pMemMap = pMemDesc->map(kernel_task, 0, kIOMapAnywhere | kIOMapDefaultCache);
#endif
        if (pMemMap)
        {
            void const *pvSrc = (void const *)(uintptr_t)pMemMap->getVirtualAddress();
            memcpy(pvDst, pvSrc, cb);
            pMemMap->release();
        }
        else
            printf("rtR0MemObjDarwinReadPhys: createMappingInTask failed; HCPhys=%llx\n", HCPhys);

        pMemDesc->release();
    }
    else
        printf("rtR0MemObjDarwinReadPhys: withAddressRanges failed; HCPhys=%llx\n", HCPhys);
}


/**
 * Gets the PTE for a page.
 *
 * @returns the PTE.
 * @param   pvPage      The virtual address to get the PTE for.
 */
static uint64_t rtR0MemObjDarwinGetPTE(void *pvPage)
{
    RTUINT64U   u64;
    RTCCUINTREG cr3 = ASMGetCR3();
    RTCCUINTREG cr4 = ASMGetCR4();
    bool        fPAE = false;
    bool        fLMA = false;
    if (cr4 & X86_CR4_PAE)
    {
        fPAE = true;
        uint32_t fExtFeatures = ASMCpuId_EDX(0x80000001);
        if (fExtFeatures & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE)
        {
            uint64_t efer = ASMRdMsr(MSR_K6_EFER);
            if (efer & MSR_K6_EFER_LMA)
                fLMA = true;
        }
    }

    if (fLMA)
    {
        /* PML4 */
        rtR0MemObjDarwinReadPhys((cr3 & ~(RTCCUINTREG)PAGE_OFFSET_MASK) | (((uint64_t)(uintptr_t)pvPage >> X86_PML4_SHIFT) & X86_PML4_MASK) * 8, 8, &u64);
        if (!(u64.u & X86_PML4E_P))
        {
            printf("rtR0MemObjDarwinGetPTE: %p -> PML4E !p\n", pvPage);
            return 0;
        }

        /* PDPTR */
        rtR0MemObjDarwinReadPhys((u64.u & ~(uint64_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64) * 8, 8, &u64);
        if (!(u64.u & X86_PDPE_P))
        {
            printf("rtR0MemObjDarwinGetPTE: %p -> PDPTE !p\n", pvPage);
            return 0;
        }
        if (u64.u & X86_PDPE_LM_PS)
            return (u64.u & ~(uint64_t)(_1G -1)) | ((uintptr_t)pvPage & (_1G -1));

        /* PD */
        rtR0MemObjDarwinReadPhys((u64.u & ~(uint64_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK) * 8, 8, &u64);
        if (!(u64.u & X86_PDE_P))
        {
            printf("rtR0MemObjDarwinGetPTE: %p -> PDE !p\n", pvPage);
            return 0;
        }
        if (u64.u & X86_PDE_PS)
            return (u64.u & ~(uint64_t)(_2M -1)) | ((uintptr_t)pvPage & (_2M -1));

        /* PT */
        rtR0MemObjDarwinReadPhys((u64.u & ~(uint64_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK) * 8, 8, &u64);
        if (!(u64.u &  X86_PTE_P))
        {
            printf("rtR0MemObjDarwinGetPTE: %p -> PTE !p\n", pvPage);
            return 0;
        }
        return u64.u;
    }

    if (fPAE)
    {
        /* PDPTR */
        rtR0MemObjDarwinReadPhys((u64.u & X86_CR3_PAE_PAGE_MASK) | (((uintptr_t)pvPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE) * 8, 8, &u64);
        if (!(u64.u & X86_PDE_P))
            return 0;

        /* PD */
        rtR0MemObjDarwinReadPhys((u64.u & ~(uint64_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK) * 8, 8, &u64);
        if (!(u64.u & X86_PDE_P))
            return 0;
        if (u64.u & X86_PDE_PS)
            return (u64.u & ~(uint64_t)(_2M -1)) | ((uintptr_t)pvPage & (_2M -1));

        /* PT */
        rtR0MemObjDarwinReadPhys((u64.u & ~(uint64_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PT_PAE_SHIFT) & X86_PT_PAE_MASK) * 8, 8, &u64);
        if (!(u64.u & X86_PTE_P))
            return 0;
        return u64.u;
    }

    /* PD */
    rtR0MemObjDarwinReadPhys((u64.au32[0] & ~(uint32_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PD_SHIFT) & X86_PD_MASK) * 4, 4, &u64);
    if (!(u64.au32[0] & X86_PDE_P))
        return 0;
    if (u64.au32[0] & X86_PDE_PS)
        return (u64.u & ~(uint64_t)(_2M -1)) | ((uintptr_t)pvPage & (_2M -1));

    /* PT */
    rtR0MemObjDarwinReadPhys((u64.au32[0] & ~(uint32_t)PAGE_OFFSET_MASK) | (((uintptr_t)pvPage >> X86_PT_SHIFT) & X86_PT_MASK) * 4, 4, &u64);
    if (!(u64.au32[0] & X86_PTE_P))
        return 0;
    return u64.au32[0];

    return 0;
}

# endif /* unused */
#endif /* RT_STRICT */

DECLHIDDEN(int) rtR0MemObjNativeFree(RTR0MEMOBJ pMem)
{
    PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)pMem;
    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Release the IOMemoryDescriptor or/and IOMemoryMap associated with the object.
     */
    if (pMemDarwin->pMemDesc)
    {
        pMemDarwin->pMemDesc->complete();
        pMemDarwin->pMemDesc->release();
        pMemDarwin->pMemDesc = NULL;
    }

    if (pMemDarwin->pMemMap)
    {
        pMemDarwin->pMemMap->release();
        pMemDarwin->pMemMap = NULL;
    }

    /*
     * Release any memory that we've allocated or locked.
     */
    switch (pMemDarwin->Core.enmType)
    {
        case RTR0MEMOBJTYPE_LOW:
        case RTR0MEMOBJTYPE_PAGE:
        case RTR0MEMOBJTYPE_CONT:
            break;

        case RTR0MEMOBJTYPE_LOCK:
        {
#ifdef USE_VM_MAP_WIRE
            vm_map_t Map = pMemDarwin->Core.u.Lock.R0Process != NIL_RTR0PROCESS
                         ? get_task_map((task_t)pMemDarwin->Core.u.Lock.R0Process)
                         : kernel_map;
            kern_return_t kr = vm_map_unwire(Map,
                                             (vm_map_offset_t)pMemDarwin->Core.pv,
                                             (vm_map_offset_t)pMemDarwin->Core.pv + pMemDarwin->Core.cb,
                                             0 /* not user */);
            AssertRC(kr == KERN_SUCCESS); /** @todo don't ignore... */
#endif
            break;
        }

        case RTR0MEMOBJTYPE_PHYS:
            /*if (pMemDarwin->Core.u.Phys.fAllocated)
                IOFreePhysical(pMemDarwin->Core.u.Phys.PhysBase, pMemDarwin->Core.cb);*/
            Assert(!pMemDarwin->Core.u.Phys.fAllocated);
            break;

        case RTR0MEMOBJTYPE_PHYS_NC:
            AssertMsgFailed(("RTR0MEMOBJTYPE_PHYS_NC\n"));
            IPRT_DARWIN_RESTORE_EFL_AC();
            return VERR_INTERNAL_ERROR;

        case RTR0MEMOBJTYPE_RES_VIRT:
            AssertMsgFailed(("RTR0MEMOBJTYPE_RES_VIRT\n"));
            IPRT_DARWIN_RESTORE_EFL_AC();
            return VERR_INTERNAL_ERROR;

        case RTR0MEMOBJTYPE_MAPPING:
            /* nothing to do here. */
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", pMemDarwin->Core.enmType));
            IPRT_DARWIN_RESTORE_EFL_AC();
            return VERR_INTERNAL_ERROR;
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}



/**
 * Kernel memory alloc worker that uses inTaskWithPhysicalMask.
 *
 * @returns IPRT status code.
 * @retval  VERR_ADDRESS_TOO_BIG try another way.
 *
 * @param   ppMem           Where to return the memory object.
 * @param   cb              The page aligned memory size.
 * @param   fExecutable     Whether the mapping needs to be executable.
 * @param   fContiguous     Whether the backing memory needs to be contiguous.
 * @param   PhysMask        The mask for the backing memory (i.e. range). Use 0 if
 *                          you don't care that much or is speculating.
 * @param   MaxPhysAddr     The max address to verify the result against. Use
 *                          UINT64_MAX if it doesn't matter.
 * @param   enmType         The object type.
 */
static int rtR0MemObjNativeAllocWorker(PPRTR0MEMOBJINTERNAL ppMem, size_t cb,
                                       bool fExecutable, bool fContiguous,
                                       mach_vm_address_t PhysMask, uint64_t MaxPhysAddr,
                                       RTR0MEMOBJTYPE enmType)
{
    /*
     * Try inTaskWithPhysicalMask first, but since we don't quite trust that it
     * actually respects the physical memory mask (10.5.x is certainly busted),
     * we'll use rtR0MemObjNativeAllocCont as a fallback for dealing with that.
     *
     * The kIOMemoryKernelUserShared flag just forces the result to be page aligned.
     *
     * The kIOMemoryMapperNone flag is required since 10.8.2 (IOMMU changes?).
     */
    int rc;
    size_t cbFudged = cb;
    if (1) /** @todo Figure out why this is broken. Is it only on snow leopard? Seen allocating memory for the VM structure, last page corrupted or inaccessible. */
         cbFudged += PAGE_SIZE;
#if 1
    IOOptionBits fOptions = kIOMemoryKernelUserShared | kIODirectionInOut;
    if (fContiguous)
        fOptions |= kIOMemoryPhysicallyContiguous;
    if (version_major >= 12 /* 12 = 10.8.x = Mountain Kitten */)
        fOptions |= kIOMemoryMapperNone;
    IOBufferMemoryDescriptor *pMemDesc = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task, fOptions,
                                                                                          cbFudged, PhysMask);
#else /* Requires 10.7 SDK, but allows alignment to be specified: */
    uint64_t     uAlignment = PAGE_SIZE;
    IOOptionBits fOptions   = kIODirectionInOut | kIOMemoryMapperNone;
    if (fContiguous || MaxPhysAddr < UINT64_MAX)
    {
        fOptions  |= kIOMemoryPhysicallyContiguous;
        uAlignment = 1;                 /* PhysMask isn't respected if higher. */
    }

    IOBufferMemoryDescriptor *pMemDesc = new IOBufferMemoryDescriptor;
    if (pMemDesc && !pMemDesc->initWithPhysicalMask(kernel_task, fOptions, cbFudged, uAlignment, PhysMask))
    {
        pMemDesc->release();
        pMemDesc = NULL;
    }
#endif
    if (pMemDesc)
    {
        IOReturn IORet = pMemDesc->prepare(kIODirectionInOut);
        if (IORet == kIOReturnSuccess)
        {
            void *pv = pMemDesc->getBytesNoCopy(0, cbFudged);
            if (pv)
            {
                /*
                 * Check if it's all below 4GB.
                 */
                addr64_t AddrPrev = 0;
                MaxPhysAddr &= ~(uint64_t)PAGE_OFFSET_MASK;
                for (IOByteCount off = 0; off < cb; off += PAGE_SIZE)
                {
#ifdef __LP64__
                    addr64_t Addr = pMemDesc->getPhysicalSegment(off, NULL, kIOMemoryMapperNone);
#else
                    addr64_t Addr = pMemDesc->getPhysicalSegment64(off, NULL);
#endif
                    if (    Addr > MaxPhysAddr
                        ||  !Addr
                        || (Addr & PAGE_OFFSET_MASK)
                        ||  (   fContiguous
                             && !off
                             && Addr == AddrPrev + PAGE_SIZE))
                    {
                        /* Buggy API, try allocate the memory another way. */
                        pMemDesc->complete();
                        pMemDesc->release();
                        if (PhysMask)
                            LogRel(("rtR0MemObjNativeAllocWorker: off=%x Addr=%llx AddrPrev=%llx MaxPhysAddr=%llx PhysMas=%llx fContiguous=%RTbool fOptions=%#x - buggy API!\n",
                                    off, Addr, AddrPrev, MaxPhysAddr, PhysMask, fContiguous, fOptions));
                        return VERR_ADDRESS_TOO_BIG;
                    }
                    AddrPrev = Addr;
                }

#ifdef RT_STRICT
                /* check that the memory is actually mapped. */
                //addr64_t Addr = pMemDesc->getPhysicalSegment64(0, NULL);
                //printf("rtR0MemObjNativeAllocWorker: pv=%p %8llx %8llx\n", pv, rtR0MemObjDarwinGetPTE(pv), Addr);
                RTTHREADPREEMPTSTATE State = RTTHREADPREEMPTSTATE_INITIALIZER;
                RTThreadPreemptDisable(&State);
                rtR0MemObjDarwinTouchPages(pv, cb);
                RTThreadPreemptRestore(&State);
#endif

                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), enmType, pv, cb);
                if (pMemDarwin)
                {
                    if (fContiguous)
                    {
#ifdef __LP64__
                        addr64_t PhysBase64 = pMemDesc->getPhysicalSegment(0, NULL, kIOMemoryMapperNone);
#else
                        addr64_t PhysBase64 = pMemDesc->getPhysicalSegment64(0, NULL);
#endif
                        RTHCPHYS PhysBase = PhysBase64; Assert(PhysBase == PhysBase64);
                        if (enmType == RTR0MEMOBJTYPE_CONT)
                            pMemDarwin->Core.u.Cont.Phys = PhysBase;
                        else if (enmType == RTR0MEMOBJTYPE_PHYS)
                            pMemDarwin->Core.u.Phys.PhysBase = PhysBase;
                        else
                            AssertMsgFailed(("enmType=%d\n", enmType));
                    }

#if 1 /* Experimental code. */
                    if (fExecutable)
                    {
                        rc = rtR0MemObjNativeProtect(&pMemDarwin->Core, 0, cb, RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC);
# ifdef RT_STRICT
                        /* check that the memory is actually mapped. */
                        RTTHREADPREEMPTSTATE State2 = RTTHREADPREEMPTSTATE_INITIALIZER;
                        RTThreadPreemptDisable(&State2);
                        rtR0MemObjDarwinTouchPages(pv, cb);
                        RTThreadPreemptRestore(&State2);
# endif

                        /* Bug 6226: Ignore KERN_PROTECTION_FAILURE on Leopard and older. */
                        if (   rc == VERR_PERMISSION_DENIED
                            && version_major <= 10 /* 10 = 10.6.x = Snow Leopard. */)
                            rc = VINF_SUCCESS;
                    }
                    else
#endif
                        rc = VINF_SUCCESS;
                    if (RT_SUCCESS(rc))
                    {
                        pMemDarwin->pMemDesc = pMemDesc;
                        *ppMem = &pMemDarwin->Core;
                        return VINF_SUCCESS;
                    }

                    rtR0MemObjDelete(&pMemDarwin->Core);
                }

                if (enmType == RTR0MEMOBJTYPE_PHYS_NC)
                    rc = VERR_NO_PHYS_MEMORY;
                else if (enmType == RTR0MEMOBJTYPE_LOW)
                    rc = VERR_NO_LOW_MEMORY;
                else if (enmType == RTR0MEMOBJTYPE_CONT)
                    rc = VERR_NO_CONT_MEMORY;
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_MEMOBJ_INIT_FAILED;

            pMemDesc->complete();
        }
        else
            rc = RTErrConvertFromDarwinIO(IORet);
        pMemDesc->release();
    }
    else
        rc = VERR_MEMOBJ_INIT_FAILED;
    Assert(rc != VERR_ADDRESS_TOO_BIG);
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPage(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    int rc = rtR0MemObjNativeAllocWorker(ppMem, cb, fExecutable, false /* fContiguous */,
                                         0 /* PhysMask */, UINT64_MAX, RTR0MEMOBJTYPE_PAGE);

    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocLow(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Try IOMallocPhysical/IOMallocAligned first.
     * Then try optimistically without a physical address mask, which will always
     * end up using IOMallocAligned.
     *
     * (See bug comment in the worker and IOBufferMemoryDescriptor::initWithPhysicalMask.)
     */
    int rc = rtR0MemObjNativeAllocWorker(ppMem, cb, fExecutable, false /* fContiguous */,
                                         ~(uint32_t)PAGE_OFFSET_MASK, _4G - PAGE_SIZE, RTR0MEMOBJTYPE_LOW);
    if (rc == VERR_ADDRESS_TOO_BIG)
        rc = rtR0MemObjNativeAllocWorker(ppMem, cb, fExecutable, false /* fContiguous */,
                                         0 /* PhysMask */, _4G - PAGE_SIZE, RTR0MEMOBJTYPE_LOW);

    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocCont(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, bool fExecutable)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    int rc = rtR0MemObjNativeAllocWorker(ppMem, cb, fExecutable, true /* fContiguous */,
                                         ~(uint32_t)PAGE_OFFSET_MASK, _4G - PAGE_SIZE,
                                         RTR0MEMOBJTYPE_CONT);

    /*
     * Workaround for bogus IOKernelAllocateContiguous behavior, just in case.
     * cb <= PAGE_SIZE allocations take a different path, using a different allocator.
     */
    if (RT_FAILURE(rc) && cb <= PAGE_SIZE)
        rc = rtR0MemObjNativeAllocWorker(ppMem, cb + PAGE_SIZE, fExecutable, true /* fContiguous */,
                                         ~(uint32_t)PAGE_OFFSET_MASK, _4G - PAGE_SIZE,
                                         RTR0MEMOBJTYPE_CONT);
    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhys(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest, size_t uAlignment)
{
    /** @todo alignment */
    if (uAlignment != PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Translate the PhysHighest address into a mask.
     */
    int rc;
    if (PhysHighest == NIL_RTHCPHYS)
        rc = rtR0MemObjNativeAllocWorker(ppMem, cb, true /* fExecutable */, true /* fContiguous */,
                                         0 /* PhysMask*/, UINT64_MAX, RTR0MEMOBJTYPE_PHYS);
    else
    {
        mach_vm_address_t PhysMask = 0;
        PhysMask = ~(mach_vm_address_t)0;
        while (PhysMask > (PhysHighest | PAGE_OFFSET_MASK))
            PhysMask >>= 1;
        AssertReturn(PhysMask + 1 <= cb, VERR_INVALID_PARAMETER);
        PhysMask &= ~(mach_vm_address_t)PAGE_OFFSET_MASK;

        rc = rtR0MemObjNativeAllocWorker(ppMem, cb, true /* fExecutable */, true /* fContiguous */,
                                         PhysMask, PhysHighest, RTR0MEMOBJTYPE_PHYS);
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeAllocPhysNC(PPRTR0MEMOBJINTERNAL ppMem, size_t cb, RTHCPHYS PhysHighest)
{
    /** @todo rtR0MemObjNativeAllocPhys / darwin.
     * This might be a bit problematic and may very well require having to create our own
     * object which we populate with pages but without mapping it into any address space.
     * Estimate is 2-3 days.
     */
    RT_NOREF(ppMem, cb, PhysHighest);
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(int) rtR0MemObjNativeEnterPhys(PPRTR0MEMOBJINTERNAL ppMem, RTHCPHYS Phys, size_t cb, uint32_t uCachePolicy)
{
    AssertReturn(uCachePolicy == RTMEM_CACHE_POLICY_DONT_CARE, VERR_NOT_SUPPORTED);
    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Create a descriptor for it (the validation is always true on intel macs, but
     * as it doesn't harm us keep it in).
     */
    int rc = VERR_ADDRESS_TOO_BIG;
    IOAddressRange aRanges[1] = { { Phys, cb } };
    if (    aRanges[0].address == Phys
        &&  aRanges[0].length == cb)
    {
        IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddressRanges(&aRanges[0], RT_ELEMENTS(aRanges),
                                                                             kIODirectionInOut, NULL /*task*/);
        if (pMemDesc)
        {
#ifdef __LP64__
            Assert(Phys == pMemDesc->getPhysicalSegment(0, NULL, kIOMemoryMapperNone));
#else
            Assert(Phys == pMemDesc->getPhysicalSegment64(0, NULL));
#endif

            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_PHYS, NULL, cb);
            if (pMemDarwin)
            {
                pMemDarwin->Core.u.Phys.PhysBase = Phys;
                pMemDarwin->Core.u.Phys.fAllocated = false;
                pMemDarwin->Core.u.Phys.uCachePolicy = uCachePolicy;
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;
                IPRT_DARWIN_RESTORE_EFL_AC();
                return VINF_SUCCESS;
            }

            rc = VERR_NO_MEMORY;
            pMemDesc->release();
        }
        else
            rc = VERR_MEMOBJ_INIT_FAILED;
    }
    else
        AssertMsgFailed(("%#llx %llx\n", (unsigned long long)Phys, (unsigned long long)cb));
    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


/**
 * Internal worker for locking down pages.
 *
 * @return IPRT status code.
 *
 * @param   ppMem           Where to store the memory object pointer.
 * @param   pv              First page.
 * @param   cb              Number of bytes.
 * @param   fAccess         The desired access, a combination of RTMEM_PROT_READ
 *                          and RTMEM_PROT_WRITE.
 * @param   Task            The task \a pv and \a cb refers to.
 */
static int rtR0MemObjNativeLock(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess, task_t Task)
{
    IPRT_DARWIN_SAVE_EFL_AC();
    NOREF(fAccess);
#ifdef USE_VM_MAP_WIRE
    vm_map_t Map = get_task_map(Task);
    Assert(Map);

    /*
     * First try lock the memory.
     */
    int rc = VERR_LOCK_FAILED;
    kern_return_t kr = vm_map_wire(get_task_map(Task),
                                   (vm_map_offset_t)pv,
                                   (vm_map_offset_t)pv + cb,
                                   VM_PROT_DEFAULT,
                                   0 /* not user */);
    if (kr == KERN_SUCCESS)
    {
        /*
         * Create the IPRT memory object.
         */
        PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_LOCK, pv, cb);
        if (pMemDarwin)
        {
            pMemDarwin->Core.u.Lock.R0Process = (RTR0PROCESS)Task;
            *ppMem = &pMemDarwin->Core;

            IPRT_DARWIN_RESTORE_EFL_AC();
            return VINF_SUCCESS;
        }

        kr = vm_map_unwire(get_task_map(Task), (vm_map_offset_t)pv, (vm_map_offset_t)pv + cb, 0 /* not user */);
        Assert(kr == KERN_SUCCESS);
        rc = VERR_NO_MEMORY;
    }

#else

    /*
     * Create a descriptor and try lock it (prepare).
     */
    int rc = VERR_MEMOBJ_INIT_FAILED;
    IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withAddressRange((vm_address_t)pv, cb, kIODirectionInOut, Task);
    if (pMemDesc)
    {
        IOReturn IORet = pMemDesc->prepare(kIODirectionInOut);
        if (IORet == kIOReturnSuccess)
        {
            /*
             * Create the IPRT memory object.
             */
            PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_LOCK, pv, cb);
            if (pMemDarwin)
            {
                pMemDarwin->Core.u.Lock.R0Process = (RTR0PROCESS)Task;
                pMemDarwin->pMemDesc = pMemDesc;
                *ppMem = &pMemDarwin->Core;

                IPRT_DARWIN_RESTORE_EFL_AC();
                return VINF_SUCCESS;
            }

            pMemDesc->complete();
            rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_LOCK_FAILED;
        pMemDesc->release();
    }
#endif
    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeLockUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3Ptr, size_t cb, uint32_t fAccess, RTR0PROCESS R0Process)
{
    return rtR0MemObjNativeLock(ppMem, (void *)R3Ptr, cb, fAccess, (task_t)R0Process);
}


DECLHIDDEN(int) rtR0MemObjNativeLockKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pv, size_t cb, uint32_t fAccess)
{
    return rtR0MemObjNativeLock(ppMem, pv, cb, fAccess, kernel_task);
}


DECLHIDDEN(int) rtR0MemObjNativeReserveKernel(PPRTR0MEMOBJINTERNAL ppMem, void *pvFixed, size_t cb, size_t uAlignment)
{
    RT_NOREF(ppMem, pvFixed, cb, uAlignment);
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(int) rtR0MemObjNativeReserveUser(PPRTR0MEMOBJINTERNAL ppMem, RTR3PTR R3PtrFixed, size_t cb, size_t uAlignment, RTR0PROCESS R0Process)
{
    RT_NOREF(ppMem, R3PtrFixed, cb, uAlignment, R0Process);
    return VERR_NOT_SUPPORTED;
}


DECLHIDDEN(int) rtR0MemObjNativeMapKernel(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, void *pvFixed, size_t uAlignment,
                                          unsigned fProt, size_t offSub, size_t cbSub)
{
    RT_NOREF(fProt);
    AssertReturn(pvFixed == (void *)-1, VERR_NOT_SUPPORTED);

    /*
     * Check that the specified alignment is supported.
     */
    if (uAlignment > PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Must have a memory descriptor that we can map.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJDARWIN pMemToMapDarwin = (PRTR0MEMOBJDARWIN)pMemToMap;
    if (pMemToMapDarwin->pMemDesc)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->createMappingInTask(kernel_task,
                                                                              0,
                                                                              kIOMapAnywhere | kIOMapDefaultCache,
                                                                              offSub,
                                                                              cbSub);
#else
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->map(kernel_task,
                                                              0,
                                                              kIOMapAnywhere | kIOMapDefaultCache,
                                                              offSub,
                                                              cbSub);
#endif
        if (pMemMap)
        {
            IOVirtualAddress VirtAddr = pMemMap->getVirtualAddress();
            void *pv = (void *)(uintptr_t)VirtAddr;
            if ((uintptr_t)pv == VirtAddr)
            {
                //addr64_t Addr = pMemToMapDarwin->pMemDesc->getPhysicalSegment64(offSub, NULL);
                //printf("pv=%p: %8llx %8llx\n", pv, rtR0MemObjDarwinGetPTE(pv), Addr);

//                /*
//                 * Explicitly lock it so that we're sure it is present and that
//                 * its PTEs cannot be recycled.
//                 * Note! withAddressRange() doesn't work as it adds kIOMemoryTypeVirtual64
//                 *       to the options which causes prepare() to not wire the pages.
//                 *       This is probably a bug.
//                 */
//                IOAddressRange Range = { (mach_vm_address_t)pv, cbSub };
//                IOMemoryDescriptor *pMemDesc = IOMemoryDescriptor::withOptions(&Range,
//                                                                               1 /* count */,
//                                                                               0 /* offset */,
//                                                                               kernel_task,
//                                                                               kIODirectionInOut | kIOMemoryTypeVirtual,
//                                                                               kIOMapperSystem);
//                if (pMemDesc)
//                {
//                    IOReturn IORet = pMemDesc->prepare(kIODirectionInOut);
//                    if (IORet == kIOReturnSuccess)
//                    {
                        /* HACK ALERT! */
                        rtR0MemObjDarwinTouchPages(pv, cbSub);
                        /** @todo First, the memory should've been mapped by now, and second, it
                         *        should have the wired attribute in the PTE (bit 9). Neither
                         *        seems to be the case. The disabled locking code doesn't make any
                         *        difference, which is extremely odd, and breaks
                         *        rtR0MemObjNativeGetPagePhysAddr (getPhysicalSegment64 -> 64 for the
                         *        lock descriptor. */
                        //addr64_t Addr = pMemDesc->getPhysicalSegment64(0, NULL);
                        //printf("pv=%p: %8llx %8llx (%d)\n", pv, rtR0MemObjDarwinGetPTE(pv), Addr, 2);

                        /*
                         * Create the IPRT memory object.
                         */
                        PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_MAPPING,
                                                                                        pv, cbSub);
                        if (pMemDarwin)
                        {
                            pMemDarwin->Core.u.Mapping.R0Process = NIL_RTR0PROCESS;
                            pMemDarwin->pMemMap = pMemMap;
//                            pMemDarwin->pMemDesc = pMemDesc;
                            *ppMem = &pMemDarwin->Core;

                            IPRT_DARWIN_RESTORE_EFL_AC();
                            return VINF_SUCCESS;
                        }

//                        pMemDesc->complete();
//                        rc = VERR_NO_MEMORY;
//                    }
//                    else
//                        rc = RTErrConvertFromDarwinIO(IORet);
//                    pMemDesc->release();
//                }
//                else
//                    rc = VERR_MEMOBJ_INIT_FAILED;
            }
            else
                rc = VERR_ADDRESS_TOO_BIG;
            pMemMap->release();
        }
        else
            rc = VERR_MAP_FAILED;
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeMapUser(PPRTR0MEMOBJINTERNAL ppMem, RTR0MEMOBJ pMemToMap, RTR3PTR R3PtrFixed, size_t uAlignment,
                                        unsigned fProt, RTR0PROCESS R0Process)
{
    RT_NOREF(fProt);

    /*
     * Check for unsupported things.
     */
    AssertReturn(R3PtrFixed == (RTR3PTR)-1, VERR_NOT_SUPPORTED);
    if (uAlignment > PAGE_SIZE)
        return VERR_NOT_SUPPORTED;

    IPRT_DARWIN_SAVE_EFL_AC();

    /*
     * Must have a memory descriptor.
     */
    int rc = VERR_INVALID_PARAMETER;
    PRTR0MEMOBJDARWIN pMemToMapDarwin = (PRTR0MEMOBJDARWIN)pMemToMap;
    if (pMemToMapDarwin->pMemDesc)
    {
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1050
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->createMappingInTask((task_t)R0Process,
                                                                              0,
                                                                              kIOMapAnywhere | kIOMapDefaultCache,
                                                                              0 /* offset */,
                                                                              0 /* length */);
#else
        IOMemoryMap *pMemMap = pMemToMapDarwin->pMemDesc->map((task_t)R0Process,
                                                              0,
                                                              kIOMapAnywhere | kIOMapDefaultCache);
#endif
        if (pMemMap)
        {
            IOVirtualAddress VirtAddr = pMemMap->getVirtualAddress();
            void *pv = (void *)(uintptr_t)VirtAddr;
            if ((uintptr_t)pv == VirtAddr)
            {
                /*
                 * Create the IPRT memory object.
                 */
                PRTR0MEMOBJDARWIN pMemDarwin = (PRTR0MEMOBJDARWIN)rtR0MemObjNew(sizeof(*pMemDarwin), RTR0MEMOBJTYPE_MAPPING,
                                                                                pv, pMemToMapDarwin->Core.cb);
                if (pMemDarwin)
                {
                    pMemDarwin->Core.u.Mapping.R0Process = R0Process;
                    pMemDarwin->pMemMap = pMemMap;
                    *ppMem = &pMemDarwin->Core;

                    IPRT_DARWIN_RESTORE_EFL_AC();
                    return VINF_SUCCESS;
                }

                rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_ADDRESS_TOO_BIG;
            pMemMap->release();
        }
        else
            rc = VERR_MAP_FAILED;
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return rc;
}


DECLHIDDEN(int) rtR0MemObjNativeProtect(PRTR0MEMOBJINTERNAL pMem, size_t offSub, size_t cbSub, uint32_t fProt)
{
    IPRT_DARWIN_SAVE_EFL_AC();

    /* Get the map for the object. */
    vm_map_t pVmMap = rtR0MemObjDarwinGetMap(pMem);
    if (!pVmMap)
    {
        IPRT_DARWIN_RESTORE_EFL_AC();
        return VERR_NOT_SUPPORTED;
    }

    /*
     * Convert the protection.
     */
    vm_prot_t fMachProt;
    switch (fProt)
    {
        case RTMEM_PROT_NONE:
            fMachProt = VM_PROT_NONE;
            break;
        case RTMEM_PROT_READ:
            fMachProt = VM_PROT_READ;
            break;
        case RTMEM_PROT_READ | RTMEM_PROT_WRITE:
            fMachProt = VM_PROT_READ | VM_PROT_WRITE;
            break;
        case RTMEM_PROT_READ | RTMEM_PROT_WRITE | RTMEM_PROT_EXEC:
            fMachProt = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
            break;
        case RTMEM_PROT_WRITE:
            fMachProt = VM_PROT_WRITE | VM_PROT_READ;                   /* never write-only */
            break;
        case RTMEM_PROT_WRITE | RTMEM_PROT_EXEC:
            fMachProt = VM_PROT_WRITE | VM_PROT_EXECUTE | VM_PROT_READ; /* never write-only or execute-only */
            break;
        case RTMEM_PROT_EXEC:
            fMachProt = VM_PROT_EXECUTE | VM_PROT_READ;                 /* never execute-only */
            break;
        default:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }

    /*
     * Do the job.
     */
    vm_offset_t Start = (uintptr_t)pMem->pv + offSub;
    kern_return_t krc = vm_protect(pVmMap,
                                   Start,
                                   cbSub,
                                   false,
                                   fMachProt);
    if (krc != KERN_SUCCESS)
    {
        static int s_cComplaints = 0;
        if (s_cComplaints < 10)
        {
            s_cComplaints++;
            printf("rtR0MemObjNativeProtect: vm_protect(%p,%p,%p,false,%#x) -> %d\n",
                   pVmMap, (void *)Start, (void *)cbSub, fMachProt, krc);

            kern_return_t               krc2;
            vm_offset_t                 pvReal = Start;
            vm_size_t                   cbReal = 0;
            mach_msg_type_number_t      cInfo  = VM_REGION_BASIC_INFO_COUNT;
            struct vm_region_basic_info Info;
            RT_ZERO(Info);
            krc2 = vm_region(pVmMap, &pvReal, &cbReal, VM_REGION_BASIC_INFO, (vm_region_info_t)&Info, &cInfo, NULL);
            printf("rtR0MemObjNativeProtect: basic info - krc2=%d pv=%p cb=%p prot=%#x max=%#x inh=%#x shr=%d rvd=%d off=%#x behavior=%#x wired=%#x\n",
                   krc2, (void *)pvReal, (void *)cbReal, Info.protection, Info.max_protection,  Info.inheritance,
                   Info.shared, Info.reserved, Info.offset, Info.behavior, Info.user_wired_count);
        }
        IPRT_DARWIN_RESTORE_EFL_AC();
        return RTErrConvertFromDarwinKern(krc);
    }

    /*
     * Touch the pages if they should be writable afterwards and accessible
     * from code which should never fault. vm_protect() may leave pages
     * temporarily write protected, possibly due to pmap no-upgrade rules?
     *
     * This is the same trick (or HACK ALERT if you like) as applied in
     * rtR0MemObjNativeMapKernel.
     */
    if (   pMem->enmType != RTR0MEMOBJTYPE_MAPPING
        || pMem->u.Mapping.R0Process == NIL_RTR0PROCESS)
    {
        if (fProt & RTMEM_PROT_WRITE)
            rtR0MemObjDarwinTouchPages((void *)Start, cbSub);
        /*
         * Sniff (read) read-only pages too, just to be sure.
         */
        else if (fProt & (RTMEM_PROT_READ | RTMEM_PROT_EXEC))
            rtR0MemObjDarwinSniffPages((void const *)Start, cbSub);
    }

    IPRT_DARWIN_RESTORE_EFL_AC();
    return VINF_SUCCESS;
}


DECLHIDDEN(RTHCPHYS) rtR0MemObjNativeGetPagePhysAddr(PRTR0MEMOBJINTERNAL pMem, size_t iPage)
{
    RTHCPHYS            PhysAddr;
    PRTR0MEMOBJDARWIN   pMemDarwin = (PRTR0MEMOBJDARWIN)pMem;
    IPRT_DARWIN_SAVE_EFL_AC();

#ifdef USE_VM_MAP_WIRE
    /*
     * Locked memory doesn't have a memory descriptor and
     * needs to be handled differently.
     */
    if (pMemDarwin->Core.enmType == RTR0MEMOBJTYPE_LOCK)
    {
        ppnum_t PgNo;
        if (pMemDarwin->Core.u.Lock.R0Process == NIL_RTR0PROCESS)
            PgNo = pmap_find_phys(kernel_pmap, (uintptr_t)pMemDarwin->Core.pv + iPage * PAGE_SIZE);
        else
        {
            /*
             * From what I can tell, Apple seems to have locked up the all the
             * available interfaces that could help us obtain the pmap_t of a task
             * or vm_map_t.

             * So, we'll have to figure out where in the vm_map_t  structure it is
             * and read it our selves. ASSUMING that kernel_pmap is pointed to by
             * kernel_map->pmap, we scan kernel_map to locate the structure offset.
             * Not nice, but it will hopefully do the job in a reliable manner...
             *
             * (get_task_pmap, get_map_pmap or vm_map_pmap is what we really need btw.)
             */
            static int s_offPmap = -1;
            if (RT_UNLIKELY(s_offPmap == -1))
            {
                pmap_t const *p = (pmap_t *)kernel_map;
                pmap_t const * const pEnd = p + 64;
                for (; p < pEnd; p++)
                    if (*p == kernel_pmap)
                    {
                        s_offPmap = (uintptr_t)p - (uintptr_t)kernel_map;
                        break;
                    }
                AssertReturn(s_offPmap >= 0, NIL_RTHCPHYS);
            }
            pmap_t Pmap = *(pmap_t *)((uintptr_t)get_task_map((task_t)pMemDarwin->Core.u.Lock.R0Process) + s_offPmap);
            PgNo = pmap_find_phys(Pmap, (uintptr_t)pMemDarwin->Core.pv + iPage * PAGE_SIZE);
        }

        IPRT_DARWIN_RESTORE_EFL_AC();
        AssertReturn(PgNo, NIL_RTHCPHYS);
        PhysAddr = (RTHCPHYS)PgNo << PAGE_SHIFT;
        Assert((PhysAddr >> PAGE_SHIFT) == PgNo);
    }
    else
#endif /* USE_VM_MAP_WIRE */
    {
        /*
         * Get the memory descriptor.
         */
        IOMemoryDescriptor *pMemDesc = pMemDarwin->pMemDesc;
        if (!pMemDesc)
            pMemDesc = pMemDarwin->pMemMap->getMemoryDescriptor();
        AssertReturn(pMemDesc, NIL_RTHCPHYS);

        /*
         * If we've got a memory descriptor, use getPhysicalSegment64().
         */
#ifdef __LP64__
        addr64_t Addr = pMemDesc->getPhysicalSegment(iPage * PAGE_SIZE, NULL, kIOMemoryMapperNone);
#else
        addr64_t Addr = pMemDesc->getPhysicalSegment64(iPage * PAGE_SIZE, NULL);
#endif
        IPRT_DARWIN_RESTORE_EFL_AC();
        AssertMsgReturn(Addr, ("iPage=%u\n", iPage), NIL_RTHCPHYS);
        PhysAddr = Addr;
        AssertMsgReturn(PhysAddr == Addr, ("PhysAddr=%RHp Addr=%RX64\n", PhysAddr, (uint64_t)Addr), NIL_RTHCPHYS);
    }

    return PhysAddr;
}

