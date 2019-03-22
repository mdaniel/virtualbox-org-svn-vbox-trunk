/* $Id$ */
/** @file
 * VBox Qt GUI - UIMediumDetailsWidget class implementation.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
# include <QComboBox>
# include <QLabel>
# include <QPushButton>
# include <QSlider>
# include <QStackedLayout>
# include <QVBoxLayout>

/* GUI includes: */
# include "QIDialogButtonBox.h"
# include "QILabel.h"
# include "QILineEdit.h"
# include "QITabWidget.h"
# include "UIConverter.h"
# include "UIFilePathSelector.h"
# include "UIIconPool.h"
# include "UIMediumDetailsWidget.h"
# include "UIMediumSizeEditor.h"
# include "VBoxGlobal.h"

/* COM includes: */
# include "CSystemProperties.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMediumDetailsWidget::UIMediumDetailsWidget(EmbedTo enmEmbedding, QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI<QWidget>(pParent)
    , m_enmEmbedding(enmEmbedding)
    , m_oldData(UIDataMedium())
    , m_newData(UIDataMedium())
    , m_pTabWidget(0)
    , m_pLabelType(0), m_pComboBoxType(0), m_pErrorPaneType(0)
    , m_pLabelLocation(0), m_pSelectorLocation(0), m_pErrorPaneLocation(0)
    , m_pLabelSize(0), m_pEditorSize(0), m_pErrorPaneSize(0)
    , m_pButtonBox(0)
    , m_pLayoutDetails(0)
{
    /* Prepare: */
    prepare();
}

void UIMediumDetailsWidget::setCurrentType(UIMediumType enmType)
{
    /* If known type was requested => raise corresponding container: */
    if (m_aContainers.contains(enmType))
        m_pLayoutDetails->setCurrentWidget(infoContainer(enmType));
}

void UIMediumDetailsWidget::setData(const UIDataMedium &data)
{
    /* Cache old/new data: */
    m_oldData = data;
    m_newData = m_oldData;

    /* Load options data: */
    loadDataForOptions();
    /* Load details data: */
    loadDataForDetails();
}

