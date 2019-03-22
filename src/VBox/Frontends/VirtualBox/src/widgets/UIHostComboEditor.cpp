/* $Id$ */
/** @file
 * VBox Qt GUI - VirtualBox Qt extensions: UIHostComboEditor class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QHBoxLayout>
# include <QKeyEvent>
# include <QStyleOption>
# include <QStylePainter>
# include <QTimer>
# ifdef VBOX_WS_X11
#  include <QX11Info>
# endif /* VBOX_WS_X11 */

/* GUI includes: */
# include "UIHostComboEditor.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "VBoxGlobal.h"
# include "QIToolButton.h"
# ifdef VBOX_WS_MAC
#  include "UICocoaApplication.h"
#  include "VBoxUtils-darwin.h"
# endif /* VBOX_WS_MAC */

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
# include <QAbstractNativeEventFilter>
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */

/* GUI includes: */
#if defined(VBOX_WS_MAC)
# include "DarwinKeyboard.h"
#elif defined(VBOX_WS_WIN)
# include "WinKeyboard.h"
#elif defined(VBOX_WS_X11)
# include "XKeyboard.h"
#endif /* VBOX_WS_X11 */

/* Other VBox includes: */
#if defined(VBOX_WS_X11)
# include <VBox/VBoxKeyboard.h>
#endif /* VBOX_WS_X11 */

/* External includes: */
#if defined(VBOX_WS_MAC)
# include <Carbon/Carbon.h>
#elif defined(VBOX_WS_X11)
# include <X11/Xlib.h>
# include <X11/Xutil.h>
# include <X11/keysym.h>
# include <xcb/xcb.h>
#endif /* VBOX_WS_X11 */

/* Namespaces: */
using namespace UIExtraDataDefs;


#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
/** QAbstractNativeEventFilter extension
  * allowing to handle native platform events.
  * Why do we need it? It's because Qt5 have unhandled
  * well .. let's call it 'a bug' about native keyboard events
  * which come to top-level widget (window) instead of focused sub-widget
  * which actually supposed to get them. The strange thing is that target of
  * those events on at least Windows host (MSG::hwnd) is indeed window itself,
  * not the sub-widget we expect, so that's probably the reason Qt devs
  * haven't fixed that bug so far for Windows and Mac OS X hosts. */
class ComboEditorEventFilter : public QAbstractNativeEventFilter
{
public:

    /** Constructor which takes the passed @a pParent to redirect events to. */
    ComboEditorEventFilter(UIHostComboEditorPrivate *pParent)
        : m_pParent(pParent)
    {}

    /** Handles all native events. */
    bool nativeEventFilter(const QByteArray &eventType, void *pMessage, long *pResult)
    {
        /* Redirect event to parent: */
        return m_pParent->nativeEvent(eventType, pMessage, pResult);
    }

private:

    /** Holds the passed parent reference. */
    UIHostComboEditorPrivate *m_pParent;
};
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */


/*********************************************************************************************************************************
*   Namespace UINativeHotKey implementation.                                                                                     *
*********************************************************************************************************************************/

#ifdef VBOX_WS_X11
namespace UINativeHotKey
{
    QMap<QString, QString> m_keyNames;
}
#endif /* VBOX_WS_X11 */

