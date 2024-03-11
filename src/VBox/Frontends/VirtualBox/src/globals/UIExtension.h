/* $Id$ */
/** @file
 * VBox Qt GUI - UIExtension namespace declaration.
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

#ifndef FEQT_INCLUDED_SRC_globals_UIExtension_h
#define FEQT_INCLUDED_SRC_globals_UIExtension_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UILibraryDefs.h"

/* Forward declarations: */
class QString;
class QWidget;

/** Namespace with common extension pack stuff. */
namespace UIExtension
{
    /** Initiates the extension pack installation process.
      * @param  strFilePath      Brings the extension pack file path.
      * @param  strDigest        Brings the extension pack file digest.
      * @param  pParent          Brings the parent dialog reference.
      * @param  pstrExtPackName  Brings the extension pack name. */
    void SHARED_LIBRARY_STUFF install(const QString &strFilePath,
                                      const QString &strDigest,
                                      QWidget *pParent,
                                      QString *pstrExtPackName);

    /** Returns true if it can find at least one usable ext. pack. Else returns false. */
    bool SHARED_LIBRARY_STUFF isExtentionPackInstalled();
}

#endif /* !FEQT_INCLUDED_SRC_globals_UIExtension_h */
