/* $Id$ */
/** @file
 * VBox Qt GUI - UIStorageSettingsEditor class implementation.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
#include <QAction>
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QCommonStyle>
#include <QDrag>
#include <QDragMoveEvent>
#include <QGridLayout>
#include <QItemDelegate>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

/* GUI includes: */
#include "QILabel.h"
#include "QILabelSeparator.h"
#include "QISplitter.h"
#include "QIToolBar.h"
#include "QIToolButton.h"
#include "QITreeView.h"
#include "UICommon.h"
#include "UIConverter.h"
#include "UIExtraDataManager.h"
#include "UIIconPool.h"
#include "UIMedium.h"
#include "UIMediumSelector.h"
#include "UIMessageCenter.h"
#include "UIStorageSettingsEditor.h"

/* COM includes: */
#include "CPlatformProperties.h"
#include "CSystemProperties.h"

/* Defines: */
typedef QList<StorageSlot> SlotsList;
typedef QList<KDeviceType> DeviceTypeList;
typedef QList<KStorageBus> ControllerBusList;
typedef QList<KStorageControllerType> ControllerTypeList;
Q_DECLARE_METATYPE(SlotsList);
Q_DECLARE_METATYPE(DeviceTypeList);
Q_DECLARE_METATYPE(ControllerBusList);
Q_DECLARE_METATYPE(ControllerTypeList);


/** Item states. */
enum ItemState
{
    ItemState_Default,
    ItemState_Collapsed,
    ItemState_Expanded,
    ItemState_Max
};


/** Pixmap types. */
enum PixmapType
{
    PixmapType_Invalid,

    PixmapType_ControllerAddEn,
    PixmapType_ControllerAddDis,
    PixmapType_ControllerDelEn,
    PixmapType_ControllerDelDis,

    PixmapType_AttachmentAddEn,
    PixmapType_AttachmentAddDis,
    PixmapType_AttachmentDelEn,
    PixmapType_AttachmentDelDis,

    PixmapType_IDEControllerNormal,
    PixmapType_IDEControllerExpand,
    PixmapType_IDEControllerCollapse,
    PixmapType_SATAControllerNormal,
    PixmapType_SATAControllerExpand,
    PixmapType_SATAControllerCollapse,
    PixmapType_SCSIControllerNormal,
    PixmapType_SCSIControllerExpand,
    PixmapType_SCSIControllerCollapse,
    PixmapType_SASControllerNormal,
    PixmapType_SASControllerExpand,
    PixmapType_SASControllerCollapse,
    PixmapType_USBControllerNormal,
    PixmapType_USBControllerExpand,
    PixmapType_USBControllerCollapse,
    PixmapType_NVMeControllerNormal,
    PixmapType_NVMeControllerExpand,
    PixmapType_NVMeControllerCollapse,
    PixmapType_VirtioSCSIControllerNormal,
    PixmapType_VirtioSCSIControllerExpand,
    PixmapType_VirtioSCSIControllerCollapse,
    PixmapType_FloppyControllerNormal,
    PixmapType_FloppyControllerExpand,
    PixmapType_FloppyControllerCollapse,

    PixmapType_IDEControllerAddEn,
    PixmapType_IDEControllerAddDis,
    PixmapType_SATAControllerAddEn,
    PixmapType_SATAControllerAddDis,
    PixmapType_SCSIControllerAddEn,
    PixmapType_SCSIControllerAddDis,
    PixmapType_SASControllerAddEn,
    PixmapType_SASControllerAddDis,
    PixmapType_USBControllerAddEn,
    PixmapType_USBControllerAddDis,
    PixmapType_NVMeControllerAddEn,
    PixmapType_NVMeControllerAddDis,
    PixmapType_VirtioSCSIControllerAddEn,
    PixmapType_VirtioSCSIControllerAddDis,
    PixmapType_FloppyControllerAddEn,
    PixmapType_FloppyControllerAddDis,

    PixmapType_HDAttachmentNormal,
    PixmapType_CDAttachmentNormal,
    PixmapType_FDAttachmentNormal,

    PixmapType_HDAttachmentAddEn,
    PixmapType_HDAttachmentAddDis,
    PixmapType_CDAttachmentAddEn,
    PixmapType_CDAttachmentAddDis,
    PixmapType_FDAttachmentAddEn,
    PixmapType_FDAttachmentAddDis,

    PixmapType_ChooseExistingEn,
    PixmapType_ChooseExistingDis,
    PixmapType_CDUnmountEnabled,
    PixmapType_CDUnmountDisabled,
    PixmapType_FDUnmountEnabled,
    PixmapType_FDUnmountDisabled,

    PixmapType_Max
};


/** UIIconPool interface extension for Storage settings editor. */
class UIIconPoolStorageSettings : public UIIconPool
{
public:

    /** Create icon-pool instance. */
    static void create();
    /** Destroy icon-pool instance. */
    static void destroy();

    /** Returns pixmap corresponding to passed @a enmPixmapType. */
    QPixmap pixmap(PixmapType enmPixmapType) const;
    /** Returns icon (probably merged) corresponding to passed @a enmPixmapType and @a enmPixmapDisabledType. */
    QIcon icon(PixmapType enmPixmapType, PixmapType enmPixmapDisabledType = PixmapType_Invalid) const;

private:

    /** Icon-pool constructor. */
    UIIconPoolStorageSettings();
    /** Icon-pool destructor. */
    virtual ~UIIconPoolStorageSettings() RT_OVERRIDE;

    /** Icon-pool instance access method. */
    static UIIconPoolStorageSettings *instance();

    /** Icon-pool instance. */
    static UIIconPoolStorageSettings *s_pInstance;
    /** Icon-pool names cache. */
    QMap<PixmapType, QString>         m_names;
    /** Icon-pool icons cache. */
    mutable QMap<PixmapType, QIcon>   m_icons;

    /** Allows for shortcut access. */
    friend UIIconPoolStorageSettings *iconPool();
};
UIIconPoolStorageSettings *iconPool() { return UIIconPoolStorageSettings::instance(); }


/** QITreeViewItem subclass used as abstract storage tree-view item. */
class AbstractItem : public QITreeViewItem
{
    Q_OBJECT;

public:

    /** Item types. */
    enum ItemType
    {
        Type_InvalidItem    = 0,
        Type_RootItem       = 1,
        Type_ControllerItem = 2,
        Type_AttachmentItem = 3
    };

    /** Constructs top-level item passing @a pParentTree to the base-class. */
    AbstractItem(QITreeView *pParentTree);
    /** Constructs sub-level item passing @a pParentItem to the base-class. */
    AbstractItem(AbstractItem *pParentItem);
    /** Destructs item. */
    virtual ~AbstractItem() RT_OVERRIDE;

    /** Returns parent-item. */
    AbstractItem *parent() const;
    /** Returns ID. */
    QUuid id() const;

    /** Returns machine ID. */
    QUuid machineId() const;
    /** Defines @a uMachineId. */
    void setMachineId(const QUuid &uMachineId);

    /** Returns runtime type information. */
    virtual ItemType rtti() const = 0;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const = 0;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const = 0;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const = 0;

    /** Returns tool-tip information. */
    virtual QString toolTip() const = 0;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState = ItemState_Default) = 0;

protected:

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) = 0;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) = 0;

private:

    /** Holds the parent item reference. */
    AbstractItem *m_pParentItem;
    /** Holds the item ID. */
    QUuid         m_uId;
    /** Holds the item machine ID. */
    QUuid         m_uMachineId;
};
Q_DECLARE_METATYPE(AbstractItem::ItemType);


/** AbstractItem subclass used as root storage tree-view item. */
class RootItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs top-level item passing @a pParentTree to the base-class. */
    RootItem(QITreeView *pParentTree);
    /** Destructs item. */
    virtual ~RootItem() RT_OVERRIDE;

    /** Returns a number of children of certain @a enmBus type. */
    ULONG childCount(KStorageBus enmBus) const;

protected:

    /** Returns runtime type information. */
    virtual ItemType rtti() const RT_OVERRIDE;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const RT_OVERRIDE;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const RT_OVERRIDE;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const RT_OVERRIDE;
    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE;

    /** Returns the item text. */
    virtual QString text() const RT_OVERRIDE;
    /** Returns tool-tip information. */
    virtual QString toolTip() const RT_OVERRIDE;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) RT_OVERRIDE;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) RT_OVERRIDE;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) RT_OVERRIDE;

private:

    /** Holds the list of controller items. */
    QList<AbstractItem*>  m_controllers;
};


/** AbstractItem subclass used as controller storage tree-view item. */
class ControllerItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs sub-level item passing @a pParentItem to the base-class.
      * @param  strName  Brings the name.
      * @param  enmBus   Brings the bus.
      * @param  enmType  Brings the type. */
    ControllerItem(AbstractItem *pParentItem, const QString &strName,
                   KStorageBus enmBus, KStorageControllerType enmType);
    /** Destructs item. */
    virtual ~ControllerItem() RT_OVERRIDE;

    /** Defines current @a strName. */
    void setName(const QString &strName);
    /** Returns current name. */
    QString name() const;

    /** Defines @a enmBus. */
    void setBus(KStorageBus enmBus);
    /** Returns bus. */
    KStorageBus bus() const;
    /** Returns possible buses to switch from current one. */
    ControllerBusList buses() const;

    /** Defines @a enmType. */
    void setType(KStorageControllerType enmType);
    /** Returns type. */
    KStorageControllerType type() const;
    /** Returns possible types for specified @a enmBus to switch from current one. */
    ControllerTypeList types(KStorageBus enmBus) const;

    /** Defines current @a uPortCount. */
    void setPortCount(uint uPortCount);
    /** Returns current port count. */
    uint portCount();
    /** Returns maximum port count. */
    uint maxPortCount();

    /** Defines whether controller @a fUseIoCache. */
    void setUseIoCache(bool fUseIoCache);
    /** Returns whether controller uses IO cache. */
    bool useIoCache() const;

    /** Returns possible controller slots. */
    SlotsList allSlots() const;
    /** Returns used controller slots. */
    SlotsList usedSlots() const;
    /** Returns supported device type list. */
    DeviceTypeList deviceTypeList() const;

    /** Defines a list of @a attachments. */
    void setAttachments(const QList<AbstractItem*> &attachments) { m_attachments = attachments; }
    /** Returns a list of attachments. */
    QList<AbstractItem*> attachments() const { return m_attachments; }
    /** Returns an ID list of attached media of specified @a enmType. */
    QList<QUuid> attachmentIDs(KDeviceType enmType = KDeviceType_Null) const;

private:

    /** Returns runtime type information. */
    virtual ItemType rtti() const RT_OVERRIDE;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const RT_OVERRIDE;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const RT_OVERRIDE;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const RT_OVERRIDE;
    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE;

    /** Returns the item text. */
    virtual QString text() const RT_OVERRIDE;
    /** Returns tool-tip information. */
    virtual QString toolTip() const RT_OVERRIDE;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) RT_OVERRIDE;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) RT_OVERRIDE;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) RT_OVERRIDE;

    /** Updates possible buses. */
    void updateBusInfo();
    /** Updates possible types. */
    void updateTypeInfo();
    /** Updates pixmaps of possible buses. */
    void updatePixmaps();

    /** Holds the current name. */
    QString  m_strName;

    /** Holds the bus. */
    KStorageBus             m_enmBus;
    /** Holds the type. */
    KStorageControllerType  m_enmType;

    /** Holds the possible buses. */
    ControllerBusList                      m_buses;
    /** Holds the possible types on per bus basis. */
    QMap<KStorageBus, ControllerTypeList>  m_types;
    /** Holds the pixmaps of possible buses. */
    QList<PixmapType>                      m_pixmaps;

    /** Holds the current port count. */
    uint  m_uPortCount;
    /** Holds whether controller uses IO cache. */
    bool  m_fUseIoCache;

    /** Holds the list of attachments. */
    QList<AbstractItem*>  m_attachments;
};


/** AbstractItem subclass used as attachment storage tree-view item. */
class AttachmentItem : public AbstractItem
{
    Q_OBJECT;

public:

    /** Constructs sub-level item passing @a pParentItem to the base-class.
      * @param  enmDeviceType  Brings the attachment device type. */
    AttachmentItem(AbstractItem *pParentItem, KDeviceType enmDeviceType);

    /** Defines @a enmDeviceType. */
    void setDeviceType(KDeviceType enmDeviceType);
    /** Returns device type. */
    KDeviceType deviceType() const;
    /** Returns possible device types. */
    DeviceTypeList deviceTypes() const;

    /** Defines storage @a slot. */
    void setStorageSlot(const StorageSlot &slot);
    /** Returns storage slot. */
    StorageSlot storageSlot() const;
    /** Returns possible storage slots. */
    SlotsList storageSlots() const;

    /** Defines @a uMediumId. */
    void setMediumId(const QUuid &uMediumId);
    /** Returns the medium id. */
    QUuid mediumId() const;

    /** Returns whether attachment is a host drive. */
    bool isHostDrive() const;

    /** Defines whether attachment is @a fPassthrough. */
    void setPassthrough(bool fPassthrough);
    /** Returns whether attachment is passthrough. */
    bool isPassthrough() const;

    /** Defines whether attachment is @a fTemporaryEjectable. */
    void setTempEject(bool fTemporaryEjectable);
    /** Returns whether attachment is temporary ejectable. */
    bool isTempEject() const;

    /** Defines whether attachment is @a fNonRotational. */
    void setNonRotational(bool fNonRotational);
    /** Returns whether attachment is non-rotational. */
    bool isNonRotational() const;

    /** Returns whether attachment is @a fIsHotPluggable. */
    void setHotPluggable(bool fIsHotPluggable);
    /** Returns whether attachment is hot-pluggable. */
    bool isHotPluggable() const;

    /** Returns medium size. */
    QString size() const;
    /** Returns logical medium size. */
    QString logicalSize() const;
    /** Returns medium location. */
    QString location() const;
    /** Returns medium format. */
    QString format() const;
    /** Returns medium details. */
    QString details() const;
    /** Returns medium usage. */
    QString usage() const;
    /** Returns medium encryption password ID. */
    QString encryptionPasswordId() const;

private:

    /** Caches medium information. */
    void cache();

    /** Returns runtime type information. */
    virtual ItemType rtti() const RT_OVERRIDE;

    /** Returns child item with specified @a iIndex. */
    virtual AbstractItem *childItem(int iIndex) const RT_OVERRIDE;
    /** Returns child item with specified @a uId. */
    virtual AbstractItem *childItemById(const QUuid &uId) const RT_OVERRIDE;
    /** Returns position of specified child @a pItem. */
    virtual int posOfChild(AbstractItem *pItem) const RT_OVERRIDE;
    /** Returns the number of children. */
    virtual int childCount() const RT_OVERRIDE;

    /** Returns the item text. */
    virtual QString text() const RT_OVERRIDE;
    /** Returns tool-tip information. */
    virtual QString toolTip() const RT_OVERRIDE;
    /** Returns pixmap information for specified @a enmState. */
    virtual QPixmap pixmap(ItemState enmState) RT_OVERRIDE;

    /** Adds a child @a pItem. */
    virtual void addChild(AbstractItem *pItem) RT_OVERRIDE;
    /** Removes the child @a pItem. */
    virtual void delChild(AbstractItem *pItem) RT_OVERRIDE;

    /** Holds the device type. */
    KDeviceType  m_enmDeviceType;
    /** Holds the storage slot. */
    StorageSlot  m_storageSlot;
    /** Holds the medium ID. */
    QUuid        m_uMediumId;
    /** Holds whether attachment is a host drive. */
    bool         m_fHostDrive;
    /** Holds whether attachment is passthrough. */
    bool         m_fPassthrough;
    /** Holds whether attachment is temporary ejectable. */
    bool         m_fTempEject;
    /** Holds whether attachment is non-rotational. */
    bool         m_fNonRotational;
    /** Holds whether attachment is hot-pluggable. */
    bool         m_fHotPluggable;

    /** Holds the name. */
    QString  m_strName;
    /** Holds the tool-tip. */
    QString  m_strTip;
    /** Holds the pixmap. */
    QPixmap  m_strPixmap;

    /** Holds the medium size. */
    QString  m_strSize;
    /** Holds the logical medium size. */
    QString  m_strLogicalSize;
    /** Holds the medium location. */
    QString  m_strLocation;
    /** Holds the medium format. */
    QString  m_strFormat;
    /** Holds the medium details. */
    QString  m_strDetails;
    /** Holds the medium usage. */
    QString  m_strUsage;
    /** Holds the medium encryption password ID. */
    QString  m_strAttEncryptionPasswordID;
};


/** QAbstractItemModel subclass used as complex storage model. */
class StorageModel : public QAbstractItemModel
{
    Q_OBJECT;

public:

    /** Data roles. */
    enum DataRole
    {
        R_ItemId = Qt::UserRole + 1,
        R_ItemPixmap,
        R_ItemPixmapRect,
        R_ItemName,
        R_ItemNamePoint,
        R_ItemType,
        R_IsController,
        R_IsAttachment,

        R_ToolTipType,
        R_IsMoreIDEControllersPossible,
        R_IsMoreSATAControllersPossible,
        R_IsMoreSCSIControllersPossible,
        R_IsMoreFloppyControllersPossible,
        R_IsMoreSASControllersPossible,
        R_IsMoreUSBControllersPossible,
        R_IsMoreNVMeControllersPossible,
        R_IsMoreVirtioSCSIControllersPossible,
        R_IsMoreAttachmentsPossible,

        R_CtrName,
        R_CtrType,
        R_CtrTypesForIDE,
        R_CtrTypesForSATA,
        R_CtrTypesForSCSI,
        R_CtrTypesForFloppy,
        R_CtrTypesForSAS,
        R_CtrTypesForUSB,
        R_CtrTypesForPCIe,
        R_CtrTypesForVirtioSCSI,
        R_CtrDevices,
        R_CtrBusType,
        R_CtrBusTypes,
        R_CtrPortCount,
        R_CtrMaxPortCount,
        R_CtrIoCache,

        R_AttSlot,
        R_AttSlots,
        R_AttDevice,
        R_AttMediumId,
        R_AttIsShowDiffs,
        R_AttIsHostDrive,
        R_AttIsPassthrough,
        R_AttIsTempEject,
        R_AttIsNonRotational,
        R_AttIsHotPluggable,
        R_AttSize,
        R_AttLogicalSize,
        R_AttLocation,
        R_AttFormat,
        R_AttDetails,
        R_AttUsage,
        R_AttEncryptionPasswordID,

        R_Margin,
        R_Spacing,
        R_IconSize,

        R_HDPixmapEn,
        R_CDPixmapEn,
        R_FDPixmapEn,

        R_HDPixmapAddEn,
        R_HDPixmapAddDis,
        R_CDPixmapAddEn,
        R_CDPixmapAddDis,
        R_FDPixmapAddEn,
        R_FDPixmapAddDis,
        R_HDPixmapRect,
        R_CDPixmapRect,
        R_FDPixmapRect
    };

    /** Tool-tip types. */
    enum ToolTipType
    {
        ToolTipType_Default  = 0,
        ToolTipType_Expander = 1,
        ToolTipType_HDAdder  = 2,
        ToolTipType_CDAdder  = 3,
        ToolTipType_FDAdder  = 4
    };

    /** Constructs storage model passing @a pParentTree to the base-class. */
    StorageModel(QITreeView *pParentTree);
    /** Destructs storage model. */
    virtual ~StorageModel() RT_OVERRIDE;

    /** Returns row count for the passed @a parentIndex. */
    int rowCount(const QModelIndex &parentIndex = QModelIndex()) const;
    /** Returns column count for the passed @a parentIndex. */
    int columnCount(const QModelIndex &parentIndex = QModelIndex()) const;

    /** Returns root item. */
    QModelIndex root() const;
    /** Returns item specified by @a iRow, @a iColum and @a parentIndex. */
    QModelIndex index(int iRow, int iColumn, const QModelIndex &parentIndex = QModelIndex()) const;
    /** Returns parent item of @a specifiedIndex item. */
    QModelIndex parent(const QModelIndex &specifiedIndex) const;

    /** Returns model data for @a specifiedIndex and @a iRole. */
    QVariant data(const QModelIndex &specifiedIndex, int iRole) const;
    /** Defines model data for @a specifiedIndex and @a iRole as @a value. */
    bool setData(const QModelIndex &specifiedIndex, const QVariant &value, int iRole);

    /** Adds controller with certain @a strCtrName, @a enmBus and @a enmType. */
    QModelIndex addController(const QString &strCtrName, KStorageBus enmBus, KStorageControllerType enmType);
    /** Deletes controller with certain @a uCtrId. */
    void delController(const QUuid &uCtrId);

    /** Adds attachment with certain @a enmDeviceType and @a uMediumId to controller with certain @a uCtrId. */
    QModelIndex addAttachment(const QUuid &uCtrId, KDeviceType enmDeviceType, const QUuid &uMediumId);
    /** Deletes attachment with certain @a uAttId from controller with certain @a uCtrId. */
    void delAttachment(const QUuid &uCtrId, const QUuid &uAttId);
    /** Moves attachment with certain @a uAttId from controller with certain @a uCtrOldId to one with another @a uCtrNewId. */
    void moveAttachment(const QUuid &uAttId, const QUuid &uCtrOldId, const QUuid &uCtrNewId);

    /** Returns device type of attachment with certain @a uAttId from controller with certain @a uCtrId. */
    KDeviceType attachmentDeviceType(const QUuid &uCtrId, const QUuid &uAttId) const;

    /** Defines @a uMachineId for reference. */
    void setMachineId(const QUuid &uMachineId);

    /** Sorts the contents of model by @a iColumn and @a enmOrder. */
    void sort(int iColumn = 0, Qt::SortOrder enmOrder = Qt::AscendingOrder);
    /** Returns attachment index by specified @a controllerIndex and @a attachmentStorageSlot. */
    QModelIndex attachmentBySlot(QModelIndex controllerIndex, StorageSlot attachmentStorageSlot);

    /** Returns chipset type. */
    KChipsetType chipsetType() const;
    /** Defines @a enmChipsetType. */
    void setChipsetType(KChipsetType enmChipsetType);

    /** Defines @a enmConfigurationAccessLevel. */
    void setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel);

    /** Clears model of all contents. */
    void clear();

    /** Returns current controller types. */
    QMap<KStorageBus, int> currentControllerTypes() const;
    /** Returns maximum controller types. */
    QMap<KStorageBus, int> maximumControllerTypes() const;

    /** Returns bus corresponding to passed enmRole. */
    static KStorageBus roleToBus(StorageModel::DataRole enmRole);
    /** Returns role corresponding to passed enmBus. */
    static StorageModel::DataRole busToRole(KStorageBus enmBus);

private:

    /** Returns model flags for @a specifiedIndex. */
    Qt::ItemFlags flags(const QModelIndex &specifiedIndex) const;

    /** Holds the root item instance. */
    AbstractItem *m_pRootItem;

    /** Holds the enabled plus pixmap instance. */
    QPixmap  m_pixmapPlusEn;
    /** Holds the disabled plus pixmap instance. */
    QPixmap  m_pixmapPlusDis;
    /** Holds the enabled minus pixmap instance. */
    QPixmap  m_pixmapMinusEn;
    /** Holds the disabled minus pixmap instance. */
    QPixmap  m_pixmapMinusDis;

    /** Holds the tool-tip type. */
    ToolTipType  m_enmToolTipType;

    /** Holds the chipset type. */
    KChipsetType  m_enmChipsetType;

    /** Holds configuration access level. */
    ConfigurationAccessLevel  m_enmConfigurationAccessLevel;
};
Q_DECLARE_METATYPE(StorageModel::ToolTipType);


