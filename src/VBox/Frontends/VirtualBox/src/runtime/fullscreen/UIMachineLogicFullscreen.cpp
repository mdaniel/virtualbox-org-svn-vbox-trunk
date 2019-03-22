/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineLogicFullscreen class implementation.
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

/* Qt includes: */
#include <QTimer>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMultiScreenLayout.h"
#include "UIShortcutPool.h"
#include "UIMachineView.h"
#include "QIMenu.h"
#ifdef VBOX_WS_MAC
# include "UICocoaApplication.h"
# include "UIExtraDataManager.h"
# include "VBoxUtils.h"
# include "UIFrameBuffer.h"
# include <Carbon/Carbon.h>
#endif /* VBOX_WS_MAC */


UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Fullscreen)
    , m_pPopupMenu(0)
#ifdef VBOX_WS_MAC
    , m_fScreensHaveSeparateSpaces(darwinScreensHaveSeparateSpaces())
#endif /* VBOX_WS_MAC */
{
    /* Create multiscreen layout: */
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Delete multiscreen layout: */
    delete m_pScreenLayout;
}

bool UIMachineLogicFullscreen::checkAvailability()
{
    /* Check if there is enough physical memory to enter fullscreen: */
    if (uisession()->isGuestSupportsGraphics())
    {
        quint64 availBits = machine().GetVRAMSize() /* VRAM */ * _1M /* MiB to bytes */ * 8 /* to bits */;
        quint64 usedBits = m_pScreenLayout->memoryRequirements();
        if (availBits < usedBits)
        {
            if (!msgCenter().cannotEnterFullscreenMode(0, 0, 0, (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M))
                return false;
        }
    }

    /* Show the info message. */
    const UIShortcut &shortcut =
            gShortcutPool->shortcut(actionPool()->shortcutsExtraDataID(),
                                    actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen)->shortcutExtraDataID());
    const QString strHotKey = QString("Host+%1").arg(shortcut.toString());
    if (!msgCenter().confirmGoingFullscreen(strHotKey))
        return false;

    return true;
}

Qt::WindowFlags UIMachineLogicFullscreen::windowFlags(ulong uScreenId) const
{
    Q_UNUSED(uScreenId);
#ifdef VBOX_WS_MAC
    return vboxGlobal().osRelease() <= MacOSXRelease_Lion ? Qt::FramelessWindowHint :
           uScreenId == 0 || screensHaveSeparateSpaces() ? Qt::Window : Qt::FramelessWindowHint;
#else /* !VBOX_WS_MAC */
    return Qt::FramelessWindowHint;
#endif /* !VBOX_WS_MAC */
}

void UIMachineLogicFullscreen::adjustMachineWindowsGeometry()
{
    LogRel(("GUI: UIMachineLogicFullscreen::adjustMachineWindowsGeometry\n"));

    /* Rebuild multi-screen layout: */
    m_pScreenLayout->rebuild();

#ifdef VBOX_WS_MAC
    /* For Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
    {
        /* Make sure all machine-window(s) have proper geometry: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
            pMachineWindow->showInNecessaryMode();
    }
    /* For ML and next revalidate native fullscreen: */
    else revalidateNativeFullScreen();
#else /* !VBOX_WS_MAC */
    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
#endif /* !VBOX_WS_MAC */
}

int UIMachineLogicFullscreen::hostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(iScreenId);
}

bool UIMachineLogicFullscreen::hasHostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hasHostScreenForGuestScreen(iScreenId);
}

