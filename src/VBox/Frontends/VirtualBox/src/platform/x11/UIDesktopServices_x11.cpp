/* $Id$ */
/** @file
 * VBox Qt GUI - Qt GUI - Utility Classes and Functions specific to X11..
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* VBox includes */
#include "UIDesktopServices.h"

/* Qt includes */
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QUrl>


bool UIDesktopServices::createMachineShortcut(const QString & /* strSrcFile */, const QString &strDstPath, const QString &strName, const QUuid &uUuid)
{
    QFile link(strDstPath + QDir::separator() + strName + ".desktop");
    if (link.open(QFile::WriteOnly | QFile::Truncate))
    {
#ifdef VBOX_GUI_WITH_SHARED_LIBRARY
        const QString strVBox = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" + VBOX_GUI_VMRUNNER_IMAGE);
#else
        const QString strVBox = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
#endif
        QTextStream out(&link);
        out.setCodec("UTF-8");
        /* Create a link which starts VirtualBox with the machine uuid. */
        out << "[Desktop Entry]" << endl
            << "Encoding=UTF-8" << endl
            << "Version=1.0" << endl
            << "Name=" << strName << endl
            << "Comment=Starts the VirtualBox machine " << strName << endl
            << "Type=Application" << endl
            << "Exec=" << strVBox << " --comment \"" << strName << "\" --startvm \"" << uUuid.toString() << "\"" << endl
            << "Icon=virtualbox-vbox.png" << endl;
        /* This would be a real file link entry, but then we could also simply
         * use a soft link (on most UNIX fs):
        out << "[Desktop Entry]" << endl
            << "Encoding=UTF-8" << endl
            << "Version=1.0" << endl
            << "Name=" << strName << endl
            << "Type=Link" << endl
            << "Icon=virtualbox-vbox.png" << endl
        */
        link.setPermissions(link.permissions() | QFile::ExeOwner);
        return true;
    }
    return false;
}

bool UIDesktopServices::openInFileManager(const QString &strFile)
{
    QFileInfo fi(strFile);
    return QDesktopServices::openUrl(QUrl("file://" + QDir::toNativeSeparators(fi.absolutePath()), QUrl::TolerantMode));
}

