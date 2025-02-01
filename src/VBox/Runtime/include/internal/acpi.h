/* $Id$ */
/** @file
 * IPRT - Internal RTAcpi header.
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

#ifndef IPRT_INCLUDED_INTERNAL_acpi_h
#define IPRT_INCLUDED_INTERNAL_acpi_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/acpi.h>

RT_C_DECLS_BEGIN

/**
 * Worker for decompiling AML bytecode to the ASL source language.
 *
 * @returns IPRT status code.
 * @param   hVfsIosOut          The VFS I/O stream handle to output the result to.
 * @param   hVfsIosIn           The VFS I/O stream handle to read the ACPI table from.
 * @param   pErrInfo            Where to return additional error information.
 */
DECLHIDDEN(int) rtAcpiTblConvertFromAmlToAsl(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo);


/**
 * Worker for decompiling ASL bytecode to the AML source language.
 *
 * @returns IPRT status code.
 * @param   hVfsIosOut          The VFS I/O stream handle to output the result to.
 * @param   hVfsIosIn           The VFS I/O stream handle to read the ACPI table from.
 * @param   pErrInfo            Where to return additional error information.
 */
DECLHIDDEN(int) rtAcpiTblConvertFromAslToAml(RTVFSIOSTREAM hVfsIosOut, RTVFSIOSTREAM hVfsIosIn, PRTERRINFO pErrInfo);


RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_INTERNAL_acpi_h */

