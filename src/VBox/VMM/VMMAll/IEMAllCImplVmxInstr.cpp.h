/* $Id$ */
/** @file
 * IEM - VT-x instruction implementation.
 */

/*
 * Copyright (C) 2011-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/**
 * Implements 'VMCALL'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmcall)
{
    /** @todo NSTVMX: intercept. */

    /* Join forces with vmmcall. */
    return IEM_CIMPL_CALL_1(iemCImpl_Hypercall, OP_VMCALL);
}

#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/**
 * Map of VMCS field encodings to their virtual-VMCS structure offsets.
 *
 * The first array dimension is VMCS field encoding of Width OR'ed with Type and the
 * second dimension is the Index, see VMXVMCSFIELDENC.
 */
uint16_t const g_aoffVmcsMap[16][VMX_V_VMCS_MAX_INDEX + 1] =
{
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u16Vpid),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u16PostIntNotifyVector),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u16EptpIndex),
        /*  3-10 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 11-18 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 19-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*   0-7 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, GuestEs),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, GuestCs),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, GuestSs),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, GuestDs),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, GuestFs),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, GuestGs),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, GuestLdtr),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, GuestTr),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u16GuestIntStatus),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u16PmlIndex),
        /* 10-17 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_16BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, HostEs),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, HostCs),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, HostSs),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, HostDs),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, HostFs),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, HostGs),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, HostTr),
        /*  7-14 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 15-22 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 23-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64AddrIoBitmapA),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64AddrIoBitmapB),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64AddrMsrBitmap),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64AddrExitMsrStore),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64AddrExitMsrLoad),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64AddrEntryMsrLoad),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64ExecVmcsPtr),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64AddrPml),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64TscOffset),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64AddrVirtApic),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64AddrApicAccess),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64AddrPostedIntDesc),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u64VmFuncCtls),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u64EptpPtr),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap0),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap1),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap2),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u64EoiExitBitmap3),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u64AddrEptpList),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmreadBitmap),
        /*    20 */ RT_OFFSETOF(VMXVVMCS, u64AddrVmwriteBitmap),
        /*    21 */ RT_OFFSETOF(VMXVVMCS, u64AddrXcptVeInfo),
        /*    22 */ RT_OFFSETOF(VMXVVMCS, u64AddrXssBitmap),
        /*    23 */ RT_OFFSETOF(VMXVVMCS, u64AddrEnclsBitmap),
        /*    24 */ UINT16_MAX,
        /*    25 */ RT_OFFSETOF(VMXVVMCS, u64TscMultiplier)
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64GuestPhysAddr),
        /*   1-8 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  9-16 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 17-24 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*    25 */ UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64VmcsLinkPtr),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64GuestDebugCtlMsr),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64GuestPatMsr),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64GuestEferMsr),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64GuestPerfGlobalCtlMsr),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte0),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte1),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte2),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64GuestPdpte3),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64GuestBndcfgsMsr),
        /* 10-17 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_64BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64HostPatMsr),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64HostEferMsr),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64HostPerfGlobalCtlMsr),
        /*  3-10 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 11-18 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 19-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32PinCtls),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32ProcCtls),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32XcptBitmap),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32XcptPFMask),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32XcptPFMatch),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32Cr3TargetCount),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32ExitCtls),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32ExitMsrStoreCount),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u32ExitMsrLoadCount),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u32EntryCtls),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u32EntryMsrLoadCount),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u32EntryIntInfo),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u32EntryXcptErrCode),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u32EntryInstrLen),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u32TprThreshold),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u32ProcCtls2),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u32PleGap),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u32PleWindow),
        /* 18-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32RoVmInstrError),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32RoExitReason),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32RoExitIntInfo),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32RoExitErrCode),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringInfo),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32RoIdtVectoringErrCode),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32RoExitInstrLen),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32RoExitInstrInfo),
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsLimit),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u32GuestCsLimit),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u32GuestSsLimit),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u32GuestDsLimit),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsLimit),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u32GuestFsLimit),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u32GuestGsLimit),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u32GuestLdtrLimit),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u32GuestTrLimit),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u32GuestGdtrLimit),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u32GuestIdtrLimit),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u32GuestEsAttr),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u32GuestCsAttr),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u32GuestSsAttr),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u32GuestDsAttr),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u32GuestFsAttr),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u32GuestGsAttr),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u32GuestLdtrAttr),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u32GuestTrAttr),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u32GuestIntrState),
        /*    20 */ RT_OFFSETOF(VMXVVMCS, u32GuestActivityState),
        /*    21 */ RT_OFFSETOF(VMXVVMCS, u32GuestSmBase),
        /*    22 */ RT_OFFSETOF(VMXVVMCS, u32GuestSysenterCS),
        /*    23 */ RT_OFFSETOF(VMXVVMCS, u32PreemptTimer),
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_32BIT | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u32HostSysenterCs),
        /*   1-8 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*  9-16 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 17-24 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /*    25 */ UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_CONTROL: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64Cr0Mask),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64Cr4Mask),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64Cr0ReadShadow),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64Cr4ReadShadow),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target0),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target1),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target2),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64Cr3Target3),
        /*  8-15 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 16-23 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 24-25 */ UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_VMEXIT_INFO: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64ExitQual),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64IoRcx),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64IoRsi),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64IoRdi),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64IoRip),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestLinearAddr),
        /*  6-13 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 14-21 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 22-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_GUEST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr0),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr3),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64GuestCr4),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64GuestEsBase),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64GuestCsBase),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64GuestSsBase),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64GuestDsBase),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64GuestFsBase),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64GuestGsBase),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64GuestLdtrBase),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64GuestTrBase),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64GuestGdtrBase),
        /*    12 */ RT_OFFSETOF(VMXVVMCS, u64GuestIdtrBase),
        /*    13 */ RT_OFFSETOF(VMXVVMCS, u64GuestDr7),
        /*    14 */ RT_OFFSETOF(VMXVVMCS, u64GuestRsp),
        /*    15 */ RT_OFFSETOF(VMXVVMCS, u64GuestRip),
        /*    16 */ RT_OFFSETOF(VMXVVMCS, u64GuestRFlags),
        /*    17 */ RT_OFFSETOF(VMXVVMCS, u64GuestPendingDbgXcpt),
        /*    18 */ RT_OFFSETOF(VMXVVMCS, u64GuestSysenterEsp),
        /*    19 */ RT_OFFSETOF(VMXVVMCS, u64GuestSysenterEip),
        /* 20-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    },
    /* VMX_VMCS_ENC_WIDTH_NATURAL | VMX_VMCS_ENC_TYPE_HOST_STATE: */
    {
        /*     0 */ RT_OFFSETOF(VMXVVMCS, u64HostCr0),
        /*     1 */ RT_OFFSETOF(VMXVVMCS, u64HostCr3),
        /*     2 */ RT_OFFSETOF(VMXVVMCS, u64HostCr4),
        /*     3 */ RT_OFFSETOF(VMXVVMCS, u64HostFsBase),
        /*     4 */ RT_OFFSETOF(VMXVVMCS, u64HostGsBase),
        /*     5 */ RT_OFFSETOF(VMXVVMCS, u64HostTrBase),
        /*     6 */ RT_OFFSETOF(VMXVVMCS, u64HostGdtrBase),
        /*     7 */ RT_OFFSETOF(VMXVVMCS, u64HostIdtrBase),
        /*     8 */ RT_OFFSETOF(VMXVVMCS, u64HostSysenterEsp),
        /*     9 */ RT_OFFSETOF(VMXVVMCS, u64HostSysenterEip),
        /*    10 */ RT_OFFSETOF(VMXVVMCS, u64HostRsp),
        /*    11 */ RT_OFFSETOF(VMXVVMCS, u64HostRip),
        /* 12-19 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
        /* 20-25 */ UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX
    }
};


/**
 * Gets the ModR/M, SIB and displacement byte(s) from decoded opcodes given their
 * relative offsets.
 */
# ifdef IEM_WITH_CODE_TLB
#  define IEM_MODRM_GET_U8(a_pVCpu, a_bModRm, a_offModRm)         do { } while (0)
#  define IEM_SIB_GET_U8(a_pVCpu, a_bSib, a_offSib)               do { } while (0)
#  define IEM_DISP_GET_U16(a_pVCpu, a_u16Disp, a_offDisp)         do { } while (0)
#  define IEM_DISP_GET_S8_SX_U16(a_pVCpu, a_u16Disp, a_offDisp)   do { } while (0)
#  define IEM_DISP_GET_U32(a_pVCpu, a_u32Disp, a_offDisp)         do { } while (0)
#  define IEM_DISP_GET_S8_SX_U32(a_pVCpu, a_u32Disp, a_offDisp)   do { } while (0)
#  define IEM_DISP_GET_S32_SX_U64(a_pVCpu, a_u64Disp, a_offDisp)  do { } while (0)
#  define IEM_DISP_GET_S8_SX_U64(a_pVCpu, a_u64Disp, a_offDisp)   do { } while (0)
#  error "Implement me: Getting ModR/M, SIB, displacement needs to work even when instruction crosses a page boundary."
# else  /* !IEM_WITH_CODE_TLB */
#  define IEM_MODRM_GET_U8(a_pVCpu, a_bModRm, a_offModRm) \
    do \
    { \
        Assert((a_offModRm) < (a_pVCpu)->iem.s.cbOpcode); \
        (a_bModRm) = (a_pVCpu)->iem.s.abOpcode[(a_offModRm)]; \
    } while (0)

#  define IEM_SIB_GET_U8(a_pVCpu, a_bSib, a_offSib)      IEM_MODRM_GET_U8(a_pVCpu, a_bSib, a_offSib)

#  define IEM_DISP_GET_U16(a_pVCpu, a_u16Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmpLo = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmpHi = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        (a_u16Disp) = RT_MAKE_U16(bTmpLo, bTmpHi); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U16(a_pVCpu, a_u16Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u16Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_U32(a_pVCpu, a_u32Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 3 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmp0 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmp1 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        uint8_t const bTmp2 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 2]; \
        uint8_t const bTmp3 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 3]; \
        (a_u32Disp) = RT_MAKE_U32_FROM_U8(bTmp0, bTmp1, bTmp2, bTmp3); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U32(a_pVCpu, a_u32Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u32Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_S8_SX_U64(a_pVCpu, a_u64Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 1 < (a_pVCpu)->iem.s.cbOpcode); \
        (a_u64Disp) = (int8_t)((a_pVCpu)->iem.s.abOpcode[(a_offDisp)]); \
    } while (0)

#  define IEM_DISP_GET_S32_SX_U64(a_pVCpu, a_u64Disp, a_offDisp) \
    do \
    { \
        Assert((a_offDisp) + 3 < (a_pVCpu)->iem.s.cbOpcode); \
        uint8_t const bTmp0 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp)]; \
        uint8_t const bTmp1 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 1]; \
        uint8_t const bTmp2 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 2]; \
        uint8_t const bTmp3 = (a_pVCpu)->iem.s.abOpcode[(a_offDisp) + 3]; \
        (a_u64Disp) = (int32_t)RT_MAKE_U32_FROM_U8(bTmp0, bTmp1, bTmp2, bTmp3); \
    } while (0)
# endif /* !IEM_WITH_CODE_TLB */

/** Whether a shadow VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_SHADOW_VMCS(a_pVCpu)            RT_BOOL(IEM_VMX_GET_SHADOW_VMCS(a_pVCpu) != NIL_RTGCPHYS)


/** Gets the guest-physical address of the shadows VMCS for the given VCPU. */
#define IEM_VMX_GET_SHADOW_VMCS(a_pVCpu)            ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->u64VmcsLinkPtr.u)

/** Whether a current VMCS is present for the given VCPU. */
#define IEM_VMX_HAS_CURRENT_VMCS(a_pVCpu)           RT_BOOL(IEM_VMX_GET_CURRENT_VMCS(a_pVCpu) != NIL_RTGCPHYS)

/** Gets the guest-physical address of the current VMCS for the given VCPU. */
#define IEM_VMX_GET_CURRENT_VMCS(a_pVCpu)           ((a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs)

/** Assigns the guest-physical address of the current VMCS for the given VCPU. */
#define IEM_VMX_SET_CURRENT_VMCS(a_pVCpu, a_GCPhysVmcs) \
    do \
    { \
        Assert((a_GCPhysVmcs) != NIL_RTGCPHYS); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = (a_GCPhysVmcs); \
    } while (0)

/** Clears any current VMCS for the given VCPU. */
#define IEM_VMX_CLEAR_CURRENT_VMCS(a_pVCpu) \
    do \
    { \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.GCPhysVmcs = NIL_RTGCPHYS; \
    } while (0)

/** Check the common VMX instruction preconditions.
 * @note Any changes here, also check if IEMOP_HLP_VMX_INSTR needs updating.
 */
