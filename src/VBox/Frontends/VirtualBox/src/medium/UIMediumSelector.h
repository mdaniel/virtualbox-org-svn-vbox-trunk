/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumSelector class declaration.
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

#ifndef FEQT_INCLUDED_SRC_medium_UIMediumSelector_h
#define FEQT_INCLUDED_SRC_medium_UIMediumSelector_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIMainDialog.h"
#include "QIWithRetranslateUI.h"
#include "UIMedium.h"
#include "UIMediumDefs.h"


/* Forward declarations: */
class QAction;
class QTreeWidgetItem;
class QITreeWidget;
class QITreeWidgetItem;
class QVBoxLayout;
class QIDialogButtonBox;
class UIMediumItem;
class UIMediumSearchWidget;
class UIToolBar;


/** QIDialog extension providing GUI with a dialog to select an existing medium. */
class SHARED_LIBRARY_STUFF UIMediumSelector : public QIWithRetranslateUI<QIMainDialog>
{

    Q_OBJECT;

signals:

public:

    UIMediumSelector(UIMediumDeviceType enmMediumType, const QString &machineName = QString(),
                     const QString &machineSettigFilePath = QString(), QWidget *pParent = 0);
    QList<QUuid> selectedMediumIds() const;

protected:

    void showEvent(QShowEvent *pEvent);

private slots:

    void sltAddMedium();
    void sltCreateMedium();
    void sltHandleItemSelectionChanged();
    void sltHandleTreeWidgetDoubleClick(QTreeWidgetItem * item, int column);
    void sltHandleMediumEnumerationStart();
    void sltHandleMediumEnumerated();
    void sltHandleMediumEnumerationFinish();
    void sltHandleRefresh();
    void sltHandlePerformSearch();
    void sltHandleShowNextMatchingItem();
    void sltHandleShowPreviousMatchingItem();
    void sltHandleTreeContextMenuRequest(const QPoint &point);
    void sltHandleTreeExpandAllSignal();
    void sltHandleTreeCollapseAllSignal();

 private:


    /** @name Event-handling stuff.
      * @{ */
        /** Handles translation event. */
        virtual void retranslateUi() /* override */;
    /** @} */

    /** @name Prepare/cleanup cascade.
      * @{ */
        /** Configures all. */
            void configure();
            void prepareWidgets();
            void prepareActions();
            void prepareConnections();
        /** Perform final preparations. */
        void finalize();
    /** @} */

    void          repopulateTreeWidget();
    /** Disable/enable 'ok' button on the basis of having a selected item */
    void          updateOkButton();
    UIMediumItem* addTreeItem(const UIMedium &medium, QITreeWidgetItem *pParent);
    void          restoreSelection(const QList<QUuid> &selectedMediums, QVector<UIMediumItem*> &mediumList);
    /** Recursively create the hard disk hierarchy under the tree widget */
    UIMediumItem* createHardDiskItem(const UIMedium &medium, QITreeWidgetItem *pParent);
    UIMediumItem* searchItem(const QTreeWidgetItem *pParent, const QUuid &mediumId);
    void          performMediumSearch();
    /** Remember the default foreground brush of the tree so that we can reset tree items' foreground later */
    void          saveDefaultForeground();
    void          selectMedium(const QUuid &uMediumID);
    void          scrollToItem(UIMediumItem* pItem);
    QWidget              *m_pCentralWidget;
    QVBoxLayout          *m_pMainLayout;
    QITreeWidget         *m_pTreeWidget;
    UIMediumDeviceType    m_enmMediumType;
    QIDialogButtonBox    *m_pButtonBox;
    QMenu                *m_pMainMenu;
    UIToolBar            *m_pToolBar;
    QAction              *m_pActionAdd;
    QAction              *m_pActionCreate;
    QAction              *m_pActionRefresh;
    /** All the known media that are already attached to some vm are added under the following top level tree item */
    QITreeWidgetItem     *m_pAttachedSubTreeRoot;
    /** All the known media that are not attached to any vm are added under the following top level tree item */
    QITreeWidgetItem     *m_pNotAttachedSubTreeRoot;
    QWidget              *m_pParent;
    UIMediumSearchWidget *m_pSearchWidget;
    /** The list all items added to tree. kept in sync. with tree to make searching easier (faster). */
    QList<UIMediumItem*>  m_mediumItemList;
    /** List of items that are matching to the search. */
    QList<UIMediumItem*>  m_mathingItemList;
    /** Index of the currently shown (scrolled) item in the m_mathingItemList. */
    int                   m_iCurrentShownIndex;
    QBrush                m_defaultItemForeground;
    QString               m_strMachineSettingsFilePath;
    QString               m_strMachineName;
};

#endif /* !FEQT_INCLUDED_SRC_medium_UIMediumSelector_h */
