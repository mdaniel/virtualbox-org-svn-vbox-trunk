/* $Id$ */
/** @file
 * VBox Qt GUI - UIVMActivityMonitor class implementation.
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

/* Qt includes: */
#include <QApplication>
#include <QDateTime>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QGridLayout>
#include <QScrollArea>
#include <QStyle>
#include <QToolTip>
#include <QXmlStreamReader>
#include <QTimer>

/* GUI includes: */
#include "QIFileDialog.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIIconPool.h"
#include "UIProgressTask.h"
#include "UITranslator.h"
#include "UIVMActivityMonitor.h"
#include "UIVirtualBoxEventHandler.h"

/* COM includes: */
#include "CConsole.h"
#include "CForm.h"
#include "CFormValue.h"
#include "CGuest.h"
#include "CPerformanceCollector.h"
#include "CPerformanceMetric.h"
#include "CStringArray.h"
#include "CRangedIntegerFormValue.h"
#include <iprt/string.h>
#include <VBox/com/VirtualBox.h>

/* External includes: */
#include <math.h>

/** The time in seconds between metric inquries done to API. */
const ULONG g_iPeriod = 1;

/** This is passed to IPerformanceCollector during its setup. When 1 that means IPerformanceCollector object does a data cache of size 1. */
const int g_iMetricSetupCount = 1;
const int g_iDecimalCount = 2;
const int g_iBackgroundTint = 104;
const quint64 uInvalidValueSentinel = ~0U;

/*********************************************************************************************************************************
*   UIProgressTaskReadCloudMachineMetricList definition.                                                                         *
*********************************************************************************************************************************/

class UIProgressTaskReadCloudMachineMetricList : public UIProgressTask
{
    Q_OBJECT;

signals:

    void sigMetricListReceived(QVector<QString> metricNamesList);

public:

    UIProgressTaskReadCloudMachineMetricList(QObject *pParent, CCloudMachine comCloudMachine);

protected:

    virtual CProgress createProgress() RT_OVERRIDE;
    virtual void handleProgressFinished(CProgress &comProgress) RT_OVERRIDE;

private:

    CCloudMachine m_comCloudMachine;
    CStringArray m_metricNamesArray;
};


/*********************************************************************************************************************************
*   UIProgressTaskReadCloudMachineMetricList definition.                                                                         *
*********************************************************************************************************************************/

class UIProgressTaskReadCloudMachineMetricData : public UIProgressTask
{
    Q_OBJECT;

signals:

    void sigMetricDataReceived(KMetricType enmMetricType, QVector<QString> data, QVector<QString> timeStamps);

public:

    UIProgressTaskReadCloudMachineMetricData(QObject *pParent, CCloudMachine comCloudMachine,
                                             KMetricType enmMetricType, ULONG uDataPointsCount);

protected:

    virtual CProgress createProgress() RT_OVERRIDE;
    virtual void handleProgressFinished(CProgress &comProgress) RT_OVERRIDE;

private:

    CCloudMachine m_comCloudMachine;
    CStringArray m_metricData;
    CStringArray m_timeStamps;
    KMetricType m_enmMetricType;
    ULONG m_uDataPointsCount;
};


/*********************************************************************************************************************************
*   UIChart definition.                                                                                                          *
*********************************************************************************************************************************/

class UIChart : public QIWithRetranslateUI<QWidget>
{

    Q_OBJECT;

signals:

    void sigExportMetricsToFile();
    void sigDataIndexUnderCursor(int iIndex);

public:

    UIChart(QWidget *pParent, UIMetric *pMetric, int iMaximumQueueSize);
    void setFontSize(int iFontSize);
    int  fontSize() const;
    const QStringList &textList() const;

    bool isPieChartAllowed() const;
    void setIsPieChartAllowed(bool fWithPieChart);

    bool usePieChart() const;
    void setShowPieChart(bool fShowPieChart);

    bool useGradientLineColor() const;
    void setUseGradientLineColor(bool fUseGradintLineColor);

    bool useAreaChart() const;
    void setUseAreaChart(bool fUseAreaChart);

    bool isAreaChartAllowed() const;
    void setIsAreaChartAllowed(bool fIsAreaChartAllowed);

    QColor dataSeriesColor(int iDataSeriesIndex, int iDark = 0);
    void setDataSeriesColor(int iDataSeriesIndex, const QColor &color);

    QString XAxisLabel();
    void setXAxisLabel(const QString &strLabel);

    bool isAvailable() const;
    void setIsAvailable(bool fIsAvailable);

    void setMouseOver(bool isOver);

    void setDataIndexUnderCursor(int iIndex);

protected:

    virtual void resizeEvent(QResizeEvent *pEvent) RT_OVERRIDE;
    virtual void mouseMoveEvent(QMouseEvent *pEvent) RT_OVERRIDE;
    virtual void paintEvent(QPaintEvent *pEvent) RT_OVERRIDE;
    virtual QSize minimumSizeHint() const RT_OVERRIDE;
    virtual QSize sizeHint() const  RT_OVERRIDE;
    virtual void retranslateUi()  RT_OVERRIDE;
    virtual bool event(QEvent *pEvent) RT_OVERRIDE;

private slots:

    void sltCreateContextMenu(const QPoint &point);
    void sltResetMetric();
    void sltSetShowPieChart(bool fShowPieChart);
    void sltSetUseAreaChart(bool fUseAreaChart);

private:

    /** @name Drawing helper functions.
     * @{ */
       void drawXAxisLabels(QPainter &painter, int iXSubAxisCount);
       void drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex, const QRectF &chartRect, bool fWithBorder = true);
       void drawCombinedPieCharts(QPainter &painter, quint64 iMaximum);
       QString YAxisValueLabel(quint64 iValue) const;
       /** Drawing an overlay rectangle over the charts to indicate that they are disabled. */
       void drawDisabledChartRectangle(QPainter &painter);
       QConicalGradient conicalGradientForDataSeries(const QRectF &rectangle, int iDataIndex);
    /** @} */
    int maxDataSize() const;
    QString toolTipText() const;

    UIMetric *m_pMetric;
    QSize m_size;
    QFont m_axisFont;
    int m_iMarginLeft;
    int m_iMarginRight;
    int m_iMarginTop;
    int m_iMarginBottom;
    int m_iOverlayAlpha;
    QRect m_lineChartRect;
    int m_iPieChartRadius;
    int m_iPieChartSpacing;
    float m_fPixelPerDataPoint;
    /** is set to -1 if mouse cursor is not over a data point*/
    int m_iDataIndexUnderCursor;
    /** For some chart it is not possible to have a pie chart, Then We dont present the
      * option to show it to user. see m_fIsPieChartAllowed. */
    bool m_fIsPieChartAllowed;
    /**  m_fShowPieChart is considered only if m_fIsPieChartAllowed is true. */
    bool m_fShowPieChart;
    bool m_fUseGradientLineColor;
    /** When it is true we draw an area graph where data series drawn on top of each other.
     *  We draw first data0 then data 1 on top. Makes sense where the summation of data is guaranteed not to exceed some max. */
    bool m_fUseAreaChart;
    /** False if the chart is not useable for some reason. For example it depends guest additions and they are not installed. */
    bool m_fIsAvailable;
    /** For some charts it does not make sense to have an area chart. */
    bool m_fIsAreaChartAllowed;
    QColor m_dataSeriesColor[DATA_SERIES_SIZE];
    QString m_strXAxisLabel;
    QString m_strGAWarning;
    QString m_strResetActionLabel;
    QString m_strPieChartToggleActionLabel;
    QString m_strAreaChartToggleActionLabel;
    bool    m_fDrawCurenValueIndicators;
    /** The width of the right margin in characters. */
    int m_iRightMarginCharWidth;
    int m_iMaximumQueueSize;
    QLabel *m_pMouseOverLabel;
};


/*********************************************************************************************************************************
*   UIProgressTaskReadCloudMachineMetricList implementation.                                                                     *
*********************************************************************************************************************************/

UIProgressTaskReadCloudMachineMetricList::UIProgressTaskReadCloudMachineMetricList(QObject *pParent, CCloudMachine comCloudMachine)
    :UIProgressTask(pParent)
    , m_comCloudMachine(comCloudMachine)
{
}

CProgress UIProgressTaskReadCloudMachineMetricList::createProgress()
{
    if (!m_comCloudMachine.isOk())
        return CProgress();
    return m_comCloudMachine.ListMetricNames(m_metricNamesArray);
}

void UIProgressTaskReadCloudMachineMetricList::handleProgressFinished(CProgress &comProgress)
{
    if (!comProgress.isOk())
        return;
    emit sigMetricListReceived(m_metricNamesArray.GetValues());
}


/*********************************************************************************************************************************
*   UIProgressTaskReadCloudMachineMetricData implementation.                                                                     *
*********************************************************************************************************************************/

UIProgressTaskReadCloudMachineMetricData::UIProgressTaskReadCloudMachineMetricData(QObject *pParent,
                                                                                   CCloudMachine comCloudMachine,
                                                                                   KMetricType enmMetricType,
                                                                                   ULONG uDataPointsCount)
    :UIProgressTask(pParent)
    , m_comCloudMachine(comCloudMachine)
    , m_enmMetricType(enmMetricType)
    , m_uDataPointsCount(uDataPointsCount)
{
}

CProgress UIProgressTaskReadCloudMachineMetricData::createProgress()
{
    if (!m_comCloudMachine.isOk())
        return CProgress();

    CStringArray aUnit;
    return m_comCloudMachine.EnumerateMetricData(m_enmMetricType, m_uDataPointsCount, m_metricData, m_timeStamps, aUnit);
}


void UIProgressTaskReadCloudMachineMetricData::handleProgressFinished(CProgress &comProgress)
{
    if (!comProgress.isOk())
        return;
    if (m_metricData.isOk() && m_timeStamps.isOk())
        emit sigMetricDataReceived(m_enmMetricType, m_metricData.GetValues(), m_timeStamps.GetValues());
}

/*********************************************************************************************************************************
*   UIChart implementation.                                                                                     *
*********************************************************************************************************************************/

UIChart::UIChart(QWidget *pParent, UIMetric *pMetric, int iMaximumQueueSize)
    :QIWithRetranslateUI<QWidget>(pParent)
    , m_pMetric(pMetric)
    , m_size(QSize(50, 50))
    , m_iOverlayAlpha(80)
    , m_fPixelPerDataPoint(0.f)
    , m_iDataIndexUnderCursor(-1)
    , m_fIsPieChartAllowed(false)
    , m_fShowPieChart(true)
    , m_fUseGradientLineColor(false)
    , m_fUseAreaChart(true)
    , m_fIsAvailable(true)
    , m_fIsAreaChartAllowed(false)
    , m_fDrawCurenValueIndicators(true)
    , m_iRightMarginCharWidth(10)
    , m_iMaximumQueueSize(iMaximumQueueSize)
    , m_pMouseOverLabel(0)
{
    QPalette tempPal = palette();
    tempPal.setColor(QPalette::Window, tempPal.color(QPalette::Window).lighter(g_iBackgroundTint));
    setPalette(tempPal);
    setAutoFillBackground(true);

    setToolTipDuration(-1);
    m_axisFont = font();
    m_axisFont.setPixelSize(14);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setMouseTracking(true);
    connect(this, &UIChart::customContextMenuRequested,
            this, &UIChart::sltCreateContextMenu);

    m_dataSeriesColor[0] = QColor(200, 0, 0, 255);
    m_dataSeriesColor[1] = QColor(0, 0, 200, 255);

    m_iMarginLeft = 3 * QFontMetricsF(m_axisFont).averageCharWidth();
    m_iMarginRight = m_iRightMarginCharWidth * QFontMetricsF(m_axisFont).averageCharWidth();
    m_iMarginTop = QFontMetrics(m_axisFont).height();
    m_iMarginBottom = QFontMetrics(m_axisFont).height();

    float fAppIconSize = qApp->style()->pixelMetric(QStyle::PM_LargeIconSize);
    m_size = QSize(14 * fAppIconSize,  3.5 * fAppIconSize);
    m_iPieChartSpacing = 2;
    m_iPieChartRadius = m_size.height() - (m_iMarginTop + m_iMarginBottom + 2 * m_iPieChartSpacing);

    m_pMouseOverLabel = new QLabel(this);
    m_pMouseOverLabel->hide();
    m_pMouseOverLabel->setFrameStyle(QFrame::Box);
    m_pMouseOverLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_pMouseOverLabel->setAutoFillBackground(true);
    m_pMouseOverLabel->setMargin(0.1 * QStyle::PM_HeaderMargin);
    retranslateUi();
}

