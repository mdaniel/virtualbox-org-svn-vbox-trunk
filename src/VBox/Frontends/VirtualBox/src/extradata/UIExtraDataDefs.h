/* $Id$ */
/** @file
 * VBox Qt GUI - Extra-data related definitions.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIExtraDataDefs_h___
#define ___UIExtraDataDefs_h___

/* Qt includes: */
#include <QMetaType>
#include <QObject>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>


/** Extra-data namespace. */
namespace UIExtraDataDefs
{
    /** @name General
      * @{ */
        /** Holds event handling type. */
        SHARED_LIBRARY_STUFF extern const char *GUI_EventHandlingType;
    /** @} */

    /** @name Messaging
      * @{ */
        /** Holds the list of supressed messages for the Message/Popup center frameworks. */
        SHARED_LIBRARY_STUFF extern const char *GUI_SuppressMessages;
        /** Holds the list of messages for the Message/Popup center frameworks with inverted check-box state. */
        SHARED_LIBRARY_STUFF extern const char *GUI_InvertMessageOption;
#if !defined(VBOX_BLEEDING_EDGE) && !defined(DEBUG)
        /** Holds version for which user wants to prevent BETA build warning. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PreventBetaWarning;
#endif
    /** @} */

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    /** @name Application Update
      * @{ */
        /** Holds whether Application Update functionality enabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PreventApplicationUpdate;
        /** Holds Application Update data. */
        SHARED_LIBRARY_STUFF extern const char *GUI_UpdateDate;
        /** Holds Application Update check counter. */
        SHARED_LIBRARY_STUFF extern const char *GUI_UpdateCheckCount;
    /** @} */
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

    /** @name Progress
      * @{ */
        /** Holds whether legacy progress handling method is requested. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Progress_LegacyMode;
    /** @} */

    /** @name Settings
      * @{ */
        /** Holds GUI feature list. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Customizations;
        /** Holds restricted Global Settings pages. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedGlobalSettingsPages;
        /** Holds restricted Machine Settings pages. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedMachineSettingsPages;
    /** @} */

    /** @name Settings: General
      * @{ */
        /** Holds whether host screen-saver should be disabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HostScreenSaverDisabled;
    /** @} */

    /** @name Settings: Language
      * @{ */
        /** Holds GUI language ID. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LanguageID;
    /** @} */

    /** @name Settings: Display
      * @{ */
        /** Holds maximum guest-screen resolution policy/value. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MaxGuestResolution;
        /** Holds whether hovered machine-window should be activated. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ActivateHoveredMachineWindow;
    /** @} */

    /** @name Settings: Keyboard
      * @{ */
        /** Holds Selector UI shortcut overrides. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Input_SelectorShortcuts;
        /** Holds Runtime UI shortcut overrides. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Input_MachineShortcuts;
        /** Holds Runtime UI host-key combination. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Input_HostKeyCombination;
        /** Holds whether Runtime UI auto-capture is enabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Input_AutoCapture;
        /** Holds Runtime UI remapped scan codes. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RemapScancodes;
    /** @} */

    /** @name Settings: Proxy
      * @{ */
        /** Holds VBox proxy settings. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ProxySettings;
    /** @} */

    /** @name Settings: Storage
      * @{ */
        /** Holds recent folder for hard-drives. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentFolderHD;
        /** Holds recent folder for optical-disks. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentFolderCD;
        /** Holds recent folder for floppy-disks. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentFolderFD;
        /** Holds recent folder for VISO creation content. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentFolderVISOContent;
        /** Holds the list of recently used hard-drives. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentListHD;
        /** Holds the list of recently used optical-disks. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentListCD;
        /** Holds the list of recently used floppy-disks. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RecentListFD;
    /** @} */

