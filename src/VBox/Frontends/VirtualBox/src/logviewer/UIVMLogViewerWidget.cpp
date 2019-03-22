/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMLogViewerWidget class implementation.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
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
# include <QDateTime>
# include <QDir>
# include <QFont>
# include <QMenu>
# include <QPainter>
# include <QPlainTextEdit>
# include <QScrollBar>
# include <QStyle>
# include <QTextBlock>
# include <QVBoxLayout>
# ifdef RT_OS_SOLARIS
#  include <QFontDatabase>
# endif

/* GUI includes: */
# include "QIFileDialog.h"
# include "QITabWidget.h"
# include "UIActionPool.h"
# include "UIExtraDataManager.h"
# include "UIIconPool.h"
# include "UIMessageCenter.h"
# include "UIVMLogPage.h"
# include "UIVMLogViewerWidget.h"
# include "UIVMLogViewerBookmarksPanel.h"
# include "UIVMLogViewerFilterPanel.h"
# include "UIVMLogViewerSearchPanel.h"
# include "UIVMLogViewerOptionsPanel.h"
# include "UIToolBar.h"

/* COM includes: */
# include "CSystemProperties.h"

# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIVMLogViewerWidget::UIVMLogViewerWidget(EmbedTo enmEmbedding,
                                         UIActionPool *pActionPool,
                                         bool fShowToolbar /* = true */,
                                         const CMachine &comMachine /* = CMachine() */,
                                         QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_pActionPool(pActionPool)
    , m_fShowToolbar(fShowToolbar)
    , m_comMachine(comMachine)
    , m_fIsPolished(false)
    , m_pTabWidget(0)
    , m_pSearchPanel(0)
    , m_pFilterPanel(0)
    , m_pBookmarksPanel(0)
    , m_pOptionsPanel(0)
    , m_pMainLayout(0)
    , m_pToolBar(0)
    , m_bShowLineNumbers(true)
    , m_bWrapLines(false)
    , m_font(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
    /* Prepare VM Log-Viewer: */
    prepare();
}

UIVMLogViewerWidget::~UIVMLogViewerWidget()
{
    /* Cleanup VM Log-Viewer: */
    cleanup();
}

int UIVMLogViewerWidget::defaultLogPageWidth() const
{
    if (!m_pTabWidget)
        return 0;

    QWidget *pContainer = m_pTabWidget->currentWidget();
    if (!pContainer)
        return 0;

    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    if (!pBrowser)
        return 0;
    /* Compute a width for 132 characters plus scrollbar and frame width: */
    int iDefaultWidth = pBrowser->fontMetrics().width(QChar('x')) * 132 +
                        pBrowser->verticalScrollBar()->width() +
                        pBrowser->frameWidth() * 2;

    return iDefaultWidth;
}

QMenu *UIVMLogViewerWidget::menu() const
{
    return m_pActionPool->action(UIActionIndex_M_LogWindow)->menu();
}

void UIVMLogViewerWidget::setMachine(const CMachine &comMachine)
{
    if (comMachine == m_comMachine)
        return;
    m_comMachine = comMachine;
    sltRefresh();
}

QFont UIVMLogViewerWidget::currentFont() const
{
    const UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return QFont();
    return logPage->currentFont();
}

bool UIVMLogViewerWidget::shouldBeMaximized() const
{
    return gEDataManager->logWindowShouldBeMaximized();
}

