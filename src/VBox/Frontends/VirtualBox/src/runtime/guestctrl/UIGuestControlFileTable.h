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
#include <QWidget>

/* COM includes: */
#include "COMEnums.h"
#include "CGuestSession.h"

/* GUI includes: */
#include "QITableView.h"
#include "QIWithRetranslateUI.h"

/* Forward declarations: */
class QAction;
class QFileInfo;
class QILabel;
class QILineEdit;
class QGridLayout;
class UIFileTableItem;
class UIGuestControlFileModel;
class UIGuestControlFileView;
class UIToolBar;

enum FileObjectType
{
    FileObjectType_File = 0,
    FileObjectType_Directory,
    FileObjectType_SymLink,
    FileObjectType_Other,
    FileObjectType_Unknown,
    FileObjectType_Max
};

/** A collection of simple utility functions to manipulate path strings */
class UIPathOperations
{
public:
    static QString removeMultipleDelimiters(const QString &path);
    static QString removeTrailingDelimiters(const QString &path);
    static QString addStartDelimiter(const QString &path);

    static QString sanitize(const QString &path);
    /** Merge prefix and suffix by making sure they have a single '/' in between */
    static QString mergePaths(const QString &path, const QString &baseName);
    /** Returns the last part of the @p path. That is the filename or directory name without the path */
    static QString getObjectName(const QString &path);
    /** Remove the object name and return the path */
    static QString getPathExceptObjectName(const QString &path);
    /** Replace the last part of the @p previusPath with newBaseName */
    static QString constructNewItemPath(const QString &previousPath, const QString &newBaseName);

    static const QChar delimiter;
};

/** A UIFileTableItem instance is a tree node representing a file object (file, directory, etc). The tree contructed
    by these instances is the data source for the UIGuestControlFileModel */
class UIFileTableItem
{
public:

    explicit UIFileTableItem(const QList<QVariant> &data,
                             UIFileTableItem *parentItem, FileObjectType type);
    ~UIFileTableItem();

    void appendChild(UIFileTableItem *child);

    UIFileTableItem *child(int row) const;
    /** Return a child (if possible) by path */
    UIFileTableItem *child(const QString &path) const;
    int childCount() const;
    int columnCount() const;
    QVariant data(int column) const;
    void setData(const QVariant &data, int index);
    int row() const;
    UIFileTableItem *parentItem();

    bool isDirectory() const;
    bool isSymLink() const;
    bool isFile() const;

    bool isOpened() const;
    void setIsOpened(bool flag);

    const QString  &path() const;
    void setPath(const QString &path);

    /** True if this is directory and name is ".." */
    bool isUpDirectory() const;
    void clearChildren();

    FileObjectType   type() const;

    const QString &targetPath() const;
    void setTargetPath(const QString &path);

    bool isTargetADirectory() const;
    void setIsTargetADirectory(bool flag);

private:
    QList<UIFileTableItem*>         m_childItems;
    /** Used to find children by path */
    QMap<QString, UIFileTableItem*> m_childMap;
    QList<QVariant>  m_itemData;
    UIFileTableItem *m_parentItem;
    bool             m_bIsOpened;
    /** Full absolute path of the item. Without the trailing '/' */
    QString          m_strPath;
    /** If this is a symlink m_targetPath keeps the absolute path of the target */
    QString          m_strTargetPath;
    /** True if this is a symlink and the target is a directory */
    bool             m_isTargetADirectory;
    FileObjectType   m_type;

};


/** This class serves a base class for file table. Currently a guest version
    and a host version are derived from this base. Each of these children
    populates the UIGuestControlFileModel by scanning the file system
    differently. The file structre kept in this class as a tree. */
class UIGuestControlFileTable : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

signals:

    void sigLogOutput(QString);

public:

    UIGuestControlFileTable(QWidget *pParent = 0);
    virtual ~UIGuestControlFileTable();
    /** Delete all the tree nodes */
    void reset();
    void emitLogOutput(const QString& strOutput);
    /** Returns the path of the rootIndex */
    QString     currentDirectoryPath() const;
    /** Returns the paths of the selected items (if any) as a list */
    QStringList selectedItemPathList();
    virtual void refresh();

protected:

    void retranslateUi();
    void updateCurrentLocationEdit(const QString& strLocation);
    void changeLocation(const QModelIndex &index);
    void initializeFileTree();
    void insertItemsToTree(QMap<QString,UIFileTableItem*> &map, UIFileTableItem *parent,
                           bool isDirectoryMap, bool isStartDir);
    virtual void readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) = 0;
    virtual void deleteByItem(UIFileTableItem *item) = 0;
    virtual void goToHomeDirectory() = 0;
    virtual bool renameItem(UIFileTableItem *item, QString newBaseName) = 0;
    virtual bool createDirectory(const QString &path, const QString &directoryName) = 0;
    virtual QString fsObjectPropertyString() = 0;
    static QString fileTypeString(FileObjectType type);
    void             goIntoDirectory(const QModelIndex &itemIndex);
    /** Follow the path trail, open directories as we go and descend */
    void             goIntoDirectory(const QList<QString> &pathTrail);
    /** Go into directory pointed by the @p item */
    void             goIntoDirectory(UIFileTableItem *item);
    UIFileTableItem* indexData(const QModelIndex &index) const;
    void keyPressEvent(QKeyEvent * pEvent);
    CGuestFsObjInfo guestFsObjectInfo(const QString& path, CGuestSession &comGuestSession) const;
    static QString humanReadableSize(ULONG64 size);

    UIFileTableItem         *m_pRootItem;

    UIGuestControlFileView  *m_pView;
    UIGuestControlFileModel *m_pModel;
    QILabel                 *m_pLocationLabel;

private slots:

    void sltItemDoubleClicked(const QModelIndex &index);
    void sltGoUp();
    void sltGoHome();
    void sltRefresh();
    void sltDelete();
    void sltRename();
    void sltCopy();
    void sltCut();
    void sltPaste();
    void sltShowProperties();
    void sltCreateNewDirectory();
    void sltSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);

private:

    void             prepareObjects();
    void             prepareActions();
    void             deleteByIndex(const QModelIndex &itemIndex);
    /** Return the UIFileTableItem for path / which is a direct (and single) child of m_pRootItem */
    UIFileTableItem *getStartDirectoryItem();
    /** Shows a modal dialog with a line edit for user to enter a new directory name and return the entered string*/
    QString         getNewDirectoryName();
    void            enableSelectionDependentActions();
    void            disableSelectionDependentActions();
    QGridLayout     *m_pMainLayout;
    QILineEdit      *m_pCurrentLocationEdit;
    UIToolBar       *m_pToolBar;
    QAction         *m_pGoUp;
    QAction         *m_pGoHome;
    QAction         *m_pRefresh;
    QAction         *m_pDelete;
    QAction         *m_pRename;
    QAction         *m_pCreateNewDirectory;
    QAction         *m_pCopy;
    QAction         *m_pCut;
    QAction         *m_pPaste;
    QAction         *m_pShowProperties;
    /** The vector of action which need some selection to work on like cut, copy etc. */
    QVector<QAction*> m_selectionDependentActions;
    /** The absolue path list of the file objects which user has chosen to cut/copy. this
        list will be cleaned after a paste operation or overwritten by a subsequent cut/copy */
    QStringList       m_copyCutBuffer;
    friend class UIGuestControlFileModel;
};

#endif /* !___UIGuestControlFileTable_h___ */