void UIMediumDetailsWidget::retranslateUi()
{
    /* Translate tab-widget: */
    m_pTabWidget->setTabText(0, tr("&Attributes"));
    m_pTabWidget->setTabText(1, tr("&Information"));

    /* Translate 'Options' tab content. */

    /* Translate labels: */
    m_pLabelType->setText(tr("&Type:"));
    m_pLabelLocation->setText(tr("&Location:"));
    m_pLabelSize->setText(tr("&Size:"));

    /* Translate fields: */
    m_pComboBoxType->setToolTip(tr("Holds the type of this medium."));
    for (int i = 0; i < m_pComboBoxType->count(); ++i)
        m_pComboBoxType->setItemText(i, gpConverter->toString(m_pComboBoxType->itemData(i).value<KMediumType>()));
    m_pSelectorLocation->setToolTip(tr("Holds the location of this medium."));
    m_pEditorSize->setToolTip(tr("Holds the size of this medium."));

    /* Translate button-box: */
    if (m_pButtonBox)
    {
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Reset"));
        m_pButtonBox->button(QDialogButtonBox::Ok)->setText(tr("Apply"));
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setShortcut(Qt::Key_Escape);
        m_pButtonBox->button(QDialogButtonBox::Ok)->setShortcut(QString("Ctrl+Return"));
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setStatusTip(tr("Reset changes in current medium details"));
        m_pButtonBox->button(QDialogButtonBox::Ok)->setStatusTip(tr("Apply changes in current medium details"));
        m_pButtonBox->button(QDialogButtonBox::Cancel)->
            setToolTip(tr("Reset Changes (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Cancel)->shortcut().toString()));
        m_pButtonBox->button(QDialogButtonBox::Ok)->
            setToolTip(tr("Apply Changes (%1)").arg(m_pButtonBox->button(QDialogButtonBox::Ok)->shortcut().toString()));
    }

    /* Translate 'Details' tab content. */

    /* Retranslate validation: */
    retranslateValidation();
}

void UIMediumDetailsWidget::sltTypeIndexChanged(int iIndex)
{
    m_newData.m_options.m_enmType = m_pComboBoxType->itemData(iIndex).value<KMediumType>();
    revalidate(m_pErrorPaneType);
    updateButtonStates();
}

void UIMediumDetailsWidget::sltLocationPathChanged(const QString &strPath)
{
    m_newData.m_options.m_strLocation = strPath;
    revalidate(m_pErrorPaneLocation);
    updateButtonStates();
}

void UIMediumDetailsWidget::sltSizeValueChanged(qulonglong uSize)
{
    m_newData.m_options.m_uLogicalSize = uSize;
    revalidate(m_pErrorPaneSize);
    updateButtonStates();
}

void UIMediumDetailsWidget::sltHandleButtonBoxClick(QAbstractButton *pButton)
{
    /* Make sure button-box exists: */
    AssertPtrReturnVoid(m_pButtonBox);

    /* Disable buttons first of all: */
    m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
    m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    /* Compare with known buttons: */
    if (pButton == m_pButtonBox->button(QDialogButtonBox::Cancel))
        emit sigDataChangeRejected();
    else
    if (pButton == m_pButtonBox->button(QDialogButtonBox::Ok))
        emit sigDataChangeAccepted();
}

void UIMediumDetailsWidget::prepare()
{
    /* Prepare this: */
    prepareThis();

    /* Apply language settings: */
    retranslateUi();

    /* Update button states finally: */
    updateButtonStates();
}

void UIMediumDetailsWidget::prepareThis()
{
    /* Create layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    AssertPtrReturnVoid(pLayout);
    {
        /* Configure layout: */
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Prepare tab-widget: */
        prepareTabWidget();
    }
}

void UIMediumDetailsWidget::prepareTabWidget()
{
    /* Create tab-widget: */
    m_pTabWidget = new QITabWidget;
    AssertPtrReturnVoid(m_pTabWidget);
    {
        /* Prepare 'Options' tab: */
        prepareTabOptions();
        /* Prepare 'Details' tab: */
        prepareTabDetails();

        /* Add into layout: */
        layout()->addWidget(m_pTabWidget);
    }
}

void UIMediumDetailsWidget::prepareTabOptions()
{
    /* Create 'Options' tab: */
    QWidget *pTabOptions = new QWidget;
    AssertPtrReturnVoid(pTabOptions);
    {
        /* Create 'Options' layout: */
        QGridLayout *pLayoutOptions = new QGridLayout(pTabOptions);
        AssertPtrReturnVoid(pLayoutOptions);
        {
#ifdef VBOX_WS_MAC
            /* Configure layout: */
            pLayoutOptions->setSpacing(10);
            pLayoutOptions->setContentsMargins(10, 10, 10, 10);
#endif

            /* Get the required icon metric: */
            const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);

            /* Create type label: */
            m_pLabelType = new QLabel;
            AssertPtrReturnVoid(m_pLabelType);
            {
                /* Configure label: */
                m_pLabelType->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

                /* Add into layout: */
                pLayoutOptions->addWidget(m_pLabelType, 0, 0);
            }

            /* Create type layout: */
            QHBoxLayout *pLayoutType = new QHBoxLayout;
            AssertPtrReturnVoid(pLayoutType);
            {
                /* Configure layout: */
                pLayoutType->setContentsMargins(0, 0, 0, 0);

                /* Create type editor: */
                m_pComboBoxType = new QComboBox;
                AssertPtrReturnVoid(m_pComboBoxType);
                {
                    /* Configure editor: */
                    m_pLabelType->setBuddy(m_pComboBoxType);
                    m_pComboBoxType->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
                    connect(m_pComboBoxType, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
                            this, &UIMediumDetailsWidget::sltTypeIndexChanged);

                    /* Add into layout: */
                    pLayoutType->addWidget(m_pComboBoxType);
                }

                /* Add stretch: */
                pLayoutType->addStretch();

                /* Create type error pane: */
                m_pErrorPaneType = new QLabel;
                AssertPtrReturnVoid(m_pErrorPaneType);
                {
                    /* Configure label: */
                    m_pErrorPaneType->setAlignment(Qt::AlignCenter);
                    m_pErrorPaneType->setPixmap(UIIconPool::iconSet(":/status_error_16px.png")
                                                .pixmap(QSize(iIconMetric, iIconMetric)));

                    /* Add into layout: */
                    pLayoutType->addWidget(m_pErrorPaneType);
                }

                /* Add into layout: */
                pLayoutOptions->addLayout(pLayoutType, 0, 1);
            }

            /* Create location label: */
            m_pLabelLocation = new QLabel;
            AssertPtrReturnVoid(m_pLabelLocation);
            {
                /* Configure label: */
                m_pLabelLocation->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

                /* Add into layout: */
                pLayoutOptions->addWidget(m_pLabelLocation, 1, 0);
            }

            /* Create location layout: */
            QHBoxLayout *pLayoutLocation = new QHBoxLayout;
            AssertPtrReturnVoid(pLayoutLocation);
            {
                /* Configure layout: */
                pLayoutLocation->setContentsMargins(0, 0, 0, 0);

                /* Create location editor: */
                m_pSelectorLocation = new UIFilePathSelector;
                AssertPtrReturnVoid(m_pSelectorLocation);
                {
                    /* Configure editor: */
                    m_pLabelLocation->setBuddy(m_pSelectorLocation);
                    m_pSelectorLocation->setResetEnabled(false);
                    m_pSelectorLocation->setMode(UIFilePathSelector::Mode_File_Save);
                    m_pSelectorLocation->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
                    connect(m_pSelectorLocation, &UIFilePathSelector::pathChanged,
                            this, &UIMediumDetailsWidget::sltLocationPathChanged);

                    /* Add into layout: */
                    pLayoutLocation->addWidget(m_pSelectorLocation);
                }

                /* Create location error pane: */
                m_pErrorPaneLocation = new QLabel;
                AssertPtrReturnVoid(m_pErrorPaneLocation);
                {
                    /* Configure label: */
                    m_pErrorPaneLocation->setAlignment(Qt::AlignCenter);
                    m_pErrorPaneLocation->setPixmap(UIIconPool::iconSet(":/status_error_16px.png")
                                                    .pixmap(QSize(iIconMetric, iIconMetric)));
                    /* Add into layout: */
                    pLayoutLocation->addWidget(m_pErrorPaneLocation);
                }

                /* Add into layout: */
                pLayoutOptions->addLayout(pLayoutLocation, 1, 1);
            }

            /* Create size label: */
            m_pLabelSize = new QLabel;
            AssertPtrReturnVoid(m_pLabelSize);
            {
                /* Configure label: */
                m_pLabelSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

                /* Add into layout: */
                pLayoutOptions->addWidget(m_pLabelSize, 2, 0);
            }

            /* Create size layout: */
            QGridLayout *pLayoutSize = new QGridLayout;
            AssertPtrReturnVoid(pLayoutSize);
            {
                /* Configure layout: */
                pLayoutSize->setContentsMargins(0, 0, 0, 0);

                /* Create size editor: */
                m_pEditorSize = new UIMediumSizeEditor;
                AssertPtrReturnVoid(m_pEditorSize);
                {
                    /* Configure editor: */
                    m_pLabelSize->setBuddy(m_pEditorSize);
                    m_pEditorSize->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
                    connect(m_pEditorSize, &UIMediumSizeEditor::sigSizeChanged,
                            this, &UIMediumDetailsWidget::sltSizeValueChanged);

                    /* Add into layout: */
                    pLayoutSize->addWidget(m_pEditorSize, 0, 0, 2, 1);
                }

                /* Create size error pane: */
                m_pErrorPaneSize = new QLabel;
                AssertPtrReturnVoid(m_pErrorPaneSize);
                {
                    /* Configure label: */
                    m_pErrorPaneSize->setAlignment(Qt::AlignCenter);
                    m_pErrorPaneSize->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    m_pErrorPaneSize->setPixmap(UIIconPool::iconSet(":/status_error_16px.png")
                                                .pixmap(QSize(iIconMetric, iIconMetric)));

                    /* Add into layout: */
                    pLayoutSize->addWidget(m_pErrorPaneSize, 0, 1, Qt::AlignCenter);
                }

                /* Add into layout: */
                pLayoutOptions->addLayout(pLayoutSize, 2, 1, 2, 1);
            }

            /* Create stretch: */
            QSpacerItem *pSpacer2 = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer2);
            {
                /* Add into layout: */
                pLayoutOptions->addItem(pSpacer2, 4, 0, 1, 2);
            }

            /* If parent embedded into stack: */
            if (m_enmEmbedding == EmbedTo_Stack)
            {
                /* Create button-box: */
                m_pButtonBox = new QIDialogButtonBox;
                AssertPtrReturnVoid(m_pButtonBox);
                /* Configure button-box: */
                m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
                connect(m_pButtonBox, &QIDialogButtonBox::clicked, this, &UIMediumDetailsWidget::sltHandleButtonBoxClick);

                /* Add into layout: */
                pLayoutOptions->addWidget(m_pButtonBox, 5, 0, 1, 2);
            }
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(pTabOptions, QString());
    }
}

