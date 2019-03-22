/* $Id$ */
/** @file
 * VBox Qt GUI - UIKeyboardHandler class implementation.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * Things worth testing when changing this code:
 * - That automatic keyboard capture works.
 * - That the keyboard is captured when the mouse is.
 * - That the host key releases the keyboard when
 *   the keyboard is captured but the mouse not, and both when both are.
 * - That the host key captures both keyboard and mouse.
 * - That the keyboard is captured when the mouse capture notification is
 *   displayed.
 * - That keyboard capture works on X11 hosts when windows are dragged with
 *   various window managers.
 * - That multiple machine windows do not fight for the focus on X11 hosts
 *   (noticeable through strange modifier key and capitals behaviour).
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QKeyEvent>
# include <QTimer>
# ifdef VBOX_WS_X11
#  include <QX11Info>
# endif

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UIMessageCenter.h"
# include "UIPopupCenter.h"
# include "UIActionPool.h"
# include "UISession.h"
# include "UIMachineLogic.h"
# include "UIMachineWindow.h"
# include "UIMachineView.h"
# include "UIHostComboEditor.h"
# include "UIKeyboardHandlerNormal.h"
# include "UIKeyboardHandlerFullscreen.h"
# include "UIKeyboardHandlerSeamless.h"
# include "UIKeyboardHandlerScale.h"
# include "UIMouseHandler.h"
# ifdef VBOX_WS_MAC
#  include "UICocoaApplication.h"
#  include "VBoxUtils-darwin.h"
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CKeyboard.h"

/* Other VBox includes: */
# ifdef VBOX_WS_MAC
#  include "iprt/cpp/utils.h"
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
#ifdef VBOX_WS_MAC
# include "DarwinKeyboard.h"
#endif /* VBOX_WS_MAC */
#ifdef VBOX_WS_WIN
# include "WinKeyboard.h"
#endif /* VBOX_WS_WIN */
#ifdef VBOX_WS_X11
# include "XKeyboard.h"
#endif /* VBOX_WS_X11 */

/* External includes: */
#ifdef VBOX_WS_MAC
# include <Carbon/Carbon.h>
#endif /* VBOX_WS_MAC */
#ifdef VBOX_WS_X11
# include <X11/XKBlib.h>
# include <X11/keysym.h>
# ifdef KeyPress
const int XFocusIn = FocusIn;
const int XFocusOut = FocusOut;
const int XKeyPress = KeyPress;
const int XKeyRelease = KeyRelease;
#  undef KeyRelease
#  undef KeyPress
#  undef FocusOut
#  undef FocusIn
# endif /* KeyPress */
# include <xcb/xcb.h>
#endif /* VBOX_WS_X11 */

/* Enums representing different keyboard-states: */
enum { KeyExtended = 0x01, KeyPressed = 0x02, KeyPause = 0x04, KeyPrint = 0x08 };
enum { IsKeyPressed = 0x01, IsExtKeyPressed = 0x02, IsKbdCaptured = 0x80 };


#ifdef VBOX_WS_WIN
UIKeyboardHandler* UIKeyboardHandler::m_spKeyboardHandler = 0;
#endif /* VBOX_WS_WIN */

/* Factory function to create keyboard-handler: */
UIKeyboardHandler* UIKeyboardHandler::create(UIMachineLogic *pMachineLogic,
                                             UIVisualStateType visualStateType)
{
    /* Prepare keyboard-handler: */
    UIKeyboardHandler *pKeyboardHandler = 0;

    /* Depending on visual-state type: */
    switch (visualStateType)
    {
        case UIVisualStateType_Normal:
            pKeyboardHandler = new UIKeyboardHandlerNormal(pMachineLogic);
            break;
        case UIVisualStateType_Fullscreen:
            pKeyboardHandler = new UIKeyboardHandlerFullscreen(pMachineLogic);
            break;
        case UIVisualStateType_Seamless:
            pKeyboardHandler = new UIKeyboardHandlerSeamless(pMachineLogic);
            break;
        case UIVisualStateType_Scale:
            pKeyboardHandler = new UIKeyboardHandlerScale(pMachineLogic);
            break;
        default:
            break;
    }

#ifdef VBOX_WS_WIN
    /* It is necessary to have static pointer to created handler
     * because windows keyboard-hook works only with static members: */
    m_spKeyboardHandler = pKeyboardHandler;
#endif /* VBOX_WS_WIN */

    /* Return prepared keyboard-handler: */
    return pKeyboardHandler;
}

/* Factory function to destroy keyboard-handler: */
void UIKeyboardHandler::destroy(UIKeyboardHandler *pKeyboardHandler)
{
#ifdef VBOX_WS_WIN
    /* It was necessary to have static pointer to created handler
     * because windows keyboard-hook works only with static members: */
    m_spKeyboardHandler = 0;
#endif /* VBOX_WS_WIN */

    /* Delete keyboard-handler: */
    delete pKeyboardHandler;
}

/* Prepare listened objects: */
void UIKeyboardHandler::prepareListener(ulong uScreenId, UIMachineWindow *pMachineWindow)
{
    /* If that window is NOT registered yet: */
    if (!m_windows.contains(uScreenId))
    {
        /* Add window: */
        m_windows.insert(uScreenId, pMachineWindow);
        /* Install event-filter for window: */
        m_windows[uScreenId]->installEventFilter(this);
    }

    /* If that view is NOT registered yet: */
    if (!m_views.contains(uScreenId))
    {
        /* Add view: */
        m_views.insert(uScreenId, pMachineWindow->machineView());
        /* Install event-filter for view: */
        m_views[uScreenId]->installEventFilter(this);
    }
}

/* Cleanup listened objects: */
void UIKeyboardHandler::cleanupListener(ulong uScreenId)
{
    /* Check if we should release keyboard first: */
    if ((int)uScreenId == m_iKeyboardCaptureViewIndex)
        releaseKeyboard();

    /* If window still registered: */
    if (m_windows.contains(uScreenId))
    {
        /* Remove window: */
        m_windows.remove(uScreenId);
    }

    /* If view still registered: */
    if (m_views.contains(uScreenId))
    {
        /* Remove view: */
        m_views.remove(uScreenId);
    }
}

void UIKeyboardHandler::captureKeyboard(ulong uScreenId)
{
    /* Do NOT capture the keyboard if it is already captured: */
    if (m_fIsKeyboardCaptured)
    {
        /* Make sure the right screen had captured the keyboard: */
        Assert((int)uScreenId == m_iKeyboardCaptureViewIndex);
        return;
    }

    /* If the view exists: */
    if (m_views.contains(uScreenId))
    {
        /* Remember which screen wishes to capture the keyboard: */
        m_iKeyboardCaptureViewIndex = uScreenId;

        /* On X11, we do not grab the keyboard as soon as it is captured, but delay it
         * for 300 milliseconds after the formal capture. We do this for several reasons:
         * - First, when several windows are created they all try to capture the keyboard when
         *   they get the focus. Due to the asynchronous nature of X11 the first window may only
         *   gets notified after the last is created, and there is a dance if they respond to
         *   the notifications by grabbing the keyboard and trigger new focus changes in the process.
         * - Second, grabbing the keyboard immediately on focus change upsets some window managers,
         *   they give us the focus then try to grab the keyboard themselves, and sulk if they fail
         *   by refusing to e.g. drag a window using its title bar.
         *
         * IMPORTANT! We do the same under all other hosts as well mainly to have the
         *            common behavior everywhere while X11 is forced to behave that way. */
        QTimer::singleShot(300, this, SLOT(sltFinaliseCaptureKeyboard()));
    }
}