    /** @name VirtualBox Manager
      * @{ */
        /** Holds selector-window geometry. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastSelectorWindowPosition;
        /** Holds selector-window splitter hints. */
        SHARED_LIBRARY_STUFF extern const char *GUI_SplitterSizes;
        /** Holds whether selector-window tool-bar visible. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Toolbar;
        /** Holds whether selector-window tool-bar text visible. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Toolbar_Text;
        /** Holds the selector-window machine tools order. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Toolbar_MachineTools_Order;
        /** Holds the selector-window global tools order. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Toolbar_GlobalTools_Order;
        /** Holds the last selected tool set of VirtualBox Manager. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Tools_LastItemsSelected;
        /** Holds whether selector-window status-bar visible. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Statusbar;
        /** Prefix used by composite extra-data keys,
          * which holds selector-window chooser-pane' groups definitions. */
        SHARED_LIBRARY_STUFF extern const char *GUI_GroupDefinitions;
        /** Holds last item chosen in selector-window chooser-pane. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastItemSelected;
        /** Holds selector-window details-pane' elements. */
        SHARED_LIBRARY_STUFF extern const char *GUI_DetailsPageBoxes;
        /** Holds selector-window details-pane' preview update interval. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PreviewUpdate;
    /** @} */

    /** @name Snapshot Manager
      * @{ */
        /** Holds whether Snapshot Manager details expanded. */
        SHARED_LIBRARY_STUFF extern const char *GUI_SnapshotManager_Details_Expanded;
    /** @} */

    /** @name Virtual Media Manager
      * @{ */
        /** Holds whether Virtual Media Manager details expanded. */
        SHARED_LIBRARY_STUFF extern const char *GUI_VirtualMediaManager_Details_Expanded;
    /** @} */

    /** @name Host Network Manager
      * @{ */
        /** Holds whether Host Network Manager details expanded. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HostNetworkManager_Details_Expanded;
    /** @} */

#ifdef VBOX_GUI_WITH_EXTRADATA_MANAGER_UI
    /** @name Extra-data Manager
      * @{ */
        /** Holds extra-data manager geometry. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ExtraDataManager_Geometry;
        /** Holds extra-data manager splitter hints. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ExtraDataManager_SplitterHints;
    /** @} */
#endif /* VBOX_GUI_WITH_EXTRADATA_MANAGER_UI */

    /** @name Wizards
      * @{ */
        /** Holds wizard types for which descriptions should be hidden. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HideDescriptionForWizards;
    /** @} */