QString UINativeHotKey::toString(int iKeyCode)
{
    QString strKeyName;

#if defined(VBOX_WS_MAC)

    UInt32 modMask = DarwinKeyCodeToDarwinModifierMask(iKeyCode);
    switch (modMask)
    {
        case shiftKey:
        case optionKey:
        case controlKey:
        case cmdKey:
            strKeyName = UIHostComboEditor::tr("Left %1");
            break;
        case rightShiftKey:
        case rightOptionKey:
        case rightControlKey:
        case kEventKeyModifierRightCmdKeyMask:
            strKeyName = UIHostComboEditor::tr("Right %1");
            break;
        default:
            AssertMsgFailedReturn(("modMask=%#x\n", modMask), QString());
    }
    switch (modMask)
    {
        case shiftKey:
        case rightShiftKey:
            strKeyName = strKeyName.arg(QChar(kShiftUnicode));
            break;
        case optionKey:
        case rightOptionKey:
            strKeyName = strKeyName.arg(QChar(kOptionUnicode));
            break;
        case controlKey:
        case rightControlKey:
            strKeyName = strKeyName.arg(QChar(kControlUnicode));
            break;
        case cmdKey:
        case kEventKeyModifierRightCmdKeyMask:
            strKeyName = strKeyName.arg(QChar(kCommandUnicode));
            break;
    }

#elif defined(VBOX_WS_WIN)

    /* MapVirtualKey doesn't distinguish between right and left vkeys,
     * even under XP, despite that it stated in MSDN. Do it by hands.
     * Besides that it can't recognize such virtual keys as
     * VK_DIVIDE & VK_PAUSE, this is also known bug. */
    int iScan;
    switch (iKeyCode)
    {
        /* Processing special keys... */
        case VK_PAUSE: iScan = 0x45 << 16; break;
        case VK_RSHIFT: iScan = 0x36 << 16; break;
        case VK_RCONTROL: iScan = (0x1D << 16) | (1 << 24); break;
        case VK_RMENU: iScan = (0x38 << 16) | (1 << 24); break;
        /* Processing extended keys... */
        case VK_APPS:
        case VK_LWIN:
        case VK_RWIN:
        case VK_NUMLOCK: iScan = (::MapVirtualKey(iKeyCode, 0) | 256) << 16; break;
        default: iScan = ::MapVirtualKey(iKeyCode, 0) << 16;
    }
    TCHAR *pKeyName = new TCHAR[256];
    if (::GetKeyNameText(iScan, pKeyName, 256))
    {
        strKeyName = QString::fromUtf16(pKeyName);
    }
    else
    {
        AssertMsgFailed(("That key have no name!\n"));
        strKeyName = UIHostComboEditor::tr("<key_%1>").arg(iKeyCode);
    }
    delete[] pKeyName;

#elif defined(VBOX_WS_X11)

    if (char *pNativeKeyName = ::XKeysymToString((KeySym)iKeyCode))
    {
        strKeyName = m_keyNames[pNativeKeyName].isEmpty() ?
                     QString(pNativeKeyName) : m_keyNames[pNativeKeyName];
    }
    else
    {
        AssertMsgFailed(("That key have no name!\n"));
        strKeyName = UIHostComboEditor::tr("<key_%1>").arg(iKeyCode);
    }

#else

# warning "port me!"

#endif

    return strKeyName;
}

bool UINativeHotKey::isValidKey(int iKeyCode)
{
#if defined(VBOX_WS_MAC)

    UInt32 modMask = ::DarwinKeyCodeToDarwinModifierMask(iKeyCode);
    switch (modMask)
    {
        case shiftKey:
        case optionKey:
        case controlKey:
        case rightShiftKey:
        case rightOptionKey:
        case rightControlKey:
        case cmdKey:
        case kEventKeyModifierRightCmdKeyMask:
            return true;
        default:
            return false;
    }

#elif defined(VBOX_WS_WIN)

    return (iKeyCode >= VK_SHIFT  && iKeyCode <= VK_CAPITAL)
        || (iKeyCode >= VK_LSHIFT && iKeyCode <= VK_RMENU)
        || (iKeyCode >= VK_F1     && iKeyCode <= VK_F24)
        || iKeyCode == VK_NUMLOCK
        || iKeyCode == VK_SCROLL
        || iKeyCode == VK_LWIN
        || iKeyCode == VK_RWIN
        || iKeyCode == VK_APPS
        || iKeyCode == VK_PRINT;

#elif defined(VBOX_WS_X11)

    return (IsModifierKey(iKeyCode) /* allow modifiers */ ||
            IsFunctionKey(iKeyCode) /* allow function keys */ ||
            IsMiscFunctionKey(iKeyCode) /* allow miscellaneous function keys */ ||
            iKeyCode == XK_Scroll_Lock /* allow 'Scroll Lock' missed in IsModifierKey() */) &&
           (iKeyCode != NoSymbol /* ignore some special symbol */ &&
            iKeyCode != XK_Insert /* ignore 'insert' included into IsMiscFunctionKey */);

#else

# warning "port me!"

    return false;
#endif
}