bool UIKeyboardHandler::finaliseCaptureKeyboard()
{
    /* Do NOT capture the keyboard if it is already captured: */
    if (m_fIsKeyboardCaptured)
        return true;

    /* Make sure capture was really requested: */
    if (m_iKeyboardCaptureViewIndex == -1)
        return true;

    /* If the view exists: */
    if (m_views.contains(m_iKeyboardCaptureViewIndex))
    {
#if defined(VBOX_WS_MAC)

        /* On Mac, keyboard grabbing is ineffective, a low-level keyboard-hook is used instead.
         * It is being installed on focus-in event and uninstalled on focus-out.
         * S.a. UIKeyboardHandler::eventFilter for more information. */

        /* Besides that, we are not just using the Qt stuff to grab the keyboard,
         * we also disable global hot keys and enable watching
         * modifiers (for right/left separation). */
        /// @todo Is that really needed?
        ::DarwinDisableGlobalHotKeys(true);
        m_views[m_iKeyboardCaptureViewIndex]->grabKeyboard();

#elif defined(VBOX_WS_WIN)

        /* On Win, keyboard grabbing is ineffective, a low-level keyboard-hook is used instead.
         * It is being installed on focus-in event and uninstalled on focus-out.
         * S.a. UIKeyboardHandler::eventFilter for more information. */

#elif defined(VBOX_WS_X11)

        /* On X11, we are using XCB stuff to grab the keyboard.
         * This stuff is a part of the active keyboard grabbing functionality.
         * Active keyboard grabbing causes a problems on many window managers - a window cannot
         * be moved using the mouse. So we additionally grab the mouse buttons as well to detect
         * that the user is trying to click outside of the internal window geometry and release
         * the keyboard before the target window sees the click. (GNOME Shell's hot corner has
         * the same problem. At present we just let that problem be.) */

        /* Grab the mouse button.
         * We do not check for failure as we do not currently implement a back-up plan. */
        /* If any previous grab is still in process, release it. */
        if (m_hButtonGrabWindow != 0)
            xcb_ungrab_button_checked(QX11Info::connection(), XCB_BUTTON_INDEX_ANY,
                                      m_hButtonGrabWindow, XCB_MOD_MASK_ANY);
        m_hButtonGrabWindow = QX11Info::appRootWindow();
        xcb_grab_button_checked(QX11Info::connection(), 0, m_hButtonGrabWindow,
                                XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC,
                                XCB_NONE, XCB_NONE, XCB_BUTTON_INDEX_ANY, XCB_MOD_MASK_ANY);
        /* And grab the keyboard, using XCB directly, as Qt does not report failure. */
        xcb_grab_keyboard_cookie_t xcbGrabCookie = xcb_grab_keyboard(QX11Info::connection(), false, m_views[m_iKeyboardCaptureViewIndex]->winId(),
                                                                     XCB_TIME_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        xcb_grab_keyboard_reply_t *pGrabReply = xcb_grab_keyboard_reply(QX11Info::connection(), xcbGrabCookie, NULL);
        if (pGrabReply == NULL || pGrabReply->status != XCB_GRAB_STATUS_SUCCESS)
        {
            /* Release the mouse button grab.
             * We do not check for failure as we do not currently implement a back-up plan. */
            xcb_ungrab_button_checked(QX11Info::connection(), XCB_BUTTON_INDEX_ANY,
                                      m_hButtonGrabWindow, XCB_MOD_MASK_ANY);
            m_hButtonGrabWindow = 0;
            /* Try again later: */
            free(pGrabReply);
            return false;
        }
        free(pGrabReply);

#else

        /* On other platforms we are just praying Qt method to work: */
        m_views[m_iKeyboardCaptureViewIndex]->grabKeyboard();

#endif

        /* Store new keyboard-captured state value: */
        m_fIsKeyboardCaptured = true;

        /* Notify all the listeners: */
        emit sigStateChange(state());

        return true;
    }

    return false;
}

void UIKeyboardHandler::releaseKeyboard()
{
    /* Do NOT release the keyboard if it is already released: */
    if (!m_fIsKeyboardCaptured)
    {
        /* If a delayed capture is scheduled then cancel it: */
        m_iKeyboardCaptureViewIndex = -1;
        return;
    }

    /* If the view exists: */
    if (m_views.contains(m_iKeyboardCaptureViewIndex))
    {
#if defined(VBOX_WS_MAC)

        /* On Mac, keyboard grabbing is ineffective, a low-level keyboard-hook is used instead.
         * It is being installed on focus-in event and uninstalled on focus-out.
         * S.a. UIKeyboardHandler::eventFilter for more information. */

        /* Besides that, we are not just using the Qt stuff to ungrab the keyboard,
         * we also enable global hot keys and disable watching
         * modifiers (for right/left separation). */
        /// @todo Is that really needed?
        ::DarwinDisableGlobalHotKeys(false);
        m_views[m_iKeyboardCaptureViewIndex]->releaseKeyboard();

#elif defined(VBOX_WS_WIN)

        /* On Win, keyboard grabbing is ineffective, a low-level keyboard-hook is used instead.
         * It is being installed on focus-in event and uninstalled on focus-out.
         * S.a. UIKeyboardHandler::eventFilter for more information. */

#elif defined(VBOX_WS_X11)

        /* On X11, we are using XCB stuff to grab the keyboard.
         * This stuff is a part of the active keyboard grabbing functionality.
         * Active keyboard grabbing causes a problems on many window managers - a window cannot
         * be moved using the mouse. So we finally releasing additionally grabbed mouse as well
         * to allow further user interactions. */

        /* Ungrab using XCB: */
        xcb_ungrab_keyboard(QX11Info::connection(), XCB_TIME_CURRENT_TIME);
        /* Release the mouse button grab.
         * We do not check for failure as we do not currently implement a back-up plan. */
        xcb_ungrab_button_checked(QX11Info::connection(), XCB_BUTTON_INDEX_ANY,
                                  m_hButtonGrabWindow, XCB_MOD_MASK_ANY);
        m_hButtonGrabWindow = 0;

#else

        /* On other platforms we are just praying Qt method to work: */
        m_views[m_iKeyboardCaptureViewIndex]->releaseKeyboard();

#endif

        /* Forget which screen had captured the keyboard: */
        m_iKeyboardCaptureViewIndex = -1;

        /* Store new keyboard-captured state value: */
        m_fIsKeyboardCaptured = false;

        /* Notify all the listeners: */
        emit sigStateChange(state());
    }
}

void UIKeyboardHandler::releaseAllPressedKeys(bool aReleaseHostKey /* = true */)
{
    bool fSentRESEND = false;

    /* Send a dummy modifier sequence to prevent the guest OS from recognizing
     * a single key click (for ex., Alt) and performing an unwanted action
     * (for ex., activating the menu) when we release all pressed keys below.
     * This is just a work-around and is likely to fail in some cases.  We are
     * not aware of any ideal solution.  Historically we sent an 0xFE scan code,
     * but this is a real key release code on Brazilian keyboards. Now we send
     * a sequence of all modifier keys contained in the host sequence, hoping
     * that the user will choose something which the guest does not interpret. */
    for (uint i = 0; i < SIZEOF_ARRAY (m_pressedKeys); i++)
    {
        if ((m_pressedKeys[i] & IsKeyPressed) || (m_pressedKeys[i] & IsExtKeyPressed))
        {
            if (!fSentRESEND)
            {
                QList <unsigned> shortCodes = UIHostCombo::modifiersToScanCodes(gEDataManager->hostKeyCombination());
                QVector <LONG> codes;
                foreach (unsigned idxCode, shortCodes)
                {
                    if (idxCode & 0x100)
                        codes << 0xE0;
                    codes << (idxCode & 0x7F);
                    m_pressedKeys[idxCode & 0x7F] &= idxCode & 0x100 ? ~IsExtKeyPressed : ~IsKeyPressed;
                }
                foreach (unsigned idxCode, shortCodes)
                {
                    if (idxCode & 0x100)
                        codes << 0xE0;
                    codes << ((idxCode & 0x7F) | 0x80);
                }
                keyboard().PutScancodes(codes);
                fSentRESEND = true;
            }
            if (m_pressedKeys[i] & IsKeyPressed)
                keyboard().PutScancode(i | 0x80);
            else
            {
                QVector <LONG> codes(2);
                codes[0] = 0xE0;
                codes[1] = i | 0x80;
                keyboard().PutScancodes(codes);
            }
        }
        m_pressedKeys[i] = 0;
    }

    if (aReleaseHostKey)
    {
        m_bIsHostComboPressed = false;
        m_pressedHostComboKeys.clear();
    }

#ifdef VBOX_WS_MAC
    unsigned int hostComboModifierMask = 0;
    QList<int> hostCombo = UIHostCombo::toKeyCodeList(gEDataManager->hostKeyCombination());
    for (int i = 0; i < hostCombo.size(); ++i)
        hostComboModifierMask |= ::DarwinKeyCodeToDarwinModifierMask(hostCombo.at(i));
    /* Clear most of the modifiers: */
    m_uDarwinKeyModifiers &=
        alphaLock | kEventKeyModifierNumLockMask |
        (aReleaseHostKey ? 0 : hostComboModifierMask);
#endif /* VBOX_WS_MAC */

    /* Notify all the listeners: */
    emit sigStateChange(state());
}

/* Current keyboard state: */
int UIKeyboardHandler::state() const
{
    return (m_fIsKeyboardCaptured ? UIViewStateType_KeyboardCaptured : 0) |
           (m_bIsHostComboPressed ? UIViewStateType_HostKeyPressed : 0);
}

#ifdef VBOX_WITH_DEBUGGER_GUI
void UIKeyboardHandler::setDebuggerActive(bool aActive /* = true*/)
{
    if (aActive)
    {
        m_fDebuggerActive = true;
        releaseKeyboard();
    }
    else
        m_fDebuggerActive = false;
}

#endif /* VBOX_WITH_DEBUGGER_GUI */

#ifdef VBOX_WS_WIN
void UIKeyboardHandler::winSkipKeyboardEvents(bool fSkip)
{
    m_fSkipKeyboardEvents = fSkip;
}
#endif /* VBOX_WS_WIN */

bool UIKeyboardHandler::nativeEventFilter(void *pMessage, ulong uScreenId)
{
    /* Make sure view with passed index exists: */
    if (!m_views.contains(uScreenId))
        return false;

    /* Check if some system event should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    bool fResult = false; /* Pass to Qt by default. */

# if defined(VBOX_WS_MAC)

    /* Acquire carbon event reference from the cocoa one: */
    EventRef event = static_cast<EventRef>(darwinCocoaToCarbonEvent(pMessage));

    /* Depending on event kind: */
    const UInt32 uEventKind = ::GetEventKind(event);
    switch (uEventKind)
    {
        /* Watch for simple key-events: */
        case kEventRawKeyDown:
        case kEventRawKeyRepeat:
        case kEventRawKeyUp:
        {
            /* Acquire keycode: */
            UInt32 uKeyCode = ~0U;
            ::GetEventParameter(event, kEventParamKeyCode, typeUInt32,
                                NULL, sizeof(uKeyCode), NULL, &uKeyCode);

            /* The usb keyboard driver translates these codes to different virtual
             * keycodes depending of the keyboard type. There are ANSI, ISO, JIS
             * and unknown. For European keyboards (ISO) the key 0xa and 0x32 have
             * to be switched. Here we are doing this at runtime, cause the user
             * can have more than one keyboard (of different type), where he may
             * switch at will all the time. Default is the ANSI standard as defined
             * in g_aDarwinToSet1. Please note that the "~" on some English ISO
             * keyboards will be wrongly swapped. This can maybe fixed by
             * using a Apple keyboard layout in the guest. */
            if (   (uKeyCode == 0xa || uKeyCode == 0x32)
                && KBGetLayoutType(LMGetKbdType()) == kKeyboardISO)
                uKeyCode = 0x3c - uKeyCode;

            /* Translate keycode to set 1 scan code: */
            unsigned uScanCode = ::DarwinKeycodeToSet1Scancode(uKeyCode);

            /* If scan code is valid: */
            if (uScanCode)
            {
                /* Calculate flags: */
                int iFlags = 0;
                if (uEventKind != kEventRawKeyUp)
                    iFlags |= KeyPressed;
                if (uScanCode & VBOXKEY_EXTENDED)
                    iFlags |= KeyExtended;
                /** @todo KeyPause, KeyPrint. */
                uScanCode &= VBOXKEY_SCANCODE_MASK;

                /* Get the unicode string (if present): */
                AssertCompileSize(wchar_t, 2);
                AssertCompileSize(UniChar, 2);
                ByteCount cbWritten = 0;
                wchar_t ucs[8];
                if (::GetEventParameter(event, kEventParamKeyUnicodes, typeUnicodeText,
                                        NULL, sizeof(ucs), &cbWritten, &ucs[0]) != 0)
                    cbWritten = 0;
                ucs[cbWritten / sizeof(wchar_t)] = 0; /* The api doesn't terminate it. */

                /* Finally, handle parsed key-event: */
                fResult = keyEvent(uKeyCode, uScanCode, iFlags, uScreenId, ucs[0] ? ucs : NULL);
            }

            break;
        }
        /* Watch for modifier key-events: */
        case kEventRawKeyModifiersChanged:
        {
            /* Acquire new modifier mask, it may contain
             * multiple modifier changes, kind of annoying: */
            UInt32 uNewMask = 0;
            ::GetEventParameter(event, kEventParamKeyModifiers, typeUInt32,
                                NULL, sizeof(uNewMask), NULL, &uNewMask);

            /* Adjust new modifier mask to distinguish left/right modifiers: */
            uNewMask = ::DarwinAdjustModifierMask(uNewMask, pMessage);

            /* Determine what is really changed: */
            const UInt32 changed = uNewMask ^ m_uDarwinKeyModifiers;
            if (changed)
            {
                for (UInt32 bit = 0; bit < 32; ++bit)
                {
                    /* Skip unchanged bits: */
                    if (!(changed & (1 << bit)))
                        continue;
                    /* Acquire set 1 scan code from new mask: */
                    unsigned uScanCode = ::DarwinModifierMaskToSet1Scancode(1 << bit);
                    /* Skip invalid scan codes: */
                    if (!uScanCode)
                        continue;
                    /* Acquire darwin keycode from new mask: */
                    unsigned uKeyCode = ::DarwinModifierMaskToDarwinKeycode(1 << bit);
                    /* Assert invalid keycodes: */
                    Assert(uKeyCode);

                    /* For non-lockable modifier: */
                    if (!(uScanCode & VBOXKEY_LOCK))
                    {
                        /* Calculate flags: */
                        unsigned uFlags = (uNewMask & (1 << bit)) ? KeyPressed : 0;
                        if (uScanCode & VBOXKEY_EXTENDED)
                            uFlags |= KeyExtended;
                        uScanCode &= VBOXKEY_SCANCODE_MASK;

                        /* Finally, handle parsed key-event: */
                        keyEvent(uKeyCode, uScanCode & 0xff, uFlags, uScreenId);
                    }
                    /* For lockable modifier: */
                    else
                    {
                        /* Calculate flags: */
                        unsigned uFlags = 0;
                        if (uScanCode & VBOXKEY_EXTENDED)
                            uFlags |= KeyExtended;
                        uScanCode &= VBOXKEY_SCANCODE_MASK;

                        /* Finally, handle parsed press/release pair: */
                        keyEvent(uKeyCode, uScanCode, uFlags | KeyPressed, uScreenId);
                        keyEvent(uKeyCode, uScanCode, uFlags, uScreenId);
                    }
                }
            }

            /* Remember new modifier mask: */
            m_uDarwinKeyModifiers = uNewMask;

            /* Always return true here because we'll otherwise getting a Qt event
             * we don't want and that will only cause the Pause warning to pop up: */
            fResult = true;

            break;
        }
        default:
            break;
    }

# elif defined(VBOX_WS_WIN)

    /* Ignore this event if m_fSkipKeyboardEvents is set by winSkipKeyboardEvents(). */
    if (m_fSkipKeyboardEvents)
        return false;

    /* Cast to MSG event: */
    MSG *pMsg = static_cast<MSG*>(pMessage);

    /* Depending on message type: */
    switch (pMsg->message)
    {
        /* Watch for key-events: */
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            // WORKAROUND:
            // Can't do COM inter-process calls from a SendMessage handler,
            // see http://support.microsoft.com/kb/131056.
            if (vboxGlobal().isSeparateProcess() && InSendMessage())
            {
                PostMessage(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
                fResult = true;
                break;
            }

            /* Check for our own special flag to ignore this event.
             * That flag could only be set later in this function
             * so having it here means this event came here
             * for the second time already. */
            if (pMsg->lParam & (0x1 << 25))
            {
                /* Remove that flag as well: */
                pMsg->lParam &= ~(0x1 << 25);
                fResult = false;
                break;
            }

            /* Scan codes 0x80 and 0x00 should be filtered out: */
            unsigned uScan = (pMsg->lParam >> 16) & 0x7F;
            if (!uScan)
            {
                fResult = true;
                break;
            }

            /* Get the virtual key: */
            int iVKey = pMsg->wParam;

            /* Calculate flags: */
            int iFlags = 0;
            if (pMsg->lParam & 0x1000000)
                iFlags |= KeyExtended;
            if (!(pMsg->lParam & 0x80000000))
                iFlags |= KeyPressed;

            /* Make sure AltGr monitor exists: */
            AssertPtrReturn(m_pAltGrMonitor, false);
            {
                /* Filter event out if we are sure that this is a fake left control event: */
                if (m_pAltGrMonitor->isCurrentEventDefinitelyFake(uScan, iFlags & KeyPressed, iFlags & KeyExtended))
                {
                    fResult = true;
                    break;
                }
                /* Update AltGR monitor state from key-event: */
                m_pAltGrMonitor->updateStateFromKeyEvent(uScan, iFlags & KeyPressed, iFlags & KeyExtended);
                /* And release left Ctrl key early (if required): */
                if (m_pAltGrMonitor->isLeftControlReleaseNeeded())
                    keyboard().PutScancode(0x1D | 0x80);
            }

            /* Check for special Korean keys. Based on the keyboard layout selected
             * on the host, the scan code in lParam might be 0x71/0x72 or 0xF1/0xF2.
             * In either case, we must deliver 0xF1/0xF2 scan code to the guest when
             * the key is pressed and nothing when it's released. */
            if (uScan == 0x71 || uScan == 0x72)
            {
                uScan |= 0x80;
                iFlags = KeyPressed;   /* Because a release would be ignored. */
                iVKey = VK_PROCESSKEY; /* In case it was 0xFF. */
            }

            /* When one of the SHIFT keys is held and one of the cursor movement
             * keys is pressed, Windows duplicates SHIFT press/release messages,
             * but with the virtual keycode set to 0xFF. These virtual keys are also
             * sent in some other situations (Pause, PrtScn, etc.). Filter out such messages. */
            if (iVKey == 0xFF)
            {
                fResult = true;
                break;
            }

            /* Handle special virtual keys: */
            switch (iVKey)
            {
                case VK_SHIFT:
                case VK_CONTROL:
                case VK_MENU:
                {
                    /* Overcome Win32 modifier key generalization: */
                    int iKeyscan = uScan;
                    if (iFlags & KeyExtended)
                        iKeyscan |= 0xE000;
                    switch (iKeyscan)
                    {
                        case 0x002A: iVKey = VK_LSHIFT;   break;
                        case 0x0036: iVKey = VK_RSHIFT;   break;
                        case 0x001D: iVKey = VK_LCONTROL; break;
                        case 0xE01D: iVKey = VK_RCONTROL; break;
                        case 0x0038: iVKey = VK_LMENU;    break;
                        case 0xE038: iVKey = VK_RMENU;    break;
                    }
                    break;
                }
                case VK_NUMLOCK:
                    /* Win32 sets the extended bit for the NumLock key. Reset it: */
                    iFlags &= ~KeyExtended;
                    break;
                case VK_SNAPSHOT:
                    iFlags |= KeyPrint;
                    break;
                case VK_PAUSE:
                    iFlags |= KeyPause;
                    break;
            }

            /* Finally, handle parsed key-event: */
            fResult = keyEvent(iVKey, uScan, iFlags, uScreenId);

            /* Always let Windows see key releases to prevent stuck keys.
             * Hopefully this won't cause any other issues. */
            if (pMsg->message == WM_KEYUP || pMsg->message == WM_SYSKEYUP)
            {
                fResult = false;
                break;
            }

            /* Above keyEvent() returned that it didn't processed the event, but since the
             * keyboard is captured, we don't want to pass it to Windows. We just want
             * to let Qt process the message (to handle non-alphanumeric <HOST>+key
             * shortcuts for example). So send it directly to the window with the
             * special flag in the reserved area of lParam (to avoid recursion). */
            if (!fResult && m_fIsKeyboardCaptured)
            {
                ::SendMessage(pMsg->hwnd, pMsg->message,
                              pMsg->wParam, pMsg->lParam | (0x1 << 25));
                fResult = true;
                break;
            }

            /* These special keys have to be handled by Windows as well to update the
             * internal modifier state and to enable/disable the keyboard LED: */
            if (iVKey == VK_NUMLOCK || iVKey == VK_CAPITAL || iVKey == VK_LSHIFT || iVKey == VK_RSHIFT)
            {
                fResult = false;
                break;
            }

            break;
        }
        default:
            break;
    }

# elif defined(VBOX_WS_X11)

    /* Cast to XCB event: */
    xcb_generic_event_t *pEvent = static_cast<xcb_generic_event_t*>(pMessage);

    /* Depending on event type: */
    switch (pEvent->response_type & ~0x80)
    {
        /* Watch for key-events: */
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
        {
            /* Cast to XCB key-event: */
            xcb_key_press_event_t *pKeyEvent = static_cast<xcb_key_press_event_t*>(pMessage);

            /* Translate the keycode to a PC scan code: */
            unsigned uScan = handleXKeyEvent(QX11Info::display(), pKeyEvent->detail);

            /* Scan codes 0x00 (no valid translation) and 0x80 (extended flag) are ignored: */
            if (!(uScan & 0x7F))
            {
                fResult = true;
                break;
            }

            /* Calculate flags: */
            int iflags = 0;
            if (uScan >> 8)
                iflags |= KeyExtended;
            if ((pEvent->response_type & ~0x80) == XCB_KEY_PRESS)
                iflags |= KeyPressed;

            /* Remove the extended flag: */
            uScan &= 0x7F;

            /* Special Korean keys must send scan code 0xF1/0xF2
             * when pressed and nothing when released. */
            if (uScan == 0x71 || uScan == 0x72)
            {
                if ((pEvent->response_type & ~0x80) == XCB_KEY_RELEASE)
                {
                    fResult = true;
                    break;
                }
                /* Re-create the bizarre scan code: */
                uScan |= 0x80;
            }

            /* Translate the keycode to a keysym: */
            KeySym ks = ::wrapXkbKeycodeToKeysym(QX11Info::display(), pKeyEvent->detail, 0, 0);

            /* Update special flags: */
            switch (ks)
            {
                case XK_Print:
                    iflags |= KeyPrint;
                    break;
                case XK_Pause:
                    if (pKeyEvent->state & ControlMask) /* Break */
                    {
                        ks = XK_Break;
                        iflags |= KeyExtended;
                        uScan = 0x46;
                    }
                    else
                        iflags |= KeyPause;
                    break;
            }

            /* Finally, handle parsed key-event: */
            fResult = keyEvent(ks, uScan, iflags, uScreenId);

            break;
        }
        default:
            break;
    }

# else

#  warning "port me!"

# endif

    /* Return result: */
    return fResult;
}

/* Machine state-change handler: */
void UIKeyboardHandler::sltMachineStateChanged()
{
    /* Get machine state: */
    KMachineState state = uisession()->machineState();
    /* Handle particular machine states: */
    switch (state)
    {
        case KMachineState_Paused:
        case KMachineState_TeleportingPausedVM:
        case KMachineState_Stuck:
        {
            /* Release the keyboard: */
            releaseKeyboard();
            /* And all pressed keys except the host-one : */
            releaseAllPressedKeys(false /* release host-key? */);
            break;
        }
        case KMachineState_Running:
        {
            /* Capture the keyboard by the first focused view: */
            QList<ulong> theListOfViewIds = m_views.keys();
            for (int i = 0; i < theListOfViewIds.size(); ++i)
            {
                if (viewHasFocus(theListOfViewIds[i]))
                {
                    /* Capture keyboard: */
#ifdef VBOX_WS_WIN
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally() &&
                        GetAncestor((HWND)m_views[theListOfViewIds[i]]->winId(), GA_ROOT) == GetForegroundWindow())
#else /* !VBOX_WS_WIN */
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally())
#endif /* !VBOX_WS_WIN */
                        captureKeyboard(theListOfViewIds[i]);
                    /* Reset the single-time disable capture flag: */
                    if (isAutoCaptureDisabled())
                        setAutoCaptureDisabled(false);
                    break;
                }
            }
            break;
        }
        default:
            break;
    }

    /* Recall reminder about paused VM input
     * if we are not in paused VM state already: */
    if (machineLogic()->activeMachineWindow() &&
        state != KMachineState_Paused &&
        state != KMachineState_TeleportingPausedVM)
        popupCenter().forgetAboutPausedVMInput(machineLogic()->activeMachineWindow());
}

