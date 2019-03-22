/* $Id$ */
/** @file
 * VBox Qt GUI - UIExtraDataManager class declaration.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_extradata_UIExtraDataManager_h
#define FEQT_INCLUDED_SRC_extradata_UIExtraDataManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QObject>
#include <QSize>
#include <QUuid>
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
# include <QPointer>
#endif

/* GUI includes: */
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class UIExtraDataEventHandler;
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
class UIExtraDataManagerWindow;
#endif

/** Defines the map of extra data values. The index is an extra-data key. */
typedef QMap<QString, QString> ExtraDataMap;
/** Defines the map of extra data maps. */
typedef QMap<QUuid, ExtraDataMap> MapOfExtraDataMaps;

/** Singleton QObject extension
  * providing GUI with corresponding extra-data values,
  * and notifying it whenever any of those values changed. */
class SHARED_LIBRARY_STUFF UIExtraDataManager : public QObject
{
    Q_OBJECT;

    /** Extra-data Manager constructor. */
    UIExtraDataManager();
    /** Extra-data Manager destructor. */
    ~UIExtraDataManager();

signals:

    /** Notifies about extra-data map acknowledging. */
    void sigExtraDataMapAcknowledging(const QUuid &uID);

    /** Notifies about extra-data change. */
    void sigExtraDataChange(const QUuid &uID, const QString &strKey, const QString &strValue);

    /** Notifies about GUI language change. */
    void sigLanguageChange(QString strLanguage);

    /** Notifies about Selector UI keyboard shortcut change. */
    void sigSelectorUIShortcutChange();
    /** Notifies about Runtime UI keyboard shortcut change. */
    void sigRuntimeUIShortcutChange();
    /** Notifies about Runtime UI host-key combination change. */
    void sigRuntimeUIHostKeyCombinationChange();

    /** Notifies about VirtualBox Manager / Details pane categories change. */
    void sigDetailsCategoriesChange();
    /** Notifies about VirtualBox Manager / Details pane options change. */
    void sigDetailsOptionsChange(DetailsElementType enmType);

    /** Notifies about menu-bar configuration change. */
    void sigMenuBarConfigurationChange(const QUuid &uMachineID);
    /** Notifies about status-bar configuration change. */
    void sigStatusBarConfigurationChange(const QUuid &uMachineID);

    /** Notifies about HID LEDs synchronization state change. */
    void sigHidLedsSyncStateChange(bool fEnabled);

    /** Notifies about the scale-factor change. */
    void sigScaleFactorChange(const QUuid &uMachineID);

    /** Notifies about the scaling optimization type change. */
    void sigScalingOptimizationTypeChange(const QUuid &uMachineID);

#ifdef VBOX_WS_MAC
    /** Notifies about the HiDPI optimization type change. */
    void sigHiDPIOptimizationTypeChange(const QUuid &uMachineID);

    /** Mac OS X: Notifies about 'dock icon' appearance change. */
    void sigDockIconAppearanceChange(bool fEnabled);
    /** Mac OS X: Notifies about 'dock icon overlay' appearance change. */
    void sigDockIconOverlayAppearanceChange(bool fEnabled);
#endif /* VBOX_WS_MAC */

public:

    /** Global extra-data ID. */
    static const QUuid GlobalID;

    /** Static Extra-data Manager instance/constructor. */
    static UIExtraDataManager* instance();
    /** Static Extra-data Manager destructor. */
    static void destroy();

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /** Static show and raise API. */
    static void openWindow(QWidget *pCenterWidget);
#endif

    /** @name Base
      * @{ */
        /** Returns whether Extra-data Manager cached the map with passed @a uID. */
        bool contains(const QUuid &uID) const { return m_data.contains(uID); }
        /** Returns read-only extra-data map for passed @a uID. */
        const ExtraDataMap map(const QUuid &uID) const { return m_data.value(uID); }

        /** Hot-load machine extra-data map. */
        void hotloadMachineExtraDataMap(const QUuid &uID);

        /** Returns extra-data value corresponding to passed @a strKey as QString.
          * If valid @a uID is set => applies to machine extra-data, otherwise => to global one. */
        QString extraDataString(const QString &strKey, const QUuid &uID = GlobalID);
        /** Defines extra-data value corresponding to passed @a strKey as strValue.
          * If valid @a uID is set => applies to machine extra-data, otherwise => to global one. */
        void setExtraDataString(const QString &strKey, const QString &strValue, const QUuid &uID = GlobalID);

        /** Returns extra-data value corresponding to passed @a strKey as QStringList.
          * If valid @a uID is set => applies to machine extra-data, otherwise => to global one. */
        QStringList extraDataStringList(const QString &strKey, const QUuid &uID = GlobalID);
        /** Defines extra-data value corresponding to passed @a strKey as value.
          * If valid @a uID is set => applies to machine extra-data, otherwise => to global one. */
        void setExtraDataStringList(const QString &strKey, const QStringList &value, const QUuid &uID = GlobalID);
    /** @} */