void UIVMLogViewerWidget::sltRefresh()
{
    if (!m_pTabWidget)
        return;
    /* Disconnect this connection to avoid initial signals during page creation/deletion: */
    disconnect(m_pTabWidget, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    disconnect(m_pTabWidget, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    m_logPageList.clear();
    m_pTabWidget->setEnabled(true);
    int currentTabIndex = m_pTabWidget->currentIndex();
    /* Hide the container widget during updates to avoid flickering: */
    m_pTabWidget->hide();
    QVector<QVector<LogBookmark> > logPageBookmarks;
    /* Clear the tab widget. This might be an overkill but most secure way to deal with the case where
       number of the log files changes. Store the bookmark vectors before deleting the pages*/
    while (m_pTabWidget->count())
    {
        QWidget *pFirstPage = m_pTabWidget->widget(0);
        UIVMLogPage *pLogPage = qobject_cast<UIVMLogPage*>(pFirstPage);
        if (pLogPage)
            logPageBookmarks.push_back(pLogPage->bookmarkVector());
        m_pTabWidget->removeTab(0);
        delete pFirstPage;
    }

    bool noLogsToShow = createLogViewerPages();

    /* Apply the filter settings: */
    if (m_pFilterPanel)
        m_pFilterPanel->applyFilter();

    /* Restore the bookmarks: */
    if (!noLogsToShow)
    {
        for (int i = 0; i <  m_pTabWidget->count(); ++i)
        {
            UIVMLogPage *pLogPage = qobject_cast<UIVMLogPage*>(m_pTabWidget->widget(i));
            if (pLogPage && i < logPageBookmarks.size())
                pLogPage->setBookmarkVector(logPageBookmarks[i]);
        }
    }

    /* Setup this connection after refresh to avoid initial signals during page creation: */
    if (m_pFilterPanel)
        connect(m_pTabWidget, &QITabWidget::currentChanged, m_pFilterPanel, &UIVMLogViewerFilterPanel::applyFilter);
    connect(m_pTabWidget, &QITabWidget::currentChanged, this, &UIVMLogViewerWidget::sltTabIndexChange);

    /* Show the first tab widget's page after the refresh: */
    int tabIndex = (currentTabIndex < m_pTabWidget->count()) ? currentTabIndex : 0;
    m_pTabWidget->setCurrentIndex(tabIndex);
    sltTabIndexChange(tabIndex);

    /* Enable/Disable toolbar actions (except Refresh) & tab widget according log presence: */
    m_pActionPool->action(UIActionIndex_M_Log_T_Find)->setEnabled(!noLogsToShow);
    m_pActionPool->action(UIActionIndex_M_Log_T_Filter)->setEnabled(!noLogsToShow);
    m_pActionPool->action(UIActionIndex_M_Log_S_Save)->setEnabled(!noLogsToShow);
    m_pActionPool->action(UIActionIndex_M_Log_T_Bookmark)->setEnabled(!noLogsToShow);
    m_pActionPool->action(UIActionIndex_M_Log_T_Options)->setEnabled(!noLogsToShow);

    m_pTabWidget->show();
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();

    /* If there are no log files to show the hide all the open panels: */
    if (noLogsToShow)
    {
        for (QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.begin();
            iterator != m_panelActionMap.end(); ++iterator)
        {
            if (iterator.key())
            hidePanel(iterator.key());
        }
    }
}

void UIVMLogViewerWidget::sltSave()
{
    if (m_comMachine.isNull())
        return;

    UIVMLogPage *logPage = currentLogPage();
    if (!logPage)
        return;

    const QString& fileName = logPage->logFileName();
    if (fileName.isEmpty())
        return;
    /* Prepare "save as" dialog: */
    const QFileInfo fileInfo(fileName);
    /* Prepare default filename: */
    const QDateTime dtInfo = fileInfo.lastModified();
    const QString strDtString = dtInfo.toString("yyyy-MM-dd-hh-mm-ss");
    const QString strDefaultFileName = QString("%1-%2.log").arg(m_comMachine.GetName()).arg(strDtString);
    const QString strDefaultFullName = QDir::toNativeSeparators(QDir::home().absolutePath() + "/" + strDefaultFileName);

    const QString strNewFileName = QIFileDialog::getSaveFileName(strDefaultFullName,
                                                                 "",
                                                                 this,
                                                                 tr("Save VirtualBox Log As"),
                                                                 0 /* selected filter */,
                                                                 true /* resolve symlinks */,
                                                                 true /* confirm overwrite */);
    /* Make sure file-name is not empty: */
    if (!strNewFileName.isEmpty())
    {
        /* Delete the previous file if already exists as user already confirmed: */
        if (QFile::exists(strNewFileName))
            QFile::remove(strNewFileName);
        /* Copy log into the file: */
        QFile::copy(m_comMachine.QueryLogFilename(m_pTabWidget->currentIndex()), strNewFileName);
    }
}

void UIVMLogViewerWidget::sltDeleteBookmark(int index)
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteBookmark(index);
    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIVMLogViewerWidget::sltDeleteAllBookmarks()
{
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->deleteAllBookmarks();

    if (m_pBookmarksPanel)
        m_pBookmarksPanel->updateBookmarkList(logPage->bookmarkVector());
}

void UIVMLogViewerWidget::sltUpdateBookmarkPanel()
{
    if (!currentLogPage() || !m_pBookmarksPanel)
        return;
    m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIVMLogViewerWidget::gotoBookmark(int bookmarkIndex)
{
    if (!currentLogPage())
        return;
    currentLogPage()->scrollToBookmark(bookmarkIndex);
}

void UIVMLogViewerWidget::sltPanelActionToggled(bool fChecked)
{
    QAction *pSenderAction = qobject_cast<QAction*>(sender());
    if (!pSenderAction)
        return;
    UIVMLogViewerPanel* pPanel = 0;
    /* Look for the sender() within the m_panelActionMap's values: */
    for (QMap<UIVMLogViewerPanel*, QAction*>::const_iterator iterator = m_panelActionMap.begin();
        iterator != m_panelActionMap.end(); ++iterator)
    {
        if (iterator.value() == pSenderAction)
            pPanel = iterator.key();
    }
    if (!pPanel)
        return;
    if (fChecked)
        showPanel(pPanel);
    else
        hidePanel(pPanel);
}

void UIVMLogViewerWidget::sltSearchResultHighLigting()
{
    if (!m_pSearchPanel)
        return;

    if (!currentLogPage())
        return;
    currentLogPage()->setScrollBarMarkingsVector(m_pSearchPanel->getMatchLocationVector());
}

void UIVMLogViewerWidget::sltTabIndexChange(int tabIndex)
{
    Q_UNUSED(tabIndex);

    resetHighlighthing();
    if (m_pSearchPanel)
        m_pSearchPanel->reset();

    /* We keep a separate QVector<LogBookmark> for each log page: */
    if (m_pBookmarksPanel && currentLogPage())
        m_pBookmarksPanel->updateBookmarkList(currentLogPage()->bookmarkVector());
}

void UIVMLogViewerWidget::sltFilterApplied(bool isOriginal)
{
    if (currentLogPage())
        currentLogPage()->setFiltered(!isOriginal);
    /* Reapply the search to get highlighting etc. correctly */
    if (m_pSearchPanel && m_pSearchPanel->isVisible())
        m_pSearchPanel->refresh();
}

void UIVMLogViewerWidget::sltLogPageFilteredChanged(bool isFiltered)
{
    /* Disable bookmark panel since bookmarks are stored as line numbers within
       the original log text and does not mean much in a reduced/filtered one. */
    if (m_pBookmarksPanel)
        m_pBookmarksPanel->disableEnableBookmarking(!isFiltered);
}

void UIVMLogViewerWidget::sltShowLineNumbers(bool bShowLineNumbers)
{
    if (m_bShowLineNumbers == bShowLineNumbers)
        return;

    m_bShowLineNumbers = bShowLineNumbers;
    /* Set all log page instances. */
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setShowLineNumbers(m_bShowLineNumbers);
    }
}

void UIVMLogViewerWidget::sltWrapLines(bool bWrapLines)
{
    if (m_bWrapLines == bWrapLines)
        return;

    m_bWrapLines = bWrapLines;
    /* Set all log page instances. */
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setWrapLines(m_bWrapLines);
    }
}

void UIVMLogViewerWidget::sltFontSizeChanged(int fontSize)
{
    if (m_font.pointSize() == fontSize)
        return;
    m_font.setPointSize(fontSize);
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setCurrentFont(m_font);
    }
}

