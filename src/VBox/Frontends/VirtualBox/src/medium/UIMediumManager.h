/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumManager class declaration.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_medium_UIMediumManager_h
#define FEQT_INCLUDED_SRC_medium_UIMediumManager_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIManagerDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMediumDefs.h"

/* Forward declarations: */
class QAbstractButton;
class QLabel;
class QProgressBar;
class QTabWidget;
class QTreeWidgetItem;
class QIDialogButtonBox;
class QILabel;
class QITreeWidget;
class UIActionPool;
class UIMedium;
class UIMediumDetailsWidget;
class UIMediumItem;
class UIToolBar;


/** Functor interface allowing to check if passed UIMediumItem is suitable. */
class CheckIfSuitableBy
{
public:

    /** Destructs functor. */
    virtual ~CheckIfSuitableBy() { /* Makes MSC happy. */ }

    /** Determines whether passed @a pItem is suitable. */
    virtual bool isItSuitable(UIMediumItem *pItem) const = 0;
};


/** Medium manager progress-bar.
  * Reflects medium-enumeration progress, stays hidden otherwise. */
class UIEnumerationProgressBar : public QWidget
{
    Q_OBJECT;

public:

    /** Constructor on the basis of passed @a pParent. */
    UIEnumerationProgressBar(QWidget *pParent = 0);

    /** Defines progress-bar label-text. */
    void setText(const QString &strText);

    /** Returns progress-bar current-value. */
    int value() const;
    /** Defines progress-bar current-value. */
    void setValue(int iValue);
    /** Defines progress-bar maximum-value. */
    void setMaximum(int iValue);

private:

    /** Prepares progress-bar content. */
    void prepare();

    /** Progress-bar label. */
    QLabel       *m_pLabel;
    /** Progress-bar itself. */
    QProgressBar *m_pProgressBar;
};


/** QWidget extension providing GUI with the pane to control media related functionality. */
class UIMediumManagerWidget : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

    /** Item action types. */
    enum Action { Action_Add, Action_Edit, Action_Copy, Action_Remove, Action_Release };

signals:

    /** Notifies listeners about medium details-widget @a fVisible. */
    void sigMediumDetailsVisibilityChanged(bool fVisible);
    /** Notifies listeners about accept is @a fAllowed. */
    void sigAcceptAllowed(bool fAllowed);
    /** Notifies listeners about reject is @a fAllowed. */
    void sigRejectAllowed(bool fAllowed);

public:

    /** Constructs Virtual Media Manager widget.
      * @param  enmEmbedding  Brings the type of widget embedding.
      * @param  pActionPool   Brings the action-pool reference.
      * @param  fShowToolbar  Brings whether we should create/show toolbar. */
    UIMediumManagerWidget(EmbedTo enmEmbedding, UIActionPool *pActionPool, bool fShowToolbar = true, QWidget *pParent = 0);

    /** Returns the menu. */
    QMenu *menu() const;

#ifdef VBOX_WS_MAC
    /** Returns the toolbar. */
    UIToolBar *toolbar() const { return m_pToolBar; }
#endif

    /** Defines @a pProgressBar reference. */
    void setProgressBar(UIEnumerationProgressBar *pProgressBar);

protected:

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

public slots:

    /** @name Details-widget stuff.
      * @{ */
        /** Handles command to reset medium details changes. */
        void sltResetMediumDetailsChanges();
        /** Handles command to apply medium details changes. */
        void sltApplyMediumDetailsChanges();
    /** @} */

private slots:

    /** @name Medium operation stuff.
      * @{ */
        /** Handles VBoxGlobal::sigMediumCreated signal. */
        void sltHandleMediumCreated(const QUuid &uMediumID);
        /** Handles VBoxGlobal::sigMediumDeleted signal. */
        void sltHandleMediumDeleted(const QUuid &uMediumID);
    /** @} */

    /** @name Medium enumeration stuff.
      * @{ */
        /** Handles VBoxGlobal::sigMediumEnumerationStarted signal. */
        void sltHandleMediumEnumerationStart();
        /** Handles VBoxGlobal::sigMediumEnumerated signal. */
        void sltHandleMediumEnumerated(const QUuid &uMediumID);
        /** Handles VBoxGlobal::sigMediumEnumerationFinished signal. */
        void sltHandleMediumEnumerationFinish();
    /** @} */

    /** @name Menu/action stuff.
      * @{ */
        /** Handles command to add medium. */
        void sltAddMedium();
        /** Handles command to copy medium. */
        void sltCopyMedium();
        /** Handles command to move medium. */
        void sltMoveMedium();
        /** Handles command to remove medium. */
        void sltRemoveMedium();
        /** Handles command to release medium. */
        void sltReleaseMedium();
        /** Handles command to make medium details @a fVisible. */
        void sltToggleMediumDetailsVisibility(bool fVisible);
        /** Handles command to refresh medium. */
        void sltRefreshAll();
    /** @} */

    /** @name Tab-widget stuff.
      * @{ */
        /** Handles tab change case. */
        void sltHandleCurrentTabChanged();
        /** Handles item change case. */
        void sltHandleCurrentItemChanged();
        /** Handles item context-menu-call case. */
        void sltHandleContextMenuRequest(const QPoint &position);
    /** @} */

    /** @name Tree-widget stuff.
      * @{ */
        /** Adjusts tree-widgets according content. */
        void sltPerformTablesAdjustment();
    /** @} */