    /** @name Virtual Machine
      * @{ */
        /** Holds whether machine shouldn't be shown in selector-window chooser-pane. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HideFromManager;
        /** Holds whether machine shouldn't be shown in selector-window details-pane. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HideDetails;
        /** Holds whether machine reconfiguration disabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PreventReconfiguration;
        /** Holds whether machine snapshot operations disabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PreventSnapshotOperations;
        /** Holds whether this machine is first time started. */
        SHARED_LIBRARY_STUFF extern const char *GUI_FirstRun;
        /** Except Mac OS X: Holds redefined machine-window icon names. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MachineWindowIcons;
#ifndef VBOX_WS_MAC
        /** Except Mac OS X: Holds redefined machine-window name postfix. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MachineWindowNamePostfix;
#endif
        /** Prefix used by composite extra-data keys,
          * which holds normal machine-window geometry per screen-index. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastNormalWindowPosition;
        /** Prefix used by composite extra-data keys,
          * which holds scaled machine-window geometry per screen-index. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastScaleWindowPosition;
        /** Holds machine-window geometry maximized state flag. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Geometry_State_Max;
#ifndef VBOX_WS_MAC
        /** Holds Runtime UI menu-bar availability status. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MenuBar_Enabled;
#endif
        /** Holds Runtime UI menu-bar context-menu availability status. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MenuBar_ContextMenu_Enabled;
        /** Holds restricted Runtime UI menu types. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeMenus;
        /** Holds restricted Runtime UI action types for 'Application' menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeApplicationMenuActions;
        /** Holds restricted Runtime UI action types for Machine menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeMachineMenuActions;
        /** Holds restricted Runtime UI action types for View menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeViewMenuActions;
        /** Holds restricted Runtime UI action types for Input menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeInputMenuActions;
        /** Holds restricted Runtime UI action types for Devices menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeDevicesMenuActions;
#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Holds restricted Runtime UI action types for Debugger menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeDebuggerMenuActions;
#endif
#ifdef VBOX_WS_MAC
        /** Mac OS X: Holds restricted Runtime UI action types for 'Window' menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeWindowMenuActions;
#endif
        /** Holds restricted Runtime UI action types for Help menu. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedRuntimeHelpMenuActions;
        /** Holds restricted Runtime UI visual-states. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedVisualStates;
        /** Holds whether full screen visual-state is requested. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Fullscreen;
        /** Holds whether seamless visual-state is requested. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Seamless;
        /** Holds whether scaled visual-state is requested. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Scale;
#ifdef VBOX_WS_X11
        /** Holds whether legacy full-screen mode is requested. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Fullscreen_LegacyMode;
        /** Holds whether internal machine-window names should be unique. */
        SHARED_LIBRARY_STUFF extern const char *GUI_DistinguishMachineWindowGroups;
#endif /* VBOX_WS_X11 */
        /** Holds whether guest-screen auto-resize according machine-window size is enabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_AutoresizeGuest;
        /** Prefix used by composite extra-data keys,
          * which holds last guest-screen visibility status per screen-index. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastVisibilityStatusForGuestScreen;
        /** Prefix used by composite extra-data keys,
          * which holds last guest-screen size-hint per screen-index. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastGuestSizeHint;
        /** Prefix used by composite extra-data keys,
          * which holds host-screen index per guest-screen index. */
        SHARED_LIBRARY_STUFF extern const char *GUI_VirtualScreenToHostScreen;
        /** Holds whether automatic mounting/unmounting of guest-screens enabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_AutomountGuestScreens;
#ifdef VBOX_WITH_VIDEOHWACCEL
        /** Holds whether 2D acceleration should use linear sretch. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Accelerate2D_StretchLinear;
        /** Holds whether 2D acceleration should use YV12 pixel format. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Accelerate2D_PixformatYV12;
        /** Holds whether 2D acceleration should use UYVY pixel format. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Accelerate2D_PixformatUYVY;
        /** Holds whether 2D acceleration should use YUY2 pixel format. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Accelerate2D_PixformatYUY2;
        /** Holds whether 2D acceleration should use AYUV pixel format. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Accelerate2D_PixformatAYUV;
#endif /* VBOX_WITH_VIDEOHWACCEL */
#ifndef VBOX_WS_MAC
        /** Holds whether mini-toolbar is enabled for full and seamless screens. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ShowMiniToolBar;
        /** Holds whether mini-toolbar should auto-hide itself. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MiniToolBarAutoHide;
        /** Holds mini-toolbar alignment. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MiniToolBarAlignment;
#endif /* !VBOX_WS_MAC */
        /** Holds Runtime UI status-bar availability status. */
        SHARED_LIBRARY_STUFF extern const char *GUI_StatusBar_Enabled;
        /** Holds Runtime UI status-bar context-menu availability status. */
        SHARED_LIBRARY_STUFF extern const char *GUI_StatusBar_ContextMenu_Enabled;
        /** Holds restricted Runtime UI status-bar indicators. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedStatusBarIndicators;
        /** Holds Runtime UI status-bar indicator order. */
        SHARED_LIBRARY_STUFF extern const char *GUI_StatusBar_IndicatorOrder;
#ifdef VBOX_WS_MAC
        /** Mac OS X: Holds whether Dock icon should be updated at runtime. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RealtimeDockIconUpdateEnabled;
        /** Mac OS X: Holds guest-screen which Dock icon should reflect at runtime. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RealtimeDockIconUpdateMonitor;
        /** Mac OS X: Holds whether Dock icon should have overlay disabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_DockIconDisableOverlay;
#endif /* VBOX_WS_MAC */
        /** Holds whether machine should pass CAD to guest. */
        SHARED_LIBRARY_STUFF extern const char *GUI_PassCAD;
        /** Holds the mouse capture policy. */
        SHARED_LIBRARY_STUFF extern const char *GUI_MouseCapturePolicy;
        /** Holds redefined guru-meditation handler type. */
        SHARED_LIBRARY_STUFF extern const char *GUI_GuruMeditationHandler;
        /** Holds whether machine should perform HID LEDs synchronization. */
        SHARED_LIBRARY_STUFF extern const char *GUI_HidLedsSync;
        /** Holds the scale-factor. */
        SHARED_LIBRARY_STUFF extern const char *GUI_ScaleFactor;
        /** Holds the scaling optimization type. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Scaling_Optimization;
    /** @} */