void UIKeyboardHandler::sltFinaliseCaptureKeyboard()
{
    /* Try to finalise keyboard capture: */
    if (!finaliseCaptureKeyboard())
    {
        /* Try again in another 300 milliseconds in case of failure: */
        QTimer::singleShot(300, this, SLOT(sltFinaliseCaptureKeyboard()));
    }
}

/* Keyboard-handler constructor: */
UIKeyboardHandler::UIKeyboardHandler(UIMachineLogic *pMachineLogic)
    : QObject(pMachineLogic)
    , m_pMachineLogic(pMachineLogic)
    , m_iKeyboardCaptureViewIndex(-1)
    , m_fIsKeyboardCaptured(false)
    , m_bIsHostComboPressed(false)
    , m_bIsHostComboAlone(false)
    , m_bIsHostComboProcessed(false)
    , m_fPassCADtoGuest(false)
    , m_fDebuggerActive(false)
    , m_iKeyboardHookViewIndex(-1)
#if defined(VBOX_WS_MAC)
    , m_uDarwinKeyModifiers(0)
#elif defined(VBOX_WS_WIN)
    , m_fIsHostkeyInCapture(false)
    , m_fSkipKeyboardEvents(false)
    , m_keyboardHook(NULL)
    , m_pAltGrMonitor(0)