void UIVMLogViewerWidget::sltChangeFont(QFont font)
{
    if (m_font == font)
        return;
    m_font = font;
    for (int i = 0; i < m_logPageList.size(); ++i)
    {
        UIVMLogPage* pLogPage = qobject_cast<UIVMLogPage*>(m_logPageList[i]);
        if (pLogPage)
            pLogPage->setCurrentFont(m_font);
    }
}

void UIVMLogViewerWidget::sltResetOptionsToDefault()
{
    sltShowLineNumbers(true);
    sltWrapLines(false);
    sltChangeFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    if (m_pOptionsPanel)
    {
        m_pOptionsPanel->setShowLineNumbers(true);
        m_pOptionsPanel->setWrapLines(false);
        m_pOptionsPanel->setFontSizeInPoints(m_font.pointSize());
    }
}

void UIVMLogViewerWidget::prepare()
{
    /* Prepare stuff: */
    prepareActions();
    /* Prepare widgets: */
    prepareWidgets();
    /* Load options: */
    loadOptions();

    /* Apply language settings: */
    retranslateUi();

    /* Reading log files: */
    sltRefresh();
    /* Setup escape shortcut: */
    manageEscapeShortCut();
}

void UIVMLogViewerWidget::prepareActions()
{
    /* First of all, add actions which has smaller shortcut scope: */
    addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Find));
    addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Filter));
    addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Bookmark));
    addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Options));
    addAction(m_pActionPool->action(UIActionIndex_M_Log_S_Refresh));
    addAction(m_pActionPool->action(UIActionIndex_M_Log_S_Save));

    /* Connect actions: */
    connect(m_pActionPool->action(UIActionIndex_M_Log_T_Find), &QAction::toggled,
            this, &UIVMLogViewerWidget::sltPanelActionToggled);
    connect(m_pActionPool->action(UIActionIndex_M_Log_T_Filter), &QAction::toggled,
            this, &UIVMLogViewerWidget::sltPanelActionToggled);
    connect(m_pActionPool->action(UIActionIndex_M_Log_T_Bookmark), &QAction::toggled,
            this, &UIVMLogViewerWidget::sltPanelActionToggled);
    connect(m_pActionPool->action(UIActionIndex_M_Log_T_Options), &QAction::toggled,
            this, &UIVMLogViewerWidget::sltPanelActionToggled);
    connect(m_pActionPool->action(UIActionIndex_M_Log_S_Refresh), &QAction::triggered,
            this, &UIVMLogViewerWidget::sltRefresh);
    connect(m_pActionPool->action(UIActionIndex_M_Log_S_Save), &QAction::triggered,
            this, &UIVMLogViewerWidget::sltSave);
}