    /** @name Virtual Machine: Information dialog
      * @{ */
        /** Holds information-window geometry. */
        SHARED_LIBRARY_STUFF extern const char *GUI_InformationWindowGeometry;
        /** Holds information-window elements. */
        SHARED_LIBRARY_STUFF extern const char *GUI_InformationWindowElements;
        extern const char *GUI_GuestSessionManagerTabSplitterHints;
        extern const char *GUI_GuestFileManagerTabSplitterHints;
    /** @} */

    /** @name Virtual Machine: Close dialog
      * @{ */
        /** Holds default machine close action. */
        SHARED_LIBRARY_STUFF extern const char *GUI_DefaultCloseAction;
        /** Holds restricted machine close actions. */
        SHARED_LIBRARY_STUFF extern const char *GUI_RestrictedCloseActions;
        /** Holds last machine close action. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LastCloseAction;
        /** Holds machine close hook script name as simple string. */
        SHARED_LIBRARY_STUFF extern const char *GUI_CloseActionHook;
    /** @} */

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** @name Virtual Machine: Debug UI
      * @{ */
        /** Holds whether debugger UI enabled. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Dbg_Enabled;
        /** Holds whether debugger UI should be auto-shown. */
        SHARED_LIBRARY_STUFF extern const char *GUI_Dbg_AutoShow;
    /** @} */
#endif /* VBOX_WITH_DEBUGGER_GUI */

    /** @name Virtual Machine: Log-viewer
      * @{ */
        /** Holds log-viewer geometry. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LogWindowGeometry;
    /** @} */
    /** @name Virtual Machine: Log-viewer widget settings
      * @{ */
        SHARED_LIBRARY_STUFF extern const char *GUI_LogViewerSettings;
        /** Holds log-viewer wrap line flag. */
        SHARED_LIBRARY_STUFF extern const char *GUI_LogViewerWrapLinesEnabled;
        SHARED_LIBRARY_STUFF extern const char *GUI_LogViewerShowLineNumbersDisabled;
        SHARED_LIBRARY_STUFF extern const char *GUI_LogViewerNoFontStyleName;
    /** @} */

}


/** Extra-data meta definitions. */
class SHARED_LIBRARY_STUFF UIExtraDataMetaDefs : public QObject
{
    Q_OBJECT;
    Q_ENUMS(MenuType);
    Q_ENUMS(MenuApplicationActionType);
    Q_ENUMS(MenuHelpActionType);
    Q_ENUMS(RuntimeMenuMachineActionType);
    Q_ENUMS(RuntimeMenuViewActionType);
    Q_ENUMS(RuntimeMenuInputActionType);
    Q_ENUMS(RuntimeMenuDevicesActionType);
#ifdef VBOX_WITH_DEBUGGER_GUI
    Q_ENUMS(RuntimeMenuDebuggerActionType);
#endif
#ifdef VBOX_WS_MAC
    Q_ENUMS(MenuWindowActionType);
#endif

public:

    /** Common UI: Menu types. */
    enum MenuType
    {
        MenuType_Invalid     = 0,
        MenuType_Application = RT_BIT(0),
        MenuType_Machine     = RT_BIT(1),
        MenuType_View        = RT_BIT(2),
        MenuType_Input       = RT_BIT(3),
        MenuType_Devices     = RT_BIT(4),
#ifdef VBOX_WITH_DEBUGGER_GUI
        MenuType_Debug       = RT_BIT(5),
#endif
#ifdef VBOX_WS_MAC
        MenuType_Window      = RT_BIT(6),
#endif
        MenuType_Help        = RT_BIT(7),
        MenuType_All         = 0xFF
    };

    /** Menu "Application": Action types. */
    enum MenuApplicationActionType
    {
        MenuApplicationActionType_Invalid              = 0,
#ifdef VBOX_WS_MAC
        MenuApplicationActionType_About                = RT_BIT(0),
#endif
        MenuApplicationActionType_Preferences          = RT_BIT(1),
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
        MenuApplicationActionType_NetworkAccessManager = RT_BIT(2),
        MenuApplicationActionType_CheckForUpdates      = RT_BIT(3),
#endif
        MenuApplicationActionType_ResetWarnings        = RT_BIT(4),
        MenuApplicationActionType_Close                = RT_BIT(5),
        MenuApplicationActionType_All                  = 0xFFFF
    };

