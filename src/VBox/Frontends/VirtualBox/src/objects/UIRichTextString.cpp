/* $Id$ */
/** @file
 * VBox Qt GUI - UIRichTextString class implementation.
 */

/*
 * Copyright (C) 2015-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QPalette>

/* GUI includes: */
# include "UIRichTextString.h"

/* Other VBox includes: */
# include "iprt/assert.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

const QString UIRichTextString::m_sstrAny = QString("[\\s\\S]*");
const QMap<UIRichTextString::Type, QString> UIRichTextString::m_sPatterns = populatePatterns();
const QMap<UIRichTextString::Type, bool> UIRichTextString::m_sPatternHasMeta = populatePatternHasMeta();

UIRichTextString::UIRichTextString(Type type /* = Type_None */)
    : m_type(type)
    , m_strString(QString())
    , m_strStringMeta(QString())
{
}

UIRichTextString::UIRichTextString(const QString &strString, Type type /* = Type_None */, const QString &strStringMeta /* = QString() */)
    : m_type(type)
    , m_strString(strString)
    , m_strStringMeta(strStringMeta)
{
    //printf("Creating new UIRichTextString with string=\"%s\" and string-meta=\"%s\"\n",
    //       m_strString.toUtf8().constData(), m_strStringMeta.toUtf8().constData());
    parse();
}

UIRichTextString::~UIRichTextString()
{
    /* Erase the map: */
    qDeleteAll(m_strings.begin(), m_strings.end());
    m_strings.clear();
}

QString UIRichTextString::toString() const
{
    /* Add own string first: */
    QString strString = m_strString;

    /* Add all the strings of children finally: */
    foreach (const int &iPosition, m_strings.keys())
        strString.insert(iPosition, m_strings.value(iPosition)->toString());

    /* Return result: */
    return strString;
}

QList<QTextLayout::FormatRange> UIRichTextString::formatRanges(int iShift /* = 0 */) const
{
    /* Prepare format range list: */
    QList<QTextLayout::FormatRange> ranges;

    /* Add own format range first: */
    QTextLayout::FormatRange range;
    range.start = iShift;
    range.length = toString().size();
    range.format = textCharFormat(m_type);
    /* Enable anchor if present: */
    if (!m_strAnchor.isNull())
    {
        range.format.setAnchorHref(m_strAnchor);
        /* Highlight anchor if hovered: */
        if (range.format.anchorHref() == m_strHoveredAnchor)
            range.format.setForeground(qApp->palette().color(QPalette::Link));
    }
    ranges.append(range);

    /* Add all the format ranges of children finally: */
    foreach (const int &iPosition, m_strings.keys())
        ranges.append(m_strings.value(iPosition)->formatRanges(iShift + iPosition));

    /* Return result: */
    return ranges;
}

void UIRichTextString::setHoveredAnchor(const QString &strHoveredAnchor)
{
    /* Define own hovered anchor first: */
    m_strHoveredAnchor = strHoveredAnchor;

    /* Propagate hovered anchor to children finally: */
    foreach (const int &iPosition, m_strings.keys())
        m_strings.value(iPosition)->setHoveredAnchor(m_strHoveredAnchor);
}

void UIRichTextString::parse()
{
    /* Assign the meta to anchor directly for now,
     * will do a separate parsing when there will
     * be more than one type of meta: */
    if (!m_strStringMeta.isNull())
        m_strAnchor = m_strStringMeta;

    /* Parse the passed QString with all the known patterns: */
    foreach (const Type &enmPattern, m_sPatterns.keys())
    {
        /* Get the current pattern: */
        const QString strPattern = m_sPatterns.value(enmPattern);

        /* Recursively parse the string: */
        int iMaxLevel = 0;
        do
        {
            /* Search for the maximum level of the current pattern: */
            iMaxLevel = searchForMaxLevel(m_strString, strPattern, strPattern);
            //printf(" Maximum level for the pattern \"%s\" is %d.\n",
            //       strPattern.toUtf8().constData(), iMaxLevel);
            /* If current pattern of at least level 1 is found: */
            if (iMaxLevel > 0)
            {
                /* Compose full pattern of the corresponding level: */
                const QString strFullPattern = composeFullPattern(strPattern, strPattern, iMaxLevel);
                //printf("  Full pattern: %s\n", strFullPattern.toUtf8().constData());
                QRegExp regExp(strFullPattern);
                regExp.setMinimal(true);
                const int iPosition = regExp.indexIn(m_strString);
                AssertReturnVoid(iPosition != -1);
                if (iPosition != -1)
                {
                    /* Cut the found string: */
                    m_strString.remove(iPosition, regExp.cap(0).size());
                    /* And paste that string as our child: */
                    const bool fPatterHasMeta = m_sPatternHasMeta.value(enmPattern);
                    const QString strSubString = !fPatterHasMeta ? regExp.cap(1) : regExp.cap(2);
                    const QString strSubMeta   = !fPatterHasMeta ? QString()     : regExp.cap(1);
                    m_strings.insert(iPosition, new UIRichTextString(strSubString, enmPattern, strSubMeta));
                }
            }
        }
        while (iMaxLevel > 0);
    }
}

/* static */
QMap<UIRichTextString::Type, QString> UIRichTextString::populatePatterns()
{
    QMap<Type, QString> patterns;
    patterns.insert(Type_Anchor, QString("<a href=([^>]+)>(%1)</a>"));
    patterns.insert(Type_Bold,   QString("<b>(%1)</b>"));
    patterns.insert(Type_Italic, QString("<i>(%1)</i>"));
    return patterns;
}

/* static */
QMap<UIRichTextString::Type, bool> UIRichTextString::populatePatternHasMeta()
{
    QMap<Type, bool> patternHasMeta;
    patternHasMeta.insert(Type_Anchor, true);
    patternHasMeta.insert(Type_Bold,   false);
    patternHasMeta.insert(Type_Italic, false);
    return patternHasMeta;
}

/* static */
int UIRichTextString::searchForMaxLevel(const QString &strString, const QString &strPattern,
                                        const QString &strCurrentPattern, int iCurrentLevel /* = 0 */)
{
    QRegExp regExp(strCurrentPattern.arg(m_sstrAny));
    regExp.setMinimal(true);
    if (regExp.indexIn(strString) != -1)
        return searchForMaxLevel(strString, strPattern,
                                 strCurrentPattern.arg(m_sstrAny + strPattern + m_sstrAny),
                                 iCurrentLevel + 1);
    return iCurrentLevel;
}

/* static */
QString UIRichTextString::composeFullPattern(const QString &strPattern,
                                             const QString &strCurrentPattern, int iCurrentLevel)
{
    if (iCurrentLevel > 1)
        return composeFullPattern(strPattern,
                                  strCurrentPattern.arg(m_sstrAny + strPattern + m_sstrAny),
                                  iCurrentLevel - 1);
    return strCurrentPattern.arg(m_sstrAny);
}

/* static */
QTextCharFormat UIRichTextString::textCharFormat(Type type)
{
    QTextCharFormat format;
    switch (type)
    {
        case Type_Anchor:
        {
            format.setAnchor(true);
            break;
        }
        case Type_Bold:
        {
            QFont font = format.font();
            font.setBold(true);
            format.setFont(font);
            break;
        }
        case Type_Italic:
        {
            QFont font = format.font();
            font.setItalic(true);
            format.setFont(font);
            break;
        }

        case Type_None: break; /* Shut up MSC */
    }
    return format;
}