    /** @name General
      * @{ */
        /** Returns event handling type. */
        EventHandlingType eventHandlingType();
    /** @} */

    /** @name Messaging
      * @{ */
        /** Returns the list of supressed messages for the Message/Popup center frameworks. */
        QStringList suppressedMessages(const QUuid &uID = GlobalID);
        /** Defines the @a list of supressed messages for the Message/Popup center frameworks. */
        void setSuppressedMessages(const QStringList &list);

        /** Returns the list of messages for the Message/Popup center frameworks with inverted check-box state. */
        QStringList messagesWithInvertedOption();

#if !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG)
        /** Returns version for which user wants to prevent BETA build warning. */
        QString preventBetaBuildWarningForVersion();
#endif
    /** @} */

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /** @name Application Update
      * @{ */
        /** Returns whether Application Update functionality enabled. */
        bool applicationUpdateEnabled();

        /** Returns Application Update data. */
        QString applicationUpdateData();
        /** Defines Application Update data as @a strValue. */
        void setApplicationUpdateData(const QString &strValue);

        /** Returns Application Update check counter. */
        qulonglong applicationUpdateCheckCounter();
        /** Increments Application Update check counter. */
        void incrementApplicationUpdateCheckCounter();
    /** @} */
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /** @name Progress
      * @{ */
        /** Returns whether legacy progress handling method is requested. */
        bool legacyProgressHandlingRequested();
    /** @} */

    /** @name Settings
      * @{ */
        /** Returns whether GUI @a enmFeature is enabled. */
        bool guiFeatureEnabled(GUIFeatureType enmFeature);

        /** Returns restricted global settings pages. */
        QList<GlobalSettingsPageType> restrictedGlobalSettingsPages();
        /** Returns restricted machine settings pages. */
        QList<MachineSettingsPageType> restrictedMachineSettingsPages(const QUuid &uID);
    /** @} */

    /** @name Settings: General
      * @{ */
        /** Returns whether the host screen-saver should be disabled. */
        bool hostScreenSaverDisabled();
        /** Defines whether the host screen-saver should be @a fDisabled. */
        void setHostScreenSaverDisabled(bool fDisabled);
    /** @} */

    /** @name Settings: Language
      * @{ */
        /** Returns the GUI language ID. */
        QString languageId();
        /** Defines the GUI @a strLanguageId. */
        void setLanguageId(const QString &strLanguageId);
    /** @} */

    /** @name Settings: Display
      * @{ */
        /** Returns maximum guest-screen resolution policy. */
        MaxGuestResolutionPolicy maxGuestResolutionPolicy();
        /** Defines maximum guest-screen resolution @a enmPolicy or @a resolution itself for Fixed policy. */
        void setMaxGuestScreenResolution(MaxGuestResolutionPolicy enmPolicy, const QSize resolution = QSize());
        /** Returns maximum guest-screen resolution for fixed policy. */
        QSize maxGuestResolutionForPolicyFixed();
        /** Defines maximum guest-screen @a resolution for fixed policy. */
        void setMaxGuestResolutionForPolicyFixed(const QSize &resolution);

        /** Returns whether hovered machine-window should be activated. */
        bool activateHoveredMachineWindow();
        /** Defines whether hovered machine-window should be @a fActivated. */
        void setActivateHoveredMachineWindow(bool fActivate);
    /** @} */

    /** @name Settings: Keyboard
      * @{ */
        /** Returns the Runtime UI host-key combination. */
        QString hostKeyCombination();
        /** Defines the Runtime UI host-key combination. */
        void setHostKeyCombination(const QString &strHostCombo);

        /** Returns shortcut overrides for shortcut-pool with @a strPoolExtraDataID. */
        QStringList shortcutOverrides(const QString &strPoolExtraDataID);

        /** Returns whether the Runtime UI auto-capture is enabled. */
        bool autoCaptureEnabled();
        /** Defines whether the Runtime UI auto-capture is @a fEnabled. */
        void setAutoCaptureEnabled(bool fEnabled);

        /** Returns the Runtime UI remapped scan codes. */
        QString remappedScanCodes();
    /** @} */

    /** @name Settings: Proxy
      * @{ */
        /** Returns VBox proxy settings. */
        QString proxySettings();
        /** Defines VBox proxy @a strSettings. */
        void setProxySettings(const QString &strSettings);
    /** @} */