#ifdef RT_OS_DARWIN
void UIMachineLogicFullscreen::sltHandleNativeFullscreenWillEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenWillEnter: "
            "Machine-window #%d will enter native fullscreen\n",
            (int)pMachineWindow->screenId()));
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenDidEnter: "
            "Machine-window #%d did enter native fullscreen\n",
            (int)pMachineWindow->screenId()));

    /* Add machine-window to corresponding set: */
    m_fullscreenMachineWindows.insert(pMachineWindow);
    AssertReturnVoid(m_fullscreenMachineWindows.contains(pMachineWindow));

    /* Rebuild multi-screen layout: */
    m_pScreenLayout->rebuild();
    /* Revalidate native fullscreen: */
    revalidateNativeFullScreen();
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenWillExit()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenWillExit: "
            "Machine-window #%d will exit native fullscreen\n",
            (int)pMachineWindow->screenId()));
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);

    /* Remove machine-window from corresponding set: */
    bool fResult = m_fullscreenMachineWindows.remove(pMachineWindow);
    AssertReturnVoid(!m_fullscreenMachineWindows.contains(pMachineWindow));

    /* We have same signal if window did fail to enter native fullscreen.
     * In that case window missed in m_fullscreenMachineWindows,
     * ignore this signal silently: */
    if (!fResult)
        return;

    /* If that window was invalidated: */
    if (m_invalidFullscreenMachineWindows.contains(pMachineWindow))
    {
        LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                "Machine-window #%d exited invalidated native fullscreen, revalidate it\n",
                (int)pMachineWindow->screenId()));

        /* Exclude machine-window from invalidation set: */
        m_invalidFullscreenMachineWindows.remove(pMachineWindow);
        AssertReturnVoid(!m_invalidFullscreenMachineWindows.contains(pMachineWindow));

        /* Rebuild multi-screen layout: */
        m_pScreenLayout->rebuild();
        /* Revalidate native fullscreen: */
        revalidateNativeFullScreen();
    }
    /* If there are no invalidated windows: */
    else if (m_invalidFullscreenMachineWindows.isEmpty())
    {
        /* If there are 'fullscreen' windows: */
        if (!m_fullscreenMachineWindows.isEmpty())
        {
            LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window #%d exited native fullscreen, asking others to exit too...\n",
                    (int)pMachineWindow->screenId()));

            /* Ask window(s) to exit 'fullscreen' mode: */
            emit sigNotifyAboutNativeFullscreenShouldBeExited();
        }
        /* If there are no 'fullscreen' windows: */
        else
        {
            LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window #%d exited native fullscreen, changing visual-state to requested...\n",
                    (int)pMachineWindow->screenId()));

            /* Change visual-state to requested: */
            UIVisualStateType type = uisession()->requestedVisualState();
            if (type == UIVisualStateType_Invalid)
                type = UIVisualStateType_Normal;
            uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
            uisession()->changeVisualState(type);
        }
    }
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertReturnVoid(pMachineWindow);

    /* Make sure this window is not registered somewhere: */
    AssertReturnVoid(!m_fullscreenMachineWindows.remove(pMachineWindow));
    AssertReturnVoid(!m_invalidFullscreenMachineWindows.remove(pMachineWindow));

    /* If there are 'fullscreen' windows: */
    if (!m_fullscreenMachineWindows.isEmpty())
    {
        LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter: "
                "Machine-window #%d failed to enter native fullscreen, asking others to exit...\n",
                (int)pMachineWindow->screenId()));

        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
    /* If there are no 'fullscreen' windows: */
    else
    {
        LogRel(("GUI: UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter: "
                "Machine-window #%d failed to enter native fullscreen, requesting change visual-state to normal...\n",
                (int)pMachineWindow->screenId()));

        /* Ask session to change 'fullscreen' mode to 'normal': */
        uisession()->setRequestedVisualState(UIVisualStateType_Normal);

        /* If session already initialized => push mode-change directly: */
        if (uisession()->isInitialized())
            sltCheckForRequestedVisualStateType();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToNormal()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToNormal();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'normal' (window) visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Normal);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToSeamless()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToSeamless();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'seamless' visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Seamless);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToScale()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToScale();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'scale' visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Scale);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltCheckForRequestedVisualStateType()
{
    LogRel(("GUI: UIMachineLogicFullscreen::sltCheckForRequestedVisualStateType: Requested-state=%d, Machine-state=%d\n",
            uisession()->requestedVisualState(), uisession()->machineState()));

    /* Do not try to change visual-state type if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* Do not try to change visual-state type in 'manual override' mode: */
    if (isManualOverrideMode())
        return;

    /* Check requested visual-state types: */
    switch (uisession()->requestedVisualState())
    {
        /* If 'normal' visual-state type is requested: */
        case UIVisualStateType_Normal:
        {
            LogRel(("GUI: UIMachineLogicFullscreen::sltCheckForRequestedVisualStateType: "
                    "Going 'normal' as requested...\n"));
            uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
            uisession()->changeVisualState(UIVisualStateType_Normal);
            break;
        }
        default:
            break;
    }
}
#endif /* RT_OS_DARWIN */

