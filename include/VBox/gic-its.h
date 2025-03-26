/** @file
 * ARMv8 Generic Interrupt Controller (GIC) Interrupt Translation Service (ITS) definitions.
 */

/*
 * Copyright (C) 2025 Oracle and/or its affiliates.
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

#ifndef VBOX_INCLUDED_gic_its_h
#define VBOX_INCLUDED_gic_its_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/armv8.h>

/** Size of the ITS register frame. */
#define GITS_REG_FRAME_SIZE                                     _64K

/** GITS_CTLR: Control register - RW. */
#define GITS_CTRL_REG_CTLR_OFF                                  0x0000
/** GITS_CTLR: Enabled. */
#define GITS_BF_CTRL_REG_CTLR_ENABLED_SHIFT                     0
#define GITS_BF_CTRL_REG_CTLR_ENABLED_MASK                      UINT32_C(0x00000001)
/** GITS_CTLR: ImDe - Implementation Defined. */
#define GITS_BF_CTRL_REG_CTLR_IM_DE_SHIFT                       1
#define GITS_BF_CTRL_REG_CTLR_IM_DE_MASK                        UINT32_C(0x00000002)
/** GITS_CTLR: Reserved (bits 3:2). */
#define GITS_BF_CTRL_REG_CTLR_RSVD_3_2_SHIFT                    2
#define GITS_BF_CTRL_REG_CTLR_RSVD_3_2_MASK                     UINT32_C(0x0000000c)
/** GITS_CTLR: ITS_Number (0 for GICv3). */
#define GITS_BF_CTRL_REG_CTLR_ITS_NUMBER_SHIFT                  4
#define GITS_BF_CTRL_REG_CTLR_ITS_NUMBER_MASK                   UINT32_C(0x000000f0)
/** GITS_CTLR: UMSIirq - Unmapped MSI reporting interrupt enable. */
#define GITS_BF_CTRL_REG_CTLR_UMSI_IRQ_SHIFT                    8
#define GITS_BF_CTRL_REG_CTLR_UMSI_IRQ_MASK                     UINT32_C(0x00000100)
/** GITS_CTLR: Reserved (bits 30:9). */
#define GITS_BF_CTRL_REG_CTLR_RSVD_30_9_SHIFT                   9
#define GITS_BF_CTRL_REG_CTLR_RSVD_30_9_MASK                    UINT32_C(0x7ffffe00)
/** GITS_CTLR: Quiescent. */
#define GITS_BF_CTRL_REG_CTLR_QUIESCENT_SHIFT                   31
#define GITS_BF_CTRL_REG_CTLR_QUIESCENT_MASK                    UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_CTLR_, UINT32_C(0), UINT32_MAX,
                            (ENABLED, IM_DE, RSVD_3_2, ITS_NUMBER, UMSI_IRQ, RSVD_30_9, QUIESCENT));

/** GITS_IIDR: Implementer and revision register - RO. */
#define GITS_CTRL_REG_IIDR_OFF                                  0x0004
/** GITS_IIDR: Implementer. */
#define GITS_BF_CTRL_REG_IIDR_IMPLEMENTER_SHIFT                 0
#define GITS_BF_CTRL_REG_IIDR_IMPLEMENTER_MASK                  UINT32_C(0x00000fff)
/** GITS_IIDR: Revision. */
#define GITS_BF_CTRL_REG_IIDR_REVISION_SHIFT                    12
#define GITS_BF_CTRL_REG_IIDR_REVISION_MASK                     UINT32_C(0x0000f000)
/** GITS_IIDR: Variant. */
#define GITS_BF_CTRL_REG_IIDR_VARIANT_SHIFT                     16
#define GITS_BF_CTRL_REG_IIDR_VARIANT_MASK                      UINT32_C(0x000f0000)
/** GITS_IIDR: Reserved (bits 23:20). */
#define GITS_BF_CTRL_REG_IIDR_RSVD_23_20_SHIFT                  20
#define GITS_BF_CTRL_REG_IIDR_RSVD_23_20_MASK                   UINT32_C(0x00f00000)
/** GITS_IIDR: Product ID. */
#define GITS_BF_CTRL_REG_IIDR_PRODUCT_ID_SHIFT                  24
#define GITS_BF_CTRL_REG_IIDR_PRODUCT_ID_MASK                   UINT32_C(0xff000000)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_IIDR_, UINT32_C(0), UINT32_MAX,
                            (IMPLEMENTER, REVISION, VARIANT, RSVD_23_20, PRODUCT_ID));