#elif defined(VBOX_WS_X11)
    , m_hButtonGrabWindow(0)
#endif /* VBOX_WS_X11 */
    , m_cMonitors(1)
{
    /* Prepare: */
    prepareCommon();

    /* Load settings: */
    loadSettings();

    /* Initialize: */
    sltMachineStateChanged();
}

/* Keyboard-handler destructor: */
UIKeyboardHandler::~UIKeyboardHandler()
{
    /* Cleanup: */
    cleanupCommon();
}

void UIKeyboardHandler::prepareCommon()
{
#ifdef VBOX_WS_WIN
    /* Prepare AltGR monitor: */
    m_pAltGrMonitor = new WinAltGrMonitor;
#endif /* VBOX_WS_WIN */

    /* Machine state-change updater: */
    connect(uisession(), SIGNAL(sigMachineStateChange()), this, SLOT(sltMachineStateChanged()));

    /* Pressed keys: */
    ::memset(m_pressedKeys, 0, sizeof(m_pressedKeys));

    m_cMonitors = uisession()->machine().GetMonitorCount();
}

void UIKeyboardHandler::loadSettings()
{
    /* Global settings: */
#ifdef VBOX_WS_X11
    /* Initialize the X keyboard subsystem: */
    initMappedX11Keyboard(QX11Info::display(), gEDataManager->remappedScanCodes());
    /* Fix for http://www.virtualbox.org/ticket/1296:
     * when X11 sends events for repeated keys, it always inserts an XKeyRelease
     * before the XKeyPress. */
    /* Disable key release events during key auto-repeat: */
    XkbSetDetectableAutoRepeat(QX11Info::display(), True, NULL);
#endif /* VBOX_WS_X11 */

    /* Extra data settings: */
    {
        /* CAD setting: */
        m_fPassCADtoGuest = gEDataManager->passCADtoGuest(vboxGlobal().managedVMUuid());
    }
}