void UIVMLogViewerWidget::prepareWidgets()
{
    /* Create main layout: */
    m_pMainLayout = new QVBoxLayout(this);
    if (m_pMainLayout)
    {
        /* Configure layout: */
        m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
        m_pMainLayout->setSpacing(10);
#else
        m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

        /* Prepare toolbar, if requested: */
        if (m_fShowToolbar)
            prepareToolBar();

        /* Create VM Log-Viewer container: */
        m_pTabWidget = new QITabWidget;
        if (m_pTabWidget)
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pTabWidget);
        }

        /* Create VM Log-Viewer search-panel: */
        m_pSearchPanel = new UIVMLogViewerSearchPanel(0, this);
        if (m_pSearchPanel)
        {
            /* Configure panel: */
            installEventFilter(m_pSearchPanel);
            m_pSearchPanel->hide();
            connect(m_pSearchPanel, &UIVMLogViewerSearchPanel::sigHighlightingUpdated,
                    this, &UIVMLogViewerWidget::sltSearchResultHighLigting);
            m_panelActionMap.insert(m_pSearchPanel, m_pActionPool->action(UIActionIndex_M_Log_T_Find));

            /* Add into layout: */
            m_pMainLayout->addWidget(m_pSearchPanel);
        }

        /* Create VM Log-Viewer filter-panel: */
        m_pFilterPanel = new UIVMLogViewerFilterPanel(0, this);
        if (m_pFilterPanel)
        {
            /* Configure panel: */
            installEventFilter(m_pFilterPanel);
            m_pFilterPanel->hide();
            connect(m_pFilterPanel, &UIVMLogViewerFilterPanel::sigFilterApplied,
                    this, &UIVMLogViewerWidget::sltFilterApplied);
            m_panelActionMap.insert(m_pFilterPanel, m_pActionPool->action(UIActionIndex_M_Log_T_Filter));

            /* Add into layout: */
            m_pMainLayout->addWidget(m_pFilterPanel);
        }

        /* Create VM Log-Viewer bookmarks-panel: */
        m_pBookmarksPanel = new UIVMLogViewerBookmarksPanel(0, this);
        if (m_pBookmarksPanel)
        {
            /* Configure panel: */
            m_pBookmarksPanel->hide();
            connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigDeleteBookmark,
                    this, &UIVMLogViewerWidget::sltDeleteBookmark);
            connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigDeleteAllBookmarks,
                    this, &UIVMLogViewerWidget::sltDeleteAllBookmarks);
            connect(m_pBookmarksPanel, &UIVMLogViewerBookmarksPanel::sigBookmarkSelected,
                    this, &UIVMLogViewerWidget::gotoBookmark);
            m_panelActionMap.insert(m_pBookmarksPanel, m_pActionPool->action(UIActionIndex_M_Log_T_Bookmark));

            /* Add into layout: */
            m_pMainLayout->addWidget(m_pBookmarksPanel);
        }

        /* Create VM Log-Viewer options-panel: */
        m_pOptionsPanel = new UIVMLogViewerOptionsPanel(0, this);
        if (m_pOptionsPanel)
        {
            /* Configure panel: */
            m_pOptionsPanel->hide();
            m_pOptionsPanel->setShowLineNumbers(m_bShowLineNumbers);
            m_pOptionsPanel->setWrapLines(m_bWrapLines);
            m_pOptionsPanel->setFontSizeInPoints(m_font.pointSize());
            connect(m_pOptionsPanel, &UIVMLogViewerOptionsPanel::sigShowLineNumbers, this, &UIVMLogViewerWidget::sltShowLineNumbers);
            connect(m_pOptionsPanel, &UIVMLogViewerOptionsPanel::sigWrapLines, this, &UIVMLogViewerWidget::sltWrapLines);
            connect(m_pOptionsPanel, &UIVMLogViewerOptionsPanel::sigChangeFontSizeInPoints, this, &UIVMLogViewerWidget::sltFontSizeChanged);
            connect(m_pOptionsPanel, &UIVMLogViewerOptionsPanel::sigChangeFont, this, &UIVMLogViewerWidget::sltChangeFont);
            connect(m_pOptionsPanel, &UIVMLogViewerOptionsPanel::sigResetToDefaults, this, &UIVMLogViewerWidget::sltResetOptionsToDefault);
            m_panelActionMap.insert(m_pOptionsPanel, m_pActionPool->action(UIActionIndex_M_Log_T_Options));

            /* Add into layout: */
            m_pMainLayout->addWidget(m_pOptionsPanel);
        }
    }
}