#define IEM_VMX_INSTR_CHECKS(a_pVCpu, a_szInstr, a_InsDiagPrefix) \
    do { \
        if (   !IEM_IS_REAL_OR_V86_MODE(a_pVCpu) \
            && (   !IEM_IS_LONG_MODE(a_pVCpu) \
                ||  IEM_IS_64BIT_CODE(a_pVCpu))) \
        { /* likely */ } \
        else \
        { \
            if (IEM_IS_REAL_OR_V86_MODE(a_pVCpu)) \
            { \
                Log((a_szInstr ": Real or v8086 mode -> #UD\n")); \
                (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = a_InsDiagPrefix##_RealOrV86Mode; \
                return iemRaiseUndefinedOpcode(a_pVCpu); \
            } \
            if (IEM_IS_LONG_MODE(a_pVCpu) && !IEM_IS_64BIT_CODE(a_pVCpu)) \
            { \
                Log((a_szInstr ": Long mode without 64-bit code segment -> #UD\n")); \
                (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = a_InsDiagPrefix##_LongModeCS; \
                return iemRaiseUndefinedOpcode(a_pVCpu); \
            } \
        } \
    } while (0)

/** Check for VMX instructions requiring to be in VMX operation.
 * @note Any changes here, check if IEMOP_HLP_IN_VMX_OPERATION needs udpating. */
#define IEM_VMX_IN_VMX_OPERATION(a_pVCpu, a_szInstr, a_InsDiagPrefix) \
    do \
    { \
        if (IEM_IS_VMX_ROOT_MODE(a_pVCpu)) \
        { /* likely */ } \
        else \
        { \
            Log((a_szInstr ": Not in VMX operation (root mode) -> #UD\n")); \
            (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = a_InsDiagPrefix##_VmxRoot; \
            return iemRaiseUndefinedOpcode(a_pVCpu); \
        } \
    } while (0)

/** Marks a VM-entry failure with a diagnostic reason, logs and returns. */
#define IEM_VMX_VMENTRY_FAILED_RET(a_pVCpu, a_pszInstr, a_pszFailure, a_InsDiag) \
    do \
    { \
        Log(("%s: VM-entry failed! enmDiag=%u (%s) -> %s\n", (a_pszInstr), (a_InsDiag), \
                HMVmxGetDiagDesc(a_InsDiag), (a_pszFailure))); \
        (a_pVCpu)->cpum.GstCtx.hwvirt.vmx.enmDiag = (a_InsDiag); \
        return VERR_VMX_VMENTRY_FAILED; \
    } while (0)


/**
 * Returns whether the given VMCS field is valid and supported by our emulation.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   u64FieldEnc     The VMCS field encoding.
 *
 * @remarks This takes into account the CPU features exposed to the guest.
 */
IEM_STATIC bool iemVmxIsVmcsFieldValid(PVMCPU pVCpu, uint64_t u64FieldEnc)
{
    uint32_t const uFieldEncHi = RT_HI_U32(u64FieldEnc);
    uint32_t const uFieldEncLo = RT_LO_U32(u64FieldEnc);
    if (!uFieldEncHi)
    { /* likely */ }
    else
        return false;

    PCCPUMFEATURES pFeat = IEM_GET_GUEST_CPU_FEATURES(pVCpu);
    switch (uFieldEncLo)
    {
        /*
         * 16-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS16_VPID:                             return pFeat->fVmxVpid;
        case VMX_VMCS16_POSTED_INT_NOTIFY_VECTOR:         return pFeat->fVmxPostedInt;
        case VMX_VMCS16_EPTP_INDEX:                       return pFeat->fVmxEptXcptVe;

        /* Guest-state fields. */
        case VMX_VMCS16_GUEST_ES_SEL:
        case VMX_VMCS16_GUEST_CS_SEL:
        case VMX_VMCS16_GUEST_SS_SEL:
        case VMX_VMCS16_GUEST_DS_SEL:
        case VMX_VMCS16_GUEST_FS_SEL:
        case VMX_VMCS16_GUEST_GS_SEL:
        case VMX_VMCS16_GUEST_LDTR_SEL:
        case VMX_VMCS16_GUEST_TR_SEL:
        case VMX_VMCS16_GUEST_INTR_STATUS:                return pFeat->fVmxVirtIntDelivery;
        case VMX_VMCS16_GUEST_PML_INDEX:                  return pFeat->fVmxPml;

        /* Host-state fields. */
        case VMX_VMCS16_HOST_ES_SEL:
        case VMX_VMCS16_HOST_CS_SEL:
        case VMX_VMCS16_HOST_SS_SEL:
        case VMX_VMCS16_HOST_DS_SEL:
        case VMX_VMCS16_HOST_FS_SEL:
        case VMX_VMCS16_HOST_GS_SEL:
        case VMX_VMCS16_HOST_TR_SEL:                      return true;

        /*
         * 64-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS64_CTRL_IO_BITMAP_A_FULL:
        case VMX_VMCS64_CTRL_IO_BITMAP_A_HIGH:
        case VMX_VMCS64_CTRL_IO_BITMAP_B_FULL:
        case VMX_VMCS64_CTRL_IO_BITMAP_B_HIGH:            return pFeat->fVmxUseIoBitmaps;
        case VMX_VMCS64_CTRL_MSR_BITMAP_FULL:
        case VMX_VMCS64_CTRL_MSR_BITMAP_HIGH:             return pFeat->fVmxUseMsrBitmaps;
        case VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_STORE_HIGH:
        case VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_EXIT_MSR_LOAD_HIGH:
        case VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL:
        case VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_HIGH:
        case VMX_VMCS64_CTRL_EXEC_VMCS_PTR_FULL:
        case VMX_VMCS64_CTRL_EXEC_VMCS_PTR_HIGH:          return true;
        case VMX_VMCS64_CTRL_EXEC_PML_ADDR_FULL:
        case VMX_VMCS64_CTRL_EXEC_PML_ADDR_HIGH:          return pFeat->fVmxPml;
        case VMX_VMCS64_CTRL_TSC_OFFSET_FULL:
        case VMX_VMCS64_CTRL_TSC_OFFSET_HIGH:             return true;
        case VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_FULL:
        case VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_HIGH:     return pFeat->fVmxUseTprShadow;
        case VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL:
        case VMX_VMCS64_CTRL_APIC_ACCESSADDR_HIGH:        return pFeat->fVmxVirtApicAccess;
        case VMX_VMCS64_CTRL_POSTED_INTR_DESC_FULL:
        case VMX_VMCS64_CTRL_POSTED_INTR_DESC_HIGH:       return pFeat->fVmxPostedInt;
        case VMX_VMCS64_CTRL_VMFUNC_CTRLS_FULL:
        case VMX_VMCS64_CTRL_VMFUNC_CTRLS_HIGH:           return pFeat->fVmxVmFunc;
        case VMX_VMCS64_CTRL_EPTP_FULL:
        case VMX_VMCS64_CTRL_EPTP_HIGH:                   return pFeat->fVmxEpt;
        case VMX_VMCS64_CTRL_EOI_BITMAP_0_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_0_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_1_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_1_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_2_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_2_HIGH:
        case VMX_VMCS64_CTRL_EOI_BITMAP_3_FULL:
        case VMX_VMCS64_CTRL_EOI_BITMAP_3_HIGH:           return pFeat->fVmxVirtIntDelivery;
        case VMX_VMCS64_CTRL_EPTP_LIST_FULL:
        case VMX_VMCS64_CTRL_EPTP_LIST_HIGH:
        {
            uint64_t const uVmFuncMsr = CPUMGetGuestIa32VmxVmFunc(pVCpu);
            return RT_BOOL(RT_BF_GET(uVmFuncMsr, VMX_BF_VMFUNC_EPTP_SWITCHING));
        }
        case VMX_VMCS64_CTRL_VMREAD_BITMAP_FULL:
        case VMX_VMCS64_CTRL_VMREAD_BITMAP_HIGH:
        case VMX_VMCS64_CTRL_VMWRITE_BITMAP_FULL:
        case VMX_VMCS64_CTRL_VMWRITE_BITMAP_HIGH:         return pFeat->fVmxVmcsShadowing;
        case VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_FULL:
        case VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_HIGH:     return pFeat->fVmxEptXcptVe;
        case VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_FULL:
        case VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_HIGH:     return pFeat->fVmxXsavesXrstors;
        case VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_FULL:
        case VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_HIGH:   return false;
        case VMX_VMCS64_CTRL_TSC_MULTIPLIER_FULL:
        case VMX_VMCS64_CTRL_TSC_MULTIPLIER_HIGH:         return pFeat->fVmxUseTscScaling;

        /* Read-only data fields. */
        case VMX_VMCS64_RO_GUEST_PHYS_ADDR_FULL:
        case VMX_VMCS64_RO_GUEST_PHYS_ADDR_HIGH:          return pFeat->fVmxEpt;

        /* Guest-state fields. */
        case VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL:
        case VMX_VMCS64_GUEST_VMCS_LINK_PTR_HIGH:
        case VMX_VMCS64_GUEST_DEBUGCTL_FULL:
        case VMX_VMCS64_GUEST_DEBUGCTL_HIGH:              return true;
        case VMX_VMCS64_GUEST_PAT_FULL:
        case VMX_VMCS64_GUEST_PAT_HIGH:                   return pFeat->fVmxEntryLoadPatMsr || pFeat->fVmxExitSavePatMsr;
        case VMX_VMCS64_GUEST_EFER_FULL:
        case VMX_VMCS64_GUEST_EFER_HIGH:                  return pFeat->fVmxEntryLoadEferMsr || pFeat->fVmxExitSaveEferMsr;
        case VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL:
        case VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_HIGH:      return false;
        case VMX_VMCS64_GUEST_PDPTE0_FULL:
        case VMX_VMCS64_GUEST_PDPTE0_HIGH:
        case VMX_VMCS64_GUEST_PDPTE1_FULL:
        case VMX_VMCS64_GUEST_PDPTE1_HIGH:
        case VMX_VMCS64_GUEST_PDPTE2_FULL:
        case VMX_VMCS64_GUEST_PDPTE2_HIGH:
        case VMX_VMCS64_GUEST_PDPTE3_FULL:
        case VMX_VMCS64_GUEST_PDPTE3_HIGH:                return pFeat->fVmxEpt;
        case VMX_VMCS64_GUEST_BNDCFGS_FULL:
        case VMX_VMCS64_GUEST_BNDCFGS_HIGH:               return false;

        /* Host-state fields. */
        case VMX_VMCS64_HOST_PAT_FULL:
        case VMX_VMCS64_HOST_PAT_HIGH:                    return pFeat->fVmxExitLoadPatMsr;
        case VMX_VMCS64_HOST_EFER_FULL:
        case VMX_VMCS64_HOST_EFER_HIGH:                   return pFeat->fVmxExitLoadEferMsr;
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL:
        case VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_HIGH:       return false;

        /*
         * 32-bit fields.
         */
        /* Control fields. */
        case VMX_VMCS32_CTRL_PIN_EXEC:
        case VMX_VMCS32_CTRL_PROC_EXEC:
        case VMX_VMCS32_CTRL_EXCEPTION_BITMAP:
        case VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK:
        case VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH:
        case VMX_VMCS32_CTRL_CR3_TARGET_COUNT:
        case VMX_VMCS32_CTRL_EXIT:
        case VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT:
        case VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT:
        case VMX_VMCS32_CTRL_ENTRY:
        case VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT:
        case VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO:
        case VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE:
        case VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH:          return true;
        case VMX_VMCS32_CTRL_TPR_THRESHOLD:               return pFeat->fVmxUseTprShadow;
        case VMX_VMCS32_CTRL_PROC_EXEC2:                  return pFeat->fVmxSecondaryExecCtls;
        case VMX_VMCS32_CTRL_PLE_GAP:
        case VMX_VMCS32_CTRL_PLE_WINDOW:                  return pFeat->fVmxPauseLoopExit;

        /* Read-only data fields. */
        case VMX_VMCS32_RO_VM_INSTR_ERROR:
        case VMX_VMCS32_RO_EXIT_REASON:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO:
        case VMX_VMCS32_RO_EXIT_INTERRUPTION_ERROR_CODE:
        case VMX_VMCS32_RO_IDT_VECTORING_INFO:
        case VMX_VMCS32_RO_IDT_VECTORING_ERROR_CODE:
        case VMX_VMCS32_RO_EXIT_INSTR_LENGTH:
        case VMX_VMCS32_RO_EXIT_INSTR_INFO:               return true;

        /* Guest-state fields. */
        case VMX_VMCS32_GUEST_ES_LIMIT:
        case VMX_VMCS32_GUEST_CS_LIMIT:
        case VMX_VMCS32_GUEST_SS_LIMIT:
        case VMX_VMCS32_GUEST_DS_LIMIT:
        case VMX_VMCS32_GUEST_FS_LIMIT:
        case VMX_VMCS32_GUEST_GS_LIMIT:
        case VMX_VMCS32_GUEST_LDTR_LIMIT:
        case VMX_VMCS32_GUEST_TR_LIMIT:
        case VMX_VMCS32_GUEST_GDTR_LIMIT:
        case VMX_VMCS32_GUEST_IDTR_LIMIT:
        case VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS:
        case VMX_VMCS32_GUEST_INT_STATE:
        case VMX_VMCS32_GUEST_ACTIVITY_STATE:
        case VMX_VMCS32_GUEST_SMBASE:
        case VMX_VMCS32_GUEST_SYSENTER_CS:                return true;
        case VMX_VMCS32_PREEMPT_TIMER_VALUE:              return pFeat->fVmxPreemptTimer;

        /* Host-state fields. */
        case VMX_VMCS32_HOST_SYSENTER_CS:                 return true;

        /*
         * Natural-width fields.
         */
        /* Control fields. */
        case VMX_VMCS_CTRL_CR0_MASK:
        case VMX_VMCS_CTRL_CR4_MASK:
        case VMX_VMCS_CTRL_CR0_READ_SHADOW:
        case VMX_VMCS_CTRL_CR4_READ_SHADOW:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL0:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL1:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL2:
        case VMX_VMCS_CTRL_CR3_TARGET_VAL3:               return true;

        /* Read-only data fields. */
        case VMX_VMCS_RO_EXIT_QUALIFICATION:
        case VMX_VMCS_RO_IO_RCX:
        case VMX_VMCS_RO_IO_RSX:
        case VMX_VMCS_RO_IO_RDI:
        case VMX_VMCS_RO_IO_RIP:
        case VMX_VMCS_RO_EXIT_GUEST_LINEAR_ADDR:          return true;

        /* Guest-state fields. */
        case VMX_VMCS_GUEST_CR0:
        case VMX_VMCS_GUEST_CR3:
        case VMX_VMCS_GUEST_CR4:
        case VMX_VMCS_GUEST_ES_BASE:
        case VMX_VMCS_GUEST_CS_BASE:
        case VMX_VMCS_GUEST_SS_BASE:
        case VMX_VMCS_GUEST_DS_BASE:
        case VMX_VMCS_GUEST_FS_BASE:
        case VMX_VMCS_GUEST_GS_BASE:
        case VMX_VMCS_GUEST_LDTR_BASE:
        case VMX_VMCS_GUEST_TR_BASE:
        case VMX_VMCS_GUEST_GDTR_BASE:
        case VMX_VMCS_GUEST_IDTR_BASE:
        case VMX_VMCS_GUEST_DR7:
        case VMX_VMCS_GUEST_RSP:
        case VMX_VMCS_GUEST_RIP:
        case VMX_VMCS_GUEST_RFLAGS:
        case VMX_VMCS_GUEST_PENDING_DEBUG_XCPTS:
        case VMX_VMCS_GUEST_SYSENTER_ESP:
        case VMX_VMCS_GUEST_SYSENTER_EIP:                 return true;

        /* Host-state fields. */
        case VMX_VMCS_HOST_CR0:
        case VMX_VMCS_HOST_CR3:
        case VMX_VMCS_HOST_CR4:
        case VMX_VMCS_HOST_FS_BASE:
        case VMX_VMCS_HOST_GS_BASE:
        case VMX_VMCS_HOST_TR_BASE:
        case VMX_VMCS_HOST_GDTR_BASE:
        case VMX_VMCS_HOST_IDTR_BASE:
        case VMX_VMCS_HOST_SYSENTER_ESP:
        case VMX_VMCS_HOST_SYSENTER_EIP:
        case VMX_VMCS_HOST_RSP:
        case VMX_VMCS_HOST_RIP:                           return true;
    }

    return false;
}


/**
 * Gets a segment register from the VMCS given its index.
 *
 * @returns VBox status code.
 * @param   pVmcs       Pointer to the virtual VMCS.
 * @param   iSegReg     The index of the segment register (X86_SREG_XXX).
 * @param   pSelReg     Where to store the segment register (only updated when
 *                      VINF_SUCCESS is returned).
 *
 * @remarks Warning! This does not validate the contents of the retreived segment
 *          register.
 */
IEM_STATIC int iemVmxVmcsGetGuestSegReg(PCVMXVVMCS pVmcs, uint8_t iSegReg, PCPUMSELREG pSelReg)
{
    Assert(pSelReg);
    Assert(iSegReg < X86_SREG_COUNT);

    /* Selector. */
    uint16_t u16Sel;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_16BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS16_GUEST_ES_SEL, VMX_BF_VMCS_ENC_INDEX);
        AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u16Sel = *(uint16_t *)pbField;
    }

    /* Limit. */
    uint32_t u32Limit;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_LIMIT, VMX_BF_VMCS_ENC_INDEX);
        AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u32Limit = *(uint32_t *)pbField;
    }

    /* Base. */
    uint64_t u64Base;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_NATURAL;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS_GUEST_ES_BASE, VMX_BF_VMCS_ENC_INDEX);
        AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u64Base = *(uint64_t *)pbField;
        /** @todo NSTVMX: Should we zero out high bits here for 32-bit virtual CPUs? */
    }

    /* Attributes. */
    uint32_t u32Attr;
    {
        uint8_t  const  uWidth     = VMX_VMCS_ENC_WIDTH_32BIT;
        uint8_t  const  uType      = VMX_VMCS_ENC_TYPE_GUEST_STATE;
        uint8_t  const  uWidthType = (uWidth << 2) | uType;
        uint8_t  const  uIndex     = (iSegReg << 1) + RT_BF_GET(VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS, VMX_BF_VMCS_ENC_INDEX);
        AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_3);
        uint16_t const  offField   = g_aoffVmcsMap[uWidthType][uIndex];
        uint8_t  const *pbVmcs     = (uint8_t *)pVmcs;
        uint8_t  const *pbField    = pbVmcs + offField;
        u32Attr = *(uint32_t *)pbField;
    }

    pSelReg->Sel      = u16Sel;
    pSelReg->u32Limit = u32Limit;
    pSelReg->u64Base  = u64Base;
    pSelReg->Attr.u   = u32Attr;
    return VINF_SUCCESS;
}