unsigned UINativeHotKey::modifierToSet1ScanCode(int iKeyCode)
{
    switch(iKeyCode)
    {
#if defined(VBOX_WS_MAC)

        case controlKey:                        return 0x1D;
        case rightControlKey:                   return 0x11D;
        case shiftKey:                          return 0x2A;
        case rightShiftKey:                     return 0x36;
        case optionKey:                         return 0x38;
        case rightOptionKey:                    return 0x138;
        case cmdKey:                            return 0x15B;
        case kEventKeyModifierRightCmdKeyMask:  return 0x15C;
        default:                                return 0;

#elif defined(VBOX_WS_WIN)

        case VK_CONTROL:
        case VK_LCONTROL:  return 0x1D;
        case VK_RCONTROL:  return 0x11D;
        case VK_SHIFT:
        case VK_LSHIFT:    return 0x2A;
        case VK_RSHIFT:    return 0x36;
        case VK_MENU:
        case VK_LMENU:     return 0x38;
        case VK_RMENU:     return 0x138;
        case VK_LWIN:      return 0x15B;
        case VK_RWIN:      return 0x15C;
        case VK_APPS:      return 0x15D;
        default:           return 0;

#elif defined(VBOX_WS_X11)

        case XK_Control_L:         return 0x1D;
        case XK_Control_R:         return 0x11D;
        case XK_Shift_L:           return 0x2A;
        case XK_Shift_R:           return 0x36;
        case XK_Alt_L:             return 0x38;
        case XK_ISO_Level3_Shift:
        case XK_Alt_R:             return 0x138;
        case XK_Meta_L:
        case XK_Super_L:           return 0x15B;
        case XK_Meta_R:
        case XK_Super_R:           return 0x15C;
        case XK_Menu:              return 0x15D;
        default:                   return 0;

#else

# warning "port me!"

        default:  return 0;

#endif
    }
}

#if defined(VBOX_WS_WIN)

int UINativeHotKey::distinguishModifierVKey(int wParam, int lParam)
{
    int iKeyCode = wParam;
    switch (iKeyCode)
    {
        case VK_SHIFT:
        {
            UINT uCurrentScanCode = (lParam & 0x01FF0000) >> 16;
            UINT uLeftScanCode = ::MapVirtualKey(iKeyCode, 0);
            if (uCurrentScanCode == uLeftScanCode)
                iKeyCode = VK_LSHIFT;
            else
                iKeyCode = VK_RSHIFT;
            break;
        }
        case VK_CONTROL:
        {
            UINT uCurrentScanCode = (lParam & 0x01FF0000) >> 16;
            UINT uLeftScanCode = ::MapVirtualKey(iKeyCode, 0);
            if (uCurrentScanCode == uLeftScanCode)
                iKeyCode = VK_LCONTROL;
            else
                iKeyCode = VK_RCONTROL;
            break;
        }
        case VK_MENU:
        {
            UINT uCurrentScanCode = (lParam & 0x01FF0000) >> 16;
            UINT uLeftScanCode = ::MapVirtualKey(iKeyCode, 0);
            if (uCurrentScanCode == uLeftScanCode)
                iKeyCode = VK_LMENU;
            else
                iKeyCode = VK_RMENU;
            break;
        }
    }
    return iKeyCode;
}

#elif defined(VBOX_WS_X11)

void UINativeHotKey::retranslateKeyNames()
{
    m_keyNames["Shift_L"]          = UIHostComboEditor::tr("Left Shift");
    m_keyNames["Shift_R"]          = UIHostComboEditor::tr("Right Shift");
    m_keyNames["Control_L"]        = UIHostComboEditor::tr("Left Ctrl");
    m_keyNames["Control_R"]        = UIHostComboEditor::tr("Right Ctrl");
    m_keyNames["Alt_L"]            = UIHostComboEditor::tr("Left Alt");
    m_keyNames["Alt_R"]            = UIHostComboEditor::tr("Right Alt");
    m_keyNames["Super_L"]          = UIHostComboEditor::tr("Left WinKey");
    m_keyNames["Super_R"]          = UIHostComboEditor::tr("Right WinKey");
    m_keyNames["Menu"]             = UIHostComboEditor::tr("Menu key");
    m_keyNames["ISO_Level3_Shift"] = UIHostComboEditor::tr("Alt Gr");
    m_keyNames["Caps_Lock"]        = UIHostComboEditor::tr("Caps Lock");
    m_keyNames["Scroll_Lock"]      = UIHostComboEditor::tr("Scroll Lock");
}

