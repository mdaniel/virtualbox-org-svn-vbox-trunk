/* $Id$ */
/** @file
 * VBox Qt GUI - QIManagerDialog class implementation.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
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
# include <QMenuBar>
# include <QPushButton>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QIManagerDialog.h"
# include "UIDesktopWidgetWatchdog.h"
# ifdef VBOX_WS_MAC
#  include "UIToolBar.h"
#  include "UIWindowMenuManager.h"
# endif /* VBOX_WS_MAC */
# include "VBoxGlobal.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/*********************************************************************************************************************************
*   Class QIManagerDialogFactory implementation.                                                                                 *
*********************************************************************************************************************************/

void QIManagerDialogFactory::prepare(QIManagerDialog *&pDialog, QWidget *pCenterWidget /* = 0 */)
{
    create(pDialog, pCenterWidget);
    pDialog->prepare();
}

void QIManagerDialogFactory::cleanup(QIManagerDialog *&pDialog)
{
    pDialog->cleanup();
    delete pDialog;
    pDialog = 0;
}


/*********************************************************************************************************************************
*   Class QIManagerDialog implementation.                                                                                        *
*********************************************************************************************************************************/

QIManagerDialog::QIManagerDialog(QWidget *pCenterWidget)
    : pCenterWidget(pCenterWidget)
    , m_pWidget(0)
    , m_pWidgetMenu(0)
#ifdef VBOX_WS_MAC
    , m_pWidgetToolbar(0)
#endif
    , m_pButtonBox(0)
{
}

void QIManagerDialog::prepare()
{
    /* Tell the application we are not that important: */
    setAttribute(Qt::WA_QuitOnClose, false);

    /* Invent initial size: */
    QSize proposedSize;
    const int iHostScreen = gpDesktop->screenNumber(pCenterWidget);
    if (iHostScreen >= 0 && iHostScreen < gpDesktop->screenCount())
    {
        /* On the basis of current host-screen geometry if possible: */
        const QRect screenGeometry = gpDesktop->screenGeometry(iHostScreen);
        if (screenGeometry.isValid())
            proposedSize = screenGeometry.size() * 2 / 5;
    }
    /* Fallback to default size if we failed: */
    if (proposedSize.isNull())
        proposedSize = QSize(800, 600);
    /* Resize to initial size: */
    resize(proposedSize);

    /* Configure: */
    configure();

    /* Prepare central-widget: */
    prepareCentralWidget();
    /* Prepare menu-bar: */
    prepareMenuBar();
#ifdef VBOX_WS_MAC
    /* Prepare toolbar: */
    prepareToolBar();
#endif

    /* Finalize: */
    finalize();

    /* Center according requested widget: */
    VBoxGlobal::centerWidget(this, pCenterWidget, false);
}

void QIManagerDialog::prepareCentralWidget()
{
    /* Create central-widget: */
    setCentralWidget(new QWidget);
    AssertPtrReturnVoid(centralWidget());
    {
        /* Create main-layout: */
        new QVBoxLayout(centralWidget());
        AssertPtrReturnVoid(centralWidget()->layout());
        {
            /* Configure layout: */
            centralWidget()->layout()->setContentsMargins(5, 5, 5, 5);
            centralWidget()->layout()->setSpacing(10);

            /* Configure central-widget: */
            configureCentralWidget();

            /* Prepare button-box: */
            prepareButtonBox();
        }
    }
}

void QIManagerDialog::prepareButtonBox()
{
    /* Create button-box: */
    m_pButtonBox = new QIDialogButtonBox;
    AssertPtrReturnVoid(m_pButtonBox);
    {
        /* Configure button-box: */
        m_pButtonBox->setStandardButtons(QDialogButtonBox::Reset | QDialogButtonBox::Save |  QDialogButtonBox::Close);
        m_pButtonBox->button(QDialogButtonBox::Close)->setShortcut(Qt::Key_Escape);
        m_pButtonBox->button(QDialogButtonBox::Reset)->hide();
        m_pButtonBox->button(QDialogButtonBox::Save)->hide();
        m_pButtonBox->button(QDialogButtonBox::Reset)->setEnabled(false);
        m_pButtonBox->button(QDialogButtonBox::Save)->setEnabled(false);
        connect(m_pButtonBox, &QIDialogButtonBox::rejected, this, &QIManagerDialog::sigClose);

        /* Configure button-box: */
        configureButtonBox();

        /* Add into layout: */
        centralWidget()->layout()->addWidget(m_pButtonBox);
    }
}

void QIManagerDialog::prepareMenuBar()
{
    /* Add widget menu: */
    menuBar()->addMenu(m_pWidgetMenu);

#ifdef VBOX_WS_MAC
    /* Prepare 'Window' menu: */
    AssertPtrReturnVoid(gpWindowMenuManager);
    menuBar()->addMenu(gpWindowMenuManager->createMenu(this));
    gpWindowMenuManager->addWindow(this);
#endif
}

#ifdef VBOX_WS_MAC
void QIManagerDialog::prepareToolBar()
{
    /* Enable unified toolbar on macOS: */
    addToolBar(m_pWidgetToolbar);
    m_pWidgetToolbar->enableMacToolbar();
}
#endif

void QIManagerDialog::cleanupMenuBar()
{
#ifdef VBOX_WS_MAC
    /* Cleanup 'Window' menu: */
    AssertPtrReturnVoid(gpWindowMenuManager);
    gpWindowMenuManager->removeWindow(this);
    gpWindowMenuManager->destroyMenu(this);
#endif
}

void QIManagerDialog::cleanup()
{
    /* Cleanup menu-bar: */
    cleanupMenuBar();
}

void QIManagerDialog::closeEvent(QCloseEvent *pEvent)
{
    /* Ignore the event itself: */
    pEvent->ignore();
    /* But tell the listener to close us: */
    emit sigClose();
}