    /** Menu "Help": Action types. */
    enum MenuHelpActionType
    {
        MenuHelpActionType_Invalid              = 0,
        MenuHelpActionType_Contents             = RT_BIT(0),
        MenuHelpActionType_WebSite              = RT_BIT(1),
        MenuHelpActionType_BugTracker           = RT_BIT(2),
        MenuHelpActionType_Forums               = RT_BIT(3),
        MenuHelpActionType_Oracle               = RT_BIT(4),
#ifndef VBOX_WS_MAC
        MenuHelpActionType_About                = RT_BIT(5),
#endif
        MenuHelpActionType_All                  = 0xFFFF
    };

    /** Runtime UI: Menu "Machine": Action types. */
    enum RuntimeMenuMachineActionType
    {
        RuntimeMenuMachineActionType_Invalid           = 0,
        RuntimeMenuMachineActionType_SettingsDialog    = RT_BIT(0),
        RuntimeMenuMachineActionType_TakeSnapshot      = RT_BIT(1),
        RuntimeMenuMachineActionType_InformationDialog = RT_BIT(2),
        RuntimeMenuMachineActionType_Pause             = RT_BIT(3),
        RuntimeMenuMachineActionType_Reset             = RT_BIT(4),
        RuntimeMenuMachineActionType_Detach            = RT_BIT(5),
        RuntimeMenuMachineActionType_SaveState         = RT_BIT(6),
        RuntimeMenuMachineActionType_Shutdown          = RT_BIT(7),
        RuntimeMenuMachineActionType_PowerOff          = RT_BIT(8),
        RuntimeMenuMachineActionType_Nothing           = RT_BIT(9),
        RuntimeMenuMachineActionType_All               = 0xFFFF
    };

    /** Runtime UI: Menu "View": Action types. */
    enum RuntimeMenuViewActionType
    {
        RuntimeMenuViewActionType_Invalid              = 0,
        RuntimeMenuViewActionType_Fullscreen           = RT_BIT(0),
        RuntimeMenuViewActionType_Seamless             = RT_BIT(1),
        RuntimeMenuViewActionType_Scale                = RT_BIT(2),
#ifndef VBOX_WS_MAC
        RuntimeMenuViewActionType_MinimizeWindow       = RT_BIT(3),
#endif
        RuntimeMenuViewActionType_AdjustWindow         = RT_BIT(4),
        RuntimeMenuViewActionType_GuestAutoresize      = RT_BIT(5),
        RuntimeMenuViewActionType_TakeScreenshot       = RT_BIT(6),
        RuntimeMenuViewActionType_VideoCapture         = RT_BIT(7),
        RuntimeMenuViewActionType_VideoCaptureSettings = RT_BIT(8),
        RuntimeMenuViewActionType_StartVideoCapture    = RT_BIT(9),
        RuntimeMenuViewActionType_VRDEServer           = RT_BIT(10),
        RuntimeMenuViewActionType_MenuBar              = RT_BIT(11),
        RuntimeMenuViewActionType_MenuBarSettings      = RT_BIT(12),
#ifndef VBOX_WS_MAC
        RuntimeMenuViewActionType_ToggleMenuBar        = RT_BIT(13),
#endif
        RuntimeMenuViewActionType_StatusBar            = RT_BIT(14),
        RuntimeMenuViewActionType_StatusBarSettings    = RT_BIT(15),
        RuntimeMenuViewActionType_ToggleStatusBar      = RT_BIT(16),
        RuntimeMenuViewActionType_ScaleFactor          = RT_BIT(17),
        RuntimeMenuViewActionType_Resize               = RT_BIT(18),
        RuntimeMenuViewActionType_Multiscreen          = RT_BIT(19),
        RuntimeMenuViewActionType_All                  = 0xFFFF
    };