void UIMediumDetailsWidget::prepareTabDetails()
{
    /* Create 'Details' tab: */
    QWidget *pTabDetails = new QWidget;
    AssertPtrReturnVoid(pTabDetails);
    {
        /* Create stacked layout: */
        m_pLayoutDetails = new QStackedLayout(pTabDetails);
        AssertPtrReturnVoid(m_pLayoutDetails);
        {
            /* Create information-containers: */
            for (int i = (int)UIMediumType_HardDisk; i < (int)UIMediumType_All; ++i)
            {
                const UIMediumType enmType = (UIMediumType)i;
                prepareInformationContainer(enmType, enmType == UIMediumType_HardDisk ? 6 : 3); /// @todo Remove hard-coded values.
            }
        }

        /* Add to tab-widget: */
        m_pTabWidget->addTab(pTabDetails, QString());
    }
}

void UIMediumDetailsWidget::prepareInformationContainer(UIMediumType enmType, int cFields)
{
    /* Create information-container: */
    m_aContainers[enmType] = new QWidget;
    QWidget *pContainer = infoContainer(enmType);
    AssertPtrReturnVoid(pContainer);
    {
        /* Create layout: */
        new QGridLayout(pContainer);
        QGridLayout *pLayout = qobject_cast<QGridLayout*>(pContainer->layout());
        AssertPtrReturnVoid(pLayout);
        {
            /* Configure layout: */
            pLayout->setVerticalSpacing(0);
            pLayout->setContentsMargins(5, 5, 5, 5);
            pLayout->setColumnStretch(1, 1);

            /* Create labels & fields: */
            int i = 0;
            for (; i < cFields; ++i)
            {
                /* Create label: */
                m_aLabels[enmType] << new QLabel;
                QLabel *pLabel = infoLabel(enmType, i);
                AssertPtrReturnVoid(pLabel);
                {
                    /* Add into layout: */
                    pLayout->addWidget(pLabel, i, 0);
                }

                /* Create field: */
                m_aFields[enmType] << new QILabel;
                QILabel *pField = infoField(enmType, i);
                AssertPtrReturnVoid(pField);
                {
                    /* Configure field: */
                    pField->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed));
                    pField->setFullSizeSelection(true);

                    /* Add into layout: */
                    pLayout->addWidget(pField, i, 1);
                }
            }

            /* Create stretch: */
            QSpacerItem *pSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
            AssertPtrReturnVoid(pSpacer);
            {
                /* Add into layout: */
                pLayout->addItem(pSpacer, i, 0, 1, 2);
            }
        }

        /* Add into layout: */
        m_pLayoutDetails->addWidget(pContainer);
    }
}