/**
 * Gets VM-exit instruction information along with any displacement for an
 * instruction VM-exit.
 *
 * @returns The VM-exit instruction information.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   uExitReason     The VM-exit reason.
 * @param   uInstrId        The VM-exit instruction identity (VMXINSTRID_XXX) if
 *                          any. Pass VMXINSTRID_NONE otherwise.
 * @param   fPrimaryOpRead  If the primary operand of the ModR/M byte (bits 0:3) is
 *                          a read or write.
 * @param   pGCPtrDisp      Where to store the displacement field. Optional, can be
 *                          NULL.
 */
IEM_STATIC uint32_t iemVmxGetExitInstrInfo(PVMCPU pVCpu, uint32_t uExitReason, VMXINSTRID uInstrId, bool fPrimaryOpRead,
                                           PRTGCPTR pGCPtrDisp)
{
    RTGCPTR          GCPtrDisp;
    VMXEXITINSTRINFO ExitInstrInfo;
    ExitInstrInfo.u = 0;

    /*
     * Get and parse the ModR/M byte from our decoded opcodes.
     */
    uint8_t bRm;
    uint8_t const offModRm = pVCpu->iem.s.offModRm;
    IEM_MODRM_GET_U8(pVCpu, bRm, offModRm);
    if ((bRm & X86_MODRM_MOD_MASK) == (3 << X86_MODRM_MOD_SHIFT))
    {
        /*
         * ModR/M indicates register addressing.
         *
         * The primary/secondary register operands are reported in the iReg1 or iReg2
         * fields depending on whether it is a read/write form.
         */
        uint8_t idxReg1;
        uint8_t idxReg2;
        if (fPrimaryOpRead)
        {
            idxReg1 = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
            idxReg2 = (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB;
        }
        else
        {
            idxReg1 = (bRm & X86_MODRM_RM_MASK) | pVCpu->iem.s.uRexB;
            idxReg2 = ((bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK) | pVCpu->iem.s.uRexReg;
        }
        ExitInstrInfo.All.u2Scaling       = 0;
        ExitInstrInfo.All.iReg1           = idxReg1;
        ExitInstrInfo.All.u3AddrSize      = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand   = 1;
        ExitInstrInfo.All.uOperandSize    = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg         = 0;
        ExitInstrInfo.All.iIdxReg         = 0;
        ExitInstrInfo.All.fIdxRegInvalid  = 1;
        ExitInstrInfo.All.iBaseReg        = 0;
        ExitInstrInfo.All.fBaseRegInvalid = 1;
        ExitInstrInfo.All.iReg2           = idxReg2;

        /* Displacement not applicable for register addressing. */
        GCPtrDisp = 0;
    }
    else
    {
        /*
         * ModR/M indicates memory addressing.
         */
        uint8_t  uScale        = 0;
        bool     fBaseRegValid = false;
        bool     fIdxRegValid  = false;
        uint8_t  iBaseReg      = 0;
        uint8_t  iIdxReg       = 0;
        if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_16BIT)
        {
            /*
             * Parse the ModR/M, displacement for 16-bit addressing mode.
             * See Intel instruction spec. Table 2-1. "16-Bit Addressing Forms with the ModR/M Byte".
             */
            uint16_t u16Disp = 0;
            uint8_t const offDisp = offModRm + sizeof(bRm);
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
            {
                /* Displacement without any registers. */
                IEM_DISP_GET_U16(pVCpu, u16Disp, offDisp);
            }
            else
            {
                /* Register (index and base). */
                switch (bRm & X86_MODRM_RM_MASK)
                {
                    case 0: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; fIdxRegValid = true; iIdxReg = X86_GREG_xSI; break;
                    case 1: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; fIdxRegValid = true; iIdxReg = X86_GREG_xDI; break;
                    case 2: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; fIdxRegValid = true; iIdxReg = X86_GREG_xSI; break;
                    case 3: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; fIdxRegValid = true; iIdxReg = X86_GREG_xDI; break;
                    case 4: fIdxRegValid  = true; iIdxReg  = X86_GREG_xSI; break;
                    case 5: fIdxRegValid  = true; iIdxReg  = X86_GREG_xDI; break;
                    case 6: fBaseRegValid = true; iBaseReg = X86_GREG_xBP; break;
                    case 7: fBaseRegValid = true; iBaseReg = X86_GREG_xBX; break;
                }

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0:                                                  break;
                    case 1: IEM_DISP_GET_S8_SX_U16(pVCpu, u16Disp, offDisp); break;
                    case 2: IEM_DISP_GET_U16(pVCpu, u16Disp, offDisp);       break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            Assert(!uScale);                    /* There's no scaling/SIB byte for 16-bit addressing. */
            GCPtrDisp = (int16_t)u16Disp;       /* Sign-extend the displacement. */
        }
        else if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_32BIT)
        {
            /*
             * Parse the ModR/M, SIB, displacement for 32-bit addressing mode.
             * See Intel instruction spec. Table 2-2. "32-Bit Addressing Forms with the ModR/M Byte".
             */
            uint32_t u32Disp = 0;
            if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
            {
                /* Displacement without any registers. */
                uint8_t const offDisp = offModRm + sizeof(bRm);
                IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);
            }
            else
            {
                /* Register (and perhaps scale, index and base). */
                uint8_t offDisp = offModRm + sizeof(bRm);
                iBaseReg = (bRm & X86_MODRM_RM_MASK);
                if (iBaseReg == 4)
                {
                    /* An SIB byte follows the ModR/M byte, parse it. */
                    uint8_t bSib;
                    uint8_t const offSib = offModRm + sizeof(bRm);
                    IEM_SIB_GET_U8(pVCpu, bSib, offSib);

                    /* A displacement may follow SIB, update its offset. */
                    offDisp += sizeof(bSib);

                    /* Get the scale. */
                    uScale = (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* Get the index register. */
                    iIdxReg = (bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK;
                    fIdxRegValid = RT_BOOL(iIdxReg != 4);

                    /* Get the base register. */
                    iBaseReg = bSib & X86_SIB_BASE_MASK;
                    fBaseRegValid = true;
                    if (iBaseReg == 5)
                    {
                        if ((bRm & X86_MODRM_MOD_MASK) == 0)
                        {
                            /* Mod is 0 implies a 32-bit displacement with no base. */
                            fBaseRegValid = false;
                            IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);
                        }
                        else
                        {
                            /* Mod is not 0 implies an 8-bit/32-bit displacement (handled below) with an EBP base. */
                            iBaseReg = X86_GREG_xBP;
                        }
                    }
                }

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0: /* Handled above */                              break;
                    case 1: IEM_DISP_GET_S8_SX_U32(pVCpu, u32Disp, offDisp); break;
                    case 2: IEM_DISP_GET_U32(pVCpu, u32Disp, offDisp);       break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            GCPtrDisp = (int32_t)u32Disp;       /* Sign-extend the displacement. */
        }
        else
        {
            Assert(pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT);

            /*
             * Parse the ModR/M, SIB, displacement for 64-bit addressing mode.
             * See Intel instruction spec. 2.2 "IA-32e Mode".
             */
            uint64_t u64Disp = 0;
            bool const fRipRelativeAddr = RT_BOOL((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5);
            if (fRipRelativeAddr)
            {
                /*
                 * RIP-relative addressing mode.
                 *
                 * The displacment is 32-bit signed implying an offset range of +/-2G.
                 * See Intel instruction spec. 2.2.1.6 "RIP-Relative Addressing".
                 */
                uint8_t const offDisp = offModRm + sizeof(bRm);
                IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp);
            }
            else
            {
                uint8_t offDisp = offModRm + sizeof(bRm);

                /*
                 * Register (and perhaps scale, index and base).
                 *
                 * REX.B extends the most-significant bit of the base register. However, REX.B
                 * is ignored while determining whether an SIB follows the opcode. Hence, we
                 * shall OR any REX.B bit -after- inspecting for an SIB byte below.
                 *
                 * See Intel instruction spec. Table 2-5. "Special Cases of REX Encodings".
                 */
                iBaseReg = (bRm & X86_MODRM_RM_MASK);
                if (iBaseReg == 4)
                {
                    /* An SIB byte follows the ModR/M byte, parse it. Displacement (if any) follows SIB. */
                    uint8_t bSib;
                    uint8_t const offSib = offModRm + sizeof(bRm);
                    IEM_SIB_GET_U8(pVCpu, bSib, offSib);

                    /* Displacement may follow SIB, update its offset. */
                    offDisp += sizeof(bSib);

                    /* Get the scale. */
                    uScale = (bSib >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;

                    /* Get the index. */
                    iIdxReg = ((bSib >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK) | pVCpu->iem.s.uRexIndex;
                    fIdxRegValid = RT_BOOL(iIdxReg != 4);   /* R12 -can- be used as an index register. */

                    /* Get the base. */
                    iBaseReg = (bSib & X86_SIB_BASE_MASK);
                    fBaseRegValid = true;
                    if (iBaseReg == 5)
                    {
                        if ((bRm & X86_MODRM_MOD_MASK) == 0)
                        {
                            /* Mod is 0 implies a signed 32-bit displacement with no base. */
                            IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp);
                        }
                        else
                        {
                            /* Mod is non-zero implies an 8-bit/32-bit displacement (handled below) with RBP or R13 as base. */
                            iBaseReg = pVCpu->iem.s.uRexB ? X86_GREG_x13 : X86_GREG_xBP;
                        }
                    }
                }
                iBaseReg |= pVCpu->iem.s.uRexB;

                /* Register + displacement. */
                switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
                {
                    case 0: /* Handled above */                               break;
                    case 1: IEM_DISP_GET_S8_SX_U64(pVCpu, u64Disp, offDisp);  break;
                    case 2: IEM_DISP_GET_S32_SX_U64(pVCpu, u64Disp, offDisp); break;
                    default:
                    {
                        /* Register addressing, handled at the beginning. */
                        AssertMsgFailed(("ModR/M %#x implies register addressing, memory addressing expected!", bRm));
                        break;
                    }
                }
            }

            GCPtrDisp = fRipRelativeAddr ? pVCpu->cpum.GstCtx.rip + u64Disp : u64Disp;
        }

        /*
         * The primary or secondary register operand is reported in iReg2 depending
         * on whether the primary operand is in read/write form.
         */
        uint8_t idxReg2;
        if (fPrimaryOpRead)
        {
            idxReg2 = bRm & X86_MODRM_RM_MASK;
            if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
                idxReg2 |= pVCpu->iem.s.uRexB;
        }
        else
        {
            idxReg2 = (bRm >> X86_MODRM_REG_SHIFT) & X86_MODRM_REG_SMASK;
            if (pVCpu->iem.s.enmEffAddrMode == IEMMODE_64BIT)
                 idxReg2 |= pVCpu->iem.s.uRexReg;
        }
        ExitInstrInfo.All.u2Scaling      = uScale;
        ExitInstrInfo.All.iReg1          = 0;   /* Not applicable for memory addressing. */
        ExitInstrInfo.All.u3AddrSize     = pVCpu->iem.s.enmEffAddrMode;
        ExitInstrInfo.All.fIsRegOperand  = 0;
        ExitInstrInfo.All.uOperandSize   = pVCpu->iem.s.enmEffOpSize;
        ExitInstrInfo.All.iSegReg        = pVCpu->iem.s.iEffSeg;
        ExitInstrInfo.All.iIdxReg        = iIdxReg;
        ExitInstrInfo.All.fIdxRegInvalid = !fIdxRegValid;
        ExitInstrInfo.All.iBaseReg       = iBaseReg;
        ExitInstrInfo.All.iIdxReg        = !fBaseRegValid;
        ExitInstrInfo.All.iReg2          = idxReg2;
    }

    /*
     * Handle exceptions to the norm for certain instructions.
     * (e.g. some instructions convey an instruction identity in place of iReg2).
     */
    switch (uExitReason)
    {
        case VMX_EXIT_GDTR_IDTR_ACCESS:
        {
            Assert(VMXINSTRID_IS_VALID(uInstrId));
            ExitInstrInfo.GdtIdt.u2InstrId = VMXINSTRID_GET_ID(uInstrId);
            ExitInstrInfo.GdtIdt.u2Undef0  = 0;
            break;
        }

        case VMX_EXIT_LDTR_TR_ACCESS:
        {
            Assert(VMXINSTRID_IS_VALID(uInstrId));
            ExitInstrInfo.LdtTr.u2InstrId = VMXINSTRID_GET_ID(uInstrId);
            ExitInstrInfo.LdtTr.u2Undef0 = 0;
            break;
        }

        case VMX_EXIT_RDRAND:
        case VMX_EXIT_RDSEED:
        {
            Assert(ExitInstrInfo.RdrandRdseed.u2OperandSize != 3);
            break;
        }
    }

    /* Update displacement and return the constructed VM-exit instruction information field. */
    if (pGCPtrDisp)
        *pGCPtrDisp = GCPtrDisp;
    return ExitInstrInfo.u;
}


/**
 * Implements VMSucceed for VMX instruction success.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmSucceed(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
}


/**
 * Implements VMFailInvalid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmFailInvalid(PVMCPU pVCpu)
{
    pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
    pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_CF;
}


/**
 * Implements VMFailValid for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECL_FORCE_INLINE(void) iemVmxVmFailValid(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        pVCpu->cpum.GstCtx.eflags.u32 &= ~(X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_OF);
        pVCpu->cpum.GstCtx.eflags.u32 |= X86_EFL_ZF;
        /** @todo NSTVMX: VMWrite enmInsErr to VM-instruction error field. */
        RT_NOREF(enmInsErr);
    }
}


/**
 * Implements VMFail for VMX instruction failure.
 *
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   enmInsErr   The VM instruction error.
 */
DECL_FORCE_INLINE(void) iemVmxVmFail(PVMCPU pVCpu, VMXINSTRERR enmInsErr)
{
    if (IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        iemVmxVmFailValid(pVCpu, enmInsErr);
        /** @todo Set VM-instruction error field in the current virtual-VMCS. */
    }
    else
        iemVmxVmFailInvalid(pVCpu);
}


/**
 * Flushes the current VMCS contents back to guest memory.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(int) iemVmxCommitCurrentVmcsToMemory(PVMCPU pVCpu)
{
    Assert(IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    int rc = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), IEM_VMX_GET_CURRENT_VMCS(pVCpu),
                                      pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs), sizeof(VMXVVMCS));
    IEM_VMX_CLEAR_CURRENT_VMCS(pVCpu);
    return rc;
}


/**
 * Implements VMSucceed for the VMREAD instruction and increments the guest RIP.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 */
DECL_FORCE_INLINE(void) iemVmxVmreadSuccess(PVMCPU pVCpu, uint8_t cbInstr)
{
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
}


/**
 * VMREAD common (memory/register) instruction execution worker
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu64Dst         Where to write the VMCS value (only updated when
 *                          VINF_SUCCESS is returned).
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadCommon(PVMCPU pVCpu, uint8_t cbInstr, uint64_t *pu64Dst, uint64_t u64FieldEnc,
                                           PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo); RT_NOREF(cbInstr);
        /** @todo NSTVMX: intercept. */
        /** @todo NSTVMX: VMCS shadowing intercept (VMREAD bitmap). */
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmread: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMCS pointer in root mode. */
    if (    IEM_IS_VMX_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("vmread: VMCS pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS-link pointer in non-root mode. */
    if (    IEM_IS_VMX_NON_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_SHADOW_VMCS(pVCpu))
    {
        Log(("vmread: VMCS-link pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_SHADOW_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_LinkPtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Supported VMCS field. */
    if (iemVmxIsVmcsFieldValid(pVCpu, u64FieldEnc))
    {
        Log(("vmread: VMCS field %#RX64 invalid -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_FieldInvalid;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMREAD_INVALID_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Setup reading from the current or shadow VMCS.
     */
    uint8_t *pbVmcs;
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs);
    else
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pbVmcs);

    VMXVMCSFIELDENC FieldEnc;
    FieldEnc.u = RT_LO_U32(u64FieldEnc);
    uint8_t  const uWidth     = FieldEnc.n.u2Width;
    uint8_t  const uType      = FieldEnc.n.u2Type;
    uint8_t  const uWidthType = (uWidth << 2) | uType;
    uint8_t  const uIndex     = FieldEnc.n.u8Index;
    AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_2);
    uint16_t const offField   = g_aoffVmcsMap[uWidthType][uIndex];

    /*
     * Read the VMCS component based on the field's effective width.
     *
     * The effective width is 64-bit fields adjusted to 32-bits if the access-type
     * indicates high bits (little endian).
     *
     * Note! The caller is responsible to trim the result and update registers
     * or memory locations are required. Here we just zero-extend to the largest
     * type (i.e. 64-bits).
     */
    uint8_t      *pbField = pbVmcs + offField;
    uint8_t const uEffWidth = HMVmxGetVmcsFieldWidthEff(FieldEnc.u);
    switch (uEffWidth)
    {
        case VMX_VMCS_ENC_WIDTH_64BIT:
        case VMX_VMCS_ENC_WIDTH_NATURAL: *pu64Dst = *(uint64_t *)pbField; break;
        case VMX_VMCS_ENC_WIDTH_32BIT:   *pu64Dst = *(uint32_t *)pbField; break;
        case VMX_VMCS_ENC_WIDTH_16BIT:   *pu64Dst = *(uint16_t *)pbField; break;
    }
    return VINF_SUCCESS;
}


/**
 * VMREAD (64-bit register) instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu64Dst         Where to store the VMCS field's value.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadReg64(PVMCPU pVCpu, uint8_t cbInstr, uint64_t *pu64Dst, uint64_t u64FieldEnc,
                                          PCVMXVEXITINFO pExitInfo)
{
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, pu64Dst, u64FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        iemVmxVmreadSuccess(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    Log(("vmread/reg: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMREAD (32-bit register) instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   pu32Dst         Where to store the VMCS field's value.
 * @param   u32FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadReg32(PVMCPU pVCpu, uint8_t cbInstr, uint32_t *pu32Dst, uint64_t u32FieldEnc,
                                          PCVMXVEXITINFO pExitInfo)
{
    uint64_t u64Dst;
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, &u64Dst, u32FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        *pu32Dst = u64Dst;
        iemVmxVmreadSuccess(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    Log(("vmread/reg: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMREAD (memory) instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a u64Val.
 *                          Pass UINT8_MAX if it is a register access.
 * @param   enmEffAddrMode  The effective addressing mode (only used with memory
 *                          operand).
 * @param   GCPtrDst        The guest linear address to store the VMCS field's
 *                          value.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmreadMem(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, IEMMODE enmEffAddrMode,
                                        RTGCPTR GCPtrDst, uint64_t u64FieldEnc, PCVMXVEXITINFO pExitInfo)
{
    uint64_t u64Dst;
    VBOXSTRICTRC rcStrict = iemVmxVmreadCommon(pVCpu, cbInstr, &u64Dst, u64FieldEnc, pExitInfo);
    if (rcStrict == VINF_SUCCESS)
    {
        /*
         * Write the VMCS field's value to the location specified in guest-memory.
         *
         * The pointer size depends on the address size (address-size prefix allowed).
         * The operand size depends on IA-32e mode (operand-size prefix not allowed).
         */
        static uint64_t const s_auAddrSizeMasks[] = { UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_C(0xffffffffffffffff) };
        Assert(enmEffAddrMode < RT_ELEMENTS(s_auAddrSizeMasks));
        GCPtrDst &= s_auAddrSizeMasks[enmEffAddrMode];

        if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
            rcStrict = iemMemStoreDataU64(pVCpu, iEffSeg, GCPtrDst, u64Dst);
        else
            rcStrict = iemMemStoreDataU32(pVCpu, iEffSeg, GCPtrDst, u64Dst);
        if (rcStrict == VINF_SUCCESS)
        {
            iemVmxVmreadSuccess(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        Log(("vmread/mem: Failed to write to memory operand at %#RGv, rc=%Rrc\n", GCPtrDst, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmread_PtrMap;
        return rcStrict;
    }

    Log(("vmread/mem: iemVmxVmreadCommon failed rc=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    return rcStrict;
}


/**
 * VMWRITE instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a u64Val.
 *                          Pass UINT8_MAX if it is a register access.
 * @param   enmEffAddrMode  The effective addressing mode (only used with memory
 *                          operand).
 * @param   u64Val          The value to write (or guest linear address to the
 *                          value), @a iEffSeg will indicate if it's a memory
 *                          operand.
 * @param   u64FieldEnc     The VMCS field encoding.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmwrite(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, IEMMODE enmEffAddrMode, uint64_t u64Val,
                                      uint64_t u64FieldEnc, PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
        /** @todo NSTVMX: VMCS shadowing intercept (VMWRITE bitmap). */
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmwrite: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMCS pointer in root mode. */
    if (    IEM_IS_VMX_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS-link pointer in non-root mode. */
    if (    IEM_IS_VMX_NON_ROOT_MODE(pVCpu)
        && !IEM_VMX_HAS_SHADOW_VMCS(pVCpu))
    {
        Log(("vmwrite: VMCS-link pointer %#RGp invalid -> VMFailInvalid\n", IEM_VMX_GET_SHADOW_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_LinkPtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* If the VMWRITE instruction references memory, access the specified memory operand. */
    bool const fIsRegOperand = iEffSeg == UINT8_MAX;
    if (!fIsRegOperand)
    {
        static uint64_t const s_auAddrSizeMasks[] = { UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_C(0xffffffffffffffff) };
        Assert(enmEffAddrMode < RT_ELEMENTS(s_auAddrSizeMasks));
        RTGCPTR const GCPtrVal = u64Val & s_auAddrSizeMasks[enmEffAddrMode];

        /* Read the value from the specified guest memory location. */
        VBOXSTRICTRC rcStrict;
        if (pVCpu->iem.s.enmCpuMode == IEMMODE_64BIT)
            rcStrict = iemMemFetchDataU64(pVCpu, &u64Val, iEffSeg, GCPtrVal);
        else
        {
            uint32_t u32Val;
            rcStrict = iemMemFetchDataU32(pVCpu, &u32Val, iEffSeg, GCPtrVal);
            u64Val = u32Val;
        }
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmwrite: Failed to read value from memory operand at %#RGv, rc=%Rrc\n", GCPtrVal, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_PtrMap;
            return rcStrict;
        }
    }
    else
        Assert(!pExitInfo || pExitInfo->InstrInfo.VmreadVmwrite.fIsRegOperand);

    /* Supported VMCS field. */
    if (!iemVmxIsVmcsFieldValid(pVCpu, u64FieldEnc))
    {
        Log(("vmwrite: VMCS field %#RX64 invalid -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_FieldInvalid;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_INVALID_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Read-only VMCS field. */
    bool const fReadOnlyField = HMVmxIsVmcsFieldReadOnly(u64FieldEnc);
    if (   fReadOnlyField
        && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmwriteAll)
    {
        Log(("vmwrite: Write to read-only VMCS component %#RX64 -> VMFail\n", u64FieldEnc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_FieldRo;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMWRITE_RO_COMPONENT);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Setup writing to the current or shadow VMCS.
     */
    uint8_t *pbVmcs;
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pShadowVmcs);
    else
        pbVmcs = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    Assert(pbVmcs);

    VMXVMCSFIELDENC FieldEnc;
    FieldEnc.u = RT_LO_U32(u64FieldEnc);
    uint8_t  const uWidth     = FieldEnc.n.u2Width;
    uint8_t  const uType      = FieldEnc.n.u2Type;
    uint8_t  const uWidthType = (uWidth << 2) | uType;
    uint8_t  const uIndex     = FieldEnc.n.u8Index;
    AssertRCReturn(uIndex <= VMX_V_VMCS_MAX_INDEX, VERR_IEM_IPE_2);
    uint16_t const offField   = g_aoffVmcsMap[uWidthType][uIndex];

    /*
     * Write the VMCS component based on the field's effective width.
     *
     * The effective width is 64-bit fields adjusted to 32-bits if the access-type
     * indicates high bits (little endian).
     */
    uint8_t      *pbField = pbVmcs + offField;
    uint8_t const uEffWidth = HMVmxGetVmcsFieldWidthEff(FieldEnc.u);
    switch (uEffWidth)
    {
        case VMX_VMCS_ENC_WIDTH_64BIT:
        case VMX_VMCS_ENC_WIDTH_NATURAL: *(uint64_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_32BIT:   *(uint32_t *)pbField = u64Val; break;
        case VMX_VMCS_ENC_WIDTH_16BIT:   *(uint16_t *)pbField = u64Val; break;
    }

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmwrite_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMCLEAR instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a GCPtrVmcs.
 * @param   GCPtrVmcs       The linear address of the VMCS pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmclear(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmclear: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmclear: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmclear: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
    {
        Log(("vmclear: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmclear: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmclear: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_PtrAbnormal;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * VMCLEAR allows committing and clearing any valid VMCS pointer.
     *
     * If the current VMCS is the one being cleared, set its state to 'clear' and commit
     * to guest memory. Otherwise, set the state of the VMCS referenced in guest memory
     * to 'clear'.
     */
    uint8_t const fVmcsStateClear = VMX_V_VMCS_STATE_CLEAR;
    if (IEM_VMX_GET_CURRENT_VMCS(pVCpu) == GCPhysVmcs)
    {
        Assert(GCPhysVmcs != NIL_RTGCPHYS);                     /* Paranoia. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs));
        pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState = fVmcsStateClear;
        iemVmxCommitCurrentVmcsToMemory(pVCpu);
        Assert(!IEM_VMX_HAS_CURRENT_VMCS(pVCpu));
    }
    else
    {
        rcStrict = PGMPhysSimpleWriteGCPhys(pVCpu->CTX_SUFF(pVM), GCPtrVmcs + RT_OFFSETOF(VMXVVMCS, fVmcsState),
                                            (const void *)&fVmcsStateClear, sizeof(fVmcsStateClear));
    }

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmclear_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return rcStrict;
}


/**
 * VMPTRST instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a GCPtrVmcs.
 * @param   GCPtrVmcs       The linear address of where to store the current VMCS
 *                          pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmptrst(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmptrst: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrst_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Set the VMCS pointer to the location specified by the destination memory operand. */
    AssertCompile(NIL_RTGCPHYS == ~(RTGCPHYS)0U);
    VBOXSTRICTRC rcStrict = iemMemStoreDataU64(pVCpu, iEffSeg, GCPtrVmcs, IEM_VMX_GET_CURRENT_VMCS(pVCpu));
    if (RT_LIKELY(rcStrict == VINF_SUCCESS))
    {
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrst_Success;
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return rcStrict;
    }

    Log(("vmptrst: Failed to store VMCS pointer to memory at destination operand %#Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrst_PtrMap;
    return rcStrict;
}


/**
 * VMPTRLD instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   GCPtrVmcs       The linear address of the current VMCS pointer.
 * @param   pExitInfo       Pointer to the VM-exit information struct. Optional, can
 *                          be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. VMX operation, CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmptrld(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmcs,
                                      PCVMXVEXITINFO pExitInfo)
{
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmptrld: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Get the VMCS pointer from the location specified by the source memory operand. */
    RTGCPHYS GCPhysVmcs;
    VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmcs, iEffSeg, GCPtrVmcs);
    if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
    {
        Log(("vmptrld: Failed to read VMCS physaddr from %#RGv, rc=%Rrc\n", GCPtrVmcs, VBOXSTRICTRC_VAL(rcStrict)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrMap;
        return rcStrict;
    }

    /* VMCS pointer alignment. */
    if (GCPhysVmcs & X86_PAGE_4K_OFFSET_MASK)
    {
        Log(("vmptrld: VMCS pointer not page-aligned -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrAlign;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS physical-address width limits. */
    if (GCPhysVmcs >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
    {
        Log(("vmptrld: VMCS pointer extends beyond physical-address width -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrWidth;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* VMCS is not the VMXON region. */
    if (GCPhysVmcs == pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon)
    {
        Log(("vmptrld: VMCS pointer cannot be identical to VMXON region pointer -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrVmxon;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_VMXON_PTR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Ensure VMCS is not MMIO, ROM etc. This is not an Intel requirement but a
       restriction imposed by our implementation. */
    if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmcs))
    {
        Log(("vmptrld: VMCS not normal memory -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrAbnormal;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /* Read the VMCS revision ID from the VMCS. */
    VMXVMCSREVID VmcsRevId;
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), &VmcsRevId, GCPhysVmcs, sizeof(VmcsRevId));
    if (RT_FAILURE(rc))
    {
        Log(("vmptrld: Failed to read VMCS at %#RGp, rc=%Rrc\n", GCPhysVmcs, rc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_PtrReadPhys;
        return rc;
    }

    /* Verify the VMCS revision specified by the guest matches what we reported to the guest,
       also check VMCS shadowing feature. */
    if (   VmcsRevId.n.u31RevisionId != VMX_V_VMCS_REVISION_ID
        || (   VmcsRevId.n.fIsShadowVmcs
            && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxVmcsShadowing))
    {
        if (VmcsRevId.n.u31RevisionId != VMX_V_VMCS_REVISION_ID)
        {
            Log(("vmptrld: VMCS revision mismatch, expected %#RX32 got %#RX32 -> VMFail()\n", VMX_V_VMCS_REVISION_ID,
                 VmcsRevId.n.u31RevisionId));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_VmcsRevId;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        Log(("vmptrld: Shadow VMCS -> VMFail()\n"));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_ShadowVmcs;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * We only maintain only the current VMCS in our virtual CPU context (CPUMCTX). Therefore,
     * VMPTRLD shall always flush any existing current VMCS back to guest memory before loading
     * a new VMCS as current.
     */
    if (IEM_VMX_GET_CURRENT_VMCS(pVCpu) != GCPhysVmcs)
    {
        iemVmxCommitCurrentVmcsToMemory(pVCpu);
        IEM_VMX_SET_CURRENT_VMCS(pVCpu, GCPhysVmcs);
    }
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmptrld_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
}


/**
 * VMXON instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   iEffSeg         The effective segment register to use with @a
 *                          GCPtrVmxon.
 * @param   GCPtrVmxon      The linear address of the VMXON pointer.
 * @param   pExitInfo       Pointer to the VM-exit instruction information struct.
 *                          Optional, can  be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmxon(PVMCPU pVCpu, uint8_t cbInstr, uint8_t iEffSeg, RTGCPHYS GCPtrVmxon,
                                    PCVMXVEXITINFO pExitInfo)
{
#if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF5(pVCpu, cbInstr, iEffSeg, GCPtrVmxon, pExitInfo);
    return VINF_EM_RAW_EMULATE_INSTR;
#else
    if (!IEM_IS_VMX_ROOT_MODE(pVCpu))
    {
        /* CPL. */
        if (pVCpu->iem.s.uCpl > 0)
        {
            Log(("vmxon: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cpl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* A20M (A20 Masked) mode. */
        if (!PGMPhysIsA20Enabled(pVCpu))
        {
            Log(("vmxon: A20M mode -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_A20M;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR0 fixed bits. */
        bool const     fUnrestrictedGuest = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxUnrestrictedGuest;
        uint64_t const uCr0Fixed0         = fUnrestrictedGuest ? VMX_V_CR0_FIXED0_UX : VMX_V_CR0_FIXED0;
        if ((pVCpu->cpum.GstCtx.cr0 & uCr0Fixed0) != uCr0Fixed0)
        {
            Log(("vmxon: CR0 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr0Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* CR4 fixed bits. */
        if ((pVCpu->cpum.GstCtx.cr4 & VMX_V_CR4_FIXED0) != VMX_V_CR4_FIXED0)
        {
            Log(("vmxon: CR4 fixed0 bits cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Cr4Fixed0;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Feature control MSR's LOCK and VMXON bits. */
        uint64_t const uMsrFeatCtl = CPUMGetGuestIa32FeatureControl(pVCpu);
        if (!(uMsrFeatCtl & (MSR_IA32_FEATURE_CONTROL_LOCK | MSR_IA32_FEATURE_CONTROL_VMXON)))
        {
            Log(("vmxon: Feature control lock bit or VMXON bit cleared -> #GP(0)\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_MsrFeatCtl;
            return iemRaiseGeneralProtectionFault0(pVCpu);
        }

        /* Get the VMXON pointer from the location specified by the source memory operand. */
        RTGCPHYS GCPhysVmxon;
        VBOXSTRICTRC rcStrict = iemMemFetchDataU64(pVCpu, &GCPhysVmxon, iEffSeg, GCPtrVmxon);
        if (RT_UNLIKELY(rcStrict != VINF_SUCCESS))
        {
            Log(("vmxon: Failed to read VMXON region physaddr from %#RGv, rc=%Rrc\n", GCPtrVmxon, VBOXSTRICTRC_VAL(rcStrict)));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrMap;
            return rcStrict;
        }

        /* VMXON region pointer alignment. */
        if (GCPhysVmxon & X86_PAGE_4K_OFFSET_MASK)
        {
            Log(("vmxon: VMXON region pointer not page-aligned -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrAlign;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* VMXON physical-address width limits. */
        if (GCPhysVmxon >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
        {
            Log(("vmxon: VMXON region pointer extends beyond physical-address width -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrWidth;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Ensure VMXON region is not MMIO, ROM etc. This is not an Intel requirement but a
           restriction imposed by our implementation. */
        if (!PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmxon))
        {
            Log(("vmxon: VMXON region not normal memory -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrAbnormal;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /* Read the VMCS revision ID from the VMXON region. */
        VMXVMCSREVID VmcsRevId;
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), &VmcsRevId, GCPhysVmxon, sizeof(VmcsRevId));
        if (RT_FAILURE(rc))
        {
            Log(("vmxon: Failed to read VMXON region at %#RGp, rc=%Rrc\n", GCPhysVmxon, rc));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_PtrReadPhys;
            return rc;
        }

        /* Verify the VMCS revision specified by the guest matches what we reported to the guest. */
        if (RT_UNLIKELY(VmcsRevId.u != VMX_V_VMCS_REVISION_ID))
        {
            /* Revision ID mismatch. */
            if (!VmcsRevId.n.fIsShadowVmcs)
            {
                Log(("vmxon: VMCS revision mismatch, expected %#RX32 got %#RX32 -> VMFailInvalid\n", VMX_V_VMCS_REVISION_ID,
                     VmcsRevId.n.u31RevisionId));
                pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmcsRevId;
                iemVmxVmFailInvalid(pVCpu);
                iemRegAddToRipAndClearRF(pVCpu, cbInstr);
                return VINF_SUCCESS;
            }

            /* Shadow VMCS disallowed. */
            Log(("vmxon: Shadow VMCS -> VMFailInvalid\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_ShadowVmcs;
            iemVmxVmFailInvalid(pVCpu);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }

        /*
         * Record that we're in VMX operation, block INIT, block and disable A20M.
         */
        pVCpu->cpum.GstCtx.hwvirt.vmx.GCPhysVmxon    = GCPhysVmxon;
        IEM_VMX_CLEAR_CURRENT_VMCS(pVCpu);
        pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = true;
        /** @todo NSTVMX: clear address-range monitoring. */
        /** @todo NSTVMX: Intel PT. */
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_Success;
        iemVmxVmSucceed(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
        return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, true);
# else
        return VINF_SUCCESS;
# endif
    }
    else if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }

    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxon: In VMX root mode: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmxRootCpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* VMXON when already in VMX root mode. */
    iemVmxVmFail(pVCpu, VMXINSTRERR_VMXON_IN_VMXROOTMODE);
    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxon_VmxAlreadyRoot;
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VINF_SUCCESS;
#endif
}


/**
 * Gets the instruction diagnostic for segment base checks during VM-entry of a
 * nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegBase(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegBaseCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegBaseDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegBaseEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegBaseFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegBaseGs;
        default:          return kVmxVDiag_Vmentry_GuestSegBaseSs;
    }
}


/**
 * Gets the instruction diagnostic for segment base checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegBaseV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegBaseV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegBaseV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegBaseV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegBaseV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegBaseV86Gs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegBaseV86Ss;
    }
}


/**
 * Gets the instruction diagnostic for segment limit checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegLimitV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegLimitV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegLimitV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegLimitV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegLimitV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegLimitV86Gs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegLimitV86Ss;
    }
}


/**
 * Gets the instruction diagnostic for segment attribute checks during VM-entry of a
 * nested-guest that is in Virtual-8086 mode.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrV86(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrV86Cs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrV86Ds;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrV86Es;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrV86Fs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrV86Gs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrV86Ss;
    }
}


/**
 * Gets the instruction diagnostic for segment attributes reserved bits failure
 * during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrRsvd(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrRsvdEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrRsvdGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrRsvdSs;
    }
}


/**
 * Gets the instruction diagnostic for segment attributes descriptor-type
 * (code/segment or system) failure during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrDescType(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrDescTypeGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrDescTypeSs;
    }
}


/**
 * Gets the instruction diagnostic for segment attributes descriptor-type
 * (code/segment or system) failure during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrPresent(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrPresentCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrPresentDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrPresentEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrPresentFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrPresentGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrPresentSs;
    }
}


/**
 * Gets the instruction diagnostic for segment attribute granularity failure during
 * VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrGran(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrGranCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrGranDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrGranEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrGranFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrGranGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrGranSs;
    }
}

/**
 * Gets the instruction diagnostic for segment attribute DPL/RPL failure during
 * VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrDplRpl(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrDplRplEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrDplRplGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrDplRplSs;
    }
}


/**
 * Gets the instruction diagnostic for segment attribute type accessed failure
 * during VM-entry of a nested-guest.
 *
 * @param   iSegReg     The segment index (X86_SREG_XXX).
 */
IEM_STATIC VMXVDIAG iemVmxGetDiagVmentrySegAttrTypeAcc(unsigned iSegReg)
{
    switch (iSegReg)
    {
        case X86_SREG_CS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccCs;
        case X86_SREG_DS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccDs;
        case X86_SREG_ES: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccEs;
        case X86_SREG_FS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccFs;
        case X86_SREG_GS: return kVmxVDiag_Vmentry_GuestSegAttrTypeAccGs;
        default:
            Assert(iSegReg == X86_SREG_SS);
            return kVmxVDiag_Vmentry_GuestSegAttrTypeAccSs;
    }
}


/**
 * Checks guest control registers, debug registers and MSRs as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestControlRegsMsrs(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Guest Control Registers, Debug Registers, and MSRs.
     * See Intel spec. 26.3.1.1 "Checks on Guest Control Registers, Debug Registers, and MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fUnrestrictedGuest = RT_BOOL(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST);
    const char *const pszFailure = "VM-exit";

    /* CR0 reserved bits. */
    {
        /* CR0 MB1 bits. */
        uint64_t u64Cr0Fixed0 = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
        Assert(u64Cr0Fixed0 & (X86_CR0_NW | X86_CR0_CD));
        if (fUnrestrictedGuest)
            u64Cr0Fixed0 &= ~(X86_CR0_PE | X86_CR0_PG);
        if (~pVmcs->u64GuestCr0.u & u64Cr0Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0Fixed0);

        /* CR0 MBZ bits. */
        uint64_t const u64Cr0Fixed1 = CPUMGetGuestIa32VmxCr0Fixed1(pVCpu);
        if (pVmcs->u64GuestCr0.u & ~u64Cr0Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0Fixed1);

        /* Without unrestricted guest support, VT-x supports does not support unpaged protected mode. */
        if (   !fUnrestrictedGuest
            &&  (pVmcs->u64GuestCr0.u & X86_CR0_PG)
            && !(pVmcs->u64GuestCr0.u & X86_CR0_PE))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr0PgPe);
    }

    /* CR4 reserved bits. */
    {
        /* CR4 MB1 bits. */
        uint64_t const u64Cr4Fixed0 = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
        if (~pVmcs->u64GuestCr4.u & u64Cr4Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr4Fixed0);

        /* CR4 MBZ bits. */
        uint64_t const u64Cr4Fixed1 = CPUMGetGuestIa32VmxCr4Fixed1(pVCpu);
        if (pVmcs->u64GuestCr4.u & ~u64Cr4Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr4Fixed1);
    }

    /* DEBUGCTL MSR. */
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_DEBUG)
        && (pVmcs->u64GuestDebugCtlMsr.u & ~MSR_IA32_DEBUGCTL_VALID_MASK_INTEL))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestDebugCtl);

    /* 64-bit CPU checks. */
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        if (fGstInLongMode)
        {
            /* PAE must be set. */
            if (   (pVmcs->u64GuestCr0.u & X86_CR0_PG)
                && (pVmcs->u64GuestCr0.u & X86_CR4_PAE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPae);
        }
        else
        {
            /* PCIDE should not be set. */
            if (!(pVmcs->u64GuestCr4.u & X86_CR4_PCIDE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPcide);
        }

        /* CR3. */
        if (!(pVmcs->u64GuestCr3.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestCr3);

        /* DR7. */
        if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_DEBUG)
            && (pVmcs->u64GuestDr7.u & X86_DR7_MBZ_MASK))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestDr7);

        /* SYSENTER ESP and SYSENTER EIP. */
        if (   X86_IS_CANONICAL(pVmcs->u64GuestSysenterEsp.u)
            && X86_IS_CANONICAL(pVmcs->u64GuestSysenterEip.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSysenterEspEip);
    }

    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PERF_MSR));  /* We don't support loading IA32_PERF_GLOBAL_CTRL MSR yet. */

    /* PAT MSR. */
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_PAT_MSR)
        && !CPUMIsPatMsrValid(pVmcs->u64GuestPatMsr.u))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestPatMsr);

    /* EFER MSR. */
    uint64_t const uValidEferMask = CPUMGetGuestEferMsrValidMask(pVCpu->CTX_SUFF(pVM));
    if (   (pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_EFER_MSR)
        && (pVmcs->u64GuestEferMsr.u & ~uValidEferMask))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestEferMsrRsvd);

    bool const fGstLma        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LMA);
    bool const fGstLme        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LME);
    if (   fGstInLongMode == fGstLma
        && (   !(pVmcs->u64GuestCr0.u & X86_CR0_PG)
            || fGstLma == fGstLme))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestEferMsr);

    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_LOAD_BNDCFGS_MSR));   /* We don't support loading IA32_BNDCFGS MSR yet. */

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest segment registers, LDTR and TR as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestSegRegs(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Segment registers.
     * See Intel spec. 26.3.1.2 "Checks on Guest Segment Registers".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    bool const fGstInV86Mode      = RT_BOOL(pVmcs->u64GuestRFlags.u & X86_EFL_VM);
    bool const fUnrestrictedGuest = RT_BOOL(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST);
    bool const fGstInLongMode     = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    const char *const pszFailure = "VM-exit";

    /* Selectors. */
    if (   !fGstInV86Mode
        && !fUnrestrictedGuest
        && (pVmcs->GuestSs & X86_SEL_RPL) != (pVmcs->GuestCs & X86_SEL_RPL))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelCsSsRpl);

    for (unsigned iSegReg = 0; iSegReg < X86_SREG_COUNT; iSegReg++)
    {
        CPUMSELREG SelReg;
        int rc = iemVmxVmcsGetGuestSegReg(pVmcs, iSegReg, &SelReg);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        { /* likely */ }
        else
            return rc;

        /*
         * Virtual-8086 mode checks.
         */
        if (fGstInV86Mode)
        {
            /* Base address. */
            if (SelReg.u64Base == (uint64_t)SelReg.Sel << 4)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBaseV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Limit. */
            if (SelReg.u32Limit == 0xffff)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegLimitV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Attribute. */
            if (SelReg.Attr.u == 0xf3)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrV86(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* We're done; move to checking the next segment. */
            continue;
        }

        /* Checks done by 64-bit CPUs. */
        if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
        {
            /* Base address. */
            if (   iSegReg == X86_SREG_FS
                || iSegReg == X86_SREG_GS)
            {
                if (X86_IS_CANONICAL(SelReg.u64Base))
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBase(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }
            }
            else if (iSegReg == X86_SREG_CS)
            {
                if (!RT_HI_U32(SelReg.u64Base))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseCs);
            }
            else
            {
                if (   SelReg.Attr.n.u1Unusable
                    || !RT_HI_U32(SelReg.u64Base))
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegBase(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }
            }
        }

        /*
         * Checks outside Virtual-8086 mode.
         */
        uint8_t const uSegType     =  SelReg.Attr.n.u4Type;
        uint8_t const fCodeDataSeg =  SelReg.Attr.n.u1DescType;
        uint8_t const fUsable      = !SelReg.Attr.n.u1Unusable;
        uint8_t const uDpl         =  SelReg.Attr.n.u2Dpl;
        uint8_t const fPresent     =  SelReg.Attr.n.u1Present;
        uint8_t const uGranularity =  SelReg.Attr.n.u1Granularity;
        uint8_t const uDefBig      =  SelReg.Attr.n.u1DefBig;
        uint8_t const fSegLong     =  SelReg.Attr.n.u1Long;

        /* Code or usable segment. */
        if (   iSegReg == X86_SREG_CS
            || fUsable)
        {
            /* Reserved bits (bits 31:17 and bits 11:8). */
            if (!(SelReg.Attr.u & 0xfffe0f00))
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrRsvd(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Descriptor type. */
            if (fCodeDataSeg)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrDescType(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Present. */
            if (fPresent)
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrPresent(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }

            /* Granularity. */
            if (   ((SelReg.u32Limit & 0x00000fff) == 0x00000fff || !uGranularity)
                && ((SelReg.u32Limit & 0xfff00000) == 0x00000000 ||  uGranularity))
            { /* likely */ }
            else
            {
                VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrGran(iSegReg);
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
            }
        }

        if (iSegReg == X86_SREG_CS)
        {
            /* Segment Type and DPL. */
            if (   uSegType == (X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED)
                && fUnrestrictedGuest)
            {
                if (uDpl == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplZero);
            }
            else if (   uSegType == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
                     || uSegType == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_ACCESSED))
            {
                X86DESCATTR SsAttr; SsAttr.u = pVmcs->u32GuestSsAttr;
                if (uDpl == SsAttr.n.u2Dpl)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplEqSs);
            }
            else if ((uSegType & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF | X86_SEL_TYPE_ACCESSED))
                              == (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF | X86_SEL_TYPE_ACCESSED))
            {
                X86DESCATTR SsAttr; SsAttr.u = pVmcs->u32GuestSsAttr;
                if (uDpl <= SsAttr.n.u2Dpl)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDplLtSs);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsType);

            /* Def/Big. */
            if (   fGstInLongMode
                && fSegLong)
            {
                if (uDefBig == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsDefBig);
            }
        }
        else if (iSegReg == X86_SREG_SS)
        {
            /* Segment Type. */
            if (   !fUsable
                || uSegType == (X86_SEL_TYPE_RW   | X86_SEL_TYPE_ACCESSED)
                || uSegType == (X86_SEL_TYPE_DOWN | X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsType);

            /* DPL. */
            if (fUnrestrictedGuest)
            {
                if (uDpl == (SelReg.Sel & X86_SEL_RPL))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsDplEqRpl);
            }
            X86DESCATTR CsAttr; CsAttr.u = pVmcs->u32GuestCsAttr;
            if (   CsAttr.n.u4Type == (X86_SEL_TYPE_RW | X86_SEL_TYPE_ACCESSED)
                || (pVmcs->u64GuestCr0.u & X86_CR0_PE))
            {
                if (uDpl == 0)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrSsDplZero);
            }
        }
        else
        {
            /* DS, ES, FS, GS. */
            if (fUsable)
            {
                /* Segment type. */
                if (uSegType & X86_SEL_TYPE_ACCESSED)
                { /* likely */ }
                else
                {
                    VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrTypeAcc(iSegReg);
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                }

                if (   !(uSegType & X86_SEL_TYPE_CODE)
                    ||  (uSegType & X86_SEL_TYPE_READ))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrCsTypeRead);

                /* DPL. */
                if (   !fUnrestrictedGuest
                    && uSegType <= (X86_SEL_TYPE_CODE | X86_SEL_TYPE_READ | X86_SEL_TYPE_ACCESSED))
                {
                    if (uDpl >= (SelReg.Sel & X86_SEL_RPL))
                    { /* likely */ }
                    else
                    {
                        VMXVDIAG const enmDiag = iemVmxGetDiagVmentrySegAttrDplRpl(iSegReg);
                        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, enmDiag);
                    }
                }
            }
        }
    }

    /*
     * LDTR.
     */
    {
        CPUMSELREG Ldtr;
        Ldtr.Sel      = pVmcs->GuestLdtr;
        Ldtr.u32Limit = pVmcs->u32GuestLdtrLimit;
        Ldtr.u64Base  = pVmcs->u64GuestLdtrBase.u;
        Ldtr.Attr.u   = pVmcs->u32GuestLdtrLimit;

        if (!Ldtr.Attr.n.u1Unusable)
        {
            /* Selector. */
            if (!(Ldtr.Sel & X86_SEL_LDT))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelLdtr);

            /* Base. */
            if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
            {
                if (X86_IS_CANONICAL(Ldtr.u64Base))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseLdtr);
            }

            /* Attributes. */
            /* Reserved bits (bits 31:17 and bits 11:8). */
            if (!(Ldtr.Attr.u & 0xfffe0f00))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrRsvd);

            if (Ldtr.Attr.n.u4Type == X86_SEL_TYPE_SYS_LDT)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrType);

            if (!Ldtr.Attr.n.u1DescType)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrDescType);

            if (Ldtr.Attr.n.u1Present)
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrPresent);

            if (   ((Ldtr.u32Limit & 0x00000fff) == 0x00000fff || !Ldtr.Attr.n.u1Granularity)
                && ((Ldtr.u32Limit & 0xfff00000) == 0x00000000 ||  Ldtr.Attr.n.u1Granularity))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrLdtrGran);
        }
    }

    /*
     * TR.
     */
    {
        CPUMSELREG Tr;
        Tr.Sel      = pVmcs->GuestTr;
        Tr.u32Limit = pVmcs->u32GuestTrLimit;
        Tr.u64Base  = pVmcs->u64GuestTrBase.u;
        Tr.Attr.u   = pVmcs->u32GuestTrLimit;

        /* Selector. */
        if (!(Tr.Sel & X86_SEL_LDT))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegSelTr);

        /* Base. */
        if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
        {
            if (X86_IS_CANONICAL(Tr.u64Base))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegBaseTr);
        }

        /* Attributes. */
        /* Reserved bits (bits 31:17 and bits 11:8). */
        if (!(Tr.Attr.u & 0xfffe0f00))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrRsvd);

        if (!Tr.Attr.n.u1Unusable)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrUnusable);

        if (   Tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_386_TSS_BUSY
            || (   !fGstInLongMode
                && Tr.Attr.n.u4Type == X86_SEL_TYPE_SYS_286_TSS_BUSY))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrType);

        if (!Tr.Attr.n.u1DescType)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrDescType);

        if (Tr.Attr.n.u1Present)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrPresent);

        if (   ((Tr.u32Limit & 0x00000fff) == 0x00000fff || !Tr.Attr.n.u1Granularity)
            && ((Tr.u32Limit & 0xfff00000) == 0x00000000 ||  Tr.Attr.n.u1Granularity))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestSegAttrTrGran);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest GDTR and IDTR as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestGdtrIdtr(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * GDTR and IDTR.
     * See Intel spec. 26.3.1.3 "Checks on Guest Descriptor-Table Registers".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* Base. */
        if (X86_IS_CANONICAL(pVmcs->u64GuestGdtrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestGdtrBase);

        if (X86_IS_CANONICAL(pVmcs->u64GuestIdtrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIdtrBase);
    }

    /* Limit. */
    if (!RT_HI_U16(pVmcs->u32GuestGdtrLimit))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestGdtrLimit);

    if (!RT_HI_U16(pVmcs->u32GuestIdtrLimit))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestIdtrLimit);

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest RIP and RFLAGS as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestRipRFlags(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * RIP and RFLAGS.
     * See Intel spec. 26.3.1.4 "Checks on Guest RIP and RFLAGS".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);

    /* RIP. */
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        X86DESCATTR CsAttr; CsAttr.u = pVmcs->u32GuestCsAttr;
        if (   !fGstInLongMode
            || !CsAttr.n.u1Long)
        {
            if (!RT_HI_U32(pVmcs->u64GuestRip.u))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRipRsvd);
        }

        if (   fGstInLongMode
            && CsAttr.n.u1Long)
        {
            Assert(IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxLinearAddrWidth == 48);   /* Canonical. */
            if (   IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxLinearAddrWidth < 64
                && X86_IS_CANONICAL(pVmcs->u64GuestRip.u))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRip);
        }
    }

    /* RFLAGS (bits 63:22 (or 31:22), bits 15, 5, 3 are reserved, bit 1 MB1). */
    uint64_t fMbzMask = IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode ? UINT64_C(0xffffffffffc08028) : UINT32_C(0xffc08028);
    uint64_t fMb1Mask = X86_EFL_RA1_MASK;
    if (   !(pVmcs->u64GuestRFlags.u & fMbzMask)
        &&  (pVmcs->u64GuestRFlags.u & fMb1Mask) == fMb1Mask)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsRsvd);

    if (   fGstInLongMode
        || !(pVmcs->u64GuestCr0.u & X86_CR0_PE))
    {
        if (!(pVmcs->u64GuestRFlags.u & X86_EFL_VM))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsVm);
    }

    if (   VMX_ENTRY_INT_INFO_IS_VALID(pVmcs->u32EntryIntInfo)
        && VMX_ENTRY_INT_INFO_TYPE(pVmcs->u32EntryIntInfo) == VMX_ENTRY_INT_INFO_TYPE_EXT_INT)
    {
        if (pVmcs->u64GuestRFlags.u & X86_EFL_IF)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestRFlagsIf);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest non-register state as part of VM-entry.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestNonRegState(PVMCPU pVCpu,  const char *pszInstr)
{
    /*
     * Guest non-register state.
     * See Intel spec. 26.3.1.5 "Checks on Guest Non-Register State".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char *const pszFailure = "VM-exit";

    /*
     * Activity state.
     */
    if (!(pVmcs->u32GuestActivityState & VMX_V_GUEST_ACTIVITY_STATE_MASK))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateRsvd);

    X86DESCATTR SsAttr; SsAttr.u = pVmcs->u32GuestSsAttr;
    if (SsAttr.n.u2Dpl != 0)
    {
        if (pVmcs->u32GuestActivityState != VMX_VMCS_GUEST_ACTIVITY_HLT)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateSsDpl);
    }

    if (   pVmcs->u32GuestIntrState == VMX_VMCS_GUEST_INT_STATE_BLOCK_STI
        || pVmcs->u32GuestIntrState == VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS)
    {
        if (pVmcs->u32GuestActivityState == VMX_VMCS_GUEST_ACTIVITY_ACTIVE)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateStiMovSs);
    }

    if (VMX_ENTRY_INT_INFO_IS_VALID(pVmcs->u32EntryIntInfo))
    {
        uint8_t const uIntType = VMX_ENTRY_INT_INFO_TYPE(pVmcs->u32EntryIntInfo);
        uint8_t const uVector  = VMX_ENTRY_INT_INFO_VECTOR(pVmcs->u32EntryIntInfo);
        switch (pVmcs->u32GuestActivityState)
        {
            case VMX_VMCS_GUEST_ACTIVITY_HLT:
            {
                if (   uIntType == VMX_ENTRY_INT_INFO_TYPE_EXT_INT
                    || uIntType == VMX_ENTRY_INT_INFO_TYPE_NMI
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                        && (   uVector == X86_XCPT_DB
                            || uVector == X86_XCPT_MC))
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT
                        && uVector == 0))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateHlt);
                break;
            }

            case VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN:
            {
                if (   uIntType == VMX_ENTRY_INT_INFO_TYPE_NMI
                    || (   uIntType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                        && uVector == X86_XCPT_MC))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_GuestActStateShutdown);
                break;
            }

            case VMX_VMCS_GUEST_ACTIVITY_ACTIVE:
            default:
                break;
        }
    }

    /*
     * Interruptibility state.
     */
    /** @todo NSTVMX: interruptibility-state. */

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks guest-state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckGuestState(PVMCPU pVCpu, const char *pszInstr)
{
    int rc = iemVmxVmentryCheckGuestControlRegsMsrs(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
        return rc;

    rc = iemVmxVmentryCheckGuestSegRegs(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
        return rc;

    rc = iemVmxVmentryCheckGuestGdtrIdtr(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
        return rc;

    rc = iemVmxVmentryCheckGuestRipRFlags(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
        return rc;

    rc = iemVmxVmentryCheckGuestNonRegState(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
        return rc;

    return VINF_SUCCESS;
}


/**
 * Checks host-state as part of VM-entry.
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckHostState(PVMCPU pVCpu, const char *pszInstr)
{
    /*
     * Host Control Registers and MSRs.
     * See Intel spec. 26.2.2 "Checks on Host Control Registers and MSRs".
     */
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* CR0 reserved bits. */
    {
        /* CR0 MB1 bits. */
        uint64_t const u64Cr0Fixed0 = CPUMGetGuestIa32VmxCr0Fixed0(pVCpu);
        if (~pVmcs->u64HostCr0.u & u64Cr0Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr0Fixed0);

        /* CR0 MBZ bits. */
        uint64_t const u64Cr0Fixed1 = CPUMGetGuestIa32VmxCr0Fixed1(pVCpu);
        if (pVmcs->u64HostCr0.u & ~u64Cr0Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr0Fixed1);
    }

    /* CR4 reserved bits. */
    {
        /* CR4 MB1 bits. */
        uint64_t const u64Cr4Fixed0 = CPUMGetGuestIa32VmxCr4Fixed0(pVCpu);
        if (~pVmcs->u64HostCr4.u & u64Cr4Fixed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Fixed0);

        /* CR4 MBZ bits. */
        uint64_t const u64Cr4Fixed1 = CPUMGetGuestIa32VmxCr4Fixed1(pVCpu);
        if (pVmcs->u64HostCr4.u & ~u64Cr4Fixed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Fixed1);
    }

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* CR3 reserved bits. */
        if (!(pVmcs->u64HostCr3.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cMaxPhysAddrWidth))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr3);

        /* SYSENTER ESP and SYSENTER EIP. */
        if (   X86_IS_CANONICAL(pVmcs->u64HostSysenterEsp.u)
            && X86_IS_CANONICAL(pVmcs->u64HostSysenterEip.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSysenterEspEip);
    }

    Assert(!(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_PERF_MSR));   /* We don't support loading IA32_PERF_GLOBAL_CTRL MSR yet. */

    /* PAT MSR. */
    if (   !(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_PAT_MSR)
        ||  CPUMIsPatMsrValid(pVmcs->u64HostPatMsr.u))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostPatMsr);

    /* EFER MSR. */
    uint64_t const uValidEferMask = CPUMGetGuestEferMsrValidMask(pVCpu->CTX_SUFF(pVM));
    if (   !(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_LOAD_EFER_MSR)
        || !(pVmcs->u64HostEferMsr.u & ~uValidEferMask))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostEferMsrRsvd);

    bool const fHostInLongMode = RT_BOOL(pVmcs->u32ExitCtls & VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE);
    bool const fHostLma        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LMA);
    bool const fHostLme        = RT_BOOL(pVmcs->u64HostEferMsr.u & MSR_K6_EFER_BIT_LME);
    if (   fHostInLongMode == fHostLma
        && fHostInLongMode == fHostLme)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostEferMsr);

    /*
     * Host Segment and Descriptor-Table Registers.
     * See Intel spec. 26.2.3 "Checks on Host Segment and Descriptor-Table Registers".
     */
    /* Selector RPL and TI. */
    if (   !(pVmcs->HostCs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostSs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostDs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostEs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostFs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostGs & (X86_SEL_RPL | X86_SEL_LDT))
        && !(pVmcs->HostTr & (X86_SEL_RPL | X86_SEL_LDT)))
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSel);

    /* CS and TR selectors cannot be 0. */
    if (   pVmcs->HostCs
        && pVmcs->HostTr)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCsTr);

    /* SS cannot be 0 if 32-bit host. */
    if (   fHostInLongMode
        || pVmcs->HostSs)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSs);

    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        /* FS, GS, GDTR, IDTR, TR base address. */
        if (   X86_IS_CANONICAL(pVmcs->u64HostFsBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostFsBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostGdtrBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostIdtrBase.u)
            && X86_IS_CANONICAL(pVmcs->u64HostTrBase.u))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostSegBase);
    }

    /*
     * Host address-space size for 64-bit CPUs.
     * See Intel spec. 26.2.4 "Checks Related to Address-Space Size".
     */
    bool const fGstInLongMode = RT_BOOL(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_IA32E_MODE_GUEST);
    if (IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fLongMode)
    {
        bool const fCpuInLongMode = CPUMIsGuestInLongMode(pVCpu);

        /* Logical processor in IA-32e mode. */
        if (fCpuInLongMode)
        {
            if (fHostInLongMode)
            {
                /* PAE must be set. */
                if (pVmcs->u64HostCr4.u & X86_CR4_PAE)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Pae);

                /* RIP must be canonical. */
                if (X86_IS_CANONICAL(pVmcs->u64HostRip.u))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostRip);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostLongMode);
        }
        else
        {
            /* Logical processor is outside IA-32e mode. */
            if (   !fGstInLongMode
                && !fHostInLongMode)
            {
                /* PCIDE should not be set. */
                if (!(pVmcs->u64HostCr4.u & X86_CR4_PCIDE))
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostCr4Pcide);

                /* The high 32-bits of RIP MBZ. */
                if (!pVmcs->u64HostRip.s.Hi)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostRipRsvd);
            }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostGuestLongMode);
        }
    }
    else
    {
        /* Host address-space size for 32-bit CPUs. */
        if (   !fGstInLongMode
            && !fHostInLongMode)
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_HostGuestLongModeNoCpu);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-entry controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.3 "VM-Entry Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckEntryCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* VM-entry controls. */
    VMXCTLSMSR EntryCtls;
    EntryCtls.u = CPUMGetGuestIa32VmxEntryCtls(pVCpu);
    if (~pVmcs->u32EntryCtls & EntryCtls.n.disallowed0)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryCtlsDisallowed0);

    if (pVmcs->u32EntryCtls & ~EntryCtls.n.allowed1)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryCtlsAllowed1);

    /* Event injection. */
    uint32_t const uIntInfo = pVmcs->u32EntryIntInfo;
    if (RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_VALID))
    {
        /* Type and vector. */
        uint8_t const uType         = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_TYPE);
        uint8_t const uVector       = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_VECTOR);
        uint8_t const uRsvd         = RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_RSVD_12_30);
        if (   uRsvd == 0
            && HMVmxIsEntryIntInfoTypeValid(IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxMonitorTrapFlag, uType)
            && HMVmxIsEntryIntInfoVectorValid(uVector, uType))
        { /* likely */ }
        else
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoTypeVecRsvd);

        /* Exception error code. */
        if (RT_BF_GET(uIntInfo, VMX_BF_ENTRY_INT_INFO_ERR_CODE_VALID))
        {
            /* Delivery possible only in Unrestricted-guest mode when CR0.PE is set. */
            if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)
                ||  (pVmcs->u64GuestCr0.s.Lo & X86_CR0_PE))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoErrCodePe);

            /* Exceptions that provide an error code. */
            if (   uType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
                && (   uVector == X86_XCPT_DF
                    || uVector == X86_XCPT_TS
                    || uVector == X86_XCPT_NP
                    || uVector == X86_XCPT_SS
                    || uVector == X86_XCPT_GP
                    || uVector == X86_XCPT_PF
                    || uVector == X86_XCPT_AC))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryIntInfoErrCodeVec);

            /* Exception error-code reserved bits. */
            if (!(pVmcs->u32EntryXcptErrCode & ~VMX_ENTRY_INT_XCPT_ERR_CODE_VALID_MASK))
            { /* likely */ }
            else
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryXcptErrCodeRsvd);

            /* Injecting a software interrupt, software exception or privileged software exception. */
            if (   uType == VMX_ENTRY_INT_INFO_TYPE_SW_INT
                || uType == VMX_ENTRY_INT_INFO_TYPE_SW_XCPT
                || uType == VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT)
            {
                /* Instruction length must be in the range 0-15. */
                if (pVmcs->u32EntryInstrLen <= VMX_ENTRY_INSTR_LEN_MAX)
                { /* likely */ }
                else
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryInstrLen);

                /* Instruction length of 0 is allowed only when its CPU feature is present. */
                if (   pVmcs->u32EntryInstrLen == 0
                    && !IEM_GET_GUEST_CPU_FEATURES(pVCpu)->fVmxEntryInjectSoftInt)
                    IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_EntryInstrLenZero);
            }
        }
    }

    /* VM-entry MSR-load count and VM-entry MSR-load area address. */
    if (pVmcs->u32EntryMsrLoadCount)
    {
        if (   (pVmcs->u64AddrEntryMsrLoad.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrEntryMsrLoad.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrEntryMsrLoad.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrEntryMsrLoad);
    }

    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_ENTRY_TO_SMM));           /* We don't support SMM yet. */
    Assert(!(pVmcs->u32EntryCtls & VMX_ENTRY_CTLS_DEACTIVATE_DUAL_MON));    /* We don't support dual-monitor treatment yet. */

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-exit controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.2 "VM-Exit Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 */
IEM_STATIC int iemVmxVmentryCheckExitCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PCVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* VM-exit controls. */
    VMXCTLSMSR ExitCtls;
    ExitCtls.u = CPUMGetGuestIa32VmxExitCtls(pVCpu);
    if (~pVmcs->u32ExitCtls & ExitCtls.n.disallowed0)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ExitCtlsDisallowed0);

    if (pVmcs->u32ExitCtls & ~ExitCtls.n.allowed1)
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ExitCtlsAllowed1);

    /* Save preemption timer without activating it. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_PREEMPT_TIMER)
        && (pVmcs->u32ProcCtls & VMX_EXIT_CTLS_SAVE_PREEMPT_TIMER))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_SavePreemptTimer);

    /* VM-exit MSR-store count and VM-exit MSR-store area address. */
    if (pVmcs->u32ExitMsrStoreCount)
    {
        if (   (pVmcs->u64AddrExitMsrStore.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrExitMsrStore.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrExitMsrStore.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrExitMsrStore);
    }

    /* VM-exit MSR-load count and VM-exit MSR-load area address. */
    if (pVmcs->u32ExitMsrLoadCount)
    {
        if (   (pVmcs->u64AddrExitMsrLoad.u & VMX_AUTOMSR_OFFSET_MASK)
            || (pVmcs->u64AddrExitMsrLoad.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrExitMsrLoad.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrExitMsrLoad);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * Checks VM-execution controls fields as part of VM-entry.
 * See Intel spec. 26.2.1.1 "VM-Execution Control Fields".
 *
 * @returns VBox status code.
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   pszInstr        The VMX instruction name (for logging purposes).
 *
 * @remarks This may update secondary-processor based VM-execution control fields
 *          in the current VMCS if necessary.
 */
IEM_STATIC int iemVmxVmentryCheckExecCtls(PVMCPU pVCpu, const char *pszInstr)
{
    PVMXVVMCS pVmcs = pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs);
    const char * const pszFailure = "VMFail";

    /* Pin-based VM-execution controls. */
    {
        VMXCTLSMSR PinCtls;
        PinCtls.u = CPUMGetGuestIa32VmxPinbasedCtls(pVCpu);
        if (~pVmcs->u32PinCtls & PinCtls.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_PinCtlsDisallowed0);

        if (pVmcs->u32PinCtls & ~PinCtls.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_PinCtlsAllowed1);
    }

    /* Processor-based VM-execution controls. */
    {
        VMXCTLSMSR ProcCtls;
        ProcCtls.u = CPUMGetGuestIa32VmxProcbasedCtls(pVCpu);
        if (~pVmcs->u32ProcCtls & ProcCtls.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtlsDisallowed0);

        if (pVmcs->u32ProcCtls & ~ProcCtls.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtlsAllowed1);
    }

    /* Secondary processor-based VM-execution controls. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_SECONDARY_CTLS)
    {
        VMXCTLSMSR ProcCtls2;
        ProcCtls2.u = CPUMGetGuestIa32VmxProcbasedCtls2(pVCpu);
        if (~pVmcs->u32ProcCtls2 & ProcCtls2.n.disallowed0)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtls2Disallowed0);

        if (pVmcs->u32ProcCtls2 & ~ProcCtls2.n.allowed1)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ProcCtls2Allowed1);
    }
    else
        Assert(!pVmcs->u32ProcCtls2);

    /* CR3-target count. */
    if (pVmcs->u32Cr3TargetCount <= VMX_V_CR3_TARGET_COUNT)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_Cr3TargetCount);

    /* IO bitmaps physical addresses. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_IO_BITMAPS)
    {
        if (   (pVmcs->u64AddrIoBitmapA.u & X86_PAGE_4K_OFFSET_MASK)
            || (pVmcs->u64AddrIoBitmapA.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrIoBitmapA.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrIoBitmapA);

        if (   (pVmcs->u64AddrIoBitmapB.u & X86_PAGE_4K_OFFSET_MASK)
            || (pVmcs->u64AddrIoBitmapB.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrIoBitmapB.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrIoBitmapB);
    }

    /* MSR bitmap physical address. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_MSR_BITMAPS)
    {
        if (   (pVmcs->u64AddrMsrBitmap.u & X86_PAGE_4K_OFFSET_MASK)
            || (pVmcs->u64AddrMsrBitmap.u >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), pVmcs->u64AddrMsrBitmap.u))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrMsrBitmap);
    }

    /* TPR shadow related controls. */
    if (pVmcs->u32ProcCtls & VMX_PROC_CTLS_USE_TPR_SHADOW)
    {
        /* Virtual-APIC page physical address. */
        RTGCPHYS GCPhysVirtApic = pVmcs->u64AddrVirtApic.u;
        if (   (GCPhysVirtApic & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysVirtApic >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVirtApic))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVirtApicPage);

        /* Read the Virtual-APIC page. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage),
                                         GCPhysVirtApic, VMX_V_VIRT_APIC_PAGES);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtApicPagePtrReadPhys);

        /* TPR threshold without virtual-interrupt delivery. */
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
            &&  (pVmcs->u32TprThreshold & VMX_TPR_THRESHOLD_MASK))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_TprThreshold);

        /* TPR threshold and VTPR. */
        uint8_t const *pbVirtApic = (uint8_t *)pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVirtApicPage);
        uint8_t const u8VTpr = *(pbVirtApic + XAPIC_OFF_TPR);
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
            && RT_BF_GET(pVmcs->u32TprThreshold, VMX_BF_TPR_THRESHOLD_TPR) > ((u8VTpr >> 4) & UINT32_C(0xf)) /* Bits 4:7 */)
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_TprThresholdVTpr);
    }
    else
    {
        if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_APIC_REG_VIRT)
            && !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY))
        { /* likely */ }
        else
        {
            if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicTprShadow);
            if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_APIC_REG_VIRT)
                IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_ApicRegVirt);
            Assert(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY);
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtIntDelivery);
        }
    }

    /* NMI exiting and virtual-NMIs. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_NMI_EXIT)
        &&  (pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtNmi);

    /* Virtual-NMIs and NMI-window exiting. */
    if (   !(pVmcs->u32PinCtls & VMX_PIN_CTLS_VIRT_NMI)
        && (pVmcs->u32ProcCtls & VMX_PROC_CTLS_NMI_WINDOW_EXIT))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_NmiWindowExit);

    /* Virtualize APIC accesses. */
    if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS)
    {
        /* APIC-access physical address. */
        RTGCPHYS GCPhysApicAccess = pVmcs->u64AddrApicAccess.u;
        if (   (GCPhysApicAccess & X86_PAGE_4K_OFFSET_MASK)
            || (GCPhysApicAccess >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysApicAccess))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrApicAccess);
    }

    /* Virtualize-x2APIC mode is mutually exclusive with virtualize-APIC accesses. */
    if (   (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_X2APIC_MODE)
        && (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_APIC_ACCESS))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicVirtApic);

    /* Virtual-interrupt delivery requires external interrupt exiting. */
    if (   (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VIRT_INT_DELIVERY)
        && !(pVmcs->u32PinCtls & VMX_PIN_CTLS_EXT_INT_EXIT))
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VirtX2ApicVirtApic);

    /* VPID. */
    if (   !(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VPID)
        || pVmcs->u16Vpid != 0)
    { /* likely */ }
    else
        IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_Vpid);

    Assert(!(pVmcs->u32PinCtls & VMX_PIN_CTLS_POSTED_INT));             /* We don't support posted interrupts yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT));                /* We don't support EPT yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_PML));                /* We don't support PML yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_UNRESTRICTED_GUEST)); /* We don't support Unrestricted-guests yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VMFUNC));             /* We don't support VM functions yet. */
    Assert(!(pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_EPT_VE));             /* We don't support EPT-violation #VE yet. */

    /* VMCS shadowing. */
    if (pVmcs->u32ProcCtls2 & VMX_PROC_CTLS2_VMCS_SHADOWING)
    {
        /* VMREAD-bitmap physical address. */
        RTGCPHYS GCPhysVmreadBitmap = pVmcs->u64AddrVmreadBitmap.u;
        if (   ( GCPhysVmreadBitmap & X86_PAGE_4K_OFFSET_MASK)
            || ( GCPhysVmreadBitmap >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmreadBitmap))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVmreadBitmap);

        /* VMWRITE-bitmap physical address. */
        RTGCPHYS GCPhysVmwriteBitmap = pVmcs->u64AddrVmreadBitmap.u;
        if (   ( GCPhysVmwriteBitmap & X86_PAGE_4K_OFFSET_MASK)
            || ( GCPhysVmwriteBitmap >> IEM_GET_GUEST_CPU_FEATURES(pVCpu)->cVmxMaxPhysAddrWidth)
            || !PGMPhysIsGCPhysNormal(pVCpu->CTX_SUFF(pVM), GCPhysVmwriteBitmap))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_AddrVmwriteBitmap);

        /* Read the VMREAD-bitmap. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap));
        int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmreadBitmap),
                                         GCPhysVmreadBitmap, VMX_V_VMREAD_VMWRITE_BITMAP_SIZE);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmreadBitmapPtrReadPhys);

        /* Read the VMWRITE-bitmap. */
        Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmwriteBitmap));
        rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pvVmwriteBitmap),
                                     GCPhysVmwriteBitmap, VMX_V_VMREAD_VMWRITE_BITMAP_SIZE);
        if (RT_FAILURE(rc))
            IEM_VMX_VMENTRY_FAILED_RET(pVCpu, pszInstr, pszFailure, kVmxVDiag_Vmentry_VmwriteBitmapPtrReadPhys);
    }

    NOREF(pszInstr);
    NOREF(pszFailure);
    return VINF_SUCCESS;
}


