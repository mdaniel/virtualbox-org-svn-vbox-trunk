/* $Id$ */
/** @file
 * IPRT - ASN.1, BOOLEAN Type, Collection Type Template.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
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

#define RTASN1TMPL_DECL         RTDECL

#define RTASN1TMPL_TYPE         RTASN1SEQOFBOOLEANS
#define RTASN1TMPL_EXT_NAME     RTAsn1SeqOfBooleans
#define RTASN1TMPL_INT_NAME     rtAsn1SeqOfBooleans
RTASN1TMPL_SEQ_OF(RTASN1BOOLEAN, RTAsn1Boolean);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME


#define RTASN1TMPL_TYPE         RTASN1SETOFBOOLEANS
#define RTASN1TMPL_EXT_NAME     RTAsn1SetOfBooleans
#define RTASN1TMPL_INT_NAME     rtAsn1SetOfBooleans
RTASN1TMPL_SET_OF(RTASN1BOOLEAN, RTAsn1Boolean);
#undef RTASN1TMPL_TYPE
#undef RTASN1TMPL_EXT_NAME
#undef RTASN1TMPL_INT_NAME

