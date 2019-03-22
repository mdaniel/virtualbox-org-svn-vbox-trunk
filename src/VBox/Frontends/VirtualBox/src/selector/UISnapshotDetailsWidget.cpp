/* $Id$ */
/** @file
 * VBox Qt GUI - UISnapshotDetailsWidget class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QHBoxLayout>
# include <QDateTime>
# include <QDir>
# include <QGridLayout>
# include <QLabel>
# include <QLineEdit>
# include <QPainter>
# include <QPushButton>
# include <QScrollArea>
# include <QStackedLayout>
# include <QTabWidget>
# include <QTextBrowser>
# include <QTextEdit>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIFlowLayout.h"
# include "UIConverter.h"
# include "UIDesktopWidgetWatchdog.h"
# include "UIIconPool.h"
# include "UISnapshotDetailsWidget.h"
# include "UIMessageCenter.h"
# include "VBoxGlobal.h"
# include "VBoxUtils.h"

/* COM includes: */
# include "CAudioAdapter.h"
# include "CMachine.h"
# include "CMedium.h"
# include "CMediumAttachment.h"
# include "CNetworkAdapter.h"
# include "CSerialPort.h"
# include "CSharedFolder.h"
# include "CStorageController.h"
# include "CSystemProperties.h"
# include "CUSBController.h"
# include "CUSBDeviceFilter.h"
# include "CUSBDeviceFilters.h"
# include "CVRDEServer.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/** QWiget extension providing GUI with snapshot details elements. */
class UISnapshotDetailsElement : public QWidget
{
    Q_OBJECT;

signals:

    /** Notifies listeners about @a link was clicked. */
    void sigAnchorClicked(const QUrl &link);

public:

    /** Constructs details element passing @a pParent to the base-class.
      * @param  fLinkSupport  Brings whether we should construct text-browser
      *                       instead of simple text-edit otherwise. */
    UISnapshotDetailsElement(bool fLinkSupport, QWidget *pParent = 0);

    /** Returns underlying text-document. */
    QTextDocument *document() const;

    /** Defines text-document text. */
    void setText(const QString &strText);

    /** Returns the minimum size-hint. */
    QSize minimumSizeHint() const;

protected:

    /** Handles paint @a pEvent. */
    virtual void paintEvent(QPaintEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Holds whether we should construct text-browser
      * instead of simple text-edit otherwise. */
    bool  m_fLinkSupport;

    /** Holds the text-edit interface instance. */
    QTextEdit *m_pTextEdit;
};


/** QWiget extension providing GUI with snapshot screenshot viewer widget. */
class UIScreenshotViewer : public QIWithRetranslateUI2<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs screenshow viewer passing @a pParent to the base-class.
      * @param  pixmapScreenshot  Brings the screenshot to show.
      * @param  strSnapshotName   Brings the snapshot name.
      * @param  strMachineName    Brings the machine name. */
    UIScreenshotViewer(const QPixmap &pixmapScreenshot,
                       const QString &strSnapshotName,
                       const QString &strMachineName,
                       QWidget *pParent = 0);

protected:

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

    /** Handles show @a pEvent. */
    virtual void showEvent(QShowEvent *pEvent) /* override */;
    /** Handles polish @a pEvent. */
    virtual void polishEvent(QShowEvent *pEvent);

    /** Handles resize @a pEvent. */
    virtual void resizeEvent(QResizeEvent *pEvent) /* override */;

    /** Handles mouse press @a pEvent. */
    virtual void mousePressEvent(QMouseEvent *pEvent) /* override */;
    /** Handles key press @a pEvent. */
    virtual void keyPressEvent(QKeyEvent *pEvent) /* override */;

private:

    /** Prepares all. */
    void prepare();

    /** Adjusts window size. */
    void adjustWindowSize();

    /** Adjusts picture. */
    void adjustPicture();

    /** Holds whether this widget was polished. */
    bool  m_fPolished;

    /** Holds the screenshot to show. */
    QPixmap  m_pixmapScreenshot;
    /** Holds the snapshot name. */
    QString  m_strSnapshotName;
    /** Holds the machine name. */
    QString  m_strMachineName;

    /** Holds the scroll-area instance. */
    QScrollArea *m_pScrollArea;
    /** Holds the picture label instance. */
    QLabel      *m_pLabelPicture;

    /** Holds whether we are in zoom mode. */
    bool  m_fZoomMode;
};


/*********************************************************************************************************************************
*   Class UISnapshotDetailsElement implementation.                                                                               *
*********************************************************************************************************************************/

UISnapshotDetailsElement::UISnapshotDetailsElement(bool fLinkSupport, QWidget *pParent /* = 0 */)
    : QWidget(pParent)
    , m_fLinkSupport(fLinkSupport)
    , m_pTextEdit(0)
{
    /* Prepare: */
    prepare();
}

QTextDocument *UISnapshotDetailsElement::document() const
{
    /* Pass to private object: */
    return m_pTextEdit->document();
}

void UISnapshotDetailsElement::setText(const QString &strText)
{
    /* Pass to private object: */
    m_pTextEdit->setText(strText);
    /* Update the layout: */
    updateGeometry();
}

QSize UISnapshotDetailsElement::minimumSizeHint() const
{
    /* Calculate minimum size-hint on the basis of:
     * 1. context and text-documnt margins, 2. text-document ideal width and height: */
    int iTop = 0, iLeft = 0, iRight = 0, iBottom = 0;
    layout()->getContentsMargins(&iTop, &iLeft, &iRight, &iBottom);
    const QSize size = m_pTextEdit->document()->size().toSize();
    const int iDocumentMargin = (int)m_pTextEdit->document()->documentMargin();
    const int iIdealWidth = (int)m_pTextEdit->document()->idealWidth() + 2 * iDocumentMargin + iLeft + iRight;
    const int iIdealHeight = size.height() + 2 * iDocumentMargin + iTop + iBottom;
    return QSize(iIdealWidth, iIdealHeight);
}

void UISnapshotDetailsElement::paintEvent(QPaintEvent * /* pEvent */)
{
    /* Prepare painter: */
    QPainter painter(this);

    /* Prepare palette colors: */
    const QPalette pal = palette();
    QColor color0 = pal.color(QPalette::Window);
    QColor color1 = pal.color(QPalette::Window).lighter(110);
    color1.setAlpha(0);
    QColor color2 = pal.color(QPalette::Window).darker(200);

    /* Invent pixel metric: */
    const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

    /* Top-left corner: */
    QRadialGradient grad1(QPointF(iMetric, iMetric), iMetric);
    {
        grad1.setColorAt(0, color2);
        grad1.setColorAt(1, color1);
    }
    /* Top-right corner: */
    QRadialGradient grad2(QPointF(width() - iMetric, iMetric), iMetric);
    {
        grad2.setColorAt(0, color2);
        grad2.setColorAt(1, color1);
    }
    /* Bottom-left corner: */
    QRadialGradient grad3(QPointF(iMetric, height() - iMetric), iMetric);
    {
        grad3.setColorAt(0, color2);
        grad3.setColorAt(1, color1);
    }
    /* Botom-right corner: */
    QRadialGradient grad4(QPointF(width() - iMetric, height() - iMetric), iMetric);
    {
        grad4.setColorAt(0, color2);
        grad4.setColorAt(1, color1);
    }

    /* Top line: */
    QLinearGradient grad5(QPointF(iMetric, 0), QPointF(iMetric, iMetric));
    {
        grad5.setColorAt(0, color1);
        grad5.setColorAt(1, color2);
    }
    /* Bottom line: */
    QLinearGradient grad6(QPointF(iMetric, height()), QPointF(iMetric, height() - iMetric));
    {
        grad6.setColorAt(0, color1);
        grad6.setColorAt(1, color2);
    }
    /* Left line: */
    QLinearGradient grad7(QPointF(0, height() - iMetric), QPointF(iMetric, height() - iMetric));
    {
        grad7.setColorAt(0, color1);
        grad7.setColorAt(1, color2);
    }
    /* Right line: */
    QLinearGradient grad8(QPointF(width(), height() - iMetric), QPointF(width() - iMetric, height() - iMetric));
    {
        grad8.setColorAt(0, color1);
        grad8.setColorAt(1, color2);
    }

    /* Paint shape/shadow: */
    painter.fillRect(QRect(iMetric,           iMetric,            width() - iMetric * 2, height() - iMetric * 2), color0);
    painter.fillRect(QRect(0,                 0,                  iMetric,               iMetric),                grad1);
    painter.fillRect(QRect(width() - iMetric, 0,                  iMetric,               iMetric),                grad2);
    painter.fillRect(QRect(0,                 height() - iMetric, iMetric,               iMetric),                grad3);
    painter.fillRect(QRect(width() - iMetric, height() - iMetric, iMetric,               iMetric),                grad4);
    painter.fillRect(QRect(iMetric,           0,                  width() - iMetric * 2, iMetric),                grad5);
    painter.fillRect(QRect(iMetric,           height() - iMetric, width() - iMetric * 2, iMetric),                grad6);
    painter.fillRect(QRect(0,                 iMetric,            iMetric,               height() - iMetric * 2), grad7);
    painter.fillRect(QRect(width() - iMetric, iMetric,            iMetric,               height() - iMetric * 2), grad8);
}