/**
 * VMLAUNCH/VMRESUME instruction execution worker.
 *
 * @param   pVCpu           The cross context virtual CPU structure.
 * @param   cbInstr         The instruction length.
 * @param   uInstrId        The instruction identity (either VMXINSTRID_VMLAUNCH or
 *                          VMXINSTRID_VMRESUME).
 * @param   pExitInfo       Pointer to the VM-exit instruction information struct.
 *                          Optional, can  be NULL.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_STATIC VBOXSTRICTRC iemVmxVmlaunchVmresume(PVMCPU pVCpu, uint8_t cbInstr, VMXINSTRID uInstrId, PCVMXVEXITINFO pExitInfo)
{
    Assert(   uInstrId == VMXINSTRID_VMLAUNCH
           || uInstrId == VMXINSTRID_VMRESUME);

    const char *pszInstr = uInstrId == VMXINSTRID_VMLAUNCH ? "vmlaunch" : "vmresume";
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        RT_NOREF(pExitInfo);
        /** @todo NSTVMX: intercept. */
    }
    Assert(IEM_IS_VMX_ROOT_MODE(pVCpu));

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("%s: CPL %u -> #GP(0)\n", pszInstr, pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Current VMCS valid. */
    if (!IEM_VMX_HAS_CURRENT_VMCS(pVCpu))
    {
        Log(("%s: VMCS pointer %#RGp invalid -> VMFailInvalid\n", pszInstr, IEM_VMX_GET_CURRENT_VMCS(pVCpu)));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_PtrInvalid;
        iemVmxVmFailInvalid(pVCpu);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /** @todo Distinguish block-by-MOV-SS from block-by-STI. Currently we
     *        use block-by-STI here which is not quite correct. */
    if (   VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_INHIBIT_INTERRUPTS)
        && pVCpu->cpum.GstCtx.rip == EMGetInhibitInterruptsPC(pVCpu))
    {
        Log(("%s: VM entry with events blocked by MOV SS -> VMFail\n", pszInstr));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_BlocKMovSS;
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_BLOCK_MOVSS);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    if (uInstrId == VMXINSTRID_VMLAUNCH)
    {
        /* VMLAUNCH with non-clear VMCS. */
        if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState != VMX_V_VMCS_STATE_CLEAR)
        {
            Log(("vmlaunch: VMLAUNCH with non-clear VMCS -> VMFail\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_VmcsClear;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMLAUNCH_NON_CLEAR_VMCS);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }
    }
    else
    {
        /* VMRESUME with non-launched VMCS. */
        if (pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs)->fVmcsState != VMX_V_VMCS_STATE_LAUNCHED)
        {
            Log(("vmresume: VMRESUME with non-launched VMCS -> VMFail\n"));
            pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_VmcsLaunch;
            iemVmxVmFail(pVCpu, VMXINSTRERR_VMRESUME_NON_LAUNCHED_VMCS);
            iemRegAddToRipAndClearRF(pVCpu, cbInstr);
            return VINF_SUCCESS;
        }
    }

    /*
     * Load the current VMCS.
     */
    Assert(pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs));
    int rc = PGMPhysSimpleReadGCPhys(pVCpu->CTX_SUFF(pVM), pVCpu->cpum.GstCtx.hwvirt.vmx.CTX_SUFF(pVmcs),
                                     IEM_VMX_GET_CURRENT_VMCS(pVCpu), VMX_V_VMCS_SIZE);
    if (RT_FAILURE(rc))
    {
        Log(("%s: Failed to read VMCS at %#RGp, rc=%Rrc\n", pszInstr, IEM_VMX_GET_CURRENT_VMCS(pVCpu), rc));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_PtrReadPhys;
        return rc;
    }

    /*
     * Check VM-execution control fields.
     */
    rc = iemVmxVmentryCheckExecCtls(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_CTLS);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Check VM-exit control fields.
     */
    rc = iemVmxVmentryCheckExitCtls(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_CTLS);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Check VM-entry control fields.
     */
    rc = iemVmxVmentryCheckEntryCtls(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_CTLS);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Check host-state fields.
     */
    rc = iemVmxVmentryCheckHostState(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMENTRY_INVALID_HOST_STATE);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Check guest-state fields.
     */
    rc = iemVmxVmentryCheckGuestState(pVCpu, pszInstr);
    if (rc == VINF_SUCCESS)
    { /* likely */ }
    else
    {
        /* VMExit. */
        return VINF_SUCCESS;
    }

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmentry_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
    return VERR_IEM_IPE_2;
}