    /** @name Settings: Storage
      * @{ */
        /** Returns recent folder for hard-drives. */
        QString recentFolderForHardDrives();
        /** Returns recent folder for optical-disks. */
        QString recentFolderForOpticalDisks();
        /** Returns recent folder for floppy-disks. */
        QString recentFolderForFloppyDisks();
        /** Returns recent folder for VISO creation content. */
        QString recentFolderForVISOContent();
        /** Defines recent folder for hard-drives as @a strValue. */
        void setRecentFolderForHardDrives(const QString &strValue);
        /** Defines recent folder for optical-disk as @a strValue. */
        void setRecentFolderForOpticalDisks(const QString &strValue);
        /** Defines recent folder for floppy-disk as @a strValue. */
        void setRecentFolderForFloppyDisks(const QString &strValue);
        /** Defines recent folder for VISO creation content as @a strValue. */
        void setRecentFolderForVISOContent(const QString &strValue);

        /** Returns the list of recently used hard-drives. */
        QStringList recentListOfHardDrives();
        /** Returns the list of recently used optical-disk. */
        QStringList recentListOfOpticalDisks();
        /** Returns the list of recently used floppy-disk. */
        QStringList recentListOfFloppyDisks();
        /** Defines the list of recently used hard-drives as @a value. */
        void setRecentListOfHardDrives(const QStringList &value);
        /** Defines the list of recently used optical-disks as @a value. */
        void setRecentListOfOpticalDisks(const QStringList &value);
        /** Defines the list of recently used floppy-disks as @a value. */
        void setRecentListOfFloppyDisks(const QStringList &value);
    /** @} */

    /** @name VirtualBox Manager
      * @{ */
        /** Returns selector-window geometry using @a pWidget as the hint. */
        QRect selectorWindowGeometry(QWidget *pWidget);
        /** Returns whether selector-window should be maximized. */
        bool selectorWindowShouldBeMaximized();
        /** Defines selector-window @a geometry and @a fMaximized state. */
        void setSelectorWindowGeometry(const QRect &geometry, bool fMaximized);

        /** Returns selector-window splitter hints. */
        QList<int> selectorWindowSplitterHints();
        /** Defines selector-window splitter @a hints. */
        void setSelectorWindowSplitterHints(const QList<int> &hints);

        /** Returns whether selector-window tool-bar visible. */
        bool selectorWindowToolBarVisible();
        /** Defines whether selector-window tool-bar @a fVisible. */
        void setSelectorWindowToolBarVisible(bool fVisible);

        /** Returns whether selector-window tool-bar text visible. */
        bool selectorWindowToolBarTextVisible();
        /** Defines whether selector-window tool-bar text @a fVisible. */
        void setSelectorWindowToolBarTextVisible(bool fVisible);

        /** Returns last selected tool set of VirtualBox Manager. */
        QList<UIToolType> toolsPaneLastItemsChosen();
        /** Defines last selected tool @a set of VirtualBox Manager. */
        void setToolsPaneLastItemsChosen(const QList<UIToolType> &set);

        /** Returns whether selector-window status-bar visible. */
        bool selectorWindowStatusBarVisible();
        /** Defines whether selector-window status-bar @a fVisible. */
        void setSelectorWindowStatusBarVisible(bool fVisible);

        /** Clears all the existing selector-window chooser-pane' group definitions. */
        void clearSelectorWindowGroupsDefinitions();
        /** Returns selector-window chooser-pane' groups definitions for passed @a strGroupID. */
        QStringList selectorWindowGroupsDefinitions(const QString &strGroupID);
        /** Defines selector-window chooser-pane' groups @a definitions for passed @a strGroupID. */
        void setSelectorWindowGroupsDefinitions(const QString &strGroupID, const QStringList &definitions);

        /** Returns last-item ID of the item chosen in selector-window chooser-pane. */
        QString selectorWindowLastItemChosen();
        /** Defines @a lastItemID of the item chosen in selector-window chooser-pane. */
        void setSelectorWindowLastItemChosen(const QString &strItemID);

        /** Returns selector-window details-pane' elements. */
        QMap<DetailsElementType, bool> selectorWindowDetailsElements();
        /** Defines selector-window details-pane' @a elements. */
        void setSelectorWindowDetailsElements(const QMap<DetailsElementType, bool> &elements);

        /** Returns selector-window details-pane' preview update interval. */
        PreviewUpdateIntervalType selectorWindowPreviewUpdateInterval();
        /** Defines selector-window details-pane' preview update @a interval. */
        void setSelectorWindowPreviewUpdateInterval(PreviewUpdateIntervalType interval);

        /** Returns VirtualBox Manager / Details pane options for certain @a enmElementType. */
        QStringList vboxManagerDetailsPaneElementOptions(DetailsElementType enmElementType);
        /** Defines VirtualBox Manager / Details pane @a options for certain @a enmElementType. */
        void setVBoxManagerDetailsPaneElementOptions(DetailsElementType enmElementType, const QStringList &options);
    /** @} */