/** GITS_TYPER: Feature register - RO. */
#define GITS_CTRL_REG_TYPER_OFF                                 0x0008
/** GITS_TYPER: Physical - Physical LPI support. */
#define GITS_BF_CTRL_REG_TYPER_PHYSICAL_SHIFT                   0
#define GITS_BF_CTRL_REG_TYPER_PHYSICAL_MASK                    UINT64_C(0x0000000000000001)
/** GITS_TYPER: Virtual - Virtual LPI support. */
#define GITS_BF_CTRL_REG_TYPER_VIRTUAL_SHIFT                    1
#define GITS_BF_CTRL_REG_TYPER_VIRTUAL_MASK                     UINT64_C(0x0000000000000002)
/** GITS_TYPER: CCT - Cumulative Collections Table. */
#define GITS_BF_CTRL_REG_TYPER_CCT_SHIFT                        2
#define GITS_BF_CTRL_REG_TYPER_CCT_MASK                         UINT64_C(0x0000000000000004)
/** GITS_TYPER: Implementation Defined. */
#define GITS_BF_CTRL_REG_TYPER_IM_DE_SHIFT                      3
#define GITS_BF_CTRL_REG_TYPER_IM_DE_MASK                       UINT64_C(0x0000000000000008)
/** GITS_TYPER: ITT_entry_size - Size of translation table entry. */
#define GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE_SHIFT             4
#define GITS_BF_CTRL_REG_TYPER_ITT_ENTRY_SIZE_MASK              UINT64_C(0x00000000000000f0)
/** GITS_TYPER: ID_bits - Number of event ID bits implemented (minus one). */
#define GITS_BF_CTRL_REG_TYPER_ID_BITS_SHIFT                    8
#define GITS_BF_CTRL_REG_TYPER_ID_BITS_MASK                     UINT64_C(0x0000000000001f00)
/** GITS_TYPER: Devbits - Number of device ID bits implemented (minus one). */
#define GITS_BF_CTRL_REG_TYPER_DEV_BITS_SHIFT                   13
#define GITS_BF_CTRL_REG_TYPER_DEV_BITS_MASK                    UINT64_C(0x000000000003e000)
/** GITS_TYPER: SEIS - SEI support for virtual CPUs. */
#define GITS_BF_CTRL_REG_TYPER_SEIS_SHIFT                       18
#define GITS_BF_CTRL_REG_TYPER_SEIS_MASK                        UINT64_C(0x0000000000040000)
/** GITS_TYPER: PTA - Physical target address format. */
#define GITS_BF_CTRL_REG_TYPER_PTA_SHIFT                        19
#define GITS_BF_CTRL_REG_TYPER_PTA_MASK                         UINT64_C(0x0000000000080000)
/** GITS_TYPER: Reserved (bits 23:20). */
#define GITS_BF_CTRL_REG_TYPER_RSVD_23_20_SHIFT                 20
#define GITS_BF_CTRL_REG_TYPER_RSVD_23_20_MASK                  UINT64_C(0x0000000000f00000)
/** GITS_TYPER: HCC - Hardware collection count. */
#define GITS_BF_CTRL_REG_TYPER_HCC_SHIFT                        24
#define GITS_BF_CTRL_REG_TYPER_HCC_MASK                         UINT64_C(0x00000000ff000000)
/** GITS_TYPER: CIDbits - Number of collection ID bits (minus one). */
#define GITS_BF_CTRL_REG_TYPER_CID_BITS_SHIFT                   32
#define GITS_BF_CTRL_REG_TYPER_CID_BITS_MASK                    UINT64_C(0x0000000f00000000)
/** GITS_TYPER: CIL - Collection ID limit. */
#define GITS_BF_CTRL_REG_TYPER_CIL_SHIFT                        36
#define GITS_BF_CTRL_REG_TYPER_CIL_MASK                         UINT64_C(0x0000001000000000)
/** GITS_TYPER: VMOVP - Form of VMOVP command. */
#define GITS_BF_CTRL_REG_TYPER_VMOVP_SHIFT                      37
#define GITS_BF_CTRL_REG_TYPER_VMOVP_MASK                       UINT64_C(0x0000002000000000)
/** GITS_TYPER: MPAM - Memory partitioning and monitoring support. */
#define GITS_BF_CTRL_REG_TYPER_MPAM_SHIFT                       38
#define GITS_BF_CTRL_REG_TYPER_MPAM_MASK                        UINT64_C(0x0000004000000000)
/** GITS_TYPER: VSGI - Direct injection of virtual SGI support. */
#define GITS_BF_CTRL_REG_TYPER_VSGI_SHIFT                       39
#define GITS_BF_CTRL_REG_TYPER_VSGI_MASK                        UINT64_C(0x0000008000000000)
/** GITS_TYPER: VMAPP - VMAPP command support. */
#define GITS_BF_CTRL_REG_TYPER_VMAPP_SHIFT                      40
#define GITS_BF_CTRL_REG_TYPER_VMAPP_MASK                       UINT64_C(0x0000010000000000)
/** GITS_TYPER: SVPET - Shared VPE table configuration. */
#define GITS_BF_CTRL_REG_TYPER_SVPET_SHIFT                      41
#define GITS_BF_CTRL_REG_TYPER_SVPET_MASK                       UINT64_C(0x0000060000000000)
/** GITS_TYPER: nID - Individual doorbell interrupt support. */
#define GITS_BF_CTRL_REG_TYPER_NID_SHIFT                        43
#define GITS_BF_CTRL_REG_TYPER_NID_MASK                         UINT64_C(0x0000080000000000)
/** GITS_TYPER: UMSI - Support for reporting receipts of unmapped MSI. */
#define GITS_BF_CTRL_REG_TYPER_UMSI_SHIFT                       44
#define GITS_BF_CTRL_REG_TYPER_UMSI_MASK                        UINT64_C(0x0000100000000000)
/** GITS_TYPER: UMSIirq - Support for generating interrupt on receiving unmapped MSI. */
#define GITS_BF_CTRL_REG_TYPER_UMSI_IRQ_SHIFT                   45
#define GITS_BF_CTRL_REG_TYPER_UMSI_IRQ_MASK                    UINT64_C(0x0000200000000000)
/** GITS_TYPER: INV - Invalidate ITS cache on disable. */
#define GITS_BF_CTRL_REG_TYPER_INV_SHIFT                        46
#define GITS_BF_CTRL_REG_TYPER_INV_MASK                         UINT64_C(0x0000400000000000)
/** GITS_TYPER: Reserved (bits 63:47). */
#define GITS_BF_CTRL_REG_TYPER_RSVD_63_47_SHIFT                 47
#define GITS_BF_CTRL_REG_TYPER_RSVD_63_47_MASK                  UINT64_C(0xffff800000000000)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_TYPER_, UINT64_C(0), UINT64_MAX,
                            (PHYSICAL, VIRTUAL, CCT, IM_DE, ITT_ENTRY_SIZE, ID_BITS, DEV_BITS, SEIS, PTA, RSVD_23_20, HCC,
                             CID_BITS, CIL, VMOVP, MPAM, VSGI, VMAPP, SVPET, NID, UMSI, UMSI_IRQ, INV, RSVD_63_47));

