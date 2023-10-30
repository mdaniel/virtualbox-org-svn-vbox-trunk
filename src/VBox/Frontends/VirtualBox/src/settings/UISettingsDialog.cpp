/* $Id$ */
/** @file
 * VBox Qt GUI - UISettingsDialog class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

/* Qt includes: */
#include <QCloseEvent>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QVariant>

/* GUI includes: */
#include "QIDialogButtonBox.h"
#include "UICommon.h"
#include "UIDesktopWidgetWatchdog.h"
#include "UIMessageCenter.h"
#include "UIModalWindowManager.h"
#include "UIPopupCenter.h"
#include "UISettingsDialog.h"
#include "UISettingsPage.h"
#include "UISettingsPageValidator.h"
#include "UISettingsSelector.h"
#include "UISettingsSerializer.h"
#include "UISettingsWarningPane.h"
#ifdef VBOX_WS_MAC
# include "VBoxUtils.h"
#endif

#ifdef VBOX_WS_MAC
# define VBOX_GUI_WITH_TOOLBAR_SETTINGS
#endif

#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
# include "QIToolBar.h"
#endif


UISettingsDialog::UISettingsDialog(QWidget *pParent,
                                   const QString &strCategory,
                                   const QString &strControl)
    : QIWithRetranslateUI<QMainWindow>(pParent)
    , m_strCategory(strCategory)
    , m_strControl(strControl)
    , m_pSelector(0)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_pSerializeProcess(0)
    , m_fPolished(false)
    , m_fSerializationIsInProgress(false)
    , m_fSerializationClean(false)
    , m_fClosed(false)
    , m_pStatusBar(0)
    , m_pProcessBar(0)
    , m_pWarningPane(0)
    , m_fValid(true)
    , m_fSilent(true)
    , m_pLayoutMain(0)
    , m_pLabelTitle(0)
    , m_pStack(0)
    , m_pButtonBox(0)
{
    prepare();
}

UISettingsDialog::~UISettingsDialog()
{
    cleanup();
}

void UISettingsDialog::accept()
{
    /* Save data: */
    save();

    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        sltClose();
}

void UISettingsDialog::reject()
{
    /* Close if there is no ongoing serialization: */
    if (!isSerializationInProgress())
        sltClose();
}

void UISettingsDialog::sltCategoryChanged(int cId)
{
#ifndef VBOX_WS_MAC
    if (m_pButtonBox)
        uiCommon().setHelpKeyword(m_pButtonBox->button(QDialogButtonBox::Help), m_pageHelpKeywords[cId]);
#endif
    const int iIndex = m_pages.value(cId);

#ifdef VBOX_WS_MAC
    /* If index is within the stored size list bounds: */
    if (iIndex < m_sizeList.count())
    {
        /* Get current/stored size: */
        const QSize cs = size();
        const QSize ss = m_sizeList.at(iIndex);

        /* Switch to the new page first if we are shrinking: */
        if (cs.height() > ss.height())
            m_pStack->setCurrentIndex(iIndex);

        /* Do the animation: */
        ::darwinWindowAnimateResize(this, QRect (x(), y(), ss.width(), ss.height()));

        /* Switch to the new page last if we are zooming: */
        if (cs.height() <= ss.height())
            m_pStack->setCurrentIndex(iIndex);

        /* Unlock all page policies but lock the current one: */
        for (int i = 0; i < m_pStack->count(); ++i)
            m_pStack->widget(i)->setSizePolicy(QSizePolicy::Minimum, i == iIndex ? QSizePolicy::Minimum : QSizePolicy::Ignored);

        /* And make sure layouts are freshly calculated: */
        foreach (QLayout *pLayout, findChildren<QLayout*>())
        {
            pLayout->update();
            pLayout->activate();
        }
    }
#else /* !VBOX_WS_MAC */
    m_pStack->setCurrentIndex(iIndex);
#endif /* !VBOX_WS_MAC */

#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle(title());
#else
    m_pLabelTitle->setText(m_pSelector->itemText(cId));
#endif
}

void UISettingsDialog::sltHandleSerializationStarted()
{
    m_pProcessBar->setValue(0);
    m_pStatusBar->setCurrentWidget(m_pProcessBar);
}

void UISettingsDialog::sltHandleSerializationProgressChange(int iValue)
{
    m_pProcessBar->setValue(iValue);
    if (m_pProcessBar->value() == m_pProcessBar->maximum())
    {
        if (!m_fValid || !m_fSilent)
            m_pStatusBar->setCurrentWidget(m_pWarningPane);
        else
            m_pStatusBar->setCurrentIndex(0);
    }
}