bool UIChart::isPieChartAllowed() const
{
    return m_fIsPieChartAllowed;
}

void UIChart::setIsPieChartAllowed(bool fWithPieChart)
{
    if (m_fIsPieChartAllowed == fWithPieChart)
        return;
    m_fIsPieChartAllowed = fWithPieChart;
    update();
}

bool UIChart::usePieChart() const
{
    return m_fShowPieChart;
}

void UIChart::setShowPieChart(bool fDrawChart)
{
    if (m_fShowPieChart == fDrawChart)
        return;
    m_fShowPieChart = fDrawChart;
    update();
}

bool UIChart::useGradientLineColor() const
{
    return m_fUseGradientLineColor;
}

void UIChart::setUseGradientLineColor(bool fUseGradintLineColor)
{
    if (m_fUseGradientLineColor == fUseGradintLineColor)
        return;
    m_fUseGradientLineColor = fUseGradintLineColor;
    update();
}

bool UIChart::useAreaChart() const
{
    return m_fUseAreaChart;
}

void UIChart::setUseAreaChart(bool fUseAreaChart)
{
    if (m_fUseAreaChart == fUseAreaChart)
        return;
    m_fUseAreaChart = fUseAreaChart;
    update();
}

bool UIChart::isAreaChartAllowed() const
{
    return m_fIsAreaChartAllowed;
}

void UIChart::setIsAreaChartAllowed(bool fIsAreaChartAllowed)
{
    m_fIsAreaChartAllowed = fIsAreaChartAllowed;
}

QColor UIChart::dataSeriesColor(int iDataSeriesIndex, int iDark /* = 0 */)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return QColor();
    return QColor(qMax(m_dataSeriesColor[iDataSeriesIndex].red() - iDark, 0),
                  qMax(m_dataSeriesColor[iDataSeriesIndex].green() - iDark, 0),
                  qMax(m_dataSeriesColor[iDataSeriesIndex].blue() - iDark, 0),
                  m_dataSeriesColor[iDataSeriesIndex].alpha());
}

void UIChart::setDataSeriesColor(int iDataSeriesIndex, const QColor &color)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    if (m_dataSeriesColor[iDataSeriesIndex] == color)
        return;
    m_dataSeriesColor[iDataSeriesIndex] = color;
    update();
}

QString UIChart::XAxisLabel()
{
    return m_strXAxisLabel;
}

void UIChart::setXAxisLabel(const QString &strLabel)
{
    m_strXAxisLabel = strLabel;
}

bool UIChart::isAvailable() const
{
    return m_fIsAvailable;
}

void UIChart::setIsAvailable(bool fIsAvailable)
{
    if (m_fIsAvailable == fIsAvailable)
        return;
    m_fIsAvailable = fIsAvailable;
    update();
}

void UIChart::setMouseOver(bool isOver)
{
    if (!isOver)
    {
        m_iDataIndexUnderCursor = -1;
        emit sigDataIndexUnderCursor(m_iDataIndexUnderCursor);
    }

}

void UIChart::setDataIndexUnderCursor(int iIndex)
{
    m_iDataIndexUnderCursor = iIndex;
    update();
}

QSize UIChart::minimumSizeHint() const
{
    return m_size;
}

QSize UIChart::sizeHint() const
{
    return m_size;
}

void UIChart::retranslateUi()
{
    m_strGAWarning = QApplication::translate("UIVMInformationDialog", "This metric requires guest additions to work.");
    m_strResetActionLabel = QApplication::translate("UIVMInformationDialog", "Reset");
    m_strPieChartToggleActionLabel = QApplication::translate("UIVMInformationDialog", "Show Pie Chart");
    m_strAreaChartToggleActionLabel = QApplication::translate("UIVMInformationDialog", "Draw Area Chart");
    update();
}

bool UIChart::event(QEvent *pEvent)
{
    if (pEvent->type() == QEvent::Leave)
    {
        if (m_pMouseOverLabel)
            m_pMouseOverLabel->setVisible(false);
        m_iDataIndexUnderCursor = -1;
        emit sigDataIndexUnderCursor(m_iDataIndexUnderCursor);
    }
    else if (pEvent->type() == QEvent::ToolTip)
    {
        QHelpEvent *pToolTipEvent = static_cast<QHelpEvent *>(pEvent);
        if (m_pMouseOverLabel)
        {
            if (m_iDataIndexUnderCursor < 0)
                m_pMouseOverLabel->setVisible(false);
            else
            {
                QString strToolTip = toolTipText();
                if (!strToolTip.isEmpty())
                {
                    m_pMouseOverLabel->setText(strToolTip);
                    m_pMouseOverLabel->move(QPoint(pToolTipEvent->pos().x(), pToolTipEvent->pos().y() - m_pMouseOverLabel->height()));
                    m_pMouseOverLabel->setVisible(true);
                }
                else
                    m_pMouseOverLabel->setVisible(false);
            }
        }


    }
    return QIWithRetranslateUI<QWidget>::event(pEvent);
}

void UIChart::resizeEvent(QResizeEvent *pEvent)
{
    int iWidth = width() - m_iMarginLeft - m_iMarginRight;
    if (m_iMaximumQueueSize > 0)
        m_fPixelPerDataPoint = iWidth / (float)m_iMaximumQueueSize;
    QIWithRetranslateUI<QWidget>::resizeEvent(pEvent);
}

void UIChart::mouseMoveEvent(QMouseEvent *pEvent)
{
    const int iX = width() - pEvent->position().x() - m_iMarginRight;
    QPoint eventPosition(pEvent->position().x(), pEvent->position().y());
    int iDataSize = maxDataSize();
    m_iDataIndexUnderCursor = -1;

    if (iDataSize > 0 && m_lineChartRect.contains(eventPosition))
    {
        m_iDataIndexUnderCursor = m_iMaximumQueueSize  - (int)((iX) / m_fPixelPerDataPoint) - 1;
        m_iDataIndexUnderCursor = m_iDataIndexUnderCursor - (m_iMaximumQueueSize - iDataSize);
    }

    emit sigDataIndexUnderCursor(m_iDataIndexUnderCursor);

    update();
    QIWithRetranslateUI<QWidget>::mouseMoveEvent(pEvent);
}

void UIChart::paintEvent(QPaintEvent *pEvent)
{
    Q_UNUSED(pEvent);
    if (!m_pMetric || m_iMaximumQueueSize <= 1)
        return;

    QPainter painter(this);
    painter.setFont(m_axisFont);
    painter.setRenderHint(QPainter::Antialiasing);

    /* Draw a rectanglar grid over which we will draw the line graphs: */
    QPoint chartTopLeft(m_iMarginLeft, m_iMarginTop);
    QSize chartSize(width() - (m_iMarginLeft + m_iMarginRight), height() - (m_iMarginTop + m_iMarginBottom));

    m_lineChartRect = QRect(chartTopLeft, chartSize);
    QColor mainAxisColor(120, 120, 120);
    QColor subAxisColor(200, 200, 200);
    /* Draw the main axes: */
    painter.setPen(mainAxisColor);
    painter.drawRect(m_lineChartRect);

    /* draw Y subaxes: */
    painter.setPen(subAxisColor);
    int iYSubAxisCount = 3;
    for (int i = 0; i < iYSubAxisCount; ++i)
    {
        float fSubAxisY = m_iMarginTop + (i + 1) * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        painter.drawLine(m_lineChartRect.left(), fSubAxisY,
                         m_lineChartRect.right(), fSubAxisY);
    }

    /* draw X subaxes: */
    int iXSubAxisCount = 5;
    for (int i = 0; i < iXSubAxisCount; ++i)
    {
        float fSubAxisX = m_lineChartRect.left() + (i + 1) * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        painter.drawLine(fSubAxisX, m_lineChartRect.top(), fSubAxisX, m_lineChartRect.bottom());
    }

    /* Draw XAxis tick labels: */
    painter.setPen(mainAxisColor);
    drawXAxisLabels(painter, iXSubAxisCount);

    if (!isEnabled())
        return;

    /* Draw a half-transparent rectangle over the whole widget to indicate the it is not available: */
    if (!isAvailable())
    {
        drawDisabledChartRectangle(painter);
        return;
    }

    quint64 iMaximum = m_pMetric->maximum();
    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    /* Draw the data lines: */
    float fBarWidth = m_lineChartRect.width() / (float) (m_iMaximumQueueSize - 1);
    float fH = iMaximum == 0 ? 0 : m_lineChartRect.height() / (float)iMaximum;
    const float fPenWidth = 1.5f;
    const float fPointSize = 3.5f;
    for (int k = 0; k < DATA_SERIES_SIZE; ++k)
    {
        if (m_fUseGradientLineColor)
        {
            QLinearGradient gradient(0, 0, 0, m_lineChartRect.height());
            gradient.setColorAt(0, Qt::black);
            gradient.setColorAt(1, m_dataSeriesColor[k]);
            painter.setPen(QPen(gradient, fPenWidth));
        }
        const QQueue<quint64> *data = m_pMetric->data(k);
        if (!m_fUseGradientLineColor)
            painter.setPen(QPen(m_dataSeriesColor[k], fPenWidth));
        if (m_fUseAreaChart && m_fIsAreaChartAllowed)
        {
            QVector<QPointF> points;
            for (int i = 0; i < data->size(); ++i)
            {
                float fHeight = fH * data->at(i);
                /* Stack 0th data series on top of the 1st data series: */
                if (k == 0)
                {
                    if (m_pMetric->data(1) && m_pMetric->data(1)->size() > i)
                        fHeight += fH * m_pMetric->data(1)->at(i);
                }
                float fX = (width() - m_iMarginRight) - ((data->size() - i - 1) * fBarWidth);
                if (i == 0)
                    points << QPointF(fX, height() - m_iMarginBottom);
                points << QPointF(fX, height() - (fHeight + m_iMarginBottom));
                if (i == data->size() - 1)
                    points << QPointF(fX, height() - + m_iMarginBottom);
            }
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_dataSeriesColor[k]);
            painter.drawPolygon(points, Qt::WindingFill);
        }
        else
        {
            /* Draw lines between data  points: */
            for (int i = 0; i < data->size() - 1; ++i)
            {
                int j = i + 1;
                if (data->at(i) == uInvalidValueSentinel || data->at(j) == uInvalidValueSentinel)
                    continue;

                float fHeight = fH * data->at(i);
                float fX = (width() - m_iMarginRight) - ((data->size() - i - 1) * fBarWidth);
                float fHeight2 = fH * data->at(j);
                float fX2 = (width() - m_iMarginRight) - ((data->size() - j - 1) * fBarWidth);
                QLineF bar(fX, height() - (fHeight + m_iMarginBottom), fX2, height() - (fHeight2 + m_iMarginBottom));
                painter.drawLine(bar);
            }
            /* Draw a point at each data point: */
            painter.setPen(QPen(m_dataSeriesColor[k], fPointSize));
            for (int i = 0; i < data->size(); ++i)
            {
                if (data->at(i) == uInvalidValueSentinel)
                    continue;
                float fHeight = fH * data->at(i);
                float fX = (width() - m_iMarginRight) - ((data->size() - i - 1) * fBarWidth);
                painter.drawPoint(fX, height() - (fHeight + m_iMarginBottom));
            }
        }

        /* Draw a horizontal and vertical line on data point under the mouse cursor
         * and draw the value on the left hand side of the chart: */
        if (m_fDrawCurenValueIndicators && m_iDataIndexUnderCursor >= 0 && m_iDataIndexUnderCursor < data->size())
        {

            painter.setPen(QPen(m_dataSeriesColor[k], 0.5));
            painter.setPen(mainAxisColor);
            float fX = (width() - m_iMarginRight) - ((data->size() - m_iDataIndexUnderCursor - 1) * fBarWidth);
            painter.drawLine(fX, m_iMarginTop, fX, height() - m_iMarginBottom);
        }
    }

    /* Draw YAxis tick labels: */
    painter.setPen(mainAxisColor);
    /* This skips 0 and starts at the 2nd Y axis tick to label: */
    for (int i = iYSubAxisCount; i >= 0; --i)
    {
        /* Draw the bottom most label and skip others when data maximum is 0: */
        if (iMaximum == 0 && i <= iYSubAxisCount)
            break;
        int iTextY = 0.5 * iFontHeight + m_iMarginTop + i * m_lineChartRect.height() / (float) (iYSubAxisCount + 1);
        quint64 iValue = (iYSubAxisCount + 1 - i) * (iMaximum / (float) (iYSubAxisCount + 1));
        QString strValue = YAxisValueLabel(iValue);
        /* Leave space of one character  between the text and chart rectangle: */
        painter.drawText(width() - (m_iRightMarginCharWidth - 1) * QFontMetricsF(m_axisFont).averageCharWidth(), iTextY, strValue);
    }

    if (iMaximum != 0 && m_fIsPieChartAllowed && m_fShowPieChart)
        drawCombinedPieCharts(painter, iMaximum);
}