void UISnapshotDetailsElement::prepare()
{
    /* Create layout: */
    new QHBoxLayout(this);
    AssertPtrReturnVoid(layout());
    {
        /* Invent pixel metric: */
        const int iMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize) / 4;

        /* Configure layout: */
        layout()->setContentsMargins(iMetric, iMetric, iMetric, iMetric);

        /* Create text-browser if requested, text-edit otherwise: */
        m_pTextEdit = m_fLinkSupport ? new QTextBrowser : new QTextEdit;
        AssertPtrReturnVoid(m_pTextEdit);
        {
            /* Configure that we have: */
            m_pTextEdit->setReadOnly(true);
            m_pTextEdit->setFocusPolicy(Qt::NoFocus);
            m_pTextEdit->setFrameShape(QFrame::NoFrame);
            m_pTextEdit->viewport()->setAutoFillBackground(false);
            m_pTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_pTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            m_pTextEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
            if (m_fLinkSupport)
            {
                // WORKAROUND:
                // Intentionally using old kind of API here:
                connect(m_pTextEdit, SIGNAL(anchorClicked(const QUrl &)),
                        this, SIGNAL(sigAnchorClicked(const QUrl &)));
            }

            /* Add into layout: */
            layout()->addWidget(m_pTextEdit);
        }
    }
}


/*********************************************************************************************************************************
*   Class UIScreenshotViewer implementation.                                                                                   *
*********************************************************************************************************************************/