private:

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Prepares all. */
        void prepare();
        /** Prepares connections. */
        void prepareConnections();
        /** Prepares actions. */
        void prepareActions();
        /** Prepares widgets. */
        void prepareWidgets();
        /** Prepares toolbar. */
        void prepareToolBar();
        /** Prepares tab-widget. */
        void prepareTabWidget();
        /** Prepares tab-widget's tab. */
        void prepareTab(UIMediumDeviceType type);
        /** Prepares tab-widget's tree-widget. */
        void prepareTreeWidget(UIMediumDeviceType type, int iColumns);
        /** Prepares details-widget. */
        void prepareDetailsWidget();
        /** Load settings: */
        void loadSettings();

        /** Repopulates tree-widgets content. */
        void repopulateTreeWidgets();

        /** Updates details according latest changes in current item of passed @a type. */
        void refetchCurrentMediumItem(UIMediumDeviceType type);
        /** Updates details according latest changes in current item of chosen type. */
        void refetchCurrentChosenMediumItem();
        /** Updates details according latest changes in all current items. */
        void refetchCurrentMediumItems();

        /** Updates actions according currently chosen item. */
        void updateActions();
        /** Updates action icons according currently chosen tab. */
        void updateActionIcons();
        /** Updates tab icons according last @a action happened with @a pItem. */
        void updateTabIcons(UIMediumItem *pItem, Action action);
    /** @} */

    /** @name Widget operation stuff.
      * @{ */
        /** Creates UIMediumItem for corresponding @a medium. */
        UIMediumItem *createMediumItem(const UIMedium &medium);
        /** Creates UIMediumItemHD for corresponding @a medium. */
        UIMediumItem *createHardDiskItem(const UIMedium &medium);
        /** Updates UIMediumItem for corresponding @a medium. */
        void updateMediumItem(const UIMedium &medium);
        /** Deletes UIMediumItem for corresponding @a uMediumID. */
        void deleteMediumItem(const QUuid &uMediumID);

        /** Returns tab for passed medium @a type. */
        QWidget *tab(UIMediumDeviceType type) const;
        /** Returns tree-widget for passed medium @a type. */
        QITreeWidget *treeWidget(UIMediumDeviceType type) const;
        /** Returns item for passed medium @a type. */
        UIMediumItem *mediumItem(UIMediumDeviceType type) const;

        /** Returns medium type for passed @a pTreeWidget. */
        UIMediumDeviceType mediumType(QITreeWidget *pTreeWidget) const;

        /** Returns current medium type. */
        UIMediumDeviceType currentMediumType() const;
        /** Returns current tree-widget. */
        QITreeWidget *currentTreeWidget() const;
        /** Returns current item. */
        UIMediumItem *currentMediumItem() const;

        /** Defines current item for passed @a pTreeWidget as @a pItem. */
        void setCurrentItem(QITreeWidget *pTreeWidget, QTreeWidgetItem *pItem);
    /** @} */

    /** @name Helper stuff.
      * @{ */
        /** Returns tab index for passed UIMediumDeviceType. */
        static int tabIndex(UIMediumDeviceType type);

        /** Performs search for the @a pTree child which corresponds to the @a condition but not @a pException. */
        static UIMediumItem *searchItem(QITreeWidget *pTree,
                                        const CheckIfSuitableBy &condition,
                                        CheckIfSuitableBy *pException = 0);
        /** Performs search for the @a pParentItem child which corresponds to the @a condition but not @a pException. */
        static UIMediumItem *searchItem(QTreeWidgetItem *pParentItem,
                                        const CheckIfSuitableBy &condition,
                                        CheckIfSuitableBy *pException = 0);

        /** Checks if @a action can be used for @a pItem. */
        static bool checkMediumFor(UIMediumItem *pItem, Action action);

        /** Casts passed QTreeWidgetItem @a pItem to UIMediumItem if possible. */
        static UIMediumItem *toMediumItem(QTreeWidgetItem *pItem);
    /** @} */

    /** @name General variables.
      * @{ */
        /** Holds the widget embedding type. */
        const EmbedTo m_enmEmbedding;
        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;
        /** Holds whether we should create/show toolbar. */
        const bool    m_fShowToolbar;

        /** Holds whether Virtual Media Manager should preserve current item change. */
        bool m_fPreventChangeCurrentItem;
    /** @} */

    /** @name Tab-widget variables.
      * @{ */
        /** Holds the tab-widget instance. */
        QTabWidget                  *m_pTabWidget;
        /** Holds the tab-widget tab-count. */
        const int                    m_iTabCount;
        /** Holds the map of tree-widget instances. */
        QMap<int, QITreeWidget*>     m_trees;
        /** Holds whether hard-drive tab-widget have inaccessible item. */
        bool                         m_fInaccessibleHD;
        /** Holds whether optical-disk tab-widget have inaccessible item. */
        bool                         m_fInaccessibleCD;
        /** Holds whether floppy-disk tab-widget have inaccessible item. */
        bool                         m_fInaccessibleFD;
        /** Holds cached hard-drive tab-widget icon. */
        const QIcon                  m_iconHD;
        /** Holds cached optical-disk tab-widget icon. */
        const QIcon                  m_iconCD;
        /** Holds cached floppy-disk tab-widget icon. */
        const QIcon                  m_iconFD;
        /** Holds current hard-drive tree-view item ID. */
        QUuid                        m_uCurrentIdHD;
        /** Holds current optical-disk tree-view item ID. */
        QUuid                        m_uCurrentIdCD;
        /** Holds current floppy-disk tree-view item ID. */
        QUuid                        m_uCurrentIdFD;
    /** @} */

    /** @name Details-widget variables.
      * @{ */
        /** Holds the medium details-widget instance. */
        UIMediumDetailsWidget *m_pDetailsWidget;
    /** @} */

    /** @name Toolbar and menu variables.
      * @{ */
        /** Holds the toolbar widget instance. */
        UIToolBar *m_pToolBar;
    /** @} */

    /** @name Progress-bar variables.
      * @{ */
        /** Holds the progress-bar widget reference. */
        UIEnumerationProgressBar *m_pProgressBar;
    /** @} */
};