/** QItemDelegate subclass used as storage table item delegate. */
class StorageDelegate : public QItemDelegate
{
    Q_OBJECT;

public:

    /** Constructs storage delegate passing @a pParent to the base-class. */
    StorageDelegate(QObject *pParent);

private:

    /** Paints @a index item with specified @a option using specified @a pPainter. */
    void paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};


/** QObject subclass used as UI medium ID holder.
  * Used for compliance with other storage page widgets
  * which caching and holding corresponding information. */
class UIMediumIDHolder : public QObject
{
    Q_OBJECT;

signals:

    /** Notify about medium ID changed. */
    void sigChanged();

public:

    /** Constructs medium ID holder passing @a pParent to the base-class. */
    UIMediumIDHolder(QWidget *pParent) : QObject(pParent) {}

    /** Defines medium @a uId. */
    void setId(const QUuid &uId) { m_uId = uId; emit sigChanged(); }
    /** Returns medium ID. */
    QUuid id() const { return m_uId; }

    /** Defines medium device @a enmType. */
    void setType(UIMediumDeviceType enmType) { m_enmType = enmType; }
    /** Returns medium device type. */
    UIMediumDeviceType type() const { return m_enmType; }

    /** Returns whether medium ID is null. */
    bool isNull() const { return m_uId == UIMedium().id(); }

private:

    /** Holds the medium ID. */
    QUuid               m_uId;
    /** Holds the medium device type. */
    UIMediumDeviceType  m_enmType;
};


/** QWidget sub-class
  * with manual size-hint recalculation.
  * Tries to take into account size-hint even for hidden items.
  * Assumes QGridLayout is present as this widget is used purely
  * for controller/attachment pane. */
class UIWidgetOfMaximumSize : public QWidget
{
    Q_OBJECT;

public:

    /** Constructs widget passing @a pParent to base-class. */
    UIWidgetOfMaximumSize(QWidget *pParent = 0);

    /** Returns the minimum widget size. */
    virtual QSize minimumSizeHint() const RT_OVERRIDE;
};


/*********************************************************************************************************************************
*   Class UIIconPoolStorageSettings implementation.                                                                              *
*********************************************************************************************************************************/

/* static */
UIIconPoolStorageSettings *UIIconPoolStorageSettings::s_pInstance = 0;
UIIconPoolStorageSettings *UIIconPoolStorageSettings::instance() { return s_pInstance; }
void UIIconPoolStorageSettings::create() { new UIIconPoolStorageSettings; }
void UIIconPoolStorageSettings::destroy() { delete s_pInstance; }

QPixmap UIIconPoolStorageSettings::pixmap(PixmapType enmPixmapType) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(enmPixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(enmPixmapType))
            m_icons[enmPixmapType] = iconSet(m_names[enmPixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[enmPixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve corresponding icon: */
    const QIcon &icon = m_icons[enmPixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapType),
                    nullPixmap);

    /* Retrieve available sizes for that icon: */
    const QList<QSize> availableSizes = icon.availableSizes();
    AssertMsgReturn(!availableSizes.isEmpty(),
                    ("Undefined icon for type '%s'.", (int)enmPixmapType),
                    nullPixmap);

    /* Determine icon metric: */
    const QStyle *pStyle = QApplication::style();
    const int iIconMetric = pStyle->pixelMetric(QStyle::PM_SmallIconSize);

    /* Return pixmap of first available size: */
    return icon.pixmap(QSize(iIconMetric, iIconMetric));
}

QIcon UIIconPoolStorageSettings::icon(PixmapType enmPixmapType,
                                      PixmapType enmPixmapDisabledType /* = PixmapType_Invalid */) const
{
    /* Prepare fallback pixmap: */
    static QPixmap nullPixmap;
    /* Prepare fallback icon: */
    static QIcon nullIcon;

    /* If we do NOT have that 'pixmap type' icon cached already: */
    if (!m_icons.contains(enmPixmapType))
    {
        /* Compose proper icon if we have that 'pixmap type' known: */
        if (m_names.contains(enmPixmapType))
            m_icons[enmPixmapType] = iconSet(m_names[enmPixmapType]);
        /* Assign fallback icon if we do NOT have that 'pixmap type' known: */
        else
            m_icons[enmPixmapType] = iconSet(nullPixmap);
    }

    /* Retrieve normal icon: */
    const QIcon &icon = m_icons[enmPixmapType];
    AssertMsgReturn(!icon.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapType),
                    nullIcon);

    /* If 'disabled' icon is invalid => just return 'normal' icon: */
    if (enmPixmapDisabledType == PixmapType_Invalid)
        return icon;

    /* If we do NOT have that 'pixmap disabled type' icon cached already: */
    if (!m_icons.contains(enmPixmapDisabledType))
    {
        /* Compose proper icon if we have that 'pixmap disabled type' known: */
        if (m_names.contains(enmPixmapDisabledType))
            m_icons[enmPixmapDisabledType] = iconSet(m_names[enmPixmapDisabledType]);
        /* Assign fallback icon if we do NOT have that 'pixmap disabled type' known: */
        else
            m_icons[enmPixmapDisabledType] = iconSet(nullPixmap);
    }

    /* Retrieve disabled icon: */
    const QIcon &iconDisabled = m_icons[enmPixmapDisabledType];
    AssertMsgReturn(!iconDisabled.isNull(),
                    ("Undefined icon for type '%d'.", (int)enmPixmapDisabledType),
                    nullIcon);

    /* Return icon composed on the basis of two above: */
    QIcon resultIcon = icon;
    foreach (const QSize &size, iconDisabled.availableSizes())
        resultIcon.addPixmap(iconDisabled.pixmap(size), QIcon::Disabled);
    return resultIcon;
}

UIIconPoolStorageSettings::UIIconPoolStorageSettings()
{
    /* Connect instance: */
    s_pInstance = this;

    /* Controller file-names: */
    m_names.insert(PixmapType_ControllerAddEn,              ":/controller_add_16px.png");
    m_names.insert(PixmapType_ControllerAddDis,             ":/controller_add_disabled_16px.png");
    m_names.insert(PixmapType_ControllerDelEn,              ":/controller_remove_16px.png");
    m_names.insert(PixmapType_ControllerDelDis,             ":/controller_remove_disabled_16px.png");
    /* Attachment file-names: */
    m_names.insert(PixmapType_AttachmentAddEn,              ":/attachment_add_16px.png");
    m_names.insert(PixmapType_AttachmentAddDis,             ":/attachment_add_disabled_16px.png");
    m_names.insert(PixmapType_AttachmentDelEn,              ":/attachment_remove_16px.png");
    m_names.insert(PixmapType_AttachmentDelDis,             ":/attachment_remove_disabled_16px.png");
    /* Specific controller default/expand/collapse file-names: */
    m_names.insert(PixmapType_IDEControllerNormal,          ":/ide_16px.png");
    m_names.insert(PixmapType_IDEControllerExpand,          ":/ide_expand_16px.png");
    m_names.insert(PixmapType_IDEControllerCollapse,        ":/ide_collapse_16px.png");
    m_names.insert(PixmapType_SATAControllerNormal,         ":/sata_16px.png");
    m_names.insert(PixmapType_SATAControllerExpand,         ":/sata_expand_16px.png");
    m_names.insert(PixmapType_SATAControllerCollapse,       ":/sata_collapse_16px.png");
    m_names.insert(PixmapType_SCSIControllerNormal,         ":/scsi_16px.png");
    m_names.insert(PixmapType_SCSIControllerExpand,         ":/scsi_expand_16px.png");
    m_names.insert(PixmapType_SCSIControllerCollapse,       ":/scsi_collapse_16px.png");
    m_names.insert(PixmapType_SASControllerNormal,          ":/sas_16px.png");
    m_names.insert(PixmapType_SASControllerExpand,          ":/sas_expand_16px.png");
    m_names.insert(PixmapType_SASControllerCollapse,        ":/sas_collapse_16px.png");
    m_names.insert(PixmapType_USBControllerNormal,          ":/usb_16px.png");
    m_names.insert(PixmapType_USBControllerExpand,          ":/usb_expand_16px.png");
    m_names.insert(PixmapType_USBControllerCollapse,        ":/usb_collapse_16px.png");
    m_names.insert(PixmapType_NVMeControllerNormal,         ":/pcie_16px.png");
    m_names.insert(PixmapType_NVMeControllerExpand,         ":/pcie_expand_16px.png");
    m_names.insert(PixmapType_NVMeControllerCollapse,       ":/pcie_collapse_16px.png");
    m_names.insert(PixmapType_VirtioSCSIControllerNormal,   ":/virtio_scsi_16px.png");
    m_names.insert(PixmapType_VirtioSCSIControllerExpand,   ":/virtio_scsi_expand_16px.png");
    m_names.insert(PixmapType_VirtioSCSIControllerCollapse, ":/virtio_scsi_collapse_16px.png");
    m_names.insert(PixmapType_FloppyControllerNormal,       ":/floppy_16px.png");
    m_names.insert(PixmapType_FloppyControllerExpand,       ":/floppy_expand_16px.png");
    m_names.insert(PixmapType_FloppyControllerCollapse,     ":/floppy_collapse_16px.png");
    /* Specific controller add file-names: */
    m_names.insert(PixmapType_IDEControllerAddEn,           ":/ide_add_16px.png");
    m_names.insert(PixmapType_IDEControllerAddDis,          ":/ide_add_disabled_16px.png");
    m_names.insert(PixmapType_SATAControllerAddEn,          ":/sata_add_16px.png");
    m_names.insert(PixmapType_SATAControllerAddDis,         ":/sata_add_disabled_16px.png");
    m_names.insert(PixmapType_SCSIControllerAddEn,          ":/scsi_add_16px.png");
    m_names.insert(PixmapType_SCSIControllerAddDis,         ":/scsi_add_disabled_16px.png");
    m_names.insert(PixmapType_SASControllerAddEn,           ":/sas_add_16px.png");
    m_names.insert(PixmapType_SASControllerAddDis,          ":/sas_add_disabled_16px.png");
    m_names.insert(PixmapType_USBControllerAddEn,           ":/usb_add_16px.png");
    m_names.insert(PixmapType_USBControllerAddDis,          ":/usb_add_disabled_16px.png");
    m_names.insert(PixmapType_NVMeControllerAddEn,          ":/pcie_add_16px.png");
    m_names.insert(PixmapType_NVMeControllerAddDis,         ":/pcie_add_disabled_16px.png");
    m_names.insert(PixmapType_VirtioSCSIControllerAddEn,    ":/virtio_scsi_add_16px.png");
    m_names.insert(PixmapType_VirtioSCSIControllerAddDis,   ":/virtio_scsi_add_disabled_16px.png");
    m_names.insert(PixmapType_FloppyControllerAddEn,        ":/floppy_add_16px.png");
    m_names.insert(PixmapType_FloppyControllerAddDis,       ":/floppy_add_disabled_16px.png");
    /* Specific attachment file-names: */
    m_names.insert(PixmapType_HDAttachmentNormal,           ":/hd_16px.png");
    m_names.insert(PixmapType_CDAttachmentNormal,           ":/cd_16px.png");
    m_names.insert(PixmapType_FDAttachmentNormal,           ":/fd_16px.png");
    /* Specific attachment add file-names: */
    m_names.insert(PixmapType_HDAttachmentAddEn,            ":/hd_add_16px.png");
    m_names.insert(PixmapType_HDAttachmentAddDis,           ":/hd_add_disabled_16px.png");
    m_names.insert(PixmapType_CDAttachmentAddEn,            ":/cd_add_16px.png");
    m_names.insert(PixmapType_CDAttachmentAddDis,           ":/cd_add_disabled_16px.png");
    m_names.insert(PixmapType_FDAttachmentAddEn,            ":/fd_add_16px.png");
    m_names.insert(PixmapType_FDAttachmentAddDis,           ":/fd_add_disabled_16px.png");
    /* Specific attachment custom file-names: */
    m_names.insert(PixmapType_ChooseExistingEn,             ":/select_file_16px.png");
    m_names.insert(PixmapType_ChooseExistingDis,            ":/select_file_disabled_16px.png");
    m_names.insert(PixmapType_CDUnmountEnabled,             ":/cd_unmount_16px.png");
    m_names.insert(PixmapType_CDUnmountDisabled,            ":/cd_unmount_disabled_16px.png");
    m_names.insert(PixmapType_FDUnmountEnabled,             ":/fd_unmount_16px.png");
    m_names.insert(PixmapType_FDUnmountDisabled,            ":/fd_unmount_disabled_16px.png");
}

UIIconPoolStorageSettings::~UIIconPoolStorageSettings()
{
    /* Disconnect instance: */
    s_pInstance = 0;
}


/*********************************************************************************************************************************
*   Class AbstractItem implementation.                                                                                           *
*********************************************************************************************************************************/

AbstractItem::AbstractItem(QITreeView *pParentTree)
    : QITreeViewItem(pParentTree)
    , m_pParentItem(0)
    , m_uId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::AbstractItem(AbstractItem *pParentItem)
    : QITreeViewItem(pParentItem)
    , m_pParentItem(pParentItem)
    , m_uId(QUuid::createUuid())
{
    if (m_pParentItem)
        m_pParentItem->addChild(this);
}

AbstractItem::~AbstractItem()
{
    if (m_pParentItem)
        m_pParentItem->delChild(this);
}

AbstractItem *AbstractItem::parent() const
{
    return m_pParentItem;
}

QUuid AbstractItem::id() const
{
    return m_uId;
}

QUuid AbstractItem::machineId() const
{
    return m_uMachineId;
}

void AbstractItem::setMachineId(const QUuid &uMachineId)
{
    m_uMachineId = uMachineId;
}


/*********************************************************************************************************************************
*   Class RootItem implementation.                                                                                               *
*********************************************************************************************************************************/

RootItem::RootItem(QITreeView *pParentTree)
    : AbstractItem(pParentTree)
{
}

RootItem::~RootItem()
{
    while (!m_controllers.isEmpty())
        delete m_controllers.first();
}

ULONG RootItem::childCount(KStorageBus enmBus) const
{
    ULONG uResult = 0;
    foreach (AbstractItem *pItem, m_controllers)
    {
        ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
        if (pItemController->bus() == enmBus)
            ++ uResult;
    }
    return uResult;
}

AbstractItem::ItemType RootItem::rtti() const
{
    return Type_RootItem;
}

AbstractItem *RootItem::childItem(int iIndex) const
{
    return m_controllers[iIndex];
}

AbstractItem *RootItem::childItemById(const QUuid &uId) const
{
    for (int i = 0; i < childCount(); ++ i)
        if (m_controllers[i]->id() == uId)
            return m_controllers[i];
    return 0;
}

int RootItem::posOfChild(AbstractItem *pItem) const
{
    return m_controllers.indexOf(pItem);
}

int RootItem::childCount() const
{
    return m_controllers.size();
}

QString RootItem::text() const
{
    return QString();
}

QString RootItem::toolTip() const
{
    return QString();
}

QPixmap RootItem::pixmap(ItemState /* enmState */)
{
    return QPixmap();
}

void RootItem::addChild(AbstractItem *pItem)
{
    m_controllers << pItem;
}

void RootItem::delChild(AbstractItem *pItem)
{
    m_controllers.removeAll(pItem);
}


/*********************************************************************************************************************************
*   Class ControllerItem implementation.                                                                                         *
*********************************************************************************************************************************/

ControllerItem::ControllerItem(AbstractItem *pParentItem, const QString &strName,
                               KStorageBus enmBus, KStorageControllerType enmType)
    : AbstractItem(pParentItem)
    , m_strName(strName)
    , m_enmBus(enmBus)
    , m_enmType(enmType)
    , m_uPortCount(0)
    , m_fUseIoCache(false)
{
    /* Check for proper parent type: */
    AssertMsg(parent()->rtti() == AbstractItem::Type_RootItem, ("Incorrect parent type!\n"));

    AssertMsg(m_enmBus != KStorageBus_Null, ("Wrong Bus Type {%d}!\n", m_enmBus));
    AssertMsg(m_enmType != KStorageControllerType_Null, ("Wrong Controller Type {%d}!\n", m_enmType));

    updateBusInfo();
    updateTypeInfo();
    updatePixmaps();

    m_fUseIoCache = uiCommon().virtualBox().GetSystemProperties().GetDefaultIoCacheSettingForStorageController(enmType);
}

ControllerItem::~ControllerItem()
{
    while (!m_attachments.isEmpty())
        delete m_attachments.first();
}

void ControllerItem::setName(const QString &strName)
{
    m_strName = strName;
}

QString ControllerItem::name() const
{
    return m_strName;
}

void ControllerItem::setBus(KStorageBus enmBus)
{
    m_enmBus = enmBus;

    updateBusInfo();
    updateTypeInfo();
    updatePixmaps();
}

KStorageBus ControllerItem::bus() const
{
    return m_enmBus;
}

ControllerBusList ControllerItem::buses() const
{
    return m_buses;
}

void ControllerItem::setType(KStorageControllerType enmType)
{
    m_enmType = enmType;

    updateTypeInfo();
}

KStorageControllerType ControllerItem::type() const
{
    return m_enmType;
}

ControllerTypeList ControllerItem::types(KStorageBus enmBus) const
{
    return m_types.value(enmBus);
}

void ControllerItem::setPortCount(uint uPortCount)
{
    /* Limit maximum port count: */
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    m_uPortCount = qMin(uPortCount, (uint)comProperties.GetMaxPortCountForStorageBus(bus()));
}

uint ControllerItem::portCount()
{
    /* Recalculate actual port count: */
    for (int i = 0; i < m_attachments.size(); ++i)
    {
        AttachmentItem *pItem = qobject_cast<AttachmentItem*>(m_attachments.at(i));
        if (m_uPortCount < (uint)pItem->storageSlot().port + 1)
            m_uPortCount = (uint)pItem->storageSlot().port + 1;
    }
    return m_uPortCount;
}

uint ControllerItem::maxPortCount()
{
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    return (uint)comProperties.GetMaxPortCountForStorageBus(bus());
}

void ControllerItem::setUseIoCache(bool fUseIoCache)
{
    m_fUseIoCache = fUseIoCache;
}

bool ControllerItem::useIoCache() const
{
    return m_fUseIoCache;
}

SlotsList ControllerItem::allSlots() const
{
    SlotsList allSlots;
    CPlatformProperties comProps = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    for (ULONG i = 0; i < comProps.GetMaxPortCountForStorageBus(bus()); ++ i)
        for (ULONG j = 0; j < comProps.GetMaxDevicesPerPortForStorageBus(bus()); ++ j)
            allSlots << StorageSlot(bus(), i, j);
    return allSlots;
}

SlotsList ControllerItem::usedSlots() const
{
    SlotsList usedSlots;
    for (int i = 0; i < m_attachments.size(); ++ i)
        usedSlots << qobject_cast<AttachmentItem*>(m_attachments.at(i))->storageSlot();
    return usedSlots;
}

DeviceTypeList ControllerItem::deviceTypeList() const
{
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    return comProperties.GetDeviceTypesForStorageBus(m_enmBus).toList();
}

QList<QUuid> ControllerItem::attachmentIDs(KDeviceType enmType /* = KDeviceType_Null */) const
{
    QList<QUuid> ids;
    foreach (AbstractItem *pItem, m_attachments)
    {
        AttachmentItem *pItemAttachment = qobject_cast<AttachmentItem*>(pItem);
        if (   enmType == KDeviceType_Null
            || pItemAttachment->deviceType() == enmType)
            ids << pItem->id();
    }
    return ids;
}

AbstractItem::ItemType ControllerItem::rtti() const
{
    return Type_ControllerItem;
}

AbstractItem *ControllerItem::childItem(int iIndex) const
{
    return m_attachments[iIndex];
}

AbstractItem *ControllerItem::childItemById(const QUuid &uId) const
{
    for (int i = 0; i < childCount(); ++ i)
        if (m_attachments[i]->id() == uId)
            return m_attachments[i];
    return 0;
}

int ControllerItem::posOfChild(AbstractItem *pItem) const
{
    return m_attachments.indexOf(pItem);
}

int ControllerItem::childCount() const
{
    return m_attachments.size();
}

QString ControllerItem::text() const
{
    return UIStorageSettingsEditor::tr("Controller: %1").arg(name());
}

QString ControllerItem::toolTip() const
{
    return UIStorageSettingsEditor::tr("<nobr><b>%1</b></nobr><br>"
                                       "<nobr>Bus:&nbsp;&nbsp;%2</nobr><br>"
                                       "<nobr>Type:&nbsp;&nbsp;%3</nobr>")
                                       .arg(m_strName)
                                       .arg(gpConverter->toString(bus()))
                                       .arg(gpConverter->toString(type()));
}

QPixmap ControllerItem::pixmap(ItemState enmState)
{
    return iconPool()->pixmap(m_pixmaps.at(enmState));
}

void ControllerItem::addChild(AbstractItem *pItem)
{
    m_attachments << pItem;
}

void ControllerItem::delChild(AbstractItem *pItem)
{
    m_attachments.removeAll(pItem);
}

void ControllerItem::updateBusInfo()
{
    /* Clear the buses initially: */
    m_buses.clear();

    /* Load currently supported storage buses: */
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();

    /* If current bus is NOT KStorageBus_Floppy: */
    if (m_enmBus != KStorageBus_Floppy)
    {
        /* We update the list with all supported buses
         * and remove the current one from that list. */
        m_buses << supportedBuses.toList();
        m_buses.removeAll(m_enmBus);
    }

    /* And prepend current bus finally: */
    m_buses.prepend(m_enmBus);
}

void ControllerItem::updateTypeInfo()
{
    /* Clear the types initially: */
    m_types.clear();

    /* Load currently supported storage buses & types: */
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();
    const QVector<KStorageControllerType> supportedTypes = comProperties.GetSupportedStorageControllerTypes();

    /* We update the list with all supported buses
     * and remove the current one from that list. */
    ControllerBusList possibleBuses = supportedBuses.toList();
    possibleBuses.removeAll(m_enmBus);

    /* And prepend current bus finally: */
    possibleBuses.prepend(m_enmBus);

    /* Enumerate possible buses: */
    foreach (const KStorageBus &enmBus, possibleBuses)
    {
        /* Enumerate possible types and check whether type is supported or already selected before adding it: */
        foreach (const KStorageControllerType &enmType, comProperties.GetStorageControllerTypesForBus(enmBus))
            if (supportedTypes.contains(enmType) || enmType == m_enmType)
                m_types[enmBus] << enmType;
    }
}