void UISettingsDialog::sltHandleSerializationFinished()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Mark serialization finished: */
    m_fSerializationIsInProgress = false;
}

void UISettingsDialog::retranslateUi()
{
#ifndef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* Retranslate current page headline: */
    m_pLabelTitle->setText(m_pSelector->itemText(m_pSelector->currentId()));
#endif

    /* Translate warning-pane stuff: */
    m_pWarningPane->setWarningLabelText(tr("Invalid settings detected"));

    /* Retranslate all validators: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
        pValidator->setTitlePrefix(m_pSelector->itemTextByPage(pValidator->page()));
    revalidate();
}

void UISettingsDialog::showEvent(QShowEvent *pEvent)
{
    /* Polish stuff: */
    if (!m_fPolished)
    {
        m_fPolished = true;
        polishEvent();
    }

    /* Call to base-class: */
    QIWithRetranslateUI<QMainWindow>::showEvent(pEvent);
}

void UISettingsDialog::polishEvent()
{
    /* Check what's the minimum selector size: */
    const int iMinWidth = m_pSelector->minWidth();

#ifdef VBOX_WS_MAC

    /* Remove all title bar buttons (Buggy Qt): */
    ::darwinSetHidesAllTitleButtons(this);

    /* Unlock all page policies initially: */
    for (int i = 0; i < m_pStack->count(); ++i)
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);

    /* Activate every single page to get the optimal size: */
    for (int i = m_pStack->count() - 1; i >= 0; --i)
    {
        /* Activate current page: */
        m_pStack->setCurrentIndex(i);

        /* Lock current page policy temporary: */
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        /* And make sure layouts are freshly calculated: */
        foreach (QLayout *pLayout, findChildren<QLayout*>())
        {
            pLayout->update();
            pLayout->activate();
        }

        /* Acquire minimum size-hint: */
        QSize s = minimumSizeHint();
        // WORKAROUND:
        // Take into account the height of native tool-bar title.
        // It will be applied only after widget is really shown.
        // The height is 11pix * 2 (possible HiDPI support).
        s.setHeight(s.height() + 11 * 2);
        /* Also make sure that width is no less than tool-bar: */
        if (iMinWidth > s.width())
            s.setWidth(iMinWidth);
        /* And remember the size finally: */
        m_sizeList.insert(0, s);

        /* Unlock the policy for current page again: */
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
    }

    sltCategoryChanged(m_pSelector->currentId());

#else /* VBOX_WS_MAC */

    /* Resize to the minimum possible size: */
    QSize s = minimumSize();
    if (iMinWidth > s.width())
        s.setWidth(iMinWidth);
    resize(s);

#endif /* VBOX_WS_MAC */

    /* Explicit centering according to our parent: */
    gpDesktop->centerWidget(this, parentWidget(), false);
}

void UISettingsDialog::closeEvent(QCloseEvent *pEvent)
{
    /* Ignore event initially: */
    pEvent->ignore();

    /* Use pure QWidget close functionality,
     * QWindow stuff is kind of overkill here.. */
    sltClose();
}

void UISettingsDialog::choosePageAndTab(bool fKeepPreviousByDefault /* = false */)
{
    /* Setup settings window: */
    if (!m_strCategory.isNull())
    {
        m_pSelector->selectByLink(m_strCategory);
        /* Search for a widget with the given name: */
        if (!m_strControl.isNull())
        {
            if (QWidget *pWidget = m_pStack->findChild<QWidget*>(m_strControl))
            {
                QList<QWidget*> parents;
                QWidget *pParentWidget = pWidget;
                while ((pParentWidget = pParentWidget->parentWidget()) != 0)
                {
                    if (QTabWidget *pTabWidget = qobject_cast<QTabWidget*>(pParentWidget))
                    {
                        // WORKAROUND:
                        // The tab contents widget is two steps down
                        // (QTabWidget -> QStackedWidget -> QWidget).
                        QWidget *pTabPage = parents[parents.count() - 1];
                        if (pTabPage)
                            pTabPage = parents[parents.count() - 2];
                        if (pTabPage)
                            pTabWidget->setCurrentWidget(pTabPage);
                    }
                    parents.append(pParentWidget);
                }
                pWidget->setFocus();
            }
        }
    }
    /* First item as default (if previous is not guarded): */
    else if (!fKeepPreviousByDefault)
        m_pSelector->selectById(1);
}