void UIVMLogViewerWidget::prepareToolBar()
{
    /* Create toolbar: */
    m_pToolBar = new UIToolBar(parentWidget());
    if (m_pToolBar)
    {
        /* Configure toolbar: */
        const int iIconMetric = (int)(QApplication::style()->pixelMetric(QStyle::PM_LargeIconSize));
        m_pToolBar->setIconSize(QSize(iIconMetric, iIconMetric));
        m_pToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        /* Add toolbar actions: */
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_S_Save));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Find));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Filter));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Bookmark));
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_T_Options));
        m_pToolBar->addSeparator();
        m_pToolBar->addAction(m_pActionPool->action(UIActionIndex_M_Log_S_Refresh));

#ifdef VBOX_WS_MAC
        /* Check whether we are embedded into a stack: */
        if (m_enmEmbedding == EmbedTo_Stack)
        {
            /* Add into layout: */
            m_pMainLayout->addWidget(m_pToolBar);
        }
#else
        /* Add into layout: */
        m_pMainLayout->addWidget(m_pToolBar);
#endif
    }
}

void UIVMLogViewerWidget::loadOptions()
{
    m_bWrapLines = gEDataManager->logViewerWrapLines();
    m_bShowLineNumbers = gEDataManager->logViewerShowLineNumbers();
    QFont loadedFont = gEDataManager->logViewerFont();
    if (loadedFont != QFont())
        m_font = loadedFont;
}