void UIMachineLogicFullscreen::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineLogic::sltMachineStateChanged();

    /* If machine-state changed from 'paused' to 'running': */
    if (uisession()->isRunning() && uisession()->wasPaused())
    {
        LogRel(("GUI: UIMachineLogicFullscreen::sltMachineStateChanged:"
                "Machine-state changed from 'paused' to 'running': "
                "Adjust machine-window geometry...\n"));

        /* Make sure further code will be called just once: */
        uisession()->forgetPreviousMachineState();
        /* Adjust machine-window geometry if necessary: */
        adjustMachineWindowsGeometry();
    }
}

void UIMachineLogicFullscreen::sltInvokePopupMenu()
{
    /* Popup main-menu if present: */
    if (m_pPopupMenu && !m_pPopupMenu->isEmpty())
    {
        m_pPopupMenu->popup(activeMachineWindow()->geometry().center());
        QTimer::singleShot(0, m_pPopupMenu, SLOT(sltHighlightFirstAction()));
    }
}

void UIMachineLogicFullscreen::sltScreenLayoutChanged()
{
    LogRel(("GUI: UIMachineLogicFullscreen::sltScreenLayoutChanged: Multi-screen layout changed\n"));

#ifdef VBOX_WS_MAC
    /* For Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
    {
        /* Make sure all machine-window(s) have proper geometry: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
            pMachineWindow->showInNecessaryMode();
    }
    /* Revalidate native fullscreen for ML and next: */
    else revalidateNativeFullScreen();
#else /* !VBOX_WS_MAC */
    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
#endif /* !VBOX_WS_MAC */
}

void UIMachineLogicFullscreen::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    LogRel(("GUI: UIMachineLogicFullscreen: Guest-screen count changed\n"));

    /* Rebuild multi-screen layout: */
    m_pScreenLayout->rebuild();

#ifdef VBOX_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
    /* Revalidate native fullscreen for ML and next: */
    else revalidateNativeFullScreen();
#else /* !VBOX_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
#endif /* !VBOX_WS_MAC */
}

void UIMachineLogicFullscreen::sltHostScreenCountChange()
{
    LogRel(("GUI: UIMachineLogicFullscreen: Host-screen count changed\n"));

    /* Rebuild multi-screen layout: */
    m_pScreenLayout->rebuild();

#ifdef VBOX_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltHostScreenCountChange();
    /* Revalidate native fullscreen for ML and next: */
    else revalidateNativeFullScreen();
#else /* !VBOX_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltHostScreenCountChange();
#endif /* !VBOX_WS_MAC */
}

void UIMachineLogicFullscreen::sltHostScreenAvailableAreaChange()
{
    LogRel2(("GUI: UIMachineLogicFullscreen: Host-screen available-area change ignored\n"));
}

void UIMachineLogicFullscreen::sltAdditionsStateChanged()
{
    /* Call to base-class: */
    UIMachineLogic::sltAdditionsStateChanged();

    LogRel(("GUI: UIMachineLogicFullscreen: Additions-state actual-change event, rebuild multi-screen layout\n"));
    /* Rebuild multi-screen layout: */
    m_pScreenLayout->rebuild();
}

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Restrict 'Adjust Window', 'Status Bar' and 'Resize' actions for 'View' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuView(UIActionRestrictionLevel_Logic,
                                                         (UIExtraDataMetaDefs::RuntimeMenuViewActionType)
                                                         (UIExtraDataMetaDefs::RuntimeMenuViewActionType_AdjustWindow |
                                                          UIExtraDataMetaDefs::RuntimeMenuViewActionType_MenuBar |
                                                          UIExtraDataMetaDefs::RuntimeMenuViewActionType_StatusBar |
                                                          UIExtraDataMetaDefs::RuntimeMenuViewActionType_Resize));
#ifdef VBOX_WS_MAC
    /* Restrict 'Window' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuBar(UIActionRestrictionLevel_Logic,
                                                        UIExtraDataMetaDefs::MenuType_Window);
#endif /* VBOX_WS_MAC */

    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen);
    if (!pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(true);
        pActionFullscreen->blockSignals(false);
    }
}

