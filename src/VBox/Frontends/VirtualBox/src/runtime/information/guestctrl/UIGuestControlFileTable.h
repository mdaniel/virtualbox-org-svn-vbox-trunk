/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlFileTable class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlFileTable_h___
#define ___UIGuestControlFileTable_h___

/* Qt includes: */
#include <QAbstractItemModel>
#include <QTreeView>
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QITableView.h"

/* Forward declarations: */
class QILineEdit;
class QVBoxLayout;
class UIFileTableItem;
class UIGuestControlFileTable;

/** UIGuestControlFileModel serves as the model for a file structure.
    it supports a tree level hierarchy which can be displayed with
    QTableView and/or QTreeView */
class UIGuestControlFileModel : public QAbstractItemModel
{

    Q_OBJECT;

public:

    explicit UIGuestControlFileModel(QObject *parent = 0);
    ~UIGuestControlFileModel();

    QVariant data(const QModelIndex &index, int role) const /* override */;
    Qt::ItemFlags flags(const QModelIndex &index) const /* override */;
    QVariant headerData(int section, Qt::Orientation orientation,
    int role = Qt::DisplayRole) const /* override */;
    QModelIndex index(int row, int column,
    const QModelIndex &parent = QModelIndex()) const /* override */;
    QModelIndex parent(const QModelIndex &index) const /* override */;
    int rowCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    int columnCount(const QModelIndex &parent = QModelIndex()) const /* override */;
    void signalUpdate();
    QModelIndex rootIndex() const;

private:

    UIFileTableItem* rootItem() const;
    void setupModelData(const QStringList &lines, UIFileTableItem *parent);
    UIGuestControlFileTable* m_pParent;
    UIFileTableItem *m_pRootItem;
};

/** This serves a base class for file table. Currently a guest version
    and a host version are derived from this base. Each of these children
    populates the UIGuestControlFileModel by scanning the file system
    differently. */
class UIGuestControlFileTable : public QWidget
{
    Q_OBJECT;

public:

    UIGuestControlFileTable(QWidget *pParent = 0);
    virtual ~UIGuestControlFileTable();

protected:

    void updateCurrentLocationEdit(const QString& strLocation);
    void changeLocation(const QModelIndex &index);
    UIFileTableItem         *m_pRootItem;

    /** Using QITableView causes the following problem when I click on the table items
        Qt WARNING: Cannot creat accessible child interface for object:  UIGuestControlFileView.....
        so for now subclass QTableView */
    QTableView   *m_pView;
    UIGuestControlFileModel *m_pModel;
    QTreeView   *m_pTree;


private:

    void                    prepareObjects();
    QVBoxLayout             *m_pMainLayout;
    QILineEdit              *m_pCurrentLocationEdit;

    friend class UIGuestControlFileModel;
};

/** This class scans the guest file system by using the VBox API
    and populates the UIGuestControlFileModel*/
class UIGuestFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIGuestFileTable(QWidget *pParent = 0);
    void initGuestFileTable(const CGuestSession &session);

private slots:

    void sltItemDoubleClicked(const QModelIndex &index);

private:
    void readDirectory(const QString& strPath,
                       UIFileTableItem *parent);
    void insertToTree(const QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent);
    CGuestSession m_comGuestSession;
};

#endif /* !___UIGuestControlFileTable_h___ */