#endif /* VBOX_WS_X11 */


/*********************************************************************************************************************************
*   Namespace UIHostCombo implementation.                                                                                        *
*********************************************************************************************************************************/

namespace UIHostCombo
{
    int m_iMaxComboSize = 3;
}

int UIHostCombo::hostComboModifierIndex()
{
    return -1;
}

QString UIHostCombo::hostComboModifierName()
{
    return UIHostComboEditor::tr("Host+");
}

QString UIHostCombo::hostComboCacheKey()
{
    return QString(GUI_Input_MachineShortcuts) + "/" + "HostCombo";
}

QString UIHostCombo::toReadableString(const QString &strKeyCombo)
{
    QStringList encodedKeyList = strKeyCombo.split(',');
    QStringList readableKeyList;
    for (int i = 0; i < encodedKeyList.size(); ++i)
        if (int iKeyCode = encodedKeyList[i].toInt())
            readableKeyList << UINativeHotKey::toString(iKeyCode);
    return readableKeyList.isEmpty() ? UIHostComboEditor::tr("None") : readableKeyList.join(" + ");
}

QList<int> UIHostCombo::toKeyCodeList(const QString &strKeyCombo)
{
    QStringList encodedKeyList = strKeyCombo.split(',');
    QList<int> keyCodeList;
    for (int i = 0; i < encodedKeyList.size(); ++i)
        if (int iKeyCode = encodedKeyList[i].toInt())
            keyCodeList << iKeyCode;
    return keyCodeList;
}

QList<unsigned> UIHostCombo::modifiersToScanCodes(const QString &strKeyCombo)
{
    QStringList encodedKeyList = strKeyCombo.split(',');
    QList<unsigned> scanCodeList;
    for (int i = 0; i < encodedKeyList.size(); ++i)
        if (unsigned idxScanCode = UINativeHotKey::modifierToSet1ScanCode(encodedKeyList[i].toInt()))
            if (idxScanCode != 0)
                scanCodeList << idxScanCode;
    return scanCodeList;
}

bool UIHostCombo::isValidKeyCombo(const QString &strKeyCombo)
{
    QList<int> keyCodeList = toKeyCodeList(strKeyCombo);
    if (keyCodeList.size() > m_iMaxComboSize)
        return false;
    for (int i = 0; i < keyCodeList.size(); ++i)
        if (!UINativeHotKey::isValidKey(keyCodeList[i]))
            return false;
    return true;
}


/*********************************************************************************************************************************
*   Class UIHostComboEditor implementation.                                                                                      *
*********************************************************************************************************************************/

UIHostComboEditor::UIHostComboEditor(QWidget *pParent)
    : QIWithRetranslateUI<QWidget>(pParent)
{
    /* Prepare: */
    prepare();
}

void UIHostComboEditor::sltCommitData()
{
    /* Commit data to the listener: */
    emit sigCommitData(this);
}

void UIHostComboEditor::prepare()
{
    /* Configure self: */
    setAutoFillBackground(true);
    /* Create layout: */
    QHBoxLayout *pLayout = new QHBoxLayout(this);
    {
        /* Configure layout: */
#ifdef VBOX_WS_MAC
        pLayout->setSpacing(5);
#else
        pLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif
        pLayout->setContentsMargins(0, 0, 0, 0);
        /* Create UIHostComboEditorPrivate instance: */
        m_pEditor = new UIHostComboEditorPrivate;
        {
            /* Configure UIHostComboEditorPrivate instance: */
            setFocusProxy(m_pEditor);
            connect(m_pEditor, SIGNAL(sigDataChanged()), this, SLOT(sltCommitData()));
        }
        /* Create 'clear' tool-button: */
        m_pButtonClear = new QIToolButton;
        {
            /* Configure 'clear' tool-button: */
            m_pButtonClear->removeBorder();
            m_pButtonClear->setIcon(UIIconPool::iconSet(":/eraser_16px.png"));
            connect(m_pButtonClear, SIGNAL(clicked(bool)), m_pEditor, SLOT(sltClear()));
        }
        /* Add widgets to layout: */
        pLayout->addWidget(m_pEditor);
        pLayout->addWidget(m_pButtonClear);
    }
    /* Translate finally: */
    retranslateUi();
}

