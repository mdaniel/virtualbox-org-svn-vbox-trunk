/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostComboEditor class declaration.
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

#ifndef ___UIHostComboEditor_h___
#define ___UIHostComboEditor_h___

/* Qt includes: */
#include <QLineEdit>
#include <QMap>
#include <QMetaType>
#include <QSet>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QString;
class QWidget;
class QIToolButton;
class UIHostComboEditorPrivate;
#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
class ComboEditorEventFilter;
#endif
#ifdef VBOX_WS_WIN
class WinAltGrMonitor;
#endif


/** Native hot-key namespace to unify
  * all the related hot-key processing stuff. */
namespace UINativeHotKey
{
    /** Translates passed @a iKeyCode to string. */
    SHARED_LIBRARY_STUFF QString toString(int iKeyCode);

    /** Returns whether passed @a iKeyCode is valid. */
    SHARED_LIBRARY_STUFF bool isValidKey(int iKeyCode);

    /** Translates passed @a iKeyCode in host platform
      * encoding to the corresponding set 1 PC scan code.
      * @note  Non-modifier keys will return zero. */
    SHARED_LIBRARY_STUFF unsigned modifierToSet1ScanCode(int iKeyCode);

#if defined(VBOX_WS_WIN)
    /** Distinguishes modifier VKey by @a wParam and @a lParam. */
    SHARED_LIBRARY_STUFF int distinguishModifierVKey(int wParam, int lParam);
#elif defined(VBOX_WS_X11)
    /** Retranslates key names. */
    SHARED_LIBRARY_STUFF void retranslateKeyNames();
#endif
}


/** Host-combo namespace to unify
  * all the related hot-combo processing stuff. */
namespace UIHostCombo
{
    /** Returns host-combo modifier index. */
    SHARED_LIBRARY_STUFF int hostComboModifierIndex();
    /** Returns host-combo modifier name. */
    SHARED_LIBRARY_STUFF QString hostComboModifierName();
    /** Returns host-combo cached key. */
    SHARED_LIBRARY_STUFF QString hostComboCacheKey();

    /** Translates passed @strKeyCombo to readable string. */
    SHARED_LIBRARY_STUFF QString toReadableString(const QString &strKeyCombo);
    /** Translates passed @strKeyCombo to key codes list. */
    SHARED_LIBRARY_STUFF QList<int> toKeyCodeList(const QString &strKeyCombo);

    /** Returns a sequence of the set 1 PC scan codes for all
      * modifiers contained in the (host platform format) sequence passed. */
    SHARED_LIBRARY_STUFF QList<unsigned> modifiersToScanCodes(const QString &strKeyCombo);

    /** Returns whether passed @a strKeyCombo is valid. */
    SHARED_LIBRARY_STUFF bool isValidKeyCombo(const QString &strKeyCombo);
}


/** Host-combo QString wrapper. */
class SHARED_LIBRARY_STUFF UIHostComboWrapper
{
public:

    /** Constructs host-combo wrapper on the basis of passed @a strHostCombo. */
    UIHostComboWrapper(const QString &strHostCombo = QString())
        : m_strHostCombo(strHostCombo)
    {}

    /** Returns the host-combo. */
    const QString &toString() const { return m_strHostCombo; }

private:

    /** Holds the host-combo. */
    QString m_strHostCombo;
};
Q_DECLARE_METATYPE(UIHostComboWrapper);


/** Host-combo editor widget. */
class SHARED_LIBRARY_STUFF UIHostComboEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(UIHostComboWrapper combo READ combo WRITE setCombo USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /** Constructs host-combo editor passing @a pParent to the base-class. */
    UIHostComboEditor(QWidget *pParent);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Notifies listener about data should be committed. */
    void sltCommitData();

private:

    /** Prepares all. */
    void prepare();

    /** Defines host @a strCombo sequence. */
    void setCombo(const UIHostComboWrapper &strCombo);
    /** Returns host-combo sequence. */
    UIHostComboWrapper combo() const;

    /** UIHostComboEditorPrivate instance. */
    UIHostComboEditorPrivate *m_pEditor;
    /** <b>Clear</b> QIToolButton instance. */
    QIToolButton             *m_pButtonClear;
};


/** Host-combo editor widget private stuff. */
class SHARED_LIBRARY_STUFF UIHostComboEditorPrivate : public QLineEdit
{
    Q_OBJECT;

signals:

    /** Notifies parent about data changed. */
    void sigDataChanged();

public:

    /** Constructs host-combo editor private part. */
    UIHostComboEditorPrivate();
    /** Destructs host-combo editor private part. */
    ~UIHostComboEditorPrivate();

    /** Defines host @a strCombo sequence. */
    void setCombo(const UIHostComboWrapper &strCombo);
    /** Returns host-combo sequence. */
    UIHostComboWrapper combo() const;

public slots:

    /** Clears the host-combo selection. */
    void sltDeselect();
    /** Clears the host-combo editor. */
    void sltClear();

protected:

    /** Handles native events. */
    virtual bool nativeEvent(const QByteArray &eventType, void *pMessage, long *pResult) /* override */;

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** Handles key-release @a pEvent. */
    virtual void keyReleaseEvent(QKeyEvent *pEvent) /* override */;
    /** Handles mouse-press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles mouse-release @a pEvent. */
    virtual void mouseReleaseEvent(QMouseEvent *pEvent) /* override */;

private slots:

    /** Releases pending keys. */
    void sltReleasePendingKeys();

private:

    /** PRocesses key event of @a fKeyPress type for a passed @a iKeyCode. */
    bool processKeyEvent(int iKeyCode, bool fKeyPress);

    /** Updates text. */
    void updateText();

    /** Holds the pressed keys. */
    QSet<int>           m_pressedKeys;
    /** Holds the released keys. */
    QSet<int>           m_releasedKeys;
    /** Holds the shown keys. */
    QMap<int, QString>  m_shownKeys;

    /** Holds the release timer instance. */
    QTimer *m_pReleaseTimer;

    /** Holds whether new sequence should be started. */
    bool  m_fStartNewSequence;

#if defined(VBOX_WS_MAC) || defined(VBOX_WS_WIN)
    /** Mac, Win: Holds the native event filter instance. */
    ComboEditorEventFilter *m_pPrivateEventFilter;
    /** Mac, Win: Allows the native event filter to redirect events directly to nativeEvent handler. */
    friend class ComboEditorEventFilter;
#endif /* VBOX_WS_MAC || VBOX_WS_WIN */

#if defined(VBOX_WS_MAC)
    /** Mac: Holds the current modifier key mask. */
    uint32_t         m_uDarwinKeyModifiers;
#elif defined(VBOX_WS_WIN)
    /** Win: Holds the object monitoring key event stream for problematic AltGr events. */
    WinAltGrMonitor *m_pAltGrMonitor;
#endif /* VBOX_WS_WIN */
};


#endif /* !___UIHostComboEditor_h___ */
