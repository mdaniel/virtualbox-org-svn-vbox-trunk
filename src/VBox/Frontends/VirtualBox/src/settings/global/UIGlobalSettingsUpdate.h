/* $Id$ */
/** @file
 * VBox Qt GUI - UIGlobalSettingsUpdate class declaration.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h
#define FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "UISettingsPage.h"
#include "UIUpdateDefs.h"

/* Forward declarations: */
class UIUpdateSettingsEditor;
struct UIDataSettingsGlobalUpdate;
typedef UISettingsCache<UIDataSettingsGlobalUpdate> UISettingsCacheGlobalUpdate;

/** Global settings: Update page. */
class SHARED_LIBRARY_STUFF UIGlobalSettingsUpdate : public UISettingsPageGlobal
{
    Q_OBJECT;

public:

    /** Constructs settings page. */
    UIGlobalSettingsUpdate();
    /** Destructs settings page. */
    virtual ~UIGlobalSettingsUpdate() RT_OVERRIDE;

protected:

    /** Returns whether the page content was changed. */
    virtual bool changed() const RT_OVERRIDE;

    /** Loads settings from external object(s) packed inside @a data to cache.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    /** Loads data from cache to corresponding widgets.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void getFromCache() RT_OVERRIDE;

    /** Saves data from corresponding widgets to cache.
      * @note  This task WILL be performed in the GUI thread only, all widget interactions here! */
    virtual void putToCache() RT_OVERRIDE;
    /** Saves settings from cache to external object(s) packed inside @a data.
      * @note  This task WILL be performed in other than the GUI thread, no widget interactions! */
    virtual void saveFromCacheTo(QVariant &data) RT_OVERRIDE;

private slots:

    /** Handles translation event. */
    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

private:

    /** Prepares all. */
    void prepare();
    /** Prepares widgets. */
    void prepareWidgets();
    /** Cleanups all. */
    void cleanup();

    /** Saves existing data from cache. */
    bool saveData();

    /** Holds the page data cache instance. */
    UISettingsCacheGlobalUpdate *m_pCache;

    /** @name Widgets
     * @{ */
        /** Holds the 'update settings' editor instance. */
        UIUpdateSettingsEditor *m_pEditorUpdateSettings;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_global_UIGlobalSettingsUpdate_h */