void UIMediumDetailsWidget::loadDataForOptions()
{
    /* Clear type combo-box: */
    m_pLabelType->setEnabled(m_newData.m_fValid);
    m_pComboBoxType->setEnabled(m_newData.m_fValid);
    m_pComboBoxType->clear();
    if (m_newData.m_fValid)
    {
        /* Populate type combo-box: */
        switch (m_newData.m_enmType)
        {
            case UIMediumType_DVD:
            case UIMediumType_Floppy:
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_Readonly));
                break;
            case UIMediumType_HardDisk:
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_Normal));
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_Immutable));
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_Writethrough));
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_Shareable));
                m_pComboBoxType->addItem(QString(), QVariant::fromValue(KMediumType_MultiAttach));
                break;
            default:
                break;
        }
        /* Translate type combo-box: */
        for (int i = 0; i < m_pComboBoxType->count(); ++i)
            m_pComboBoxType->setItemText(i, gpConverter->toString(m_pComboBoxType->itemData(i).value<KMediumType>()));
    }

    /* Choose the item with required type to be the current one: */
    for (int i = 0; i < m_pComboBoxType->count(); ++i)
        if (m_pComboBoxType->itemData(i).value<KMediumType>() == m_newData.m_options.m_enmType)
            m_pComboBoxType->setCurrentIndex(i);
    sltTypeIndexChanged(m_pComboBoxType->currentIndex());

    /* Load location: */
    m_pLabelLocation->setEnabled(m_newData.m_fValid);
    m_pSelectorLocation->setEnabled(m_newData.m_fValid);
    m_pSelectorLocation->setPath(m_newData.m_options.m_strLocation);
    sltLocationPathChanged(m_pSelectorLocation->path());

    /* Load size: */
    const bool fEnableResize =    m_newData.m_fValid
                               && m_newData.m_enmType == UIMediumType_HardDisk
                               && !(m_newData.m_enmVariant & KMediumVariant_Fixed);
    m_pLabelSize->setEnabled(fEnableResize);
    m_pEditorSize->setEnabled(fEnableResize);
    m_pEditorSize->setMediumSize(m_newData.m_options.m_uLogicalSize);
}

