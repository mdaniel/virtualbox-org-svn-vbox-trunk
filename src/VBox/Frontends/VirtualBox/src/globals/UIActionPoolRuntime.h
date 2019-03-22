/* $Id$ */
/** @file
 * VBox Qt GUI - UIActionPoolRuntime class declaration.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIActionPoolRuntime_h___
#define ___UIActionPoolRuntime_h___

/* Qt includes: */
#include <QList>
#include <QMap>

/* GUI includes: */
#include "UIActionPool.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QSize;


/** Runtime action-pool index enum.
  * Naming convention is following:
  * 1. Every menu index prepended with 'M',
  * 2. Every simple-action index prepended with 'S',
  * 3. Every toggle-action index presended with 'T',
  * 4. Every polymorphic-action index presended with 'P',
  * 5. Every sub-index contains full parent-index name. */
enum UIActionIndexRT
{
    /* 'Machine' menu actions: */
    UIActionIndexRT_M_Machine = UIActionIndex_Max + 1,
    UIActionIndexRT_M_Machine_S_Settings,
    UIActionIndexRT_M_Machine_S_TakeSnapshot,
    UIActionIndexRT_M_Machine_S_ShowInformation,
    UIActionIndexRT_M_Machine_S_ShowGuestControl,
    UIActionIndexRT_M_Machine_T_Pause,
    UIActionIndexRT_M_Machine_S_Reset,
    UIActionIndexRT_M_Machine_S_Detach,
    UIActionIndexRT_M_Machine_S_SaveState,
    UIActionIndexRT_M_Machine_S_Shutdown,
    UIActionIndexRT_M_Machine_S_PowerOff,

    /* 'View' menu actions: */
    UIActionIndexRT_M_View,
    UIActionIndexRT_M_ViewPopup,
    UIActionIndexRT_M_View_T_Fullscreen,
    UIActionIndexRT_M_View_T_Seamless,
    UIActionIndexRT_M_View_T_Scale,
#ifndef VBOX_WS_MAC
    UIActionIndexRT_M_View_S_MinimizeWindow,
#endif
    UIActionIndexRT_M_View_S_AdjustWindow,
    UIActionIndexRT_M_View_T_GuestAutoresize,
    UIActionIndexRT_M_View_S_TakeScreenshot,
    UIActionIndexRT_M_View_M_VideoCapture,
    UIActionIndexRT_M_View_M_VideoCapture_S_Settings,
    UIActionIndexRT_M_View_M_VideoCapture_T_Start,
    UIActionIndexRT_M_View_T_VRDEServer,
    UIActionIndexRT_M_View_M_MenuBar,
    UIActionIndexRT_M_View_M_MenuBar_S_Settings,
#ifndef VBOX_WS_MAC
    UIActionIndexRT_M_View_M_MenuBar_T_Visibility,
#endif
    UIActionIndexRT_M_View_M_StatusBar,
    UIActionIndexRT_M_View_M_StatusBar_S_Settings,
    UIActionIndexRT_M_View_M_StatusBar_T_Visibility,

    /* 'Input' menu actions: */
    UIActionIndexRT_M_Input,
    UIActionIndexRT_M_Input_M_Keyboard,
    UIActionIndexRT_M_Input_M_Keyboard_S_Settings,
    UIActionIndexRT_M_Input_M_Keyboard_S_TypeCAD,
#ifdef VBOX_WS_X11
    UIActionIndexRT_M_Input_M_Keyboard_S_TypeCABS,
#endif
    UIActionIndexRT_M_Input_M_Keyboard_S_TypeCtrlBreak,
    UIActionIndexRT_M_Input_M_Keyboard_S_TypeInsert,
    UIActionIndexRT_M_Input_M_Keyboard_S_TypePrintScreen,
    UIActionIndexRT_M_Input_M_Keyboard_S_TypeAltPrintScreen,
    UIActionIndexRT_M_Input_M_Keyboard_T_TypeHostKeyCombo,
    UIActionIndexRT_M_Input_M_Mouse,
    UIActionIndexRT_M_Input_M_Mouse_T_Integration,