void ControllerItem::updatePixmaps()
{
    m_pixmaps.clear();

    for (int i = 0; i < ItemState_Max; ++i)
    {
        m_pixmaps << PixmapType_Invalid;
        switch (m_enmBus)
        {
            case KStorageBus_IDE:        m_pixmaps[i] = static_cast<PixmapType>(PixmapType_IDEControllerNormal + i); break;
            case KStorageBus_SATA:       m_pixmaps[i] = static_cast<PixmapType>(PixmapType_SATAControllerNormal + i); break;
            case KStorageBus_SCSI:       m_pixmaps[i] = static_cast<PixmapType>(PixmapType_SCSIControllerNormal + i); break;
            case KStorageBus_Floppy:     m_pixmaps[i] = static_cast<PixmapType>(PixmapType_FloppyControllerNormal + i); break;
            case KStorageBus_SAS:        m_pixmaps[i] = static_cast<PixmapType>(PixmapType_SASControllerNormal + i); break;
            case KStorageBus_USB:        m_pixmaps[i] = static_cast<PixmapType>(PixmapType_USBControllerNormal + i); break;
            case KStorageBus_PCIe:       m_pixmaps[i] = static_cast<PixmapType>(PixmapType_NVMeControllerNormal + i); break;
            case KStorageBus_VirtioSCSI: m_pixmaps[i] = static_cast<PixmapType>(PixmapType_VirtioSCSIControllerNormal + i); break;
            default: break;
        }
        AssertMsg(m_pixmaps[i] != PixmapType_Invalid, ("Invalid item state pixmap!\n"));
    }
}


/*********************************************************************************************************************************
*   Class AttachmentItem implementation.                                                                                         *
*********************************************************************************************************************************/

AttachmentItem::AttachmentItem(AbstractItem *pParentItem, KDeviceType enmDeviceType)
    : AbstractItem(pParentItem)
    , m_enmDeviceType(enmDeviceType)
    , m_fHostDrive(false)
    , m_fPassthrough(false)
    , m_fTempEject(false)
    , m_fNonRotational(false)
    , m_fHotPluggable(false)
{
    /* Check for proper parent type: */
    AssertMsg(parent()->rtti() == AbstractItem::Type_ControllerItem, ("Incorrect parent type!\n"));

    /* Select default slot: */
    AssertMsg(!storageSlots().isEmpty(), ("There should be at least one available slot!\n"));
    m_storageSlot = storageSlots()[0];
}

void AttachmentItem::setDeviceType(KDeviceType enmDeviceType)
{
    m_enmDeviceType = enmDeviceType;
}

KDeviceType AttachmentItem::deviceType() const
{
    return m_enmDeviceType;
}

DeviceTypeList AttachmentItem::deviceTypes() const
{
    return qobject_cast<ControllerItem*>(parent())->deviceTypeList();
}

void AttachmentItem::setStorageSlot(const StorageSlot &storageSlot)
{
    m_storageSlot = storageSlot;
}

StorageSlot AttachmentItem::storageSlot() const
{
    return m_storageSlot;
}

SlotsList AttachmentItem::storageSlots() const
{
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(parent());

    /* Filter list from used slots: */
    SlotsList allSlots(pItemController->allSlots());
    SlotsList usedSlots(pItemController->usedSlots());
    foreach(StorageSlot usedSlot, usedSlots)
        if (usedSlot != m_storageSlot)
            allSlots.removeAll(usedSlot);

    return allSlots;
}

void AttachmentItem::setMediumId(const QUuid &uMediumId)
{
    /// @todo is this required?
    //AssertMsg(!aAttMediumId.isNull(), ("Medium ID value can't be null!\n"));
    m_uMediumId = uiCommon().medium(uMediumId).id();
    cache();
}

QUuid AttachmentItem::mediumId() const
{
    return m_uMediumId;
}

bool AttachmentItem::isHostDrive() const
{
    return m_fHostDrive;
}

void AttachmentItem::setPassthrough(bool fPassthrough)
{
    m_fPassthrough = fPassthrough;
}

bool AttachmentItem::isPassthrough() const
{
    return m_fPassthrough;
}

void AttachmentItem::setTempEject(bool fTempEject)
{
    m_fTempEject = fTempEject;
}

bool AttachmentItem::isTempEject() const
{
    return m_fTempEject;
}

void AttachmentItem::setNonRotational(bool fNonRotational)
{
    m_fNonRotational = fNonRotational;
}

bool AttachmentItem::isNonRotational() const
{
    return m_fNonRotational;
}

void AttachmentItem::setHotPluggable(bool fHotPluggable)
{
    m_fHotPluggable = fHotPluggable;
}

bool AttachmentItem::isHotPluggable() const
{
    return m_fHotPluggable;
}

QString AttachmentItem::size() const
{
    return m_strSize;
}

QString AttachmentItem::logicalSize() const
{
    return m_strLogicalSize;
}

QString AttachmentItem::location() const
{
    return m_strLocation;
}

QString AttachmentItem::format() const
{
    return m_strFormat;
}

QString AttachmentItem::details() const
{
    return m_strDetails;
}

QString AttachmentItem::usage() const
{
    return m_strUsage;
}

QString AttachmentItem::encryptionPasswordId() const
{
    return m_strAttEncryptionPasswordID;
}

void AttachmentItem::cache()
{
    UIMedium guiMedium = uiCommon().medium(m_uMediumId);

    /* Cache medium information: */
    m_strName = guiMedium.name(true);
    m_strTip = guiMedium.toolTipCheckRO(true, m_enmDeviceType != KDeviceType_HardDisk);
    m_strPixmap = guiMedium.iconCheckRO(true);
    m_fHostDrive = guiMedium.isHostDrive();

    /* Cache additional information: */
    m_strSize = guiMedium.size(true);
    m_strLogicalSize = guiMedium.logicalSize(true);
    m_strLocation = guiMedium.location(true);
    m_strAttEncryptionPasswordID = QString("--");
    if (guiMedium.isNull())
    {
        m_strFormat = QString("--");
    }
    else
    {
        switch (m_enmDeviceType)
        {
            case KDeviceType_HardDisk:
            {
                m_strFormat = QString("%1 (%2)").arg(guiMedium.hardDiskType(true)).arg(guiMedium.hardDiskFormat(true));
                m_strDetails = guiMedium.storageDetails();
                const QString strAttEncryptionPasswordID = guiMedium.encryptionPasswordID();
                if (!strAttEncryptionPasswordID.isNull())
                    m_strAttEncryptionPasswordID = strAttEncryptionPasswordID;
                break;
            }
            case KDeviceType_DVD:
            case KDeviceType_Floppy:
            {
                m_strFormat = m_fHostDrive ? UIStorageSettingsEditor::tr("Host Drive") : UIStorageSettingsEditor::tr("Image", "storage image");
                break;
            }
            default:
                break;
        }
    }
    m_strUsage = guiMedium.usage(true);

    /* Fill empty attributes: */
    if (m_strUsage.isEmpty())
        m_strUsage = QString("--");
}

AbstractItem::ItemType AttachmentItem::rtti() const
{
    return Type_AttachmentItem;
}

AbstractItem *AttachmentItem::childItem(int /* iIndex */) const
{
    return 0;
}

AbstractItem *AttachmentItem::childItemById(const QUuid& /* uId */) const
{
    return 0;
}

int AttachmentItem::posOfChild(AbstractItem * /* pItem */) const
{
    return 0;
}

int AttachmentItem::childCount() const
{
    return 0;
}

QString AttachmentItem::text() const
{
    return m_strName;
}

QString AttachmentItem::toolTip() const
{
    return m_strTip;
}

QPixmap AttachmentItem::pixmap(ItemState /* enmState */)
{
    if (m_strPixmap.isNull())
    {
        switch (m_enmDeviceType)
        {
            case KDeviceType_HardDisk:
                m_strPixmap = iconPool()->pixmap(PixmapType_HDAttachmentNormal);
                break;
            case KDeviceType_DVD:
                m_strPixmap = iconPool()->pixmap(PixmapType_CDAttachmentNormal);
                break;
            case KDeviceType_Floppy:
                m_strPixmap = iconPool()->pixmap(PixmapType_FDAttachmentNormal);
                break;
            default:
                break;
        }
    }
    return m_strPixmap;
}

void AttachmentItem::addChild(AbstractItem * /* pItem */)
{
}

void AttachmentItem::delChild(AbstractItem * /* pItem */)
{
}


/*********************************************************************************************************************************
*   Class StorageModel implementation.                                                                                           *
*********************************************************************************************************************************/

StorageModel::StorageModel(QITreeView *pParentTree)
    : QAbstractItemModel(pParentTree)
    , m_pRootItem(new RootItem(pParentTree))
    , m_enmToolTipType(ToolTipType_Default)
    , m_enmChipsetType(KChipsetType_PIIX3)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
{
}

StorageModel::~StorageModel()
{
    delete m_pRootItem;
}

int StorageModel::rowCount(const QModelIndex &parentIndex) const
{
    return !parentIndex.isValid() ? 1 /* only root item has invalid parent */ :
           static_cast<AbstractItem*>(parentIndex.internalPointer())->childCount();
}

int StorageModel::columnCount(const QModelIndex & /* parentIndex */) const
{
    return 1;
}

QModelIndex StorageModel::root() const
{
    return index(0, 0);
}

QModelIndex StorageModel::index(int iRow, int iColumn, const QModelIndex &parentIndex) const
{
    if (!hasIndex(iRow, iColumn, parentIndex))
        return QModelIndex();

    AbstractItem *pItem = !parentIndex.isValid() ? m_pRootItem :
                          static_cast<AbstractItem*>(parentIndex.internalPointer())->childItem(iRow);

    return pItem ? createIndex(iRow, iColumn, pItem) : QModelIndex();
}

QModelIndex StorageModel::parent(const QModelIndex &specifiedIndex) const
{
    if (!specifiedIndex.isValid())
        return QModelIndex();

    AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer());
    AbstractItem *pParentOfItem = pItem->parent();
    AbstractItem *pParentOfParent = pParentOfItem ? pParentOfItem->parent() : 0;
    int iPosition = pParentOfParent ? pParentOfParent->posOfChild(pParentOfItem) : 0;

    if (pParentOfItem)
        return createIndex(iPosition, 0, pParentOfItem);
    else
        return QModelIndex();
}

QVariant StorageModel::data(const QModelIndex &specifiedIndex, int iRole) const
{
    if (!specifiedIndex.isValid())
        return QVariant();

    switch (iRole)
    {
        /* Basic Attributes: */
        case Qt::FontRole:
        {
            return QVariant(qApp->font());
        }
        case Qt::SizeHintRole:
        {
            QFontMetrics fm(data(specifiedIndex, Qt::FontRole).value<QFont>());
            int iMinimumHeight = qMax(fm.height(), data(specifiedIndex, R_IconSize).toInt());
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            return QSize(1 /* ignoring width */, 2 * iMargin + iMinimumHeight);
        }
        case Qt::ToolTipRole:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
            {
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    QString strTip(pItem->toolTip());
                    switch (m_enmToolTipType)
                    {
                        case ToolTipType_Expander:
                            if (index(0, 0, specifiedIndex).isValid())
                                strTip = UIStorageSettingsEditor::tr("<nobr>Expands/Collapses&nbsp;item.</nobr>");
                            break;
                        case ToolTipType_HDAdder:
                            strTip = UIStorageSettingsEditor::tr("<nobr>Adds&nbsp;hard&nbsp;disk.</nobr>");
                            break;
                        case ToolTipType_CDAdder:
                            strTip = UIStorageSettingsEditor::tr("<nobr>Adds&nbsp;optical&nbsp;drive.</nobr>");
                            break;
                        case ToolTipType_FDAdder:
                            strTip = UIStorageSettingsEditor::tr("<nobr>Adds&nbsp;floppy&nbsp;drive.</nobr>");
                            break;
                        default:
                            break;
                    }
                    return strTip;
                }
                return pItem->toolTip();
            }
            return QString();
        }

        /* Advanced Attributes: */
        case R_ItemId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                return pItem->id();
            return QUuid();
        }
        case R_ItemPixmap:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
            {
                ItemState enmState = ItemState_Default;
                if (hasChildren(specifiedIndex))
                    if (QTreeView *view = qobject_cast<QTreeView*>(QObject::parent()))
                        enmState = view->isExpanded(specifiedIndex) ? ItemState_Expanded : ItemState_Collapsed;
                return pItem->pixmap(enmState);
            }
            return QPixmap();
        }
        case R_ItemPixmapRect:
        {
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            int iWidth = data(specifiedIndex, R_IconSize).toInt();
            return QRect(iMargin, iMargin, iWidth, iWidth);
        }
        case R_ItemName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                return pItem->text();
            return QString();
        }
        case R_ItemNamePoint:
        {
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            int iSpacing = data(specifiedIndex, R_Spacing).toInt();
            int iWidth = data(specifiedIndex, R_IconSize).toInt();
            QFontMetrics fm(data(specifiedIndex, Qt::FontRole).value<QFont>());
            QSize sizeHint = data(specifiedIndex, Qt::SizeHintRole).toSize();
            return QPoint(iMargin + iWidth + 2 * iSpacing,
                          sizeHint.height() / 2 + fm.ascent() / 2 - 1 /* base line */);
        }
        case R_ItemType:
        {
            QVariant result(QVariant::fromValue(AbstractItem::Type_InvalidItem));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                result.setValue(pItem->rtti());
            return result;
        }
        case R_IsController:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                return pItem->rtti() == AbstractItem::Type_ControllerItem;
            return false;
        }
        case R_IsAttachment:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                return pItem->rtti() == AbstractItem::Type_AttachmentItem;
            return false;
        }

        case R_ToolTipType:
        {
            return QVariant::fromValue(m_enmToolTipType);
        }
        case R_IsMoreIDEControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_IDE) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_IDE));
        }
        case R_IsMoreSATAControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SATA) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SATA));
        }
        case R_IsMoreSCSIControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SCSI) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SCSI));
        }
        case R_IsMoreFloppyControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_Floppy) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_Floppy));
        }
        case R_IsMoreSASControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_SAS) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_SAS));
        }
        case R_IsMoreUSBControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_USB) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_USB));
        }
        case R_IsMoreNVMeControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_PCIe) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_PCIe));
        }
        case R_IsMoreVirtioSCSIControllersPossible:
        {
            CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
            return (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full) &&
                   (qobject_cast<RootItem*>(m_pRootItem)->childCount(KStorageBus_VirtioSCSI) <
                    comProperties.GetMaxInstancesOfStorageBus(chipsetType(), KStorageBus_VirtioSCSI));
        }
        case R_IsMoreAttachmentsPossible:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
            {
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
                    CPlatformProperties comProps = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
                    const bool fIsMoreAttachmentsPossible = (ULONG)rowCount(specifiedIndex) <
                                                            (comProps.GetMaxPortCountForStorageBus(pItemController->bus()) *
                                                             comProps.GetMaxDevicesPerPortForStorageBus(pItemController->bus()));
                    if (fIsMoreAttachmentsPossible)
                    {
                        switch (m_enmConfigurationAccessLevel)
                        {
                            case ConfigurationAccessLevel_Full:
                                return true;
                            case ConfigurationAccessLevel_Partial_Running:
                            {
                                switch (pItemController->bus())
                                {
                                    case KStorageBus_USB:
                                        return true;
                                    case KStorageBus_SATA:
                                        return (uint)rowCount(specifiedIndex) < pItemController->portCount();
                                    default:
                                        break;
                                }
                            }
                            default:
                                break;
                        }
                    }
                }
            }
            return false;
        }

        case R_CtrName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->name();
            return QString();
        }
        case R_CtrType:
        {
            QVariant result(QVariant::fromValue(KStorageControllerType_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->type());
            return result;
        }
        case R_CtrTypesForIDE:
        case R_CtrTypesForSATA:
        case R_CtrTypesForSCSI:
        case R_CtrTypesForFloppy:
        case R_CtrTypesForSAS:
        case R_CtrTypesForUSB:
        case R_CtrTypesForPCIe:
        case R_CtrTypesForVirtioSCSI:
        {
            QVariant result(QVariant::fromValue(ControllerTypeList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->types(roleToBus((StorageModel::DataRole)iRole)));
            return result;
        }
        case R_CtrDevices:
        {
            QVariant result(QVariant::fromValue(DeviceTypeList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->deviceTypeList());
            return result;
        }
        case R_CtrBusType:
        {
            QVariant result(QVariant::fromValue(KStorageBus_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->bus());
            return result;
        }
        case R_CtrBusTypes:
        {
            QVariant result(QVariant::fromValue(ControllerBusList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    result.setValue(qobject_cast<ControllerItem*>(pItem)->buses());
            return result;
        }
        case R_CtrPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->portCount();
            return 0;
        }
        case R_CtrMaxPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->maxPortCount();
            return 0;
        }
        case R_CtrIoCache:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                    return qobject_cast<ControllerItem*>(pItem)->useIoCache();
            return false;
        }

        case R_AttSlot:
        {
            QVariant result(QVariant::fromValue(StorageSlot()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->storageSlot());
            return result;
        }
        case R_AttSlots:
        {
            QVariant result(QVariant::fromValue(SlotsList()));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->storageSlots());
            return result;
        }
        case R_AttDevice:
        {
            QVariant result(QVariant::fromValue(KDeviceType_Null));
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    result.setValue(qobject_cast<AttachmentItem*>(pItem)->deviceType());
            return result;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->mediumId();
            return QUuid();
        }
        case R_AttIsHostDrive:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isHostDrive();
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isPassthrough();
            return false;
        }
        case R_AttIsTempEject:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isTempEject();
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isNonRotational();
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->isHotPluggable();
            return false;
        }
        case R_AttSize:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->size();
            return QString();
        }
        case R_AttLogicalSize:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->logicalSize();
            return QString();
        }
        case R_AttLocation:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->location();
            return QString();
        }
        case R_AttFormat:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->format();
            return QString();
        }
        case R_AttDetails:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->details();
            return QString();
        }
        case R_AttUsage:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->usage();
            return QString();
        }
        case R_AttEncryptionPasswordID:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                    return qobject_cast<AttachmentItem*>(pItem)->encryptionPasswordId();
            return QString();
        }
        case R_Margin:
        {
            return 4;
        }
        case R_Spacing:
        {
            return 4;
        }
        case R_IconSize:
        {
            return QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        }

        case R_HDPixmapEn:
        {
            return iconPool()->pixmap(PixmapType_HDAttachmentNormal);
        }
        case R_CDPixmapEn:
        {
            return iconPool()->pixmap(PixmapType_CDAttachmentNormal);
        }
        case R_FDPixmapEn:
        {
            return iconPool()->pixmap(PixmapType_FDAttachmentNormal);
        }

        case R_HDPixmapAddEn:
        {
            return iconPool()->pixmap(PixmapType_HDAttachmentAddEn);
        }
        case R_HDPixmapAddDis:
        {
            return iconPool()->pixmap(PixmapType_HDAttachmentAddDis);
        }
        case R_CDPixmapAddEn:
        {
            return iconPool()->pixmap(PixmapType_CDAttachmentAddEn);
        }
        case R_CDPixmapAddDis:
        {
            return iconPool()->pixmap(PixmapType_CDAttachmentAddDis);
        }
        case R_FDPixmapAddEn:
        {
            return iconPool()->pixmap(PixmapType_FDAttachmentAddEn);
        }
        case R_FDPixmapAddDis:
        {
            return iconPool()->pixmap(PixmapType_FDAttachmentAddDis);
        }
        case R_HDPixmapRect:
        {
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            int iWidth = data(specifiedIndex, R_IconSize).toInt();
            return QRect(0 - iWidth - iMargin, iMargin, iWidth, iWidth);
        }
        case R_CDPixmapRect:
        {
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            int iSpacing = data(specifiedIndex, R_Spacing).toInt();
            int iWidth = data(specifiedIndex, R_IconSize).toInt();
            return QRect(0 - iWidth - iSpacing - iWidth - iMargin, iMargin, iWidth, iWidth);
        }
        case R_FDPixmapRect:
        {
            int iMargin = data(specifiedIndex, R_Margin).toInt();
            int iWidth = data(specifiedIndex, R_IconSize).toInt();
            return QRect(0 - iWidth - iMargin, iMargin, iWidth, iWidth);
        }

        default:
            break;
    }
    return QVariant();
}

bool StorageModel::setData(const QModelIndex &specifiedIndex, const QVariant &aValue, int iRole)
{
    if (!specifiedIndex.isValid())
        return QAbstractItemModel::setData(specifiedIndex, aValue, iRole);

    switch (iRole)
    {
        case R_ToolTipType:
        {
            m_enmToolTipType = aValue.value<ToolTipType>();
            emit dataChanged(specifiedIndex, specifiedIndex);
            return true;
        }
        case R_CtrName:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setName(aValue.toString());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_CtrBusType:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    /* Acquire controller item and requested storage bus type: */
                    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
                    const KStorageBus enmNewCtrBusType = aValue.value<KStorageBus>();

                    /* PCIe devices allows for hard-drives attachments only,
                     * no optical devices. So, lets make sure that rule is fulfilled. */
                    if (enmNewCtrBusType == KStorageBus_PCIe)
                    {
                        const QList<QUuid> opticalIds = pItemController->attachmentIDs(KDeviceType_DVD);
                        if (!opticalIds.isEmpty())
                        {
                            if (!msgCenter().confirmStorageBusChangeWithOpticalRemoval(qobject_cast<QWidget*>(QObject::parent())))
                                return false;
                            foreach (const QUuid &uId, opticalIds)
                                delAttachment(pItemController->id(), uId);
                        }
                    }

                    /* Lets make sure there is enough of place for all the remaining attachments: */
                    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
                    const uint uMaxPortCount =
                        (uint)comProperties.GetMaxPortCountForStorageBus(enmNewCtrBusType);
                    const uint uMaxDevicePerPortCount =
                        (uint)comProperties.GetMaxDevicesPerPortForStorageBus(enmNewCtrBusType);
                    const QList<QUuid> ids = pItemController->attachmentIDs();
                    if (uMaxPortCount * uMaxDevicePerPortCount < (uint)ids.size())
                    {
                        if (!msgCenter().confirmStorageBusChangeWithExcessiveRemoval(qobject_cast<QWidget*>(QObject::parent())))
                            return false;
                        for (int i = uMaxPortCount * uMaxDevicePerPortCount; i < ids.size(); ++i)
                            delAttachment(pItemController->id(), ids.at(i));
                    }

                    /* Push new bus/controller type: */
                    pItemController->setBus(enmNewCtrBusType);
                    pItemController->setType(pItemController->types(enmNewCtrBusType).first());
                    emit dataChanged(specifiedIndex, specifiedIndex);

                    /* Make sure each of remaining attachments has valid slot: */
                    foreach (AbstractItem *pChildItem, pItemController->attachments())
                    {
                        AttachmentItem *pChildItemAttachment = qobject_cast<AttachmentItem*>(pChildItem);
                        const SlotsList availableSlots = pChildItemAttachment->storageSlots();
                        const StorageSlot currentSlot = pChildItemAttachment->storageSlot();
                        if (!availableSlots.isEmpty() && !availableSlots.contains(currentSlot))
                            pChildItemAttachment->setStorageSlot(availableSlots.first());
                    }

                    /* This means success: */
                    return true;
                }
            return false;
        }
        case R_CtrType:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setType(aValue.value<KStorageControllerType>());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_CtrPortCount:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setPortCount(aValue.toUInt());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_CtrIoCache:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_ControllerItem)
                {
                    qobject_cast<ControllerItem*>(pItem)->setUseIoCache(aValue.toBool());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttSlot:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setStorageSlot(aValue.value<StorageSlot>());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    sort();
                    return true;
                }
            return false;
        }
        case R_AttDevice:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setDeviceType(aValue.value<KDeviceType>());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttMediumId:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setMediumId(aValue.toUuid());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsPassthrough:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setPassthrough(aValue.toBool());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsTempEject:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setTempEject(aValue.toBool());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsNonRotational:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setNonRotational(aValue.toBool());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        case R_AttIsHotPluggable:
        {
            if (AbstractItem *pItem = static_cast<AbstractItem*>(specifiedIndex.internalPointer()))
                if (pItem->rtti() == AbstractItem::Type_AttachmentItem)
                {
                    qobject_cast<AttachmentItem*>(pItem)->setHotPluggable(aValue.toBool());
                    emit dataChanged(specifiedIndex, specifiedIndex);
                    return true;
                }
            return false;
        }
        default:
            break;
    }

    return false;
}

