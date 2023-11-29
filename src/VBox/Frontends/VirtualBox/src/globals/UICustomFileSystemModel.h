/* $Id$ */
/** @file
 * VBox Qt GUI - UICustomFileSystemModel class declaration.
 */

/*
 * Copyright (C) 2016-2023 Oracle and/or its affiliates.
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

#ifndef FEQT_INCLUDED_SRC_globals_UICustomFileSystemModel_h
#define FEQT_INCLUDED_SRC_globals_UICustomFileSystemModel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

class UICustomFileSystemModel;

enum UICustomFileSystemModelData
{
    UICustomFileSystemModelData_Name = 0,
    UICustomFileSystemModelData_Size,
    UICustomFileSystemModelData_ChangeTime,
    UICustomFileSystemModelData_Owner,
    UICustomFileSystemModelData_Permissions,
    UICustomFileSystemModelData_LocalPath,
    UICustomFileSystemModelData_ISOFilePath, /* in case of import-iso this contains full path of the container iso file. */
    UICustomFileSystemModelData_RemovedFromVISO,
    UICustomFileSystemModelData_DescendantRemovedFromVISO,
    UICustomFileSystemModelData_Max
};

/** A UICustomFileSystemItem instance is a tree node representing a file object (file, directory, etc). The tree contructed
    by these instances is the data source for the UICustomFileSystemModel. */
class SHARED_LIBRARY_STUFF UICustomFileSystemItem
{

public:

    /** @p strName contains file object name which is assumed to be unique among a parent object's children. */
    UICustomFileSystemItem(const QString &strFileObjectName, UICustomFileSystemItem *parentItem, KFsObjType type);
    virtual ~UICustomFileSystemItem();

    void reset();
    virtual UICustomFileSystemItem *child(int row) const;
    /** Searches for the child by file object name and returns it if found. */
    UICustomFileSystemItem *child(const QString &strFileObjectName) const;
    int childCount() const;
    QList<UICustomFileSystemItem*> children() const;
    /** Removes the item from the list of children and !!DELETES!! the item. */
    void removeChild(UICustomFileSystemItem *pItem);
    void removeChildren();
    int columnCount() const;
    QVariant data(int column) const;
    void setData(const QVariant &data, int index);
    void setData(const QVariant &data, UICustomFileSystemModelData enmColumn);
    int row() const;
    UICustomFileSystemItem *parentItem();
    UICustomFileSystemItem *parentItem() const;

    bool isDirectory() const;
    bool isSymLink() const;
    bool isFile() const;

    bool isOpened() const;
    void setIsOpened(bool flag);

    /** Full absolute path of the item. With or without the trailing '/' */
    QString  path(bool fRemoveTrailingDelimiters = false) const;

    /** Returns true if this is directory and file object name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

    KFsObjType  type() const;

    const QString &targetPath() const;
    void setTargetPath(const QString &path);

    bool isSymLinkToADirectory() const;
    void setIsSymLinkToADirectory(bool flag);

    bool isSymLinkToAFile() const;

    const QString &owner() const;
    void setOwner(const QString &owner);

    QString fileObjectName() const;

    void setIsDriveItem(bool flag);
    bool isDriveItem() const;

    void setIsHidden(bool flag);
    bool isHidden() const;

    void setRemovedFromViso(bool fRemoved);
    bool isRemovedFromViso() const;

    void setToolTip(const QString &strToolTip);
    const QString &toolTip() const;

private:

    void appendChild(UICustomFileSystemItem *child);
    void setParentModel(UICustomFileSystemModel *pModel);
    UICustomFileSystemModel *parentModel();

    QList<UICustomFileSystemItem*>               m_childItems;
    QMap<UICustomFileSystemModelData, QVariant>  m_itemData;
    UICustomFileSystemItem *m_parentItem;
    bool             m_bIsOpened;
    /** If this is a symlink m_targetPath keeps the absolute path of the target */
    QString          m_strTargetPath;
    /** True if this is a symlink and the target is a directory */
    bool             m_fIsTargetADirectory;
    KFsObjType       m_type;
    /** True if only this item represents a DOS style drive letter item */
    bool             m_fIsDriveItem;
    /** True if the file object is hidden in the file system. */
    bool             m_fIsHidden;
    QString          m_strToolTip;
    /** Pointer to the parent model. */
    UICustomFileSystemModel *m_pParentModel;
    friend UICustomFileSystemModel;
};

/** A QSortFilterProxyModel extension used in file tables. Modifies some
 *  of the base class behavior like lessThan(..) */
class SHARED_LIBRARY_STUFF UICustomFileSystemProxyModel : public QSortFilterProxyModel
{

    Q_OBJECT;

public:

    UICustomFileSystemProxyModel(QObject *parent = 0);

    void setListDirectoriesOnTop(bool fListDirectoriesOnTop);
    bool listDirectoriesOnTop() const;

    void setShowHiddenObjects(bool fShowHiddenObjects);
    bool showHiddenObjects() const;

protected:

    virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const RT_OVERRIDE;
    /** Currently filters out hidden objects if options is set to "not showing them". */
    virtual bool filterAcceptsRow(int iSourceRow, const QModelIndex &sourceParent) const RT_OVERRIDE;

private:

    bool m_fListDirectoriesOnTop;
    bool m_fShowHiddenObjects;
};

/** UICustomFileSystemModel serves as the model for a file structure.
 *  it supports a tree level hierarchy which can be displayed with
 *  QTableView and/or QTreeView.*/
class SHARED_LIBRARY_STUFF UICustomFileSystemModel : public QAbstractItemModel
{

    Q_OBJECT;

signals:

    void sigItemRenamed(UICustomFileSystemItem *pItem, const QString &strOldPath,
                        const QString &strOldName, const QString &strNewName);

public:

    explicit UICustomFileSystemModel(QObject *parent = 0);
    ~UICustomFileSystemModel();

    QVariant       data(const QModelIndex &index, int role) const RT_OVERRIDE;
    bool           setData(const QModelIndex &index, const QVariant &value, int role);

    Qt::ItemFlags  flags(const QModelIndex &index) const RT_OVERRIDE;
    QVariant       headerData(int section, Qt::Orientation orientation,
                              int role = Qt::DisplayRole) const RT_OVERRIDE;
    QModelIndex    index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE;
    QModelIndex    index(const UICustomFileSystemItem* item);
    QModelIndex    parent(const QModelIndex &index) const RT_OVERRIDE;
    int            rowCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE;
    int            columnCount(const QModelIndex &parent = QModelIndex()) const RT_OVERRIDE;
    void           signalUpdate();
    QModelIndex    rootIndex() const;
    void           beginReset();
    void           endReset();
    void           reset();

    void           setShowHumanReadableSizes(bool fShowHumanReadableSizes);
    bool           showHumanReadableSizes() const;
    void           deleteItem(UICustomFileSystemItem* pItem);
    UICustomFileSystemItem* rootItem();
    const UICustomFileSystemItem* rootItem() const;

    void setIsWindowsFileSystem(bool fIsWindowsFileSystem);
    bool isWindowsFileSystem() const;

    static const char* strUpDirectoryString;

private:

    UICustomFileSystemItem    *m_pRootItem;
    void setupModelData(const QStringList &lines, UICustomFileSystemItem *parent);
    bool m_fShowHumanReadableSizes;
    bool m_fIsWindowFileSystemModel;
};


#endif /* !FEQT_INCLUDED_SRC_globals_UICustomFileSystemModel_h */
