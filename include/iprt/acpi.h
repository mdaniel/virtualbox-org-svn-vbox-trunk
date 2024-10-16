/** @file
 * IPRT - Advanced Configuration and Power Interface (ACPI) Table generation API.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
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

#ifndef IPRT_INCLUDED_acpi_h
#define IPRT_INCLUDED_acpi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/vfs.h>

#include <iprt/formats/acpi-tables.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_acpi   RTAcpi - Advanced Configuration and Power Interface (ACPI) Table generation API.
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING3


/**
 * Regenerates the ACPI checksum for the given data.
 *
 * @returns The checksum for the given data.
 * @param   pvData              The data to check sum.
 * @param   cbData              Number of bytes to check sum.
 */
RTDECL(uint8_t) RTAcpiChecksumGenerate(const void *pvData, size_t cbData);


/**
 * Generates and writes the table header checksum for the given ACPI table.
 *
 * @returns nothing.
 * @param   pTbl                Pointer to the ACPI table to set the checksum for.
 * @param   cbTbl               Size of the table in bytes, including the ACPI table header.
 */
RTDECL(void) RTAcpiTblHdrChecksumGenerate(PACPITBLHDR pTbl, size_t cbTbl);


/**
 * Creates a new empty ACPI table.
 *
 * @returns IPRT status code.
 * @param   phAcpiTbl           Where to store the ACPI table handle on success.
 * @param   u32TblSig           The signature of the table to use.
 * @param   bRevision           The revision of the table.
 * @param   pszOemId            The OEM supplied string identifiying the OEM, maximum of 6 characters.
 * @param   pszOemTblId         The OEM supplied string identifiying the OEM table, maximum of 8 characters.
 * @param   u32OemRevision      The OEM supplied revision number.
 * @param   pszCreatorId        Vendor ID of the utility that created the table, maximum of 4 characters.
 * @param   u32CreatorRevision  Revision of the utility that created the table.
 */
RTDECL(int) RTAcpiTblCreate(PRTACPITBL phAcpiTbl, uint32_t u32TblSig, uint8_t bRevision, const char *pszOemId,
                            const char *pszOemTblId, uint32_t u32OemRevision, const char *pszCreatorId,
                            uint32_t u32CreatorRevision);


/**
 * Destroys the given ACPI table, freeing all resources.
 *
 * @parm   hAcpiTbl             The ACPI table handle to destroy.
 */
RTDECL(void) RTAcpiTblDestroy(RTACPITBL hAcpiTbl);


/**
 * Finalizes the given ACPI table, setting the header and generating checksums.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle to finalize.
 *
 * @note Nothing can be added to the table after this was called.
 */
RTDECL(int) RTAcpiTblFinalize(RTACPITBL hAcpiTbl);


/**
 * Returns the size of the given ACPI table.
 *
 * @returns Size of the given ACPI table in bytes, 0 on error.
 * @param   hAcpiTbl            The ACPI table handle.
 *
 * @note This can only be called after RTAcpiTblFinalize() was called successfully.
 */
RTDECL(uint32_t) RTAcpiTblGetSize(RTACPITBL hAcpiTbl);


/**
 * Dumps the given ACPI table to the given VFS I/O stream.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   hVfsIos             The VFS I/O stream handle to dump the table to.
 */
RTDECL(int) RTAcpiTblDumpToVfsIoStrm(RTACPITBL hAcpiTbl, RTVFSIOSTREAM hVfsIos);


/**
 * Dumps the given ACPI table to the given file.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszFilename         The file path to dump the table to.
 */
RTDECL(int) RTAcpiTblDumpToFile(RTACPITBL hAcpiTbl, const char *pszFilename);


/**
 * Starts a new DefScope object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             Name of the scope, can have a root (\) specifier optionally.
 */