    /* 'Devices' menu actions: */
    UIActionIndexRT_M_Devices,
    UIActionIndexRT_M_Devices_M_HardDrives,
    UIActionIndexRT_M_Devices_M_HardDrives_S_Settings,
    UIActionIndexRT_M_Devices_M_OpticalDevices,
    UIActionIndexRT_M_Devices_M_FloppyDevices,
    UIActionIndexRT_M_Devices_M_Audio,
    UIActionIndexRT_M_Devices_M_Audio_T_Output,
    UIActionIndexRT_M_Devices_M_Audio_T_Input,
    UIActionIndexRT_M_Devices_M_Network,
    UIActionIndexRT_M_Devices_M_Network_S_Settings,
    UIActionIndexRT_M_Devices_M_USBDevices,
    UIActionIndexRT_M_Devices_M_USBDevices_S_Settings,
    UIActionIndexRT_M_Devices_M_WebCams,
    UIActionIndexRT_M_Devices_M_SharedClipboard,
    UIActionIndexRT_M_Devices_M_DragAndDrop,
    UIActionIndexRT_M_Devices_M_SharedFolders,
    UIActionIndexRT_M_Devices_M_SharedFolders_S_Settings,
    UIActionIndexRT_M_Devices_S_InstallGuestTools,

#ifdef VBOX_WITH_DEBUGGER_GUI
    /* 'Debugger' menu actions: */
    UIActionIndexRT_M_Debug,
    UIActionIndexRT_M_Debug_S_ShowStatistics,
    UIActionIndexRT_M_Debug_S_ShowCommandLine,
    UIActionIndexRT_M_Debug_T_Logging,
    UIActionIndexRT_M_Debug_S_ShowLogDialog,
#endif

#ifdef VBOX_WS_MAC
    /* 'Dock' menu actions: */
    UIActionIndexRT_M_Dock,
    UIActionIndexRT_M_Dock_M_DockSettings,
    UIActionIndexRT_M_Dock_M_DockSettings_T_PreviewMonitor,
    UIActionIndexRT_M_Dock_M_DockSettings_T_DisableMonitor,
    UIActionIndexRT_M_Dock_M_DockSettings_T_DisableOverlay,
#endif

    /* Maximum index: */
    UIActionIndexRT_Max
};


/** UIActionPool extension
  * representing action-pool singleton for Runtime UI. */
class SHARED_LIBRARY_STUFF UIActionPoolRuntime : public UIActionPool
{
    Q_OBJECT;

signals:

    /** Notifies about 'View' : 'Virtual Screen #' menu : 'Toggle' action trigger. */
    void sigNotifyAboutTriggeringViewScreenToggle(int iGuestScreenIndex, bool fEnabled);
    /** Notifies about 'View' : 'Virtual Screen #' menu : 'Resize' action trigger. */
    void sigNotifyAboutTriggeringViewScreenResize(int iGuestScreenIndex, const QSize &size);
    /** Notifies about 'View' : 'Virtual Screen #' menu : 'Remap' action trigger. */
    void sigNotifyAboutTriggeringViewScreenRemap(int iGuestScreenIndex, int iHostScreenIndex);

public:

    /** Defines host-screen @a cCount. */
    void setHostScreenCount(int cCount);
    /** Defines guest-screen @a cCount. */
    void setGuestScreenCount(int cCount);

    /** Defines @a iGuestScreen @a size. */
    void setGuestScreenSize(int iGuestScreen, const QSize &size);
    /** Defines whether @a iGuestScreen is @a fVisible. */
    void setGuestScreenVisible(int iGuestScreen, bool fVisible);

    /** Defines whether guest supports graphics. */
    void setGuestSupportsGraphics(bool fSupports);

    /** Defines host-to-guest mapping scheme. */
    void setHostScreenForGuestScreenMap(const QMap<int, int> &map);

    /** Returns whether the action with passed @a type is allowed in the 'Machine' menu. */
    bool isAllowedInMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType type) const;
    /** Defines 'Machine' menu @a restriction for passed @a level. */
    void setRestrictionForMenuMachine(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuMachineActionType restriction);

    /** Returns whether the action with passed @a type is allowed in the 'View' menu. */
    bool isAllowedInMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType type) const;
    /** Defines 'View' menu @a restriction for passed @a level. */
    void setRestrictionForMenuView(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuViewActionType restriction);

    /** Returns whether the action with passed @a type is allowed in the 'Input' menu. */
    bool isAllowedInMenuInput(UIExtraDataMetaDefs::RuntimeMenuInputActionType type) const;
    /** Defines 'Input' menu @a restriction for passed @a level. */
    void setRestrictionForMenuInput(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuInputActionType restriction);

    /** Returns whether the action with passed @a type is allowed in the 'Devices' menu. */
    bool isAllowedInMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType type) const;
    /** Defines 'Devices' menu @a restriction for passed @a level. */
    void setRestrictionForMenuDevices(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restriction);

#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Returns whether the action with passed @a type is allowed in the 'Debug' menu. */
    bool isAllowedInMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType type) const;
    /** Defines 'Debug' menu @a restriction for passed @a level. */
    void setRestrictionForMenuDebugger(UIActionRestrictionLevel level, UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restriction);
#endif

protected slots:

    /** Handles configuration-change. */
    void sltHandleConfigurationChange(const QUuid &uMachineID);

    /** Handles 'View' : 'Scale Factor' menu : @a pAction trigger. */
    void sltHandleActionTriggerViewScaleFactor(QAction *pAction);

    /** Prepares 'View' : 'Virtual Screen #' menu (Normal, Scale). */
    void sltPrepareMenuViewScreen();
    /** Prepares 'View' : 'Virtual Screen #' menu (Fullscreen, Seamless). */
    void sltPrepareMenuViewMultiscreen();

    /** Handles 'View' : 'Virtual Screen #' menu : 'Toggle' action trigger. */
    void sltHandleActionTriggerViewScreenToggle();
    /** Handles 'View' : 'Virtual Screen #' menu : 'Resize' @a pAction trigger. */
    void sltHandleActionTriggerViewScreenResize(QAction *pAction);
    /** Handles 'View' : 'Virtual Screen #' menu : 'Remap' @a pAction trigger. */
    void sltHandleActionTriggerViewScreenRemap(QAction *pAction);