QModelIndex StorageModel::addController(const QString &aCtrName, KStorageBus enmBus, KStorageControllerType enmType)
{
    beginInsertRows(root(), m_pRootItem->childCount(), m_pRootItem->childCount());
    new ControllerItem(m_pRootItem, aCtrName, enmBus, enmType);
    endInsertRows();
    return index(m_pRootItem->childCount() - 1, 0, root());
}

void StorageModel::delController(const QUuid &uCtrId)
{
    if (AbstractItem *pItem = m_pRootItem->childItemById(uCtrId))
    {
        int iItemPosition = m_pRootItem->posOfChild(pItem);
        beginRemoveRows(root(), iItemPosition, iItemPosition);
        delete pItem;
        endRemoveRows();
    }
}

QModelIndex StorageModel::addAttachment(const QUuid &uCtrId, KDeviceType enmDeviceType, const QUuid &uMediumId)
{
    if (AbstractItem *pParentItem = m_pRootItem->childItemById(uCtrId))
    {
        int iParentPosition = m_pRootItem->posOfChild(pParentItem);
        QModelIndex parentIndex = index(iParentPosition, 0, root());
        beginInsertRows(parentIndex, pParentItem->childCount(), pParentItem->childCount());
        AttachmentItem *pItem = new AttachmentItem(pParentItem, enmDeviceType);
        pItem->setHotPluggable(m_enmConfigurationAccessLevel != ConfigurationAccessLevel_Full);
        pItem->setMediumId(uMediumId);
        endInsertRows();
        return index(pParentItem->childCount() - 1, 0, parentIndex);
    }
    return QModelIndex();
}

void StorageModel::delAttachment(const QUuid &uCtrId, const QUuid &uAttId)
{
    if (AbstractItem *pParentItem = m_pRootItem->childItemById(uCtrId))
    {
        int iParentPosition = m_pRootItem->posOfChild(pParentItem);
        if (AbstractItem *pItem = pParentItem->childItemById(uAttId))
        {
            int iItemPosition = pParentItem->posOfChild(pItem);
            beginRemoveRows(index(iParentPosition, 0, root()), iItemPosition, iItemPosition);
            delete pItem;
            endRemoveRows();
        }
    }
}

void StorageModel::moveAttachment(const QUuid &uAttId, const QUuid &uCtrOldId, const QUuid &uCtrNewId)
{
    /* No known info about attachment device type and medium ID: */
    KDeviceType enmDeviceType = KDeviceType_Null;
    QUuid uMediumId;

    /* First of all we are looking for old controller item: */
    AbstractItem *pOldItem = m_pRootItem->childItemById(uCtrOldId);
    if (pOldItem)
    {
        /* And acquire controller position: */
        const int iOldCtrPosition = m_pRootItem->posOfChild(pOldItem);

        /* Then we are looking for an attachment item: */
        if (AbstractItem *pSubItem = pOldItem->childItemById(uAttId))
        {
            /* And make sure this is really an attachment: */
            AttachmentItem *pSubItemAttachment = qobject_cast<AttachmentItem*>(pSubItem);
            if (pSubItemAttachment)
            {
                /* This way we can acquire actual attachment device type and medium ID: */
                enmDeviceType = pSubItemAttachment->deviceType();
                uMediumId = pSubItemAttachment->mediumId();

                /* And delete atachment item finally: */
                const int iAttPosition = pOldItem->posOfChild(pSubItem);
                beginRemoveRows(index(iOldCtrPosition, 0, root()), iAttPosition, iAttPosition);
                delete pSubItem;
                endRemoveRows();
            }
        }
    }

    /* As the last step we are looking for new controller item: */
    AbstractItem *pNewItem = m_pRootItem->childItemById(uCtrNewId);
    if (pNewItem)
    {
        /* And acquire controller position: */
        const int iNewCtrPosition = m_pRootItem->posOfChild(pNewItem);

        /* Then we have to make sure moved attachment is valid: */
        if (enmDeviceType != KDeviceType_Null)
        {
            /* And create new attachment item finally: */
            QModelIndex newCtrIndex = index(iNewCtrPosition, 0, root());
            beginInsertRows(newCtrIndex, pNewItem->childCount(), pNewItem->childCount());
            AttachmentItem *pItem = new AttachmentItem(pNewItem, enmDeviceType);
            pItem->setHotPluggable(m_enmConfigurationAccessLevel != ConfigurationAccessLevel_Full);
            pItem->setMediumId(uMediumId);
            endInsertRows();
        }
    }
}

KDeviceType StorageModel::attachmentDeviceType(const QUuid &uCtrId, const QUuid &uAttId) const
{
    /* First of all we are looking for top-level (controller) item: */
    AbstractItem *pTopLevelItem = m_pRootItem->childItemById(uCtrId);
    if (pTopLevelItem)
    {
        /* Then we are looking for sub-level (attachment) item: */
        AbstractItem *pSubLevelItem = pTopLevelItem->childItemById(uAttId);
        if (pSubLevelItem)
        {
            /* And make sure this is really an attachment: */
            AttachmentItem *pAttachmentItem = qobject_cast<AttachmentItem*>(pSubLevelItem);
            if (pAttachmentItem)
            {
                /* This way we can acquire actual attachment device type: */
                return pAttachmentItem->deviceType();
            }
        }
    }

    /* Null by default: */
    return KDeviceType_Null;
}

void StorageModel::setMachineId(const QUuid &uMachineId)
{
    m_pRootItem->setMachineId(uMachineId);
}

void StorageModel::sort(int /* iColumn */, Qt::SortOrder enmOrder)
{
    /* Count of controller items: */
    int iItemLevel1Count = m_pRootItem->childCount();
    /* For each of controller items: */
    for (int iItemLevel1Pos = 0; iItemLevel1Pos < iItemLevel1Count; ++iItemLevel1Pos)
    {
        /* Get iterated controller item: */
        AbstractItem *pItemLevel1 = m_pRootItem->childItem(iItemLevel1Pos);
        ControllerItem *pControllerItem = qobject_cast<ControllerItem*>(pItemLevel1);
        /* Count of attachment items: */
        int iItemLevel2Count = pItemLevel1->childCount();
        /* Prepare empty list for sorted attachments: */
        QList<AbstractItem*> newAttachments;
        /* For each of attachment items: */
        for (int iItemLevel2Pos = 0; iItemLevel2Pos < iItemLevel2Count; ++iItemLevel2Pos)
        {
            /* Get iterated attachment item: */
            AbstractItem *pItemLevel2 = pItemLevel1->childItem(iItemLevel2Pos);
            AttachmentItem *pAttachmentItem = qobject_cast<AttachmentItem*>(pItemLevel2);
            /* Get iterated attachment storage slot: */
            StorageSlot attachmentSlot = pAttachmentItem->storageSlot();
            int iInsertPosition = 0;
            for (; iInsertPosition < newAttachments.size(); ++iInsertPosition)
            {
                /* Get sorted attachment item: */
                AbstractItem *pNewItemLevel2 = newAttachments[iInsertPosition];
                AttachmentItem *pNewAttachmentItem = qobject_cast<AttachmentItem*>(pNewItemLevel2);
                /* Get sorted attachment storage slot: */
                StorageSlot newAttachmentSlot = pNewAttachmentItem->storageSlot();
                /* Apply sorting rule: */
                if (((enmOrder == Qt::AscendingOrder) && (attachmentSlot < newAttachmentSlot)) ||
                    ((enmOrder == Qt::DescendingOrder) && (attachmentSlot > newAttachmentSlot)))
                    break;
            }
            /* Insert iterated attachment into sorted position: */
            newAttachments.insert(iInsertPosition, pItemLevel2);
        }

        /* If that controller has attachments: */
        if (iItemLevel2Count)
        {
            /* We should update corresponding model-indexes: */
            QModelIndex controllerIndex = index(iItemLevel1Pos, 0, root());
            pControllerItem->setAttachments(newAttachments);
            /* That is actually beginMoveRows() + endMoveRows() which
             * unfortunately become available only in Qt 4.6 version. */
            beginRemoveRows(controllerIndex, 0, iItemLevel2Count - 1);
            endRemoveRows();
            beginInsertRows(controllerIndex, 0, iItemLevel2Count - 1);
            endInsertRows();
        }
    }
}

QModelIndex StorageModel::attachmentBySlot(QModelIndex controllerIndex, StorageSlot attachmentStorageSlot)
{
    /* Check what parent model index is valid, set and of 'controller' type: */
    AssertMsg(controllerIndex.isValid(), ("Controller index should be valid!\n"));
    AbstractItem *pParentItem = static_cast<AbstractItem*>(controllerIndex.internalPointer());
    AssertMsg(pParentItem, ("Parent item should be set!\n"));
    AssertMsg(pParentItem->rtti() == AbstractItem::Type_ControllerItem, ("Parent item should be of 'controller' type!\n"));
    NOREF(pParentItem);

    /* Search for suitable attachment one by one: */
    for (int i = 0; i < rowCount(controllerIndex); ++i)
    {
        QModelIndex curAttachmentIndex = index(i, 0, controllerIndex);
        StorageSlot curAttachmentStorageSlot = data(curAttachmentIndex, R_AttSlot).value<StorageSlot>();
        if (curAttachmentStorageSlot ==  attachmentStorageSlot)
            return curAttachmentIndex;
    }
    return QModelIndex();
}

KChipsetType StorageModel::chipsetType() const
{
    return m_enmChipsetType;
}

void StorageModel::setChipsetType(KChipsetType enmChipsetType)
{
    m_enmChipsetType = enmChipsetType;
}

void StorageModel::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;
}

void StorageModel::clear()
{
    while (m_pRootItem->childCount())
    {
        beginRemoveRows(root(), 0, 0);
        delete m_pRootItem->childItem(0);
        endRemoveRows();
    }
}

QMap<KStorageBus, int> StorageModel::currentControllerTypes() const
{
    QMap<KStorageBus, int> currentMap;
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType < KStorageBus_Max; ++iStorageBusType)
    {
        currentMap.insert((KStorageBus)iStorageBusType,
                          qobject_cast<RootItem*>(m_pRootItem)->childCount((KStorageBus)iStorageBusType));
    }
    return currentMap;
}

QMap<KStorageBus, int> StorageModel::maximumControllerTypes() const
{
    QMap<KStorageBus, int> maximumMap;
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    for (int iStorageBusType = KStorageBus_IDE; iStorageBusType < KStorageBus_Max; ++iStorageBusType)
        maximumMap.insert((KStorageBus)iStorageBusType,
                          comProperties.GetMaxInstancesOfStorageBus(chipsetType(), (KStorageBus)iStorageBusType));
    return maximumMap;
}

/* static */
KStorageBus StorageModel::roleToBus(StorageModel::DataRole enmRole)
{
    QMap<StorageModel::DataRole, KStorageBus> typeRoles;
    typeRoles[StorageModel::R_CtrTypesForIDE]        = KStorageBus_IDE;
    typeRoles[StorageModel::R_CtrTypesForSATA]       = KStorageBus_SATA;
    typeRoles[StorageModel::R_CtrTypesForSCSI]       = KStorageBus_SCSI;
    typeRoles[StorageModel::R_CtrTypesForFloppy]     = KStorageBus_Floppy;
    typeRoles[StorageModel::R_CtrTypesForSAS]        = KStorageBus_SAS;
    typeRoles[StorageModel::R_CtrTypesForUSB]        = KStorageBus_USB;
    typeRoles[StorageModel::R_CtrTypesForPCIe]       = KStorageBus_PCIe;
    typeRoles[StorageModel::R_CtrTypesForVirtioSCSI] = KStorageBus_VirtioSCSI;
    return typeRoles.value(enmRole);
}

/* static */
StorageModel::DataRole StorageModel::busToRole(KStorageBus enmBus)
{
    QMap<KStorageBus, StorageModel::DataRole> typeRoles;
    typeRoles[KStorageBus_IDE]        = StorageModel::R_CtrTypesForIDE;
    typeRoles[KStorageBus_SATA]       = StorageModel::R_CtrTypesForSATA;
    typeRoles[KStorageBus_SCSI]       = StorageModel::R_CtrTypesForSCSI;
    typeRoles[KStorageBus_Floppy]     = StorageModel::R_CtrTypesForFloppy;
    typeRoles[KStorageBus_SAS]        = StorageModel::R_CtrTypesForSAS;
    typeRoles[KStorageBus_USB]        = StorageModel::R_CtrTypesForUSB;
    typeRoles[KStorageBus_PCIe]       = StorageModel::R_CtrTypesForPCIe;
    typeRoles[KStorageBus_VirtioSCSI] = StorageModel::R_CtrTypesForVirtioSCSI;
    return typeRoles.value(enmBus);
}

Qt::ItemFlags StorageModel::flags(const QModelIndex &specifiedIndex) const
{
    return !specifiedIndex.isValid() ? QAbstractItemModel::flags(specifiedIndex) :
           Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}


/*********************************************************************************************************************************
*   Class StorageDelegate implementation.                                                                                        *
*********************************************************************************************************************************/

StorageDelegate::StorageDelegate(QObject *pParent)
    : QItemDelegate(pParent)
{
}

void StorageDelegate::paint(QPainter *pPainter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!index.isValid()) return;

    /* Initialize variables: */
    QStyle::State enmState = option.state;
    QRect rect = option.rect;
    const StorageModel *pModel = qobject_cast<const StorageModel*>(index.model());
    Assert(pModel);

    pPainter->save();

    /* Draw item background: */
    QItemDelegate::drawBackground(pPainter, option, index);

    /* Setup foreground settings: */
    QPalette::ColorGroup cg = enmState & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
    bool fSelected = enmState & QStyle::State_Selected;
    bool fFocused = enmState & QStyle::State_HasFocus;
    bool fGrayOnLoosingFocus = QApplication::style()->styleHint(QStyle::SH_ItemView_ChangeHighlightOnFocus, &option) != 0;
    pPainter->setPen(option.palette.color(cg, fSelected &&(fFocused || !fGrayOnLoosingFocus) ?
                                          QPalette::HighlightedText : QPalette::Text));

    pPainter->translate(rect.x(), rect.y());

    /* Draw Item Pixmap: */
    pPainter->drawPixmap(pModel->data(index, StorageModel::R_ItemPixmapRect).toRect().topLeft(),
                         pModel->data(index, StorageModel::R_ItemPixmap).value<QPixmap>());

    /* Draw compressed item name: */
    int iMargin = pModel->data(index, StorageModel::R_Margin).toInt();
    int iIconWidth = pModel->data(index, StorageModel::R_IconSize).toInt();
    int iSpacing = pModel->data(index, StorageModel::R_Spacing).toInt();
    QPoint textPosition = pModel->data(index, StorageModel::R_ItemNamePoint).toPoint();
    int iTextWidth = rect.width() - textPosition.x();
    if (pModel->data(index, StorageModel::R_IsController).toBool() && enmState & QStyle::State_Selected)
    {
        iTextWidth -= (2 * iSpacing + iIconWidth + iMargin);
        if (pModel->data(index, StorageModel::R_CtrBusType).value<KStorageBus>() != KStorageBus_Floppy)
            iTextWidth -= (iSpacing + iIconWidth);
    }
    QString strText(pModel->data(index, StorageModel::R_ItemName).toString());
    QString strShortText(strText);
    QFont font = pModel->data(index, Qt::FontRole).value<QFont>();
    QFontMetrics fm(font);
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    while ((strShortText.size() > 1) && (fm.horizontalAdvance(strShortText) + fm.horizontalAdvance("...") > iTextWidth))
#else
    while ((strShortText.size() > 1) && (fm.width(strShortText) + fm.width("...") > iTextWidth))
#endif
        strShortText.truncate(strShortText.size() - 1);
    if (strShortText != strText)
        strShortText += "...";
    pPainter->setFont(font);
    pPainter->drawText(textPosition, strShortText);

    /* Draw Controller Additions: */
    if (pModel->data(index, StorageModel::R_IsController).toBool() && enmState & QStyle::State_Selected)
    {
        DeviceTypeList devicesList(pModel->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            QPixmap devicePixmap;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = pModel->data(index, StorageModel::R_HDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_HDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_HDPixmapAddDis).value<QPixmap>();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = pModel->data(index, StorageModel::R_CDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_CDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_CDPixmapAddDis).value<QPixmap>();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = pModel->data(index, StorageModel::R_FDPixmapRect).value<QRect>();
                    devicePixmap = pModel->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool() ?
                                   pModel->data(index, StorageModel::R_FDPixmapAddEn).value<QPixmap>() :
                                   pModel->data(index, StorageModel::R_FDPixmapAddDis).value<QPixmap>();
                    break;
                }
                default:
                    break;
            }

            pPainter->drawPixmap(QPoint(rect.width() + deviceRect.x(), deviceRect.y()), devicePixmap);
        }
    }

    pPainter->restore();

    drawFocus(pPainter, option, rect);
}


/*********************************************************************************************************************************
*   Class UIWidgetOfMaximumSize implementation.                                                                                  *
*********************************************************************************************************************************/

UIWidgetOfMaximumSize::UIWidgetOfMaximumSize(QWidget *pParent /* = 0 */)
    : QWidget(pParent)
{
}

QSize UIWidgetOfMaximumSize::minimumSizeHint() const
{
    /* Assume there is just one grid-layout: */
    QList<QGridLayout*> gridLayouts = findChildren<QGridLayout*>();
    AssertReturn(gridLayouts.size() == 1, QWidget::minimumSizeHint());
    QGridLayout *pLayout = gridLayouts.first();
    AssertPtrReturn(pLayout, QWidget::minimumSizeHint());

    /* Acquire layout spacing: */
    const int iSpacing = pLayout->spacing();

    /* Calcualte column widths: */
    QList<int> columns(pLayout->columnCount());
    for (int iColumn = 0; iColumn < pLayout->columnCount(); ++iColumn)
        for (int iRow = 0; iRow < pLayout->rowCount(); ++iRow)
        {
            QLayoutItem *pItem = pLayout->itemAtPosition(iRow, iColumn);
            if (!pItem)
                continue;
            if (QWidget *pWidget = pItem->widget())
            {
                const QSizePolicy sp = pWidget->sizePolicy();
                if (sp.horizontalPolicy() != QSizePolicy::Ignored)
                    columns[iColumn] = qMax(columns.at(iColumn), pWidget->minimumSizeHint().width());
            }
        }

    /* Calcualte row heights: */
    QList<int> rows(pLayout->rowCount());
    for (int iRow = 0; iRow < pLayout->rowCount(); ++iRow)
        for (int iColumn = 0; iColumn < pLayout->columnCount(); ++iColumn)
        {
            QLayoutItem *pItem = pLayout->itemAtPosition(iRow, iColumn);
            if (!pItem)
                continue;
            if (QWidget *pWidget = pItem->widget())
            {
                const QSizePolicy sp = pWidget->sizePolicy();
                if (sp.verticalPolicy() != QSizePolicy::Ignored)
                    rows[iRow] = qMax(rows.at(iRow), pWidget->minimumSizeHint().height());
            }
        }

    /* Calculate effective layout size: */
    int iTotalWidth = 0;
    for (int iColumn = 0; iColumn < columns.size(); ++iColumn)
        iTotalWidth += columns.at(iColumn) + iSpacing;
    int iTotalHeight = 0;
    for (int iRow = 0; iRow < rows.size(); ++iRow)
        iTotalHeight += rows.at(iRow) + iSpacing;
    return QSize(iTotalWidth, iTotalHeight);
}


/*********************************************************************************************************************************
*   Class UIStorageSettingsEditor implementation.                                                                                *
*********************************************************************************************************************************/

/* static */
const QString UIStorageSettingsEditor::s_strControllerMimeType = QString("application/virtualbox;value=StorageControllerID");
const QString UIStorageSettingsEditor::s_strAttachmentMimeType = QString("application/virtualbox;value=StorageAttachmentID");

UIStorageSettingsEditor::UIStorageSettingsEditor(QWidget *pParent /* = 0 */)
    : UIEditor(pParent, true /* show in basic mode? */)
    , m_fLoadingInProgress(0)
    , m_enmConfigurationAccessLevel(ConfigurationAccessLevel_Null)
    , m_pActionPool(0)
    , m_pModelStorage(0)
    , m_pMediumIdHolder(new UIMediumIDHolder(this))
    , m_pSplitter(0)
    , m_pWidgetLeftPane(0)
    , m_pLabelSeparatorLeftPane(0)
    , m_pLayoutTree(0)
    , m_pTreeViewStorage(0)
    , m_pLayoutToolbar(0)
    , m_pToolbar(0)
    , m_pActionAddController(0)
    , m_pActionRemoveController(0)
    , m_pActionAddAttachment(0)
    , m_pActionRemoveAttachment(0)
    , m_pActionAddAttachmentHD(0)
    , m_pActionAddAttachmentCD(0)
    , m_pActionAddAttachmentFD(0)
    , m_pStackRightPane(0)
    , m_pLabelSeparatorEmpty(0)
    , m_pLabelInfo(0)
    , m_pLabelSeparatorParameters(0)
    , m_pLabelName(0)
    , m_pEditorName(0)
    , m_pLabelType(0)
    , m_pComboType(0)
    , m_pLabelPortCount(0)
    , m_pSpinboxPortCount(0)
    , m_pCheckBoxIoCache(0)
    , m_pLabelSeparatorAttributes(0)
    , m_pLabelMedium(0)
    , m_pComboSlot(0)
    , m_pToolButtonOpen(0)
    , m_pCheckBoxPassthrough(0)
    , m_pCheckBoxTempEject(0)
    , m_pCheckBoxNonRotational(0)
    , m_pCheckBoxHotPluggable(0)
    , m_pLabelSeparatorInformation(0)
    , m_pLabelHDFormat(0)
    , m_pFieldHDFormat(0)
    , m_pLabelCDFDType(0)
    , m_pFieldCDFDType(0)
    , m_pLabelHDVirtualSize(0)
    , m_pFieldHDVirtualSize(0)
    , m_pLabelHDActualSize(0)
    , m_pFieldHDActualSize(0)
    , m_pLabelCDFDSize(0)
    , m_pFieldCDFDSize(0)
    , m_pLabelHDDetails(0)
    , m_pFieldHDDetails(0)
    , m_pLabelLocation(0)
    , m_pFieldLocation(0)
    , m_pLabelUsage(0)
    , m_pFieldUsage(0)
    , m_pLabelEncryption(0)
    , m_pFieldEncryption(0)
{
    prepare();
}

UIStorageSettingsEditor::~UIStorageSettingsEditor()
{
    cleanup();
}

void UIStorageSettingsEditor::setActionPool(UIActionPool *pActionPool)
{
    m_pActionPool = pActionPool;
}

void UIStorageSettingsEditor::setMachineId(const QUuid &uMachineId)
{
    m_uMachineId = uMachineId;
    if (m_pModelStorage)
        m_pModelStorage->setMachineId(uMachineId);
}

