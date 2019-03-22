/* $Id$*/
/** @file
 * VBox Qt GUI - Precompiled header for Visual C++.
 */

/*
 * Copyright (C) 2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*
 * General pickings
 *
 * Note! We do not include iprt/log.h or similar because we need to
 *       support selecting different log groups.  So, for now we restrict
 *       ourselves to frequently used QT, compiler, and SDK headers.
 */
#include <QVariant>
#include <QVarLengthArray>
#include <QMutex>
#include <QSysInfo>
#include <QString>
#include <QChar>

#include <QApplication>

#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

/* The most frequently used qt headers on windows hosts based on dependency files. */
#include <qalgorithms.h>
#include <qarraydata.h>
#include <qatomic.h>
#include <qatomic_msvc.h>
#include <qbasicatomic.h>
#include <qbytearray.h>
#include <qchar.h>
#include <qcompilerdetection.h>
#include <qconfig.h>
#include <qcontainerfwd.h>
#include <qfeatures.h>
#include <qflags.h>
#include <qgenericatomic.h>
#include <qglobalstatic.h>
#include <qisenum.h>
#include <qlogging.h>
#include <qmutex.h>
#include <qnamespace.h>
#include <qnumeric.h>
#include <qobjectdefs.h>
#include <qprocessordetection.h>
#include <qrefcount.h>
#include <qstring.h>
#include <qsysinfo.h>
#include <qsystemdetection.h>
#include <qtypeinfo.h>
#include <qvarlengtharray.h>
#include <qpair.h>
#include <qmetatype.h>
#include <qobject.h>
#include <qscopedpointer.h>
#include <qglobal.h>
#include <qbytearraylist.h>
#include <qiterator.h>
#include <qlist.h>
#include <qregexp.h>
#include <qstringlist.h>
#include <qstringmatcher.h>
#include <qtypetraits.h>

/* Less frequently included: */
#include <QtWidgets/QGraphicsWidget>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicslayoutitem.h>
#include <QtWidgets/qgraphicswidget.h>
#include <QtCore/QMetaType>
#include <QtGui/qevent.h>
#include <QtGui/qtouchdevice.h>
#include <QtGui/qvector2d.h>
#include <QtCore/QEvent>
#include <QtGui/qguiapplication.h>
#include <QtGui/qinputmethod.h>
#include <QtWidgets/QApplication>
#include <QtWidgets/qapplication.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qurl.h>
#include <QtCore/qset.h>
#include <QtCore/qfile.h>
#include <QtCore/qfiledevice.h>
#include <QtCore/qlocale.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/QObject>
#include <QtWidgets/qwidget.h>
#include <QtCore/qvariant.h>
#include <QtGui/qfontinfo.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qcursor.h>
#include <QtWidgets/qsizepolicy.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qpalette.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtCore/qmap.h>
#include <QtCore/qline.h>
#include <QtGui/qcolor.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpixelformat.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qpolygon.h>
#include <QtGui/qrgb.h>
#include <QtGui/qtransform.h>
#include <QtCore/qdatastream.h>
#include <QtGui/qregion.h>
#include <QtGui/qwindowdefs.h>
#include <QtGui/qwindowdefs_win.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qsharedpointer.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qvector.h>
#include <QtCore/qmargins.h>
#include <QtCore/qrect.h>
#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtCore/qhash.h>

/* cdefs.h is a little bit of a question since it defines RT_STRICT, which
   someone may want to redefine locally. But we need it for windows.h. */
#include <iprt/cdefs.h>
#include <iprt/win/windows.h>
#include <iprt/types.h>
#include <iprt/cpp/list.h>
#include <iprt/cpp/meta.h>
#include <iprt/cpp/ministring.h>
#include <VBox/com/microatl.h>
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/Guid.h>
#include <VBox/com/string.h>

#if defined(Log) || defined(LogIsEnabled)
# error "Log() from iprt/log.h cannot be defined in the precompiled header!"
#endif