UIScreenshotViewer::UIScreenshotViewer(const QPixmap &pixmapScreenshot,
                                       const QString &strSnapshotName,
                                       const QString &strMachineName,
                                       QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QWidget>(pParent, Qt::Tool)
    , m_fPolished(false)
    , m_pixmapScreenshot(pixmapScreenshot)
    , m_strSnapshotName(strSnapshotName)
    , m_strMachineName(strMachineName)
    , m_pScrollArea(0)
    , m_pLabelPicture(0)
    , m_fZoomMode(true)
{
    /* Prepare: */
    prepare();
}

void UIScreenshotViewer::retranslateUi()
{
    /* Translate window title: */
    setWindowTitle(tr("Screenshot of %1 (%2)").arg(m_strSnapshotName).arg(m_strMachineName));
}

void UIScreenshotViewer::showEvent(QShowEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::showEvent(pEvent);

    /* Make sure we should polish dialog: */
    if (m_fPolished)
        return;

    /* Call to polish-event: */
    polishEvent(pEvent);

    /* Mark dialog as polished: */
    m_fPolished = true;
}

void UIScreenshotViewer::polishEvent(QShowEvent * /* pEvent */)
{
    /* Adjust the picture: */
    adjustPicture();
}

void UIScreenshotViewer::resizeEvent(QResizeEvent *pEvent)
{
    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::resizeEvent(pEvent);

    /* Adjust the picture: */
    adjustPicture();
}

void UIScreenshotViewer::mousePressEvent(QMouseEvent *pEvent)
{
    /* Toggle the zoom mode: */
    m_fZoomMode = !m_fZoomMode;

    /* Adjust the windiow size: */
    adjustWindowSize();
    /* Adjust the picture: */
    adjustPicture();

    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::mousePressEvent(pEvent);
}

void UIScreenshotViewer::keyPressEvent(QKeyEvent *pEvent)
{
    /* Close on escape: */
    if (pEvent->key() == Qt::Key_Escape)
        close();

    /* Call to base-class: */
    QIWithRetranslateUI2<QWidget>::keyPressEvent(pEvent);
}

void UIScreenshotViewer::prepare()
{
    /* Screenshot viewer is an application-modal window: */
    setWindowModality(Qt::ApplicationModal);
    /* With the pointing-hand cursor: */
    setCursor(Qt::PointingHandCursor);
    /* And it's being deleted when closed: */
    setAttribute(Qt::WA_DeleteOnClose);

    /* Create layout: */
    new QVBoxLayout(this);
    AssertPtrReturnVoid(layout());
    {
        /* Configure layout: */
        layout()->setContentsMargins(0, 0, 0, 0);

        /* Create scroll-area: */
        m_pScrollArea = new QScrollArea;
        AssertPtrReturnVoid(m_pScrollArea);
        {
            /* Configure scroll-area: */
            m_pScrollArea->setWidgetResizable (true);

            /* Create picture label: */
            m_pLabelPicture = new QLabel;
            AssertPtrReturnVoid(m_pLabelPicture);
            {
                /* Add into scroll-area: */
                m_pScrollArea->setWidget(m_pLabelPicture);
            }

            /* Add into layout: */
            layout()->addWidget(m_pScrollArea);
        }
    }

    /* Apply language settings: */
    retranslateUi();

    /* Adjust window size: */
    adjustWindowSize();

    /* Center according requested widget: */
    VBoxGlobal::centerWidget(this, parentWidget(), false);
}

void UIScreenshotViewer::adjustWindowSize()
{
    /* Acquire current host-screen size, fallback to 1024x768 if failed: */
    QSize screenSize = gpDesktop->screenGeometry(parentWidget()).size();
    if (!screenSize.isValid())
        screenSize = QSize(1024, 768);
    const int iInitWidth = screenSize.width() * .50 /* 50% of host-screen width */;

    /* Calculate screenshot aspect-ratio: */
    const double dAspectRatio = (double)m_pixmapScreenshot.height() / m_pixmapScreenshot.width();

    /* Calculate maximum window size: */
    const QSize maxSize = m_fZoomMode
                        ? screenSize * .9 /* 90% of host-screen size */ +
                          QSize(m_pScrollArea->frameWidth() * 2, m_pScrollArea->frameWidth() * 2)
                        : m_pixmapScreenshot.size() /* just the screenshot size */ +
                          QSize(m_pScrollArea->frameWidth() * 2, m_pScrollArea->frameWidth() * 2);

    /* Calculate initial window size: */
    const QSize initSize = QSize(iInitWidth, (int)(iInitWidth * dAspectRatio)).boundedTo(maxSize);

    /* Apply maximum window size restrictions: */
    setMaximumSize(maxSize);
    /* Apply initial window size: */
    resize(initSize);
}

void UIScreenshotViewer::adjustPicture()
{
    if (m_fZoomMode)
    {
        /* Adjust visual aspects: */
        m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_pLabelPicture->setPixmap(m_pixmapScreenshot.scaled(m_pScrollArea->viewport()->size(),
                                                             Qt::IgnoreAspectRatio,
                                                             Qt::SmoothTransformation));
        m_pLabelPicture->setToolTip(tr("Click to view non-scaled screenshot."));
    }
    else
    {
        /* Adjust visual aspects: */
        m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_pLabelPicture->setPixmap(m_pixmapScreenshot);
        m_pLabelPicture->setToolTip(tr("Click to view scaled screenshot."));
    }
}


/*********************************************************************************************************************************
*   Class UISnapshotDetailsWidget implementation.                                                                                *
*********************************************************************************************************************************/

UISnapshotDetailsWidget::UISnapshotDetailsWidget(QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pStackedLayout(0)
    , m_pEmptyWidget(0)
    , m_pEmptyWidgetLabel(0)
    , m_pTabWidget(0)
    , m_pLayoutOptions(0)
    , m_pLabelName(0), m_pEditorName(0), m_pErrorPaneName(0)
    , m_pLabelDescription(0), m_pBrowserDescription(0), m_pErrorPaneDescription(0)
    , m_pButtonBox(0)
    , m_pLayoutDetails(0)
    , m_pScrollAreaDetails(0)
{
    /* Prepare: */
    prepare();
}

void UISnapshotDetailsWidget::setData(const UIDataSnapshot &data, const CSnapshot &comSnapshot)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;

    /* Cache snapshot: */
    m_comSnapshot = comSnapshot;

    /* Load snapshot data: */
    loadSnapshotData();
}

void UISnapshotDetailsWidget::clearData()
{
    /* Reset old/new data: */
    m_oldData = UIDataSnapshot();
    m_newData = m_oldData;

    /* Reset snapshot: */
    m_comSnapshot = CSnapshot();

    /* Load snapshot data: */
    loadSnapshotData();
}

void UISnapshotDetailsWidget::retranslateUi()
{
    /* Translate labels: */
    m_pEmptyWidgetLabel->setText(tr("<p>You have the <b>Current State</b> item selected.<br>"
                                    "Press the <b>Take</b> button if you wish to take a new snapshot.</p>"));
    m_pTabWidget->setTabText(0, tr("&Attributes"));
    m_pTabWidget->setTabText(1, tr("&Information"));
    m_pLabelName->setText(tr("&Name:"));
    m_pLabelDescription->setText(tr("&Description:"));
    m_pEditorName->setToolTip(tr("Holds the snapshot name."));
    m_pBrowserDescription->setToolTip(tr("Holds the snapshot description."));
    m_pButtonBox->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Reset"));
    m_pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(QString("Ctrl+Return"));
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
    m_pButtonBox->button(QDialogButtonBox::Ok)->setStatusTip(tr("Apply changes in current snapshot details"));
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setStatusTip(tr("Reset changes in current snapshot details"));
    m_pButtonBox->button(QDialogButtonBox::Ok)->
        setToolTip(tr("Apply Changes (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Ok)->shortcut().toString()));
    m_pButtonBox->button(QDialogButtonBox::Cancel)->
        setToolTip(tr("Reset Changes (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Cancel)->shortcut().toString()));

    /* And if snapshot is valid: */
    if (!m_comSnapshot.isNull())
    {
        /* Get the machine: */
        const CMachine comMachine = m_comSnapshot.GetMachine();

        /* Update the picture tool-tip and visibility: */
        m_details.value(DetailsElementType_Preview)->setToolTip(tr("Click to enlarge the screenshot."));
        if (!m_pixmapScreenshot.isNull() && m_details.value(DetailsElementType_Preview)->isHidden())
            m_details.value(DetailsElementType_Preview)->setHidden(false);
        else if (m_pixmapScreenshot.isNull() && !m_details.value(DetailsElementType_Preview)->isHidden())
            m_details.value(DetailsElementType_Preview)->setHidden(true);

        /* Update USB details visibility: */
        const CUSBDeviceFilters &comFilters = comMachine.GetUSBDeviceFilters();
        const bool fUSBMissing = comFilters.isNull() || !comMachine.GetUSBProxyAvailable();
        if (fUSBMissing && !m_details.value(DetailsElementType_USB)->isHidden())
            m_details.value(DetailsElementType_USB)->setHidden(true);

        /* Rebuild the details report: */
        foreach (const DetailsElementType &enmType, m_details.keys())
            m_details.value(enmType)->setText(detailsReport(comMachine, enmType));
    }
    else
    {
        /* Clear the details report: */
        // WORKAROUND:
        // How stupid Qt *is* to wipe out registered icons not just on clear()
        // call but on setText(QString()) and even setText("") as well.
        // Nice way to oversmart itself..
        foreach (const DetailsElementType &enmType, m_details.keys())
            m_details.value(enmType)->setText("<empty>");
    }

    /* Retranslate validation: */
    retranslateValidation();
}

void UISnapshotDetailsWidget::sltHandleNameChange()
{
    m_newData.m_strName = m_pEditorName->text();
    revalidate(m_pErrorPaneName);
    updateButtonStates();
}

void UISnapshotDetailsWidget::sltHandleDescriptionChange()
{
    m_newData.m_strDescription = m_pBrowserDescription->toPlainText();
    revalidate(m_pErrorPaneDescription);
    updateButtonStates();
}

void UISnapshotDetailsWidget::sltHandleAnchorClicked(const QUrl &link)
{
    /* Get the link out of url: */
    const QString strLink = link.toString();
    if (strLink == "#thumbnail")
    {
        /* We are creating screenshot viewer and show it: */
        UIScreenshotViewer *pViewer = new UIScreenshotViewer(m_pixmapScreenshot,
                                                             m_comSnapshot.GetMachine().GetName(),
                                                             m_comSnapshot.GetName(),
                                                             this);
        pViewer->show();
        pViewer->activateWindow();
    }
}

void UISnapshotDetailsWidget::sltHandleChangeAccepted()
{
    /* Disable buttons first of all: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);

    /* Notify listeners: */
    emit sigDataChangeAccepted();
}

void UISnapshotDetailsWidget::sltHandleChangeRejected()
{
    /* Reset new data to old: */
    m_newData = m_oldData;

    /* Load snapshot data: */
    loadSnapshotData();
}

void UISnapshotDetailsWidget::prepare()
{
    /* Create stacked layout: */
    m_pStackedLayout = new QStackedLayout(this);
    AssertPtrReturnVoid(m_pStackedLayout);
    {
        /* Prepare empty-widget: */
        prepareEmptyWidget();
        /* Prepare tab-widget: */
        prepareTabWidget();
    }
}

void UISnapshotDetailsWidget::prepareEmptyWidget()
{
    /* Create empty-widget: */
    m_pEmptyWidget = new QWidget;
    AssertPtrReturnVoid(m_pEmptyWidget);
    {
        /* Create empty-widget layout: */
        new QVBoxLayout(m_pEmptyWidget);
        AssertPtrReturnVoid(m_pEmptyWidget->layout());
        {
            /* Create empty-widget label: */
            m_pEmptyWidgetLabel = new QLabel;
            {
                /* Configure label: */
                QFont font = m_pEmptyWidgetLabel->font();
                font.setPointSize(font.pointSize() * 1.5);
                m_pEmptyWidgetLabel->setAlignment(Qt::AlignCenter);
                m_pEmptyWidgetLabel->setFont(font);

                /* Add into layout: */
                m_pEmptyWidget->layout()->addWidget(m_pEmptyWidgetLabel);
            }
        }

        /* Add into layout: */
        m_pStackedLayout->addWidget(m_pEmptyWidget);
    }
}

void UISnapshotDetailsWidget::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QTabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare 'Options' tab: */
        prepareTabOptions();
        /* Prepare 'Details' tab: */
        prepareTabDetails();

        /* Add into layout: */
        m_pStackedLayout->addWidget(m_pTabWidget);
    }
}

void UISnapshotDetailsWidget::prepareTabOptions()
{
    /* Create widget itself: */
    QWidget *pWidget = new QWidget;
    AssertPtrReturnVoid(pWidget);
    {
        /* Create 'Options' layout: */
        m_pLayoutOptions = new QGridLayout(pWidget);
        AssertPtrReturnVoid(m_pLayoutOptions);
        {
#ifdef VBOX_WS_MAC
            /* Configure layout: */
            m_pLayoutOptions->setSpacing(10);
            m_pLayoutOptions->setContentsMargins(10, 10, 10, 10);
#endif

            /* Get the required icon metric: */
            const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);

            /* Create name label: */
            m_pLabelName = new QLabel;
            AssertPtrReturnVoid(m_pLabelName);
            {
                /* Configure label: */
                m_pLabelName->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelName, 0, 0);
            }
            /* Create name layout: */
            QHBoxLayout *pLayoutName = new QHBoxLayout;
            AssertPtrReturnVoid(pLayoutName);
            {
                /* Create name editor: */
                m_pEditorName = new QLineEdit;
                AssertPtrReturnVoid(m_pEditorName);
                {
                    /* Configure editor: */
                    m_pLabelName->setBuddy(m_pEditorName);
                    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Minimum);
                    policy.setHorizontalStretch(1);
                    m_pEditorName->setSizePolicy(policy);
                    connect(m_pEditorName, &QLineEdit::textChanged,
                            this, &UISnapshotDetailsWidget::sltHandleNameChange);

                    /* Add into layout: */
                    pLayoutName->addWidget(m_pEditorName);
                }
                /* Create name error pane: */
                m_pErrorPaneName = new QLabel;
                AssertPtrReturnVoid(m_pErrorPaneName);
                {
                    /* Configure error pane: */
                    m_pErrorPaneName->setAlignment(Qt::AlignCenter);
                    m_pErrorPaneName->setPixmap(UIIconPool::iconSet(":/status_error_16px.png")
                                                .pixmap(QSize(iIconMetric, iIconMetric)));

                    /* Add into layout: */
                    pLayoutName->addWidget(m_pErrorPaneName);
                }

                /* Add into layout: */
                m_pLayoutOptions->addLayout(pLayoutName, 0, 1);
            }

            /* Create description label: */
            m_pLabelDescription = new QLabel;
            AssertPtrReturnVoid(m_pLabelDescription);
            {
                /* Configure label: */
                m_pLabelDescription->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignTop);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pLabelDescription, 1, 0);
            }
            /* Create description layout: */
            QHBoxLayout *pLayoutDescription = new QHBoxLayout;
            AssertPtrReturnVoid(pLayoutDescription);
            {
                /* Create description browser: */
                m_pBrowserDescription = new QTextEdit;
                AssertPtrReturnVoid(m_pBrowserDescription);
                {
                    /* Configure browser: */
                    m_pLabelDescription->setBuddy(m_pBrowserDescription);
                    m_pBrowserDescription->setTabChangesFocus(true);
                    m_pBrowserDescription->setAcceptRichText(false);
                    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
                    policy.setHorizontalStretch(1);
                    m_pBrowserDescription->setSizePolicy(policy);
                    connect(m_pBrowserDescription, &QTextEdit::textChanged,
                            this, &UISnapshotDetailsWidget::sltHandleDescriptionChange);

                    /* Add into layout: */
                    pLayoutDescription->addWidget(m_pBrowserDescription);
                }
                /* Create description error pane: */
                m_pErrorPaneDescription = new QLabel;
                AssertPtrReturnVoid(m_pErrorPaneDescription);
                {
                    /* Configure error pane: */
                    m_pErrorPaneDescription->setAlignment(Qt::AlignCenter);
                    m_pErrorPaneDescription->setPixmap(UIIconPool::iconSet(":/status_error_16px.png")
                                                       .pixmap(QSize(iIconMetric, iIconMetric)));

                    /* Add into layout: */
                    pLayoutDescription->addWidget(m_pErrorPaneDescription);
                }

                /* Add into layout: */
                m_pLayoutOptions->addLayout(pLayoutDescription, 1, 1);
            }

            /* Create button-box: */
            m_pButtonBox = new QIDialogButtonBox;
            AssertPtrReturnVoid(m_pButtonBox);
            {
                /* Configure button-box: */
                m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UISnapshotDetailsWidget::sltHandleChangeAccepted);
                connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UISnapshotDetailsWidget::sltHandleChangeRejected);

                /* Add into layout: */
                m_pLayoutOptions->addWidget(m_pButtonBox, 2, 0, 1, 2);
            }
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(pWidget, QString());
    }
}