void UIMachineLogicFullscreen::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* Prepare 'View' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToNormal()));
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
}

void UIMachineLogicFullscreen::prepareMachineWindows()
{
    /* Do not create machine-window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef VBOX_WS_MAC
    /* For ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Register to native notifications: */
        UICocoaApplication::instance()->registerToNotificationOfWorkspace("NSWorkspaceDidActivateApplicationNotification", this,
                                                                          UIMachineLogicFullscreen::nativeHandlerForApplicationActivation);
        UICocoaApplication::instance()->registerToNotificationOfWorkspace("NSWorkspaceActiveSpaceDidChangeNotification", this,
                                                                          UIMachineLogicFullscreen::nativeHandlerForActiveSpaceChange);
    }

    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    darwinSetFrontMostProcess();
#endif /* VBOX_WS_MAC */

    /* Update the multi-screen layout: */
    m_pScreenLayout->update();

    /* Create machine-window(s): */
    for (uint cScreenId = 0; cScreenId < machine().GetMonitorCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, cScreenId));

    /* Listen for frame-buffer resize: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(pMachineWindow, SIGNAL(sigFrameBufferResize()),
                this, SIGNAL(sigFrameBufferResize()));
    emit sigFrameBufferResize();

    /* Connect multi-screen layout change handler: */
    connect(m_pScreenLayout, SIGNAL(sigScreenLayoutChange()),
            this, SLOT(sltScreenLayoutChanged()));

#ifdef VBOX_WS_MAC
    /* For ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Enable native fullscreen support: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
        {
            /* Logic => window signals: */
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeEntered(UIMachineWindow*)),
                    pMachineWindow, SLOT(sltEnterNativeFullscreen(UIMachineWindow*)));
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeExited(UIMachineWindow*)),
                    pMachineWindow, SLOT(sltExitNativeFullscreen(UIMachineWindow*)));
            /* Window => logic signals: */
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenWillEnter()),
                    this, SLOT(sltHandleNativeFullscreenWillEnter()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidEnter()),
                    this, SLOT(sltHandleNativeFullscreenDidEnter()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenWillExit()),
                    this, SLOT(sltHandleNativeFullscreenWillExit()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidExit()),
                    this, SLOT(sltHandleNativeFullscreenDidExit()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenFailToEnter()),
                    this, SLOT(sltHandleNativeFullscreenFailToEnter()),
                    Qt::QueuedConnection);
        }
        /* Revalidate native fullscreen: */
        revalidateNativeFullScreen();
    }
#endif /* VBOX_WS_MAC */

    /* Mark machine-window(s) created: */
    setMachineWindowsCreated(true);

#ifdef VBOX_WS_X11
    switch (vboxGlobal().typeOfWindowManager())
    {
        case X11WMType_GNOMEShell:
        case X11WMType_Mutter:
        {
            // WORKAROUND:
            // Under certain WMs we can loose machine-window activation due to any Qt::Tool
            // overlay asynchronously shown above it. Qt is not become aware of such event.
            // We are going to ask to return machine-window activation in let's say 100ms.
            QTimer::singleShot(100, machineWindows().first(), SLOT(sltActivateWindow()));
            break;
        }
        default:
            break;
    }
#endif /* VBOX_WS_X11 */
}

void UIMachineLogicFullscreen::prepareMenu()
{
    /* Prepare popup-menu: */
    m_pPopupMenu = new QIMenu;
    AssertPtrReturnVoid(m_pPopupMenu);
    {
        /* Prepare popup-menu: */
        foreach (QMenu *pMenu, actionPool()->menus())
            m_pPopupMenu->addMenu(pMenu);
    }
}

