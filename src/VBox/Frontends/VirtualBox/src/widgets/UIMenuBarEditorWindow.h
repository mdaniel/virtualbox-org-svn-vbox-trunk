/* $Id$ */
/** @file
 * VBox Qt GUI - UIMenuBarEditorWindow class declaration.
 */

/*
 * Copyright (C) 2014-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIMenuBarEditorWindow_h___
#define ___UIMenuBarEditorWindow_h___

/* Qt includes: */
#include <QMap>
#include <QUuid>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"
#include "UILibraryDefs.h"
#include "UISlidingToolBar.h"

/* Forward declarations: */
class QAction;
class QHBoxLayout;
class QMenu;
#ifndef VBOX_WS_MAC
class QCheckBox;
#endif
class QString;
class QWidget;
class QIToolButton;
class UIAction;
class UIActionPool;
class UIToolBar;
class UIMachineWindow;


/** UISlidingToolBar subclass
  * providing user with possibility to edit menu-bar layout. */
class SHARED_LIBRARY_STUFF UIMenuBarEditorWindow : public UISlidingToolBar
{
    Q_OBJECT;

public:

    /** Constructs sliding toolbar passing @a pParent to the base-class.
      * @param  pActionPool  Brings the action-pool reference for internal use. */
    UIMenuBarEditorWindow(UIMachineWindow *pParent, UIActionPool *pActionPool);
};


/** QWidget subclass
  * used as menu-bar editor widget. */
class SHARED_LIBRARY_STUFF UIMenuBarEditorWidget : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies about Cancel button click. */
    void sigCancelClicked();

public:

    /** Constructs menu-bar editor widget passing @a pParent to the base-class.
      * @param  fStartedFromVMSettings  Brings whether 'this' is a part of VM settings.
      * @param  strMachineID            Brings the machine ID to be used by the editor.
      * @param  pActionPool             Brings the action-pool to be used by the editor. */
    UIMenuBarEditorWidget(QWidget *pParent,
                          bool fStartedFromVMSettings = true,
                          const QUuid &aMachineID = QUuid(),
                          UIActionPool *pActionPool = 0);

    /** Returns the machine ID instance. */
    const QUuid &machineID() const { return m_uMachineID; }
    /** Defines the @a strMachineID instance. */
    void setMachineID(const QUuid &aMachineID);

    /** Returns the action-pool reference. */
    const UIActionPool *actionPool() const { return m_pActionPool; }
    /** Defines the @a pActionPool reference. */
    void setActionPool(UIActionPool *pActionPool);

#ifndef VBOX_WS_MAC
    /** Returns whether the menu-bar enabled. */
    bool isMenuBarEnabled() const;
    /** Defines whether the menu-bar @a fEnabled. */
    void setMenuBarEnabled(bool fEnabled);
#endif

    /** Returns the cached restrictions of menu-bar. */
    UIExtraDataMetaDefs::MenuType restrictionsOfMenuBar() const { return m_restrictionsOfMenuBar; }
    /** Returns the cached restrictions of menu 'Application'. */
    UIExtraDataMetaDefs::MenuApplicationActionType restrictionsOfMenuApplication() const { return m_restrictionsOfMenuApplication; }
    /** Returns the cached restrictions of menu 'Machine'. */
    UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictionsOfMenuMachine() const { return m_restrictionsOfMenuMachine; }
    /** Returns the cached restrictions of menu 'View'. */
    UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictionsOfMenuView() const { return m_restrictionsOfMenuView; }
    /** Returns the cached restrictions of menu 'Input'. */
    UIExtraDataMetaDefs::RuntimeMenuInputActionType restrictionsOfMenuInput() const { return m_restrictionsOfMenuInput; }
    /** Returns the cached restrictions of menu 'Devices'. */
    UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictionsOfMenuDevices() const { return m_restrictionsOfMenuDevices; }
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Returns the cached restrictions of menu 'Debug'. */
    UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restrictionsOfMenuDebug() const { return m_restrictionsOfMenuDebug; }
#endif
#ifdef VBOX_WS_MAC
    /** Returns the cached restrictions of menu 'Window'. */
    UIExtraDataMetaDefs::MenuWindowActionType restrictionsOfMenuWindow() const { return m_restrictionsOfMenuWindow; }
#endif
    /** Returns the cached restrictions of menu 'Help'. */
    UIExtraDataMetaDefs::MenuHelpActionType restrictionsOfMenuHelp() const { return m_restrictionsOfMenuHelp; }

    /** Defines the cached @a restrictions of menu-bar. */
    void setRestrictionsOfMenuBar(UIExtraDataMetaDefs::MenuType restrictions);
    /** Defines the cached @a restrictions of menu 'Application'. */
    void setRestrictionsOfMenuApplication(UIExtraDataMetaDefs::MenuApplicationActionType restrictions);
    /** Defines the cached @a restrictions of menu 'Machine'. */
    void setRestrictionsOfMenuMachine(UIExtraDataMetaDefs::RuntimeMenuMachineActionType restrictions);
    /** Defines the cached @a restrictions of menu 'View'. */
    void setRestrictionsOfMenuView(UIExtraDataMetaDefs::RuntimeMenuViewActionType restrictions);
    /** Defines the cached @a restrictions of menu 'Input'. */
    void setRestrictionsOfMenuInput(UIExtraDataMetaDefs::RuntimeMenuInputActionType restrictions);
    /** Defines the cached @a restrictions of menu 'Devices'. */
    void setRestrictionsOfMenuDevices(UIExtraDataMetaDefs::RuntimeMenuDevicesActionType restrictions);
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Defines the cached @a restrictions of menu 'Debug'. */
    void setRestrictionsOfMenuDebug(UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType restrictions);