void UIStorageSettingsEditor::setMachineName(const QString &strName)
{
    m_strMachineName = strName;
}

void UIStorageSettingsEditor::setMachineSettingsFilePath(const QString &strFilePath)
{
    m_strMachineSettingsFilePath = strFilePath;
}

void UIStorageSettingsEditor::setMachineGuestOSTypeId(const QString &strId)
{
    m_strMachineGuestOSTypeId = strId;
}

void UIStorageSettingsEditor::setConfigurationAccessLevel(ConfigurationAccessLevel enmConfigurationAccessLevel)
{
    /* Check whether update is required: */
    if (m_enmConfigurationAccessLevel != enmConfigurationAccessLevel)
    {
        /* Update value and let model know: */
        m_enmConfigurationAccessLevel = enmConfigurationAccessLevel;
        if (m_pModelStorage)
            m_pModelStorage->setConfigurationAccessLevel(enmConfigurationAccessLevel);

        /* Check actual level: */
        const bool fMachineOffline = m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full;
        const bool fMachinePoweredOff = m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_PoweredOff;
        const bool fMachineSaved = m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_Saved;
        const bool fMachineOnline = m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_Running;
        const bool fMachineInValidMode = fMachineOffline || fMachinePoweredOff || fMachineSaved || fMachineOnline;

        /* Declare required variables: */
        const QModelIndex index = m_pTreeViewStorage->currentIndex();
        const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();

        /* Polish left pane availability: */
        m_pLabelSeparatorLeftPane->setEnabled(fMachineInValidMode);
        m_pTreeViewStorage->setEnabled(fMachineInValidMode);

        /* Polish empty information pane availability: */
        m_pLabelSeparatorEmpty->setEnabled(fMachineInValidMode);
        m_pLabelInfo->setEnabled(fMachineInValidMode);

        /* Polish controllers pane availability: */
        m_pLabelSeparatorParameters->setEnabled(fMachineInValidMode);
        m_pLabelName->setEnabled(fMachineOffline);
        m_pEditorName->setEnabled(fMachineOffline);
        m_pLabelType->setEnabled(fMachineOffline);
        m_pComboType->setEnabled(fMachineOffline);
        m_pLabelPortCount->setEnabled(fMachineOffline);
        m_pSpinboxPortCount->setEnabled(fMachineOffline);
        m_pCheckBoxIoCache->setEnabled(fMachineOffline);

        /* Polish attachments pane availability: */
        m_pLabelSeparatorAttributes->setEnabled(fMachineInValidMode);
        m_pLabelMedium->setEnabled(fMachineOffline || (fMachineOnline && enmDeviceType != KDeviceType_HardDisk));
        m_pComboSlot->setEnabled(fMachineOffline);
        m_pToolButtonOpen->setEnabled(fMachineOffline || (fMachineOnline && enmDeviceType != KDeviceType_HardDisk));
        m_pCheckBoxPassthrough->setEnabled(fMachineOffline);
        m_pCheckBoxTempEject->setEnabled(fMachineInValidMode);
        m_pCheckBoxNonRotational->setEnabled(fMachineOffline);
        m_pCheckBoxHotPluggable->setEnabled(fMachineOffline);
        m_pLabelSeparatorInformation->setEnabled(fMachineInValidMode);
        m_pLabelHDFormat->setEnabled(fMachineInValidMode);
        m_pFieldHDFormat->setEnabled(fMachineInValidMode);
        m_pLabelCDFDType->setEnabled(fMachineInValidMode);
        m_pFieldCDFDType->setEnabled(fMachineInValidMode);
        m_pLabelHDVirtualSize->setEnabled(fMachineInValidMode);
        m_pFieldHDVirtualSize->setEnabled(fMachineInValidMode);
        m_pLabelHDActualSize->setEnabled(fMachineInValidMode);
        m_pFieldHDActualSize->setEnabled(fMachineInValidMode);
        m_pLabelCDFDSize->setEnabled(fMachineInValidMode);
        m_pFieldCDFDSize->setEnabled(fMachineInValidMode);
        m_pLabelHDDetails->setEnabled(fMachineInValidMode);
        m_pFieldHDDetails->setEnabled(fMachineInValidMode);
        m_pLabelLocation->setEnabled(fMachineInValidMode);
        m_pFieldLocation->setEnabled(fMachineInValidMode);
        m_pLabelUsage->setEnabled(fMachineInValidMode);
        m_pFieldUsage->setEnabled(fMachineInValidMode);
        m_pLabelEncryption->setEnabled(fMachineInValidMode);
        m_pFieldEncryption->setEnabled(fMachineInValidMode);

        /* Update remaining stuff: */
        sltUpdateActionStates();
        sltGetInformation();
    }
}

void UIStorageSettingsEditor::setChipsetType(KChipsetType enmType)
{
    if (m_pModelStorage)
    {
        /* Make sure chipset type has changed: */
        if (m_pModelStorage->chipsetType() != enmType)
        {
            /* Update chipset type value: */
            m_pModelStorage->setChipsetType(enmType);
            sltUpdateActionStates();

            /* Notify listeners: */
            emit sigValueChanged();
        }
    }
}

KChipsetType UIStorageSettingsEditor::chipsetType() const
{
    return m_pModelStorage ? m_pModelStorage->chipsetType() : KChipsetType_Null;
}

QMap<KStorageBus, int> UIStorageSettingsEditor::currentControllerTypes() const
{
    return m_pModelStorage ? m_pModelStorage->currentControllerTypes() : QMap<KStorageBus, int>();
}

QMap<KStorageBus, int> UIStorageSettingsEditor::maximumControllerTypes() const
{
    return m_pModelStorage ? m_pModelStorage->maximumControllerTypes() : QMap<KStorageBus, int>();
}

void UIStorageSettingsEditor::setValue(const QList<UIDataStorageController> &controllers,
                                       const QList<QList<UIDataStorageAttachment> > &attachments)
{
    /* Clear model initially: */
    m_pModelStorage->clear();

    /* For each controller: */
    for (int iControllerIndex = 0; iControllerIndex < controllers.size(); ++iControllerIndex)
    {
        /* Get old data from cache: */
        const UIDataStorageController &oldControllerData = controllers.at(iControllerIndex);

        /* Load old data from cache: */
        const QModelIndex controllerIndex = m_pModelStorage->addController(oldControllerData.m_strName,
                                                                           oldControllerData.m_enmBus,
                                                                           oldControllerData.m_enmType);
        const QUuid controllerId = QUuid(m_pModelStorage->data(controllerIndex, StorageModel::R_ItemId).toString());
        m_pModelStorage->setData(controllerIndex, oldControllerData.m_uPortCount, StorageModel::R_CtrPortCount);
        m_pModelStorage->setData(controllerIndex, oldControllerData.m_fUseHostIOCache, StorageModel::R_CtrIoCache);

        /* For each attachment: */
        const QList<UIDataStorageAttachment> &controllerAttachments = attachments.at(iControllerIndex);
        for (int iAttachmentIndex = 0; iAttachmentIndex < controllerAttachments.size(); ++iAttachmentIndex)
        {
            /* Get old data from cache: */
            const UIDataStorageAttachment &oldAttachmentData = controllerAttachments.at(iAttachmentIndex);

            /* Load old data from cache: */
            const QModelIndex attachmentIndex = m_pModelStorage->addAttachment(controllerId,
                                                                               oldAttachmentData.m_enmDeviceType,
                                                                               oldAttachmentData.m_uMediumId);
            const StorageSlot attachmentStorageSlot(oldControllerData.m_enmBus,
                                                    oldAttachmentData.m_iPort,
                                                    oldAttachmentData.m_iDevice);
            m_pModelStorage->setData(attachmentIndex, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fPassthrough, StorageModel::R_AttIsPassthrough);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fTempEject, StorageModel::R_AttIsTempEject);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fNonRotational, StorageModel::R_AttIsNonRotational);
            m_pModelStorage->setData(attachmentIndex, oldAttachmentData.m_fHotPluggable, StorageModel::R_AttIsHotPluggable);
        }
    }

    /* Choose first controller as current: */
    if (m_pModelStorage->rowCount(m_pModelStorage->root()) > 0)
        m_pTreeViewStorage->setCurrentIndex(m_pModelStorage->index(0, 0, m_pModelStorage->root()));

    /* Fetch recent information: */
    sltHandleCurrentItemChange();
}

void UIStorageSettingsEditor::getValue(QList<UIDataStorageController> &controllers,
                                       QList<QList<UIDataStorageAttachment> > &attachments)
{
    /* For each controller: */
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int iControllerIndex = 0; iControllerIndex < m_pModelStorage->rowCount(rootIndex); ++iControllerIndex)
    {
        /* Prepare new data & key: */
        UIDataStorageController newControllerData;

        /* Gather new data & cache key from model: */
        const QModelIndex controllerIndex = m_pModelStorage->index(iControllerIndex, 0, rootIndex);
        newControllerData.m_strName = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrName).toString();
        newControllerData.m_enmBus = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrBusType).value<KStorageBus>();
        newControllerData.m_enmType = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrType).value<KStorageControllerType>();
        newControllerData.m_uPortCount = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrPortCount).toUInt();
        newControllerData.m_fUseHostIOCache = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrIoCache).toBool();
        newControllerData.m_strKey = newControllerData.m_strName;

        /* For each attachment: */
        QList<UIDataStorageAttachment> controllerAttachments;
        for (int iAttachmentIndex = 0; iAttachmentIndex < m_pModelStorage->rowCount(controllerIndex); ++iAttachmentIndex)
        {
            /* Prepare new data & key: */
            UIDataStorageAttachment newAttachmentData;

            /* Gather new data & cache key from model: */
            const QModelIndex attachmentIndex = m_pModelStorage->index(iAttachmentIndex, 0, controllerIndex);
            newAttachmentData.m_enmDeviceType = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            const StorageSlot attachmentSlot = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttSlot).value<StorageSlot>();
            newAttachmentData.m_iPort = attachmentSlot.port;
            newAttachmentData.m_iDevice = attachmentSlot.device;
            newAttachmentData.m_fPassthrough = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsPassthrough).toBool();
            newAttachmentData.m_fTempEject = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsTempEject).toBool();
            newAttachmentData.m_fNonRotational = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsNonRotational).toBool();
            newAttachmentData.m_fHotPluggable = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttIsHotPluggable).toBool();
            newAttachmentData.m_uMediumId = QUuid(m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString());
            newAttachmentData.m_strKey = QString("%1:%2").arg(newAttachmentData.m_iPort)
                                                         .arg(newAttachmentData.m_iDevice);

            /* Cache new data: */
            controllerAttachments << newAttachmentData;
        }

        /* Cache new data: */
        controllers << newControllerData;
        attachments << controllerAttachments;
    }
}

void UIStorageSettingsEditor::retranslateUi()
{
    m_pLabelSeparatorLeftPane->setText(tr("&Storage Devices"));
    m_pLabelSeparatorEmpty->setText(tr("Information"));
    m_pLabelInfo->setText(tr("The Storage Tree can contain several controllers of different types. This machine currently has no "
                             "controllers."));
    m_pLabelSeparatorParameters->setText(tr("Attributes"));
    m_pLabelName->setText(tr("&Name:"));
    m_pEditorName->setToolTip(tr("Holds the name of the storage controller currently selected in the Storage Tree."));
    m_pLabelType->setText(tr("&Type:"));
    m_pComboType->setToolTip(tr("Selects the sub-type of the storage controller currently selected in the Storage Tree."));
    m_pLabelPortCount->setText(tr("&Port Count:"));
    m_pSpinboxPortCount->setToolTip(tr("Selects the port count of the SATA storage controller currently selected in the "
                                       "Storage Tree. This must be at least one more than the highest port number you need to "
                                       "use."));
    m_pCheckBoxIoCache->setToolTip(tr("When checked, allows to use host I/O caching capabilities."));
    m_pCheckBoxIoCache->setText(tr("Use Host I/O Cache"));
    m_pLabelSeparatorAttributes->setText(tr("Attributes"));
    m_pComboSlot->setToolTip(tr("Selects the slot on the storage controller used by this attachment. The available slots depend "
                                "on the type of the controller and other attachments on it."));
    m_pToolButtonOpen->setText(QString());
    m_pCheckBoxPassthrough->setToolTip(tr("When checked, allows the guest to send ATAPI commands directly to the host-drive "
                                          "which makes it possible to use CD/DVD writers connected to the host inside the VM. "
                                          "Note that writing audio CD inside the VM is not yet supported."));
    m_pCheckBoxPassthrough->setText(tr("&Passthrough"));
    m_pCheckBoxTempEject->setToolTip(tr("When checked, the virtual disk will not be removed when the guest system ejects it."));
    m_pCheckBoxTempEject->setText(tr("&Live CD/DVD"));
    m_pCheckBoxNonRotational->setToolTip(tr("When checked, the guest system will see the virtual disk as a solid-state device."));
    m_pCheckBoxNonRotational->setText(tr("&Solid-state Drive"));
    m_pCheckBoxHotPluggable->setToolTip(tr("When checked, the guest system will see the virtual disk as a hot-pluggable device."));
    m_pCheckBoxHotPluggable->setText(tr("&Hot-pluggable"));
    m_pLabelSeparatorInformation->setText(tr("Information"));
    m_pLabelHDFormat->setText(tr("Type (Format):"));
    m_pLabelCDFDType->setText(tr("Type:"));
    m_pLabelHDVirtualSize->setText(tr("Virtual Size:"));
    m_pLabelHDActualSize->setText(tr("Actual Size:"));
    m_pLabelCDFDSize->setText(tr("Size:"));
    m_pLabelHDDetails->setText(tr("Details:"));
    m_pLabelLocation->setText(tr("Location:"));
    m_pLabelUsage->setText(tr("Attached to:"));
    m_pLabelEncryption->setText(tr("Encryption key:"));

    /* Translate storage-view: */
    m_pTreeViewStorage->setWhatsThis(tr("Lists all storage controllers for this machine and "
                                        "the virtual images and host drives attached to them."));

    /* Translate tool-bar: */
    m_pActionAddController->setShortcut(QKeySequence("Ins"));
    m_pActionRemoveController->setShortcut(QKeySequence("Del"));
    m_pActionAddAttachment->setShortcut(QKeySequence("+"));
    m_pActionRemoveAttachment->setShortcut(QKeySequence("-"));

    m_pActionAddController->setText(tr("Add Controller"));
    m_addControllerActions.value(KStorageControllerType_PIIX3)->setText(tr("PIIX3 (IDE)"));
    m_addControllerActions.value(KStorageControllerType_PIIX4)->setText(tr("PIIX4 (Default IDE)"));
    m_addControllerActions.value(KStorageControllerType_ICH6)->setText(tr("ICH6 (IDE)"));
    m_addControllerActions.value(KStorageControllerType_IntelAhci)->setText(tr("AHCI (SATA)"));
    m_addControllerActions.value(KStorageControllerType_LsiLogic)->setText(tr("LsiLogic (Default SCSI)"));
    m_addControllerActions.value(KStorageControllerType_BusLogic)->setText(tr("BusLogic (SCSI)"));
    m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setText(tr("LsiLogic SAS (SAS)"));
    m_addControllerActions.value(KStorageControllerType_I82078)->setText(tr("I82078 (Floppy)"));
    m_addControllerActions.value(KStorageControllerType_USB)->setText(tr("USB"));
    m_addControllerActions.value(KStorageControllerType_NVMe)->setText(tr("NVMe (PCIe)"));
    m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setText(tr("virtio-scsi"));
    m_pActionRemoveController->setText(tr("Remove Controller"));
    m_pActionAddAttachment->setText(tr("Add Attachment"));
    m_pActionAddAttachmentHD->setText(tr("Hard Disk"));
    m_pActionAddAttachmentCD->setText(tr("Optical Drive"));
    m_pActionAddAttachmentFD->setText(tr("Floppy Drive"));
    m_pActionRemoveAttachment->setText(tr("Remove Attachment"));

    m_pActionAddController->setToolTip(tr("Adds new storage controller."));
    m_pActionRemoveController->setToolTip(tr("Removes selected storage controller."));
    m_pActionAddAttachment->setToolTip(tr("Adds new storage attachment."));
    m_pActionRemoveAttachment->setToolTip(tr("Removes selected storage attachment."));

    m_pActionAddController->setToolTip(m_pActionAddController->whatsThis());
    m_pActionRemoveController->setToolTip(m_pActionRemoveController->whatsThis());
    m_pActionAddAttachment->setToolTip(m_pActionAddAttachment->whatsThis());
    m_pActionRemoveAttachment->setToolTip(m_pActionRemoveAttachment->whatsThis());
}

void UIStorageSettingsEditor::showEvent(QShowEvent *pEvent)
{
    /* Turn splitter back to sane proportions: */
    m_pSplitter->setSizes(QList<int>() << 0.4 * width() << 0.6 * width());

    /* Call to base-class: */
    UIEditor::showEvent(pEvent);
}

void UIStorageSettingsEditor::sltHandleMediumEnumerated(const QUuid &uMediumId)
{
    /* Search for corresponding medium: */
    const UIMedium medium = uiCommon().medium(uMediumId);

    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = m_pModelStorage->index(i, 0, rootIndex);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            const QModelIndex attachmentIndex = m_pModelStorage->index(j, 0, controllerIndex);
            const QUuid attMediumId(m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString());
            if (attMediumId == medium.id())
            {
                m_pModelStorage->setData(attachmentIndex, attMediumId, StorageModel::R_AttMediumId);

                /* Notify listeners: */
                emit sigValueChanged();
            }
        }
    }
}

void UIStorageSettingsEditor::sltHandleMediumDeleted(const QUuid &uMediumId)
{
    QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        QModelIndex controllerIndex = m_pModelStorage->index(i, 0, rootIndex);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            QModelIndex attachmentIndex = m_pModelStorage->index(j, 0, controllerIndex);
            QUuid attMediumId(m_pModelStorage->data(attachmentIndex, StorageModel::R_AttMediumId).toString());
            if (attMediumId == uMediumId)
            {
                m_pModelStorage->setData(attachmentIndex, UIMedium().id(), StorageModel::R_AttMediumId);

                /* Notify listeners: */
                emit sigValueChanged();
            }
        }
    }
}

void UIStorageSettingsEditor::sltAddController()
{
    /* Load currently supported storage buses and types: */
    CPlatformProperties comProperties = uiCommon().virtualBox().GetPlatformProperties(KPlatformArchitecture_x86);
    const QVector<KStorageBus> supportedBuses = comProperties.GetSupportedStorageBuses();
    const QVector<KStorageControllerType> supportedTypes = comProperties.GetSupportedStorageControllerTypes();

    /* Prepare menu: */
    QMenu menu;
    foreach (const KStorageControllerType &enmType, supportedTypes)
    {
        QAction *pAction = m_addControllerActions.value(enmType);
        if (supportedBuses.contains(comProperties.GetStorageBusForControllerType(enmType)))
            menu.addAction(pAction);
    }

    /* Popup it finally: */
    menu.exec(QCursor::pos());
}

void UIStorageSettingsEditor::sltAddControllerPIIX3()
{
    addControllerWrapper(generateUniqueControllerName("PIIX3"), KStorageBus_IDE, KStorageControllerType_PIIX3);
}

void UIStorageSettingsEditor::sltAddControllerPIIX4()
{
    addControllerWrapper(generateUniqueControllerName("PIIX4"), KStorageBus_IDE, KStorageControllerType_PIIX4);
}

void UIStorageSettingsEditor::sltAddControllerICH6()
{
    addControllerWrapper(generateUniqueControllerName("ICH6"), KStorageBus_IDE, KStorageControllerType_ICH6);
}

void UIStorageSettingsEditor::sltAddControllerAHCI()
{
    addControllerWrapper(generateUniqueControllerName("AHCI"), KStorageBus_SATA, KStorageControllerType_IntelAhci);
}

void UIStorageSettingsEditor::sltAddControllerLsiLogic()
{
    addControllerWrapper(generateUniqueControllerName("LsiLogic"), KStorageBus_SCSI, KStorageControllerType_LsiLogic);
}

void UIStorageSettingsEditor::sltAddControllerBusLogic()
{
    addControllerWrapper(generateUniqueControllerName("BusLogic"), KStorageBus_SCSI, KStorageControllerType_BusLogic);
}

void UIStorageSettingsEditor::sltAddControllerFloppy()
{
    addControllerWrapper(generateUniqueControllerName("Floppy"), KStorageBus_Floppy, KStorageControllerType_I82078);
}

void UIStorageSettingsEditor::sltAddControllerLsiLogicSAS()
{
    addControllerWrapper(generateUniqueControllerName("LsiLogic SAS"), KStorageBus_SAS, KStorageControllerType_LsiLogicSas);
}

void UIStorageSettingsEditor::sltAddControllerUSB()
{
    addControllerWrapper(generateUniqueControllerName("USB"), KStorageBus_USB, KStorageControllerType_USB);
}

void UIStorageSettingsEditor::sltAddControllerNVMe()
{
    addControllerWrapper(generateUniqueControllerName("NVMe"), KStorageBus_PCIe, KStorageControllerType_NVMe);
}

void UIStorageSettingsEditor::sltAddControllerVirtioSCSI()
{
    addControllerWrapper(generateUniqueControllerName("VirtIO"), KStorageBus_VirtioSCSI, KStorageControllerType_VirtioSCSI);
}

void UIStorageSettingsEditor::sltRemoveController()
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    if (!m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
        return;

    m_pModelStorage->delController(QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()));

    /* Notify listeners: */
    emit sigValueChanged();
}

void UIStorageSettingsEditor::sltAddAttachment()
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    Assert(m_pModelStorage->data(index, StorageModel::R_IsController).toBool());

    const DeviceTypeList deviceTypeList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
    const bool fJustTrigger = deviceTypeList.size() == 1;
    const bool fShowMenu = deviceTypeList.size() > 1;
    QMenu menu;
    foreach (const KDeviceType &enmDeviceType, deviceTypeList)
    {
        switch (enmDeviceType)
        {
            case KDeviceType_HardDisk:
                if (fJustTrigger)
                    m_pActionAddAttachmentHD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentHD);
                break;
            case KDeviceType_DVD:
                if (fJustTrigger)
                    m_pActionAddAttachmentCD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentCD);
                break;
            case KDeviceType_Floppy:
                if (fJustTrigger)
                    m_pActionAddAttachmentFD->trigger();
                if (fShowMenu)
                    menu.addAction(m_pActionAddAttachmentFD);
                break;
            default:
                break;
        }
    }
    if (fShowMenu)
        menu.exec(QCursor::pos());
}

void UIStorageSettingsEditor::sltAddAttachmentHD()
{
    addAttachmentWrapper(KDeviceType_HardDisk);
}

void UIStorageSettingsEditor::sltAddAttachmentCD()
{
    addAttachmentWrapper(KDeviceType_DVD);
}

void UIStorageSettingsEditor::sltAddAttachmentFD()
{
    addAttachmentWrapper(KDeviceType_Floppy);
}