void UIKeyboardHandler::cleanupCommon()
{
#if defined(VBOX_WS_MAC)

    /* Cleanup keyboard-hook: */
    if (m_iKeyboardHookViewIndex != -1)
    {
        /* Ungrab the keyboard and unregister the event callback/hook: */
        ::DarwinReleaseKeyboard();
        UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                  UIKeyboardHandler::macKeyboardProc, this);
    }

#elif defined(VBOX_WS_WIN)

    /* Cleanup AltGR monitor: */
    delete m_pAltGrMonitor;
    m_pAltGrMonitor = 0;

    /* If keyboard-hook is installed: */
    if (m_keyboardHook)
    {
        /* Uninstall existing keyboard-hook: */
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = 0;
    }

#endif /* VBOX_WS_WIN */

    /* Update keyboard hook view index: */
    m_iKeyboardHookViewIndex = -1;
}

/* Machine-logic getter: */
UIMachineLogic* UIKeyboardHandler::machineLogic() const
{
    return m_pMachineLogic;
}

/* Action-pool getter: */
UIActionPool* UIKeyboardHandler::actionPool() const
{
    return machineLogic()->actionPool();
}

/* UI Session getter: */
UISession* UIKeyboardHandler::uisession() const
{
    return machineLogic()->uisession();
}

CKeyboard& UIKeyboardHandler::keyboard() const
{
    return uisession()->keyboard();
}

/* Event handler for prepared listener(s): */
bool UIKeyboardHandler::eventFilter(QObject *pWatchedObject, QEvent *pEvent)
{
    /* Check if pWatchedObject object is view: */
    if (UIMachineView *pWatchedView = isItListenedView(pWatchedObject))
    {
        /* Get corresponding screen index: */
        ulong uScreenId = m_views.key(pWatchedView);
        NOREF(uScreenId);
        /* Handle view events: */
        switch (pEvent->type())
        {
            case QEvent::FocusIn:
            {
#if defined(VBOX_WS_MAC)

                /* If keyboard-hook is NOT installed;
                 * Or installed but NOT for that view: */
                if ((int)uScreenId != m_iKeyboardHookViewIndex)
                {
                    /* If keyboard-hook is NOT installed: */
                    if (m_iKeyboardHookViewIndex == -1)
                    {
                        /* Disable mouse and keyboard event compression/delaying
                         * to make sure we *really* get all of the events: */
                        ::CGSetLocalEventsSuppressionInterval(0.0); /** @todo replace with CGEventSourceSetLocalEventsSuppressionInterval ? */
                        ::darwinSetMouseCoalescingEnabled(false);

                        /* Bring the caps lock state up to date,
                         * otherwise e.g. a later Shift key press will accidentally inject a CapsLock key press and release,
                         * see UIKeyboardHandler::macKeyboardEvent for the code handling modifier key state changes. */
                        m_uDarwinKeyModifiers ^= (m_uDarwinKeyModifiers ^ ::GetCurrentEventKeyModifiers()) & alphaLock;

                        /* Register the event callback/hook and grab the keyboard: */
                        UICocoaApplication::instance()->registerForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                                UIKeyboardHandler::macKeyboardProc, this);
                        ::DarwinGrabKeyboard(false);
                    }
                }

#elif defined(VBOX_WS_WIN)

                /* If keyboard-hook is NOT installed;
                 * Or installed but NOT for that view: */
                if (!m_keyboardHook || (int)uScreenId != m_iKeyboardHookViewIndex)
                {
                    /* If keyboard-hook is installed: */
                    if (m_keyboardHook)
                    {
                        /* Uninstall existing keyboard-hook: */
                        UnhookWindowsHookEx(m_keyboardHook);
                        m_keyboardHook = 0;
                    }
                    /* Install new keyboard-hook: */
                    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, UIKeyboardHandler::winKeyboardProc, GetModuleHandle(NULL), 0);
                    AssertMsg(m_keyboardHook, ("SetWindowsHookEx(): err=%d", GetLastError()));
                }

#endif /* VBOX_WS_WIN */

                /* Update keyboard hook view index: */
                m_iKeyboardHookViewIndex = uScreenId;

                if (isSessionRunning())
                {
                    /* Capture keyboard: */
#ifdef VBOX_WS_WIN
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally() &&
                        GetAncestor((HWND)pWatchedView->winId(), GA_ROOT) == GetForegroundWindow())
#else /* !VBOX_WS_WIN */
                    if (!isAutoCaptureDisabled() && autoCaptureSetGlobally())
#endif /* !VBOX_WS_WIN */
                        captureKeyboard(uScreenId);
                    /* Reset the single-time disable capture flag: */
                    if (isAutoCaptureDisabled())
                        setAutoCaptureDisabled(false);
                }

                break;
            }
            case QEvent::FocusOut:
            {
#if defined(VBOX_WS_MAC)

                /* If keyboard-hook is installed: */
                if ((int)uScreenId == m_iKeyboardHookViewIndex)
                {
                    /* Ungrab the keyboard and unregister the event callback/hook: */
                    ::DarwinReleaseKeyboard();
                    UICocoaApplication::instance()->unregisterForNativeEvents(RT_BIT_32(10) | RT_BIT_32(11) | RT_BIT_32(12) /* NSKeyDown  | NSKeyUp | | NSFlagsChanged */,
                                                                              UIKeyboardHandler::macKeyboardProc, this);
                }

#elif defined(VBOX_WS_WIN)

                /* If keyboard-hook is installed: */
                if (m_keyboardHook)
                {
                    /* Uninstall existing keyboard-hook: */
                    UnhookWindowsHookEx(m_keyboardHook);
                    m_keyboardHook = 0;
                }

#endif /* VBOX_WS_WIN */

                /* Update keyboard hook view index: */
                m_iKeyboardHookViewIndex = -1;

                /* Release keyboard: */
                if (isSessionRunning() || isSessionStuck())
                    releaseKeyboard();
                /* And all pressed keys: */
                releaseAllPressedKeys(true);

                break;
            }
            case QEvent::KeyPress:
            case QEvent::KeyRelease:
            {
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

                if (m_bIsHostComboPressed && pEvent->type() == QEvent::KeyPress)
                {
                    /* Passing F1-F12 keys to the guest: */
                    if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F12)
                    {
                        QVector <LONG> combo(6);
                        combo[0] = 0x1d; /* Ctrl down */
                        combo[1] = 0x38; /* Alt  down */
                        combo[4] = 0xb8; /* Alt  up   */
                        combo[5] = 0x9d; /* Ctrl up   */
                        if (pKeyEvent->key() >= Qt::Key_F1 && pKeyEvent->key() <= Qt::Key_F10)
                        {
                            combo[2] = 0x3b + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 down */
                            combo[3] = 0xbb + (pKeyEvent->key() - Qt::Key_F1); /* F1-F10 up   */
                        }
                        /* There is some scan slice between F10 and F11 keys, so its separated: */
                        else if (pKeyEvent->key() >= Qt::Key_F11 && pKeyEvent->key() <= Qt::Key_F12)
                        {
                            combo[2] = 0x57 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 down */
                            combo[3] = 0xd7 + (pKeyEvent->key() - Qt::Key_F11); /* F11-F12 up   */
                        }
                        keyboard().PutScancodes(combo);
                    }
                    /* Process hot keys not processed in keyEvent() (as in case of non-alphanumeric keys): */
                    actionPool()->processHotKey(QKeySequence(pKeyEvent->key()));
                }
                else if (!m_bIsHostComboPressed && pEvent->type() == QEvent::KeyRelease)
                {
                    /* Show a possible warning on key release which seems to be more expected by the end user: */
                    if (uisession()->isPaused())
                        popupCenter().remindAboutPausedVMInput(machineLogic()->activeMachineWindow());
                }

                break;
            }
            default:
                break;
        }
    }

    /* Else just propagate to base-class: */
    return QObject::eventFilter(pWatchedObject, pEvent);
}

