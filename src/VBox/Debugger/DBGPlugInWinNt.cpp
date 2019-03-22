/* $Id$ */
/** @file
 * DBGPlugInWindows - Debugger and Guest OS Digger Plugin For Windows NT.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DBGF /// @todo add new log group.
#include "DBGPlugIns.h"
#include <VBox/vmm/dbgf.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/formats/pecoff.h>
#include <iprt/formats/mz.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** @name Internal WinNT structures
 * @{ */
/**
 * PsLoadedModuleList entry for 32-bit NT aka LDR_DATA_TABLE_ENTRY.
 * Tested with XP.
 */
typedef struct NTMTE32
{
    struct
    {
        uint32_t    Flink;
        uint32_t    Blink;
    }               InLoadOrderLinks,
                    InMemoryOrderModuleList,
                    InInitializationOrderModuleList;
    uint32_t        DllBase;
    uint32_t        EntryPoint;
    /** @note This field is not a size in NT 3.1. It's NULL for images loaded by the
     *        boot loader, for other images it looks like some kind of pointer.  */
    uint32_t        SizeOfImage;
    struct
    {
        uint16_t    Length;
        uint16_t    MaximumLength;
        uint32_t    Buffer;
    }               FullDllName,
                    BaseDllName;
    uint32_t        Flags;
    uint16_t        LoadCount;
    uint16_t        TlsIndex;
    /* ... there is more ... */
} NTMTE32;
typedef NTMTE32 *PNTMTE32;

/**
 * PsLoadedModuleList entry for 64-bit NT aka LDR_DATA_TABLE_ENTRY.
 */
typedef struct NTMTE64
{
    struct
    {
        uint64_t    Flink;
        uint64_t    Blink;
    }               InLoadOrderLinks,                  /**< 0x00 */
                    InMemoryOrderModuleList,           /**< 0x10 */
                    InInitializationOrderModuleList;   /**< 0x20 */
    uint64_t        DllBase;                           /**< 0x30 */
    uint64_t        EntryPoint;                        /**< 0x38 */
    uint32_t        SizeOfImage;                       /**< 0x40 */
    uint32_t        Alignment;                         /**< 0x44 */
    struct
    {
        uint16_t    Length;                            /**< 0x48,0x58 */
        uint16_t    MaximumLength;                     /**< 0x4a,0x5a */
        uint32_t    Alignment;                         /**< 0x4c,0x5c */
        uint64_t    Buffer;                            /**< 0x50,0x60 */
    }               FullDllName,                       /**< 0x48 */
                    BaseDllName;                       /**< 0x58 */
    uint32_t        Flags;                             /**< 0x68 */
    uint16_t        LoadCount;                         /**< 0x6c */
    uint16_t        TlsIndex;                          /**< 0x6e */
    /* ... there is more ... */
} NTMTE64;
typedef NTMTE64 *PNTMTE64;

/** MTE union. */
typedef union NTMTE
{
    NTMTE32         vX_32;
    NTMTE64         vX_64;
} NTMTE;
typedef NTMTE *PNTMTE;


/**
 * The essential bits of the KUSER_SHARED_DATA structure.
 */
typedef struct NTKUSERSHAREDDATA
{
    uint32_t        TickCountLowDeprecated;
    uint32_t        TickCountMultiplier;
    struct
    {
        uint32_t    LowPart;
        int32_t     High1Time;
        int32_t     High2Time;

    }               InterruptTime,
                    SystemTime,
                    TimeZoneBias;
    uint16_t        ImageNumberLow;
    uint16_t        ImageNumberHigh;
    RTUTF16         NtSystemRoot[260];
    uint32_t        MaxStackTraceDepth;
    uint32_t        CryptoExponent;
    uint32_t        TimeZoneId;
    uint32_t        LargePageMinimum;
    uint32_t        Reserved2[7];
    uint32_t        NtProductType;
    uint8_t         ProductTypeIsValid;
    uint8_t         abPadding[3];
    uint32_t        NtMajorVersion;
    uint32_t        NtMinorVersion;
    /* uint8_t         ProcessorFeatures[64];
    ...
    */
} NTKUSERSHAREDDATA;
typedef NTKUSERSHAREDDATA *PNTKUSERSHAREDDATA;

/** KI_USER_SHARED_DATA for i386 */
#define NTKUSERSHAREDDATA_WINNT32   UINT32_C(0xffdf0000)
/** KI_USER_SHARED_DATA for AMD64 */
#define NTKUSERSHAREDDATA_WINNT64   UINT64_C(0xfffff78000000000)

/** NTKUSERSHAREDDATA::NtProductType */
typedef enum NTPRODUCTTYPE
{
    kNtProductType_Invalid = 0,
    kNtProductType_WinNt = 1,
    kNtProductType_LanManNt,
    kNtProductType_Server
} NTPRODUCTTYPE;