void UISnapshotDetailsWidget::prepareTabDetails()
{
    /* Create details scroll-area: */
    m_pScrollAreaDetails = new QScrollArea;
    AssertPtrReturnVoid(m_pScrollAreaDetails);
    {
        /* Configure browser: */
        m_pScrollAreaDetails->setWidgetResizable(true);
        m_pScrollAreaDetails->setFrameShadow(QFrame::Plain);
        m_pScrollAreaDetails->setFrameShape(QFrame::NoFrame);
        m_pScrollAreaDetails->viewport()->setAutoFillBackground(false);

        /* Create details widget: */
        QWidget *pWidgetDetails = new QWidget;
        AssertPtrReturnVoid(pWidgetDetails);
        {
            /* Create 'Details' layout: */
            m_pLayoutDetails = new QVBoxLayout(pWidgetDetails);
            AssertPtrReturnVoid(m_pLayoutDetails);
            {
                /* Configure layout: */
                m_pLayoutDetails->setSpacing(5);
#ifdef VBOX_WS_MAC
                m_pLayoutDetails->setContentsMargins(10, 10, 10, 10);
#endif

                /* Create layout 1: */
                QHBoxLayout *pLayout1 = new QHBoxLayout;
                AssertPtrReturnVoid(pLayout1);
                {
                    /* Create left layout: */
                    QIFlowLayout *pLayoutLeft = new QIFlowLayout;
                    AssertPtrReturnVoid(pLayoutLeft);
                    {
                        /* Configure layout: */
                        pLayoutLeft->setSpacing(5);
                        pLayoutLeft->setContentsMargins(0, 0, 0, 0);

                        /* Create 'General' element: */
                        m_details[DetailsElementType_General] = createDetailsElement(DetailsElementType_General);
                        AssertPtrReturnVoid(m_details[DetailsElementType_General]);
                        pLayoutLeft->addWidget(m_details[DetailsElementType_General]);

                        /* Create 'System' element: */
                        m_details[DetailsElementType_System] = createDetailsElement(DetailsElementType_System);
                        AssertPtrReturnVoid(m_details[DetailsElementType_System]);
                        pLayoutLeft->addWidget(m_details[DetailsElementType_System]);

                        /* Add to layout: */
                        pLayout1->addLayout(pLayoutLeft);
                    }

                    /* Create right layout: */
                    QVBoxLayout *pLayoutRight = new QVBoxLayout;
                    AssertPtrReturnVoid(pLayoutRight);
                    {
                        /* Configure layout: */
                        pLayoutRight->setContentsMargins(0, 0, 0, 0);

                        /* Create 'Preview' element: */
                        m_details[DetailsElementType_Preview] = createDetailsElement(DetailsElementType_Preview);
                        AssertPtrReturnVoid(m_details[DetailsElementType_Preview]);
                        connect(m_details[DetailsElementType_Preview], &UISnapshotDetailsElement::sigAnchorClicked,
                                this, &UISnapshotDetailsWidget::sltHandleAnchorClicked);
                        pLayoutRight->addWidget(m_details[DetailsElementType_Preview]);
                        pLayoutRight->addStretch();

                        /* Add to layout: */
                        pLayout1->addLayout(pLayoutRight);
                    }

                    /* Add into layout: */
                    m_pLayoutDetails->addLayout(pLayout1);
                }

                /* Create layout 2: */
                QIFlowLayout *pLayout2 = new QIFlowLayout;
                {
                    /* Configure layout: */
                    pLayout2->setSpacing(5);

                    /* Create 'Display' element: */
                    m_details[DetailsElementType_Display] = createDetailsElement(DetailsElementType_Display);
                    AssertPtrReturnVoid(m_details[DetailsElementType_Display]);
                    pLayout2->addWidget(m_details[DetailsElementType_Display]);

                    /* Create 'Audio' element: */
                    m_details[DetailsElementType_Audio] = createDetailsElement(DetailsElementType_Audio);
                    AssertPtrReturnVoid(m_details[DetailsElementType_Audio]);
                    pLayout2->addWidget(m_details[DetailsElementType_Audio]);

                    /* Create 'Storage' element: */
                    m_details[DetailsElementType_Storage] = createDetailsElement(DetailsElementType_Storage);
                    AssertPtrReturnVoid(m_details[DetailsElementType_Storage]);
                    pLayout2->addWidget(m_details[DetailsElementType_Storage]);

                    /* Create 'Network' element: */
                    m_details[DetailsElementType_Network] = createDetailsElement(DetailsElementType_Network);
                    AssertPtrReturnVoid(m_details[DetailsElementType_Network]);
                    pLayout2->addWidget(m_details[DetailsElementType_Network]);

                    /* Create 'Serial' element: */
                    m_details[DetailsElementType_Serial] = createDetailsElement(DetailsElementType_Serial);
                    AssertPtrReturnVoid(m_details[DetailsElementType_Serial]);
                    pLayout2->addWidget(m_details[DetailsElementType_Serial]);

                    /* Create 'USB' element: */
                    m_details[DetailsElementType_USB] = createDetailsElement(DetailsElementType_USB);
                    AssertPtrReturnVoid(m_details[DetailsElementType_USB]);
                    pLayout2->addWidget(m_details[DetailsElementType_USB]);

                    /* Create 'SF' element: */
                    m_details[DetailsElementType_SF] = createDetailsElement(DetailsElementType_SF);
                    AssertPtrReturnVoid(m_details[DetailsElementType_SF]);
                    pLayout2->addWidget(m_details[DetailsElementType_SF]);

                    /* Add into layout: */
                    m_pLayoutDetails->addLayout(pLayout2);
                }

                /* Add stretch: */
                m_pLayoutDetails->addStretch();
            }

            /* Add to scroll-area: */
            m_pScrollAreaDetails->setWidget(pWidgetDetails);
            pWidgetDetails->setAutoFillBackground(false);
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(m_pScrollAreaDetails, QString());
    }
}

/* static */
UISnapshotDetailsElement *UISnapshotDetailsWidget::createDetailsElement(DetailsElementType enmType)
{
    /* Create element: */
    const bool fWithHypertextNavigation = enmType == DetailsElementType_Preview;
    UISnapshotDetailsElement *pElement = new UISnapshotDetailsElement(fWithHypertextNavigation);
    AssertPtrReturn(pElement, 0);
    {
        /* Configure element: */
        switch (enmType)
        {
            case DetailsElementType_Preview:
                pElement->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
                break;
            default:
                pElement->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
                break;
        }

        /* Register DetailsElementType icon in the element text-document: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        const QSize iconSize = QSize(iIconMetric, iIconMetric);
        pElement->document()->addResource(
            QTextDocument::ImageResource,
            QUrl(QString("details://%1").arg(gpConverter->toInternalString(enmType))),
            QVariant(gpConverter->toIcon(enmType).pixmap(iconSize)));
    }
    /* Return element: */
    return pElement;
}

void UISnapshotDetailsWidget::loadSnapshotData()
{
    /* Read general snapshot properties: */
    m_pEditorName->setText(m_newData.m_strName);
    m_pBrowserDescription->setText(m_newData.m_strDescription);

    /* If there is a snapshot: */
    if (m_comSnapshot.isNotNull())
    {
        /* Choose the tab-widget as current one: */
        m_pStackedLayout->setCurrentWidget(m_pTabWidget);

        /* Read snapshot display contents: */
        CMachine comMachine = m_comSnapshot.GetMachine();
        ULONG iWidth = 0, iHeight = 0;

        /* Get screenshot if present: */
        QVector<BYTE> screenData = comMachine.ReadSavedScreenshotToArray(0, KBitmapFormat_PNG, iWidth, iHeight);
        m_pixmapScreenshot = screenData.size() != 0 ? QPixmap::fromImage(QImage::fromData(screenData.data(),
                                                                                          screenData.size(),
                                                                                          "PNG"))
                                                    : QPixmap();

        /* Register thumbnail pixmap in preview element: */
        // WORKAROUND:
        // We are generating it from the screenshot because thumbnail
        // returned by the CMachine::ReadSavedThumbnailToArray is too small.
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize);
        const QSize thumbnailSize = QSize(iIconMetric * 4, iIconMetric * 4);
        const QPixmap pixThumbnail = m_pixmapScreenshot.isNull() ? m_pixmapScreenshot
                                   : m_pixmapScreenshot.scaled(thumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_details.value(DetailsElementType_Preview)->document()->addResource(
            QTextDocument::ImageResource, QUrl("details://thumbnail"), QVariant(pixThumbnail));
    }
    else
    {
        /* Choose the empty-widget as current one: */
        m_pStackedLayout->setCurrentWidget(m_pEmptyWidget);
    }

    /* Retranslate: */
    retranslateUi();

    /* Update button states finally: */
    updateButtonStates();
}

void UISnapshotDetailsWidget::revalidate(QWidget *pWidget /* = 0 */)
{
    if (!pWidget || pWidget == m_pErrorPaneName)
    {
        const bool fError = m_newData.m_strName.isEmpty();
        m_pErrorPaneName->setVisible(fError);
    }
    if (!pWidget || pWidget == m_pErrorPaneDescription)
    {
        const bool fError = false;
        m_pErrorPaneDescription->setVisible(fError);
    }

    /* Retranslate validation: */
    retranslateValidation(pWidget);
}

void UISnapshotDetailsWidget::retranslateValidation(QWidget *pWidget /* = 0 */)
{
    if (!pWidget || pWidget == m_pErrorPaneName)
        m_pErrorPaneName->setToolTip(tr("Snapshot name is empty"));
}

void UISnapshotDetailsWidget::updateButtonStates()
{
//    if (m_oldData != m_newData)
//        printf("Snapshot: %s, %s\n",
//               m_newData.m_strName.toUtf8().constData(),
//               m_newData.m_strDescription.toUtf8().constData());

    /* Update 'Apply' / 'Reset' button states: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_oldData != m_newData);
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(m_oldData != m_newData);
}

QString UISnapshotDetailsWidget::detailsReport(const CMachine &comMachine, DetailsElementType enmType)
{
    /* Details templates: */
    static const char *sTableTpl =
        "<table border=0 cellspacing=1 cellpadding=0 style='white-space:pre'>%1</table>";
    static const char *sSectionBoldTpl1 =
        "<tr>"
        "<td width=%3 rowspan=%1 align=left><img src='%2'></td>"
        "<td colspan=3><nobr><b>%4</b></nobr></td>"
        "</tr>"
        "%5";
    static const char *sSectionBoldTpl2 =
        "<tr>"
        "<td width=%3 rowspan=%1 align=left><img src='%2'></td>"
        "<td><nobr><b>%4</b></nobr></td>"
        "</tr>"
        "%5";
    static const char *sSectionItemTpl1 =
        "<tr><td><nobr><i>%1</i></nobr></td><td/><td/></tr>";
    static const char *sSectionItemTpl2 =
        "<tr><td><nobr>%1:</nobr></td><td/><td>%2</td></tr>";
    static const char *sSectionItemTpl3 =
        "<tr><td><nobr>%1</nobr></td><td/><td/></tr>";
    static const char *sSectionItemTpl4 =
        "<tr><td><a href='%2'><img src='%1'/></a></td></tr>";

    /* Use the const ref on the basis of implicit QString constructor: */
    const QString &strSectionTpl = enmType == DetailsElementType_Preview
                                 ? sSectionBoldTpl2 : sSectionBoldTpl1;

    /* Determine icon metric: */
    const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
    const int iIconArea = iIconMetric * 1.375;

    /* Compose report: */
    QString strReport;
    switch (enmType)
    {
        case DetailsElementType_General:
        {
            /* Name, Operating System: */
            int iRowCount = 2;
            QString strItem = QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Name", "details (general)"),
                                                            comMachine.GetName())
                            + QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Operating System", "details (general)"),
                                                            vboxGlobal().vmGuestOSTypeDescription(comMachine.GetOSTypeId()));

            /* Groups? */
            QStringList aGroups = comMachine.GetGroups().toList();
            /* Do not show groups for machine which is in root group only: */
            if (aGroups.size() == 1)
                aGroups.removeAll("/");
            /* If group list still not empty: */
            if (!aGroups.isEmpty())
            {
                ++iRowCount;
                /* For all groups => Trim first '/' symbol: */
                for (int i = 0; i < aGroups.size(); ++i)
                {
                    QString &strGroup = aGroups[i];
                    if (strGroup.startsWith("/") && strGroup != "/")
                        strGroup.remove(0, 1);
                }
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Groups", "details (general)"),
                                                         aGroups.join(", "));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://general", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem /* items */);

            break;
        }
        case DetailsElementType_System:
        {
            /* Base Memory: */
            int iRowCount = 1;
            QString strItem = QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Base Memory", "details (system)"),
                                                            QApplication::translate("UIGDetails", "%1 MB", "details").arg(comMachine.GetMemorySize()));

            /* Processors? */
            const int cCPU = comMachine.GetCPUCount();
            if (cCPU > 1)
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Processors", "details (system)"),
                                                         QString::number(cCPU));
            }

            /* Execution Cap? */
            const ULONG uExecutionCap = comMachine.GetCPUExecutionCap();
            if (uExecutionCap < 100)
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Execution Cap", "details (system)"),
                                                         QApplication::translate("UIGDetails", "%1%", "details").arg(uExecutionCap));
            }

            /* Boot Order: */
            ++iRowCount;
            QStringList aBootOrder;
            for (ulong i = 1; i <= vboxGlobal().virtualBox().GetSystemProperties().GetMaxBootPosition(); ++i)
            {
                const KDeviceType enmDevice = comMachine.GetBootOrder(i);
                if (enmDevice != KDeviceType_Null)
                    aBootOrder << gpConverter->toString(enmDevice);
            }
            if (aBootOrder.isEmpty())
                aBootOrder << gpConverter->toString(KDeviceType_Null);
            strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Boot Order", "details (system)"),
                                                     aBootOrder.join(", "));

            /* Chipset type? */
            const KChipsetType enmChipsetType = comMachine.GetChipsetType();
            if (enmChipsetType == KChipsetType_ICH9)
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Chipset Type", "details (system)"),
                                                         gpConverter->toString(enmChipsetType));
            }

            /* Firware type? */
            switch (comMachine.GetFirmwareType())
            {
                case KFirmwareType_EFI:
                case KFirmwareType_EFI32:
                case KFirmwareType_EFI64:
                case KFirmwareType_EFIDUAL:
                {
                    ++iRowCount;
                    const QString strEFI = QApplication::translate("UIGDetails", "Enabled", "details (system/EFI)");
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "EFI", "details (system)"), strEFI);
                    break;
                }
                default:
                {
                    //++iRowCount;
                    const QString strEFI = QApplication::translate("UIGDetails", "Disabled", "details (system/EFI)"); Q_UNUSED(strEFI);
                    //strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "EFI", "details (system)"), strEFI);
                    break;
                }
            }

            /* Acceleration? */
            QStringList aAcceleration;
            /* VT-x/AMD-V and Nested Paging? */
            if (vboxGlobal().host().GetProcessorFeature(KProcessorFeature_HWVirtEx))
            {
                /* VT-x/AMD-V? */
                if (comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_Enabled))
                {
                    aAcceleration << QApplication::translate("UIGDetails", "VT-x/AMD-V", "details (system)");
                    /* Nested Paging? */
                    if (comMachine.GetHWVirtExProperty(KHWVirtExPropertyType_NestedPaging))
                        aAcceleration << QApplication::translate("UIGDetails", "Nested Paging", "details (system)");
                }

            }
            /* PAE/NX? */
            if (comMachine.GetCPUProperty(KCPUPropertyType_PAE))
                aAcceleration << QApplication::translate("UIGDetails", "PAE/NX", "details (system)");
            /* Paravirtualization Interface? */
            switch (comMachine.GetEffectiveParavirtProvider())
            {
                case KParavirtProvider_Minimal: aAcceleration << QApplication::translate("UIGDetails", "Minimal Paravirtualization", "details (system)"); break;
                case KParavirtProvider_HyperV:  aAcceleration << QApplication::translate("UIGDetails", "Hyper-V Paravirtualization", "details (system)"); break;
                case KParavirtProvider_KVM:     aAcceleration << QApplication::translate("UIGDetails", "KVM Paravirtualization", "details (system)"); break;
                default: break;
            }
            if (!aAcceleration.isEmpty())
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Acceleration", "details (system)"), aAcceleration.join(", "));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://system", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_Preview:
        {
            /* Preview: */
            int iRowCount = 1;
            QString strItem = QString(sSectionItemTpl4).arg("details://thumbnail").arg("#thumbnail");

            /* Append report: */
            if (!m_pixmapScreenshot.isNull())
                strReport += strSectionTpl
                    .arg(1 + iRowCount) /* rows */
                    .arg("details://preview", /* icon */
                         QString::number(iIconArea), /* icon area */
                         gpConverter->toString(enmType), /* title */
                         strItem); /* items */

            break;
        }
        case DetailsElementType_Display:
        {
            // WORKAROUND:
            // IMachine::GetExtraData still non-const..
            CMachine comExtraDataMachine = comMachine;

            /* Video Memory: */
            int iRowCount = 1;
            QString strItem = QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Video Memory", "details (display)"),
                                                            QApplication::translate("UIGDetails", "%1 MB", "details").arg(comMachine.GetVRAMSize()));

            /* Screens? */
            const int cScreens = comMachine.GetMonitorCount();
            if (cScreens > 1)
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Screens", "details (display)"),
                                                         QString::number(cScreens));
            }

            /* Scale-factor? */
            const QString strScaleFactor = comExtraDataMachine.GetExtraData(UIExtraDataDefs::GUI_ScaleFactor);
            {
                /* Try to convert loaded data to double: */
                bool fOk = false;
                double dValue = strScaleFactor.toDouble(&fOk);
                /* Invent the default value: */
                if (!fOk || !dValue)
                    dValue = 1.0;
                /* Append information: */
                if (dValue != 1.0)
                {
                    ++iRowCount;
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Scale-factor", "details (display)"),
                                                             QString::number(dValue, 'f', 2));
                }
            }

