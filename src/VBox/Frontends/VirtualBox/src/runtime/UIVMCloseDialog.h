/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMCloseDialog class declaration.
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

#ifndef ___UIVMCloseDialog_h___
#define ___UIVMCloseDialog_h___

/* Qt includes: */
#include <QIcon>

/* GUI includes: */
#include "QIDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* Forward declarations: */
class QCheckBox;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QRadioButton;
class QVBoxLayout;
class CMachine;

/** QIDialog extension to handle Runtime UI close-event. */
class UIVMCloseDialog : public QIWithRetranslateUI<QIDialog>
{
    Q_OBJECT;

public:

    /** Constructs close dialog passing @a pParent to the base-class.
      * @param  comMachine             Brings the machine dialog created for.
      * @param  fIsACPIEnabled         Brings whether ACPI is enabled.
      * @param  restictedCloseActions  Brings a set of restricted actions. */
    UIVMCloseDialog(QWidget *pParent, CMachine &comMachine,
                    bool fIsACPIEnabled, MachineCloseAction restictedCloseActions);

    /** Returns whether dialog is valid. */
    bool isValid() const { return m_fValid; }

    /** Defines dialog @a icon. */
    void setIcon(const QIcon &icon);

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles any Qt @a pEvent. */
    virtual bool event(QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Updates widgets availability. */
    void sltUpdateWidgetAvailability();

    /** Accepts the dialog. */
    void accept();

private:

    /** Defines whether 'Detach' button is enabled. */
    void setButtonEnabledDetach(bool fEnabled);
    /** Defines whether 'Detach' button is visible. */
    void setButtonVisibleDetach(bool fVisible);

    /** Defines whether 'Save' button is enabled. */
    void setButtonEnabledSave(bool fEnabled);
    /** Defines whether 'Save' button is visible. */
    void setButtonVisibleSave(bool fVisible);

    /** Defines whether 'Shutdown' button is enabled. */
    void setButtonEnabledShutdown(bool fEnabled);
    /** Defines whether 'Shutdown' button is visible. */
    void setButtonVisibleShutdown(bool fVisible);

    /** Defines whether 'PowerOff' button is enabled. */
    void setButtonEnabledPowerOff(bool fEnabled);
    /** Defines whether 'PowerOff' button is visible. */
    void setButtonVisiblePowerOff(bool fVisible);

    /** Defines whether 'Discard' check-box is visible. */
    void setCheckBoxVisibleDiscard(bool fVisible);

    /** Prepares all. */
    void prepare();
    /** Prepares main layout. */
    void prepareMainLayout();
    /** Prepares top layout. */
    void prepareTopLayout();
    /** Prepares top-left layout. */
    void prepareTopLeftLayout();
    /** Prepares top-right layout. */
    void prepareTopRightLayout();
    /** Prepares choice layout. */
    void prepareChoiceLayout();
    /** Prepares button-box. */
    void prepareButtonBox();

    /** Configures dialog. */
    void configure();

    /** Updates pixmaps. */
    void updatePixmaps();

    /** Holds the live machine reference. */
    CMachine                 &m_comMachine;
    /** Holds whether ACPI is enabled. */
    bool                      m_fIsACPIEnabled;
    /** Holds a set of restricted actions. */
    const MachineCloseAction  m_restictedCloseActions;

    /** Holds whether dialog is valid. */
    bool  m_fValid;

    /** Holds the dialog icon. */
    QIcon  m_icon;

    /** Holds the main layout instance. */
    QVBoxLayout *m_pMainLayout;
    /** Holds the top layout instance. */
    QHBoxLayout *m_pTopLayout;
    /** Holds the top-left layout instance. */
    QVBoxLayout *m_pTopLeftLayout;
    /** Holds the top-right layout instance. */
    QVBoxLayout *m_pTopRightLayout;
    /** Holds the choice layout instance. */
    QGridLayout *m_pChoiceLayout;

    /** Holds the icon label instance. */
    QLabel *m_pLabelIcon;
    /** Holds the text label instance. */
    QLabel *m_pLabelText;

    /** Holds the 'Detach' icon label instance.  */
    QLabel       *m_pLabelIconDetach;
    /** Holds the 'Detach' radio-button instance.  */
    QRadioButton *m_pRadioButtonDetach;
    /** Holds the 'Save' icon label instance.  */
    QLabel       *m_pLabelIconSave;
    /** Holds the 'Save' radio-button instance.  */
    QRadioButton *m_pRadioButtonSave;
    /** Holds the 'Shutdown' icon label instance.  */
    QLabel       *m_pLabelIconShutdown;
    /** Holds the 'Shutdown' radio-button instance.  */
    QRadioButton *m_pRadioButtonShutdown;
    /** Holds the 'PowerOff' icon label instance.  */
    QLabel       *m_pLabelIconPowerOff;
    /** Holds the 'PowerOff' radio-button instance.  */
    QRadioButton *m_pRadioButtonPowerOff;

    /** Holds the 'Discard' check-box instance.  */
    QCheckBox *m_pCheckBoxDiscard;
    /** Holds the 'Discard' check-box text. */
    QString    m_strDiscardCheckBoxText;

    /** Holds the last close action. */
    MachineCloseAction  m_enmLastCloseAction;
};

#endif /* !___UIVMCloseDialog_h___ */