#if defined(VBOX_WS_MAC)

/* static */
bool UIKeyboardHandler::macKeyboardProc(const void *pvCocoaEvent, const void *pvCarbonEvent, void *pvUser)
{
    /* Determine the event class: */
    EventRef event = (EventRef)pvCarbonEvent;
    UInt32 uEventClass = ::GetEventClass(event);

    /* Check if this is an application key combo. In that case we will not pass
     * the event to the guest, but let the host process it. */
    if (::darwinIsApplicationCommand(pvCocoaEvent))
        return false;

    /* Get the keyboard handler from the user's void data: */
    UIKeyboardHandler *pKeyboardHandler = static_cast<UIKeyboardHandler*>(pvUser);

    /* All keyboard class events needs to be handled: */
    if (uEventClass == kEventClassKeyboard && pKeyboardHandler && pKeyboardHandler->macKeyboardEvent(pvCocoaEvent, event))
        return true;

    /* Pass the event along: */
    return false;
}

bool UIKeyboardHandler::macKeyboardEvent(const void *pvCocoaEvent, EventRef event)
{
    /* Check what related machine-view was NOT unregistered yet: */
    if (!m_views.contains(m_iKeyboardHookViewIndex))
        return false;

    /* Pass event to machine-view's event handler: */
    Q_UNUSED(event);
    return nativeEventFilter(unconst(pvCocoaEvent), m_iKeyboardHookViewIndex);
}

#elif defined(VBOX_WS_WIN)

/* static */
LRESULT CALLBACK UIKeyboardHandler::winKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    /* All keyboard class events needs to be handled: */
    if (nCode == HC_ACTION && m_spKeyboardHandler && m_spKeyboardHandler->winKeyboardEvent(wParam, *(KBDLLHOOKSTRUCT*)lParam))
        return 1;

    /* Pass the event along: */
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool UIKeyboardHandler::winKeyboardEvent(UINT msg, const KBDLLHOOKSTRUCT &event)
{
    /* Check what related machine-view was NOT unregistered yet: */
    if (!m_views.contains(m_iKeyboardHookViewIndex))
        return false;

    /* It's possible that a key has been pressed while the keyboard was not
     * captured, but is being released under the capture. Detect this situation
     * and do not pass on the key press to the virtual machine. */
    uint8_t what_pressed =      (event.flags & 0x01)
                             && (event.vkCode != VK_RSHIFT)
                           ? IsExtKeyPressed : IsKeyPressed;
    if (   (event.flags & 0x80) /* released */
        && (   (   UIHostCombo::toKeyCodeList(gEDataManager->hostKeyCombination()).contains(event.vkCode)
                && !m_fIsHostkeyInCapture)
            ||    (  m_pressedKeys[event.scanCode & 0x7F]
                   & (IsKbdCaptured | what_pressed))
               == what_pressed))
        return false;

    if (!m_fIsKeyboardCaptured)
        return false;

    /* For normal user applications, Windows defines AltGr to be the same as
     * LControl + RAlt.  Without a low-level hook it is hard to recognise the
     * additional LControl event inserted, but in a hook we recognise it by
     * its special 0x21D scan code. */
    if (   m_views[m_iKeyboardHookViewIndex]->hasFocus()
        && ((event.scanCode & ~0x80) == 0x21D))
        return true;

    /* Compose the MSG: */
    MSG message;
    message.hwnd = (HWND)m_views[m_iKeyboardHookViewIndex]->winId();
    message.message = msg;
    message.wParam = event.vkCode;
    message.lParam = 1 | (event.scanCode & 0xFF) << 16 | (event.flags & 0xFF) << 24;

    /* Windows sets here the extended bit when the Right Shift key is pressed,
     * which is totally wrong. Undo it. */
    if (event.vkCode == VK_RSHIFT)
        message.lParam &= ~0x1000000;

    /* Pass event to view's event handler: */
    return nativeEventFilter(&message, m_iKeyboardHookViewIndex);
}

#endif /* VBOX_WS_WIN */

/**
 * If the user has just completed a control-alt-del combination then handle
 * that.
 * @returns true if handling should stop here, false otherwise
 */
bool UIKeyboardHandler::keyEventCADHandled(uint8_t uScan)
{
    /* Check if it's C-A-D and GUI/PassCAD is not set/allowed: */
    if (!m_fPassCADtoGuest &&
        uScan == 0x53 /* Del */ &&
        ((m_pressedKeys[0x38] & IsKeyPressed) /* Alt */ ||
         (m_pressedKeys[0x38] & IsExtKeyPressed)) &&
        ((m_pressedKeys[0x1d] & IsKeyPressed) /* Ctrl */ ||
         (m_pressedKeys[0x1d] & IsExtKeyPressed)))
    {
        /* Use the C-A-D combination as a last resort to get the keyboard and mouse back
         * to the host when the user forgets the Host Key. Note that it's always possible
         * to send C-A-D to the guest using the Host+Del combination: */
        if (isSessionRunning() && m_fIsKeyboardCaptured)
        {
            releaseKeyboard();
            if (!uisession()->isMouseSupportsAbsolute() || !uisession()->isMouseIntegrated())
                machineLogic()->mouseHandler()->releaseMouse();
        }
        return true;
    }
    return false;
}

/**
 * Handle a non-special (C-A-D, pause, print) key press or release
 * @returns true if handling should stop here, false otherwise
 */
bool UIKeyboardHandler::keyEventHandleNormal(int iKey, uint8_t uScan, int fFlags, LONG *pCodes, uint *puCodesCount)
{
    /* Get host-combo key list: */
    QSet<int> allHostComboKeys = UIHostCombo::toKeyCodeList(gEDataManager->hostKeyCombination()).toSet();
    /* Get the type of key - simple or extended: */
    uint8_t uWhatPressed = fFlags & KeyExtended ? IsExtKeyPressed : IsKeyPressed;

    /* If some key was pressed or some previously pressed key was released =>
     * we are updating the list of pressed keys and preparing scan codes: */
    if ((fFlags & KeyPressed) || (m_pressedKeys[uScan] & uWhatPressed))
    {
        /* If HID LEDs sync is disabled or not supported, check if the guest has the
         * same view on the modifier keys (NumLock, CapsLock, ScrollLock) as the host. */
        if (!machineLogic()->isHidLedsSyncEnabled())
            if (fFlags & KeyPressed)
                fixModifierState(pCodes, puCodesCount);

        /* Prepend 'extended' scan code if needed: */
        if (fFlags & KeyExtended)
            pCodes[(*puCodesCount)++] = 0xE0;

        /* Process key-press: */
        if (fFlags & KeyPressed)
        {
            /* Append scan code: */
            pCodes[(*puCodesCount)++] = uScan;
            m_pressedKeys[uScan] |= uWhatPressed;
        }
        /* Process key-release if that key was pressed before: */
        else if (m_pressedKeys[uScan] & uWhatPressed)
        {
            /* Append scan code: */
            pCodes[(*puCodesCount)++] = uScan | 0x80;
            m_pressedKeys[uScan] &= ~uWhatPressed;
        }

        /* Update keyboard-captured flag: */
        if (m_fIsKeyboardCaptured)
            m_pressedKeys[uScan] |= IsKbdCaptured;
        else
            m_pressedKeys[uScan] &= ~IsKbdCaptured;
    }
    /* Ignore key-release if that key was NOT pressed before,
     * but only if thats not one of the host-combination keys: */
    else if (!allHostComboKeys.contains(iKey))
        return true;
    return false;
}

