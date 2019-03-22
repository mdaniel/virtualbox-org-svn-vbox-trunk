/* $Id$ */
/** @file
 * VBox Qt GUI - Qt extensions: QIManagerDialog class declaration.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___QIManagerDialog_h___
#define ___QIManagerDialog_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMainWindow>
#include <QMap>

/* GUI includes: */
#include "UILibraryDefs.h"

/* Other VBox includes: */
#include <iprt/cdefs.h>

/* Forward declarations: */
class QPushButton;
class QIDialogButtonBox;
class QIManagerDialog;
#ifdef VBOX_WS_MAC
class UIToolBar;
#endif


/** Widget embedding type. */
enum EmbedTo
{
    EmbedTo_Dialog,
    EmbedTo_Stack
};


/** Dialog button types. */
enum ButtonType
{
    ButtonType_Invalid = 0,
    ButtonType_Reset   = RT_BIT(0),
    ButtonType_Apply   = RT_BIT(1),
    ButtonType_Close   = RT_BIT(2),
};


/** Manager dialog factory insterface. */
class SHARED_LIBRARY_STUFF QIManagerDialogFactory
{

public:

    /** Constructs Manager dialog factory. */
    QIManagerDialogFactory() {}
    /** Destructs Manager dialog factory. */
    virtual ~QIManagerDialogFactory() {}

    /** Prepares Manager dialog @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void prepare(QIManagerDialog *&pDialog, QWidget *pCenterWidget = 0);
    /** Cleanups Manager dialog @a pDialog instance. */
    virtual void cleanup(QIManagerDialog *&pDialog);

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) = 0;
};


/** QMainWindow sub-class used as various manager dialogs. */
class SHARED_LIBRARY_STUFF QIManagerDialog : public QMainWindow
{
    Q_OBJECT;

signals:

    /** Notifies listeners about dialog change. */
    void sigChange();

    /** Notifies listeners about dialog should be closed. */
    void sigClose();

protected:

    /** Constructs Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    QIManagerDialog(QWidget *pCenterWidget);

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all.
          * @note Normally you don't need to reimplement it. */
        void prepare();
        /** Configures all.
          * @note Injected into prepare(), reimplement to configure all there. */
        virtual void configure() {}
        /** Prepares central-widget.
          * @note Injected into prepare(), normally you don't need to reimplement it. */
        void prepareCentralWidget();
        /** Configures central-widget.
          * @note Injected into prepareCentralWidget(), reimplement to configure central-widget there. */
        virtual void configureCentralWidget() {}
        /** Prepares button-box.
          * @note Injected into prepareCentralWidget(), normally you don't need to reimplement it. */
        void prepareButtonBox();
        /** Configures button-box.
          * @note Injected into prepareButtonBox(), reimplement to configure button-box there. */
        virtual void configureButtonBox() {}
        /** Prepares menu-bar.
          * @note Injected into prepare(), normally you don't need to reimplement it. */
        void prepareMenuBar();
#ifdef VBOX_WS_MAC
        /** Prepares toolbar.
          * @note Injected into prepare(), normally you don't need to reimplement it. */
        void prepareToolBar();
#endif
        /** Performs final preparations.
          * @note Injected into prepare(), reimplement to postprocess all there. */
        virtual void finalize() {}
        /** Loads dialog setting such as geometry from extradata. */
        virtual void loadSettings() {}

        /** Saves dialog setting into extradata. */
        virtual void saveSettings() const {}
        /** Cleanup menu-bar.
          * @note Injected into cleanup(), normally you don't need to reimplement it. */
        void cleanupMenuBar();
        /** Cleanups all.
          * @note Normally you don't need to reimplement it. */
        void cleanup();
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Defines the @a pWidget instance. */
        void setWidget(QWidget *pWidget) { m_pWidget = pWidget; }
        /** Defines the @a pWidgetMenu instance. */
        void setWidgetMenu(QMenu *pWidgetMenu) { m_pWidgetMenu = pWidgetMenu; }
#ifdef VBOX_WS_MAC
        /** Defines the @a pWidgetToolbar instance. */
        void setWidgetToolbar(UIToolBar *pWidgetToolbar) { m_pWidgetToolbar = pWidgetToolbar; }
#endif

        /** Returns the widget. */
        virtual QWidget *widget() { return m_pWidget; }
        /** Returns the button-box instance. */
        QIDialogButtonBox *buttonBox() { return m_pButtonBox; }
        /** Returns button of passed @a enmType. */
        QPushButton *button(ButtonType enmType) { return m_buttons.value(enmType); }
        /** Returns center widget. */
        QWidget* centerWidget() const { return pCenterWidget; }
    /** @} */

    /** @name Event-handling stuff.
      * @{ */
        /** Handles close @a pEvent. */
        void closeEvent(QCloseEvent *pEvent);
    /** @} */

    /** @name Functions related to geometry restoration.
     * @{ */
        /** Sets the position and size of the dialog. */
        void setDialogGeometry(const QRect &geometry);
        /** Returns whether the window should be maximized when geometry being restored. */
        virtual bool shouldBeMaximized() const { return false; }
    /** @} */

private:

    /** @name General stuff.
      * @{ */
        /** Holds the widget reference to center Host Network Manager according. */
        QWidget *pCenterWidget;

        /** Holds whether the manager had emitted command to be closed. */
        bool m_fCloseEmitted;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Holds the widget instance. */
        QWidget *m_pWidget;

        /** Holds the widget menu instance. */
        QMenu     *m_pWidgetMenu;
#ifdef VBOX_WS_MAC
        /** Holds the widget toolbar instance. */
        UIToolBar *m_pWidgetToolbar;
#endif
    /** @} */

    /** @name Button-box stuff.
      * @{ */
        /** Holds the dialog button-box instance. */
        QIDialogButtonBox *m_pButtonBox;

        /** Holds the button-box button references. */
        QMap<ButtonType, QPushButton*> m_buttons;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class QIManagerDialogFactory;
};


#endif /* !___QIManagerDialog_h___ */