    /** @name Snapshot Manager
      * @{ */
        /** Returns whether Snapshot Manager details expanded. */
        bool snapshotManagerDetailsExpanded();
        /** Defines whether Snapshot Manager details @a fExpanded. */
        void setSnapshotManagerDetailsExpanded(bool fExpanded);
    /** @} */

    /** @name Virtual Media Manager
      * @{ */
        /** Returns whether Virtual Media Manager details expanded. */
        bool virtualMediaManagerDetailsExpanded();
        /** Defines whether Virtual Media Manager details @a fExpanded. */
        void setVirtualMediaManagerDetailsExpanded(bool fExpanded);
        /** Returns whether Virtual Media Manager search widget expanded. */
        bool virtualMediaManagerSearchWidgetExpanded();
        /** Defines whether Virtual Media Manager search widget @a fExpanded. */
        void setVirtualMediaManagerSearchWidgetExpanded(bool fExpanded);
    /** @} */

    /** @name Host Network Manager
      * @{ */
        /** Returns whether Host Network Manager details expanded. */
        bool hostNetworkManagerDetailsExpanded();
        /** Defines whether Host Network Manager details @a fExpanded. */
        void setHostNetworkManagerDetailsExpanded(bool fExpanded);
    /** @} */

    /** @name Cloud Profile Manager
      * @{ */
        /** Returns whether Cloud Profile Manager details expanded. */
        bool cloudProfileManagerDetailsExpanded();
        /** Defines whether Cloud Profile Manager details @a fExpanded. */
        void setCloudProfileManagerDetailsExpanded(bool fExpanded);
    /** @} */

    /** @name Wizards
      * @{ */
        /** Returns mode for wizard of passed @a type. */
        WizardMode modeForWizardType(WizardType type);
        /** Defines @a mode for wizard of passed @a type. */
        void setModeForWizardType(WizardType type, WizardMode mode);
    /** @} */

    /** @name Virtual Machine
      * @{ */
        /** Returns whether machine should be shown in selector-window chooser-pane. */
        bool showMachineInSelectorChooser(const QUuid &uID);
        /** Returns whether machine should be shown in selector-window details-pane. */
        bool showMachineInSelectorDetails(const QUuid &uID);

        /** Returns whether machine reconfiguration enabled. */
        bool machineReconfigurationEnabled(const QUuid &uID);
        /** Returns whether machine snapshot operations enabled. */
        bool machineSnapshotOperationsEnabled(const QUuid &uID);

        /** Returns whether this machine is first time started. */
        bool machineFirstTimeStarted(const QUuid &uID);
        /** Returns whether this machine is fFirstTimeStarted. */
        void setMachineFirstTimeStarted(bool fFirstTimeStarted, const QUuid &uID);

        /** Except Mac OS X: Returns redefined machine-window icon names. */
        QStringList machineWindowIconNames(const QUuid &uID);
#ifndef VBOX_WS_MAC
        /** Except Mac OS X: Returns redefined machine-window name postfix. */
        QString machineWindowNamePostfix(const QUuid &uID);
#endif

        /** Returns geometry for machine-window with @a uScreenIndex in @a visualStateType. */
        QRect machineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QUuid &uID);
        /** Returns whether machine-window with @a uScreenIndex in @a visualStateType should be maximized. */
        bool machineWindowShouldBeMaximized(UIVisualStateType visualStateType, ulong uScreenIndex, const QUuid &uID);
        /** Defines @a geometry and @a fMaximized state for machine-window with @a uScreenIndex in @a visualStateType. */
        void setMachineWindowGeometry(UIVisualStateType visualStateType, ulong uScreenIndex, const QRect &geometry, bool fMaximized, const QUuid &uID);

#ifndef VBOX_WS_MAC
        /** Returns whether Runtime UI menu-bar is enabled. */
        bool menuBarEnabled(const QUuid &uID);
        /** Defines whether Runtime UI menu-bar is @a fEnabled. */
        void setMenuBarEnabled(bool fEnabled, const QUuid &uID);