#endif
#ifdef VBOX_WS_MAC
    /** Defines the cached @a restrictions of menu 'Window'. */
    void setRestrictionsOfMenuWindow(UIExtraDataMetaDefs::MenuWindowActionType restrictions);
#endif
    /** Defines the cached @a restrictions of menu 'Help'. */
    void setRestrictionsOfMenuHelp(UIExtraDataMetaDefs::MenuHelpActionType restrictions);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private slots:

    /** Handles configuration change. */
    void sltHandleConfigurationChange(const QUuid &aMachineID);

    /** Handles menu-bar menu click. */
    void sltHandleMenuBarMenuClick();

private:

    /** Prepare routine. */
    void prepare();

#ifdef VBOX_WS_MAC
    /** Prepare named menu routine. */
    QMenu *prepareNamedMenu(const QString &strName);
#endif
    /** Prepare copied menu routine. */
    QMenu *prepareCopiedMenu(const UIAction *pAction);
#if 0
    /** Prepare copied sub-menu routine. */
    QMenu *prepareCopiedSubMenu(QMenu *pMenu, const UIAction *pAction);
#endif
    /** Prepare named action routine. */
    QAction *prepareNamedAction(QMenu *pMenu, const QString &strName,
                                int iExtraDataID, const QString &strExtraDataID);
    /** Prepare copied action routine. */
    QAction *prepareCopiedAction(QMenu *pMenu, const UIAction *pAction);

    /** Prepare menus routine. */
    void prepareMenus();
    /** Prepare 'Application' menu routine. */
    void prepareMenuApplication();
    /** Prepare 'Machine' menu routine. */
    void prepareMenuMachine();
    /** Prepare 'View' menu routine. */
    void prepareMenuView();
    /** Prepare 'Input' menu routine. */
    void prepareMenuInput();
    /** Prepare 'Devices' menu routine. */
    void prepareMenuDevices();
#ifdef VBOX_WITH_DEBUGGER_GUI
    /** Prepare 'Debug' menu routine. */
    void prepareMenuDebug();
#endif
#ifdef VBOX_WS_MAC
    /** Prepare 'Window' menu routine. */
    void prepareMenuWindow();
#endif
    /** Prepare 'Help' menu routine. */
    void prepareMenuHelp();

    /** @name General
      * @{ */
        /** Holds whether 'this' is prepared. */
        bool                m_fPrepared;
        /** Holds whether 'this' is a part of VM settings. */
        bool                m_fStartedFromVMSettings;
        /** Holds the machine ID instance. */
        QUuid               m_uMachineID;
        /** Holds the action-pool reference. */
        const UIActionPool *m_pActionPool;
    /** @} */

    /** @name Contents
      * @{ */
        /** Holds the main-layout instance. */
        QHBoxLayout             *m_pMainLayout;
        /** Holds the tool-bar instance. */
        UIToolBar               *m_pToolBar;
        /** Holds the close-button instance. */
        QIToolButton            *m_pButtonClose;
#ifndef VBOX_WS_MAC
        /** Holds the enable-checkbox instance. */
        QCheckBox               *m_pCheckBoxEnable;
#endif
        /** Holds tool-bar action references. */
        QMap<QString, QAction*>  m_actions;
    /** @} */

    /** @name Contents: Restrictions
      * @{ */
        /** Holds the cached restrictions of menu-bar. */
        UIExtraDataMetaDefs::MenuType                       m_restrictionsOfMenuBar;
        /** Holds the cached restrictions of menu 'Application'. */
        UIExtraDataMetaDefs::MenuApplicationActionType      m_restrictionsOfMenuApplication;
        /** Holds the cached restrictions of menu 'Machine'. */
        UIExtraDataMetaDefs::RuntimeMenuMachineActionType   m_restrictionsOfMenuMachine;
        /** Holds the cached restrictions of menu 'View'. */
        UIExtraDataMetaDefs::RuntimeMenuViewActionType      m_restrictionsOfMenuView;
        /** Holds the cached restrictions of menu 'Input'. */
        UIExtraDataMetaDefs::RuntimeMenuInputActionType     m_restrictionsOfMenuInput;
        /** Holds the cached restrictions of menu 'Devices'. */
        UIExtraDataMetaDefs::RuntimeMenuDevicesActionType   m_restrictionsOfMenuDevices;
#ifdef VBOX_WITH_DEBUGGER_GUI
        /** Holds the cached restrictions of menu 'Debug'. */
        UIExtraDataMetaDefs::RuntimeMenuDebuggerActionType  m_restrictionsOfMenuDebug;
#endif
#ifdef VBOX_WS_MAC
        /** Holds the cached restrictions of menu 'Window'. */
        UIExtraDataMetaDefs::MenuWindowActionType           m_restrictionsOfMenuWindow;
#endif
        /** Holds the cached restrictions of menu 'Help'. */
        UIExtraDataMetaDefs::MenuHelpActionType             m_restrictionsOfMenuHelp;
    /** @} */
};


#endif /* !___UIMenuBarEditorWindow_h___ */
