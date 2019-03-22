/* $Id$*/
/** @file
 * VBox Qt GUI - Precompiled header for Visual C++.
 */

/*
 * Copyright (C) 2016-2019 Oracle Corporation
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
 *       ourselves to frequently used QT, compiler, and system headers.
 */
#include <new>
#include <stdlib.h>

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

#include <QtCore/QObject>

/* The most frequently used qt headers on a linux hosts based on dependency files. */
#include <QtCore/qobject.h> /* 1003 */
#include <QtCore/qmetatype.h> /* 958 */
#include <QtCore/qstring.h> /* 923 */
#include <QtCore/qstringlist.h> /* 905 */
#include <QtCore/qrect.h> /* 858 */
#include <QtCore/qvector.h> /* 853 */
#include <QtCore/qmap.h> /* 849 */
#include <QtCore/qcoreevent.h> /* 838 */
#include <QtWidgets/qwidget.h> /* 826 */
#include <QtCore/qpair.h> /* 698 */
#include <QtCore/qlist.h> /* 682 */
#include <QtGui/qpixmap.h> /* 670 */
#include <QtCore/qhash.h> /* 637 */
#include <QtCore/qsize.h> /* 627 */
#include <QtCore/qglobal.h> /* 591 */
#include <QtCore/qvariant.h> /* 588 */
#include <QtCore/qregexp.h> /* 587 */
#include <QtCore/qversiontagging.h> /* 585 */
#include <QtCore/qtypeinfo.h> /* 585 */
#include <QtCore/qtcore-config.h> /* 585 */
#include <QtCore/qsystemdetection.h> /* 585 */
#include <QtCore/qsysinfo.h> /* 585 */
#include <QtCore/qstringview.h> /* 585 */
#include <QtCore/qstringliteral.h> /* 585 */
#include <QtCore/qstringalgorithms.h> /* 585 */
#include <QtCore/qrefcount.h> /* 585 */
#include <QtCore/qprocessordetection.h> /* 585 */
#include <QtCore/qnumeric.h> /* 585 */
#include <QtCore/qnamespace.h> /* 585 */
#include <QtCore/qlogging.h> /* 585 */
#include <QtCore/qglobalstatic.h> /* 585 */
#include <QtCore/qgenericatomic.h> /* 585 */
#include <QtCore/qflags.h> /* 585 */
#include <QtCore/qconfig.h> /* 585 */
#include <QtCore/qcompilerdetection.h> /* 585 */
#include <QtCore/qchar.h> /* 585 */
#include <QtCore/qbytearray.h> /* 585 */
#include <QtCore/qbasicatomic.h> /* 585 */
#include <QtCore/qatomic_cxx11.h> /* 585 */
#include <QtCore/qatomic.h> /* 585 */
#include <QtCore/qarraydata.h> /* 585 */
#include <QtCore/qstringmatcher.h> /* 584 */
#include <QtCore/qiterator.h> /* 584 */
#include <QtCore/qhashfunctions.h> /* 584 */
#include <QtCore/qbytearraylist.h> /* 584 */
#include <QtCore/qalgorithms.h> /* 584 */
#include <QtCore/qvarlengtharray.h> /* 580 */
#include <QtCore/qobjectdefs.h> /* 580 */
#include <QtCore/qcontainerfwd.h> /* 580 */
#include <QtCore/qscopedpointer.h> /* 577 */
#include <QtGui/qcolor.h> /* 574 */
#include <QtCore/qpoint.h> /* 549 */
#include <QtGui/qtransform.h> /* 546 */
#include <QtCore/qmargins.h> /* 544 */
#include <QtCore/qshareddata.h> /* 536 */
#include <QtGui/qimage.h> /* 532 */
#include <QtCore/qsharedpointer.h> /* 532 */
#include <QtGui/qkeysequence.h> /* 531 */
#include <QtGui/qcursor.h> /* 530 */
#include <QtCore/qiodevice.h> /* 530 */
#include <QtGui/qtguiglobal.h> /* 528 */
#include <QtGui/qtgui-config.h> /* 528 */
#include <QtGui/qwindowdefs.h> /* 527 */
#include <QtGui/qregion.h> /* 527 */
#include <QtGui/qrgba64.h> /* 526 */
#include <QtGui/qrgb.h> /* 526 */
#include <QtGui/qpolygon.h> /* 526 */
#include <QtGui/qpixelformat.h> /* 526 */
#include <QtGui/qpainterpath.h> /* 526 */
#include <QtGui/qpaintdevice.h> /* 526 */
#include <QtGui/qmatrix.h> /* 526 */
#include <QtCore/qline.h> /* 526 */
#include <QtCore/qdatastream.h> /* 526 */
#include <QtWidgets/qtwidgetsglobal.h> /* 520 */
#include <QtWidgets/qtwidgets-config.h> /* 520 */
#include <QtGui/qfont.h> /* 507 */
#include <QtGui/qbrush.h> /* 507 */
#include <QtGui/qpalette.h> /* 506 */
#include <QtWidgets/qsizepolicy.h> /* 505 */
#include <QtGui/qfontmetrics.h> /* 497 */
#include <QtGui/qfontinfo.h> /* 496 */

/* cdefs.h is a little bit of a question since it defines RT_STRICT, which someone
   may want to redefine locally, but it's required by all other IPRT VBox includes. */
#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>
#include <iprt/stdint.h>
#include <iprt/assert.h>
#include <iprt/assertcompile.h>
#include <iprt/errcore.h>
#include <iprt/cpp/list.h>
#include <iprt/cpp/meta.h>
#include <VBox/com/array.h>
#include <VBox/com/assert.h>
#include <VBox/com/ptr.h>
#include <VBox/com/com.h>
#include <VBox/com/defs.h>

#include "UILibraryDefs.h"
#include "QIWithRetranslateUI.h"

#if defined(Log) || defined(LogIsEnabled)
# error "Log() from iprt/log.h cannot be defined in the precompiled header!"
#endif