#endif /* !VBOX_WS_MAC */

        /** Returns whether Runtime UI menu-bar context-menu is enabled. */
        bool menuBarContextMenuEnabled(const QUuid &uID);
        /** Defines whether Runtime UI menu-bar context-menu is @a fEnabled. */
        void setMenuBarContextMenuEnabled(bool fEnabled, const QUuid &uID);

        /** Returns restricted Runtime UI menu types. */
        UIExtraDataMetaDefs::MenuType restrictedRuntimeMenuTypes(const QUuid &uID);
        /** Defines restricted Runtime UI menu types. */
        void setRestrictedRuntimeMenuTypes(UIExtraDataMetaDefs::MenuType types, const QUuid &uID);

        /** Returns restricted Runtime UI action types for Application menu. */
        UIExtraDataMetaDefs::MenuApplicationActionType restrictedRuntimeMenuApplicationActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Application menu. */
        void setRestrictedRuntimeMenuApplicationActionTypes(UIExtraDataMetaDefs::MenuApplicationActionType types, const QUuid &uID);

        /** Returns restricted Runtime UI action types for Machine menu. */
        UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictedRuntimeMenuMachineActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Machine menu. */
        void setRestrictedRuntimeMenuMachineActionTypes(UIExtraDataMetaDefs::RuntimeMenuMachineActionType types, const QUuid &uID);

        /** Returns restricted Runtime UI action types for View menu. */
        UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictedRuntimeMenuViewActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for View menu. */
        void setRestrictedRuntimeMenuViewActionTypes(UIExtraDataMetaDefs::RuntimeMenuViewActionType types, const QUuid &uID);

        /** Returns restricted Runtime UI action types for Input menu. */
        UIExtraDataMetaDefs::RuntimeMenuInputActionType restrictedRuntimeMenuInputActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Input menu. */
        void setRestrictedRuntimeMenuInputActionTypes(UIExtraDataMetaDefs::RuntimeMenuInputActionType types, const QUuid &uID);

        /** Returns restricted Runtime UI action types for Devices menu. */
        UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictedRuntimeMenuDevicesActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Devices menu. */
        void setRestrictedRuntimeMenuDevicesActionTypes(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType types, const QUuid &uID);

#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Returns restricted Runtime UI action types for Debugger menu. */
        UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restrictedRuntimeMenuDebuggerActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Debugger menu. */
        void setRestrictedRuntimeMenuDebuggerActionTypes(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType types, const QUuid &uID);
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef VBOX_WS_MAC
        /** Mac OS X: Returns restricted Runtime UI action types for Window menu. */
        UIExtraDataMetaDefs::MenuWindowActionType restrictedRuntimeMenuWindowActionTypes(const QUuid &uID);
        /** Mac OS X: Defines restricted Runtime UI action types for Window menu. */
        void setRestrictedRuntimeMenuWindowActionTypes(UIExtraDataMetaDefs::MenuWindowActionType types, const QUuid &uID);
#endif /* VBOX_WS_MAC */

        /** Returns restricted Runtime UI action types for Help menu. */
        UIExtraDataMetaDefs::MenuHelpActionType restrictedRuntimeMenuHelpActionTypes(const QUuid &uID);
        /** Defines restricted Runtime UI action types for Help menu. */
        void setRestrictedRuntimeMenuHelpActionTypes(UIExtraDataMetaDefs::MenuHelpActionType types, const QUuid &uID);

        /** Returns restricted Runtime UI visual-states. */
        UIVisualStateType restrictedVisualStates(const QUuid &uID);

        /** Returns requested Runtime UI visual-state. */
        UIVisualStateType requestedVisualState(const QUuid &uID);
        /** Defines requested Runtime UI visual-state as @a visualState. */
        void setRequestedVisualState(UIVisualStateType visualState, const QUuid &uID);

#ifdef VBOX_WS_X11
        /** Returns whether legacy full-screen mode is requested. */
        bool legacyFullscreenModeRequested();

        /** Returns whether internal machine-window name should be unique. */
        bool distinguishMachineWindowGroups(const QUuid &uID);
        /** Defines whether internal machine-window name should be unique. */
        void setDistinguishMachineWindowGroups(const QUuid &uID, bool fEnabled);
#endif /* VBOX_WS_X11 */

        /** Returns whether guest-screen auto-resize according machine-window size is enabled. */
        bool guestScreenAutoResizeEnabled(const QUuid &uID);
        /** Defines whether guest-screen auto-resize according machine-window size is @a fEnabled. */
        void setGuestScreenAutoResizeEnabled(bool fEnabled, const QUuid &uID);

        /** Returns last guest-screen visibility status for screen with @a uScreenIndex. */
        bool lastGuestScreenVisibilityStatus(ulong uScreenIndex, const QUuid &uID);
        /** Defines whether last guest-screen visibility status was @a fEnabled for screen with @a uScreenIndex. */
        void setLastGuestScreenVisibilityStatus(ulong uScreenIndex, bool fEnabled, const QUuid &uID);

        /** Returns last guest-screen size-hint for screen with @a uScreenIndex. */
        QSize lastGuestScreenSizeHint(ulong uScreenIndex, const QUuid &uID);
        /** Defines last guest-screen @a sizeHint for screen with @a uScreenIndex. */
        void setLastGuestScreenSizeHint(ulong uScreenIndex, const QSize &sizeHint, const QUuid &uID);

        /** Returns host-screen index corresponding to passed guest-screen @a iGuestScreenIndex. */
        int hostScreenForPassedGuestScreen(int iGuestScreenIndex, const QUuid &uID);
        /** Defines @a iHostScreenIndex corresponding to passed guest-screen @a iGuestScreenIndex. */
        void setHostScreenForPassedGuestScreen(int iGuestScreenIndex, int iHostScreenIndex, const QUuid &uID);

        /** Returns whether automatic mounting/unmounting of guest-screens enabled. */
        bool autoMountGuestScreensEnabled(const QUuid &uID);