void UIVMLogViewerWidget::saveOptions()
{
    gEDataManager->setLogViweverOptions(m_font, m_bWrapLines, m_bShowLineNumbers);
}

void UIVMLogViewerWidget::cleanup()
{
    /* Save options: */
    saveOptions();
}

void UIVMLogViewerWidget::retranslateUi()
{
    /* Translate toolbar: */
#ifdef VBOX_WS_MAC
    // WORKAROUND:
    // There is a bug in Qt Cocoa which result in showing a "more arrow" when
    // the necessary size of the toolbar is increased. Also for some languages
    // the with doesn't match if the text increase. So manually adjust the size
    // after changing the text. */
    if (m_pToolBar)
        m_pToolBar->updateLayout();
#endif
}

void UIVMLogViewerWidget::showEvent(QShowEvent *pEvent)
{
    QWidget::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation: */

    if (m_fIsPolished)
        return;

    m_fIsPolished = true;
}

void UIVMLogViewerWidget::keyPressEvent(QKeyEvent *pEvent)
{
    /* Depending on key pressed: */
    switch (pEvent->key())
    {
        /* Process Back key as switch to previous tab: */
        case Qt::Key_Back:
        {
            if (m_pTabWidget->currentIndex() > 0)
            {
                m_pTabWidget->setCurrentIndex(m_pTabWidget->currentIndex() - 1);
                return;
            }
            break;
        }
        /* Process Forward key as switch to next tab: */
        case Qt::Key_Forward:
        {
            if (m_pTabWidget->currentIndex() < m_pTabWidget->count())
            {
                m_pTabWidget->setCurrentIndex(m_pTabWidget->currentIndex() + 1);
                return;
            }
            break;
        }
        default:
            break;
    }
    QWidget::keyPressEvent(pEvent);
}

QPlainTextEdit* UIVMLogViewerWidget::logPage(int pIndex) const
{
    if (!m_pTabWidget->isEnabled())
        return 0;
    QWidget* pContainer = m_pTabWidget->widget(pIndex);
    if (!pContainer)
        return 0;
    QPlainTextEdit *pBrowser = pContainer->findChild<QPlainTextEdit*>();
    return pBrowser;
}

void UIVMLogViewerWidget::createLogPage(const QString &strFileName, const QString &strLogContent, bool noLogsToShow /* = false */)
{
    if (!m_pTabWidget)
        return;

    /* Create page-container: */
    UIVMLogPage* pLogPage = new UIVMLogPage();
    if (pLogPage)
    {
        connect(pLogPage, &UIVMLogPage::sigBookmarksUpdated, this, &UIVMLogViewerWidget::sltUpdateBookmarkPanel);
        connect(pLogPage, &UIVMLogPage::sigLogPageFilteredChanged, this, &UIVMLogViewerWidget::sltLogPageFilteredChanged);
        /* Initialize setting for this log page */
        pLogPage->setShowLineNumbers(m_bShowLineNumbers);
        pLogPage->setWrapLines(m_bWrapLines);
        pLogPage->setCurrentFont(m_font);

        /* Set the file name only if we really have log file to read. */
        if (!noLogsToShow)
            pLogPage->setLogFileName(strFileName);

        /* Add page-container to viewer-container: */
        int tabIndex = m_pTabWidget->insertTab(m_pTabWidget->count(), pLogPage, QFileInfo(strFileName).fileName());

        pLogPage->setTabIndex(tabIndex);
        m_logPageList.resize(m_pTabWidget->count());
        m_logPageList[tabIndex] = pLogPage;

        /* Set text edit since we want to display this text: */
        if (!noLogsToShow)
        {
            pLogPage->setTextEditText(strLogContent);
            /* Set the log string of the UIVMLogPage: */
            pLogPage->setLogString(strLogContent);
        }
        /* In case there are some errors append the error text as html: */
        else
        {
            pLogPage->setTextEditTextAsHtml(strLogContent);
            pLogPage->markForError();
        }
    }
}

const UIVMLogPage *UIVMLogViewerWidget::currentLogPage() const
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size())
        return 0;
    return qobject_cast<const UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}

