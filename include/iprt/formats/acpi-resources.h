/* $Id$ */
/** @file
 * IPRT, ACPI (Advanced Configuration and Power Interface) Resource Data Types format.
 *
 * Spec taken from: https://uefi.org/specs/ACPI/6.5/06_Device_Configuration.html#resource-data-types-for-acpi (2024-09-18)
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

#ifndef IPRT_INCLUDED_formats_acpi_resources_h
#define IPRT_INCLUDED_formats_acpi_resources_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/cdefs.h>
#include <iprt/assertcompile.h>


/** @defgroup grp_rt_formats_acpi_resouces    Advanced Configuration and Power Interface (ACPI) Resource Data Types structures and definitions
 * @ingroup grp_rt_formats
 * @{
 */

/** End tag. */
#define ACPI_RSRCS_TAG_END                              0x79


/** @name Small Resource Data Type.
 * @{ */
/** The bit identifying a small resource data type. */
#define ACPI_RSRCS_SMALL_TYPE                           0
/** IRQ format descriptor. */
#define ACPI_RSRCS_ITEM_IRQ                             0x04
/** DMA format descriptor. */
#define ACPI_RSRCS_ITEM_DMA                             0x05
/** Start dependent functions descriptor. */
#define ACPI_RSRCS_ITEM_START_DEP_FN                    0x06
/** End dependent functions descriptor. */
#define ACPI_RSRCS_ITEM_END_DEP_FN                      0x07
/** I/O port descriptor. */
#define ACPI_RSRCS_ITEM_IO                              0x08
/** Fixed location I/O port descriptor. */
#define ACPI_RSRCS_ITEM_FIXED_LOCATION_IO               0x09
/** Fixed DMA descriptor. */
#define ACPI_RSRCS_ITEM_FIXED_DMA                       0x0a
/** End tag descriptor. */
#define ACPI_RSRCS_ITEM_END_TAG                         0x0f
/** @} */


/** @name Large Resource Data Type.
 * @{ */
/** The bit identifying a large resource data type. */
#define ACPI_RSRCS_LARGE_TYPE                           RT_BIT(7)
/** 24-bit memory range descriptor. */
#define ACPI_RSRCS_ITEM_24BIT_MEMORY_RANGE              0x01
/** Generic register descriptor. */
#define ACPI_RSRCS_ITEM_GENERIC_REGISTER                0x02
/** Vendor defined descriptor. */
#define ACPI_RSRCS_ITEM_VENDOR_DEFINED                  0x04
/** 32-bit memory range descriptor. */
#define ACPI_RSRCS_ITEM_32BIT_MEMORY_RANGE              0x05
/** 32-bit fixed memory range descriptor. */
#define ACPI_RSRCS_ITEM_32BIT_FIXED_MEMORY_RANGE        0x06
/** DWord address space descriptor. */
#define ACPI_RSRCS_ITEM_DWORD_ADDR_SPACE                0x07
/** Word address space descriptor. */
#define ACPI_RSRCS_ITEM_WORD_ADDR_SPACE                 0x08
/** Extended interrupt descriptor. */
#define ACPI_RSRCS_ITEM_EXTENDED_INTERRUPT              0x09
/** QWord address space descriptor. */
#define ACPI_RSRCS_ITEM_QWORD_ADDR_SPACE                0x0a
/** Extended Address space descriptor. */
#define ACPI_RSRCS_ITEM_EXT_ADDR_SPACE_RESOURCE         0x0b
/** GPIO connection descriptor. */
#define ACPI_RSRCS_ITEM_GPIO_CONNECTION                 0x0c
/** Pin function descriptor. */
#define ACPI_RSRCS_ITEM_PIN_FUNCTION                    0x0d
/** GenericSerialBus connection descriptor. */
#define ACPI_RSRCS_ITEM_GENERIC_SERIAL_BUS              0x0e
/** Pin configuration descriptor. */
#define ACPI_RSRCS_ITEM_PIN_CONFIGURATION               0x0f
/** Pin group descriptor. */
#define ACPI_RSRCS_ITEM_PIN_GROUP                       0x10
/** Pin group function descriptor. */
#define ACPI_RSRCS_ITEM_PIN_GROUP_FUNCTION              0x11
/** Pin group configuration descriptor. */
#define ACPI_RSRCS_ITEM_PIN_GROUP_CONFIGURATION         0x12
/** Clock input resource. */
#define ACPI_RSRCS_ITEM_CLOCK_INPUT_RESOURCE            0x13
/** @} */


/** @name Extended interrupt descriptor related definitions.
 * @{ */
/** The device consumes the resource. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_CONSUMER              RT_BIT(0)
/** The device produces the resource. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_PRODUCER              0

/** Interrupt is edge triggered. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_EDGE_TRIGGERED        RT_BIT(1)
/** Interrupt is level triggered. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_LEVEL_TRIGGERED       0

/** Interrupt polarity is active low. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_ACTIVE_LOW            RT_BIT(2)
/** Interrupt polarity is active high. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_ACTIVE_HIGH           0

/** Interrupt is shared. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_SHARED                RT_BIT(3)
/** Interrupt is exclusive. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_EXCLUSIVE             0

/** Interrupt is capable of waking the system. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_WAKE_CAP              RT_BIT(4)
/** Interrupt is not capable of waking the system. */
#define ACPI_RSRCS_EXT_INTR_VEC_F_NOT_WAKE_CAP          0
/** @} */