void UIStorageSettingsEditor::sltRemoveAttachment()
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();

    const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();
    /* Check if this would be the last DVD. If so let the user confirm this again. */
    if (   enmDeviceType == KDeviceType_DVD
        && deviceCount(KDeviceType_DVD) == 1)
    {
        if (!msgCenter().confirmRemovingOfLastDVDDevice(this))
            return;
    }

    const QModelIndex parentIndex = index.parent();
    if (!index.isValid() || !parentIndex.isValid() ||
        !m_pModelStorage->data(index, StorageModel::R_IsAttachment).toBool() ||
        !m_pModelStorage->data(parentIndex, StorageModel::R_IsController).toBool())
        return;

    m_pModelStorage->delAttachment(QUuid(m_pModelStorage->data(parentIndex, StorageModel::R_ItemId).toString()),
                                   QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()));

    /* Notify listeners: */
    emit sigValueChanged();
}

void UIStorageSettingsEditor::sltGetInformation()
{
    m_fLoadingInProgress = true;

    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    if (!index.isValid() || index == m_pModelStorage->root())
    {
        /* Showing Initial Page: */
        m_pStackRightPane->setCurrentIndex(0);
    }
    else
    {
        switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
        {
            case AbstractItem::Type_ControllerItem:
            {
                /* Getting Controller Name: */
                const QString strCtrName = m_pModelStorage->data(index, StorageModel::R_CtrName).toString();
                if (m_pEditorName->text() != strCtrName)
                    m_pEditorName->setText(strCtrName);

                /* Rebuild type combo: */
                m_pComboType->clear();
                /* Getting controller buses: */
                const ControllerBusList controllerBusList(m_pModelStorage->data(index, StorageModel::R_CtrBusTypes).value<ControllerBusList>());
                foreach (const KStorageBus &enmCurrentBus, controllerBusList)
                {
                    /* Getting controller types: */
                    const ControllerTypeList controllerTypeList(m_pModelStorage->data(index, m_pModelStorage->busToRole(enmCurrentBus)).value<ControllerTypeList>());
                    foreach (const KStorageControllerType &enmCurrentType, controllerTypeList)
                    {
                        m_pComboType->addItem(gpConverter->toString(enmCurrentType));
                        m_pComboType->setItemData(m_pComboType->count() - 1, QVariant::fromValue(enmCurrentBus), StorageModel::R_CtrBusType);
                        m_pComboType->setItemData(m_pComboType->count() - 1, QVariant::fromValue(enmCurrentType), StorageModel::R_CtrType);
                    }
                }
                const KStorageControllerType enmType = m_pModelStorage->data(index, StorageModel::R_CtrType).value<KStorageControllerType>();
                const int iCtrPos = m_pComboType->findData(QVariant::fromValue(enmType), StorageModel::R_CtrType);
                m_pComboType->setCurrentIndex(iCtrPos == -1 ? 0 : iCtrPos);

                const KStorageBus enmBus = m_pModelStorage->data(index, StorageModel::R_CtrBusType).value<KStorageBus>();
                m_pLabelPortCount->setVisible(enmBus == KStorageBus_SATA || enmBus == KStorageBus_SAS);
                m_pSpinboxPortCount->setVisible(enmBus == KStorageBus_SATA || enmBus == KStorageBus_SAS);
                const uint uPortCount = m_pModelStorage->data(index, StorageModel::R_CtrPortCount).toUInt();
                const uint uMaxPortCount = m_pModelStorage->data(index, StorageModel::R_CtrMaxPortCount).toUInt();
                m_pSpinboxPortCount->setMaximum(uMaxPortCount);
                m_pSpinboxPortCount->setValue(uPortCount);

                const bool fUseIoCache = m_pModelStorage->data(index, StorageModel::R_CtrIoCache).toBool();
                m_pCheckBoxIoCache->setChecked(fUseIoCache);

                /* Showing Controller Page: */
                m_pStackRightPane->setCurrentIndex(1);
                break;
            }
            case AbstractItem::Type_AttachmentItem:
            {
                /* Getting Attachment Slot: */
                m_pComboSlot->clear();
                const SlotsList slotsList(m_pModelStorage->data(index, StorageModel::R_AttSlots).value<SlotsList>());
                for (int i = 0; i < slotsList.size(); ++i)
                    m_pComboSlot->insertItem(m_pComboSlot->count(), gpConverter->toString(slotsList[i]));
                const StorageSlot slt = m_pModelStorage->data(index, StorageModel::R_AttSlot).value<StorageSlot>();
                const int iAttSlotPos = m_pComboSlot->findText(gpConverter->toString(slt));
                m_pComboSlot->setCurrentIndex(iAttSlotPos == -1 ? 0 : iAttSlotPos);
                m_pComboSlot->setToolTip(m_pComboSlot->itemText(m_pComboSlot->currentIndex()));

                /* Getting Attachment Medium: */
                const KDeviceType enmDeviceType = m_pModelStorage->data(index, StorageModel::R_AttDevice).value<KDeviceType>();
                switch (enmDeviceType)
                {
                    case KDeviceType_HardDisk:
                        m_pLabelMedium->setText(tr("Hard &Disk:"));
                        m_pToolButtonOpen->setIcon(iconPool()->icon(PixmapType_HDAttachmentNormal));
                        m_pToolButtonOpen->setToolTip(tr("Choose or create a virtual hard disk file. The virtual machine will "
                                                         "see the data in the file as the contents of the virtual hard disk."));
                        break;
                    case KDeviceType_DVD:
                        m_pLabelMedium->setText(tr("Optical &Drive:"));
                        m_pToolButtonOpen->setIcon(iconPool()->icon(PixmapType_CDAttachmentNormal));
                        m_pToolButtonOpen->setToolTip(tr("Choose a virtual optical disk or a physical drive to use with the "
                                                         "virtual drive. The virtual machine will see a disk inserted into the "
                                                         "drive with the data in the file or on the disk in the physical drive "
                                                         "as its contents."));
                        break;
                    case KDeviceType_Floppy:
                        m_pLabelMedium->setText(tr("Floppy &Drive:"));
                        m_pToolButtonOpen->setIcon(iconPool()->icon(PixmapType_FDAttachmentNormal));
                        m_pToolButtonOpen->setToolTip(tr("Choose a virtual floppy disk or a physical drive to use with the "
                                                         "virtual drive. The virtual machine will see a disk inserted into the "
                                                         "drive with the data in the file or on the disk in the physical drive "
                                                         "as its contents."));
                        break;
                    default:
                        break;
                }

                /* Get hot-pluggable state: */
                const bool fIsHotPluggable = m_pModelStorage->data(index, StorageModel::R_AttIsHotPluggable).toBool();

                /* Fetch device-type, medium-id: */
                m_pMediumIdHolder->setType(mediumTypeToLocal(enmDeviceType));
                m_pMediumIdHolder->setId(QUuid(m_pModelStorage->data(index, StorageModel::R_AttMediumId).toString()));

                /* Get/fetch editable state: */
                const bool fIsEditable =    (m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full)
                                         || (   m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_Running
                                             && enmDeviceType != KDeviceType_HardDisk)
                                         || (   m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_Running
                                             && enmDeviceType == KDeviceType_HardDisk && fIsHotPluggable);
                m_pLabelMedium->setEnabled(fIsEditable);
                m_pToolButtonOpen->setEnabled(fIsEditable);

                /* Getting Passthrough state: */
                const bool fHostDrive = m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool();
                m_pCheckBoxPassthrough->setVisible(enmDeviceType == KDeviceType_DVD && fHostDrive);
                m_pCheckBoxPassthrough->setChecked(fHostDrive && m_pModelStorage->data(index, StorageModel::R_AttIsPassthrough).toBool());

                /* Getting TempEject state: */
                m_pCheckBoxTempEject->setVisible(enmDeviceType == KDeviceType_DVD && !fHostDrive);
                m_pCheckBoxTempEject->setChecked(!fHostDrive && m_pModelStorage->data(index, StorageModel::R_AttIsTempEject).toBool());

                /* Getting NonRotational state: */
                m_pCheckBoxNonRotational->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pCheckBoxNonRotational->setChecked(m_pModelStorage->data(index, StorageModel::R_AttIsNonRotational).toBool());

                /* Fetch hot-pluggable state: */
                m_pCheckBoxHotPluggable->setVisible(slt.bus == KStorageBus_SATA);
                m_pCheckBoxHotPluggable->setChecked(fIsHotPluggable);

                /* Update optional widgets visibility: */
                m_pLabelHDFormat->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pFieldHDFormat->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pLabelCDFDType->setVisible(enmDeviceType != KDeviceType_HardDisk);
                m_pFieldCDFDType->setVisible(enmDeviceType != KDeviceType_HardDisk);
                m_pLabelHDVirtualSize->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pFieldHDVirtualSize->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pLabelHDActualSize->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pFieldHDActualSize->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pLabelCDFDSize->setVisible(enmDeviceType != KDeviceType_HardDisk);
                m_pFieldCDFDSize->setVisible(enmDeviceType != KDeviceType_HardDisk);
                m_pLabelHDDetails->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pFieldHDDetails->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pLabelEncryption->setVisible(enmDeviceType == KDeviceType_HardDisk);
                m_pFieldEncryption->setVisible(enmDeviceType == KDeviceType_HardDisk);

                /* Getting Other Information: */
                m_pFieldHDFormat->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttFormat).toString()));
                m_pFieldCDFDType->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttFormat).toString()));
                m_pFieldHDVirtualSize->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttLogicalSize).toString()));
                m_pFieldHDActualSize->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttSize).toString()));
                m_pFieldCDFDSize->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttSize).toString()));
                m_pFieldHDDetails->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttDetails).toString()));
                m_pFieldLocation->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttLocation).toString()));
                m_pFieldUsage->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttUsage).toString()));
                m_pFieldEncryption->setText(compressText(m_pModelStorage->data(index, StorageModel::R_AttEncryptionPasswordID).toString()));

                /* Showing Attachment Page: */
                m_pStackRightPane->setCurrentIndex(2);
                break;
            }
            default:
                break;
        }
    }

    /* Notify listeners: */
    emit sigValueChanged();

    m_fLoadingInProgress = false;
}

void UIStorageSettingsEditor::sltSetInformation()
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    if (m_fLoadingInProgress || !index.isValid() || index == m_pModelStorage->root())
        return;

    QObject *pSender = sender();
    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Setting Controller Name: */
            if (pSender == m_pEditorName)
                m_pModelStorage->setData(index, m_pEditorName->text(), StorageModel::R_CtrName);
            /* Setting Controller Sub-Type: */
            else if (pSender == m_pComboType)
            {
                const KStorageBus enmBus = m_pComboType->currentData(StorageModel::R_CtrBusType).value<KStorageBus>();
                const KStorageControllerType enmType = m_pComboType->currentData(StorageModel::R_CtrType).value<KStorageControllerType>();
                const bool fResult =
                    m_pModelStorage->setData(index, QVariant::fromValue(enmBus), StorageModel::R_CtrBusType);
                if (fResult)
                    m_pModelStorage->setData(index, QVariant::fromValue(enmType), StorageModel::R_CtrType);
            }
            else if (pSender == m_pSpinboxPortCount)
                m_pModelStorage->setData(index, m_pSpinboxPortCount->value(), StorageModel::R_CtrPortCount);
            else if (pSender == m_pCheckBoxIoCache)
                m_pModelStorage->setData(index, m_pCheckBoxIoCache->isChecked(), StorageModel::R_CtrIoCache);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Setting Attachment Slot: */
            if (pSender == m_pComboSlot)
            {
                QModelIndex controllerIndex = m_pModelStorage->parent(index);
                StorageSlot attachmentStorageSlot = gpConverter->fromString<StorageSlot>(m_pComboSlot->currentText());
                m_pModelStorage->setData(index, QVariant::fromValue(attachmentStorageSlot), StorageModel::R_AttSlot);
                QModelIndex theSameIndexAtNewPosition = m_pModelStorage->attachmentBySlot(controllerIndex, attachmentStorageSlot);
                AssertMsg(theSameIndexAtNewPosition.isValid(), ("Current attachment disappears!\n"));
                m_pTreeViewStorage->setCurrentIndex(theSameIndexAtNewPosition);
            }
            /* Setting Attachment Medium: */
            else if (pSender == m_pMediumIdHolder)
                m_pModelStorage->setData(index, m_pMediumIdHolder->id(), StorageModel::R_AttMediumId);
            else if (pSender == m_pCheckBoxPassthrough)
            {
                if (m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool())
                    m_pModelStorage->setData(index, m_pCheckBoxPassthrough->isChecked(), StorageModel::R_AttIsPassthrough);
            }
            else if (pSender == m_pCheckBoxTempEject)
            {
                if (!m_pModelStorage->data(index, StorageModel::R_AttIsHostDrive).toBool())
                    m_pModelStorage->setData(index, m_pCheckBoxTempEject->isChecked(), StorageModel::R_AttIsTempEject);
            }
            else if (pSender == m_pCheckBoxNonRotational)
            {
                m_pModelStorage->setData(index, m_pCheckBoxNonRotational->isChecked(), StorageModel::R_AttIsNonRotational);
            }
            else if (pSender == m_pCheckBoxHotPluggable)
            {
                m_pModelStorage->setData(index, m_pCheckBoxHotPluggable->isChecked(), StorageModel::R_AttIsHotPluggable);
            }
            break;
        }
        default:
            break;
    }

    emit sigValueChanged();
    sltUpdateActionStates();
    sltGetInformation();
}

void UIStorageSettingsEditor::sltPrepareOpenMediumMenu()
{
    /* This slot should be called only by open-medium menu: */
    QMenu *pOpenMediumMenu = qobject_cast<QMenu*>(sender());
    AssertMsg(pOpenMediumMenu, ("Can't access open-medium menu!\n"));
    if (pOpenMediumMenu)
    {
        /* Erase menu initially: */
        pOpenMediumMenu->clear();
        /* Depending on current medium type: */
        switch (m_pMediumIdHolder->type())
        {
            case UIMediumDeviceType_HardDisk:
            {
                /* Add "Choose a virtual hard disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Hard Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                break;
            }
            case UIMediumDeviceType_DVD:
            {
                /* Add "Choose a virtual optical disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Optical Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(PixmapType_CDUnmountEnabled, PixmapType_CDUnmountDisabled));
                connect(pEjectCurrentMedium, &QAction::triggered, this, &UIStorageSettingsEditor::sltUnmountDevice);
                break;
            }
            case UIMediumDeviceType_Floppy:
            {
                /* Add "Choose a virtual floppy disk" action: */
                addChooseExistingMediumAction(pOpenMediumMenu, tr("Choose/Create a Virtual Floppy Disk..."));
                addChooseDiskFileAction(pOpenMediumMenu, tr("Choose a disk file..."));
                /* Add "Choose a physical drive" actions: */
                addChooseHostDriveActions(pOpenMediumMenu);
                pOpenMediumMenu->addSeparator();
                /* Add recent media list: */
                addRecentMediumActions(pOpenMediumMenu, m_pMediumIdHolder->type());
                /* Add "Eject current medium" action: */
                pOpenMediumMenu->addSeparator();
                QAction *pEjectCurrentMedium = pOpenMediumMenu->addAction(tr("Remove Disk from Virtual Drive"));
                pEjectCurrentMedium->setEnabled(!m_pMediumIdHolder->isNull());
                pEjectCurrentMedium->setIcon(iconPool()->icon(PixmapType_FDUnmountEnabled, PixmapType_FDUnmountDisabled));
                connect(pEjectCurrentMedium, &QAction::triggered, this, &UIStorageSettingsEditor::sltUnmountDevice);
                break;
            }
            default:
                break;
        }
    }
}

void UIStorageSettingsEditor::sltUnmountDevice()
{
    m_pMediumIdHolder->setId(UIMedium().id());
}

void UIStorageSettingsEditor::sltChooseExistingMedium()
{
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uCurrentMediumId;
    if (m_pMediumIdHolder)
        uCurrentMediumId = m_pMediumIdHolder->id();
    QUuid uSelectedMediumId;
    int iResult = UIMediumSelector::openMediumSelectorDialog(window(), m_pMediumIdHolder->type(), uCurrentMediumId, uSelectedMediumId,
                                                             strMachineFolder, m_strMachineName,
                                                             m_strMachineGuestOSTypeId,
                                                             true /* enable create action: */, m_uMachineId, m_pActionPool);

    if (iResult == UIMediumSelector::ReturnCode_Rejected ||
        (iResult == UIMediumSelector::ReturnCode_Accepted && uSelectedMediumId.isNull()))
        return;
    if (iResult == static_cast<int>(UIMediumSelector::ReturnCode_LeftEmpty) &&
        (m_pMediumIdHolder->type() != UIMediumDeviceType_DVD && m_pMediumIdHolder->type() != UIMediumDeviceType_Floppy))
        return;

    m_pMediumIdHolder->setId(uSelectedMediumId);
}

void UIStorageSettingsEditor::sltChooseDiskFile()
{
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uMediumId = uiCommon().openMediumWithFileOpenDialog(m_pMediumIdHolder->type(), QApplication::activeWindow(), strMachineFolder);
    if (uMediumId.isNull())
        return;
    m_pMediumIdHolder->setId(uMediumId);
}

void UIStorageSettingsEditor::sltChooseHostDrive()
{
    /* This slot should be called ONLY by choose-host-drive action: */
    QAction *pChooseHostDriveAction = qobject_cast<QAction*>(sender());
    AssertMsg(pChooseHostDriveAction, ("Can't access choose-host-drive action!\n"));
    if (pChooseHostDriveAction)
        m_pMediumIdHolder->setId(QUuid(pChooseHostDriveAction->data().toString()));
}

void UIStorageSettingsEditor::sltChooseRecentMedium()
{
    /* This slot should be called ONLY by choose-recent-medium action: */
    QAction *pChooseRecentMediumAction = qobject_cast<QAction*>(sender());
    AssertMsg(pChooseRecentMediumAction, ("Can't access choose-recent-medium action!\n"));
    if (pChooseRecentMediumAction)
    {
        /* Get recent medium type & name: */
        const QStringList mediumInfoList = pChooseRecentMediumAction->data().toString().split(',');
        const UIMediumDeviceType enmMediumType = (UIMediumDeviceType)mediumInfoList[0].toUInt();
        const QString strMediumLocation = mediumInfoList[1];
        const QUuid uMediumId = uiCommon().openMedium(enmMediumType, strMediumLocation, this);
        if (!uMediumId.isNull())
            m_pMediumIdHolder->setId(uMediumId);
    }
}

void UIStorageSettingsEditor::sltUpdateActionStates()
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();

    const bool fIDEPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreIDEControllersPossible).toBool();
    const bool fSATAPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSATAControllersPossible).toBool();
    const bool fSCSIPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSCSIControllersPossible).toBool();
    const bool fFloppyPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreFloppyControllersPossible).toBool();
    const bool fSASPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreSASControllersPossible).toBool();
    const bool fUSBPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreUSBControllersPossible).toBool();
    const bool fNVMePossible = m_pModelStorage->data(index, StorageModel::R_IsMoreNVMeControllersPossible).toBool();
    const bool fVirtioSCSIPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreVirtioSCSIControllersPossible).toBool();

    const bool fController = m_pModelStorage->data(index, StorageModel::R_IsController).toBool();
    const bool fAttachment = m_pModelStorage->data(index, StorageModel::R_IsAttachment).toBool();
    const bool fAttachmentsPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool();
    const bool fIsAttachmentHotPluggable = m_pModelStorage->data(index, StorageModel::R_AttIsHotPluggable).toBool();

    /* Configure "add controller" actions: */
    m_pActionAddController->setEnabled(fIDEPossible || fSATAPossible || fSCSIPossible || fFloppyPossible ||
                                       fSASPossible || fUSBPossible || fNVMePossible || fVirtioSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_PIIX3)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_PIIX4)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_ICH6)->setEnabled(fIDEPossible);
    m_addControllerActions.value(KStorageControllerType_IntelAhci)->setEnabled(fSATAPossible);
    m_addControllerActions.value(KStorageControllerType_LsiLogic)->setEnabled(fSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_BusLogic)->setEnabled(fSCSIPossible);
    m_addControllerActions.value(KStorageControllerType_I82078)->setEnabled(fFloppyPossible);
    m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setEnabled(fSASPossible);
    m_addControllerActions.value(KStorageControllerType_USB)->setEnabled(fUSBPossible);
    m_addControllerActions.value(KStorageControllerType_NVMe)->setEnabled(fNVMePossible);
    m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setEnabled(fVirtioSCSIPossible);

    /* Configure "add attachment" actions: */
    m_pActionAddAttachment->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentHD->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentCD->setEnabled(fController && fAttachmentsPossible);
    m_pActionAddAttachmentFD->setEnabled(fController && fAttachmentsPossible);

    /* Configure "delete controller" action: */
    const bool fControllerInSuitableState = m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full;
    m_pActionRemoveController->setEnabled(fController && fControllerInSuitableState);

    /* Configure "delete attachment" action: */
    const bool fAttachmentInSuitableState =    m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Full
                                            || (   m_enmConfigurationAccessLevel == ConfigurationAccessLevel_Partial_Running
                                                && fIsAttachmentHotPluggable);
    m_pActionRemoveAttachment->setEnabled(fAttachment && fAttachmentInSuitableState);
}

void UIStorageSettingsEditor::sltHandleRowInsertion(const QModelIndex &parentIndex, int iPosition)
{
    const QModelIndex index = m_pModelStorage->index(iPosition, 0, parentIndex);

    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            /* Select the newly created Controller Item: */
            m_pTreeViewStorage->setCurrentIndex(index);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            /* Expand parent if it is not expanded yet: */
            if (!m_pTreeViewStorage->isExpanded(parentIndex))
                m_pTreeViewStorage->setExpanded(parentIndex, true);
            break;
        }
        default:
            break;
    }

    sltUpdateActionStates();
    sltGetInformation();
}

void UIStorageSettingsEditor::sltHandleRowRemoval()
{
    if (m_pModelStorage->rowCount (m_pModelStorage->root()) == 0)
        m_pTreeViewStorage->setCurrentIndex (m_pModelStorage->root());

    sltUpdateActionStates();
    sltGetInformation();
}

void UIStorageSettingsEditor::sltHandleCurrentItemChange()
{
    sltUpdateActionStates();
    sltGetInformation();
}

void UIStorageSettingsEditor::sltHandleContextMenuRequest(const QPoint &position)
{
    /* Forget last mouse press position: */
    m_mousePressPosition = QPoint();

    const QModelIndex index = m_pTreeViewStorage->indexAt(position);
    if (!index.isValid())
        return sltAddController();

    QMenu menu;
    switch (m_pModelStorage->data(index, StorageModel::R_ItemType).value<AbstractItem::ItemType>())
    {
        case AbstractItem::Type_ControllerItem:
        {
            const DeviceTypeList deviceTypeList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
            foreach (KDeviceType enmDeviceType, deviceTypeList)
            {
                switch (enmDeviceType)
                {
                    case KDeviceType_HardDisk:
                        menu.addAction(m_pActionAddAttachmentHD);
                        break;
                    case KDeviceType_DVD:
                        menu.addAction(m_pActionAddAttachmentCD);
                        break;
                    case KDeviceType_Floppy:
                        menu.addAction(m_pActionAddAttachmentFD);
                        break;
                    default:
                        break;
                }
            }
            menu.addAction(m_pActionRemoveController);
            break;
        }
        case AbstractItem::Type_AttachmentItem:
        {
            menu.addAction(m_pActionRemoveAttachment);
            break;
        }
        default:
            break;
    }
    if (!menu.isEmpty())
        menu.exec(m_pTreeViewStorage->viewport()->mapToGlobal(position));
}