void UIHostComboEditor::retranslateUi()
{
    /* Translate 'clear' tool-button: */
    m_pButtonClear->setToolTip(QApplication::translate("UIHotKeyEditor", "Unset shortcut"));
}

void UIHostComboEditor::setCombo(const UIHostComboWrapper &strCombo)
{
    /* Pass combo to child: */
    m_pEditor->setCombo(strCombo);
}

UIHostComboWrapper UIHostComboEditor::combo() const
{
    /* Acquire combo from child: */
    return m_pEditor->combo();
}


/*********************************************************************************************************************************
*   Class UIHostComboEditorPrivate implementation.                                                                               *
*********************************************************************************************************************************/

UIHostComboEditorPrivate::UIHostComboEditorPrivate()
    : m_pReleaseTimer(0)
    , m_fStartNewSequence(true)
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    , m_pPrivateEventFilter(0)
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */
#ifdef VBOX_WS_WIN
    , m_pAltGrMonitor(0)
#endif /* VBOX_WS_WIN */
{
    /* Configure widget: */
    setAttribute(Qt::WA_NativeWindow);
    setContextMenuPolicy(Qt::NoContextMenu);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
    connect(this, SIGNAL(selectionChanged()), this, SLOT(sltDeselect()));

    /* Setup release-pending-keys timer: */
    m_pReleaseTimer = new QTimer(this);
    m_pReleaseTimer->setInterval(200);
    connect(m_pReleaseTimer, SIGNAL(timeout()), this, SLOT(sltReleasePendingKeys()));

#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /* Prepare private event filter: */
    m_pPrivateEventFilter = new ComboEditorEventFilter(this);
    qApp->installNativeEventFilter(m_pPrivateEventFilter);
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */

#if defined(VBOX_WS_MAC)
    m_uDarwinKeyModifiers = 0;
#elif defined(VBOX_WS_WIN)
    /* Prepare AltGR monitor: */
    m_pAltGrMonitor = new WinAltGrMonitor;
#elif defined(VBOX_WS_X11)
    /* Initialize the X keyboard subsystem: */
    initMappedX11Keyboard(QX11Info::display(), gEDataManager->remappedScanCodes());
#endif /* VBOX_WS_X11 */
}

UIHostComboEditorPrivate::~UIHostComboEditorPrivate()
{
#if defined(VBOX_WS_WIN)
    /* Cleanup AltGR monitor: */
    delete m_pAltGrMonitor;
    m_pAltGrMonitor = 0;
#endif /* VBOX_WS_WIN */

#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /* Cleanup private event filter: */
    qApp->removeNativeEventFilter(m_pPrivateEventFilter);
    delete m_pPrivateEventFilter;
    m_pPrivateEventFilter = 0;
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */
}

void UIHostComboEditorPrivate::setCombo(const UIHostComboWrapper &strCombo)
{
    /* Cleanup old combo: */
    m_shownKeys.clear();
    /* Parse newly passed combo: */
    QList<int> keyCodeList = UIHostCombo::toKeyCodeList(strCombo.toString());
    for (int i = 0; i < keyCodeList.size(); ++i)
        if (int iKeyCode = keyCodeList[i])
            m_shownKeys.insert(iKeyCode, UINativeHotKey::toString(iKeyCode));
    /* Update text: */
    updateText();
}

UIHostComboWrapper UIHostComboEditorPrivate::combo() const
{
    /* Compose current combination: */
    QStringList keyCodeStringList;
    QList<int> keyCodeList = m_shownKeys.keys();
    for (int i = 0; i < keyCodeList.size(); ++i)
        keyCodeStringList << QString::number(keyCodeList[i]);
    /* Return current combination or "0" for "None": */
    return keyCodeStringList.isEmpty() ? "0" : keyCodeStringList.join(",");
}

void UIHostComboEditorPrivate::sltDeselect()
{
    deselect();
}

void UIHostComboEditorPrivate::sltClear()
{
    /* Cleanup combo: */
    m_shownKeys.clear();
    /* Update text: */
    updateText();
    /* Move the focus to text-field: */
    setFocus();
    /* Notify data changed: */
    emit sigDataChanged();
}