/**
 * Implements 'VMXON'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmxon, uint8_t, iEffSeg, RTGCPTR, GCPtrVmxon)
{
    return iemVmxVmxon(pVCpu, cbInstr, iEffSeg, GCPtrVmxon, NULL /* pExitInfo */);
}


/**
 * Implements 'VMXOFF'.
 *
 * @remarks Common VMX instruction checks are already expected to by the caller,
 *          i.e. CR4.VMXE, Real/V86 mode, EFER/CS.L checks.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmxoff)
{
# if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && !defined(IN_RING3)
    RT_NOREF2(pVCpu, cbInstr);
    return VINF_EM_RAW_EMULATE_INSTR;
# else
    if (IEM_IS_VMX_NON_ROOT_MODE(pVCpu))
    {
        /** @todo NSTVMX: intercept. */
    }

    /* CPL. */
    if (pVCpu->iem.s.uCpl > 0)
    {
        Log(("vmxoff: CPL %u -> #GP(0)\n", pVCpu->iem.s.uCpl));
        pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxoff_Cpl;
        return iemRaiseGeneralProtectionFault0(pVCpu);
    }

    /* Dual monitor treatment of SMIs and SMM. */
    uint64_t const fSmmMonitorCtl = CPUMGetGuestIa32SmmMonitorCtl(pVCpu);
    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VALID)
    {
        iemVmxVmFail(pVCpu, VMXINSTRERR_VMXOFF_DUAL_MON);
        iemRegAddToRipAndClearRF(pVCpu, cbInstr);
        return VINF_SUCCESS;
    }

    /*
     * Record that we're no longer in VMX root operation, block INIT, block and disable A20M.
     */
    pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxRootMode = false;
    Assert(!pVCpu->cpum.GstCtx.hwvirt.vmx.fInVmxNonRootMode);

    if (fSmmMonitorCtl & MSR_IA32_SMM_MONITOR_VMXOFF_UNBLOCK_SMI)
    { /** @todo NSTVMX: Unblock SMI. */ }
    /** @todo NSTVMX: Unblock and enable A20M. */
    /** @todo NSTVMX: Clear address-range monitoring. */

    pVCpu->cpum.GstCtx.hwvirt.vmx.enmDiag = kVmxVDiag_Vmxoff_Success;
    iemVmxVmSucceed(pVCpu);
    iemRegAddToRipAndClearRF(pVCpu, cbInstr);
