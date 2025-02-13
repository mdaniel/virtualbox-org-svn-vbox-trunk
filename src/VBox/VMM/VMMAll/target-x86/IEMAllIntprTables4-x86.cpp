/* $Id$ */
/** @file
 * IEM - Instruction Decoding and Emulation, x86 target, Interpreter Tables - VEX.
 */

/*
 * Copyright (C) 2011-2024 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define IEM_WITH_VEX_TABLES
#include "IEMAllIntprTables-x86.h"
#ifdef IEM_WITH_VEX
# include "IEMAllInstVexMap1-x86.cpp.h"
# include "IEMAllInstVexMap2-x86.cpp.h"
# include "IEMAllInstVexMap3-x86.cpp.h"
#endif