QString UIChart::YAxisValueLabel(quint64 iValue) const
{
    if (iValue == uInvalidValueSentinel)
        return QString();
    if (m_pMetric->unit().compare("%", Qt::CaseInsensitive) == 0)
        return QString::number(iValue).append("%");
    if (m_pMetric->unit().compare("kb", Qt::CaseInsensitive) == 0)
        return UITranslator::formatSize(_1K * iValue, g_iDecimalCount);
    if (   m_pMetric->unit().compare("b", Qt::CaseInsensitive) == 0
        || m_pMetric->unit().compare("b/s", Qt::CaseInsensitive) == 0)
        return UITranslator::formatSize(iValue, g_iDecimalCount);
    if (m_pMetric->unit().compare("times", Qt::CaseInsensitive) == 0)
        return UITranslator::addMetricSuffixToNumber(iValue);
    if (m_pMetric->unit().compare("gb", Qt::CaseInsensitive) == 0)
        return QString::number(iValue).append(" GB");
    return QString();
}


void UIChart::drawXAxisLabels(QPainter &painter, int iXSubAxisCount)
{
    QFont painterFont = painter.font();
    QFontMetrics fontMetrics(painter.font());
    int iFontHeight = fontMetrics.height();

    int iTotalSeconds = g_iPeriod * m_iMaximumQueueSize;
    for (int i = 0; i < iXSubAxisCount + 2; ++i)
    {
        int iTimeIndex = i * iTotalSeconds / (float)(iXSubAxisCount + 1);

        QString strAxisText;
        if (m_pMetric && m_pMetric->hasDataLabels())
        {
            const QQueue<QString> *labels = m_pMetric->labels();
            int iDataIndex = qMin(labels->size() - 1, iTimeIndex - (m_iMaximumQueueSize - maxDataSize()));
            if (iDataIndex >= 0)
                strAxisText = labels->at(iDataIndex);
        }
        else
            strAxisText = QString::number(iTotalSeconds - iTimeIndex);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        int iTextWidth = fontMetrics.horizontalAdvance(strAxisText);
#else
        int iTextWidth = fontMetrics.width(strAxisText);
#endif
        int iTextX = m_lineChartRect.left() + i * m_lineChartRect.width() / (float) (iXSubAxisCount + 1);
        if (i == 0)
        {
            if (!m_pMetric || !m_pMetric->hasDataLabels())
                strAxisText += " " + m_strXAxisLabel;
            painter.drawText(iTextX, m_lineChartRect.bottom() + iFontHeight, strAxisText);
        }
        else
            painter.drawText(iTextX - 0.5 * iTextWidth, m_lineChartRect.bottom() + iFontHeight, strAxisText);
    }
}

void UIChart::drawPieChart(QPainter &painter, quint64 iMaximum, int iDataIndex,
                           const QRectF &chartRect, bool fWithBorder /* = false */)
{
    if (!m_pMetric)
        return;

    const QQueue<quint64> *data = m_pMetric->data(iDataIndex);
    if (!data || data->isEmpty())
        return;

    /* Draw a whole non-filled circle: */
    if (fWithBorder)
    {
        painter.setPen(QPen(QColor(100, 100, 100, m_iOverlayAlpha), 1));
        painter.drawArc(chartRect, 0, 3600 * 16);
        painter.setPen(Qt::NoPen);
    }

   /* Draw a white filled circle and that the arc for data: */
    QPainterPath background = UIMonitorCommon::wholeArc(chartRect);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, m_iOverlayAlpha));
    painter.drawPath(background);

    float fAngle = 360.f * data->back() / (float)iMaximum;

    QPainterPath dataPath;
    dataPath.moveTo(chartRect.center());
    dataPath.arcTo(chartRect, 90.f/*startAngle*/,
                   -1.f * fAngle /*sweepLength*/);
    painter.setBrush(conicalGradientForDataSeries(chartRect, iDataIndex));
    painter.drawPath(dataPath);
}

QConicalGradient UIChart::conicalGradientForDataSeries(const QRectF &rectangle, int iDataIndex)
{
    QConicalGradient gradient;
    gradient.setCenter(rectangle.center());
    gradient.setAngle(90);
    gradient.setColorAt(0, QColor(0, 0, 0, m_iOverlayAlpha));
    QColor pieColor(m_dataSeriesColor[iDataIndex]);
    pieColor.setAlpha(m_iOverlayAlpha);
    gradient.setColorAt(1, pieColor);
    return gradient;
}

int UIChart::maxDataSize() const
{
    int iSize = 0;
    for (int k = 0; k < DATA_SERIES_SIZE; ++k)
    {
        if (m_pMetric->data(k))
            iSize = qMax(iSize, m_pMetric->data(k)->size());
    }
    return iSize;
}

QString UIChart::toolTipText() const
{
    if (m_iDataIndexUnderCursor < 0)
        return QString();

    if (!m_pMetric->data(0) ||  m_pMetric->data(0)->isEmpty())
        return QString();
    QString strToolTip;
    QString strData0;
    if (m_iDataIndexUnderCursor < m_pMetric->data(0)->size())
        strData0 = YAxisValueLabel(m_pMetric->data(0)->at(m_iDataIndexUnderCursor));
    QString strData1;
    if (m_iDataIndexUnderCursor < m_pMetric->data(1)->size())
        strData1 = YAxisValueLabel(m_pMetric->data(1)->at(m_iDataIndexUnderCursor));
    if (!strData0.isEmpty() && !strData1.isEmpty())
    {
        strToolTip = QString("<font color=\"%1\">%2</font> / <font color=\"%3\">%4</font>")
            .arg(m_dataSeriesColor[0].name(QColor::HexRgb)).arg(strData0)
            .arg(m_dataSeriesColor[1].name(QColor::HexRgb)).arg(strData1);
    }
    else if (!strData0.isEmpty())
    {
        strToolTip = QString("<font color=\"%1\">%2</font>")
            .arg(m_dataSeriesColor[0].name(QColor::HexRgb)).arg(strData0);
    }
    else if (!strData1.isEmpty())
    {
        strToolTip = QString("<font color=\"%1\">%2</font>")
            .arg(m_dataSeriesColor[1].name(QColor::HexRgb)).arg(strData1);
    }
    return strToolTip;
}

void UIChart::drawCombinedPieCharts(QPainter &painter, quint64 iMaximum)
{
    if (!m_pMetric)
        return;

    QRectF chartRect(QPointF(m_iPieChartSpacing + m_iMarginLeft, m_iPieChartSpacing + m_iMarginTop),
                     QSizeF(m_iPieChartRadius, m_iPieChartRadius));

    bool fData0 = m_pMetric->data(0) && !m_pMetric->data(0)->isEmpty();
    bool fData1 = m_pMetric->data(0) && !m_pMetric->data(1)->isEmpty();

    if (fData0 && fData1)
    {
        /* Draw a doughnut chart where data series are stacked on to of each other: */
        if (m_pMetric->data(0) && !m_pMetric->data(0)->isEmpty() &&
            m_pMetric->data(1) && !m_pMetric->data(1)->isEmpty())
            UIMonitorCommon::drawCombinedDoughnutChart(m_pMetric->data(1)->back(), dataSeriesColor(1, 50),
                                                       m_pMetric->data(0)->back(), dataSeriesColor(0, 50),
                                                       painter, iMaximum, chartRect,
                                                       UIMonitorCommon::getScaledRect(chartRect, 0.5f, 0.5f), m_iOverlayAlpha);
#if 0
        /* Draw a doughnut shaped chart and then pie chart inside it: */
        UIMonitorCommon::drawDoughnutChart(painter, iMaximum, m_pMetric->data(0)->back(),
                                           chartRect, innerRect, m_iOverlayAlpha, dataSeriesColor(0));
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, innerRect, false);
#endif
    }
    else if (fData0 && !fData1)
        drawPieChart(painter, iMaximum, 0 /* iDataIndex */, chartRect);
    else if (!fData0 && fData1)
        drawPieChart(painter, iMaximum, 1 /* iDataIndex */, chartRect);
}

void UIChart::drawDisabledChartRectangle(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 150));
    painter.drawRect(m_lineChartRect);
    painter.setPen(QColor(20, 20, 20, 180));
    QFont font = painter.font();
    int iFontSize = 64;
    do {
        font.setPixelSize(iFontSize);
        --iFontSize;
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    } while (QFontMetrics(font).horizontalAdvance(m_strGAWarning) >= 0.8 * m_lineChartRect.width());
#else
    } while (QFontMetrics(font).width(m_strGAWarning) >= 0.8 * m_lineChartRect.width());
#endif
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(m_lineChartRect, m_strGAWarning);
}

void UIChart::sltCreateContextMenu(const QPoint &point)
{
    QMenu menu;
    QAction *pExportAction =
        menu.addAction(QApplication::translate("UIVMInformationDialog", "Export"));
    pExportAction->setIcon(UIIconPool::iconSet(":/performance_monitor_export_16px.png"));
    connect(pExportAction, &QAction::triggered, this, &UIChart::sigExportMetricsToFile);
    menu.addSeparator();
    QAction *pResetAction = menu.addAction(m_strResetActionLabel);
    connect(pResetAction, &QAction::triggered, this, &UIChart::sltResetMetric);
    if (m_fIsPieChartAllowed)
    {
        QAction *pPieChartToggle = menu.addAction(m_strPieChartToggleActionLabel);
        pPieChartToggle->setCheckable(true);
        pPieChartToggle->setChecked(m_fShowPieChart);
        connect(pPieChartToggle, &QAction::toggled, this, &UIChart::sltSetShowPieChart);
    }
    if (m_fIsAreaChartAllowed)
    {
        QAction *pAreaChartToggle = menu.addAction(m_strAreaChartToggleActionLabel);
        pAreaChartToggle->setCheckable(true);
        pAreaChartToggle->setChecked(m_fUseAreaChart);
        connect(pAreaChartToggle, &QAction::toggled, this, &UIChart::sltSetUseAreaChart);
    }

    menu.exec(mapToGlobal(point));
}