/** @name IRQ descriptor related definitions.
 * @{ */
/** Interrupt is edge triggered. */
#define ACPI_RSRCS_IRQ_F_EDGE_TRIGGERED                 RT_BIT(0)
/** Interrupt is level triggered. */
#define ACPI_RSRCS_IRQ_F_LEVEL_TRIGGERED                0

/** Interrupt polarity is active low. */
#define ACPI_RSRCS_IRQ_F_ACTIVE_LOW                     RT_BIT(3)
/** Interrupt polarity is active high. */
#define ACPI_RSRCS_IRQ_F_ACTIVE_HIGH                    0

/** Interrupt is shared. */
#define ACPI_RSRCS_IRQ_F_SHARED                         RT_BIT(4)
/** Interrupt is exclusive. */
#define ACPI_RSRCS_IRQ_F_EXCLUSIVE                      0

/** Interrupt is capable of waking the system. */
#define ACPI_RSRCS_IRQ_F_WAKE_CAP                       RT_BIT(5)
/** Interrupt is not capable of waking the system. */
#define ACPI_RSRCS_IRQ_F_NOT_WAKE_CAP                   0
/** @} */


/** @name Address space resource descriptors related definitions.
 * @{ */
/** @name Resource type.
 * @{ */
/** Memory range. */
#define ACPI_RSRCS_ADDR_SPACE_TYPE_MEMORY               0
/** I/O range. */
#define ACPI_RSRCS_ADDR_SPACE_TYPE_IO                   1
/** Bus number range. */
#define ACPI_RSRCS_ADDR_SPACE_TYPE_BUS_NUM_RANGE        2
/** @} */

/** @name General flags.
 * @{ */
/** The device consumes this resource. */
#define ACPI_RSRCS_ADDR_SPACE_F_CONSUMER                RT_BIT(0)
/** The device produces and consumes this resource. */
#define ACPI_RSRCS_ADDR_SPACE_F_PRODUCER                0

/** The bridge subtractively decodes this address. */
#define ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_SUB         RT_BIT(1)
/** The bridge positively decodes this address. */
#define ACPI_RSRCS_ADDR_SPACE_F_DECODE_TYPE_POS         0

/** The specified minimum address is fixed. */
#define ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_FIXED          RT_BIT(2)
/** The specified minimum address can be changed. */
#define ACPI_RSRCS_ADDR_SPACE_F_MIN_ADDR_CHANGEABLE     0

/** The specified maximum address is fixed. */
#define ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_FIXED          RT_BIT(3)
/** The specified maximum address can be changed. */
#define ACPI_RSRCS_ADDR_SPACE_F_MAX_ADDR_CHANGEABLE     0
/** @} */

/** @name Memory type specific flags.
 * @{ */
/** Memory range is read-write. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_RW                              RT_BIT(0)
/** Memory range is read-only. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_RO                              0

/** Cacheability mask. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_MASK                      (RT_BIT(1) | RT_BIT(2))
/** Memory range is non-cacheable. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_NON_CACHEABLE             0
/** Memory range is cacheable. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE                 2
/** Memory range is cacheable and supports write combining. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE_WR_COMB         4
/** Memory range is cacheable and prefetchable. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_CACHE_CACHEABLE_PREFETCHABLE    6

/** Attribute mask. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_MASK                       (RT_BIT(3) | RT_BIT(4))
/** Memory range is actual memory. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_MEMORY                     0x00
/** Memory range is reserved. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_RESERVED                   0x08
/** Memory range is ACPI reclaimable memory after the operating system read the ACPI tables. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_ACPI                       0x10
/** Memory range is in use by the system and must not be used by the operating system. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_ATTR_NVS                        0x18

/** Memory on the secondary side of the bridge, I/O on the primary side. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_TYPE_TRANSLATION                RT_BIT(5)
/** Memory on both secondary and primary side of the bridge. */
#define ACPI_RSRCS_ADDR_SPACE_MEM_F_TYPE_STATIC                     0
/** @} */

/** @name I/O type specific flags.
 * @{ */
/** Range bitmask. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_MASK                       (RT_BIT(0) | RT_BIT(1))
/** Memory window covers only non ISA ranges. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_NON_ISA_ONLY               1
/** Memory windows covers only ISA ranges. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_ISA_ONLY                   2
/** Memory window covers the entire range. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_RANGE_WHOLE                      3

/** I/O on the secondary side of the bridge, memory on the primary side. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_TYPE_TRANSLATION                 RT_BIT(4)
/** I/O on both secondary and primary side of the bridge. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_TYPE_STATIC                      0

/** address = (((port & 0xFFFc) << 10) || (port & 0xFFF)) + _TRA. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_TRANSLATION_SPARSE               RT_BIT(5)
/** address = port + _TRA. */
#define ACPI_RSRCS_ADDR_SPACE_IO_F_TRANSLATION_DENSE                0
/** @} */

/** @} */

/** @} */

#endif /* !IPRT_INCLUDED_formats_acpi_resources_h */