void UISettingsDialog::loadData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create settings loader: */
    m_pSerializeProcess = new UISettingsSerializer(this, UISettingsSerializer::Load,
                                                   data, m_pSelector->settingPages());
    if (m_pSerializeProcess)
    {
        /* Configure settings loader: */
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessStarted,
                this, &UISettingsDialog::sltHandleSerializationStarted);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessProgressChanged,
                this, &UISettingsDialog::sltHandleSerializationProgressChange);
        connect(m_pSerializeProcess, &UISettingsSerializer::sigNotifyAboutProcessFinished,
                this, &UISettingsDialog::sltHandleSerializationFinished);

        /* Raise current page priority: */
        m_pSerializeProcess->raisePriorityOfPage(m_pSelector->currentId());

        /* Start settings loader: */
        m_pSerializeProcess->start();

        /* Upload data finally: */
        data = m_pSerializeProcess->data();
    }
}

void UISettingsDialog::saveData(QVariant &data)
{
    /* Mark serialization started: */
    m_fSerializationIsInProgress = true;

    /* Create the 'settings saver': */
    QPointer<UISettingsSerializerProgress> pDlgSerializeProgress =
        new UISettingsSerializerProgress(this, UISettingsSerializer::Save,
                                         data, m_pSelector->settingPages());
    if (pDlgSerializeProgress)
    {
        /* Make the 'settings saver' temporary parent for all sub-dialogs: */
        windowManager().registerNewParent(pDlgSerializeProgress, windowManager().realParentWindow(this));

        /* Execute the 'settings saver': */
        pDlgSerializeProgress->exec();

        /* Any modal dialog can be destroyed in own event-loop
         * as a part of application termination procedure..
         * We have to check if the dialog still valid. */
        if (pDlgSerializeProgress)
        {
            /* Remember whether the serialization was clean: */
            m_fSerializationClean = pDlgSerializeProgress->isClean();

            /* Upload 'settings saver' data: */
            data = pDlgSerializeProgress->data();

            /* Delete the 'settings saver': */
            delete pDlgSerializeProgress;
        }
    }
}

void UISettingsDialog::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    /* Make sure something changed: */
    if (m_enmConfigurationAccessLevel == enmConfigurationAccessLevel)
        return;

    /* Apply new configuration access level: */
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;

    /* And propagate it to settings-page(s): */
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
        pPage->setConfigurationAccessLevel(configurationAccessLevel());
}

void UISettingsDialog::addItem(const QString &strBigIcon,
                               const QString &strMediumIcon,
                               const QString &strSmallIcon,
                               int cId,
                               const QString &strLink,
                               UISettingsPage *pSettingsPage /* = 0 */,
                               int iParentId /* = -1 */)
{
    /* Add new selector item: */
    if (QWidget *pPage = m_pSelector->addItem(strBigIcon, strMediumIcon, strSmallIcon,
                                              cId, strLink, pSettingsPage, iParentId))
    {
        /* Add stack-widget page if created: */
        m_pages[cId] = m_pStack->addWidget(pPage);
    }
    /* Assign validator if necessary: */
    if (pSettingsPage)
    {
        pSettingsPage->setId(cId);

        /* Create validator: */
        UISettingsPageValidator *pValidator = new UISettingsPageValidator(this, pSettingsPage);
        connect(pValidator, &UISettingsPageValidator::sigValidityChanged, this, &UISettingsDialog::sltHandleValidityChange);
        pSettingsPage->setValidator(pValidator);
        m_pWarningPane->registerValidator(pValidator);

        /* Update navigation (tab-order): */
        pSettingsPage->setOrderAfter(m_pSelector->widget());
    }
}

void UISettingsDialog::addPageHelpKeyword(int iPageType, const QString &strHelpKeyword)
{
    m_pageHelpKeywords[iPageType] = strHelpKeyword;
}