    /** Runtime UI: Menu "Input": Action types. */
    enum RuntimeMenuInputActionType
    {
        RuntimeMenuInputActionType_Invalid            = 0,
        RuntimeMenuInputActionType_Keyboard           = RT_BIT(0),
        RuntimeMenuInputActionType_KeyboardSettings   = RT_BIT(1),
        RuntimeMenuInputActionType_TypeCAD            = RT_BIT(2),
#ifdef VBOX_WS_X11
        RuntimeMenuInputActionType_TypeCABS           = RT_BIT(3),
#endif
        RuntimeMenuInputActionType_TypeCtrlBreak      = RT_BIT(4),
        RuntimeMenuInputActionType_TypeInsert         = RT_BIT(5),
        RuntimeMenuInputActionType_TypePrintScreen    = RT_BIT(6),
        RuntimeMenuInputActionType_TypeAltPrintScreen = RT_BIT(7),
        RuntimeMenuInputActionType_Mouse              = RT_BIT(8),
        RuntimeMenuInputActionType_MouseIntegration   = RT_BIT(9),
        RuntimeMenuInputActionType_TypeHostKeyCombo   = RT_BIT(10),
        RuntimeMenuInputActionType_All                = 0xFFFF
    };

    /** Runtime UI: Menu "Devices": Action types. */
    enum RuntimeMenuDevicesActionType
    {
        RuntimeMenuDevicesActionType_Invalid               = 0,
        RuntimeMenuDevicesActionType_HardDrives            = RT_BIT(0),
        RuntimeMenuDevicesActionType_HardDrivesSettings    = RT_BIT(1),
        RuntimeMenuDevicesActionType_OpticalDevices        = RT_BIT(2),
        RuntimeMenuDevicesActionType_FloppyDevices         = RT_BIT(3),
        RuntimeMenuDevicesActionType_Audio                 = RT_BIT(4),
        RuntimeMenuDevicesActionType_AudioOutput           = RT_BIT(5),
        RuntimeMenuDevicesActionType_AudioInput            = RT_BIT(6),
        RuntimeMenuDevicesActionType_Network               = RT_BIT(7),
        RuntimeMenuDevicesActionType_NetworkSettings       = RT_BIT(8),
        RuntimeMenuDevicesActionType_USBDevices            = RT_BIT(9),
        RuntimeMenuDevicesActionType_USBDevicesSettings    = RT_BIT(10),
        RuntimeMenuDevicesActionType_WebCams               = RT_BIT(11),
        RuntimeMenuDevicesActionType_SharedClipboard       = RT_BIT(12),
        RuntimeMenuDevicesActionType_DragAndDrop           = RT_BIT(13),
        RuntimeMenuDevicesActionType_SharedFolders         = RT_BIT(14),
        RuntimeMenuDevicesActionType_SharedFoldersSettings = RT_BIT(15),
        RuntimeMenuDevicesActionType_InstallGuestTools     = RT_BIT(16),
        RuntimeMenuDevicesActionType_Nothing               = RT_BIT(17),
        RuntimeMenuDevicesActionType_All                   = 0xFFFF
    };

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Runtime UI: Menu "Debugger": Action types. */
    enum RuntimeMenuDebuggerActionType
    {
        RuntimeMenuDebuggerActionType_Invalid     = 0,
        RuntimeMenuDebuggerActionType_Statistics  = RT_BIT(0),
        RuntimeMenuDebuggerActionType_CommandLine = RT_BIT(1),
        RuntimeMenuDebuggerActionType_Logging     = RT_BIT(2),
        RuntimeMenuDebuggerActionType_LogDialog   = RT_BIT(3),
        RuntimeMenuDebuggerActionType_All         = 0xFFFF
    };
#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef VBOX_WS_MAC
    /** Menu "Window": Action types. */
    enum MenuWindowActionType
    {
        MenuWindowActionType_Invalid  = 0,
        MenuWindowActionType_Minimize = RT_BIT(0),
        MenuWindowActionType_Switch   = RT_BIT(1),
        MenuWindowActionType_All      = 0xFFFF
    };
#endif /* VBOX_WS_MAC */
};


/** Common UI: Event handling types. */
enum EventHandlingType
{
    EventHandlingType_Active,
    EventHandlingType_Passive
};


/** Common UI: GUI feature types. */
enum GUIFeatureType
{
    GUIFeatureType_None        = 0,
    GUIFeatureType_NoSelector  = RT_BIT(0),
    GUIFeatureType_NoMenuBar   = RT_BIT(1),
    GUIFeatureType_NoStatusBar = RT_BIT(2),
    GUIFeatureType_All         = 0xFF
};


/** Common UI: Global settings page types. */
enum GlobalSettingsPageType
{
    GlobalSettingsPageType_Invalid,
    GlobalSettingsPageType_General,
    GlobalSettingsPageType_Input,
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    GlobalSettingsPageType_Update,
#endif
    GlobalSettingsPageType_Language,
    GlobalSettingsPageType_Display,
    GlobalSettingsPageType_Network,
    GlobalSettingsPageType_Extensions,
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
    GlobalSettingsPageType_Proxy,
#endif
    GlobalSettingsPageType_Max
};
Q_DECLARE_METATYPE(GlobalSettingsPageType);


/** Common UI: Machine settings page types. */
enum MachineSettingsPageType
{
    MachineSettingsPageType_Invalid,
    MachineSettingsPageType_General,
    MachineSettingsPageType_System,
    MachineSettingsPageType_Display,
    MachineSettingsPageType_Storage,
    MachineSettingsPageType_Audio,
    MachineSettingsPageType_Network,
    MachineSettingsPageType_Ports,
    MachineSettingsPageType_Serial,
    MachineSettingsPageType_USB,
    MachineSettingsPageType_SF,
    MachineSettingsPageType_Interface,
    MachineSettingsPageType_Max
};
Q_DECLARE_METATYPE(MachineSettingsPageType);


/** Common UI: Wizard types. */
enum WizardType
{
    WizardType_Invalid,
    WizardType_NewVM,
    WizardType_CloneVM,
    WizardType_ExportAppliance,
    WizardType_ImportAppliance,
    WizardType_FirstRun,
    WizardType_NewVD,
    WizardType_CloneVD
};


/** Common UI: Wizard modes. */
enum WizardMode
{
    WizardMode_Auto,
    WizardMode_Basic,
    WizardMode_Expert
};


/** Selector UI: Machine tool types. */
enum ToolTypeMachine
{
    ToolTypeMachine_Invalid,
    ToolTypeMachine_Desktop,
    ToolTypeMachine_Details,
    ToolTypeMachine_Snapshots,
    ToolTypeMachine_LogViewer
};
Q_DECLARE_METATYPE(ToolTypeMachine);


/** Selector UI: Global tool types. */
enum ToolTypeGlobal
{
    ToolTypeGlobal_Invalid,
    ToolTypeGlobal_Desktop,
    ToolTypeGlobal_VirtualMedia,
    ToolTypeGlobal_HostNetwork,
};
Q_DECLARE_METATYPE(ToolTypeGlobal);


/** Selector UI: Details-element types. */
enum DetailsElementType
{
    DetailsElementType_Invalid,
    DetailsElementType_General,
    DetailsElementType_System,
    DetailsElementType_Preview,
    DetailsElementType_Display,
    DetailsElementType_Storage,
    DetailsElementType_Audio,
    DetailsElementType_Network,
    DetailsElementType_Serial,
    DetailsElementType_USB,
    DetailsElementType_SF,
    DetailsElementType_UI,
    DetailsElementType_Description
};
Q_DECLARE_METATYPE(DetailsElementType);


/** Selector UI: Preview update interval types. */
enum PreviewUpdateIntervalType
{
    PreviewUpdateIntervalType_Disabled,
    PreviewUpdateIntervalType_500ms,
    PreviewUpdateIntervalType_1000ms,
    PreviewUpdateIntervalType_2000ms,
    PreviewUpdateIntervalType_5000ms,
    PreviewUpdateIntervalType_10000ms,
    PreviewUpdateIntervalType_Max
};


/** Runtime UI: Visual-state types. */
enum UIVisualStateType
{
    UIVisualStateType_Invalid    = 0,
    UIVisualStateType_Normal     = RT_BIT(0),
    UIVisualStateType_Fullscreen = RT_BIT(1),
    UIVisualStateType_Seamless   = RT_BIT(2),
    UIVisualStateType_Scale      = RT_BIT(3),
    UIVisualStateType_All        = 0xFF
};
Q_DECLARE_METATYPE(UIVisualStateType);


/** Runtime UI: Indicator types. */
enum IndicatorType
{
    IndicatorType_Invalid,
    IndicatorType_HardDisks,
    IndicatorType_OpticalDisks,
    IndicatorType_FloppyDisks,
    IndicatorType_Audio,
    IndicatorType_Network,
    IndicatorType_USB,
    IndicatorType_SharedFolders,
    IndicatorType_Display,
    IndicatorType_VideoCapture,
    IndicatorType_Features,
    IndicatorType_Mouse,
    IndicatorType_Keyboard,
    IndicatorType_KeyboardExtension,
    IndicatorType_Max
};
Q_DECLARE_METATYPE(IndicatorType);


/** Runtime UI: Machine close actions. */
enum MachineCloseAction
{
    MachineCloseAction_Invalid                    = 0,
    MachineCloseAction_Detach                     = RT_BIT(0),
    MachineCloseAction_SaveState                  = RT_BIT(1),
    MachineCloseAction_Shutdown                   = RT_BIT(2),
    MachineCloseAction_PowerOff                   = RT_BIT(3),
    MachineCloseAction_PowerOff_RestoringSnapshot = RT_BIT(4),
    MachineCloseAction_All                        = 0xFF
};
Q_DECLARE_METATYPE(MachineCloseAction);


/** Runtime UI: Mouse capture policy types. */
enum MouseCapturePolicy
{
    MouseCapturePolicy_Default,
    MouseCapturePolicy_HostComboOnly,
    MouseCapturePolicy_Disabled
};


/** Runtime UI: Guru Meditation handler types. */
enum GuruMeditationHandlerType
{
    GuruMeditationHandlerType_Default,
    GuruMeditationHandlerType_PowerOff,
    GuruMeditationHandlerType_Ignore
};


/** Runtime UI: Scaling optimization types. */
enum ScalingOptimizationType
{
    ScalingOptimizationType_None,
    ScalingOptimizationType_Performance
};


#ifndef VBOX_WS_MAC
/** Runtime UI: Mini-toolbar alignment. */
enum MiniToolbarAlignment
{
    MiniToolbarAlignment_Bottom,
    MiniToolbarAlignment_Top
};
#endif /* !VBOX_WS_MAC */


/** Runtime UI: Information-element types. */
enum InformationElementType
{
    InformationElementType_Invalid,
    InformationElementType_General,
    InformationElementType_System,
    InformationElementType_Preview,
    InformationElementType_Display,
    InformationElementType_Storage,
    InformationElementType_Audio,
    InformationElementType_Network,
    InformationElementType_Serial,
    InformationElementType_USB,
    InformationElementType_SharedFolders,
    InformationElementType_UI,
    InformationElementType_Description,
    InformationElementType_RuntimeAttributes,
    InformationElementType_StorageStatistics,
    InformationElementType_NetworkStatistics
};
Q_DECLARE_METATYPE(InformationElementType);


/** Runtime UI: Maximum guest-screen resolution policy types.
  * @note This policy determines which guest-screen resolutions we wish to
  *       handle. We also accept anything smaller than the current resolution. */
enum MaxGuestResolutionPolicy
{
    /** Anything at all. */
    MaxGuestResolutionPolicy_Any,
    /** Anything up to a fixed size. */
    MaxGuestResolutionPolicy_Fixed,
    /** Anything up to host-screen available space. */
    MaxGuestResolutionPolicy_Automatic
};


#ifdef VBOX_GUI_WITH_NEW_MANAGER
/** Tool item classes. */
enum UIToolsClass
{
    UIToolsClass_Invalid,
    UIToolsClass_Global,
    UIToolsClass_Machine
};


/** Tool item types. */
enum UIToolsType
{
    UIToolsType_Invalid,
    /* Global types: */
    UIToolsType_Media,
    UIToolsType_Network,
    /* Machine types: */
    UIToolsType_Details,
    UIToolsType_Snapshots,
    UIToolsType_Logs,
};
#endif /* VBOX_GUI_WITH_NEW_MANAGER */


#endif /* !___UIExtraDataDefs_h___ */