void UIMachineLogicFullscreen::cleanupMenu()
{
    /* Cleanup popup-menu: */
    delete m_pPopupMenu;
    m_pPopupMenu = 0;
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
{
    /* Do not destroy machine-window(s) if they destroyed already: */
    if (!isMachineWindowsCreated())
        return;

#ifdef VBOX_WS_MAC
    /* For ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Unregister from native notifications: */
        UICocoaApplication::instance()->unregisterFromNotificationOfWorkspace("NSWorkspaceDidActivateApplicationNotification", this);
        UICocoaApplication::instance()->unregisterFromNotificationOfWorkspace("NSWorkspaceActiveSpaceDidChangeNotification", this);
    }
#endif/* VBOX_WS_MAC */

    /* Mark machine-window(s) destroyed: */
    setMachineWindowsCreated(false);

    /* Destroy machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

void UIMachineLogicFullscreen::cleanupActionConnections()
{
    /* "View" actions disconnections: */
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToNormal()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

void UIMachineLogicFullscreen::cleanupActionGroups()
{
    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen);
    if (pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(false);
        pActionFullscreen->blockSignals(false);
    }

    /* Allow 'Adjust Window', 'Status Bar' and 'Resize' actions for 'View' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuView(UIActionRestrictionLevel_Logic,
                                                         UIExtraDataMetaDefs::RuntimeMenuViewActionType_Invalid);
#ifdef VBOX_WS_MAC
    /* Allow 'Window' menu: */
    actionPool()->toRuntime()->setRestrictionForMenuBar(UIActionRestrictionLevel_Logic,
                                                        UIExtraDataMetaDefs::MenuType_Invalid);
#endif /* VBOX_WS_MAC */

    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();
}

#ifdef VBOX_WS_MAC
void UIMachineLogicFullscreen::revalidateNativeFullScreen(UIMachineWindow *pMachineWindow)
{
    /* Make sure that is full-screen machine-window: */
    UIMachineWindowFullscreen *pMachineWindowFullscreen = qobject_cast<UIMachineWindowFullscreen*>(pMachineWindow);
    AssertPtrReturnVoid(pMachineWindowFullscreen);

    /* Make sure window is not already invalidated: */
    if (m_invalidFullscreenMachineWindows.contains(pMachineWindow))
        return;

    /* Ignore window if it is in 'fullscreen transition': */
    if (pMachineWindowFullscreen->isInFullscreenTransition())
        return;

    /* Get screen ID: */
    const ulong uScreenID = pMachineWindow->screenId();
    LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: For machine-window #%d\n",
            (int)uScreenID));

    /* Validate window which can't be fullscreen: */
    if (uScreenID != 0 && !screensHaveSeparateSpaces())
    {
        /* We are hiding transient window if:
         * 1. primary window is not on active user-space
         * 2. there is no fullscreen window or it's invalidated. */
        if (   !darwinIsOnActiveSpace(machineWindows().first())
            || m_fullscreenMachineWindows.isEmpty() || !m_invalidFullscreenMachineWindows.isEmpty())
        {
            LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                    "Ask transient machine-window #%d to hide\n", (int)uScreenID));

            /* Make sure window hidden: */
            pMachineWindow->hide();
        }
        /* If there is valid fullscreen window: */
        else
        {
            LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                    "Ask transient machine-window #%d to show/normalize\n", (int)uScreenID));

            /* Make sure window have proper geometry and shown: */
            pMachineWindow->showInNecessaryMode();
        }
    }
    /* Validate window which can be fullscreen: */
    else
    {
        /* Validate window which is not in fullscreen: */
        if (!darwinIsInFullscreenMode(pMachineWindow))
        {
            /* If that window
             * 1. should really be shown and
             * 2. is mapped to some host-screen: */
            if (   uisession()->isScreenVisible(uScreenID)
                && hasHostScreenForGuestScreen(uScreenID))
            {
                LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to enter native fullscreen\n", (int)uScreenID));

                /* Make sure window have proper geometry and shown: */
                pMachineWindow->showInNecessaryMode();

                /* Ask window to enter 'fullscreen' mode: */
                emit sigNotifyAboutNativeFullscreenShouldBeEntered(pMachineWindow);
            }
            /* If that window
             * is shown while shouldn't: */
            else if (pMachineWindow->isVisible())
            {
                LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to hide\n", (int)uScreenID));

                /* Make sure window hidden: */
                pMachineWindow->hide();
            }
        }
        /* Validate window which is in fullscreen: */
        else
        {
            /* Variables to compare: */
            const int iWantedHostScreenIndex = hostScreenForGuestScreen((int)uScreenID);
            const int iCurrentHostScreenIndex = gpDesktop->screenNumber(pMachineWindow);
            const QSize frameBufferSize((int)uisession()->frameBuffer(uScreenID)->width(), (int)uisession()->frameBuffer(uScreenID)->height());
            const QSize screenSize = gpDesktop->screenGeometry(iWantedHostScreenIndex).size();

            /* If that window
             * 1. shouldn't really be shown or
             * 2. isn't mapped to some host-screen or
             * 3. should be located on another host-screen than currently. */
            if (   !uisession()->isScreenVisible(uScreenID)
                || !hasHostScreenForGuestScreen(uScreenID)
                || iWantedHostScreenIndex != iCurrentHostScreenIndex)
            {
                LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to exit native fullscreen\n", (int)uScreenID));

                /* Mark window as invalidated: */
                m_invalidFullscreenMachineWindows << pMachineWindow;

                /* Ask window to exit 'fullscreen' mode: */
                emit sigNotifyAboutNativeFullscreenShouldBeExited(pMachineWindow);
                return;
            }

            /* If that window
             * 1. have another frame-buffer size than actually should. */
            else if (frameBufferSize != screenSize)
            {
                LogRel(("GUI: UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to adjust guest geometry\n", (int)uScreenID));

                /* Just adjust machine-view size if necessary: */
                pMachineWindow->adjustMachineViewSize();
                return;
            }
        }
    }
}

