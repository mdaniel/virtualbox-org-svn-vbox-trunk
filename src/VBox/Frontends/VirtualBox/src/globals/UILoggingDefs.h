/* $Id$ */
/** @file
 * VBox Qt GUI - Global logging definitions.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef FEQT_INCLUDED_SRC_globals_UILoggingDefs_h
#define FEQT_INCLUDED_SRC_globals_UILoggingDefs_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Define GUI logging group,
 * this define should go *before* VBox/log.h include! */
#ifndef LOG_GROUP
# define LOG_GROUP LOG_GROUP_GUI
#endif

/* Other VBox includes: */
#include <VBox/log.h>

#endif /* !FEQT_INCLUDED_SRC_globals_UILoggingDefs_h */