#ifdef VBOX_WITH_VIDEOHWACCEL
        /** Returns whether 2D acceleration should use linear sretch. */
        bool useLinearStretch(const QUuid &uID);
        /** Returns whether 2D acceleration should use YV12 pixel format. */
        bool usePixelFormatYV12(const QUuid &uID);
        /** Returns whether 2D acceleration should use UYVY pixel format. */
        bool usePixelFormatUYVY(const QUuid &uID);
        /** Returns whether 2D acceleration should use YUY2 pixel format. */
        bool usePixelFormatYUY2(const QUuid &uID);
        /** Returns whether 2D acceleration should use AYUV pixel format. */
        bool usePixelFormatAYUV(const QUuid &uID);
#endif /* VBOX_WITH_VIDEOHWACCEL */

#ifndef VBOX_WS_MAC
        /** Returns whether mini-toolbar is enabled for full and seamless screens. */
        bool miniToolbarEnabled(const QUuid &uID);
        /** Defines whether mini-toolbar is @a fEnabled for full and seamless screens. */
        void setMiniToolbarEnabled(bool fEnabled, const QUuid &uID);

        /** Returns whether mini-toolbar should auto-hide itself. */
        bool autoHideMiniToolbar(const QUuid &uID);
        /** Defines whether mini-toolbar should @a fAutoHide itself. */
        void setAutoHideMiniToolbar(bool fAutoHide, const QUuid &uID);

        /** Returns mini-toolbar alignment. */
        Qt::AlignmentFlag miniToolbarAlignment(const QUuid &uID);
        /** Returns mini-toolbar @a alignment. */
        void setMiniToolbarAlignment(Qt::AlignmentFlag alignment, const QUuid &uID);
#endif /* VBOX_WS_MAC */

        /** Returns whether Runtime UI status-bar is enabled. */
        bool statusBarEnabled(const QUuid &uID);
        /** Defines whether Runtime UI status-bar is @a fEnabled. */
        void setStatusBarEnabled(bool fEnabled, const QUuid &uID);

        /** Returns whether Runtime UI status-bar context-menu is enabled. */
        bool statusBarContextMenuEnabled(const QUuid &uID);
        /** Defines whether Runtime UI status-bar context-menu is @a fEnabled. */
        void setStatusBarContextMenuEnabled(bool fEnabled, const QUuid &uID);

        /** Returns restricted Runtime UI status-bar indicator list. */
        QList<IndicatorType> restrictedStatusBarIndicators(const QUuid &uID);
        /** Defines restricted Runtime UI status-bar indicator @a list. */
        void setRestrictedStatusBarIndicators(const QList<IndicatorType> &list, const QUuid &uID);

        /** Returns Runtime UI status-bar indicator order list. */
        QList<IndicatorType> statusBarIndicatorOrder(const QUuid &uID);
        /** Defines Runtime UI status-bar indicator order @a list. */
        void setStatusBarIndicatorOrder(const QList<IndicatorType> &list, const QUuid &uID);

#ifdef VBOX_WS_MAC
        /** Mac OS X: Returns whether Dock icon should be updated at runtime. */
        bool realtimeDockIconUpdateEnabled(const QUuid &uID);
        /** Mac OS X: Defines whether Dock icon update should be fEnabled at runtime. */
        void setRealtimeDockIconUpdateEnabled(bool fEnabled, const QUuid &uID);

        /** Mac OS X: Returns guest-screen which Dock icon should reflect at runtime. */
        int realtimeDockIconUpdateMonitor(const QUuid &uID);
        /** Mac OS X: Defines guest-screen @a iIndex which Dock icon should reflect at runtime. */
        void setRealtimeDockIconUpdateMonitor(int iIndex, const QUuid &uID);

        /** Mac OS X: Returns whether Dock icon overlay is disabled. */
        bool dockIconDisableOverlay(const QUuid &uID);
        /** Mac OS X: Defines whether Dock icon overlay is @a fDisabled. */
        void setDockIconDisableOverlay(bool fDisabled, const QUuid &uID);