/** GITS_MPAMIDR: Memory partitioning ID sizes. */
#define GITS_CTRL_REG_MPAMIDR_OFF                               0x0010
/** GITS_MPAMIDR: PARTIDmax - Maximum PARTID value supported. */
#define GITS_BF_CTRL_REG_MPAMIDR_PARTID_MAX_SHIFT               0
#define GITS_BF_CTRL_REG_MPAMIDR_PARTID_MAX_MASK                UINT32_C(0x0000ffff)
/** GITS_MPAMIDR: PMGmax - Maximum PMG value supported. */
#define GITS_BF_CTRL_REG_MPAMIDR_PMG_MAX_SHIFT                  16
#define GITS_BF_CTRL_REG_MPAMIDR_PMG_MAX_MASK                   UINT32_C(0x00ff0000)
/** GITS_MPAMIDR: Reserved (bits 24:31). */
#define GITS_BF_CTRL_REG_MPAMIDR_RSVD_31_24_SHIFT               24
#define GITS_BF_CTRL_REG_MPAMIDR_RSVD_31_24_MASK                UINT32_C(0xff000000)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_MPAMIDR_, UINT32_C(0), UINT32_MAX, (PARTID_MAX, PMG_MAX, RSVD_31_24));

/** GITS_PARTID: PARTID and PMG values register. */
#define GITS_CTRL_REG_PARTIDR_OFF                               0x0014
/** GITS_PARTID: PARTID - PARTID when ITS accesses memory. */
#define GITS_BF_CTRL_REG_PARTIDR_PARTID_SHIFT                   0
#define GITS_BF_CTRL_REG_PARTIDR_PARTID_MASK                    UINT32_C(0x0000ffff)
/** GITS_PARTID: PMG - PMG value when ITS accesses memory. */
#define GITS_BF_CTRL_REG_PARTIDR_PMG_SHIFT                      16
#define GITS_BF_CTRL_REG_PARTIDR_PMG_MASK                       UINT32_C(0x00ff0000)
/** GITS_PARTID: Reserved (bits 24:31). */
#define GITS_BF_CTRL_REG_PARTIDR_RSVD_31_24_SHIFT               24
#define GITS_BF_CTRL_REG_PARTIDR_RSVD_31_24_MASK                UINT32_C(0xff000000)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_PARTIDR_, UINT32_C(0), UINT32_MAX, (PARTID, PMG, RSVD_31_24));