void UIMediumDetailsWidget::loadDataForDetails()
{
    /* Get information-labels just to acquire their number: */
    const QList<QLabel*> aLabels = m_aLabels.value(m_newData.m_enmType, QList<QLabel*>());
    /* Get information-fields just to acquire their number: */
    const QList<QILabel*> aFields = m_aFields.value(m_newData.m_enmType, QList<QILabel*>());
    /* For each the label => update contents: */
    for (int i = 0; i < aLabels.size(); ++i)
        infoLabel(m_newData.m_enmType, i)->setText(m_newData.m_details.m_aLabels.value(i, QString()));
    /* For each the field => update contents: */
    for (int i = 0; i < aFields.size(); ++i)
    {
        infoField(m_newData.m_enmType, i)->setText(m_newData.m_details.m_aFields.value(i, QString()));
        infoField(m_newData.m_enmType, i)->setEnabled(!infoField(m_newData.m_enmType, i)->text().trimmed().isEmpty());
    }
}

void UIMediumDetailsWidget::revalidate(QWidget *pWidget /* = 0 */)
{
    /* Validate 'Options' tab content: */
    if (!pWidget || pWidget == m_pErrorPaneType)
    {
        /* Always valid for now: */
        const bool fError = false;
        m_pErrorPaneType->setVisible(fError);
    }
    if (!pWidget || pWidget == m_pErrorPaneLocation)
    {
        /* Always valid for now: */
        const bool fError = false;
        m_pErrorPaneLocation->setVisible(fError);
    }
    if (!pWidget || pWidget == m_pErrorPaneSize)
    {
        /* Always valid for now: */
        const bool fError = false;
        m_pErrorPaneSize->setVisible(fError);
    }

    /* Retranslate validation: */
    retranslateValidation(pWidget);
}

void UIMediumDetailsWidget::retranslateValidation(QWidget * /* pWidget = 0 */)
{
    /* Translate 'Interface' tab content: */
//    if (!pWidget || pWidget == m_pErrorPaneType)
//        m_pErrorPaneType->setToolTip(tr("Cannot change from type <b>%1</b> to <b>%2</b>.")
//                                     .arg(m_oldData.m_options.m_enmType).arg(m_newData.m_options.m_enmType));
//    if (!pWidget || pWidget == m_pErrorPaneLocation)
//        m_pErrorPaneLocation->setToolTip(tr("Cannot change medium location from <b>%1</b> to <b>%2</b>.")
//                                         .arg(m_oldData.m_options.m_strLocation).arg(m_newData.m_options.m_strLocation));
//    if (!pWidget || pWidget == m_pErrorPaneSize)
//        m_pErrorPaneSize->setToolTip(tr("Cannot change medium size from <b>%1</b> to <b>%2</b>.")
//                                         .arg(m_oldData.m_options.m_uLogicalSize).arg(m_newData.m_options.m_uLogicalSize));
}

void UIMediumDetailsWidget::updateButtonStates()
{
//    if (m_oldData != m_newData)
//        printf("Type: %d\n",
//               (int)m_newData.m_enmType);

    /* Update 'Apply' / 'Reset' button states: */
    if (m_pButtonBox)
    {
        m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(m_oldData != m_newData);
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_oldData != m_newData);
    }

    /* Notify listeners as well: */
    emit sigDataChanged(m_oldData != m_newData);
}

QWidget *UIMediumDetailsWidget::infoContainer(UIMediumType enmType) const
{
    /* Return information-container for known medium type: */
    return m_aContainers.value(enmType, 0);
}

QLabel *UIMediumDetailsWidget::infoLabel(UIMediumType enmType, int iIndex) const
{
    /* Acquire list of labels: */
    const QList<QLabel*> aLabels = m_aLabels.value(enmType, QList<QLabel*>());

    /* Return label for known index: */
    return aLabels.value(iIndex, 0);
}

QILabel *UIMediumDetailsWidget::infoField(UIMediumType enmType, int iIndex) const
{
    /* Acquire list of fields: */
    const QList<QILabel*> aFields = m_aFields.value(enmType, QList<QILabel*>());

    /* Return label for known index: */
    return aFields.value(iIndex, 0);
}