/** NT image header union. */
typedef union NTHDRSU
{
    IMAGE_NT_HEADERS32  vX_32;
    IMAGE_NT_HEADERS64  vX_64;
} NTHDRS;
/** Pointer to NT image header union. */
typedef NTHDRS *PNTHDRS;
/** Pointer to const NT image header union. */
typedef NTHDRS const *PCNTHDRS;

/** @} */



typedef enum DBGDIGGERWINNTVER
{
    DBGDIGGERWINNTVER_UNKNOWN,
    DBGDIGGERWINNTVER_3_1,
    DBGDIGGERWINNTVER_3_5,
    DBGDIGGERWINNTVER_4_0,
    DBGDIGGERWINNTVER_5_0,
    DBGDIGGERWINNTVER_5_1,
    DBGDIGGERWINNTVER_6_0
} DBGDIGGERWINNTVER;

/**
 * WinNT guest OS digger instance data.
 */
typedef struct DBGDIGGERWINNT
{
    /** Whether the information is valid or not.
     * (For fending off illegal interface method calls.) */
    bool                fValid;
    /** 32-bit (true) or 64-bit (false) */
    bool                f32Bit;
    /** Set if NT 3.1 was detected.
     * This implies both Misc.VirtualSize and NTMTE32::SizeOfImage are zero. */
    bool                fNt31;

    /** The NT version. */
    DBGDIGGERWINNTVER   enmVer;
    /** NTKUSERSHAREDDATA::NtProductType */
    NTPRODUCTTYPE       NtProductType;
    /** NTKUSERSHAREDDATA::NtMajorVersion */
    uint32_t            NtMajorVersion;
    /** NTKUSERSHAREDDATA::NtMinorVersion */
    uint32_t            NtMinorVersion;

    /** The address of the ntoskrnl.exe image. */
    DBGFADDRESS         KernelAddr;
    /** The address of the ntoskrnl.exe module table entry. */
    DBGFADDRESS         KernelMteAddr;
    /** The address of PsLoadedModuleList. */
    DBGFADDRESS         PsLoadedModuleListAddr;
} DBGDIGGERWINNT;
/** Pointer to the linux guest OS digger instance data. */
typedef DBGDIGGERWINNT *PDBGDIGGERWINNT;


/**
 * The WinNT digger's loader reader instance data.
 */
typedef struct DBGDIGGERWINNTRDR
{
    /** The VM handle (referenced). */
    PUVM                pUVM;
    /** The image base. */
    DBGFADDRESS         ImageAddr;
    /** The image size. */
    uint32_t            cbImage;
    /** The file offset of the SizeOfImage field in the optional header if it
     *  needs patching, otherwise set to UINT32_MAX. */
    uint32_t            offSizeOfImage;
    /** The correct image size. */
    uint32_t            cbCorrectImageSize;
    /** Number of entries in the aMappings table. */
    uint32_t            cMappings;
    /** Mapping hint. */
    uint32_t            iHint;
    /** Mapping file offset to memory offsets, ordered by file offset. */
    struct
    {
        /** The file offset. */
        uint32_t        offFile;
        /** The size of this mapping. */
        uint32_t        cbMem;
        /** The offset to the memory from the start of the image.  */
        uint32_t        offMem;
    } aMappings[1];
} DBGDIGGERWINNTRDR;
/** Pointer a WinNT loader reader instance data. */
typedef DBGDIGGERWINNTRDR *PDBGDIGGERWINNTRDR;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Validates a 32-bit Windows NT kernel address */
#define WINNT32_VALID_ADDRESS(Addr)         ((Addr) >         UINT32_C(0x80000000) && (Addr) <         UINT32_C(0xfffff000))
/** Validates a 64-bit Windows NT kernel address */
 #define WINNT64_VALID_ADDRESS(Addr)         ((Addr) > UINT64_C(0xffff800000000000) && (Addr) < UINT64_C(0xfffffffffffff000))
/** Validates a kernel address. */
#define WINNT_VALID_ADDRESS(pThis, Addr)    ((pThis)->f32Bit ? WINNT32_VALID_ADDRESS(Addr) : WINNT64_VALID_ADDRESS(Addr))
/** Versioned and bitness wrapper. */
#define WINNT_UNION(pThis, pUnion, Member)  ((pThis)->f32Bit ? (pUnion)->vX_32. Member : (pUnion)->vX_64. Member )

/** The length (in chars) of the kernel file name (no path). */
#define WINNT_KERNEL_BASE_NAME_LEN          12

/** WindowsNT on little endian ASCII systems. */
#define DIG_WINNT_MOD_TAG                   UINT64_C(0x54696e646f774e54)


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static DECLCALLBACK(int)  dbgDiggerWinNtInit(PUVM pUVM, void *pvData);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Kernel names. */
static const RTUTF16 g_wszKernelNames[][WINNT_KERNEL_BASE_NAME_LEN + 1] =
{
    { 'n', 't', 'o', 's', 'k', 'r', 'n', 'l', '.', 'e', 'x', 'e' }
};