#define GITS_CTRL_REG_MPIDR_OFF                                 0x0018
#define GITS_CTRL_REG_STATUSR_OFF                               0x0040
#define GITS_CTRL_REG_UMSIR_OFF                                 0x0048
#define GITS_CTRL_REG_CBASER_OFF                                0x0080
#define GITS_CTRL_REG_CWRITER_OFF                               0x0088
#define GITS_CTRL_REG_CREADR_OFF                                0x0090

#define GITS_CTRL_REG_BASER_OFF_FIRST                           0x0100
#define GITS_CTRL_REG_BASER_OFF_LAST                            0x0138
#define GITS_CTRL_REG_BASER_RANGE_SIZE                          (GITS_CTRL_REG_BASER_OFF_LAST + sizeof(uint64_t) - GITS_CTRL_REG_BASER_OFF_FIRST)

/** GITS_PIDR2: ITS Peripheral ID2 register - RO. */
#define GITS_CTRL_REG_PIDR2_OFF                                 0xffe8
/** GITS_PIDR2: JEDEC - JEP code. */
#define GITS_BF_CTRL_REG_PIDR2_JEDEC_SHIFT                      0
#define GITS_BF_CTRL_REG_PIDR2_JEDEC_MASK                       UINT32_C(0x00000007)
/** GITS_PIDR2: DES_1 - JEP106 identification code (bits 6:4).  */
#define GITS_BF_CTRL_REG_PIDR2_DES_1_SHIFT                      3
#define GITS_BF_CTRL_REG_PIDR2_DES_1_MASK                       UINT32_C(0x00000008)
/** GITS_PIDR2: Architecture revision . */
#define GITS_BF_CTRL_REG_PIDR2_ARCHREV_SHIFT                    4
#define GITS_BF_CTRL_REG_PIDR2_ARCHREV_MASK                     UINT32_C(0x000000f0)
/** GITS_PIDR2: Reserved (bits 31:8). */
#define GITS_BF_CTRL_REG_PIDR2_RSVD_31_8_SHIFT                  8
#define GITS_BF_CTRL_REG_PIDR2_RSVD_31_8_MASK                   UINT32_C(0xffffff00)
RT_BF_ASSERT_COMPILE_CHECKS(GITS_BF_CTRL_REG_PIDR2_, UINT32_C(0), UINT32_MAX,
                            (JEDEC, DES_1, ARCHREV, RSVD_31_8));

/** GITS_PIDR2: GICv1 architecture revision. */
#define GITS_CTRL_REG_PIDR2_ARCHREV_GICV1                       0x1
/** GITS_PIDR2: GICv2 architecture revision. */
#define GITS_CTRL_REG_PIDR2_ARCHREV_GICV2                       0x2
/** GITS_PIDR2: GICv3 architecture revision. */
#define GITS_CTRL_REG_PIDR2_ARCHREV_GICV3                       0x3
/** GITS_PIDR2: GICv4 architecture revision. */
#define GITS_CTRL_REG_PIDR2_ARCHREV_GICV4                       0x4

/** GITS_TRANSLATER register. */
#define GITS_TRANSLATION_REG_TRANSLATER                         0x0040

/**
 * Memory shareability attributes.
 * In accordance to the ARM GIC spec.
 */
typedef enum GITSATTRSHARE
{
    GITSATTRSHARE_NON_SHAREABLE = 0,
    GITSATTRSHARE_INNER_SHAREABLE,
    GITSATTRSHARE_OUTER_SHAREABLE,
    GITSATTRSHARE_RSVD
} GITSATTRSHARE;

/**
 * Memory cacheability attribute.
 * In accordance to the ARM GIC spec.
 */
typedef enum GITSATTRMEM
{
    GITSATTRMEM_DEFAULT = 0,
    GITSATTRMEM_NOCACHE,
    GITSATTRMEM_CACHE_RD_ALLOC_WT,
    GITSATTRMEM_CACHE_RD_ALLOC_WB,
    GITSATTRMEM_CACHE_WR_ALLOC_WT,
    GITSATTRMEM_CACHE_WR_ALLOC_WB,
    GITSATTRMEM_CACHE_RW_ALLOC_WT,
    GITSATTRMEM_CACHE_RW_ALLOC_WB
} GITSMEMATTR;


/**
 * The ITS entry type.
 * In accordance to the ARM GIC spec.
 */
typedef enum GITSITSTYPE
{
    GITSITSTYPE_UNIMPLEMENTED = 0,
    GITSITSTYPE_DEVICES,
    GITSITSTYPE_VPES,
    GITSITSTYPE_INTR_COLLECTIONS
} GITSITSTYPE;

#endif /* !VBOX_INCLUDED_gic_its_h */

