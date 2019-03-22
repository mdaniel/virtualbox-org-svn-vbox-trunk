/* $Id$ */
/** @file
 * VBox Qt GUI - UIHotKeyEditor class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIHotKeyEditor_h___
#define ___UIHotKeyEditor_h___

/* Qt includes: */
#include <QMetaType>
#include <QSet>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* Forward declarations: */
class QHBoxLayout;
class QIToolButton;
class UIHotKeyLineEdit;


/** Hot key types. */
enum UIHotKeyType
{
    UIHotKeyType_Simple,
    UIHotKeyType_WithModifiers
};


/** A string pair wrapper for hot-key sequence. */
class UIHotKey
{
public:

    /** Constructs null hot-key sequence. */
    UIHotKey()
        : m_enmType(UIHotKeyType_Simple)
    {}
    /** Constructs hot-key sequence on the basis of passed @a enmType, @a strSequence and @a strDefaultSequence. */
    UIHotKey(UIHotKeyType enmType, const QString &strSequence, const QString &strDefaultSequence)
        : m_enmType(enmType)
        , m_strSequence(strSequence)
        , m_strDefaultSequence(strDefaultSequence)
    {}
    /** Constructs hot-key sequence on the basis of @a other hot-key sequence. */
    UIHotKey(const UIHotKey &other)
        : m_enmType(other.type())
        , m_strSequence(other.sequence())
        , m_strDefaultSequence(other.defaultSequence())
    {}

    /** Makes a copy of the given other hot-key sequence and assigns it to this one. */
    UIHotKey &operator=(const UIHotKey &other)
    {
        m_enmType = other.type();
        m_strSequence = other.sequence();
        m_strDefaultSequence = other.defaultSequence();
        return *this;
    }

    /** Returns the type of this hot-key sequence. */
    UIHotKeyType type() const { return m_enmType; }

    /** Returns hot-key sequence. */
    const QString &sequence() const { return m_strSequence; }
    /** Returns default hot-key sequence. */
    const QString &defaultSequence() const { return m_strDefaultSequence; }
    /** Defines hot-key @a strSequence. */
    void setSequence(const QString &strSequence) { m_strSequence = strSequence; }

private:

    /** Holds the type of this hot-key sequence. */
    UIHotKeyType m_enmType;
    /** Holds the hot-key sequence. */
    QString m_strSequence;
    /** Holds the default hot-key sequence. */
    QString m_strDefaultSequence;
};
Q_DECLARE_METATYPE(UIHotKey);


/** QWidget subclass wrapping real hot-key editor. */
class SHARED_LIBRARY_STUFF UIHotKeyEditor : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;
    Q_PROPERTY(UIHotKey hotKey READ hotKey WRITE setHotKey USER true);

signals:

    /** Notifies listener about data should be committed. */
    void sigCommitData(QWidget *pThis);

public:

    /** Constructs hot-key editor passing @a pParent to the base-class. */
    UIHotKeyEditor(QWidget *pParent);

private slots:

    /** Resets hot-key sequence to default. */
    void sltReset();
    /** Clears hot-key sequence. */
    void sltClear();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles key-press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;
    /** Handles key-release @a pEvent. */
    virtual void keyReleaseEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Returns whether we hould skip key-event to line-edit. */
    bool shouldWeSkipKeyEventToLineEdit(QKeyEvent *pEvent);

    /** Returns whether key @a pEvent is ignored. */
    bool isKeyEventIgnored(QKeyEvent *pEvent);

    /** Fetches actual modifier states. */
    void fetchModifiersState();
    /** Returns whether Host+ modifier is required. */
    void checkIfHostModifierNeeded();

    /** Handles approved key-press @a pEvent. */
    bool approvedKeyPressed(QKeyEvent *pEvent);
    /** Handles key-press @a pEvent. */
    void handleKeyPress(QKeyEvent *pEvent);
    /** Handles key-release @a pEvent. */
    void handleKeyRelease(QKeyEvent *pEvent);
    /** Reflects recorded sequence in editor. */
    void reflectSequence();
    /** Draws recorded sequence in editor. */
    void drawSequence();

    /** Returns hot-key. */
    UIHotKey hotKey() const;
    /** Defines @a hotKey. */
    void setHotKey(const UIHotKey &hotKey);

    /** Holds the hot-key. */
    UIHotKey  m_hotKey;

    /** Holds whether the modifiers are allowed. */
    bool  m_fIsModifiersAllowed;

    /** Holds the main-layout instance. */
    QHBoxLayout      *m_pMainLayout;
    /** Holds the button-layout instance. */
    QHBoxLayout      *m_pButtonLayout;
    /** Holds the line-edit instance. */
    UIHotKeyLineEdit *m_pLineEdit;
    /** Holds the reset-button instance. */
    QIToolButton     *m_pResetButton;
    /** Holds the clear-button instance. */
    QIToolButton     *m_pClearButton;

    /** Holds the taken modifiers. */
    QSet<int>  m_takenModifiers;
    /** Holds the taken key. */
    int        m_iTakenKey;
    /** Holds whether sequence is taken. */
    bool       m_fSequenceTaken;
};


#endif /* !___UIHotKeyEditor_h___ */