void UIChart::sltResetMetric()
{
    if (m_pMetric)
        m_pMetric->reset();
}

void UIChart::sltSetShowPieChart(bool fShowPieChart)
{
    setShowPieChart(fShowPieChart);
}

void UIChart::sltSetUseAreaChart(bool fUseAreaChart)
{
    setUseAreaChart(fUseAreaChart);
}


/*********************************************************************************************************************************
*   UIMetric implementation.                                                                                                     *
*********************************************************************************************************************************/

UIMetric::UIMetric(const QString &strName, const QString &strUnit, int iMaximumQueueSize)
    : m_strName(strName)
    , m_strUnit(strUnit)
    , m_iMaximum(0)
    , m_fRequiresGuestAdditions(false)
    , m_fIsInitialized(false)
    , m_fAutoUpdateMaximum(false)
    , m_iMaximumQueueSize(iMaximumQueueSize)
{
    m_iTotal[0] = 0;
    m_iTotal[1] = 0;
}

UIMetric::UIMetric()
    : m_iMaximum(0)
    , m_fRequiresGuestAdditions(false)
    , m_fIsInitialized(false)
    , m_iMaximumQueueSize(0)
{
}

const QString &UIMetric::name() const
{
    return m_strName;
}

void UIMetric::setMaximum(quint64 iMaximum)
{
    m_iMaximum = iMaximum;
}

quint64 UIMetric::maximum() const
{
    return m_iMaximum;
}

void UIMetric::setUnit(QString strUnit)
{
    m_strUnit = strUnit;

}
const QString &UIMetric::unit() const
{
    return m_strUnit;
}

void UIMetric::addData(int iDataSeriesIndex, quint64 iData)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;

    m_data[iDataSeriesIndex].enqueue(iData);

    /* dequeue if needed and update the maximum value: */
    if (m_data[iDataSeriesIndex].size() > m_iMaximumQueueSize)
        m_data[iDataSeriesIndex].dequeue();

    updateMax();
}

void UIMetric::updateMax()
{
    if (!m_fAutoUpdateMaximum)
        return;
    m_iMaximum = 0;
    for (int k = 0; k < DATA_SERIES_SIZE; ++k)
    {
        for (int i = 0; i < m_data[k].size(); ++i)
        {
            if (m_data[k].at(i) != uInvalidValueSentinel)
                m_iMaximum = qMax(m_iMaximum, m_data[k].at(i));
        }
    }
}

void UIMetric::addData(int iDataSeriesIndex, quint64 iData, const QString &strLabel)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;

    addData(iDataSeriesIndex, iData);
    if (iDataSeriesIndex == 0)
    {
        m_labels.enqueue(strLabel);
        if (m_labels.size() > m_iMaximumQueueSize)
            m_labels.dequeue();
    }
}

const QQueue<quint64> *UIMetric::data(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return &m_data[iDataSeriesIndex];
}

const QQueue<QString> *UIMetric::labels() const
{
    return &m_labels;
}

bool UIMetric::hasDataLabels() const
{
    return !m_labels.isEmpty();
}

int UIMetric::dataSize(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return m_data[iDataSeriesIndex].size();
}

void UIMetric::setDataSeriesName(int iDataSeriesIndex, const QString &strName)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_strDataSeriesName[iDataSeriesIndex] = strName;
}

QString UIMetric::dataSeriesName(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return QString();
    return m_strDataSeriesName[iDataSeriesIndex];
}

void UIMetric::setTotal(int iDataSeriesIndex, quint64 iTotal)
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return;
    m_iTotal[iDataSeriesIndex] = iTotal;
}

quint64 UIMetric::total(int iDataSeriesIndex) const
{
    if (iDataSeriesIndex >= DATA_SERIES_SIZE)
        return 0;
    return m_iTotal[iDataSeriesIndex];
}

bool UIMetric::requiresGuestAdditions() const
{
    return m_fRequiresGuestAdditions;
}

void UIMetric::setRequiresGuestAdditions(bool fRequiresGAs)
{
    m_fRequiresGuestAdditions = fRequiresGAs;
}

bool UIMetric::isInitialized() const
{
    return m_fIsInitialized;
}

void UIMetric::setIsInitialized(bool fIsInitialized)
{
    m_fIsInitialized = fIsInitialized;
}

void UIMetric::reset()
{
    m_fIsInitialized = false;
    for (int i = 0; i < DATA_SERIES_SIZE; ++i)
    {
        m_iTotal[i] = 0;
        m_data[i].clear();
    }
    m_iMaximum = 0;
}

void UIMetric::toFile(QTextStream &stream) const
{
    stream << "Metric Name: " << m_strName << "\n";
    stream << "Unit: " << m_strUnit << "\n";
    stream << "Maximum: " << m_iMaximum << "\n";
    for (int i = 0; i < 2; ++i)
    {
        if (!m_data[i].isEmpty())
        {
            stream << "Data Series: " << m_strDataSeriesName[i] << "\n";
            foreach (const quint64& data, m_data[i])
                stream << data << " ";
            stream << "\n";
        }
    }
    stream << "\n";
}

void UIMetric::setAutoUpdateMaximum(bool fAuto)
{
    m_fAutoUpdateMaximum = fAuto;
}

bool UIMetric::autoUpdateMaximum() const
{
    return m_fAutoUpdateMaximum;
}

/*********************************************************************************************************************************
*   UIVMActivityMonitor implementation.                                                                              *
*********************************************************************************************************************************/

UIVMActivityMonitor::UIVMActivityMonitor(EmbedTo enmEmbedding, QWidget *pParent, int iMaximumQueueSize)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_pTimer(0)
    , m_iTimeStep(0)
    , m_strCPUMetricName("CPU Load")
    , m_strRAMMetricName("RAM Usage")
    , m_strNetworkMetricName("Network")
    , m_strDiskIOMetricName("DiskIO")
    , m_strVMExitMetricName("VMExits")
    , m_iMaximumQueueSize(iMaximumQueueSize)
    , m_pMainLayout(0)
    , m_enmEmbedding(enmEmbedding)
{
    uiCommon().setHelpKeyword(this, "vm-session-information");
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &UIVMActivityMonitor::customContextMenuRequested,
            this, &UIVMActivityMonitor::sltCreateContextMenu);
}

void UIVMActivityMonitor::retranslateUi()
{
    /* Translate the chart info labels: */
    m_iMaximumLabelLength = 0;
    m_strCPUInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "CPU Load");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strCPUInfoLabelTitle.length());
    m_strCPUInfoLabelGuest = QApplication::translate("UIVMInformationDialog", "Guest Load");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strCPUInfoLabelGuest.length());
    m_strCPUInfoLabelVMM = QApplication::translate("UIVMInformationDialog", "VMM Load");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strCPUInfoLabelVMM.length());
    m_strRAMInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "RAM Usage");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strRAMInfoLabelTitle.length());
    m_strRAMInfoLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strRAMInfoLabelTotal.length());
    m_strRAMInfoLabelFree = QApplication::translate("UIVMInformationDialog", "Free");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strRAMInfoLabelFree.length());
    m_strRAMInfoLabelUsed = QApplication::translate("UIVMInformationDialog", "Used");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strRAMInfoLabelUsed.length());
    m_strNetworkInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Network Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strNetworkInfoLabelTitle.length());
    m_strNetworkInfoLabelReceived = QApplication::translate("UIVMInformationDialog", "Receive Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strNetworkInfoLabelReceived.length());
    m_strNetworkInfoLabelTransmitted = QApplication::translate("UIVMInformationDialog", "Transmit Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strNetworkInfoLabelTransmitted.length());
    m_strNetworkInfoLabelReceivedTotal = QApplication::translate("UIVMInformationDialog", "Total Received");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strNetworkInfoLabelReceivedTotal.length());
    m_strNetworkInfoLabelTransmittedTotal = QApplication::translate("UIVMInformationDialog", "Total Transmitted");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strNetworkInfoLabelReceivedTotal.length());
    m_strDiskIOInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "Disk IO Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strDiskIOInfoLabelTitle.length());
    m_strDiskIOInfoLabelWritten = QApplication::translate("UIVMInformationDialog", "Write Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strDiskIOInfoLabelWritten.length());
    m_strDiskIOInfoLabelRead = QApplication::translate("UIVMInformationDialog", "Read Rate");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strDiskIOInfoLabelRead.length());
    m_strDiskIOInfoLabelWrittenTotal = QApplication::translate("UIVMInformationDialog", "Total Written");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strDiskIOInfoLabelWrittenTotal.length());
    m_strDiskIOInfoLabelReadTotal = QApplication::translate("UIVMInformationDialog", "Total Read");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strDiskIOInfoLabelReadTotal.length());
}

// bool UIVMActivityMonitor::eventFilter(QObject *pObj, QEvent *pEvent)
// {
//     if (pEvent-> type() == QEvent::Enter ||
//         pEvent-> type() == QEvent::Leave)
//     {
//         UIChart *pChart = qobject_cast<UIChart*>(pObj);
//         if (pChart)
//             pChart->setMouseOver(pEvent-> type() == QEvent::Enter);
//     }
//     return false;
// }

void UIVMActivityMonitor::prepareWidgets()
{
    m_pMainLayout = new QVBoxLayout(this);
    if (!m_pMainLayout)
        return;

    m_pMainLayout->setContentsMargins(0, 0, 0, 0);
#ifdef VBOX_WS_MAC
    m_pMainLayout->setSpacing(10);
#else
    m_pMainLayout->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 2);
#endif

    m_pTimer = new QTimer(this);
    if (m_pTimer)
        connect(m_pTimer, &QTimer::timeout, this, &UIVMActivityMonitor::sltTimeout);

    QScrollArea *pScrollArea = new QScrollArea(this);
    m_pMainLayout->addWidget(pScrollArea);

    QWidget *pContainerWidget = new QWidget(pScrollArea);
    QGridLayout *pContainerLayout = new QGridLayout(pContainerWidget);
    pContainerWidget->setLayout(pContainerLayout);
    pContainerLayout->setSpacing(10);
    pContainerWidget->show();
    pScrollArea->setWidget(pContainerWidget);
    pScrollArea->setWidgetResizable(true);

    QStringList chartOrder;
    chartOrder << m_strCPUMetricName << m_strRAMMetricName <<
        m_strNetworkMetricName << m_strDiskIOMetricName << m_strVMExitMetricName;
    int iRow = 0;
    foreach (const QString &strMetricName, chartOrder)
    {
        if (!m_metrics.contains(strMetricName))
            continue;

        QHBoxLayout *pChartLayout = new QHBoxLayout;
        pChartLayout->setSpacing(0);

        QLabel *pLabel = new QLabel(this);

        QPalette tempPal = pLabel->palette();
        tempPal.setColor(QPalette::Window, tempPal.color(QPalette::Window).lighter(g_iBackgroundTint));
        pLabel->setPalette(tempPal);
        pLabel->setAutoFillBackground(true);

        pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        pChartLayout->addWidget(pLabel);
        m_infoLabels.insert(strMetricName, pLabel);

        UIChart *pChart = new UIChart(this, &(m_metrics[strMetricName]), m_iMaximumQueueSize);
        connect(pChart, &UIChart::sigExportMetricsToFile,
                this, &UIVMActivityMonitor::sltExportMetricsToFile);
        connect(pChart, &UIChart::sigDataIndexUnderCursor,
                this, &UIVMActivityMonitor::sltChatDataIndexUnderCursorChanged);
        m_charts.insert(strMetricName, pChart);
        pChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        pChartLayout->addWidget(pChart);
        pContainerLayout->addLayout(pChartLayout, iRow, 0, 1, 2);
        ++iRow;
    }

    QWidget *bottomSpacerWidget = new QWidget(this);
    bottomSpacerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    bottomSpacerWidget->setVisible(true);
    pContainerLayout->addWidget(bottomSpacerWidget, iRow, 0, 1, 2);
}

