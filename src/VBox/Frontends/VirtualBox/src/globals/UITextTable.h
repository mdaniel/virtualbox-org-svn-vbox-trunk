/* $Id$ */
/** @file
 * VBox Qt GUI - UITextTable class declaration.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_globals_UITextTable_h
#define FEQT_INCLUDED_SRC_globals_UITextTable_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* GUI includes: */
#include "QIGraphicsWidget.h"

/* Forward declarations: */
class QTextLayout;


/** QObject extension used as an
  * accessible wrapper for QString pairs. */
class SHARED_LIBRARY_STUFF UITextTableLine : public QObject
{
    Q_OBJECT;

public:

    /** Constructs text-table line passing @a pParent to the base-class.
      * @param  str1  Brings the 1st table string.
      * @param  str2  Brings the 2nd table string. */
    UITextTableLine(const QString &str1, const QString &str2, QObject *pParent = 0)
        : QObject(pParent)
        , m_str1(str1)
        , m_str2(str2)
    {}

    /** Constructs text-table line on the basis of passed @a other. */
    UITextTableLine(const UITextTableLine &other)
        : QObject(other.parent())
        , m_str1(other.string1())
        , m_str2(other.string2())
    {}

    /** Assigns @a other to this. */
    UITextTableLine &operator=(const UITextTableLine &other)
    {
        setParent(other.parent());
        set1(other.string1());
        set2(other.string2());
        return *this;
    }

    /** Compares @a other to this. */
    bool operator==(const UITextTableLine &other) const
    {
        return string1() == other.string1()
            && string2() == other.string2();
    }

    /** Defines 1st table @a strString. */
    void set1(const QString &strString) { m_str1 = strString; }
    /** Returns 1st table string. */
    const QString &string1() const { return m_str1; }

    /** Defines 2nd table @a strString. */
    void set2(const QString &strString) { m_str2 = strString; }
    /** Returns 2nd table string. */
    const QString &string2() const { return m_str2; }

private:

    /** Holds the 1st table string. */
    QString m_str1;
    /** Holds the 2nd table string. */
    QString m_str2;
};

/** Defines the list of UITextTableLine instances. */
typedef QList<UITextTableLine> UITextTable;
Q_DECLARE_METATYPE(UITextTable);


#endif /* !FEQT_INCLUDED_SRC_globals_UITextTable_h */