#ifdef VBOX_WS_MAC
            /* Unscaled HiDPI Video Output? */
            const QString strUnscaledHiDPIMode = comExtraDataMachine.GetExtraData(UIExtraDataDefs::GUI_HiDPI_UnscaledOutput);
            {
                /* Try to convert loaded data to bool: */
                const bool fEnabled  = strUnscaledHiDPIMode.compare("true", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode.compare("yes", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode.compare("on", Qt::CaseInsensitive) == 0 ||
                                       strUnscaledHiDPIMode == "1";
                /* Append information: */
                if (fEnabled)
                {
                    ++iRowCount;
                    const QString strHiDPI = QApplication::translate("UIGDetails", "Enabled", "details (display/Unscaled HiDPI Video Output)");
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Unscaled HiDPI Video Output", "details (display)"), strHiDPI);
                }
                else
                {
                    //++iRowCount;
                    const QString strHiDPI = QApplication::translate("UIGDetails", "Disabled", "details (display/Unscaled HiDPI Video Output)"); Q_UNUSED(strHiDPI);
                    //strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Unscaled HiDPI Video Output", "details (display)"), strHiDPI);
                }
            }
#endif /* VBOX_WS_MAC */

            /* Acceleration? */
            QStringList aAcceleration;
#ifdef VBOX_WITH_VIDEOHWACCEL
            /* 2D Video Acceleration? */
            if (comMachine.GetAccelerate2DVideoEnabled())
                aAcceleration << QApplication::translate("UIGDetails", "2D Video", "details (display)");
#endif
            /* 3D Acceleration? */
            if (comMachine.GetAccelerate3DEnabled() && vboxGlobal().is3DAvailable())
                aAcceleration << QApplication::translate("UIGDetails", "3D", "details (display)");
            if (!aAcceleration.isEmpty())
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Acceleration", "details (display)"), aAcceleration.join(", "));
            }

            /* Remote Desktop Server: */
            const CVRDEServer comServer = comMachine.GetVRDEServer();
            if (!comServer.isNull())
            {
                ++iRowCount;
                if (comServer.GetEnabled())
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Remote Desktop Server Port", "details (display/vrde)"),
                                                             comServer.GetVRDEProperty("TCP/Ports"));
                else
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Remote Desktop Server", "details (display/vrde)"),
                                                             QApplication::translate("UIGDetails", "Disabled", "details (display/vrde/VRDE server)"));
            }

            /* Video Capture: */
            if (comMachine.GetVideoCaptureEnabled())
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Video Capture File", "details (display/video capture)"),
                                                         comMachine.GetVideoCaptureFile());
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Video Capture Attributes", "details (display/video capture)"),
                                                         QApplication::translate("UIGDetails", "Frame Size: %1x%2, Frame Rate: %3fps, Bit Rate: %4kbps")
                                                             .arg(comMachine.GetVideoCaptureWidth()).arg(comMachine.GetVideoCaptureHeight())
                                                             .arg(comMachine.GetVideoCaptureFPS()).arg(comMachine.GetVideoCaptureRate()));
            }
            else
            {
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Video Capture", "details (display/video capture)"),
                                                         QApplication::translate("UIGDetails", "Disabled", "details (display/video capture)"));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://display", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_Storage:
        {
            /* Nothing: */
            int iRowCount = 0;
            QString strItem;

            /* Iterate over the all machine controllers: */
            foreach (const CStorageController &comController, comMachine.GetStorageControllers())
            {
                /* Add controller information: */
                ++iRowCount;
                const QString strControllerName = QApplication::translate("UIMachineSettingsStorage", "Controller: %1");
                strItem += QString(sSectionItemTpl3).arg(strControllerName.arg(comController.GetName()));

                /* Populate sorted map with attachments information: */
                QMap<StorageSlot,QString> attachmentsMap;
                foreach (const CMediumAttachment &comAttachment, comMachine.GetMediumAttachmentsOfController(comController.GetName()))
                {
                    /* Prepare current storage slot: */
                    const StorageSlot attachmentSlot(comController.GetBus(), comAttachment.GetPort(), comAttachment.GetDevice());
                    /* Append 'device slot name' with 'device type name' for optical devices only: */
                    QString strDeviceType = comAttachment.GetType() == KDeviceType_DVD
                                          ? QApplication::translate("UIGDetails", "[Optical Drive]", "details (storage)")
                                          : QString();
                    if (!strDeviceType.isNull())
                        strDeviceType.prepend(' ');
                    /* Prepare current medium object: */
                    const CMedium &medium = comAttachment.GetMedium();
                    /* Prepare information about current medium & attachment: */
                    QString strAttachmentInfo = !comAttachment.isOk()
                                              ? QString()
                                              : QString(sSectionItemTpl2)
                                                .arg(QString("&nbsp;&nbsp;") +
                                                     gpConverter->toString(StorageSlot(comController.GetBus(),
                                                                                       comAttachment.GetPort(),
                                                                                       comAttachment.GetDevice())) + strDeviceType)
                                                .arg(vboxGlobal().details(medium, false));
                    /* Insert that attachment into map: */
                    if (!strAttachmentInfo.isNull())
                        attachmentsMap.insert(attachmentSlot, strAttachmentInfo);
                }

                /* Iterate over the sorted map with attachments information: */
                QMapIterator<StorageSlot, QString> it(attachmentsMap);
                while (it.hasNext())
                {
                    /* Add controller information: */
                    it.next();
                    ++iRowCount;
                    strItem += it.value();
                }
            }

            /* Handle side-case: */
            if (strItem.isNull())
            {
                ++iRowCount;
                strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "Not Attached", "details (storage)"));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://storage", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_Audio:
        {
            /* Nothing: */
            int iRowCount = 0;
            QString strItem;

            /* Host Driver, Controller? */
            const CAudioAdapter &comAudio = comMachine.GetAudioAdapter();
            if (comAudio.GetEnabled())
            {
                iRowCount += 2;
                strItem = QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Host Driver", "details (audio)"),
                                                        gpConverter->toString(comAudio.GetAudioDriver()))
                        + QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Controller", "details (audio)"),
                                                        gpConverter->toString(comAudio.GetAudioController()));

