/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostFileTable class declaration.
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

#ifndef ___UIHostFileTable_h___
#define ___UIHostFileTable_h___

/* GUI includes: */
#include "UIGuestControlFileTable.h"

/* Forward declarations: */
class UIActionPool;

/** This class scans the host file system by using the Qt API
    and connects to the UIGuestControlFileModel*/
class UIHostFileTable : public UIGuestControlFileTable
{
    Q_OBJECT;

public:

    UIHostFileTable(UIActionPool *pActionPool, QWidget *pParent = 0);

protected:

    FileObjectType  fileType(const QFileInfo &fsInfo);
    void            retranslateUi() /* override */;
    virtual void    readDirectory(const QString& strPath, UIFileTableItem *parent, bool isStartDir = false) /* override */;
    virtual void    deleteByItem(UIFileTableItem *item) /* override */;
    virtual void    goToHomeDirectory() /* override */;
    virtual bool    renameItem(UIFileTableItem *item, QString newBaseName);
    virtual bool    createDirectory(const QString &path, const QString &directoryName);
    virtual QString fsObjectPropertyString() /* override */;
    virtual void    showProperties() /* override */;
    virtual void    determineDriveLetters() /* override */;
    virtual void    prepareToolbar() /* override */;

private:

    QString permissionString(QFileDevice::Permissions permissions);
};

#endif /* !___UIHostFileTable_h___ */