protected:

    /** Constructs action-pool.
      * @param  fTemporary  Brings whether this action-pool is temporary,
      *                     used to (re-)initialize shortcuts-pool. */
    UIActionPoolRuntime(bool fTemporary = false);

    /** Prepares pool. */
    virtual void preparePool() /* override */;
    /** Prepares connections. */
    virtual void prepareConnections() /* override */;

    /** Updates configuration. */
    virtual void updateConfiguration() /* override */;

    /** Updates menu. */
    virtual void updateMenu(int iIndex) /* override */;
    /** Updates menus. */
    virtual void updateMenus() /* override */;

    /** Updates 'Machine' menu. */
    void updateMenuMachine();
    /** Updates 'View' menu. */
    void updateMenuView();
    /** Updates 'View' : 'Popup' menu. */
    void updateMenuViewPopup();
    /** Updates 'View' : 'Video Capture' menu. */
    void updateMenuViewVideoCapture();
    /** Updates 'View' : 'Menu Bar' menu. */
    void updateMenuViewMenuBar();
    /** Updates 'View' : 'Status Bar' menu. */
    void updateMenuViewStatusBar();
    /** Updates 'View' : 'Scale Factor' menu. */
    void updateMenuViewScaleFactor(QMenu *pMenu);
    /** Updates 'View' : 'Virtual Screen #' @a pMenu (Normal, Scale). */
    void updateMenuViewScreen(QMenu *pMenu);
    /** Updates 'View' : 'Virtual Screen #' @a pMenu (Fullscreen, Seamless). */
    void updateMenuViewMultiscreen(QMenu *pMenu);
    /** Updates 'Input' menu. */
    void updateMenuInput();
    /** Updates 'Input' : 'Keyboard' menu. */
    void updateMenuInputKeyboard();
    /** Updates 'Input' : 'Mouse' menu. */
    void updateMenuInputMouse();
    /** Updates 'Devices' menu. */
    void updateMenuDevices();
    /** Updates 'Devices' : 'Hard Drives' menu. */
    void updateMenuDevicesHardDrives();
    /** Updates 'Devices' : 'Audio' menu. */
    void updateMenuDevicesAudio();
    /** Updates 'Devices' : 'Network' menu. */
    void updateMenuDevicesNetwork();
    /** Updates 'Devices' : 'USB' menu. */
    void updateMenuDevicesUSBDevices();
    /** Updates 'Devices' : 'Shared Folders' menu. */
    void updateMenuDevicesSharedFolders();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Updates 'Debug' menu. */
    void updateMenuDebug();
#endif

    /** Updates shortcuts. */
    virtual void updateShortcuts() /* override */;

    /** Returns extra-data ID to save keyboard shortcuts under. */
    virtual QString shortcutsExtraDataID() const /* override */;

    /** Returns the list of Runtime UI main menus. */
    virtual QList<QMenu*> menus() const /* override */ { return m_mainMenus; }

private:

    /** Holds the host-screen count. */
    int  m_cHostScreens;
    /** Holds the guest-screen count. */
    int  m_cGuestScreens;

    /** Holds the map of guest-screen sizes. */
    QMap<int, QSize>  m_mapGuestScreenSize;
    /** Holds the map of guest-screen visibility states. */
    QMap<int, bool>   m_mapGuestScreenIsVisible;

    /** Holds whether guest supports graphics. */
    bool  m_fGuestSupportsGraphics;

    /** Holds the host-to-guest mapping scheme. */
    QMap<int, int>  m_mapHostScreenForGuestScreen;

    /** Holds the list of Runtime UI main menus. */
    QList<QMenu*>  m_mainMenus;

    /** Holds restricted action types of the Machine menu. */
    QMap<UIActionRestrictionLevel, UIExtraDataMetaDefs::RuntimeMenuMachineActionType>   m_restrictedActionsMenuMachine;
    /** Holds restricted action types of the View menu. */
    QMap<UIActionRestrictionLevel, UIExtraDataMetaDefs::RuntimeMenuViewActionType>      m_restrictedActionsMenuView;
    /** Holds restricted action types of the Input menu. */
    QMap<UIActionRestrictionLevel, UIExtraDataMetaDefs::RuntimeMenuInputActionType>     m_restrictedActionsMenuInput;
    /** Holds restricted action types of the Devices menu. */
    QMap<UIActionRestrictionLevel, UIExtraDataMetaDefs::RuntimeMenuDevicesActionType>   m_restrictedActionsMenuDevices;
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Holds restricted action types of the Debugger menu. */
    QMap<UIActionRestrictionLevel, UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType>  m_restrictedActionsMenuDebug;
#endif

    /** Enables factory in base-class. */
    friend class UIActionPool;
};


#endif /* !___UIActionPoolRuntime_h___ */