#ifdef VBOX_WITH_AUDIO_INOUT_INFO
                /* Output: */
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Audio Output", "details (audio)"),
                                                         comAudio.GetEnabledOut() ?
                                                         QApplication::translate("UIGDetails", "Enabled", "details (audio/output)") :
                                                         QApplication::translate("UIGDetails", "Disabled", "details (audio/output)"));

                /* Input: */
                ++iRowCount;
                strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Audio Input", "details (audio)"),
                                                         comAudio.GetEnabledIn() ?
                                                         QApplication::translate("UIGDetails", "Enabled", "details (audio/input)") :
                                                         QApplication::translate("UIGDetails", "Disabled", "details (audio/input)"));
#endif /* VBOX_WITH_AUDIO_INOUT_INFO */
            }
            else
            {
                ++iRowCount;
                strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "Disabled", "details (audio)"));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://audio", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_Network:
        {
            /* Nothing: */
            int iRowCount = 0;
            QString strItem;

            /* Enumerate all the network adapters (up to acquired/limited count): */
            const ulong iCount = vboxGlobal().virtualBox().GetSystemProperties().GetMaxNetworkAdapters(comMachine.GetChipsetType());
            for (ulong iSlot = 0; iSlot < iCount; ++iSlot)
            {
                /* Get current network adapter: */
                const CNetworkAdapter &comNetwork = comMachine.GetNetworkAdapter(iSlot);
                if (comNetwork.GetEnabled())
                {
                    /* Determine attachment type: */
                    const KNetworkAttachmentType enmType = comNetwork.GetAttachmentType();
                    QString attType = gpConverter->toString(comNetwork.GetAdapterType())
                                      .replace(QRegExp("\\s\\(.+\\)"), " (%1)");
                    /* Don't use the adapter type string for types that have
                     * an additional symbolic network/interface name field,
                     * use this name instead: */
                    switch (enmType)
                    {
                        case KNetworkAttachmentType_Bridged:
                            attType = attType.arg(QApplication::translate("UIGDetails", "Bridged Adapter, %1", "details (network)")
                                                     .arg(comNetwork.GetBridgedInterface()));
                            break;
                        case KNetworkAttachmentType_Internal:
                            attType = attType.arg(QApplication::translate("UIGDetails", "Internal Network, '%1'", "details (network)")
                                                     .arg(comNetwork.GetInternalNetwork()));
                            break;
                        case KNetworkAttachmentType_HostOnly:
                            attType = attType.arg(QApplication::translate("UIGDetails", "Host-only Adapter, '%1'", "details (network)")
                                                     .arg(comNetwork.GetHostOnlyInterface()));
                            break;
                        case KNetworkAttachmentType_Generic:
                        {
                            QString strGenericDriverProperties(summarizeGenericProperties(comNetwork));
                            attType = attType.arg(strGenericDriverProperties.isNull()
                                                  ? attType.arg(QApplication::translate("UIGDetails", "Generic Driver, '%1'", "details (network)").arg(comNetwork.GetGenericDriver()))
                                                  : attType.arg(QApplication::translate("UIGDetails", "Generic Driver, '%1' { %2 }", "details (network)")
                                                                .arg(comNetwork.GetGenericDriver(), strGenericDriverProperties)));
                            break;
                        }
                        case KNetworkAttachmentType_NATNetwork:
                            attType = attType.arg(QApplication::translate("UIGDetails", "NAT Network, '%1'", "details (network)")
                                                     .arg(comNetwork.GetNATNetwork()));
                            break;
                        default:
                            attType = attType.arg(gpConverter->toString(enmType));
                            break;
                    }
                    /* Here goes the record: */
                    ++iRowCount;
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Adapter %1", "details (network)")
                                                                .arg(comNetwork.GetSlot() + 1),
                                                             attType);
                }
            }

            /* Handle side-case: */
            if (strItem.isNull())
            {
                ++iRowCount;
                strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "Disabled", "details (network/adapter)"));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://network", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_Serial:
        {
            /* Nothing: */
            int iRowCount = 0;
            QString strItem;

            /* Enumerate all the serial ports (up to acquired/limited count): */
            const ulong iCount = vboxGlobal().virtualBox().GetSystemProperties().GetSerialPortCount();
            for (ulong iSlot = 0; iSlot < iCount; ++iSlot)
            {
                /* Get current serial port: */
                const CSerialPort &comSerial = comMachine.GetSerialPort(iSlot);
                if (comSerial.GetEnabled())
                {
                    /* Determine port mode: */
                    const KPortMode enmMode = comSerial.GetHostMode();
                    /* Compose the data: */
                    QString strData = vboxGlobal().toCOMPortName(comSerial.GetIRQ(), comSerial.GetIOBase()) + ", ";
                    if (   enmMode == KPortMode_HostPipe
                        || enmMode == KPortMode_HostDevice
                        || enmMode == KPortMode_TCP
                        || enmMode == KPortMode_RawFile)
                        strData += QString("%1 (<nobr>%2</nobr>)").arg(gpConverter->toString(enmMode))
                                                                  .arg(QDir::toNativeSeparators(comSerial.GetPath()));
                    else
                        strData += gpConverter->toString(enmMode);
                    /* Here goes the record: */
                    ++iRowCount;
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Port %1", "details (serial)")
                                                                .arg(comSerial.GetSlot() + 1),
                                                             strData);
                }
            }

            /* Handle side-case: */
            if (strItem.isNull())
            {
                ++iRowCount;
                strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "Disabled", "details (serial)"));
            }

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://serialPorts", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        case DetailsElementType_USB:
        {
            /* Acquire USB filters object: */
            const CUSBDeviceFilters &comFilters = comMachine.GetUSBDeviceFilters();
            if (   !comFilters.isNull()
                && comMachine.GetUSBProxyAvailable())
            {
                /* Device Filters: */
                int iRowCount = 1;
                QString strItem;

                /* The USB controller may be unavailable (i.e. in VirtualBox OSE): */
                const CUSBControllerVector controllers = comMachine.GetUSBControllers();
                if (!controllers.isEmpty())
                {
                    /* USB Controller: */
                    ++iRowCount;
                    QStringList aControllerList;
                    foreach (const CUSBController &comController, controllers)
                        aControllerList << gpConverter->toString(comController.GetType());
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "USB Controller", "details (usb)"),
                                                             aControllerList.join(", "));

                    /* Acquire USB filters: */
                    const CUSBDeviceFilterVector &filterVector = comFilters.GetDeviceFilters();
                    /* Calculate the amount of active filters: */
                    uint cActive = 0;
                    foreach (const CUSBDeviceFilter &comFilter, filterVector)
                        if (comFilter.GetActive())
                            ++cActive;
                    /* Here goes the record: */
                    strItem += QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Device Filters", "details (usb)"),
                                                             QApplication::translate("UIGDetails", "%1 (%2 active)", "details (usb)")
                                                                 .arg(filterVector.size()).arg(cActive));
                }

                /* Handle side-case: */
                if (strItem.isNull())
                    strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "Disabled", "details (usb)"));

                /* Append report: */
                strReport += strSectionTpl
                    .arg(1 + iRowCount) /* rows */
                    .arg("details://usb", /* icon */
                         QString::number(iIconArea), /* icon area */
                         gpConverter->toString(enmType), /* title */
                         strItem); /* items */
            }

            break;
        }
        case DetailsElementType_SF:
        {
            /* Shared Folders: */
            int iRowCount = 1;
            QString strItem;

            /* Acquire shared folders count: */
            const ulong cFolders = comMachine.GetSharedFolders().size();
            if (cFolders > 0)
                strItem = QString(sSectionItemTpl2).arg(QApplication::translate("UIGDetails", "Shared Folders", "details (shared folders)"),
                                                        QString::number(cFolders));
            else
                strItem = QString(sSectionItemTpl1).arg(QApplication::translate("UIGDetails", "None", "details (shared folders)"));

            /* Append report: */
            strReport += strSectionTpl
                .arg(1 + iRowCount) /* rows */
                .arg("details://sharedFolders", /* icon */
                     QString::number(iIconArea), /* icon area */
                     gpConverter->toString(enmType), /* title */
                     strItem); /* items */

            break;
        }
        default:
            break;
    }

    /* Return report as table: */
    return QString(sTableTpl).arg(strReport);
}

/* static */
QString UISnapshotDetailsWidget::summarizeGenericProperties(const CNetworkAdapter &comNetwork)
{
    QVector<QString> names;
    QVector<QString> props;
    props = comNetwork.GetProperties(QString(), names);
    QString strResult;
    for (int i = 0; i < names.size(); ++i)
    {
        strResult += names[i] + "=" + props[i];
        if (i < names.size() - 1)
            strResult += ", ";
    }
    return strResult;
}

#include "UISnapshotDetailsWidget.moc"