void UIMachineLogicFullscreen::revalidateNativeFullScreen()
{
    /* Revalidate all fullscreen windows: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        revalidateNativeFullScreen(pMachineWindow);
}

/* static */
void UIMachineLogicFullscreen::nativeHandlerForApplicationActivation(QObject *pObject, const QMap<QString, QString> &userInfo)
{
    /* Handle arrived notification: */
    UIMachineLogicFullscreen *pLogic = qobject_cast<UIMachineLogicFullscreen*>(pObject);
    AssertPtrReturnVoid(pLogic);
    {
        /* Redirect arrived notification: */
        pLogic->nativeHandlerForApplicationActivation(userInfo);
    }
}

void UIMachineLogicFullscreen::nativeHandlerForApplicationActivation(const QMap<QString, QString> &userInfo)
{
    /* Make sure we have BundleIdentifier key: */
    AssertReturnVoid(userInfo.contains("BundleIdentifier"));
    /* Skip other applications: */
    QStringList ourBundleIdentifiers;
    ourBundleIdentifiers << "org.virtualbox.app.VirtualBox";
    ourBundleIdentifiers << "org.virtualbox.app.VirtualBoxVM";
    ourBundleIdentifiers << "com.citrix.DesktopPlayerVM";
    if (!ourBundleIdentifiers.contains(userInfo.value("BundleIdentifier")))
        return;

    /* Skip if 'screen have separate spaces': */
    if (screensHaveSeparateSpaces())
        return;

    /* Skip if there is another than needed user-space is active: */
    if (!darwinIsOnActiveSpace(machineWindows().first()))
        return;

    LogRel(("GUI: UIMachineLogicFullscreen::nativeHandlerForApplicationActivation: "
            "Full-screen application activated\n"));

    /* Revalidate full-screen mode for transient machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        if (pMachineWindow->screenId() > 0)
            revalidateNativeFullScreen(pMachineWindow);
}

/* static */
void UIMachineLogicFullscreen::nativeHandlerForActiveSpaceChange(QObject *pObject, const QMap<QString, QString> &userInfo)
{
    /* Handle arrived notification: */
    UIMachineLogicFullscreen *pLogic = qobject_cast<UIMachineLogicFullscreen*>(pObject);
    AssertPtrReturnVoid(pLogic);
    {
        /* Redirect arrived notification: */
        pLogic->nativeHandlerForActiveSpaceChange(userInfo);
    }
}

void UIMachineLogicFullscreen::nativeHandlerForActiveSpaceChange(const QMap<QString, QString>&)
{
    /* Skip if 'screen have separate spaces': */
    if (screensHaveSeparateSpaces())
        return;

    /* Skip if there is another than needed user-space is active: */
    if (!darwinIsOnActiveSpace(machineWindows().first()))
        return;

    LogRel(("GUI: UIMachineLogicFullscreen::nativeHandlerForActiveSpaceChange: "
            "Full-screen user-space activated\n"));

    /* Revalidate full-screen mode for transient machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        if (pMachineWindow->screenId() > 0)
            revalidateNativeFullScreen(pMachineWindow);
}
#endif /* VBOX_WS_MAC */