void UIVMActivityMonitor::sltTimeout()
{
    obtainDataAndUpdate();
}

void UIVMActivityMonitor::sltExportMetricsToFile()
{
    QString strStartFileName = QString("%1/%2_%3").
        arg(QFileInfo(defaultMachineFolder()).absolutePath()).
        arg(machineName()).
        arg(QDateTime::currentDateTime().toString("dd-MM-yyyy_hh-mm-ss"));
    QString strFileName = QIFileDialog::getSaveFileName(strStartFileName,"",this,
                                                        QApplication::translate("UIVMInformationDialog",
                                                                                "Export activity data of the machine \"%1\"")
                                                                                .arg(machineName()));
    QFile dataFile(strFileName);
    if (dataFile.open(QFile::WriteOnly | QFile::Truncate))
    {
        QTextStream stream(&dataFile);
        for (QMap<QString, UIMetric>::const_iterator iterator =  m_metrics.begin(); iterator != m_metrics.end(); ++iterator)
            iterator.value().toFile(stream);
        dataFile.close();
    }
}

void UIVMActivityMonitor::sltCreateContextMenu(const QPoint &point)
{
    QMenu menu;
    QAction *pExportAction =
        menu.addAction(QApplication::translate("UIVMInformationDialog", "Export"));
    pExportAction->setIcon(UIIconPool::iconSet(":/performance_monitor_export_16px.png"));
    connect(pExportAction, &QAction::triggered, this, &UIVMActivityMonitor::sltExportMetricsToFile);
    menu.exec(mapToGlobal(point));
}

void UIVMActivityMonitor::sltChatDataIndexUnderCursorChanged(int iIndex)
{
    Q_UNUSED(iIndex);
#if 0
    foreach (UIChart *chart, m_charts)
    {
        if (chart && chart != sender())
        {
            chart->setDataIndexUnderCursor(iIndex);
        }

    }
#endif
}

void UIVMActivityMonitor::prepareActions()
{
}

void UIVMActivityMonitor::resetRAMInfoLabel()
{
    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo = QString("<b>%1</b><br/>%2: %3<br/>%4: %5<br/>%6: %7").
            arg(m_strRAMInfoLabelTitle).arg(m_strRAMInfoLabelTotal).arg("--")
            .arg(m_strRAMInfoLabelFree).arg("--")
            .arg(m_strRAMInfoLabelUsed).arg("--");
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }
}

QString UIVMActivityMonitor::dataColorString(const QString &strChartName, int iDataIndex)
{
    if (!m_charts.contains(strChartName))
        return QColor(Qt::black).name(QColor::HexRgb);
    UIChart *pChart = m_charts[strChartName];
    if (!pChart)
        return QColor(Qt::black).name(QColor::HexRgb);
    return pChart->dataSeriesColor(iDataIndex).name(QColor::HexRgb);
}

void UIVMActivityMonitor::setInfoLabelWidth()
{
    /* Compute the maximum label string length and set it as a fixed width to labels to prevent always changing widths: */
    /* Add m_iDecimalCount plus 4 characters for the number and 3 for unit string: */
    m_iMaximumLabelLength += (g_iDecimalCount + 7);
    if (!m_infoLabels.isEmpty())
    {
        QLabel *pLabel = m_infoLabels.begin().value();
        if (pLabel)
        {
            QFontMetrics labelFontMetric(pLabel->font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
            int iWidth = m_iMaximumLabelLength * labelFontMetric.horizontalAdvance('X');
#else
            int iWidth = m_iMaximumLabelLength * labelFontMetric.width('X');
#endif
            foreach (QLabel *pInfoLabel, m_infoLabels)
                pInfoLabel->setFixedWidth(iWidth);
        }
    }
}

/*********************************************************************************************************************************
*   UIVMActivityMonitorLocal definition.                                                                         *
*********************************************************************************************************************************/

UIVMActivityMonitorLocal::UIVMActivityMonitorLocal(EmbedTo enmEmbedding, QWidget *pParent, const CMachine &machine)
    :UIVMActivityMonitor(enmEmbedding, pParent, 120 /* iMaximumQueueSize */)
    , m_fGuestAdditionsAvailable(false)
{
    prepareMetrics();
    prepareWidgets();
    retranslateUi();
    prepareActions();
    connect(gVBoxEvents, &UIVirtualBoxEventHandler::sigMachineStateChange, this, &UIVMActivityMonitorLocal::sltMachineStateChange);
    connect(&uiCommon(), &UICommon::sigAskToDetachCOM, this, &UIVMActivityMonitorLocal::sltClearCOMData);
    setMachine(machine);

    /* Configure charts: */
    if (m_charts.contains(m_strCPUMetricName) && m_charts[m_strCPUMetricName])
    {
        m_charts[m_strCPUMetricName]->setIsPieChartAllowed(true);
        m_charts[m_strCPUMetricName]->setIsAreaChartAllowed(true);
    }
}

void UIVMActivityMonitorLocal::start()
{
    if (m_comMachine.isNull() || m_comMachine.GetState() != KMachineState_Running)
        return;

    m_fGuestAdditionsAvailable = guestAdditionsAvailable("6.1");
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
    if (m_pTimer)
        m_pTimer->start(1000 * g_iPeriod);
}

UIVMActivityMonitorLocal::~UIVMActivityMonitorLocal()
{
    sltClearCOMData();
}

QUuid UIVMActivityMonitorLocal::machineId() const
{
    if (m_comMachine.isNull())
        return QUuid();
    return m_comMachine.GetId();
}

void UIVMActivityMonitorLocal::retranslateUi()
{
    UIVMActivityMonitor::retranslateUi();

    foreach (UIChart *pChart, m_charts)
        pChart->setXAxisLabel(QApplication::translate("UIVMInformationDialog", "Sec."));

    m_strVMExitInfoLabelTitle = QApplication::translate("UIVMInformationDialog", "VM Exits");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strVMExitInfoLabelTitle.length());
    m_strVMExitLabelCurrent = QApplication::translate("UIVMInformationDialog", "Current");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strVMExitLabelCurrent.length());
    m_strVMExitLabelTotal = QApplication::translate("UIVMInformationDialog", "Total");
    m_iMaximumLabelLength = qMax(m_iMaximumLabelLength, m_strVMExitLabelTotal.length());

    setInfoLabelWidth();
}

void UIVMActivityMonitorLocal::setMachine(const CMachine &comMachine)
{
    reset();
    if (comMachine.isNull())
        return;

    if (!m_comSession.isNull())
        m_comSession.UnlockMachine();

    m_comMachine = comMachine;

    if (m_comMachine.GetState() == KMachineState_Running)
    {
        setEnabled(true);
        openSession();
        start();
    }
}

QString UIVMActivityMonitorLocal::machineName() const
{
    if (m_comMachine.isNull())
        return QString();
    return m_comMachine.GetName();
}

void UIVMActivityMonitorLocal::openSession()
{
    if (!m_comSession.isNull())
        return;
    m_comSession = uiCommon().openSession(m_comMachine.GetId(), KLockType_Shared);
    AssertReturnVoid(!m_comSession.isNull());

    CConsole comConsole = m_comSession.GetConsole();
    AssertReturnVoid(!comConsole.isNull());
    m_comGuest = comConsole.GetGuest();

    m_comMachineDebugger = comConsole.GetDebugger();
}

void UIVMActivityMonitorLocal::obtainDataAndUpdate()
{
    if (m_performanceCollector.isNull())
        return;
    ++m_iTimeStep;

    if (m_metrics.contains(m_strRAMMetricName))
    {
        quint64 iTotalRAM = 0;
        quint64 iFreeRAM = 0;
        UIMonitorCommon::getRAMLoad(m_performanceCollector, m_nameList, m_objectList, iTotalRAM, iFreeRAM);
        updateRAMGraphsAndMetric(iTotalRAM, iFreeRAM);
    }

    /* Update the CPU load chart with values we get from IMachineDebugger::getCPULoad(..): */
    if (m_metrics.contains(m_strCPUMetricName))
    {
        ULONG aPctExecuting;
        ULONG aPctHalted;
        ULONG aPctOther;
        m_comMachineDebugger.GetCPULoad(0x7fffffff, aPctExecuting, aPctHalted, aPctOther);
        updateCPUChart(aPctExecuting, aPctOther);
    }

    /* Update the network load chart with values we find under /Public/NetAdapter/: */
    {
        quint64 cbNetworkTotalReceived = 0;
        quint64 cbNetworkTotalTransmitted = 0;
        UIMonitorCommon::getNetworkLoad(m_comMachineDebugger, cbNetworkTotalReceived, cbNetworkTotalTransmitted);
        updateNetworkChart(cbNetworkTotalReceived, cbNetworkTotalTransmitted);
    }

    /* Update the Disk I/O chart with values we find under /Public/Storage/?/Port?/Bytes*: */
    {
        quint64 cbDiskIOTotalWritten = 0;
        quint64 cbDiskIOTotalRead = 0;
        UIMonitorCommon::getDiskLoad(m_comMachineDebugger, cbDiskIOTotalWritten, cbDiskIOTotalRead);
        updateDiskIOChart(cbDiskIOTotalWritten, cbDiskIOTotalRead);
    }

    /* Update the VM exit chart with values we find as /PROF/CPU?/EM/RecordedExits: */
    {
        quint64 cTotalVMExits = 0;
        UIMonitorCommon::getVMMExitCount(m_comMachineDebugger, cTotalVMExits);
        updateVMExitMetric(cTotalVMExits);
    }
}

void UIVMActivityMonitorLocal::sltMachineStateChange(const QUuid &uId)
{
    if (m_comMachine.isNull())
        return;
    if (m_comMachine.GetId() != uId)
        return;
    if (m_comMachine.GetState() == KMachineState_Running)
    {
        setEnabled(true);
        openSession();
        start();
    }
    else if (m_comMachine.GetState() == KMachineState_Paused)
    {
        /* If we are already active then stop: */
        if (!m_comSession.isNull() && m_pTimer && m_pTimer->isActive())
            m_pTimer->stop();
    }
    else
        reset();
}

QString UIVMActivityMonitorLocal::defaultMachineFolder() const
{
    if (m_comMachine.isOk())
        return m_comMachine.GetSettingsFilePath();
    else
        return QString();
}

void UIVMActivityMonitorLocal::sltGuestAdditionsStateChange()
{
    bool fGuestAdditionsAvailable = guestAdditionsAvailable("6.1");
    if (m_fGuestAdditionsAvailable == fGuestAdditionsAvailable)
        return;
    m_fGuestAdditionsAvailable = fGuestAdditionsAvailable;
    enableDisableGuestAdditionDependedWidgets(m_fGuestAdditionsAvailable);
}

void UIVMActivityMonitorLocal::sltClearCOMData()
{
    if (!m_comSession.isNull())
    {
        m_comSession.UnlockMachine();
        m_comSession.detach();
    }
}

void UIVMActivityMonitorLocal::reset()
{
    m_fGuestAdditionsAvailable = false;
    setEnabled(false);

    if (m_pTimer)
        m_pTimer->stop();
    /* reset the metrics. this will delete their data cache: */
    for (QMap<QString, UIMetric>::iterator iterator =  m_metrics.begin();
         iterator != m_metrics.end(); ++iterator)
        iterator.value().reset();
    /* force update on the charts to draw now emptied metrics' data: */
    for (QMap<QString, UIChart*>::iterator iterator =  m_charts.begin();
         iterator != m_charts.end(); ++iterator)
        iterator.value()->update();
    /* Reset the info labels: */
    resetCPUInfoLabel();
    resetRAMInfoLabel();
    resetNetworkInfoLabel();
    resetDiskIOInfoLabel();
    resetVMExitInfoLabel();
    update();
    sltClearCOMData();
}