void UISettingsDialog::revalidate()
{
    /* Perform dialog revalidation: */
    m_fValid = true;
    m_fSilent = true;

    /* Enumerating all the validators we have: */
    foreach (UISettingsPageValidator *pValidator, findChildren<UISettingsPageValidator*>())
    {
        /* Is current validator have something to say? */
        if (!pValidator->lastMessage().isEmpty())
        {
            /* What page is it related to? */
            UISettingsPage *pFailedSettingsPage = pValidator->page();
            LogRelFlow(("Settings Dialog:  Dialog validation FAILED: Page *%s*\n",
                        pFailedSettingsPage->internalName().toUtf8().constData()));

            /* Show error first: */
            if (!pValidator->isValid())
                m_fValid = false;
            /* Show warning if message is not an error: */
            else
                m_fSilent = false;

            /* Stop dialog revalidation on first error/warning: */
            break;
        }
    }

    /* Update warning-pane visibility: */
    m_pWarningPane->setWarningLabelVisible(!m_fValid || !m_fSilent);

    /* Make sure warning-pane visible if necessary: */
    if ((!m_fValid || !m_fSilent) && m_pStatusBar->currentIndex() == 0)
        m_pStatusBar->setCurrentWidget(m_pWarningPane);
    /* Make sure empty-pane visible otherwise: */
    else if (m_fValid && m_fSilent && m_pStatusBar->currentWidget() == m_pWarningPane)
        m_pStatusBar->setCurrentIndex(0);

    /* Lock/unlock settings-page OK button according global validity status: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_fValid);
}

bool UISettingsDialog::isSettingsChanged()
{
    bool fIsSettingsChanged = false;
    foreach (UISettingsPage *pPage, m_pSelector->settingPages())
    {
        pPage->putToCache();
        if (!fIsSettingsChanged && pPage->changed())
            fIsSettingsChanged = true;
    }
    return fIsSettingsChanged;
}

void UISettingsDialog::sltClose()
{
    /* Check whether serialization was clean (save)
     * or there are no unsaved settings to be lost (cancel): */
    if (   m_fSerializationClean
        || !isSettingsChanged()
        || msgCenter().confirmSettingsDiscarding(this))
    {
        /* Tell the listener to close us (once): */
        if (!m_fClosed)
        {
            m_fClosed = true;
            emit sigClose();
            return;
        }
    }
}

void UISettingsDialog::sltHandleValidityChange(UISettingsPageValidator *pValidator)
{
    /* Determine which settings-page had called for revalidation: */
    if (UISettingsPage *pSettingsPage = pValidator->page())
    {
        /* Determine settings-page name: */
        const QString strPageName(pSettingsPage->internalName());

        LogRelFlow(("Settings Dialog: %s Page: Revalidation in progress..\n",
                    strPageName.toUtf8().constData()));

        /* Perform page revalidation: */
        pValidator->revalidate();
        /* Perform inter-page recorrelation: */
        recorrelate(pSettingsPage);
        /* Perform dialog revalidation: */
        revalidate();

        LogRelFlow(("Settings Dialog: %s Page: Revalidation complete.\n",
                    strPageName.toUtf8().constData()));
    }
}

void UISettingsDialog::sltHandleWarningPaneHovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon hovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Show corresponding popup: */
    if (!m_fValid || !m_fSilent)
        popupCenter().popup(m_pStack, "SettingsDialogWarning",
                            pValidator->lastMessage());
}

void UISettingsDialog::sltHandleWarningPaneUnhovered(UISettingsPageValidator *pValidator)
{
    LogRelFlow(("Settings Dialog: Warning-icon unhovered: %s.\n", pValidator->internalName().toUtf8().constData()));

    /* Recall corresponding popup: */
    popupCenter().recall(m_pStack, "SettingsDialogWarning");
}

void UISettingsDialog::prepare()
{
    /* Prepare central-widget: */
    setCentralWidget(new QWidget);
    if (centralWidget())
    {
        /* Prepare main layout: */
        m_pLayoutMain = new QGridLayout(centralWidget());
        if (m_pLayoutMain)
        {
            /* Prepare widgets: */
            prepareSelector();
            prepareStack();
            prepareButtonBox();
        }
    }

    /* Apply language settings: */
    retranslateUi();
}

void UISettingsDialog::prepareSelector()
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* Prepare modern tool-bar selector: */
    m_pSelector = new UISettingsSelectorToolBar(this);
    if (m_pSelector)
    {
        static_cast<QIToolBar*>(m_pSelector->widget())->enableMacToolbar();
        addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    }

    /* No title in this mode, we change the title of the window: */
    m_pLayoutMain->setColumnMinimumWidth(0, 0);
    m_pLayoutMain->setHorizontalSpacing(0);