bool UIHostComboEditorPrivate::nativeEvent(const QByteArray &eventType, void *pMessage, long *pResult)
{
# if defined(VBOX_WS_MAC)

    /* Make sure it's generic NSEvent: */
    if (eventType != "mac_generic_NSEvent")
        return QLineEdit::nativeEvent(eventType, pMessage, pResult);
    EventRef event = static_cast<EventRef>(darwinCocoaToCarbonEvent(pMessage));

    /* Check if some NSEvent should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    switch(::GetEventClass(event))
    {
        /* Watch for keyboard-events: */
        case kEventClassKeyboard:
        {
            switch(::GetEventKind(event))
            {
                /* Watch for keyboard-modifier-events: */
                case kEventRawKeyModifiersChanged:
                {
                    /* Get modifier mask: */
                    UInt32 modifierMask = 0;
                    ::GetEventParameter(event, kEventParamKeyModifiers, typeUInt32,
                                        NULL, sizeof(modifierMask), NULL, &modifierMask);
                    modifierMask = ::DarwinAdjustModifierMask(modifierMask, pMessage);

                    /* Do not handle unchanged masks: */
                    UInt32 uChanged = m_uDarwinKeyModifiers ^ modifierMask;
                    if (!uChanged)
                        break;

                    /* Convert to keycode: */
                    unsigned uKeyCode = ::DarwinModifierMaskToDarwinKeycode(uChanged);

                    /* Do not handle empty and multiple modifier changes: */
                    if (!uKeyCode || uKeyCode == ~0U)
                        break;

                    /* Handle key-event: */
                    if (processKeyEvent(uKeyCode, uChanged & modifierMask))
                    {
                        /* Save the new modifier mask state: */
                        m_uDarwinKeyModifiers = modifierMask;
                        return true;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

# elif defined(VBOX_WS_WIN)

    /* Make sure it's generic MSG event: */
    if (eventType != "windows_generic_MSG")
        return QLineEdit::nativeEvent(eventType, pMessage, pResult);
    MSG *pEvent = static_cast<MSG*>(pMessage);

    /* Check if some MSG event should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    switch (pEvent->message)
    {
        /* Watch for key-events: */
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            /* Parse key-event: */
            int iKeyCode = UINativeHotKey::distinguishModifierVKey((int)pEvent->wParam, (int)pEvent->lParam);
            unsigned iDownScanCode = (pEvent->lParam >> 16) & 0x7F;
            const bool fPressed = !(pEvent->lParam & 0x80000000);
            const bool fExtended = pEvent->lParam & 0x1000000;

            /* If present - why not just assert this? */
            if (m_pAltGrMonitor)
            {
                /* Update AltGR monitor state from key-event: */
                m_pAltGrMonitor->updateStateFromKeyEvent(iDownScanCode, fPressed, fExtended);
                /* And release left Ctrl key early (if required): */
                if (m_pAltGrMonitor->isLeftControlReleaseNeeded())
                {
                    m_pressedKeys.remove(VK_LCONTROL);
                    m_shownKeys.remove(VK_LCONTROL);
                }
                /* Fake LCtrl release events can also end up in the released
                 * key set.  Detect them on the immediately following RAlt up. */
                if (!m_pressedKeys.contains(VK_LCONTROL))
                    m_releasedKeys.remove(VK_LCONTROL);
            }

            /* Handle key-event: */
            return processKeyEvent(iKeyCode, (pEvent->message == WM_KEYDOWN || pEvent->message == WM_SYSKEYDOWN));
        }
        default:
            break;
    }

# elif defined(VBOX_WS_X11)

    /* Make sure it's generic XCB event: */
    if (eventType != "xcb_generic_event_t")
        return QLineEdit::nativeEvent(eventType, pMessage, pResult);
    xcb_generic_event_t *pEvent = static_cast<xcb_generic_event_t*>(pMessage);

    /* Check if some XCB event should be filtered out.
     * Returning @c true means filtering-out,
     * Returning @c false means passing event to Qt. */
    switch (pEvent->response_type & ~0x80)
    {
        /* Watch for key-events: */
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
        {
            /* Parse key-event: */
            xcb_key_press_event_t *pKeyEvent = static_cast<xcb_key_press_event_t*>(pMessage);
            RT_GCC_NO_WARN_DEPRECATED_BEGIN
            const KeySym ks = ::XKeycodeToKeysym(QX11Info::display(), pKeyEvent->detail, 0);
            RT_GCC_NO_WARN_DEPRECATED_END
            const int iKeySym = static_cast<const int>(ks);

            /* Handle key-event: */
            return processKeyEvent(iKeySym, (pEvent->response_type & ~0x80) == XCB_KEY_PRESS);
        }
        default:
            break;
    }

# else

#  warning "port me!"

# endif

    /* Call to base-class: */
    return QLineEdit::nativeEvent(eventType, pMessage, pResult);
}

void UIHostComboEditorPrivate::keyPressEvent(QKeyEvent *pEvent)
{
    /* Ignore most of key presses... */
    switch (pEvent->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Escape:
            return QLineEdit::keyPressEvent(pEvent);
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
            pEvent->ignore();
            return;
        default:
            break;
    }
}

void UIHostComboEditorPrivate::keyReleaseEvent(QKeyEvent *pEvent)
{
    /* Ignore most of key presses... */
    switch (pEvent->key())
    {
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        case Qt::Key_Escape:
            return QLineEdit::keyReleaseEvent(pEvent);
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Left:
        case Qt::Key_Right:
            pEvent->ignore();
            return;
        default:
            break;
    }
}

void UIHostComboEditorPrivate::mousePressEvent(QMouseEvent *pEvent)
{
    /* Handle like for usual QWidget: */
    QWidget::mousePressEvent(pEvent);
}

void UIHostComboEditorPrivate::mouseReleaseEvent(QMouseEvent *pEvent)
{
    /* Handle like for usual QWidget: */
    QWidget::mouseReleaseEvent(pEvent);
}

void UIHostComboEditorPrivate::sltReleasePendingKeys()
{
    /* Stop the timer, we process all pending keys at once: */
    m_pReleaseTimer->stop();
    /* Something to do? */
    if (!m_releasedKeys.isEmpty())
    {
        /* Remove every key: */
        QSetIterator<int> iterator(m_releasedKeys);
        while (iterator.hasNext())
        {
            int iKeyCode = iterator.next();
            m_pressedKeys.remove(iKeyCode);
            m_shownKeys.remove(iKeyCode);
        }
        m_releasedKeys.clear();
        if (m_pressedKeys.isEmpty())
            m_fStartNewSequence = true;
        /* Notify data changed: */
        emit sigDataChanged();
    }
    /* Make sure the user see what happens: */
    updateText();
}

bool UIHostComboEditorPrivate::processKeyEvent(int iKeyCode, bool fKeyPress)
{
    /* Check if symbol is valid else pass it to Qt: */
    if (!UINativeHotKey::isValidKey(iKeyCode))
        return false;

    /* Stop the release-pending-keys timer: */
    m_pReleaseTimer->stop();

    /* Key press: */
    if (fKeyPress)
    {
        /* Clear reflected symbols if new sequence started: */
        if (m_fStartNewSequence)
            m_shownKeys.clear();
        /* Make sure any keys pending for releasing are processed: */
        sltReleasePendingKeys();
        /* Check maximum combo size: */
        if (m_shownKeys.size() < UIHostCombo::m_iMaxComboSize)
        {
            /* Remember pressed symbol: */
            m_pressedKeys << iKeyCode;
            m_shownKeys.insert(iKeyCode, UINativeHotKey::toString(iKeyCode));
            /* Remember what we already started a sequence: */
            m_fStartNewSequence = false;
            /* Notify data changed: */
            emit sigDataChanged();
        }
    }
    /* Key release: */
    else
    {
        /* Queue released symbol for processing: */
        m_releasedKeys << iKeyCode;

        /* If all pressed keys are now pending for releasing we should stop further handling.
         * Now we have the status the user want: */
        if (m_pressedKeys == m_releasedKeys)
        {
            m_pressedKeys.clear();
            m_releasedKeys.clear();
            m_fStartNewSequence = true;
        }
        else
            m_pReleaseTimer->start();
    }

    /* Update text: */
    updateText();

    /* Prevent passing to Qt: */
    return true;
}

void UIHostComboEditorPrivate::updateText()
{
    QStringList shownKeyNames(m_shownKeys.values());
    setText(shownKeyNames.isEmpty() ? UIHostComboEditor::tr("None") : shownKeyNames.join(" + "));
}