void UIVMActivityMonitorLocal::prepareMetrics()
{
    m_performanceCollector = uiCommon().virtualBox().GetPerformanceCollector();
    if (m_performanceCollector.isNull())
        return;

    m_nameList << "Guest/RAM/Usage*";
    m_objectList = QVector<CUnknown>(m_nameList.size(), CUnknown());
    m_performanceCollector.SetupMetrics(m_nameList, m_objectList, g_iPeriod, g_iMetricSetupCount);
    {
        QVector<CPerformanceMetric> metrics = m_performanceCollector.GetMetrics(m_nameList, m_objectList);
        for (int i = 0; i < metrics.size(); ++i)
        {
            QString strName(metrics[i].GetMetricName());
            if (!strName.contains(':'))
            {
                if (strName.contains("RAM", Qt::CaseInsensitive) && strName.contains("Free", Qt::CaseInsensitive))
                {
                    UIMetric ramMetric(m_strRAMMetricName, metrics[i].GetUnit(), m_iMaximumQueueSize);
                    ramMetric.setDataSeriesName(0, "Free");
                    ramMetric.setDataSeriesName(1, "Used");
                    ramMetric.setRequiresGuestAdditions(true);
                    m_metrics.insert(m_strRAMMetricName, ramMetric);
                }
            }
        }
    }

    /* CPU Metric: */
    UIMetric cpuMetric(m_strCPUMetricName, "%", m_iMaximumQueueSize);
    cpuMetric.setDataSeriesName(0, "Guest Load");
    cpuMetric.setDataSeriesName(1, "VMM Load");
    m_metrics.insert(m_strCPUMetricName, cpuMetric);

    /* Network metric: */
    UIMetric networkMetric(m_strNetworkMetricName, "B", m_iMaximumQueueSize);
    networkMetric.setDataSeriesName(0, "Receive Rate");
    networkMetric.setDataSeriesName(1, "Transmit Rate");
    networkMetric.setAutoUpdateMaximum(true);
    m_metrics.insert(m_strNetworkMetricName, networkMetric);

    /* Disk IO metric */
    UIMetric diskIOMetric(m_strDiskIOMetricName, "B", m_iMaximumQueueSize);
    diskIOMetric.setDataSeriesName(0, "Write Rate");
    diskIOMetric.setDataSeriesName(1, "Read Rate");
    diskIOMetric.setAutoUpdateMaximum(true);
    m_metrics.insert(m_strDiskIOMetricName, diskIOMetric);

    /* VM exits metric */
    UIMetric VMExitsMetric(m_strVMExitMetricName, "times", m_iMaximumQueueSize);
    VMExitsMetric.setAutoUpdateMaximum(true);
    m_metrics.insert(m_strVMExitMetricName, VMExitsMetric);
}

bool UIVMActivityMonitorLocal::guestAdditionsAvailable(const char *pszMinimumVersion)
{
    if (m_comGuest.isNull() || !pszMinimumVersion)
        return false;

    /* Guest control stuff is in userland: */
    if (!m_comGuest.GetAdditionsStatus(KAdditionsRunLevelType_Userland))
        return false;

    if (!m_comGuest.isOk())
        return false;

    /* Check the related GA facility: */
    LONG64 iLastUpdatedIgnored;
    if (m_comGuest.GetFacilityStatus(KAdditionsFacilityType_VBoxService, iLastUpdatedIgnored) != KAdditionsFacilityStatus_Active)
        return false;

    if (!m_comGuest.isOk())
        return false;

    QString strGAVersion = m_comGuest.GetAdditionsVersion();
    if (m_comGuest.isOk())
        return (RTStrVersionCompare(strGAVersion.toUtf8().constData(), pszMinimumVersion) >= 0);

    return false;
}

void UIVMActivityMonitorLocal::enableDisableGuestAdditionDependedWidgets(bool fEnable)
{
    for (QMap<QString, UIMetric>::const_iterator iterator =  m_metrics.begin();
         iterator != m_metrics.end(); ++iterator)
    {
        if (!iterator.value().requiresGuestAdditions())
            continue;
        if (m_charts.contains(iterator.key()) && m_charts[iterator.key()])
            m_charts[iterator.key()]->setIsAvailable(fEnable);
        if (m_infoLabels.contains(iterator.key()) && m_infoLabels[iterator.key()])
        {
            m_infoLabels[iterator.key()]->setEnabled(fEnable);
            m_infoLabels[iterator.key()]->update();
        }
    }
}

void UIVMActivityMonitorLocal::updateVMExitMetric(quint64 uTotalVMExits)
{
    if (uTotalVMExits <= 0)
        return;

    UIMetric &VMExitMetric = m_metrics[m_strVMExitMetricName];
    quint64 iRate = uTotalVMExits - VMExitMetric.total(0);
    VMExitMetric.setTotal(0, uTotalVMExits);
    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!VMExitMetric.isInitialized())
    {
        VMExitMetric.setIsInitialized(true);
        return;
    }
    VMExitMetric.addData(0, iRate);
    if (m_infoLabels.contains(m_strVMExitMetricName)  && m_infoLabels[m_strVMExitMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b><br/><font color=\"%2\">%3: %4 %5</font><br/>%6: %7 %8")
            .arg(m_strVMExitInfoLabelTitle)
            .arg(dataColorString(m_strCPUMetricName, 0)).arg(m_strVMExitLabelCurrent).arg(UITranslator::addMetricSuffixToNumber(iRate)).arg(VMExitMetric.unit())
            .arg(m_strVMExitLabelTotal).arg(UITranslator::addMetricSuffixToNumber(uTotalVMExits)).arg(VMExitMetric.unit());
         m_infoLabels[m_strVMExitMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strVMExitMetricName))
        m_charts[m_strVMExitMetricName]->update();
}

void UIVMActivityMonitorLocal::updateCPUChart(quint64 iExecutingPercentage, ULONG iOtherPercentage)
{
    UIMetric &CPUMetric = m_metrics[m_strCPUMetricName];
    CPUMetric.addData(0, iExecutingPercentage);
    CPUMetric.addData(1, iOtherPercentage);
    CPUMetric.setMaximum(100);
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;

        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4%5</font><br/><font color=\"%6\">%7: %8%9</font>")
            .arg(m_strCPUInfoLabelTitle)
            .arg(dataColorString(m_strCPUMetricName, 0))
            .arg(m_strCPUInfoLabelGuest).arg(QString::number(iExecutingPercentage)).arg(CPUMetric.unit())
            .arg(dataColorString(m_strCPUMetricName, 1))
            .arg(m_strCPUInfoLabelVMM).arg(QString::number(iOtherPercentage)).arg(CPUMetric.unit());
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strCPUMetricName))
        m_charts[m_strCPUMetricName]->update();
}

void UIVMActivityMonitorLocal::updateRAMGraphsAndMetric(quint64 iTotalRAM, quint64 iFreeRAM)
{
    UIMetric &RAMMetric = m_metrics[m_strRAMMetricName];
    RAMMetric.setMaximum(iTotalRAM);
    RAMMetric.addData(0, iTotalRAM - iFreeRAM);
    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b><br/>%2: %3<br/><font color=\"%4\">%5: %6</font><br/><font color=\"%7\">%8: %9</font>")
            .arg(m_strRAMInfoLabelTitle)
            .arg(m_strRAMInfoLabelTotal).arg(UITranslator::formatSize(_1K * iTotalRAM, g_iDecimalCount))
            .arg(dataColorString(m_strCPUMetricName, 1)).arg(m_strRAMInfoLabelFree).arg(UITranslator::formatSize(_1K * (iFreeRAM), g_iDecimalCount))
            .arg(dataColorString(m_strCPUMetricName, 0)).arg(m_strRAMInfoLabelUsed).arg(UITranslator::formatSize(_1K * (iTotalRAM - iFreeRAM), g_iDecimalCount));
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strRAMMetricName))
        m_charts[m_strRAMMetricName]->update();
}

void UIVMActivityMonitorLocal::updateNetworkChart(quint64 uReceiveTotal, quint64 uTransmitTotal)
{
    UIMetric &NetMetric = m_metrics[m_strNetworkMetricName];

    quint64 uReceiveRate = uReceiveTotal - NetMetric.total(0);
    quint64 uTransmitRate = uTransmitTotal - NetMetric.total(1);

    NetMetric.setTotal(0, uReceiveTotal);
    NetMetric.setTotal(1, uTransmitTotal);

    if (!NetMetric.isInitialized())
    {
        NetMetric.setIsInitialized(true);
        return;
    }

    NetMetric.addData(0, uReceiveRate);
    NetMetric.addData(1, uTransmitRate);

    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(dataColorString(m_strNetworkMetricName, 0)).arg(m_strNetworkInfoLabelReceived).arg(UITranslator::formatSize(uReceiveRate, g_iDecimalCount))
            .arg(m_strNetworkInfoLabelReceivedTotal).arg(UITranslator::formatSize(uReceiveTotal, g_iDecimalCount))
            .arg(dataColorString(m_strNetworkMetricName, 1)).arg(m_strNetworkInfoLabelTransmitted).arg(UITranslator::formatSize(uTransmitRate, g_iDecimalCount))
            .arg(m_strNetworkInfoLabelTransmittedTotal).arg(UITranslator::formatSize(uTransmitTotal, g_iDecimalCount));
        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strNetworkMetricName))
        m_charts[m_strNetworkMetricName]->update();
}

void UIVMActivityMonitorLocal::updateDiskIOChart(quint64 uDiskIOTotalWritten, quint64 uDiskIOTotalRead)
{
    UIMetric &diskMetric = m_metrics[m_strDiskIOMetricName];

    quint64 uWriteRate = uDiskIOTotalWritten - diskMetric.total(0);
    quint64 uReadRate = uDiskIOTotalRead - diskMetric.total(1);

    diskMetric.setTotal(0, uDiskIOTotalWritten);
    diskMetric.setTotal(1, uDiskIOTotalRead);

    /* Do not set data and maximum if the metric has not been initialized  since we need to initialize totals "(t-1)" first: */
    if (!diskMetric.isInitialized()){
        diskMetric.setIsInitialized(true);
        return;
    }
    diskMetric.addData(0, uWriteRate);
    diskMetric.addData(1, uReadRate);

    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4<br/>%5 %6</font><br/><font color=\"%7\">%8: %9<br/>%10 %11</font>")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(dataColorString(m_strDiskIOMetricName, 0)).arg(m_strDiskIOInfoLabelWritten).arg(UITranslator::formatSize(uWriteRate, g_iDecimalCount))
            .arg(m_strDiskIOInfoLabelWrittenTotal).arg(UITranslator::formatSize((quint64)uDiskIOTotalWritten, g_iDecimalCount))
            .arg(dataColorString(m_strDiskIOMetricName, 1)).arg(m_strDiskIOInfoLabelRead).arg(UITranslator::formatSize(uReadRate, g_iDecimalCount))
            .arg(m_strDiskIOInfoLabelReadTotal).arg(UITranslator::formatSize((quint64)uDiskIOTotalRead, g_iDecimalCount));
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strDiskIOMetricName))
        m_charts[m_strDiskIOMetricName]->update();
}