#endif /* VBOX_WS_MAC */

        /** Returns whether machine should pass CAD to guest. */
        bool passCADtoGuest(const QUuid &uID);

        /** Returns the mouse-capture policy. */
        MouseCapturePolicy mouseCapturePolicy(const QUuid &uID);

        /** Returns redefined guru-meditation handler type. */
        GuruMeditationHandlerType guruMeditationHandlerType(const QUuid &uID);

        /** Returns whether machine should perform HID LEDs synchronization. */
        bool hidLedsSyncState(const QUuid &uID);

        /** Returns the scale-factor. */
        double scaleFactor(const QUuid &uID, const int uScreenIndex);
        QList<double> scaleFactors(const QUuid &uID);
        /** Saves the @a dScaleFactor for the monitor with @a uScreenIndex. If the existing scale factor
          * list (from extra data) does not have scale factors for the screens with ids in [0, uScreenIndex)
          * the this function appends a default scale factor for said screens.*/
        void setScaleFactor(double dScaleFactor, const QUuid &uID, const int uScreenIndex);
        /** Replaces the scale factor list of the machine with @a uID with @a scaleFactors. */
        void setScaleFactors(const QList<double> &scaleFactors, const QUuid &uID);

        /** Returns the scaling optimization type. */
        ScalingOptimizationType scalingOptimizationType(const QUuid &uID);
    /** @} */

    /** @name Virtual Machine: Information dialog
      * @{ */
        /** Returns information-window geometry using @a pWidget and @a pParentWidget as hints. */
        QRect informationWindowGeometry(QWidget *pWidget, QWidget *pParentWidget, const QUuid &uID);
        /** Returns whether information-window should be maximized or not. */
        bool informationWindowShouldBeMaximized(const QUuid &uID);
        /** Defines information-window @a geometry and @a fMaximized state. */
        void setInformationWindowGeometry(const QRect &geometry, bool fMaximized, const QUuid &uID);

        /** Returns information-window elements. */
        QMap<InformationElementType, bool> informationWindowElements();
        /** Defines information-window @a elements. */
        void setInformationWindowElements(const QMap<InformationElementType, bool> &elements);
    /** @} */

    /** @name Guest Control related dialogs
      * @{ */
        void setGuestControlProcessControlSplitterHints(const QList<int> &hints);
        QList<int> guestControlProcessControlSplitterHints();
        QRect fileManagerDialogGeometry(QWidget *pWidget, const QRect &defaultGeometry);
        bool fileManagerDialogShouldBeMaximized();
        void setFileManagerDialogGeometry(const QRect &geometry, bool fMaximized);
        QRect guestProcessControlDialogGeometry(QWidget *pWidget, const QRect &defaultGeometry);
        bool guestProcessControlDialogShouldBeMaximized();
        void setGuestProcessControlDialogGeometry(const QRect &geometry, bool fMaximized);
        void setFileManagerVisiblePanels(const QStringList &panelNameList);
        QStringList fileManagerVisiblePanels();
    /** @} */

    /** @name File Manager options
      * @{ */
        void setFileManagerOptions(bool fListDirectoriesFirst,
                                   bool fShowDeleteConfirmation,
                                   bool fshowHumanReadableSizes,
                                   bool fShowHiddenObjects);
        bool fileManagerListDirectoriesFirst();
        bool fileManagerShowDeleteConfirmation();
        bool fileManagerShowHumanReadableSizes();
        bool fileManagerShowHiddenObjects();
    /** @} */

    /** @name Virtual Machine: Close dialog
      * @{ */
        /** Returns default machine close action. */
        MachineCloseAction defaultMachineCloseAction(const QUuid &uID);
        /** Returns restricted machine close actions. */
        MachineCloseAction restrictedMachineCloseActions(const QUuid &uID);

        /** Returns last machine close action. */
        MachineCloseAction lastMachineCloseAction(const QUuid &uID);
        /** Defines last @a machineCloseAction. */
        void setLastMachineCloseAction(MachineCloseAction machineCloseAction, const QUuid &uID);

        /** Returns machine close hook script name as simple string. */
        QString machineCloseHookScript(const QUuid &uID);
    /** @} */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** @name Virtual Machine: Debug UI
      * @{ */
        /** Returns debug flag value for passed @a strDebugFlagKey. */
        QString debugFlagValue(const QString &strDebugFlagKey);
    /** @} */
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /** @name VirtualBox: Extra-data Manager window
      * @{ */
        /** Returns Extra-data Manager geometry using @a pWidget as hint. */
        QRect extraDataManagerGeometry(QWidget *pWidget);
        /** Returns whether Extra-data Manager should be maximized or not. */
        bool extraDataManagerShouldBeMaximized();
        /** Defines Extra-data Manager @a geometry and @a fMaximized state. */
        void setExtraDataManagerGeometry(const QRect &geometry, bool fMaximized);

        /** Returns Extra-data Manager splitter hints using @a pWidget as hint. */
        QList<int> extraDataManagerSplitterHints(QWidget *pWidget);
        /** Defines Extra-data Manager splitter @a hints. */
        void setExtraDataManagerSplitterHints(const QList<int> &hints);
    /** @} */
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

    /** @name Virtual Machine: Log Viewer dialog
      * @{ */
        /** Returns log-window geometry using @a pWidget and @a defaultGeometry as hints. */
        QRect logWindowGeometry(QWidget *pWidget, const QRect &defaultGeometry);
        /** Returns whether log-window should be maximized or not. */
        bool logWindowShouldBeMaximized();
        /** Defines log-window @a geometry and @a fMaximized state. */
        void setLogWindowGeometry(const QRect &geometry, bool fMaximized);
    /** @} */

    /** @name Virtual Machine: Log Viewer widget options
      * @{ */
        void setLogViweverOptions(const QFont &font, bool wrapLines, bool showLineNumbers);
        /** Returns log-viewer line wrapping flag. */
        bool logViewerWrapLines();
        /** Returns log-viewer show line numbers flag. */
        bool logViewerShowLineNumbers();
        /** Tries to find system font by searching by family and style strings within the font database. */
        QFont logViewerFont();
        void setLogViewerVisiblePanels(const QStringList &panelNameList);
        QStringList logViewerVisiblePanels();
    /** @} */

private slots:

    /** Handles 'extra-data change' event: */
    void sltExtraDataChange(const QUuid &uMachineID, const QString &strKey, const QString &strValue);

private:

    /** Prepare Extra-data Manager. */
    void prepare();
    /** Prepare global extra-data map. */
    void prepareGlobalExtraDataMap();
    /** Prepare extra-data event-handler. */
    void prepareExtraDataEventHandler();
#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    // /** Prepare window. */
    // void prepareWindow();

    /** Cleanup window. */
    void cleanupWindow();
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */
    /** Cleanup extra-data event-handler. */
    void cleanupExtraDataEventHandler();
    // /** Cleanup extra-data map. */
    // void cleanupExtraDataMap();
    /** Cleanup Extra-data Manager. */
    void cleanup();

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /** Open window. */
    void open(QWidget *pCenterWidget);
#endif

    /** Retrieves an extra-data key from both machine and global sources.
      *
      * If @a uID isn't #GlobalID, this will first check the extra-data associated
      * with the machine given by @a uID then fallback on the global extra-data.
      *
      * @returns String value if found, null string if not.
      * @param   strKey      The extra-data key to get.
      * @param   uID         Machine UUID or #GlobalID.
      * @param   strValue    Where to return the value when found. */
    QString extraDataStringUnion(const QString &strKey, const QUuid &uID);
    /** Determines whether feature corresponding to passed @a strKey is allowed.
      * If valid @a uID is set => applies to machine and global extra-data,
      * otherwise => only to global one. */
    bool isFeatureAllowed(const QString &strKey, const QUuid &uID = GlobalID);
    /** Determines whether feature corresponding to passed @a strKey is restricted.
      * If valid @a uID is set => applies to machine and global extra-data,
      * otherwise => only to global one. */
    bool isFeatureRestricted(const QString &strKey, const QUuid &uID = GlobalID);

    /** Translates bool flag into QString value. */
    QString toFeatureState(bool fState);
    /** Translates bool flag into 'allowed' value. */
    QString toFeatureAllowed(bool fAllowed);
    /** Translates bool flag into 'restricted' value. */
    QString toFeatureRestricted(bool fRestricted);

    QRect dialogGeometry(const QString &strKey, QWidget *pWidget, const QRect &defaultGeometry);
    void setDialogGeometry(const QString &strKey, const QRect &geometry, bool fMaximized);

    /** Returns string consisting of @a strBase appended with @a uScreenIndex for the *non-primary* screen-index.
      * If @a fSameRuleForPrimary is 'true' same rule will be used for *primary* screen-index. Used for storing per-screen extra-data. */
    static QString extraDataKeyPerScreen(const QString &strBase, ulong uScreenIndex, bool fSameRuleForPrimary = false);

    /** Holds the singleton instance. */
    static UIExtraDataManager *s_pInstance;

    /** Holds extra-data event-handler instance. */
    UIExtraDataEventHandler *m_pHandler;

    /** Holds extra-data map instance. */
    MapOfExtraDataMaps m_data;

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /** Holds Extra-data Manager window instance. */
    QPointer<UIExtraDataManagerWindow> m_pWindow;
#endif
};

/** Singleton Extra-data Manager 'official' name. */
#define gEDataManager UIExtraDataManager::instance()

#endif /* !FEQT_INCLUDED_SRC_extradata_UIExtraDataManager_h */