RTDECL(int) RTAcpiTblScopeStart(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Finalizes the current scope object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblScopeFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new DefPackage object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   cElements           Number of element which will be inside the package,
 *                              only supports up to 255 elements, use DefVarPackage if more is required.
 */
RTDECL(int) RTAcpiTblPackageStart(RTACPITBL hAcpiTbl, uint8_t cElements);


/**
 * Finalizes the current DefPackage object, and return to the enclosing object's scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblPackageFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             Name of the device object, must be <= 4 characters long.
 */
RTDECL(int) RTAcpiTblDeviceStart(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   ...                 The format arguments.
 */
RTDECL(int) RTAcpiTblDeviceStartF(RTACPITBL hAcpiTbl, const char *pszNameFmt, ...) RT_IPRT_FORMAT_ATTR(2, 3);


/**
 * Starts a new device object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszNameFmt          The name of the device as a format string.
 * @param   va                  The format arguments.
 */
RTDECL(int) RTAcpiTblDeviceStartV(RTACPITBL hAcpiTbl, const char *pszNameFmt, va_list va) RT_IPRT_FORMAT_ATTR(2, 0);


/**
 * Finalizes the current scope object, nothing can be added to the scope afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblDeviceFinalize(RTACPITBL hAcpiTbl);


/**
 * Starts a new method object for the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The method name.
 * @param   fFlags              AML method flags, see RTACPI_METHOD_F_XXX.
 * @param   cArgs               Number of arguments this method takes.
 * @param   uSyncLvl            The sync level.
 */
RTDECL(int) RTAcpiTblMethodStart(RTACPITBL hAcpiTbl, const char *pszName, uint8_t cArgs, uint32_t fFlags, uint8_t uSyncLvl);


/** ACPI method is not serialized. */
#define RTACPI_METHOD_F_NOT_SERIALIZED 0
/** ACPI method call needs to be serialized in the ACPI interpreter. */
#define RTACPI_METHOD_F_SERIALIZED     RT_BIT_32(0)


/**
 * Finalizes the current method object, nothing can be added to the method afterwards.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 */
RTDECL(int) RTAcpiTblMethodFinalize(RTACPITBL hAcpiTbl);


/**
 * Appends a new DefName object (only the NameOp NameString part, DataRefObject is left for the caller
 * to append).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pszName             The name to append.
 */
RTDECL(int) RTAcpiTblNameAppend(RTACPITBL hAcpiTbl, const char *pszName);


/**
 * Appends a new String object.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   psz                 The string to append.
 */
RTDECL(int) RTAcpiTblStringAppend(RTACPITBL hAcpiTbl, const char *psz);


/**
 * Appends a new integer object (depending on the value ZeroOp, OneOp,
 * BytePrefix, WordPrefix, DWordPrefix or QWordPrefix is used).
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   u64                 The 64-bit value to append.
 */
RTDECL(int) RTAcpiTblIntegerAppend(RTACPITBL hAcpiTbl, uint64_t u64);


/**
 * Appends a new DefBuffer object under the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   pvBuf               The buffer data.
 * @param   cbBuf               Size of the buffer in bytes.
 */
RTDECL(int) RTAcpiTblBufferAppend(RTACPITBL hAcpiTbl, const void *pvBuf, size_t cbBuf);


/**
 * Appends the given resource as a DefBuffer under the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   hAcpiRes            The ACPI resource handle.
 */
RTDECL(int) RTAcpiTblResourceAppend(RTACPITBL hAcpiTbl, RTACPIRES hAcpiRes);


/**
 * List of statements.
 */
typedef enum RTACPISTMT
{
    /** Invalid statement. */
    kAcpiStmt_Invalid = 0,
    /** Return statement. */
    kAcpiStmt_Return,
    /** Breakpoint statement. */
    kAcpiStmt_Breakpoint,
    /** No operation statement. */
    kAcpiStmt_Nop,
    /** Break statement. */
    kAcpiStmt_Break,
    /** Continue statement. */
    kAcpiStmt_Continue
} RTACPISTMT;


/**
 * Appends the given simple statement to the given ACPI table in the current scope.
 *
 * @returns IPRT status code.
 * @param   hAcpiTbl            The ACPI table handle.
 * @param   enmStmt             The statement to add.
 */
RTDECL(int) RTAcpiTblStmtSimpleAppend(RTACPITBL hAcpiTbl, RTACPISTMT enmStmt);


/** @name ACPI resource builder related API.
 * @{ */

/**
 * Creates a new empty resource template.
 *
 * @returns IPRT status code.
 * @param   phAcpiRes           Where to store the handle to the ACPI resource on success.
 */
RTDECL(int) RTAcpiResourceCreate(PRTACPIRES phAcpiRes);


/**
 * Destroys the given ACPI resource, freeing all allocated resources.
 *
 * @param   hAcpiRes            The ACPI resource handle to destroy.
 */
RTDECL(void) RTAcpiResourceDestroy(RTACPIRES hAcpiRes);


/**
 * Resets the given ACPI resource handle to create a new empty template.
 *
 * @param   hAcpiRes            The ACPI resource handle.
 */
RTDECL(void) RTAcpiResourceReset(RTACPIRES hAcpiRes);


/**
 * Seals the given ACPI resource against further changes and adds any
 * missing data required to complete the resource buffer.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 *
 * @note After a call to this method completed successfully it is not possible
 *       to add new resources until RTAcpiResourceReset() was called.
 */
RTDECL(int) RTAcpiResourceSeal(RTACPIRES hAcpiRes);


/**
 * Queries the pointer to the buffer holding the encoded data.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   ppvBuf              Where to store the pointer to the buffer holding the encoded resource template on success.
 * @param   pcbBuf              Where to store the size of the encoded data in bytes on success.
 *
 * @note The ACPI resource must be successfully sealed with RTAcpiResourceSeal() for this function to succeed.
 *       Also the buffer pointer will only be valid until a call to any other RTAcpiResource* method.
 */
RTDECL(int) RTAcpiResourceQueryBuffer(RTACPIRES hAcpiRes, const void **ppvRes, size_t *pcbBuf);


/**
 * Adds a fixed memory range with the given start address and size to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   u32AddrBase         The base address to encode.
 * @param   cbRange             The range length in bytes to encode.
 * @param   fRw                 Flag whether this address range is read-write or read-only.
 */
RTDECL(int) RTAcpiResourceAdd32BitFixedMemoryRange(RTACPIRES hAcpiRes, uint32_t u32AddrBase, uint32_t cbRange,
                                                   bool fRw);


/**
 * Adds an extended interrupt descriptor with the given configuration to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fConsumer           Flag whether the entity this resource is assigned to consumes the interrupt (true) or produces it (false).
 * @param   fEdgeTriggered      Flag whether the interrupt is edged (true) or level (false) triggered.
 * @param   fActiveLow          Flag whether the interrupt polarity is active low (true) or active high (false).
 * @param   fShared             Flag whether the interrupt is shared between different entities (true) or exclusive to the assigned entity (false).
 * @param   fWakeCapable        Flag whether the interrupt can wake the system (true) or not (false).
 * @param   cIntrs              Number of interrupts following.
 * @param   pau32Intrs          Pointer to the array of interrupt numbers.
 */
RTDECL(int) RTAcpiResourceAddExtendedInterrupt(RTACPIRES hAcpiRes, bool fConsumer, bool fEdgeTriggered, bool fActiveLow, bool fShared,
                                               bool fWakeCapable, uint8_t cIntrs, uint32_t *pau32Intrs);


/** @name Generic address space flags.
 * @{ */
#define RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_SUB        RT_BIT_32(0)
#define RTACPI_RESOURCE_ADDR_RANGE_F_DECODE_TYPE_POS        0

#define RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_FIXED         RT_BIT_32(1)
#define RTACPI_RESOURCE_ADDR_RANGE_F_MIN_ADDR_CHANGEABLE    0

#define RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_FIXED         RT_BIT_32(2)
#define RTACPI_RESOURCE_ADDR_RANGE_F_MAX_ADDR_CHANGEABLE    0

#define RTACPI_RESOURCE_ADDR_RANGE_F_VALID_MASK             UINT32_C(0x00000007)
/** @} */

/**
 * Memory range cacheability
 */
typedef enum RTACPIRESMEMRANGECACHEABILITY
{
    /** Usual invalid value. */
    kAcpiResMemRangeCacheability_Invalid = 0,
    /** Memory range is non cacheable (like MMIO, etc.). */
    kAcpiResMemRangeCacheability_NonCacheable,
    /** Memory is cacheable. */
    kAcpiResMemRangeCacheability_Cacheable,
    /** Memory is cacheable and supports write comining. */
    kAcpiResMemRangeCacheability_CacheableWriteCombining,
    /** Memory is cacheable and supports prefetching. */
    kAcpiResMemRangeCacheability_CacheablePrefetchable,
    /** 32-bit blow up hack. */
    kAcpiResMemRangeCacheability_32BitHack = 0x7fffffff
} RTACPIRESMEMRANGECACHEABILITY;


/**
 * Memory attribute.
 */
typedef enum RTACPIRESMEMRANGETYPE
{
    /** Invalid memory range type. */
    kAcpiResMemType_Invalid = 0,
    /** Memory range is actual memory. */
    kAcpiResMemType_Memory,
    /** Memory range is reserved. */
    kAcpiResMemType_Reserved,
    /** Memory range is reserved to ACPI. */
    kAcpiResMemType_Acpi,
    /** Memory range is no volatile storage. */
    kAcpiResMemType_Nvs,
    /** 32-bit blow up hack. */
    kAcpiResMemType_32BitHack = 0x7fffffff
} RTACPIRESMEMRANGETYPE;


/**
 * Adds a quad word (64-bit) memory range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddQWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                              uint64_t u64Granularity, uint64_t u64Length);


/**
 * Adds a double word (32-bit) memory range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmCacheability     The cacheability of the memory range.
 * @param   enmType             Memory range type.
 * @param   fRw                 Flag whether the memory range is read/write (true) or readonly (false).
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u32AddrMin          The start address of the memory range.
 * @param   u32AddrMax          Last valid address of the range.
 * @param   u32OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u32Granularity      The access granularity of the range in bytes.
 * @param   u32Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddDWordMemoryRange(RTACPIRES hAcpiRes, RTACPIRESMEMRANGECACHEABILITY enmCacheability,
                                              RTACPIRESMEMRANGETYPE enmType, bool fRw, uint32_t fAddrSpace,
                                              uint32_t u32AddrMin, uint32_t u32AddrMax, uint32_t u32OffTrans,
                                              uint32_t u32Granularity, uint32_t u32Length);


/**
 * I/O range coverage.
 */
typedef enum RTACPIRESIORANGE
{
    /** Invalid range. */
    kAcpiResIoRange_Invalid = 0,
    /** Range covers only non ISA I/O ports. */
    kAcpiResIoRange_NonIsaOnly,
    /** Range covers only ISA I/O ports. */
    kAcpiResIoRange_IsaOnly,
    /** Range covers the whole I/O port range. */
    kAcpiResIoRange_Whole,
    /** 32-bit blow up hack. */
    kAcpiResIoRange_32BitHack = 0x7fffffff
} RTACPIRESIORANGE;


/**
 * I/O range type.
 */
typedef enum RTACPIRESIORANGETYPE
{
    /** Invalid value. */
    kAcpiResIoRangeType_Invalid = 0,
    /** Resource is I/O on the primary and secondary side of the bridge. */
    kAcpiResIoRangeType_Static,
    /** Resource is memory on the primary and I/O on the secondary side of the bridge,
     * primary side memory address for a given I/O port is calculated with
     * address = (((Port & 0xfffc) << 10) || (Port & 0xfff)) + AddrMin. */
    kAcpiResIoRangeType_Translation_Sparse,
    /** Resource is memory on the primary and I/O on the secondary side of the bridge,
     * primary side memory address for a given I/O port is calculated with
     * address = AddrMin + Port. */
    kAcpiResIoRangeType_Translation_Dense,
    /** 32-bit blowup hack. */
    kAcpiResIoRangeType_32BitHack = 0x7fffffff
} RTACPIRESIORANGETYPE;


/**
 * Adds a quad word (64-bit) I/O range to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   enmIoType           The I/O range type.
 * @param   enmIoRange          The I/O range coverage.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u64AddrMin          The start address of the memory range.
 * @param   u64AddrMax          Last valid address of the range.
 * @param   u64OffTrans         Translation offset being applied to the address (for a PCIe bridge or IOMMU for example).
 * @param   u64Granularity      The access granularity of the range in bytes.
 * @param   u64Length           Length of the memory range in bytes.
 */
RTDECL(int) RTAcpiResourceAddQWordIoRange(RTACPIRES hAcpiRes, RTACPIRESIORANGETYPE enmIoType, RTACPIRESIORANGE enmIoRange,
                                          uint32_t fAddrSpace, uint64_t u64AddrMin, uint64_t u64AddrMax, uint64_t u64OffTrans,
                                          uint64_t u64Granularity, uint64_t u64Length);


/**
 * Adds a word (16-bit) bus number to the given ACPI resource.
 *
 * @returns IPRT status code.
 * @param   hAcpiRes            The ACPI resource handle.
 * @param   fAddrSpace          Additional address space flags (combination of RTACPI_RESOURCE_ADDR_RANGE_F_XXX).
 * @param   u16BusMin           Starting bus number.
 * @param   u16BusMax           Last valid bus number.
 * @param   u16OffTrans         Translation offset being applied to the bus number.
 * @param   u16Granularity      The access granularity of the bus number.
 * @param   u16Length           Length of the bus range.
 */
RTDECL(int) RTAcpiResourceAddWordBusNumber(RTACPIRES hAcpiRes, uint32_t fAddrSpace, uint16_t u16BusMin, uint16_t u16BusMax,
                                           uint16_t u16OffTrans, uint16_t u16Granularity, uint16_t u16Length);

/** @} */

#endif /* IN_RING3 */

/** @} */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_acpi_h */