/** QIManagerDialogFactory extension used as a factory for Virtual Media Manager dialog. */
class UIMediumManagerFactory : public QIManagerDialogFactory
{
public:

    /** Constructs Media Manager factory acquiring additional arguments.
      * @param  pActionPool  Brings the action-pool reference. */
    UIMediumManagerFactory(UIActionPool *pActionPool = 0);

protected:

    /** Creates derived @a pDialog instance.
      * @param  pCenterWidget  Brings the widget reference to center according to. */
    virtual void create(QIManagerDialog *&pDialog, QWidget *pCenterWidget) /* override */;

    /** Holds the action-pool reference. */
    UIActionPool *m_pActionPool;
};


/** QIManagerDialog extension providing GUI with the dialog to control media related functionality. */
class UIMediumManager : public QIWithRetranslateUI<QIManagerDialog>
{
    Q_OBJECT;

signals:

    /** Notifies listeners about data change rejected and should be reseted. */
    void sigDataChangeRejected();
    /** Notifies listeners about data change accepted and should be applied. */
    void sigDataChangeAccepted();

private slots:

    /** @name Button-box stuff.
      * @{ */
        /** Handles button-box button click. */
        void sltHandleButtonBoxClick(QAbstractButton *pButton);
    /** @} */

private:

    /** Constructs Host Network Manager dialog.
      * @param  pCenterWidget  Brings the widget reference to center according to.
      * @param  pActionPool    Brings the action-pool reference. */
    UIMediumManager(QWidget *pCenterWidget, UIActionPool *pActionPool);

    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
        virtual void configure() /* override */;
        /** Configures central-widget. */
        virtual void configureCentralWidget() /* override */;
        /** Configures button-box. */
        virtual void configureButtonBox() /* override */;
        /** Perform final preparations. */
        virtual void finalize() /* override */;
    /** @} */

    /** @name Widget stuff.
      * @{ */
        /** Returns the widget. */
        virtual UIMediumManagerWidget *widget() /* override */;
    /** @} */

    /** @name Action related variables.
      * @{ */
        /** Holds the action-pool reference. */
        UIActionPool *m_pActionPool;
    /** @} */

    /** @name Progress-bar variables.
      * @{ */
        /** Holds the progress-bar widget instance. */
        UIEnumerationProgressBar *m_pProgressBar;
    /** @} */

    /** Allow factory access to private/protected members: */
    friend class UIMediumManagerFactory;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIMediumManager_h */