#else /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Prepare classical tree-view selector: */
    m_pSelector = new UISettingsSelectorTreeWidget(centralWidget());
    if (m_pSelector)
    {
        m_pLayoutMain->addWidget(m_pSelector->widget(), 0, 0, 2, 1);
        m_pSelector->widget()->setFocus();
    }

    /* Prepare title label: */
    m_pLabelTitle = new QLabel(centralWidget());
    if (m_pLabelTitle)
    {
        m_pLabelTitle->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
        QPalette pal = QApplication::palette();
        pal.setColor(QPalette::Active, QPalette::Window, pal.color(QPalette::Active, QPalette::Base));
        m_pLabelTitle->setPalette(pal);
        QFont fnt;
        fnt.setFamily(QStringLiteral("Sans Serif"));
        fnt.setPointSize(fnt.pointSize() + 2);
        fnt.setBold(true);
        fnt.setWeight(QFont::ExtraBold);
        m_pLabelTitle->setFont(fnt);
        m_pLabelTitle->setAutoFillBackground(true);
        m_pLabelTitle->setFrameShadow(QFrame::Sunken);
        m_pLabelTitle->setMargin(9);

        /* Add title-label into main layout: */
        m_pLayoutMain->addWidget(m_pLabelTitle, 0, 1);
    }
#endif /* !VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Configure selector created above: */
    if (m_pSelector)
        connect(m_pSelector, &UISettingsSelectorTreeWidget::sigCategoryChanged,
                this, &UISettingsDialog::sltCategoryChanged);
}

void UISettingsDialog::prepareStack()
{
    /* Prepare page-stack: */
    m_pStack = new QStackedWidget(centralWidget());
    if (m_pStack)
    {
        /* Configure page-stack: */
        popupCenter().setPopupStackOrientation(m_pStack, UIPopupStackOrientation_Bottom);

        /* Add page-stack into main layout: */
        m_pLayoutMain->addWidget(m_pStack, 1, 1);
    }
}

void UISettingsDialog::prepareButtonBox()
{
    /* Prepare button-box: */
    m_pButtonBox = new QIDialogButtonBox(centralWidget());
    if (m_pButtonBox)
    {
#ifndef VBOX_WS_MAC
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton | QDialogButtonBox::Help);
        m_pButtonBox->button(QDialogButtonBox::Help)->setShortcut(QKeySequence::HelpContents);
#else
        // WORKAROUND:
        // No Help button on macOS for now, conflict with old Qt.
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                         QDialogButtonBox::NoButton);
#endif
        m_pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(Qt::Key_Return);
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &UISettingsDialog::sltClose);
        connect(m_pButtonBox, &QIDialogButtonBox::accepted, this, &UISettingsDialog::accept);
#ifndef VBOX_WS_MAC
        connect(m_pButtonBox->button(QDialogButtonBox::Help), &QAbstractButton::pressed,
                m_pButtonBox, &QIDialogButtonBox::sltHandleHelpRequest);
#endif

        /* Prepare status-bar: */
        m_pStatusBar = new QStackedWidget(m_pButtonBox);
        if (m_pStatusBar)
        {
            /* Add empty widget: */
            m_pStatusBar->addWidget(new QWidget);

            /* Prepare process-bar: */
            m_pProcessBar = new QProgressBar(m_pStatusBar);
            if (m_pProcessBar)
            {
                m_pProcessBar->setMinimum(0);
                m_pProcessBar->setMaximum(100);
                m_pStatusBar->addWidget(m_pProcessBar);
            }

            /* Prepare warning-pane: */
            m_pWarningPane = new UISettingsWarningPane(m_pStatusBar);
            if (m_pWarningPane)
            {
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverEnter,
                        this, &UISettingsDialog::sltHandleWarningPaneHovered);
                connect(m_pWarningPane, &UISettingsWarningPane::sigHoverLeave,
                        this, &UISettingsDialog::sltHandleWarningPaneUnhovered);
                m_pStatusBar->addWidget(m_pWarningPane);
            }

            /* Add status-bar to button-box: */
            m_pButtonBox->addExtraWidget(m_pStatusBar);
        }

        /* Add button-box into main layout: */
        m_pLayoutMain->addWidget(m_pButtonBox, 2, 0, 1, 2);
    }
}

void UISettingsDialog::cleanup()
{
    /* Delete serializer if exists: */
    delete m_pSerializeProcess;
    m_pSerializeProcess = 0;

    /* Recall popup-pane if any: */
    popupCenter().recall(m_pStack, "SettingsDialogWarning");

    /* Delete selector early! */
    delete m_pSelector;
}
