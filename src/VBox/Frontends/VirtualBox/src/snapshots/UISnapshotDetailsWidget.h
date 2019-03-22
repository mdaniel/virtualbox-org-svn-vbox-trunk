/* $Id$ */
/** @file
 * VBox Qt GUI - UISnapshotDetailsWidget class declaration.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UISnapshotDetailsWidget_h___
#define ___UISnapshotDetailsWidget_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UIExtraDataDefs.h"

/* COM includes: */
#include "COMEnums.h"
#include "CMachine.h"
#include "CSnapshot.h"

/* Forward declarations: */
class CNetworkAdapter;
class QGridLayout;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTabWidget;
class QTextEdit;
class QVBoxLayout;
class QWidget;
class QIDialogButtonBox;
class UISnapshotDetailsElement;


/** Snapshot pane: Snapshot data structure. */
struct UIDataSnapshot
{
    /** Constructs data. */
    UIDataSnapshot()
        : m_strName(QString())
        , m_strDescription(QString())
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataSnapshot &other) const
    {
        return true
               && (m_strName == other.m_strName)
               && (m_strDescription == other.m_strDescription)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataSnapshot &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataSnapshot &other) const { return !equal(other); }

    /** Holds the snapshot name. */
    QString m_strName;
    /** Holds the snapshot description. */
    QString m_strDescription;
};


/** QWidget extension providing GUI with snapshot details-widget. */
class UISnapshotDetailsWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

public:

    /** Constructs snapshot details-widget passing @a pParent to the base-class. */
    UISnapshotDetailsWidget(QWidget *pParent = 0);

    /** Returns the snapshot data. */
    const UIDataSnapshot &data() const { return m_newData; }
    /** Defines the @a comMachine. */
    void setData(const CMachine &comMachine);
    /** Defines the @a comSnapshot and it's @a data. */
    void setData(const UIDataSnapshot &data, const CSnapshot &comSnapshot);
    /** Clears the snapshot data. */
    void clearData();

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;
    /** Handles buttons translation. */
    void retranslateButtons();

private slots:

    /** Handles snapshot name change. */
    void sltHandleNameChange();
    /** Handles snapshot description change. */
    void sltHandleDescriptionChange();

    /** Handles snapshot details anchor clicks. */
    void sltHandleAnchorClicked(const QUrl &link);

    /** Handles snapshot details change accepting. */
    void sltHandleChangeAccepted();
    /** Handles snapshot details change rejecting. */
    void sltHandleChangeRejected();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares tab-widget. */
    void prepareTabWidget();
    /** Prepares 'Options' tab. */
    void prepareTabOptions();
    /** Prepares 'Details' tab. */
    void prepareTabDetails();

    /** Creates details element of passed @a enmType. */
    static UISnapshotDetailsElement *createDetailsElement(DetailsElementType enmType);

    /** Loads snapshot data. */
    void loadSnapshotData();

    /** Revalidates changes for passed @a pWidget. */
    void revalidate(QWidget *pWidget = 0);
    /** Retranslates validation for passed @a pWidget. */
    void retranslateValidation(QWidget *pWidget = 0);
    /** Updates button states. */
    void updateButtonStates();

    /** Returns details report of requested @a enmType for a given @a comMachine. */
    QString detailsReport(DetailsElementType enmType, const CMachine &comMachine, const CSnapshot &comSnapshot = CSnapshot()) const;

    /** Acquires @a comMachine group report. */
    static QString groupReport(const CMachine &comMachine);
    /** Acquires @a comMachine boot order report. */
    static QString bootOrderReport(const CMachine &comMachine);
    /** Acquires @a comMachine efi state report. */
    static QString efiStateReport(const CMachine &comMachine);
    /** Acquires @a comMachine acceleration report. */
    static QString accelerationReport(const CMachine &comMachine);
    /** Acquires @a comMachine scale-factor report. */
    static double scaleFactorReport(CMachine comMachine);
    /** Acquires @a comMachine display acceleration report. */
    static QString displayAccelerationReport(CMachine comMachine);
    /** Acquires @a comMachine VRDE server report. */
    static QStringList vrdeServerReport(CMachine comMachine);
    /** Acquires @a comMachine recording report. */
    static QStringList recordingReport(CMachine comMachine);
    /** Acquires @a comMachine storage report. */
    static QPair<QStringList, QList<QMap<QString, QString> > > storageReport(CMachine comMachine);
    /** Acquires @a comMachine audio report. */
    static QStringList audioReport(CMachine comMachine);
    /** Acquires @a comMachine network report. */
    static QStringList networkReport(CMachine comMachine);
    /** Acquires @a comMachine serial report. */
    static QStringList serialReport(CMachine comMachine);
    /** Acquires @a comMachine usb report. */
    static QStringList usbReport(CMachine comMachine);

    /** Wipes the HTML stuff from the passed @a strString. */
    static QString wipeHtmlStuff(const QString &strString);

    /** Prepares emhasized report for a given @a strValue, comparing to @a strOldValue. */
    static QString empReport(const QString &strValue, const QString &strOldValue);
    /** Prepares emhasized report for a given @a strValue, depending on @a fDo flag. */
    static QString empReport(const QString &strValue, bool fIgnore);

    /** Summarizes generic properties. */
    static QString summarizeGenericProperties(const CNetworkAdapter &adapter);

    /** Holds the machine object to load data from. */
    CMachine   m_comMachine;
    /** Holds the snapshot object to load data from. */
    CSnapshot  m_comSnapshot;

    /** Holds the old data copy. */
    UIDataSnapshot  m_oldData;
    /** Holds the new data copy. */
    UIDataSnapshot  m_newData;

    /** Holds the cached screenshot. */
    QPixmap  m_pixmapScreenshot;

    /** Holds the tab-widget instance. */
    QTabWidget *m_pTabWidget;

    /** Holds the 'Options' layout instance. */
    QGridLayout *m_pLayoutOptions;

    /** Holds the name label instance. */
    QLabel    *m_pLabelName;
    /** Holds the name editor instance. */
    QLineEdit *m_pEditorName;
    /** Holds the name error pane. */
    QLabel    *m_pErrorPaneName;

    /** Holds the description label instance. */
    QLabel    *m_pLabelDescription;
    /** Holds the description editor instance. */
    QTextEdit *m_pBrowserDescription;
    /** Holds the description error pane. */
    QLabel    *m_pErrorPaneDescription;

    /** Holds the button-box instance. */
    QIDialogButtonBox *m_pButtonBox;

    /** Holds the 'Details' layout instance. */
    QVBoxLayout *m_pLayoutDetails;

    /** Holds the details scroll-area instance. */
    QScrollArea *m_pScrollAreaDetails;

    /** Holds the details element map. */
    QMap<DetailsElementType, UISnapshotDetailsElement*> m_details;
};

#endif /* !___UISnapshotDetailsWidget_h___ */