/**
 * Check whether the key pressed results in a host key combination being
 * handled.
 * @returns true if a combination was handled, false otherwise
 * @param pfResult  where to store the result of the handling
 */
bool UIKeyboardHandler::keyEventHostComboHandled(int iKey, wchar_t *pUniKey, bool isHostComboStateChanged, bool *pfResult)
{
    if (isHostComboStateChanged)
    {
        if (!m_bIsHostComboPressed)
        {
            m_bIsHostComboPressed = true;
            m_bIsHostComboAlone = true;
            m_bIsHostComboProcessed = false;
            if (isSessionRunning())
                saveKeyStates();
        }
    }
    else
    {
        if (m_bIsHostComboPressed)
        {
            if (m_bIsHostComboAlone)
            {
                m_bIsHostComboAlone = false;
                m_bIsHostComboProcessed = true;
                /* Process Host+<key> shortcuts.
                 * Currently, <key> is limited to alphanumeric chars.
                 * Other Host+<key> combinations are handled in Qt event(): */
                *pfResult = processHotKey(iKey, pUniKey);
                return true;
            }
        }
    }
    return false;
}

/**
 * Handle a key event that releases the host key combination
 */
void UIKeyboardHandler::keyEventHandleHostComboRelease(ulong uScreenId)
{
    if (m_bIsHostComboPressed)
    {
        m_bIsHostComboPressed = false;
        /* Capturing/releasing keyboard/mouse if necessary: */
        if (m_bIsHostComboAlone && !m_bIsHostComboProcessed)
        {
            if (isSessionRunning())
            {
                bool ok = true;
                if (!m_fIsKeyboardCaptured)
                {
                    /* Temporarily disable auto-capture that will take place after
                     * this dialog is dismissed because the capture state is to be
                     * defined by the dialog result itself: */
                    setAutoCaptureDisabled(true);
                    bool fIsAutoConfirmed = false;
                    ok = msgCenter().confirmInputCapture(fIsAutoConfirmed);
                    if (fIsAutoConfirmed)
                        setAutoCaptureDisabled(false);
                    /* Otherwise, the disable flag will be reset in the next
                     * machine-view's focus-in event (since may happen asynchronously
                     * on some platforms, after we return from this code): */
                }
                if (ok)
                {
                    /* Determine whether the mouse can be captured: */
                    bool fCaptureMouse =    !uisession()->isMouseSupportsAbsolute()
                                         || !uisession()->isMouseIntegrated();

                    if (m_fIsKeyboardCaptured)
                    {
                        releaseKeyboard();
                        if (fCaptureMouse)
                            machineLogic()->mouseHandler()->releaseMouse();
                    }
                    else
                    {
                        captureKeyboard(uScreenId);
#ifdef VBOX_WS_X11
                        /* Make sure that pending FocusOut events from the
                         * previous message box are handled, otherwise the
                         * mouse is immediately ungrabbed: */
                        /// @todo Is that really needed?
                        qApp->processEvents();
#endif /* VBOX_WS_X11 */
                        finaliseCaptureKeyboard();
                        if (fCaptureMouse)
                        {
                            const MouseCapturePolicy mcp = gEDataManager->mouseCapturePolicy(vboxGlobal().managedVMUuid());
                            if (mcp == MouseCapturePolicy_Default || mcp == MouseCapturePolicy_HostComboOnly)
                                machineLogic()->mouseHandler()->captureMouse(uScreenId);
                        }
                    }
                }
            }
        }
        if (isSessionRunning())
            sendChangedKeyStates();
    }
}

void UIKeyboardHandler::keyEventReleaseHostComboKeys(const CKeyboard &constKeyboard)
{
    /* Get keyboard: */
    CKeyboard keyboard(constKeyboard);
    /* We have to make guest to release pressed keys from the host-combination: */
    QList<uint8_t> hostComboScans = m_pressedHostComboKeys.values();
    for (int i = 0 ; i < hostComboScans.size(); ++i)
    {
        uint8_t uScan = hostComboScans[i];
        if (m_pressedKeys[uScan] & IsKeyPressed)
        {
            keyboard.PutScancode(uScan | 0x80);
        }
        else if (m_pressedKeys[uScan] & IsExtKeyPressed)
        {
            QVector<LONG> scancodes(2);
            scancodes[0] = 0xE0;
            scancodes[1] = uScan | 0x80;
            keyboard.PutScancodes(scancodes);
        }
        m_pressedKeys[uScan] = 0;
    }
}

bool UIKeyboardHandler::keyEvent(int iKey, uint8_t uScan, int fFlags, ulong uScreenId, wchar_t *pUniKey /* = 0 */)
{
    /* Get host-combo key list: */
    QSet<int> allHostComboKeys = UIHostCombo::toKeyCodeList(gEDataManager->hostKeyCombination()).toSet();

    /* Update the map of pressed host-combo keys: */
    if (allHostComboKeys.contains(iKey))
    {
        if (fFlags & KeyPressed)
        {
            if (!m_pressedHostComboKeys.contains(iKey))
                m_pressedHostComboKeys.insert(iKey, uScan);
            else if (m_bIsHostComboPressed)
                return true;
        }
        else
        {
            if (m_pressedHostComboKeys.contains(iKey))
                m_pressedHostComboKeys.remove(iKey);
        }
    }
    /* Check if we are currently holding FULL host-combo: */
    bool fIsFullHostComboPresent = !allHostComboKeys.isEmpty() && allHostComboKeys == m_pressedHostComboKeys.keys().toSet();
    /* Check if currently pressed/released key had changed host-combo state: */
    const bool isHostComboStateChanged = (!m_bIsHostComboPressed &&  fIsFullHostComboPresent) ||
                                         ( m_bIsHostComboPressed && !fIsFullHostComboPresent);

#ifdef VBOX_WS_WIN
    if (m_bIsHostComboPressed || isHostComboStateChanged)
    {
        /* Currently this is used in winKeyboardEvent() only: */
        m_fIsHostkeyInCapture = m_fIsKeyboardCaptured;
    }
#endif /* VBOX_WS_WIN */

    if (keyEventCADHandled(uScan))
        return true;

    /* Preparing the press/release scan-codes array for sending to the guest:
     * 1. if host-combo is NOT pressed, taking into account currently pressed key too,
     * 2. if currently released key releases host-combo too.
     * Using that rule, we are NOT sending to the guest:
     * 1. the last key-press of host-combo,
     * 2. all keys pressed while the host-combo being held (but we still send releases). */
    LONG aCodesBuffer[16];
    LONG *pCodes = aCodesBuffer;
    uint uCodesCount = 0;
    uint8_t uWhatPressed = fFlags & KeyExtended ? IsExtKeyPressed : IsKeyPressed;
    if ((!m_bIsHostComboPressed && !isHostComboStateChanged) ||
        ( m_bIsHostComboPressed &&  isHostComboStateChanged) ||
        (!(fFlags & KeyPressed) && (m_pressedKeys[uScan] & uWhatPressed)))
    {
        /* Special flags handling (KeyPrint): */
        if (fFlags & KeyPrint)
        {
            if (fFlags & KeyPressed)
            {
                static LONG PrintMake[] = { 0xE0, 0x2A, 0xE0, 0x37 };
                pCodes = PrintMake;
                uCodesCount = SIZEOF_ARRAY(PrintMake);
            }
            else
            {
                static LONG PrintBreak[] = { 0xE0, 0xB7, 0xE0, 0xAA };
                pCodes = PrintBreak;
                uCodesCount = SIZEOF_ARRAY(PrintBreak);
            }
        }
        /* Special flags handling (KeyPause): */
        else if (fFlags & KeyPause)
        {
            if (fFlags & KeyPressed)
            {
                static LONG Pause[] = { 0xE1, 0x1D, 0x45, 0xE1, 0x9D, 0xC5 };
                pCodes = Pause;
                uCodesCount = SIZEOF_ARRAY(Pause);
            }
            else
            {
                /* Pause shall not produce a break code: */
                return true;
            }
        }
        /* Common flags handling: */
        else
            if (keyEventHandleNormal(iKey, uScan, fFlags, pCodes, &uCodesCount))
                return true;
    }

    /* Process the host-combo funtionality: */
    if (fFlags & KeyPressed)
    {
        bool fResult;
        if (keyEventHostComboHandled(iKey, pUniKey, isHostComboStateChanged, &fResult))
            return fResult;
    }
    else
    {
        if (isHostComboStateChanged)
            keyEventHandleHostComboRelease(uScreenId);
        else
        {
            if (m_bIsHostComboPressed)
                m_bIsHostComboAlone = true;
        }
    }

    /* Notify all the listeners: */
    emit sigStateChange(state());

    /* If the VM is NOT paused: */
    if (!uisession()->isPaused())
    {
        /* If there are scan-codes to send: */
        if (uCodesCount)
        {
            /* Send prepared scan-codes to the guest: */
            std::vector<LONG> scancodes(pCodes, &pCodes[uCodesCount]);
            keyboard().PutScancodes(QVector<LONG>::fromStdVector(scancodes));
        }

        /* If full host-key sequence was just finalized: */
        if (isHostComboStateChanged && m_bIsHostComboPressed)
        {
            keyEventReleaseHostComboKeys(keyboard());
        }
    }

    /* Prevent the key from going to Qt: */
    return true;
}