#  if defined(VBOX_WITH_NESTED_HWVIRT_ONLY_IN_IEM) && defined(IN_RING3)
    return EMR3SetExecutionPolicy(pVCpu->CTX_SUFF(pVM)->pUVM, EMEXECPOLICY_IEM_ALL, false);
#  else
    return VINF_SUCCESS;
#  endif
# endif
}


/**
 * Implements 'VMLAUNCH'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmlaunch)
{
    return iemVmxVmlaunchVmresume(pVCpu, cbInstr, VMXINSTRID_VMLAUNCH, NULL /* pExitInfo */);
}


/**
 * Implements 'VMRESUME'.
 */
IEM_CIMPL_DEF_0(iemCImpl_vmresume)
{
    return iemVmxVmlaunchVmresume(pVCpu, cbInstr, VMXINSTRID_VMRESUME, NULL /* pExitInfo */);
}


/**
 * Implements 'VMPTRLD'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmptrld, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmptrld(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMPTRST'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmptrst, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmptrst(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMCLEAR'.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmclear, uint8_t, iEffSeg, RTGCPTR, GCPtrVmcs)
{
    return iemVmxVmclear(pVCpu, cbInstr, iEffSeg, GCPtrVmcs, NULL /* pExitInfo */);
}


/**
 * Implements 'VMWRITE' register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmwrite_reg, uint64_t, u64Val, uint64_t, u64FieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, UINT8_MAX /* iEffSeg */, IEMMODE_64BIT /* N/A */, u64Val, u64FieldEnc,
                         NULL /* pExitInfo */);
}