void UIStorageSettingsEditor::sltHandleDrawItemBranches(QPainter *pPainter, const QRect &rect, const QModelIndex &index)
{
    if (!index.parent().isValid() || !index.parent().parent().isValid())
        return;

    pPainter->save();
    QStyleOption options;
    options.initFrom(m_pTreeViewStorage);
    options.rect = rect;
    options.state |= QStyle::State_Item;
    if (index.row() < m_pModelStorage->rowCount(index.parent()) - 1)
        options.state |= QStyle::State_Sibling;
    /* This pen is commonly used by different
     * look and feel styles to paint tree-view branches. */
    const QPen pen(QBrush(options.palette.dark().color(), Qt::Dense4Pattern), 0);
    pPainter->setPen(pen);
    /* If we want tree-view branches to be always painted we have to use QCommonStyle::drawPrimitive()
     * because QCommonStyle performs branch painting as opposed to particular inherited sub-classing styles. */
    qobject_cast<QCommonStyle*>(style())->QCommonStyle::drawPrimitive(QStyle::PE_IndicatorBranch, &options, pPainter);
    pPainter->restore();
}

void UIStorageSettingsEditor::sltHandleMouseMove(QMouseEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    const QPoint gPos = pEvent->globalPosition().toPoint();
    const QPoint lPos = pEvent->position().toPoint();

    const QModelIndex index = m_pTreeViewStorage->indexAt(lPos);
    const QRect indexRect = m_pTreeViewStorage->visualRect(index);

    /* Expander tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = m_pModelStorage->data(index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate(indexRect.x(), indexRect.y());
        if (expanderRect.contains(lPos))
        {
            pEvent->setAccepted(true);
            if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ToolTipType_Expander)
                m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::ToolTipType_Expander), StorageModel::R_ToolTipType);
            return;
        }
    }

    /* Adder tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool() &&
        m_pTreeViewStorage->currentIndex() == index)
    {
        const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            const KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_HDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_CDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_FDPixmapRect).toRect();
                    break;
                }
                default:
                    break;
            }
            deviceRect.translate(indexRect.x() + indexRect.width(), indexRect.y());

            if (deviceRect.contains(lPos))
            {
                pEvent->setAccepted(true);
                switch (enmDeviceType)
                {
                    case KDeviceType_HardDisk:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ToolTipType_HDAdder)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::ToolTipType_HDAdder), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_DVD:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ToolTipType_CDAdder)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::ToolTipType_CDAdder), StorageModel::R_ToolTipType);
                        break;
                    }
                    case KDeviceType_Floppy:
                    {
                        if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ToolTipType_FDAdder)
                            m_pModelStorage->setData(index, QVariant::fromValue(StorageModel::ToolTipType_FDAdder), StorageModel::R_ToolTipType);
                        break;
                    }
                    default:
                        break;
                }
                return;
            }
        }
    }

    /* Default tool-tip: */
    if (m_pModelStorage->data(index, StorageModel::R_ToolTipType).value<StorageModel::ToolTipType>() != StorageModel::ToolTipType_Default)
        m_pModelStorage->setData(index, StorageModel::ToolTipType_Default, StorageModel::R_ToolTipType);

    /* Check whether we should initiate dragging: */
    if (   !m_mousePressPosition.isNull()
        && QLineF(gPos, m_mousePressPosition).length() >= QApplication::startDragDistance())
    {
        /* Forget last mouse press position: */
        m_mousePressPosition = QPoint();

        /* Check what item we are hovering currently: */
        QModelIndex index = m_pTreeViewStorage->indexAt(lPos);
        AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
        /* And make sure this is attachment item, we are supporting dragging for this kind only: */
        AttachmentItem *pItemAttachment = qobject_cast<AttachmentItem*>(pItem);
        if (pItemAttachment)
        {
            /* Initialize dragging: */
            pEvent->setAccepted(true);
            QDrag *pDrag = new QDrag(this);
            if (pDrag)
            {
                /* Assign pixmap: */
                pDrag->setPixmap(pItem->pixmap());
                /* Prepare mime: */
                QMimeData *pMimeData = new QMimeData;
                if (pMimeData)
                {
                    pMimeData->setData(s_strControllerMimeType, pItemAttachment->parent()->id().toString().toLatin1());
                    pMimeData->setData(s_strAttachmentMimeType, pItemAttachment->id().toString().toLatin1());
                    pDrag->setMimeData(pMimeData);
                }
                /* Start dragging: */
                pDrag->exec();
            }
        }
    }
}

void UIStorageSettingsEditor::sltHandleMouseClick(QMouseEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    const QPoint gPos = pEvent->globalPosition().toPoint();
    const QPoint lPos = pEvent->position().toPoint();

    /* Acquire indexes: */
    const QModelIndex currentIndex = m_pTreeViewStorage->currentIndex();
    const QModelIndex index = m_pTreeViewStorage->indexAt(lPos);
    const QRect indexRect = m_pTreeViewStorage->visualRect(index);

    /* Remember last mouse press position only if we pressed current index: */
    if (index == currentIndex)
        m_mousePressPosition = gPos;

    /* Expander icon: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool())
    {
        QRect expanderRect = m_pModelStorage->data(index, StorageModel::R_ItemPixmapRect).toRect();
        expanderRect.translate(indexRect.x(), indexRect.y());
        if (expanderRect.contains(lPos))
        {
            pEvent->setAccepted(true);
            m_pTreeViewStorage->setExpanded(index, !m_pTreeViewStorage->isExpanded(index));
            return;
        }
    }

    /* Adder icons: */
    if (m_pModelStorage->data(index, StorageModel::R_IsController).toBool() &&
        m_pTreeViewStorage->currentIndex() == index)
    {
        const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
        for (int i = 0; i < devicesList.size(); ++ i)
        {
            const KDeviceType enmDeviceType = devicesList[i];

            QRect deviceRect;
            switch (enmDeviceType)
            {
                case KDeviceType_HardDisk:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_HDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_DVD:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_CDPixmapRect).toRect();
                    break;
                }
                case KDeviceType_Floppy:
                {
                    deviceRect = m_pModelStorage->data(index, StorageModel::R_FDPixmapRect).toRect();
                    break;
                }
                default:
                    break;
            }
            deviceRect.translate(indexRect.x() + indexRect.width(), indexRect.y());

            if (deviceRect.contains(lPos))
            {
                pEvent->setAccepted(true);
                if (m_pActionAddAttachment->isEnabled())
                    addAttachmentWrapper(enmDeviceType);
                return;
            }
        }
    }
}

void UIStorageSettingsEditor::sltHandleMouseRelease(QMouseEvent *)
{
    /* Forget last mouse press position: */
    m_mousePressPosition = QPoint();
}

void UIStorageSettingsEditor::sltHandleDragEnter(QDragEnterEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);

    /* Accept event but not the proposed action: */
    pEvent->accept();
}

void UIStorageSettingsEditor::sltHandleDragMove(QDragMoveEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Make sure mime-data format is valid: */
    if (   !pMimeData->hasFormat(UIStorageSettingsEditor::s_strControllerMimeType)
        || !pMimeData->hasFormat(UIStorageSettingsEditor::s_strAttachmentMimeType))
        return;

    /* Get controller/attachment ids: */
    const QString strControllerId = pMimeData->data(UIStorageSettingsEditor::s_strControllerMimeType);
    const QString strAttachmentId = pMimeData->data(UIStorageSettingsEditor::s_strAttachmentMimeType);

    /* Check what item we are hovering currently: */
    QModelIndex index = m_pTreeViewStorage->indexAt(pEvent->position().toPoint());
    AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
    /* And make sure this is controller item, we are supporting dropping for this kind only: */
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
    if (!pItemController || pItemController->id().toString() == strControllerId)
        return;
    /* Then make sure we support such attachment device type: */
    const DeviceTypeList devicesList(m_pModelStorage->data(index, StorageModel::R_CtrDevices).value<DeviceTypeList>());
    if (!devicesList.contains(m_pModelStorage->attachmentDeviceType(QUuid(strControllerId), QUuid(strAttachmentId))))
        return;
    /* Also make sure there is enough place for new attachment: */
    const bool fIsMoreAttachmentsPossible = m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool();
    if (!fIsMoreAttachmentsPossible)
        return;

    /* Accept drag-enter event: */
    pEvent->acceptProposedAction();
}

void UIStorageSettingsEditor::sltHandleDragDrop(QDropEvent *pEvent)
{
    /* Make sure event is valid: */
    AssertPtrReturnVoid(pEvent);
    /* And mime-data is set: */
    const QMimeData *pMimeData = pEvent->mimeData();
    AssertPtrReturnVoid(pMimeData);

    /* Check what item we are hovering currently: */
    QModelIndex index = m_pTreeViewStorage->indexAt(pEvent->position().toPoint());
    AbstractItem *pItem = static_cast<AbstractItem*>(index.internalPointer());
    /* And make sure this is controller item, we are supporting dropping for this kind only: */
    ControllerItem *pItemController = qobject_cast<ControllerItem*>(pItem);
    if (pItemController)
    {
        /* Get controller/attachment ids: */
        const QString strControllerId = pMimeData->data(UIStorageSettingsEditor::s_strControllerMimeType);
        const QString strAttachmentId = pMimeData->data(UIStorageSettingsEditor::s_strAttachmentMimeType);
        m_pModelStorage->moveAttachment(QUuid(strAttachmentId), QUuid(strControllerId), pItemController->id());
    }
}

void UIStorageSettingsEditor::prepare()
{
    /* Create icon-pool: */
    UIIconPoolStorageSettings::create();

    /* Prepare everything: */
    prepareWidgets();
    prepareConnections();

    /* Apply language settings: */
    retranslateUi();
}

void UIStorageSettingsEditor::prepareWidgets()
{
    /* Create main layout: */
    QVBoxLayout *pLayout = new QVBoxLayout(this);
    if (pLayout)
    {
        pLayout->setContentsMargins(0, 0, 0, 0);

        /* Create splitter: */
        m_pSplitter = new QISplitter(this);
        if (m_pSplitter)
        {
            m_pSplitter->setChildrenCollapsible(false);
            m_pSplitter->setOrientation(Qt::Horizontal);
            m_pSplitter->setHandleWidth(4);

            /* Prepare panes: */
            prepareLeftPane();
            prepareRightPane();

            pLayout->addWidget(m_pSplitter);
        }
    }
}

void UIStorageSettingsEditor::prepareLeftPane()
{
    /* Prepare left pane: */
    m_pWidgetLeftPane = new QWidget(m_pSplitter);
    if (m_pWidgetLeftPane)
    {
        /* Prepare left pane layout: */
        QVBoxLayout *pLayoutLeftPane = new QVBoxLayout(m_pWidgetLeftPane);
        if (pLayoutLeftPane)
        {
            pLayoutLeftPane->setContentsMargins(0, 0, 10, 0);

            /* Prepare left separator: */
            m_pLabelSeparatorLeftPane = new QILabelSeparator(m_pWidgetLeftPane);
            if (m_pLabelSeparatorLeftPane)
                pLayoutLeftPane->addWidget(m_pLabelSeparatorLeftPane);

            /* Prepare storage layout: */
            m_pLayoutTree = new QVBoxLayout;
            if (m_pLayoutTree)
            {
#ifdef VBOX_WS_MAC
                m_pLayoutTree->setContentsMargins(3, 0, 3, 0);
                m_pLayoutTree->setSpacing(3);
#else
                m_pLayoutTree->setContentsMargins(0, 0, 0, 0);
                m_pLayoutTree->setSpacing(qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing) / 3);
#endif

                /* Prepare tree-view: */
                prepareTreeView();

                /* Prepare toolbar layout: */
                m_pLayoutToolbar = new QHBoxLayout;
                if (m_pLayoutToolbar)
                {
                    m_pLayoutToolbar->addStretch();

                    /* Prepare toolbar: */
                    prepareToolBar();

                    m_pLayoutTree->addLayout(m_pLayoutToolbar);
                }

                pLayoutLeftPane->addLayout(m_pLayoutTree);
            }
        }

        m_pSplitter->addWidget(m_pWidgetLeftPane);
    }
}

void UIStorageSettingsEditor::prepareTreeView()
{
    /* Prepare tree-view: */
    m_pTreeViewStorage = new QITreeView(m_pWidgetLeftPane);
    if (m_pTreeViewStorage)
    {
        if (m_pLabelSeparatorLeftPane)
            m_pLabelSeparatorLeftPane->setBuddy(m_pTreeViewStorage);
        m_pTreeViewStorage->setMouseTracking(true);
        m_pTreeViewStorage->setAcceptDrops(true);
        m_pTreeViewStorage->setContextMenuPolicy(Qt::CustomContextMenu);

        /* Prepare storage model: */
        m_pModelStorage = new StorageModel(m_pTreeViewStorage);
        if (m_pModelStorage)
        {
            m_pTreeViewStorage->setModel(m_pModelStorage);
            m_pTreeViewStorage->setRootIndex(m_pModelStorage->root());
            m_pTreeViewStorage->setCurrentIndex(m_pModelStorage->root());
        }

        /* Prepare storage delegate: */
        StorageDelegate *pStorageDelegate = new StorageDelegate(m_pTreeViewStorage);
        if (pStorageDelegate)
            m_pTreeViewStorage->setItemDelegate(pStorageDelegate);

        m_pLayoutTree->addWidget(m_pTreeViewStorage);
    }
}

void UIStorageSettingsEditor::prepareToolBar()
{
    /* Prepare toolbar: */
    m_pToolbar = new QIToolBar(m_pWidgetLeftPane);
    if (m_pToolbar)
    {
        /* Configure toolbar: */
        const int iIconMetric = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize);
        m_pToolbar->setIconSize(QSize(iIconMetric, iIconMetric));

        /* Prepare 'Add Controller' action: */
        m_pActionAddController = new QAction(this);
        if (m_pActionAddController)
        {
            m_pActionAddController->setIcon(iconPool()->icon(PixmapType_ControllerAddEn, PixmapType_ControllerAddDis));
            m_pToolbar->addAction(m_pActionAddController);
        }

        /* Prepare 'Add PIIX3 Controller' action: */
        m_addControllerActions[KStorageControllerType_PIIX3] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_PIIX3))
            m_addControllerActions.value(KStorageControllerType_PIIX3)->setIcon(iconPool()->icon(PixmapType_IDEControllerAddEn, PixmapType_IDEControllerAddDis));
        /* Prepare 'Add PIIX4 Controller' action: */
        m_addControllerActions[KStorageControllerType_PIIX4] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_PIIX4))
            m_addControllerActions.value(KStorageControllerType_PIIX4)->setIcon(iconPool()->icon(PixmapType_IDEControllerAddEn, PixmapType_IDEControllerAddDis));
        /* Prepare 'Add ICH6 Controller' action: */
        m_addControllerActions[KStorageControllerType_ICH6] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_ICH6))
            m_addControllerActions.value(KStorageControllerType_ICH6)->setIcon(iconPool()->icon(PixmapType_IDEControllerAddEn, PixmapType_IDEControllerAddDis));
        /* Prepare 'Add AHCI Controller' action: */
        m_addControllerActions[KStorageControllerType_IntelAhci] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_IntelAhci))
            m_addControllerActions.value(KStorageControllerType_IntelAhci)->setIcon(iconPool()->icon(PixmapType_SATAControllerAddEn, PixmapType_SATAControllerAddDis));
        /* Prepare 'Add LsiLogic Controller' action: */
        m_addControllerActions[KStorageControllerType_LsiLogic] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_LsiLogic))
            m_addControllerActions.value(KStorageControllerType_LsiLogic)->setIcon(iconPool()->icon(PixmapType_SCSIControllerAddEn, PixmapType_SCSIControllerAddDis));
        /* Prepare 'Add BusLogic Controller' action: */
        m_addControllerActions[KStorageControllerType_BusLogic] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_BusLogic))
            m_addControllerActions.value(KStorageControllerType_BusLogic)->setIcon(iconPool()->icon(PixmapType_SCSIControllerAddEn, PixmapType_SCSIControllerAddDis));
        /* Prepare 'Add Floppy Controller' action: */
        m_addControllerActions[KStorageControllerType_I82078] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_I82078))
            m_addControllerActions.value(KStorageControllerType_I82078)->setIcon(iconPool()->icon(PixmapType_FloppyControllerAddEn, PixmapType_FloppyControllerAddDis));
        /* Prepare 'Add LsiLogic SAS Controller' action: */
        m_addControllerActions[KStorageControllerType_LsiLogicSas] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_LsiLogicSas))
            m_addControllerActions.value(KStorageControllerType_LsiLogicSas)->setIcon(iconPool()->icon(PixmapType_SASControllerAddEn, PixmapType_SASControllerAddDis));
        /* Prepare 'Add USB Controller' action: */
        m_addControllerActions[KStorageControllerType_USB] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_USB))
            m_addControllerActions.value(KStorageControllerType_USB)->setIcon(iconPool()->icon(PixmapType_USBControllerAddEn, PixmapType_USBControllerAddDis));
        /* Prepare 'Add NVMe Controller' action: */
        m_addControllerActions[KStorageControllerType_NVMe] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_NVMe))
            m_addControllerActions.value(KStorageControllerType_NVMe)->setIcon(iconPool()->icon(PixmapType_NVMeControllerAddEn, PixmapType_NVMeControllerAddDis));
        /* Prepare 'Add virtio-scsi Controller' action: */
        m_addControllerActions[KStorageControllerType_VirtioSCSI] = new QAction(this);
        if (m_addControllerActions.value(KStorageControllerType_VirtioSCSI))
            m_addControllerActions.value(KStorageControllerType_VirtioSCSI)->setIcon(iconPool()->icon(PixmapType_VirtioSCSIControllerAddEn, PixmapType_VirtioSCSIControllerAddDis));

        /* Prepare 'Remove Controller' action: */
        m_pActionRemoveController = new QAction(this);
        if (m_pActionRemoveController)
        {
            m_pActionRemoveController->setIcon(iconPool()->icon(PixmapType_ControllerDelEn, PixmapType_ControllerDelDis));
            m_pToolbar->addAction(m_pActionRemoveController);
        }

        /* Prepare 'Add Attachment' action: */
        m_pActionAddAttachment = new QAction(this);
        if (m_pActionAddAttachment)
        {
            m_pActionAddAttachment->setIcon(iconPool()->icon(PixmapType_AttachmentAddEn, PixmapType_AttachmentAddDis));
            m_pToolbar->addAction(m_pActionAddAttachment);
        }

        /* Prepare 'Add HD Attachment' action: */
        m_pActionAddAttachmentHD = new QAction(this);
        if (m_pActionAddAttachmentHD)
            m_pActionAddAttachmentHD->setIcon(iconPool()->icon(PixmapType_HDAttachmentAddEn, PixmapType_HDAttachmentAddDis));
        /* Prepare 'Add CD Attachment' action: */
        m_pActionAddAttachmentCD = new QAction(this);
        if (m_pActionAddAttachmentCD)
            m_pActionAddAttachmentCD->setIcon(iconPool()->icon(PixmapType_CDAttachmentAddEn, PixmapType_CDAttachmentAddDis));
        /* Prepare 'Add FD Attachment' action: */
        m_pActionAddAttachmentFD = new QAction(this);
        if (m_pActionAddAttachmentFD)
            m_pActionAddAttachmentFD->setIcon(iconPool()->icon(PixmapType_FDAttachmentAddEn, PixmapType_FDAttachmentAddDis));

        /* Prepare 'Remove Attachment' action: */
        m_pActionRemoveAttachment = new QAction(this);
        if (m_pActionRemoveAttachment)
        {
            m_pActionRemoveAttachment->setIcon(iconPool()->icon(PixmapType_AttachmentDelEn, PixmapType_AttachmentDelDis));
            m_pToolbar->addAction(m_pActionRemoveAttachment);
        }

        m_pLayoutToolbar->addWidget(m_pToolbar);
    }
}

void UIStorageSettingsEditor::prepareRightPane()
{
    /* Prepare right pane: */
    m_pStackRightPane = new QStackedWidget(m_pSplitter);
    if (m_pStackRightPane)
    {
        /* Prepare stack contents: */
        prepareEmptyWidget();
        prepareControllerWidget();
        prepareAttachmentWidget();

        m_pSplitter->addWidget(m_pStackRightPane);
    }
}

void UIStorageSettingsEditor::prepareEmptyWidget()
{
    /* Prepare widget for empty case: */
    QWidget *pWidgetEmpty = new QWidget;
    if (pWidgetEmpty)
    {
        /* Create widget layout for empty case: */
        QGridLayout *pLayoutEmpty = new QGridLayout(pWidgetEmpty);
        if (pLayoutEmpty)
        {
            pLayoutEmpty->setContentsMargins(10, 0, 0, 0);
            pLayoutEmpty->setRowStretch(2, 1);

            /* Prepare separator for empty case: */
            m_pLabelSeparatorEmpty = new QILabelSeparator(pWidgetEmpty);
            if (m_pLabelSeparatorEmpty)
                pLayoutEmpty->addWidget(m_pLabelSeparatorEmpty, 0, 0, 1, 2);

            /* Prepare label for empty case: */
            m_pLabelInfo = new QLabel(pWidgetEmpty);
            if (m_pLabelInfo)
            {
                m_pLabelInfo->setWordWrap(true);
                pLayoutEmpty->addWidget(m_pLabelInfo, 1, 1);
            }

            pLayoutEmpty->setColumnMinimumWidth(0, 10);
        }

        m_pStackRightPane->addWidget(pWidgetEmpty);
    }
}

void UIStorageSettingsEditor::prepareControllerWidget()
{
    /* Create widget for controller case: */
    UIWidgetOfMaximumSize *pWidgetController = new UIWidgetOfMaximumSize;
    if (pWidgetController)
    {
        /* Create widget layout for controller case: */
        QGridLayout *m_pLayoutController = new QGridLayout(pWidgetController);
        if (m_pLayoutController)
        {
            m_pLayoutController->setContentsMargins(10, 0, 0, 0);
            m_pLayoutController->setRowStretch(5, 1);

            /* Prepare separator for controller case: */
            m_pLabelSeparatorParameters = new QILabelSeparator(pWidgetController);
            if (m_pLabelSeparatorParameters)
                m_pLayoutController->addWidget(m_pLabelSeparatorParameters, 0, 0, 1, 3);

            /* Prepare name label: */
            m_pLabelName = new QLabel(pWidgetController);
            if (m_pLabelName)
            {
                m_pLabelName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutController->addWidget(m_pLabelName, 1, 1);
            }
            /* Prepare name editor: */
            m_pEditorName = new QLineEdit(pWidgetController);
            if (m_pEditorName)
            {
                if (m_pLabelName)
                    m_pLabelName->setBuddy(m_pEditorName);
                m_pLayoutController->addWidget(m_pEditorName, 1, 2);
            }

            /* Prepare type label: */
            m_pLabelType = new QLabel(pWidgetController);
            if (m_pLabelType)
            {
                m_pLabelType->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutController->addWidget(m_pLabelType, 2, 1);
            }
            /* Prepare type combo: */
            m_pComboType = new QComboBox(pWidgetController);
            if (m_pComboType)
            {
                if (m_pLabelType)
                    m_pLabelType->setBuddy(m_pComboType);
                m_pComboType->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_pLayoutController->addWidget(m_pComboType, 2, 2);
            }

            /* Prepare port count label: */
            m_pLabelPortCount = new QLabel(pWidgetController);
            if (m_pLabelPortCount)
            {
                m_pLabelPortCount->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutController->addWidget(m_pLabelPortCount, 3, 1);
            }
            /* Prepare port count spinbox: */
            m_pSpinboxPortCount = new QSpinBox(pWidgetController);
            if (m_pSpinboxPortCount)
            {
                if (m_pLabelPortCount)
                    m_pLabelPortCount->setBuddy(m_pSpinboxPortCount);
                m_pLayoutController->addWidget(m_pSpinboxPortCount, 3, 2);
            }

            /* Prepare port count check-box: */
            m_pCheckBoxIoCache = new QCheckBox(pWidgetController);
            if (m_pCheckBoxIoCache)
                m_pLayoutController->addWidget(m_pCheckBoxIoCache, 4, 2);

            m_pLayoutController->setColumnMinimumWidth(0, 10);
        }

        m_pStackRightPane->addWidget(pWidgetController);
    }
}