void UIVMActivityMonitorLocal::resetVMExitInfoLabel()
{
    if (m_infoLabels.contains(m_strVMExitMetricName)  && m_infoLabels[m_strVMExitMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4: %5")
            .arg(m_strVMExitInfoLabelTitle)
            .arg(m_strVMExitLabelCurrent).arg("--")
            .arg(m_strVMExitLabelTotal).arg("--");

        m_infoLabels[m_strVMExitMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorLocal::resetCPUInfoLabel()
{
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo =QString("<b>%1</b></b><br/>%2: %3<br/>%4: %5")
            .arg(m_strCPUInfoLabelTitle)
            .arg(m_strCPUInfoLabelGuest).arg("--")
            .arg(m_strCPUInfoLabelVMM).arg("--");
        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorLocal::resetNetworkInfoLabel()
{
    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4 %5<br/>%6: %7<br/>%8 %9")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(m_strNetworkInfoLabelReceived).arg("--")
            .arg(m_strNetworkInfoLabelReceivedTotal).arg("--")
            .arg(m_strNetworkInfoLabelTransmitted).arg("--")
            .arg(m_strNetworkInfoLabelTransmittedTotal).arg("--");
        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorLocal::resetDiskIOInfoLabel()
{
    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4 %5<br/>%6: %7<br/>%8 %9")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(m_strDiskIOInfoLabelWritten).arg("--")
            .arg(m_strDiskIOInfoLabelWrittenTotal).arg("--")
            .arg(m_strDiskIOInfoLabelRead).arg("--")
            .arg(m_strDiskIOInfoLabelReadTotal).arg("--");
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
}

/*********************************************************************************************************************************
*   UIVMActivityMonitorCloud definition.                                                                         *
*********************************************************************************************************************************/

UIVMActivityMonitorCloud::UIVMActivityMonitorCloud(EmbedTo enmEmbedding, QWidget *pParent, const CCloudMachine &machine)
    :UIVMActivityMonitor(enmEmbedding, pParent, 60 /* iMaximumQueueSize */)
    , m_pMachineStateUpdateTimer(0)
    , m_enmMachineState(KCloudMachineState_Invalid)
{
    m_metricTypeNames[KMetricType_CpuUtilization] = m_strCPUMetricName;
    m_metricTypeNames[KMetricType_MemoryUtilization] = m_strRAMMetricName;
    m_metricTypeNames[KMetricType_DiskBytesRead] = m_strDiskIOMetricName;
    m_metricTypeNames[KMetricType_DiskBytesWritten] = m_strDiskIOMetricName;
    m_metricTypeNames[KMetricType_NetworksBytesIn] = m_strNetworkMetricName;
    m_metricTypeNames[KMetricType_NetworksBytesOut] = m_strNetworkMetricName;

    setMachine(machine);
    determineTotalRAMAmount();

    m_pMachineStateUpdateTimer = new QTimer(this);
    if (m_pMachineStateUpdateTimer)
        connect(m_pMachineStateUpdateTimer, &QTimer::timeout, this, &UIVMActivityMonitorCloud::sltMachineStateUpdateTimeout);

    prepareMetrics();
    prepareWidgets();
    retranslateUi();
    prepareActions();
    resetCPUInfoLabel();
    resetNetworkInfoLabel();
    resetDiskIOInfoLabel();
    resetRAMInfoLabel();

    /* Start the timer: */
    start();
}

void UIVMActivityMonitorCloud::determineTotalRAMAmount()
{
    CForm comForm = m_comMachine.GetDetailsForm();
    /* Ignore cloud machine errors: */
    if (m_comMachine.isOk())
    {
        /* Common anchor for all fields: */
        const QString strAnchorType = "cloud";

        /* For each form value: */
        const QVector<CFormValue> values = comForm.GetValues();
        foreach (const CFormValue &comIteratedValue, values)
        {
            /* Ignore invisible values: */
            if (!comIteratedValue.GetVisible())
                continue;

            /* Acquire label: */
            const QString strLabel = comIteratedValue.GetLabel();
            if (strLabel != "RAM")
                continue;

            AssertReturnVoid((comIteratedValue.GetType() == KFormValueType_RangedInteger));

            CRangedIntegerFormValue comValue(comIteratedValue);
            m_iTotalRAM = comValue.GetInteger();
            QString strRAMUnit = comValue.GetSuffix();
            if (strRAMUnit.compare("gb", Qt::CaseInsensitive) == 0)
                m_iTotalRAM *= _1G / _1K;
            else if (strRAMUnit.compare("mb", Qt::CaseInsensitive) == 0)
                m_iTotalRAM *= _1M / _1K;
            if (!comValue.isOk())
                m_iTotalRAM = 0;
        }
    }
}

void UIVMActivityMonitorCloud::setMachine(const CCloudMachine &comMachine)
{
    m_comMachine = comMachine;
    if (!m_comMachine.isOk())
        return;
    setEnabled(m_comMachine.GetState() == KCloudMachineState_Running);
}

void UIVMActivityMonitorCloud::sltMachineStateUpdateTimeout()
{
    if (!m_comMachine.isOk())
        return;

    KCloudMachineState enmNewState = m_comMachine.GetState();
    /* No changes. Noting to do: */
    if (m_enmMachineState == enmNewState)
        return;

    if (m_ReadListProgressTask)
    {
        disconnect(m_ReadListProgressTask, &UIProgressTaskReadCloudMachineMetricList::sigMetricListReceived,
                   this, &UIVMActivityMonitorCloud::sltMetricNameListingComplete);
        delete m_ReadListProgressTask;
    }

    if (enmNewState == KCloudMachineState_Running)
    {
        m_ReadListProgressTask = new UIProgressTaskReadCloudMachineMetricList(this, m_comMachine);
        if (m_ReadListProgressTask)
        {
            connect(m_ReadListProgressTask, &UIProgressTaskReadCloudMachineMetricList::sigMetricListReceived,
                    this, &UIVMActivityMonitorCloud::sltMetricNameListingComplete);
            m_ReadListProgressTask->start();
        }
        setEnabled(true);
        /* Every minute: */
        if (m_pTimer)
            m_pTimer->start(1000 * 60);
    }
    else
    {
        reset();
        if (m_pTimer)
            m_pTimer->stop();
    }
    m_enmMachineState = enmNewState;
}

void UIVMActivityMonitorCloud::sltMetricNameListingComplete(QVector<QString> metricNameList)
{
    m_availableMetricTypes.clear();
    foreach (const QString &strName, metricNameList)
        m_availableMetricTypes << gpConverter->fromInternalString<KMetricType>(strName);

    if (!m_availableMetricTypes.isEmpty())
        start();

    sender()->deleteLater();
    obtainDataAndUpdate();
}

void UIVMActivityMonitorCloud::sltMetricDataReceived(KMetricType enmMetricType,
                                                     const QVector<QString> &data, const QVector<QString> &timeStamps)
{
    if (data.size() != timeStamps.size())
        return;
    /* Hack alert!! I am told that time series' interval is `guaranteed` to be 1 min. although it is clearly
     * parametrized in OCI API. I would much prefer to have some way of deermining the said interval via our API
     * but it looks like Christmas is over: */
    const int iInterval = 60;
    QVector<QString> newTimeStamps;
    QVector<quint64> newData;
    for (int i = 0; i < timeStamps.size() - 1; ++i)
    {
        if (timeStamps[i].isEmpty())
            continue;
        QTime time = QDateTime::fromString(timeStamps[i], Qt::RFC2822Date).time();
        if (!time.isValid())
            continue;
        newTimeStamps << time.toString("hh:mm");
        /* It looks like in some cases OCI sends us negative values: */
        if (data[i].toFloat() < 0)
            newData << 0U;
        else
            newData << (quint64)data[i].toFloat();

        QTime nextTime = QDateTime::fromString(timeStamps[i + 1], Qt::RFC2822Date).time();
        while(time.secsTo(nextTime) > iInterval)
        {
            time = time.addSecs(iInterval);
            newTimeStamps << time.toString("hh:mm");
            newData << uInvalidValueSentinel;
        }
    }
    if (!data.isEmpty())
    {
        if (!timeStamps.last().isEmpty())
            newTimeStamps << QDateTime::fromString(timeStamps.last(), Qt::RFC2822Date).time().toString("hh:mm");
        newData << (quint64)data.last().toFloat();
    }
    AssertReturnVoid(newData.size() == newTimeStamps.size());

    if (enmMetricType == KMetricType_NetworksBytesIn)
    {
        m_networkReceiveCache.clear();
        m_metrics[m_strNetworkMetricName].reset();
    }
    else if (enmMetricType == KMetricType_NetworksBytesOut)
    {
        m_networkTransmitCache.clear();
        m_metrics[m_strNetworkMetricName].reset();
    }
    else if (enmMetricType == KMetricType_DiskBytesRead)
    {
        m_diskReadCache.clear();
        m_metrics[m_strDiskIOMetricName].reset();
    }
    else if (enmMetricType == KMetricType_DiskBytesWritten)
    {
        m_diskWriteCache.clear();
        m_metrics[m_strDiskIOMetricName].reset();

    }
    else if (enmMetricType == KMetricType_CpuUtilization)
        m_metrics[m_strCPUMetricName].reset();
    else if (enmMetricType == KMetricType_MemoryUtilization)
        m_metrics[m_strRAMMetricName].reset();


    for (int i = 0; i < newData.size(); ++i)
    {
        if (enmMetricType == KMetricType_CpuUtilization)
        {
            updateCPUChart(newData[i], newTimeStamps[i]);
        }
        else if (enmMetricType == KMetricType_NetworksBytesOut)
            cacheNetworkTransmit(newTimeStamps[i], newData[i]);
        else if (enmMetricType == KMetricType_NetworksBytesIn)
            cacheNetworkReceive(newTimeStamps[i], newData[i]);
        else if (enmMetricType == KMetricType_DiskBytesRead)
            cacheDiskRead(newTimeStamps[i], newData[i]);
        else if (enmMetricType == KMetricType_DiskBytesWritten)
            cacheDiskWrite(newTimeStamps[i], newData[i]);
        else if (enmMetricType == KMetricType_MemoryUtilization)
        {
            if (m_iTotalRAM != 0)
            {
                /* calculate used RAM amount in kb: */
                if (newData[i] != uInvalidValueSentinel)
                {
                    quint64 iUsedRAM = newData[i] * (m_iTotalRAM / 100.f);
                    updateRAMChart(iUsedRAM, newTimeStamps[i]);
                }
                else
                    updateRAMChart(newData[i], newTimeStamps[i]);
            }
        }
    }
    sender()->deleteLater();
}

QUuid UIVMActivityMonitorCloud::machineId() const
{
    if (m_comMachine.isOk())
        return m_comMachine.GetId();
    return QUuid();
}

QString UIVMActivityMonitorCloud::machineName() const
{
    if (m_comMachine.isOk())
        return m_comMachine.GetName();
    return QString();
}

void UIVMActivityMonitorCloud::retranslateUi()
{
    UIVMActivityMonitor::retranslateUi();
    foreach (UIChart *pChart, m_charts)
        pChart->setXAxisLabel(QApplication::translate("UIVMInformationDialog", "Min."));
    setInfoLabelWidth();
}

void UIVMActivityMonitorCloud::obtainDataAndUpdate()
{
    foreach (const KMetricType &enmMetricType, m_availableMetricTypes)
    {
        UIMetric metric;
        int iDataSeriesIndex = 0;
        if (!findMetric(enmMetricType, metric, iDataSeriesIndex))
            continue;
        /* Be a paranoid: */
        if (iDataSeriesIndex >= DATA_SERIES_SIZE)
            continue;
#if 0
        int iDataSize = 1;
        if (metric.dataSize(iDataSeriesIndex) == 0)
            iDataSize = 60;
#endif
        /* Request the whole time series (all 60 values) at each iteration to detect time points with no
         * data (due to stop and restart). We sanitize the data when we receive it and mark time points
         * with no data with sentinel value: */
        int iDataSize = 60;
        UIProgressTaskReadCloudMachineMetricData *pTask = new UIProgressTaskReadCloudMachineMetricData(this, m_comMachine,
                                                                                                       enmMetricType, iDataSize);
        connect(pTask, &UIProgressTaskReadCloudMachineMetricData::sigMetricDataReceived,
                this, &UIVMActivityMonitorCloud::sltMetricDataReceived);
        pTask->start();
    }
}

QString UIVMActivityMonitorCloud::defaultMachineFolder() const
{
    /** @todo */
    return QString();
}
void UIVMActivityMonitorCloud::reset()
{
    setEnabled(false);

    if (m_pTimer)
        m_pTimer->stop();
    /* reset the metrics. this will delete their data cache: */
    for (QMap<QString, UIMetric>::iterator iterator =  m_metrics.begin();
         iterator != m_metrics.end(); ++iterator)
        iterator.value().reset();
    /* force update on the charts to draw now emptied metrics' data: */
    for (QMap<QString, UIChart*>::iterator iterator =  m_charts.begin();
         iterator != m_charts.end(); ++iterator)
        iterator.value()->update();
    /* Reset the info labels: */
    resetCPUInfoLabel();
    resetRAMInfoLabel();
    resetNetworkInfoLabel();
    resetDiskIOInfoLabel();

    m_diskWriteCache.clear();
    m_diskReadCache.clear();

    m_networkReceiveCache.clear();
    m_networkTransmitCache.clear();

    update();
    //sltClearCOMData();
}

void UIVMActivityMonitorCloud::start()
{
    sltMachineStateUpdateTimeout();
    if (m_pMachineStateUpdateTimer)
        m_pMachineStateUpdateTimer->start(1000 * 10);
}

void UIVMActivityMonitorCloud::updateCPUChart(quint64 iLoadPercentage, const QString &strLabel)
{
    UIMetric &CPUMetric = m_metrics[m_strCPUMetricName];
    CPUMetric.addData(0, iLoadPercentage, strLabel);
    CPUMetric.setMaximum(100);
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;

        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4%5</font>")
            .arg(m_strCPUInfoLabelTitle)
            .arg(dataColorString(m_strCPUMetricName, 0))
            .arg(m_strCPUInfoLabelGuest).arg(QString::number(iLoadPercentage)).arg(CPUMetric.unit());

        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strCPUMetricName))
        m_charts[m_strCPUMetricName]->update();
}

void UIVMActivityMonitorCloud::updateNetworkChart(quint64 uReceiveRate, quint64 uTransmitRate, const QString &strLabel)
{
    UIMetric &networkMetric = m_metrics[m_strNetworkMetricName];
    networkMetric.addData(0, uReceiveRate, strLabel);
    networkMetric.addData(1, uTransmitRate);

    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b></b><br/><font color=\"%2\">%3: %4</font><br/><font color=\"%5\">%6: %7<br/></font>")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(dataColorString(m_strNetworkMetricName, 0)).arg(m_strNetworkInfoLabelReceived).arg(UITranslator::formatSize(uReceiveRate, g_iDecimalCount))
            .arg(dataColorString(m_strNetworkMetricName, 1)).arg(m_strNetworkInfoLabelTransmitted).arg(UITranslator::formatSize(uTransmitRate, g_iDecimalCount));

        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
    if (m_charts.contains(m_strNetworkMetricName))
        m_charts[m_strNetworkMetricName]->update();
}

void UIVMActivityMonitorCloud::updateDiskIOChart(quint64 uWriteRate, quint64 uReadRate, const QString &strLabel)
{
    UIMetric &diskMetric = m_metrics[m_strDiskIOMetricName];

    diskMetric.addData(0, uWriteRate, strLabel);
    diskMetric.addData(1, uReadRate);

    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/> <font color=\"%2\">%3: %4</font><br/> <font color=\"%5\">%6: %7</font>")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(dataColorString(m_strDiskIOMetricName, 0)).arg(m_strDiskIOInfoLabelWritten).arg(UITranslator::formatSize(uWriteRate, g_iDecimalCount))
            .arg(dataColorString(m_strDiskIOMetricName, 1)).arg(m_strDiskIOInfoLabelRead).arg(UITranslator::formatSize(uReadRate, g_iDecimalCount));

        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strDiskIOMetricName))
        m_charts[m_strDiskIOMetricName]->update();
}