/**
 * Process a PE image found in guest memory.
 *
 * @param   pThis           The instance data.
 * @param   pUVM            The user mode VM handle.
 * @param   pszName         The image name.
 * @param   pImageAddr      The image address.
 * @param   cbImage         The size of the image.
 * @param   pbBuf           Scratch buffer containing the first
 *                          RT_MIN(cbBuf, cbImage) bytes of the image.
 * @param   cbBuf           The scratch buffer size.
 */
static void dbgDiggerWinNtProcessImage(PDBGDIGGERWINNT pThis, PUVM pUVM, const char *pszName,
                                       PCDBGFADDRESS pImageAddr, uint32_t cbImage)
{
    LogFlow(("DigWinNt: %RGp %#x %s\n", pImageAddr->FlatPtr, cbImage, pszName));

    /*
     * Do some basic validation first.
     */
    if (   (cbImage < sizeof(IMAGE_NT_HEADERS64) && !pThis->fNt31)
        || cbImage >= _1M * 256)
    {
        Log(("DigWinNt: %s: Bad image size: %#x\n", pszName, cbImage));
        return;
    }

    /*
     * Use the common in-memory module reader to create a debug module.
     */
    RTERRINFOSTATIC ErrInfo;
    RTDBGMOD        hDbgMod = NIL_RTDBGMOD;
    int rc = DBGFR3ModInMem(pUVM, pImageAddr, pThis->fNt31 ? DBGFMODINMEM_F_PE_NT31 : 0, pszName,
                            pThis->f32Bit ? RTLDRARCH_X86_32 : RTLDRARCH_AMD64, cbImage,
                            &hDbgMod, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
    {
        /*
         * Tag the module.
         */
        rc = RTDbgModSetTag(hDbgMod, DIG_WINNT_MOD_TAG);
        AssertRC(rc);

        /*
         * Link the module.
         */
        RTDBGAS hAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
        if (hAs != NIL_RTDBGAS)
            rc = RTDbgAsModuleLink(hAs, hDbgMod, pImageAddr->FlatPtr, RTDBGASLINK_FLAGS_REPLACE /*fFlags*/);
        else
            rc = VERR_INTERNAL_ERROR;
        RTDbgModRelease(hDbgMod);
        RTDbgAsRelease(hAs);
    }
    else if (RTErrInfoIsSet(&ErrInfo.Core))
        Log(("DigWinNt: %s: DBGFR3ModInMem failed: %Rrc - %s\n", pszName, rc, ErrInfo.Core.pszMsg));
    else
        Log(("DigWinNt: %s: DBGFR3ModInMem failed: %Rrc\n", pszName, rc));
}


/**
 * @copydoc DBGFOSREG::pfnQueryInterface
 */
static DECLCALLBACK(void *) dbgDiggerWinNtQueryInterface(PUVM pUVM, void *pvData, DBGFOSINTERFACE enmIf)
{
    RT_NOREF3(pUVM, pvData, enmIf);
    return NULL;
}


/**
 * @copydoc DBGFOSREG::pfnQueryVersion
 */
static DECLCALLBACK(int)  dbgDiggerWinNtQueryVersion(PUVM pUVM, void *pvData, char *pszVersion, size_t cchVersion)
{
    RT_NOREF1(pUVM);
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(pThis->fValid);
    const char *pszNtProductType;
    switch (pThis->NtProductType)
    {
        case kNtProductType_WinNt:      pszNtProductType = "-WinNT";        break;
        case kNtProductType_LanManNt:   pszNtProductType = "-LanManNT";     break;
        case kNtProductType_Server:     pszNtProductType = "-Server";       break;
        default:                        pszNtProductType = "";              break;
    }
    RTStrPrintf(pszVersion, cchVersion, "%u.%u-%s%s", pThis->NtMajorVersion, pThis->NtMinorVersion,
                pThis->f32Bit ? "x86" : "AMD64", pszNtProductType);
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnTerm
 */
static DECLCALLBACK(void)  dbgDiggerWinNtTerm(PUVM pUVM, void *pvData)
{
    RT_NOREF1(pUVM);
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(pThis->fValid);

    /*
     * As long as we're using our private LDR reader implementation,
     * we must unlink and ditch the modules we created.
     */
    RTDBGAS hDbgAs = DBGFR3AsResolveAndRetain(pUVM, DBGF_AS_KERNEL);
    if (hDbgAs != NIL_RTDBGAS)
    {
        uint32_t iMod = RTDbgAsModuleCount(hDbgAs);
        while (iMod-- > 0)
        {
            RTDBGMOD hMod = RTDbgAsModuleByIndex(hDbgAs, iMod);
            if (hMod != NIL_RTDBGMOD)
            {
                if (RTDbgModGetTag(hMod) == DIG_WINNT_MOD_TAG)
                {
                    int rc = RTDbgAsModuleUnlink(hDbgAs, hMod);
                    AssertRC(rc);
                }
                RTDbgModRelease(hMod);
            }
        }
        RTDbgAsRelease(hDbgAs);
    }

    pThis->fValid = false;
}


/**
 * @copydoc DBGFOSREG::pfnRefresh
 */
static DECLCALLBACK(int)  dbgDiggerWinNtRefresh(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    NOREF(pThis);
    Assert(pThis->fValid);

    /*
     * For now we'll flush and reload everything.
     */
    dbgDiggerWinNtTerm(pUVM, pvData);

    return dbgDiggerWinNtInit(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnInit
 */
static DECLCALLBACK(int)  dbgDiggerWinNtInit(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    Assert(!pThis->fValid);

    union
    {
        uint8_t             au8[0x2000];
        RTUTF16             wsz[0x2000/2];
        NTKUSERSHAREDDATA   UserSharedData;
    }               u;
    DBGFADDRESS     Addr;
    int             rc;

    /*
     * Figure the NT version.
     */
    DBGFR3AddrFromFlat(pUVM, &Addr, pThis->f32Bit ? NTKUSERSHAREDDATA_WINNT32 : NTKUSERSHAREDDATA_WINNT64);
    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &u, PAGE_SIZE);
    if (RT_SUCCESS(rc))
    {
        pThis->NtProductType  = u.UserSharedData.ProductTypeIsValid && u.UserSharedData.NtProductType <= kNtProductType_Server
                              ? (NTPRODUCTTYPE)u.UserSharedData.NtProductType
                              : kNtProductType_Invalid;
        pThis->NtMajorVersion = u.UserSharedData.NtMajorVersion;
        pThis->NtMinorVersion = u.UserSharedData.NtMinorVersion;
    }
    else if (pThis->fNt31)
    {
        pThis->NtProductType  = kNtProductType_WinNt;
        pThis->NtMajorVersion = 3;
        pThis->NtMinorVersion = 1;
    }
    else
    {
        Log(("DigWinNt: Error reading KUSER_SHARED_DATA: %Rrc\n", rc));
        return rc;
    }

    /*
     * Dig out the module chain.
     */
    DBGFADDRESS AddrPrev = pThis->PsLoadedModuleListAddr;
    Addr                 = pThis->KernelMteAddr;
    do
    {
        /* Read the validate the MTE. */
        NTMTE Mte;
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &Addr, &Mte, pThis->f32Bit ? sizeof(Mte.vX_32) : sizeof(Mte.vX_64));
        if (RT_FAILURE(rc))
            break;
        if (WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Blink) != AddrPrev.FlatPtr)
        {
            Log(("DigWinNt: Bad Mte At %RGv - backpointer\n", Addr.FlatPtr));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Flink)) )
        {
            Log(("DigWinNt: Bad Mte at %RGv - forward pointer\n", Addr.FlatPtr));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer)))
        {
            Log(("DigWinNt: Bad Mte at %RGv - BaseDllName=%llx\n", Addr.FlatPtr, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer)));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, FullDllName.Buffer)))
        {
            Log(("DigWinNt: Bad Mte at %RGv - FullDllName=%llx\n", Addr.FlatPtr, WINNT_UNION(pThis, &Mte, FullDllName.Buffer)));
            break;
        }
        if (!WINNT_VALID_ADDRESS(pThis, WINNT_UNION(pThis, &Mte, DllBase)))
        {
            Log(("DigWinNt: Bad Mte at %RGv - DllBase=%llx\n", Addr.FlatPtr, WINNT_UNION(pThis, &Mte, DllBase) ));
            break;
        }

        uint32_t const cbImageMte = !pThis->fNt31 ? WINNT_UNION(pThis, &Mte, SizeOfImage) : 0;
        if (   !pThis->fNt31
            && (   cbImageMte > _256M
                || WINNT_UNION(pThis, &Mte, EntryPoint) - WINNT_UNION(pThis, &Mte, DllBase) > cbImageMte) )
        {
            Log(("DigWinNt: Bad Mte at %RGv - EntryPoint=%llx SizeOfImage=%x DllBase=%llx\n",
                 Addr.FlatPtr, WINNT_UNION(pThis, &Mte, EntryPoint), cbImageMte, WINNT_UNION(pThis, &Mte, DllBase)));
            break;
        }

        /* Read the full name. */
        DBGFADDRESS AddrName;
        DBGFR3AddrFromFlat(pUVM, &AddrName, WINNT_UNION(pThis, &Mte, FullDllName.Buffer));
        uint16_t    cbName = WINNT_UNION(pThis, &Mte, FullDllName.Length);
        if (cbName < sizeof(u))
            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &AddrName, &u, cbName);
        else
            rc = VERR_OUT_OF_RANGE;
        if (RT_FAILURE(rc))
        {
            DBGFR3AddrFromFlat(pUVM, &AddrName, WINNT_UNION(pThis, &Mte, BaseDllName.Buffer));
            cbName = WINNT_UNION(pThis, &Mte, BaseDllName.Length);
            if (cbName < sizeof(u))
                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &AddrName, &u, cbName);
            else
                rc = VERR_OUT_OF_RANGE;
        }
        if (RT_SUCCESS(rc))
        {
            u.wsz[cbName / 2] = '\0';

            char *pszName;
            rc = RTUtf16ToUtf8(u.wsz, &pszName);
            if (RT_SUCCESS(rc))
            {
                /* Read the start of the PE image and pass it along to a worker. */
                DBGFADDRESS ImageAddr;
                DBGFR3AddrFromFlat(pUVM, &ImageAddr, WINNT_UNION(pThis, &Mte, DllBase));
                dbgDiggerWinNtProcessImage(pThis, pUVM, pszName, &ImageAddr, cbImageMte);
                RTStrFree(pszName);
            }
        }

        /* next */
        AddrPrev = Addr;
        DBGFR3AddrFromFlat(pUVM, &Addr, WINNT_UNION(pThis, &Mte, InLoadOrderLinks.Flink));
    } while (   Addr.FlatPtr != pThis->KernelMteAddr.FlatPtr
             && Addr.FlatPtr != pThis->PsLoadedModuleListAddr.FlatPtr);

    pThis->fValid = true;
    return VINF_SUCCESS;
}