UIVMLogPage *UIVMLogViewerWidget::currentLogPage()
{
    int currentTabIndex = m_pTabWidget->currentIndex();
    if (currentTabIndex >= m_logPageList.size() || currentTabIndex == -1)
        return 0;

    return qobject_cast<UIVMLogPage*>(m_logPageList.at(currentTabIndex));
}

bool UIVMLogViewerWidget::createLogViewerPages()
{
    bool noLogsToShow = false;

    QString strDummyTabText;
    /* check if the machine is valid: */
    if (m_comMachine.isNull())
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p><b>No machine</b> is currently selected or the selected machine is not valid. "
                                     "Please select a Virtual Machine to see its logs"));
    }

    const CSystemProperties &sys = vboxGlobal().virtualBox().GetSystemProperties();
    unsigned cMaxLogs = sys.GetLogHistoryCount() + 1 /*VBox.log*/ + 1 /*VBoxHardening.log*/; /** @todo Add api for getting total possible log count! */
    bool logFileRead = false;
    for (unsigned i = 0; i < cMaxLogs && !noLogsToShow; ++i)
    {
        /* Query the log file name for index i: */
        QString strFileName = m_comMachine.QueryLogFilename(i);
        if (!strFileName.isEmpty())
        {
            /* Try to read the log file with the index i: */
            ULONG uOffset = 0;
            QString strText;
            while (true)
            {
                QVector<BYTE> data = m_comMachine.ReadLog(i, uOffset, _1M);
                if (data.size() == 0)
                    break;
                strText.append(QString::fromUtf8((char*)data.data(), data.size()));
                uOffset += data.size();
            }
            /* Anything read at all? */
            if (uOffset > 0)
            {
                logFileRead = true;
                createLogPage(strFileName, strText);
            }
        }
    }
    if (!noLogsToShow && !logFileRead)
    {
        noLogsToShow = true;
        strDummyTabText = QString(tr("<p>No log files found. Press the "
                                     "<b>Refresh</b> button to rescan the log folder "
                                     "<nobr><b>%1</b></nobr>.</p>")
                                     .arg(m_comMachine.GetLogFolder()));
    }

    /* if noLogsToShow then ceate a single log page with an error message: */
    if (noLogsToShow)
    {
        createLogPage("No Logs", strDummyTabText, noLogsToShow);
    }
    return noLogsToShow;
}

void UIVMLogViewerWidget::resetHighlighthing()
{
    /* Undo the document changes to remove highlighting: */
    UIVMLogPage* logPage = currentLogPage();
    if (!logPage)
        return;
    logPage->documentUndo();
    logPage->clearScrollBarMarkingsVector();
}

void UIVMLogViewerWidget::hidePanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isVisible())
        panel->setVisible(false);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (iterator.value() && iterator.value()->isChecked())
            iterator.value()->setChecked(false);
    }
    m_visiblePanelsList.removeOne(panel);
    manageEscapeShortCut();
}

void UIVMLogViewerWidget::showPanel(UIVMLogViewerPanel* panel)
{
    if (panel && panel->isHidden())
        panel->setVisible(true);
    QMap<UIVMLogViewerPanel*, QAction*>::iterator iterator = m_panelActionMap.find(panel);
    if (iterator != m_panelActionMap.end())
    {
        if (!iterator.value()->isChecked())
            iterator.value()->setChecked(true);
    }
    m_visiblePanelsList.push_back(panel);
    manageEscapeShortCut();
}

void UIVMLogViewerWidget::manageEscapeShortCut()
{
    /* if there is no visible panels give the escape shortcut to parent dialog: */
    if (m_visiblePanelsList.isEmpty())
    {
        emit sigSetCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
        return;
    }
    /* Take the escape shortcut from the dialog: */
    emit sigSetCloseButtonShortCut(QKeySequence());
    /* Just loop thru the visible panel list and set the esc key to the
       panel which made visible latest */
    for (int i = 0; i < m_visiblePanelsList.size() - 1; ++i)
    {
        m_visiblePanelsList[i]->setCloseButtonShortCut(QKeySequence());
    }
    m_visiblePanelsList.back()->setCloseButtonShortCut(QKeySequence(Qt::Key_Escape));
}