void UIVMActivityMonitorCloud::updateRAMChart(quint64 iUsedRAM, const QString &strLabel)
{
    UIMetric &RAMMetric = m_metrics[m_strRAMMetricName];
    RAMMetric.setMaximum(m_iTotalRAM);
    RAMMetric.addData(0, iUsedRAM, strLabel);

    if (m_infoLabels.contains(m_strRAMMetricName)  && m_infoLabels[m_strRAMMetricName])
    {
        QString strInfo;
        strInfo = QString("<b>%1</b><br/>%2: %3<br/><font color=\"%4\">%5: %6</font><br/><font color=\"%7\">%8: %9</font>")
            .arg(m_strRAMInfoLabelTitle)
            .arg(m_strRAMInfoLabelTotal).arg(UITranslator::formatSize(_1K * m_iTotalRAM, g_iDecimalCount))
            .arg(dataColorString(m_strCPUMetricName, 1)).arg(m_strRAMInfoLabelFree).arg(UITranslator::formatSize(_1K * (m_iTotalRAM - iUsedRAM), g_iDecimalCount))
            .arg(dataColorString(m_strCPUMetricName, 0)).arg(m_strRAMInfoLabelUsed).arg(UITranslator::formatSize(_1K * iUsedRAM, g_iDecimalCount));
        m_infoLabels[m_strRAMMetricName]->setText(strInfo);
    }

    if (m_charts.contains(m_strRAMMetricName))
        m_charts[m_strRAMMetricName]->update();
}

bool UIVMActivityMonitorCloud::findMetric(KMetricType enmMetricType, UIMetric &metric, int &iDataSeriesIndex) const
{
    if (m_metricTypeNames[enmMetricType].isEmpty())
        return false;

    const QString &strMetricName = m_metricTypeNames[enmMetricType];

    if (!m_metrics.contains(strMetricName))
        return false;

    metric = m_metrics[strMetricName];
    iDataSeriesIndex = 0;
    if (enmMetricType == KMetricType_NetworksBytesOut ||
        enmMetricType == KMetricType_DiskBytesRead)
        iDataSeriesIndex = 1;
    return true;
}

void UIVMActivityMonitorCloud::prepareMetrics()
{
    /* RAM Metric: */
    if (m_iTotalRAM != 0)
    {
        UIMetric ramMetric(m_strRAMMetricName, "kb", m_iMaximumQueueSize);
        ramMetric.setDataSeriesName(0, "Used");
        m_metrics.insert(m_strRAMMetricName, ramMetric);
    }

    /* CPU Metric: */
    UIMetric cpuMetric(m_strCPUMetricName, "%", m_iMaximumQueueSize);
    cpuMetric.setDataSeriesName(0, "CPU Utilization");
    m_metrics.insert(m_strCPUMetricName, cpuMetric);

    /* Network metric: */
    UIMetric networkMetric(m_strNetworkMetricName, "B", m_iMaximumQueueSize);
    networkMetric.setDataSeriesName(0, "Receive Rate");
    networkMetric.setDataSeriesName(1, "Transmit Rate");
    networkMetric.setAutoUpdateMaximum(true);
    m_metrics.insert(m_strNetworkMetricName, networkMetric);

    /* Disk IO metric */
    UIMetric diskIOMetric(m_strDiskIOMetricName, "B", m_iMaximumQueueSize);
    diskIOMetric.setDataSeriesName(0, "Write Rate");
    diskIOMetric.setDataSeriesName(1, "Read Rate");
    diskIOMetric.setAutoUpdateMaximum(true);
    m_metrics.insert(m_strDiskIOMetricName, diskIOMetric);

}
void UIVMActivityMonitorCloud::prepareWidgets()
{
    UIVMActivityMonitor::prepareWidgets();
    m_charts[m_strCPUMetricName]->setShowPieChart(false);
}

void UIVMActivityMonitorCloud::resetCPUInfoLabel()
{
    if (m_infoLabels.contains(m_strCPUMetricName)  && m_infoLabels[m_strCPUMetricName])
    {
        QString strInfo;

        strInfo = QString("<b>%1</b></b><br/><font>%2: %3</font>")
            .arg(m_strCPUInfoLabelTitle)
            .arg(m_strCPUInfoLabelGuest).arg("---");

        m_infoLabels[m_strCPUMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorCloud::resetNetworkInfoLabel()
{
    if (m_infoLabels.contains(m_strNetworkMetricName)  && m_infoLabels[m_strNetworkMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4 %5")
            .arg(m_strNetworkInfoLabelTitle)
            .arg(m_strNetworkInfoLabelReceived).arg("--")
            .arg(m_strNetworkInfoLabelTransmitted).arg("--");

        m_infoLabels[m_strNetworkMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorCloud::resetDiskIOInfoLabel()
{
    if (m_infoLabels.contains(m_strDiskIOMetricName)  && m_infoLabels[m_strDiskIOMetricName])
    {
        QString strInfo = QString("<b>%1</b></b><br/>%2: %3<br/>%4: %5")
            .arg(m_strDiskIOInfoLabelTitle)
            .arg(m_strDiskIOInfoLabelWritten).arg("--")
            .arg(m_strDiskIOInfoLabelRead).arg("--");
        m_infoLabels[m_strDiskIOMetricName]->setText(strInfo);
    }
}

void UIVMActivityMonitorCloud::cacheDiskWrite(const QString &strTimeStamp, quint64 iValue)
{
    /* Make sure this is the first time we receieve write for this time stap: */
    AssertReturnVoid(!m_diskWriteCache.contains(strTimeStamp));
    /* If we have read rate for this time stamp just update the chart and remove related data from the cache: */
    if (m_diskReadCache.contains(strTimeStamp))
    {
        updateDiskIOChart((quint64) iValue, (quint64) m_diskReadCache[strTimeStamp], strTimeStamp);
        m_diskReadCache[strTimeStamp];
    }
    else
        m_diskWriteCache[strTimeStamp] = iValue;
}

void UIVMActivityMonitorCloud::cacheDiskRead(const QString &strTimeStamp, quint64 iValue)
{
    /* Make sure this is the first time we receieve read for this time stap: */
    AssertReturnVoid(!m_diskReadCache.contains(strTimeStamp));
    /* If we have write rate for this time stamp just update the chart and remove related data from the cache: */
    if (m_diskWriteCache.contains(strTimeStamp))
    {
        updateDiskIOChart((quint64) m_diskWriteCache[strTimeStamp], (quint64) iValue, strTimeStamp);
        m_diskWriteCache.remove(strTimeStamp);
    }
    else
        m_diskReadCache[strTimeStamp] = iValue;
}

void UIVMActivityMonitorCloud::cacheNetworkReceive(const QString &strTimeStamp, quint64 iValue)
{
    AssertReturnVoid(!m_networkReceiveCache.contains(strTimeStamp));

    if (m_networkTransmitCache.contains(strTimeStamp))
    {
        updateNetworkChart((quint64) iValue, (quint64) m_networkTransmitCache[strTimeStamp], strTimeStamp);
        m_networkTransmitCache.remove(strTimeStamp);
    }
    else
        m_networkReceiveCache[strTimeStamp] = iValue;
}


void UIVMActivityMonitorCloud::cacheNetworkTransmit(const QString &strTimeStamp, quint64 iValue)
{
    AssertReturnVoid(!m_networkTransmitCache.contains(strTimeStamp));

    if (m_networkReceiveCache.contains(strTimeStamp))
    {
        updateNetworkChart((quint64)  m_networkReceiveCache[strTimeStamp], (quint64) iValue, strTimeStamp);
        m_networkReceiveCache.remove(strTimeStamp);
    }
    else
        m_networkTransmitCache[strTimeStamp] = iValue;
}

/* static */
QString UIVMActivityMonitorCloud::formatCloudTimeStamp(const QString &strInput)
{
    if (strInput.isEmpty())
        return QString();
    QDateTime dateTime = QDateTime::fromString(strInput, Qt::RFC2822Date);

    if (!dateTime.isValid())
        return QString();

    return dateTime.time().toString("HH:mm");
}

#include "UIVMActivityMonitor.moc"