/**
 * Implements 'VMWRITE' memory.
 */
IEM_CIMPL_DEF_4(iemCImpl_vmwrite_mem, uint8_t, iEffSeg, IEMMODE, enmEffAddrMode, RTGCPTR, GCPtrVal, uint32_t, u64FieldEnc)
{
    return iemVmxVmwrite(pVCpu, cbInstr, iEffSeg, enmEffAddrMode, GCPtrVal, u64FieldEnc,  NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' 64-bit register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmread64_reg, uint64_t *, pu64Dst, uint64_t, u64FieldEnc)
{
    return iemVmxVmreadReg64(pVCpu, cbInstr, pu64Dst, u64FieldEnc, NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' 32-bit register.
 */
IEM_CIMPL_DEF_2(iemCImpl_vmread32_reg, uint32_t *, pu32Dst, uint32_t, u32FieldEnc)
{
    return iemVmxVmreadReg32(pVCpu, cbInstr, pu32Dst, u32FieldEnc, NULL /* pExitInfo */);
}


/**
 * Implements 'VMREAD' memory.
 */
IEM_CIMPL_DEF_4(iemCImpl_vmread_mem, uint8_t, iEffSeg, IEMMODE, enmEffAddrMode, RTGCPTR, GCPtrDst, uint32_t, u64FieldEnc)
{
    return iemVmxVmreadMem(pVCpu, cbInstr, iEffSeg, enmEffAddrMode, GCPtrDst, u64FieldEnc, NULL /* pExitInfo */);
}

#endif