void UIStorageSettingsEditor::prepareAttachmentWidget()
{
    /* Create widget for attachment case: */
    UIWidgetOfMaximumSize *pWidgetAttachment = new UIWidgetOfMaximumSize;
    if (pWidgetAttachment)
    {
        /* Create widget layout for attachment case: */
        QGridLayout *m_pLayoutAttachment = new QGridLayout(pWidgetAttachment);
        if (m_pLayoutAttachment)
        {
            m_pLayoutAttachment->setContentsMargins(10, 0, 0, 0);
            m_pLayoutAttachment->setColumnStretch(2, 1);
            m_pLayoutAttachment->setRowStretch(13, 1);

            /* Prepare separator for attachment case: */
            m_pLabelSeparatorAttributes = new QILabelSeparator(pWidgetAttachment);
            if (m_pLabelSeparatorAttributes)
                m_pLayoutAttachment->addWidget(m_pLabelSeparatorAttributes, 0, 0, 1, 3);

            /* Prepare medium label: */
            m_pLabelMedium = new QLabel(pWidgetAttachment);
            if (m_pLabelMedium)
            {
                m_pLabelMedium->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelMedium, 1, 1);
            }

            /* Prepare slot layout: */
            QHBoxLayout *pLayoutContainer = new QHBoxLayout;
            if (pLayoutContainer)
            {
                pLayoutContainer->setContentsMargins(0, 0, 0, 0);
                pLayoutContainer->setSpacing(1);

                /* Prepare slot combo: */
                m_pComboSlot = new QComboBox(pWidgetAttachment);
                if (m_pComboSlot)
                    pLayoutContainer->addWidget(m_pComboSlot);

                /* Prepare slot combo: */
                m_pToolButtonOpen = new QIToolButton(pWidgetAttachment);
                if (m_pToolButtonOpen)
                {
                    if (m_pLabelMedium)
                        m_pLabelMedium->setBuddy(m_pToolButtonOpen);

                    /* Prepare open medium menu: */
                    QMenu *pOpenMediumMenu = new QMenu(m_pToolButtonOpen);
                    if (pOpenMediumMenu)
                        m_pToolButtonOpen->setMenu(pOpenMediumMenu);

                    pLayoutContainer->addWidget(m_pToolButtonOpen);
                }

                m_pLayoutAttachment->addLayout(pLayoutContainer, 1, 2);
            }

            /* Prepare attachment settings layout: */
            QVBoxLayout *pLayoutAttachmentSettings = new QVBoxLayout;
            if (pLayoutAttachmentSettings)
            {
                pLayoutAttachmentSettings->setContentsMargins(0, 0, 0, 0);
                pLayoutAttachmentSettings->setSpacing(0);

                /* Prepare attachment passthrough check-box: */
                m_pCheckBoxPassthrough = new QCheckBox(pWidgetAttachment);
                if (m_pCheckBoxPassthrough)
                {
                    m_pCheckBoxPassthrough->hide();
                    pLayoutAttachmentSettings->addWidget(m_pCheckBoxPassthrough);
                }

                /* Prepare attachment temporary eject check-box: */
                m_pCheckBoxTempEject = new QCheckBox(pWidgetAttachment);
                if (m_pCheckBoxTempEject)
                {
                    m_pCheckBoxTempEject->hide();
                    pLayoutAttachmentSettings->addWidget(m_pCheckBoxTempEject);
                }

                /* Prepare attachment non rotational check-box: */
                m_pCheckBoxNonRotational = new QCheckBox(pWidgetAttachment);
                if (m_pCheckBoxNonRotational)
                {
                    m_pCheckBoxNonRotational->hide();
                    pLayoutAttachmentSettings->addWidget(m_pCheckBoxNonRotational);
                }

                /* Prepare attachment hot pluggable check-box: */
                m_pCheckBoxHotPluggable = new QCheckBox(pWidgetAttachment);
                if (m_pCheckBoxHotPluggable)
                {
                    m_pCheckBoxHotPluggable->hide();
                    pLayoutAttachmentSettings->addWidget(m_pCheckBoxHotPluggable);
                }

                m_pLayoutAttachment->addLayout(pLayoutAttachmentSettings, 2, 2);
            }

            /* Prepare separator for attachment case: */
            m_pLabelSeparatorInformation = new QILabelSeparator(pWidgetAttachment);
            if (m_pLabelSeparatorInformation)
                m_pLayoutAttachment->addWidget(m_pLabelSeparatorInformation, 3, 0, 1, 3);

            /* Prepare HD format label: */
            m_pLabelHDFormat = new QLabel(pWidgetAttachment);
            if (m_pLabelHDFormat)
            {
                m_pLabelHDFormat->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelHDFormat, 4, 1);
            }
            /* Prepare HD format field: */
            m_pFieldHDFormat = new QILabel(pWidgetAttachment);
            if (m_pFieldHDFormat)
            {
                m_pFieldHDFormat->setFullSizeSelection(true);
                m_pFieldHDFormat->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldHDFormat, 4, 2);
            }

            /* Prepare CD/FD type label: */
            m_pLabelCDFDType = new QLabel(pWidgetAttachment);
            if (m_pLabelCDFDType)
            {
                m_pLabelCDFDType->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelCDFDType, 5, 1);
            }
            /* Prepare CD/FD type field: */
            m_pFieldCDFDType = new QILabel(pWidgetAttachment);
            if (m_pFieldCDFDType)
            {
                m_pFieldCDFDType->setFullSizeSelection(true);
                m_pFieldCDFDType->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldCDFDType, 5, 2);
            }

            /* Prepare HD virtual size label: */
            m_pLabelHDVirtualSize = new QLabel(pWidgetAttachment);
            if (m_pLabelHDVirtualSize)
            {
                m_pLabelHDVirtualSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelHDVirtualSize, 6, 1);
            }
            /* Prepare HD virtual size field: */
            m_pFieldHDVirtualSize = new QILabel(pWidgetAttachment);
            if (m_pFieldHDVirtualSize)
            {
                m_pFieldHDVirtualSize->setFullSizeSelection(true);
                m_pFieldHDVirtualSize->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldHDVirtualSize, 6, 2);
            }

            /* Prepare HD actual size label: */
            m_pLabelHDActualSize = new QLabel(pWidgetAttachment);
            if (m_pLabelHDActualSize)
            {
                m_pLabelHDActualSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelHDActualSize, 7, 1);
            }
            /* Prepare HD actual size field: */
            m_pFieldHDActualSize = new QILabel(pWidgetAttachment);
            if (m_pFieldHDActualSize)
            {
                m_pFieldHDActualSize->setFullSizeSelection(true);
                m_pFieldHDActualSize->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldHDActualSize, 7, 2);
            }

            /* Prepare CD/FD size label: */
            m_pLabelCDFDSize = new QLabel(pWidgetAttachment);
            if (m_pLabelCDFDSize)
            {
                m_pLabelCDFDSize->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelCDFDSize, 8, 1);
            }
            /* Prepare CD/FD size field: */
            m_pFieldCDFDSize = new QILabel(pWidgetAttachment);
            if (m_pFieldCDFDSize)
            {
                m_pFieldCDFDSize->setFullSizeSelection(true);
                m_pFieldCDFDSize->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldCDFDSize, 8, 2);
            }

            /* Prepare HD details label: */
            m_pLabelHDDetails = new QLabel(pWidgetAttachment);
            if (m_pLabelHDDetails)
            {
                m_pLabelHDDetails->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelHDDetails, 9, 1);
            }
            /* Prepare HD details field: */
            m_pFieldHDDetails = new QILabel(pWidgetAttachment);
            if (m_pFieldHDDetails)
            {
                m_pFieldHDDetails->setFullSizeSelection(true);
                m_pFieldHDDetails->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldHDDetails, 9, 2);
            }

            /* Prepare location label: */
            m_pLabelLocation = new QLabel(pWidgetAttachment);
            if (m_pLabelLocation)
            {
                m_pLabelLocation->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelLocation, 10, 1);
            }
            /* Prepare location field: */
            m_pFieldLocation = new QILabel(pWidgetAttachment);
            if (m_pFieldLocation)
            {
                m_pFieldLocation->setFullSizeSelection(true);
                m_pFieldLocation->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldLocation, 10, 2);
            }

            /* Prepare usage label: */
            m_pLabelUsage = new QLabel(pWidgetAttachment);
            if (m_pLabelUsage)
            {
                m_pLabelUsage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelUsage, 11, 1);
            }
            /* Prepare usage field: */
            m_pFieldUsage = new QILabel(pWidgetAttachment);
            if (m_pFieldUsage)
            {
                m_pFieldUsage->setFullSizeSelection(true);
                m_pFieldUsage->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldUsage, 11, 2);
            }

            /* Prepare encryption label: */
            m_pLabelEncryption = new QLabel(pWidgetAttachment);
            if (m_pLabelEncryption)
            {
                m_pLabelEncryption->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                m_pLayoutAttachment->addWidget(m_pLabelEncryption, 12, 1);
            }
            /* Prepare encryption field: */
            m_pFieldEncryption = new QILabel(pWidgetAttachment);
            if (m_pFieldEncryption)
            {
                m_pFieldEncryption->setFullSizeSelection(true);
                m_pFieldEncryption->setSizePolicy(QSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum));
                m_pLayoutAttachment->addWidget(m_pFieldEncryption, 12, 2);
            }

            m_pLayoutAttachment->setColumnMinimumWidth(0, 10);
        }

        m_pStackRightPane->addWidget(pWidgetAttachment);
    }
}

void UIStorageSettingsEditor::prepareConnections()
{
    /* Configure this: */
    connect(&uiCommon(), &UICommon::sigMediumEnumerated,
            this, &UIStorageSettingsEditor::sltHandleMediumEnumerated);
    connect(&uiCommon(), &UICommon::sigMediumDeleted,
            this, &UIStorageSettingsEditor::sltHandleMediumDeleted);

    /* Configure tree-view: */
    connect(m_pTreeViewStorage, &QITreeView::currentItemChanged,
             this, &UIStorageSettingsEditor::sltHandleCurrentItemChange);
    connect(m_pTreeViewStorage, &QITreeView::customContextMenuRequested,
            this, &UIStorageSettingsEditor::sltHandleContextMenuRequest);
    connect(m_pTreeViewStorage, &QITreeView::drawItemBranches,
            this, &UIStorageSettingsEditor::sltHandleDrawItemBranches);
    connect(m_pTreeViewStorage, &QITreeView::mouseMoved,
            this, &UIStorageSettingsEditor::sltHandleMouseMove);
    connect(m_pTreeViewStorage, &QITreeView::mousePressed,
            this, &UIStorageSettingsEditor::sltHandleMouseClick);
    connect(m_pTreeViewStorage, &QITreeView::mouseReleased,
            this, &UIStorageSettingsEditor::sltHandleMouseRelease);
    connect(m_pTreeViewStorage, &QITreeView::mouseDoubleClicked,
            this, &UIStorageSettingsEditor::sltHandleMouseClick);
    connect(m_pTreeViewStorage, &QITreeView::dragEntered,
            this, &UIStorageSettingsEditor::sltHandleDragEnter);
    connect(m_pTreeViewStorage, &QITreeView::dragMoved,
            this, &UIStorageSettingsEditor::sltHandleDragMove);
    connect(m_pTreeViewStorage, &QITreeView::dragDropped,
            this, &UIStorageSettingsEditor::sltHandleDragDrop);

    /* Create model: */
    connect(m_pModelStorage, &StorageModel::rowsInserted,
            this, &UIStorageSettingsEditor::sltHandleRowInsertion);
    connect(m_pModelStorage, &StorageModel::rowsRemoved,
            this, &UIStorageSettingsEditor::sltHandleRowRemoval);

    /* Configure actions: */
    connect(m_pActionAddController, &QAction::triggered, this, &UIStorageSettingsEditor::sltAddController);
    connect(m_addControllerActions.value(KStorageControllerType_PIIX3), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerPIIX3);
    connect(m_addControllerActions.value(KStorageControllerType_PIIX4), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerPIIX4);
    connect(m_addControllerActions.value(KStorageControllerType_ICH6), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerICH6);
    connect(m_addControllerActions.value(KStorageControllerType_IntelAhci), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerAHCI);
    connect(m_addControllerActions.value(KStorageControllerType_LsiLogic), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerLsiLogic);
    connect(m_addControllerActions.value(KStorageControllerType_BusLogic), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerBusLogic);
    connect(m_addControllerActions.value(KStorageControllerType_I82078), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerFloppy);
    connect(m_addControllerActions.value(KStorageControllerType_LsiLogicSas), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerLsiLogicSAS);
    connect(m_addControllerActions.value(KStorageControllerType_USB), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerUSB);
    connect(m_addControllerActions.value(KStorageControllerType_NVMe), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerNVMe);
    connect(m_addControllerActions.value(KStorageControllerType_VirtioSCSI), &QAction::triggered, this, &UIStorageSettingsEditor::sltAddControllerVirtioSCSI);
    connect(m_pActionRemoveController, &QAction::triggered, this, &UIStorageSettingsEditor::sltRemoveController);
    connect(m_pActionAddAttachment, &QAction::triggered, this, &UIStorageSettingsEditor::sltAddAttachment);
    connect(m_pActionAddAttachmentHD, &QAction::triggered, this, &UIStorageSettingsEditor::sltAddAttachmentHD);
    connect(m_pActionAddAttachmentCD, &QAction::triggered, this, &UIStorageSettingsEditor::sltAddAttachmentCD);
    connect(m_pActionAddAttachmentFD, &QAction::triggered, this, &UIStorageSettingsEditor::sltAddAttachmentFD);
    connect(m_pActionRemoveAttachment, &QAction::triggered, this, &UIStorageSettingsEditor::sltRemoveAttachment);

    /* Configure tool-button: */
    connect(m_pToolButtonOpen, &QIToolButton::clicked, m_pToolButtonOpen, &QIToolButton::showMenu);
    /* Configure menu: */
    connect(m_pToolButtonOpen->menu(), &QMenu::aboutToShow, this, &UIStorageSettingsEditor::sltPrepareOpenMediumMenu);

    /* Configure widgets: */
    connect(m_pMediumIdHolder, &UIMediumIDHolder::sigChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pSpinboxPortCount, &QSpinBox::valueChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pEditorName, &QLineEdit::textEdited,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pComboType, &QComboBox::activated,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pComboSlot, &QComboBox::activated,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pCheckBoxIoCache, &QCheckBox::stateChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pCheckBoxPassthrough, &QCheckBox::stateChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pCheckBoxTempEject, &QCheckBox::stateChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pCheckBoxNonRotational, &QCheckBox::stateChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
    connect(m_pCheckBoxHotPluggable, &QCheckBox::stateChanged,
            this, &UIStorageSettingsEditor::sltSetInformation);
}

void UIStorageSettingsEditor::cleanup()
{
    /* Destroy icon-pool: */
    UIIconPoolStorageSettings::destroy();
}

void UIStorageSettingsEditor::addControllerWrapper(const QString &strName, KStorageBus enmBus, KStorageControllerType enmType)
{
#ifdef RT_STRICT
    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    switch (enmBus)
    {
        case KStorageBus_IDE:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreIDEControllersPossible).toBool());
            break;
        case KStorageBus_SATA:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSATAControllersPossible).toBool());
            break;
        case KStorageBus_SCSI:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSCSIControllersPossible).toBool());
            break;
        case KStorageBus_SAS:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreSASControllersPossible).toBool());
            break;
        case KStorageBus_Floppy:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreFloppyControllersPossible).toBool());
            break;
        case KStorageBus_USB:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreUSBControllersPossible).toBool());
            break;
        case KStorageBus_PCIe:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreNVMeControllersPossible).toBool());
            break;
        case KStorageBus_VirtioSCSI:
            Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreVirtioSCSIControllersPossible).toBool());
            break;
        default:
            break;
    }
#endif

    m_pModelStorage->addController(strName, enmBus, enmType);
    emit sigValueChanged();
}

void UIStorageSettingsEditor::addAttachmentWrapper(KDeviceType enmDeviceType)
{
    const QModelIndex index = m_pTreeViewStorage->currentIndex();
    Assert(m_pModelStorage->data(index, StorageModel::R_IsController).toBool());
    Assert(m_pModelStorage->data(index, StorageModel::R_IsMoreAttachmentsPossible).toBool());
    const QString strMachineFolder(QFileInfo(m_strMachineSettingsFilePath).absolutePath());

    QUuid uMediumId;
    int iResult = UIMediumSelector::openMediumSelectorDialog(window(), UIMediumDefs::mediumTypeToLocal(enmDeviceType),
                                                             QUuid() /* current medium Id */, uMediumId,
                                                             strMachineFolder, m_strMachineName,
                                                             m_strMachineGuestOSTypeId,
                                                             true /* enable cr1eate action: */, m_uMachineId, m_pActionPool);

    /* Continue only if iResult is either UIMediumSelector::ReturnCode_Accepted or UIMediumSelector::ReturnCode_LeftEmpty: */
    /* If iResult is UIMediumSelector::ReturnCode_Accepted then we have to have a valid uMediumId: */
    if (iResult == UIMediumSelector::ReturnCode_Rejected ||
        (iResult == UIMediumSelector::ReturnCode_Accepted && uMediumId.isNull()))
        return;

    /* Only DVDs and floppy can be created empty: */
    if (iResult == static_cast<int>(UIMediumSelector::ReturnCode_LeftEmpty) &&
        (enmDeviceType != KDeviceType_DVD && enmDeviceType != KDeviceType_Floppy))
        return;

    m_pModelStorage->addAttachment(QUuid(m_pModelStorage->data(index, StorageModel::R_ItemId).toString()), enmDeviceType, uMediumId);
    m_pModelStorage->sort();

    /* Notify listeners: */
    emit sigValueChanged();
}

QString UIStorageSettingsEditor::generateUniqueControllerName(const QString &strTemplate) const
{
    int iMaxNumber = 0;
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = m_pModelStorage->index(i, 0, rootIndex);
        const QString strName = m_pModelStorage->data(controllerIndex, StorageModel::R_CtrName).toString();
        if (strName.startsWith(strTemplate))
        {
            const QString strNumber(strName.right(strName.size() - strTemplate.size()));
            bool fConverted = false;
            const int iNumber = strNumber.toInt(&fConverted);
            iMaxNumber = fConverted && (iNumber > iMaxNumber) ? iNumber : 1;
        }
    }
    return iMaxNumber ? QString("%1 %2").arg(strTemplate).arg(++iMaxNumber) : strTemplate;
}

uint32_t UIStorageSettingsEditor::deviceCount(KDeviceType enmType) const
{
    uint32_t cDevices = 0;
    const QModelIndex rootIndex = m_pModelStorage->root();
    for (int i = 0; i < m_pModelStorage->rowCount(rootIndex); ++i)
    {
        const QModelIndex controllerIndex = m_pModelStorage->index(i, 0, rootIndex);
        for (int j = 0; j < m_pModelStorage->rowCount(controllerIndex); ++j)
        {
            const QModelIndex attachmentIndex = m_pModelStorage->index(j, 0, controllerIndex);
            const KDeviceType enmDeviceType = m_pModelStorage->data(attachmentIndex, StorageModel::R_AttDevice).value<KDeviceType>();
            if (enmDeviceType == enmType)
                ++cDevices;
        }
    }

    return cDevices;
}

void UIStorageSettingsEditor::addChooseExistingMediumAction(QMenu *pOpenMediumMenu, const QString &strActionName)
{
    QAction *pChooseExistingMedium = pOpenMediumMenu->addAction(strActionName);
    pChooseExistingMedium->setIcon(iconPool()->icon(PixmapType_ChooseExistingEn, PixmapType_ChooseExistingDis));
    connect(pChooseExistingMedium, &QAction::triggered, this, &UIStorageSettingsEditor::sltChooseExistingMedium);
}

void UIStorageSettingsEditor::addChooseDiskFileAction(QMenu *pOpenMediumMenu, const QString &strActionName)
{
    QAction *pChooseDiskFile = pOpenMediumMenu->addAction(strActionName);
    pChooseDiskFile->setIcon(iconPool()->icon(PixmapType_ChooseExistingEn, PixmapType_ChooseExistingDis));
    connect(pChooseDiskFile, &QAction::triggered, this, &UIStorageSettingsEditor::sltChooseDiskFile);
}

void UIStorageSettingsEditor::addChooseHostDriveActions(QMenu *pOpenMediumMenu)
{
    foreach (const QUuid &uMediumId, uiCommon().mediumIDs())
    {
        const UIMedium guiMedium = uiCommon().medium(uMediumId);
        if (guiMedium.isHostDrive() && m_pMediumIdHolder->type() == guiMedium.type())
        {
            QAction *pHostDriveAction = pOpenMediumMenu->addAction(guiMedium.name());
            pHostDriveAction->setData(guiMedium.id());
            connect(pHostDriveAction, &QAction::triggered, this, &UIStorageSettingsEditor::sltChooseHostDrive);
        }
    }
}

void UIStorageSettingsEditor::addRecentMediumActions(QMenu *pOpenMediumMenu, UIMediumDeviceType enmRecentMediumType)
{
    /* Get recent-medium list: */
    QStringList recentMediumList;
    switch (enmRecentMediumType)
    {
        case UIMediumDeviceType_HardDisk: recentMediumList = gEDataManager->recentListOfHardDrives(); break;
        case UIMediumDeviceType_DVD:      recentMediumList = gEDataManager->recentListOfOpticalDisks(); break;
        case UIMediumDeviceType_Floppy:   recentMediumList = gEDataManager->recentListOfFloppyDisks(); break;
        default: break;
    }
    /* For every list-item: */
    for (int iIndex = 0; iIndex < recentMediumList.size(); ++iIndex)
    {
        /* Prepare corresponding action: */
        const QString &strRecentMediumLocation = recentMediumList.at(iIndex);
        if (QFile::exists(strRecentMediumLocation))
        {
            QAction *pChooseRecentMediumAction = pOpenMediumMenu->addAction(QFileInfo(strRecentMediumLocation).fileName(),
                                                                            this, SLOT(sltChooseRecentMedium()));
            pChooseRecentMediumAction->setData(QString("%1,%2").arg(enmRecentMediumType).arg(strRecentMediumLocation));
        }
    }
}

/* static */
QString UIStorageSettingsEditor::compressText(const QString &strText)
{
    return QString("<nobr><compact elipsis=\"end\">%1</compact></nobr>").arg(strText);
}


# include "UIStorageSettingsEditor.moc"