/**
 * @copydoc DBGFOSREG::pfnProbe
 */
static DECLCALLBACK(bool)  dbgDiggerWinNtProbe(PUVM pUVM, void *pvData)
{
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    DBGFADDRESS     Addr;
    union
    {
        uint8_t             au8[8192];
        uint16_t            au16[8192/2];
        uint32_t            au32[8192/4];
        IMAGE_DOS_HEADER    MzHdr;
        RTUTF16             wsz[8192/2];
        X86DESC64GATE       a32Gates[X86_XCPT_PF + 1];
        X86DESC64GATE       a64Gates[X86_XCPT_PF + 1];
    } u;

    union
    {
        NTMTE32 v32;
        NTMTE64 v64;
    } uMte, uMte2, uMte3;

    /*
     * NT only runs in protected or long mode.
     */
    CPUMMODE const enmMode = DBGFR3CpuGetMode(pUVM, 0 /*idCpu*/);
    if (enmMode != CPUMMODE_PROTECTED && enmMode != CPUMMODE_LONG)
        return false;
    bool const      f64Bit = enmMode == CPUMMODE_LONG;
    uint64_t const  uStart = f64Bit ? UINT64_C(0xffff080000000000) : UINT32_C(0x80001000);
    uint64_t const  uEnd   = f64Bit ? UINT64_C(0xffffffffffff0000) : UINT32_C(0xffff0000);

    /*
     * To approximately locate the kernel we examine the IDTR handlers.
     *
     * The exception/trap/fault handlers are all in NT kernel image, we pick
     * KiPageFault here.
     */
    uint64_t uIdtrBase = 0;
    uint16_t uIdtrLimit = 0;
    int rc = DBGFR3RegCpuQueryXdtr(pUVM, 0, DBGFREG_IDTR, &uIdtrBase, &uIdtrLimit);
    AssertRCReturn(rc, false);

    const uint16_t cbMinIdtr = (X86_XCPT_PF + 1) * (f64Bit ? sizeof(X86DESC64GATE) : sizeof(X86DESCGATE));
    if (uIdtrLimit < cbMinIdtr)
        return false;

    rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, uIdtrBase), &u, cbMinIdtr);
    if (RT_FAILURE(rc))
        return false;

    uint64_t uKrnlStart  = uStart;
    uint64_t uKrnlEnd    = uEnd;
    if (f64Bit)
    {
        uint64_t uHandler = u.a64Gates[X86_XCPT_PF].u16OffsetLow
                          | ((uint32_t)u.a64Gates[X86_XCPT_PF].u16OffsetHigh << 16)
                          | ((uint64_t)u.a64Gates[X86_XCPT_PF].u32OffsetTop  << 32);
        if (uHandler < uStart || uHandler > uEnd)
            return false;
        uKrnlStart = (uHandler & ~(uint64_t)_4M) - _512M;
        uKrnlEnd   = (uHandler + (uint64_t)_4M) & ~(uint64_t)_4M;
    }
    else
    {
        uint32_t uHandler = u.a32Gates[X86_XCPT_PF].u16OffsetLow
                          | ((uint32_t)u.a64Gates[X86_XCPT_PF].u16OffsetHigh << 16);
        if (uHandler < uStart || uHandler > uEnd)
            return false;
        uKrnlStart = (uHandler & ~(uint64_t)_4M) - _64M;
        uKrnlEnd   = (uHandler + (uint64_t)_4M) & ~(uint64_t)_4M;
    }

    /*
     * Look for the PAGELK section name that seems to be a part of all kernels.
     * Then try find the module table entry for it.  Since it's the first entry
     * in the PsLoadedModuleList we can easily validate the list head and report
     * success.
     *
     * Note! We ASSUME the section name is 8 byte aligned.
     */
    DBGFADDRESS KernelAddr;
    for (DBGFR3AddrFromFlat(pUVM, &KernelAddr, uKrnlStart);
         KernelAddr.FlatPtr < uKrnlEnd;
         KernelAddr.FlatPtr += PAGE_SIZE)
    {
        bool fNt31 = false;
        DBGFADDRESS const RetryAddress = KernelAddr;
        rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, uEnd - KernelAddr.FlatPtr,
                           8, "PAGELK\0", sizeof("PAGELK\0"), &KernelAddr);
        if (   rc == VERR_DBGF_MEM_NOT_FOUND
            && enmMode != CPUMMODE_LONG)
        {
            /* NT3.1 didn't have a PAGELK section, so look for _TEXT instead.  The
               following VirtualSize is zero, so check for that too. */
            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &RetryAddress, uEnd - RetryAddress.FlatPtr,
                               8, "_TEXT\0\0\0\0\0\0", sizeof("_TEXT\0\0\0\0\0\0"), &KernelAddr);
            fNt31 = true;
        }
        if (RT_FAILURE(rc))
            break;
        DBGFR3AddrSub(&KernelAddr, KernelAddr.FlatPtr & PAGE_OFFSET_MASK);

        /* MZ + PE header. */
        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, &KernelAddr, &u, sizeof(u));
        if (    RT_SUCCESS(rc)
            &&  u.MzHdr.e_magic == IMAGE_DOS_SIGNATURE
            &&  !(u.MzHdr.e_lfanew & 0x7)
            &&  u.MzHdr.e_lfanew >= 0x080
            &&  u.MzHdr.e_lfanew <= 0x400) /* W8 is at 0x288*/
        {
            if (enmMode != CPUMMODE_LONG)
            {
                IMAGE_NT_HEADERS32 const *pHdrs = (IMAGE_NT_HEADERS32 const *)&u.au8[u.MzHdr.e_lfanew];
                if (    pHdrs->Signature                            == IMAGE_NT_SIGNATURE
                    &&  pHdrs->FileHeader.Machine                   == IMAGE_FILE_MACHINE_I386
                    &&  pHdrs->FileHeader.SizeOfOptionalHeader      == sizeof(pHdrs->OptionalHeader)
                    &&  pHdrs->FileHeader.NumberOfSections          >= 10 /* the kernel has lots */
                    &&  (pHdrs->FileHeader.Characteristics & (IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL)) == IMAGE_FILE_EXECUTABLE_IMAGE
                    &&  pHdrs->OptionalHeader.Magic                 == IMAGE_NT_OPTIONAL_HDR32_MAGIC
                    &&  pHdrs->OptionalHeader.NumberOfRvaAndSizes   == IMAGE_NUMBEROF_DIRECTORY_ENTRIES
                    )
                {
                    /* Find the MTE. */
                    RT_ZERO(uMte);
                    uMte.v32.DllBase     = KernelAddr.FlatPtr;
                    uMte.v32.EntryPoint  = KernelAddr.FlatPtr + pHdrs->OptionalHeader.AddressOfEntryPoint;
                    uMte.v32.SizeOfImage = !fNt31 ? pHdrs->OptionalHeader.SizeOfImage : 0; /* NT 3.1 didn't set the size. */
                    DBGFADDRESS HitAddr;
                    rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &KernelAddr, uEnd - KernelAddr.FlatPtr,
                                       4 /*align*/, &uMte.v32.DllBase, 3 * sizeof(uint32_t), &HitAddr);
                    while (RT_SUCCESS(rc))
                    {
                        /* check the name. */
                        DBGFADDRESS MteAddr = HitAddr;
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrSub(&MteAddr, RT_OFFSETOF(NTMTE32, DllBase)),
                                           &uMte2.v32, sizeof(uMte2.v32));
                        if (    RT_SUCCESS(rc)
                            &&  uMte2.v32.DllBase     == uMte.v32.DllBase
                            &&  uMte2.v32.EntryPoint  == uMte.v32.EntryPoint
                            &&  uMte2.v32.SizeOfImage == uMte.v32.SizeOfImage
                            &&  WINNT32_VALID_ADDRESS(uMte2.v32.InLoadOrderLinks.Flink)
                            &&  WINNT32_VALID_ADDRESS(uMte2.v32.BaseDllName.Buffer)
                            &&  WINNT32_VALID_ADDRESS(uMte2.v32.FullDllName.Buffer)
                            &&  uMte2.v32.BaseDllName.Length <= 128
                            &&  uMte2.v32.FullDllName.Length <= 260
                           )
                        {
                            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, uMte2.v32.BaseDllName.Buffer),
                                               u.wsz, uMte2.v32.BaseDllName.Length);
                            u.wsz[uMte2.v32.BaseDllName.Length / 2] = '\0';
                            if (    RT_SUCCESS(rc)
                                &&  (   !RTUtf16ICmp(u.wsz, g_wszKernelNames[0])
                                  /* || !RTUtf16ICmp(u.wsz, g_wszKernelNames[1]) */
                                    )
                               )
                            {
                                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/,
                                                   DBGFR3AddrFromFlat(pUVM, &Addr, uMte2.v32.InLoadOrderLinks.Blink),
                                                   &uMte3.v32, RT_SIZEOFMEMB(NTMTE32, InLoadOrderLinks));
                                if (   RT_SUCCESS(rc)
                                    && uMte3.v32.InLoadOrderLinks.Flink == MteAddr.FlatPtr
                                    && WINNT32_VALID_ADDRESS(uMte3.v32.InLoadOrderLinks.Blink) )
                                {
                                    Log(("DigWinNt: MteAddr=%RGv KernelAddr=%RGv SizeOfImage=%x &PsLoadedModuleList=%RGv (32-bit)\n",
                                         MteAddr.FlatPtr, KernelAddr.FlatPtr, uMte2.v32.SizeOfImage, Addr.FlatPtr));
                                    pThis->KernelAddr               = KernelAddr;
                                    pThis->KernelMteAddr            = MteAddr;
                                    pThis->PsLoadedModuleListAddr   = Addr;
                                    pThis->f32Bit                   = true;
                                    pThis->fNt31                    = fNt31;
                                    return true;
                                }
                            }
                            else if (RT_SUCCESS(rc))
                            {
                                Log2(("DigWinNt: Wrong module: MteAddr=%RGv ImageAddr=%RGv SizeOfImage=%#x '%ls'\n",
                                      MteAddr.FlatPtr, KernelAddr.FlatPtr, uMte2.v32.SizeOfImage, u.wsz));
                                break; /* Not NT kernel */
                            }
                        }

                        /* next */
                        DBGFR3AddrAdd(&HitAddr, 4);
                        if (HitAddr.FlatPtr < uEnd)
                            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &HitAddr, uEnd - HitAddr.FlatPtr,
                                               4 /*align*/, &uMte.v32.DllBase, 3 * sizeof(uint32_t), &HitAddr);
                        else
                            rc = VERR_DBGF_MEM_NOT_FOUND;
                    }
                }
            }
            else
            {
                IMAGE_NT_HEADERS64 const *pHdrs = (IMAGE_NT_HEADERS64 const *)&u.au8[u.MzHdr.e_lfanew];
                if (    pHdrs->Signature                            == IMAGE_NT_SIGNATURE
                    &&  pHdrs->FileHeader.Machine                   == IMAGE_FILE_MACHINE_AMD64
                    &&  pHdrs->FileHeader.SizeOfOptionalHeader      == sizeof(pHdrs->OptionalHeader)
                    &&  pHdrs->FileHeader.NumberOfSections          >= 10 /* the kernel has lots */
                    &&      (pHdrs->FileHeader.Characteristics & (IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL))
                         == IMAGE_FILE_EXECUTABLE_IMAGE
                    &&  pHdrs->OptionalHeader.Magic                 == IMAGE_NT_OPTIONAL_HDR64_MAGIC
                    &&  pHdrs->OptionalHeader.NumberOfRvaAndSizes   == IMAGE_NUMBEROF_DIRECTORY_ENTRIES
                    )
                {
                    /* Find the MTE. */
                    RT_ZERO(uMte.v64);
                    uMte.v64.DllBase     = KernelAddr.FlatPtr;
                    uMte.v64.EntryPoint  = KernelAddr.FlatPtr + pHdrs->OptionalHeader.AddressOfEntryPoint;
                    uMte.v64.SizeOfImage = pHdrs->OptionalHeader.SizeOfImage;
                    DBGFADDRESS ScanAddr;
                    DBGFADDRESS HitAddr;
                    rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &ScanAddr, uStart),
                                       uEnd - uStart, 8 /*align*/, &uMte.v64.DllBase, 5 * sizeof(uint32_t), &HitAddr);
                    while (RT_SUCCESS(rc))
                    {
                        /* Read the start of the MTE and check some basic members. */
                        DBGFADDRESS MteAddr = HitAddr;
                        rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrSub(&MteAddr, RT_OFFSETOF(NTMTE64, DllBase)),
                                           &uMte2.v64, sizeof(uMte2.v64));
                        if (    RT_SUCCESS(rc)
                            &&  uMte2.v64.DllBase     == uMte.v64.DllBase
                            &&  uMte2.v64.EntryPoint  == uMte.v64.EntryPoint
                            &&  uMte2.v64.SizeOfImage == uMte.v64.SizeOfImage
                            &&  WINNT64_VALID_ADDRESS(uMte2.v64.InLoadOrderLinks.Flink)
                            &&  WINNT64_VALID_ADDRESS(uMte2.v64.BaseDllName.Buffer)
                            &&  WINNT64_VALID_ADDRESS(uMte2.v64.FullDllName.Buffer)
                            &&  uMte2.v64.BaseDllName.Length <= 128
                            &&  uMte2.v64.FullDllName.Length <= 260
                            )
                        {
                            /* Try read the base name and compare with known NT kernel names. */
                            rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/, DBGFR3AddrFromFlat(pUVM, &Addr, uMte2.v64.BaseDllName.Buffer),
                                               u.wsz, uMte2.v64.BaseDllName.Length);
                            u.wsz[uMte2.v64.BaseDllName.Length / 2] = '\0';
                            if (    RT_SUCCESS(rc)
                                &&  (   !RTUtf16ICmp(u.wsz, g_wszKernelNames[0])
                                  /* || !RTUtf16ICmp(u.wsz, g_wszKernelNames[1]) */
                                    )
                               )
                            {
                                /* Read the link entry of the previous entry in the list and check that its
                                   forward pointer points at the MTE we've found. */
                                rc = DBGFR3MemRead(pUVM, 0 /*idCpu*/,
                                                   DBGFR3AddrFromFlat(pUVM, &Addr, uMte2.v64.InLoadOrderLinks.Blink),
                                                   &uMte3.v64, RT_SIZEOFMEMB(NTMTE64, InLoadOrderLinks));
                                if (   RT_SUCCESS(rc)
                                    && uMte3.v64.InLoadOrderLinks.Flink == MteAddr.FlatPtr
                                    && WINNT64_VALID_ADDRESS(uMte3.v64.InLoadOrderLinks.Blink) )
                                {
                                    Log(("DigWinNt: MteAddr=%RGv KernelAddr=%RGv SizeOfImage=%x &PsLoadedModuleList=%RGv (32-bit)\n",
                                         MteAddr.FlatPtr, KernelAddr.FlatPtr, uMte2.v64.SizeOfImage, Addr.FlatPtr));
                                    pThis->KernelAddr               = KernelAddr;
                                    pThis->KernelMteAddr            = MteAddr;
                                    pThis->PsLoadedModuleListAddr   = Addr;
                                    pThis->f32Bit                   = false;
                                    pThis->fNt31                    = false;
                                    return true;
                                }
                            }
                            else if (RT_SUCCESS(rc))
                            {
                                Log2(("DigWinNt: Wrong module: MteAddr=%RGv ImageAddr=%RGv SizeOfImage=%#x '%ls'\n",
                                      MteAddr.FlatPtr, KernelAddr.FlatPtr, uMte2.v64.SizeOfImage, u.wsz));
                                break; /* Not NT kernel */
                            }
                        }

                        /* next */
                        DBGFR3AddrAdd(&HitAddr, 8);
                        if (HitAddr.FlatPtr < uEnd)
                            rc = DBGFR3MemScan(pUVM, 0 /*idCpu*/, &HitAddr, uEnd - HitAddr.FlatPtr,
                                               8 /*align*/, &uMte.v64.DllBase, 3 * sizeof(uint32_t), &HitAddr);
                        else
                            rc = VERR_DBGF_MEM_NOT_FOUND;
                    }
                }
            }
        }
    }
    return false;
}