bool UIKeyboardHandler::processHotKey(int iHotKey, wchar_t *pHotKey)
{
    /* Prepare processing result: */
    bool fWasProcessed = false;

#if defined(VBOX_WS_MAC)

    Q_UNUSED(iHotKey);
    if (pHotKey && pHotKey[0] && !pHotKey[1])
        fWasProcessed = actionPool()->processHotKey(QKeySequence(Qt::UNICODE_ACCEL + QChar(pHotKey[0]).toUpper().unicode()));

#elif defined(VBOX_WS_WIN)

    Q_UNUSED(pHotKey);
    int iKeyboardLayout = GetKeyboardLayoutList(0, NULL);
    Assert(iKeyboardLayout);
    HKL *pList = new HKL[iKeyboardLayout];
    GetKeyboardLayoutList(iKeyboardLayout, pList);
    for (int i = 0; i < iKeyboardLayout && !fWasProcessed; ++i)
    {
        wchar_t symbol;
        static BYTE keys[256] = {0};
        if (!ToUnicodeEx(iHotKey, 0, keys, &symbol, 1, 0, pList[i]) == 1)
            symbol = 0;
        if (symbol)
            fWasProcessed = actionPool()->processHotKey(QKeySequence((Qt::UNICODE_ACCEL + QChar(symbol).toUpper().unicode())));
    }
    delete[] pList;

#elif defined(VBOX_WS_X11)

    Q_UNUSED(pHotKey);
    Display *pDisplay = QX11Info::display();
    KeyCode keyCode = XKeysymToKeycode(pDisplay, iHotKey);
    for (int i = 0; i < 4 && !fWasProcessed; ++i) /* Up to four groups. */
    {
        KeySym ks = wrapXkbKeycodeToKeysym(pDisplay, keyCode, i, 0);
        char symbol = 0;
        if (XkbTranslateKeySym(pDisplay, &ks, 0, &symbol, 1, NULL) == 0)
            symbol = 0;
        if (symbol)
        {
            QChar qtSymbol = QString::fromLocal8Bit(&symbol, 1)[0];
            fWasProcessed = actionPool()->processHotKey(QKeySequence((Qt::UNICODE_ACCEL + qtSymbol.toUpper().unicode())));
        }
    }

#else

# warning "port me!"

#endif

    /* Grab the key from the Qt if it was processed, or pass it to the Qt otherwise
     * in order to process non-alphanumeric keys in event(), after they are converted to Qt virtual keys: */
    return fWasProcessed;
}

void UIKeyboardHandler::fixModifierState(LONG *piCodes, uint *puCount)
{
    /* Synchronize the views of the host and the guest to the modifier keys.
     * This function will add up to 6 additional keycodes to codes. */

#if defined(VBOX_WS_MAC)

    /* if (uisession()->numLockAdaptionCnt()) ... - NumLock isn't implemented by Mac OS X so ignore it. */
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(::GetCurrentEventKeyModifiers() & alphaLock)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(VBOX_WS_WIN)

    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(GetKeyState(VK_NUMLOCK))))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(GetKeyState(VK_CAPITAL))))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#elif defined(VBOX_WS_X11)

    Window   wDummy1, wDummy2;
    int      iDummy3, iDummy4, iDummy5, iDummy6;
    unsigned uMask;
    unsigned uKeyMaskNum = 0, uKeyMaskCaps = 0;

    uKeyMaskCaps          = LockMask;
    XModifierKeymap* map  = XGetModifierMapping(QX11Info::display());
    KeyCode keyCodeNum    = XKeysymToKeycode(QX11Info::display(), XK_Num_Lock);

    for (int i = 0; i < 8; ++ i)
        if (keyCodeNum != NoSymbol && map->modifiermap[map->max_keypermod * i] == keyCodeNum)
            uKeyMaskNum = 1 << i;
    XQueryPointer(QX11Info::display(), DefaultRootWindow(QX11Info::display()), &wDummy1, &wDummy2,
                  &iDummy3, &iDummy4, &iDummy5, &iDummy6, &uMask);
    XFreeModifiermap(map);

    if (uisession()->numLockAdaptionCnt() && (uisession()->isNumLock() ^ !!(uMask & uKeyMaskNum)))
    {
        uisession()->setNumLockAdaptionCnt(uisession()->numLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x45;
        piCodes[(*puCount)++] = 0x45 | 0x80;
    }
    if (uisession()->capsLockAdaptionCnt() && (uisession()->isCapsLock() ^ !!(uMask & uKeyMaskCaps)))
    {
        uisession()->setCapsLockAdaptionCnt(uisession()->capsLockAdaptionCnt() - 1);
        piCodes[(*puCount)++] = 0x3a;
        piCodes[(*puCount)++] = 0x3a | 0x80;
        /* Some keyboard layouts require shift to be pressed to break
         * capslock.  For simplicity, only do this if shift is not
         * already held down. */
        if (uisession()->isCapsLock() && !(m_pressedKeys[0x2a] & IsKeyPressed))
        {
            piCodes[(*puCount)++] = 0x2a;
            piCodes[(*puCount)++] = 0x2a | 0x80;
        }
    }

#else

# warning "port me!"

#endif
}

void UIKeyboardHandler::saveKeyStates()
{
    ::memcpy(m_pressedKeysCopy, m_pressedKeys, sizeof(m_pressedKeys));
}

void UIKeyboardHandler::sendChangedKeyStates()
{
    QVector <LONG> codes(2);
    for (uint i = 0; i < SIZEOF_ARRAY(m_pressedKeys); ++ i)
    {
        uint8_t os = m_pressedKeysCopy[i];
        uint8_t ns = m_pressedKeys[i];
        if ((os & IsKeyPressed) != (ns & IsKeyPressed))
        {
            codes[0] = i;
            if (!(ns & IsKeyPressed))
                codes[0] |= 0x80;
            keyboard().PutScancode(codes[0]);
        }
        else if ((os & IsExtKeyPressed) != (ns & IsExtKeyPressed))
        {
            codes[0] = 0xE0;
            codes[1] = i;
            if (!(ns & IsExtKeyPressed))
                codes[1] |= 0x80;
            keyboard().PutScancodes(codes);
        }
    }
}

bool UIKeyboardHandler::isAutoCaptureDisabled()
{
    return uisession()->isAutoCaptureDisabled();
}

void UIKeyboardHandler::setAutoCaptureDisabled(bool fIsAutoCaptureDisabled)
{
    uisession()->setAutoCaptureDisabled(fIsAutoCaptureDisabled);
}

bool UIKeyboardHandler::autoCaptureSetGlobally()
{
    return gEDataManager->autoCaptureEnabled() && !m_fDebuggerActive;
}

bool UIKeyboardHandler::viewHasFocus(ulong uScreenId)
{
    return m_views[uScreenId]->hasFocus();
}

bool UIKeyboardHandler::isSessionRunning()
{
    return uisession()->isRunning();
}

bool UIKeyboardHandler::isSessionStuck()
{
    return uisession()->isStuck();
}

UIMachineWindow* UIKeyboardHandler::isItListenedWindow(QObject *pWatchedObject) const
{
    UIMachineWindow *pResultWindow = 0;
    QMap<ulong, UIMachineWindow*>::const_iterator i = m_windows.constBegin();
    while (!pResultWindow && i != m_windows.constEnd())
    {
        UIMachineWindow *pIteratedWindow = i.value();
        if (pIteratedWindow == pWatchedObject)
        {
            pResultWindow = pIteratedWindow;
            continue;
        }
        ++i;
    }
    return pResultWindow;
}

UIMachineView* UIKeyboardHandler::isItListenedView(QObject *pWatchedObject) const
{
    UIMachineView *pResultView = 0;
    QMap<ulong, UIMachineView*>::const_iterator i = m_views.constBegin();
    while (!pResultView && i != m_views.constEnd())
    {
        UIMachineView *pIteratedView = i.value();
        if (pIteratedView == pWatchedObject)
        {
            pResultView = pIteratedView;
            continue;
        }
        ++i;
    }
    return pResultView;
}