/**
 * @copydoc DBGFOSREG::pfnDestruct
 */
static DECLCALLBACK(void)  dbgDiggerWinNtDestruct(PUVM pUVM, void *pvData)
{
    RT_NOREF2(pUVM, pvData);
}


/**
 * @copydoc DBGFOSREG::pfnConstruct
 */
static DECLCALLBACK(int)  dbgDiggerWinNtConstruct(PUVM pUVM, void *pvData)
{
    RT_NOREF1(pUVM);
    PDBGDIGGERWINNT pThis = (PDBGDIGGERWINNT)pvData;
    pThis->fValid = false;
    pThis->f32Bit = false;
    pThis->enmVer = DBGDIGGERWINNTVER_UNKNOWN;
    return VINF_SUCCESS;
}


const DBGFOSREG g_DBGDiggerWinNt =
{
    /* .u32Magic = */           DBGFOSREG_MAGIC,
    /* .fFlags = */             0,
    /* .cbData = */             sizeof(DBGDIGGERWINNT),
    /* .szName = */             "WinNT",
    /* .pfnConstruct = */       dbgDiggerWinNtConstruct,
    /* .pfnDestruct = */        dbgDiggerWinNtDestruct,
    /* .pfnProbe = */           dbgDiggerWinNtProbe,
    /* .pfnInit = */            dbgDiggerWinNtInit,
    /* .pfnRefresh = */         dbgDiggerWinNtRefresh,
    /* .pfnTerm = */            dbgDiggerWinNtTerm,
    /* .pfnQueryVersion = */    dbgDiggerWinNtQueryVersion,
    /* .pfnQueryInterface = */  dbgDiggerWinNtQueryInterface,
    /* .u32EndMagic = */        DBGFOSREG_MAGIC
};

