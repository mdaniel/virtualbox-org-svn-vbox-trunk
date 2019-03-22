/* $Id$ */
/** @file
 * VBox Qt GUI - UIPortForwardingTable class declaration.
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

#ifndef ___UIPortForwardingTable_h___
#define ___UIPortForwardingTable_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QString>
#include <QWidget>

/* GUI includes: */
#include "QIWithRetranslateUI.h"
#include "UILibraryDefs.h"

/* COM includes: */
#include "COMEnums.h"

/* Forward declarations: */
class QAction;
class QHBoxLayout;
class QIDialogButtonBox;
class QITableView;
class UIPortForwardingModel;
class UIToolBar;


/** QString subclass used to distinguish name data from simple QString. */
class NameData : public QString
{
public:

    /** Constructs null name data. */
    NameData() : QString() {}
    /** Constructs name data passing @a strName to the base-class. */
    NameData(const QString &strName) : QString(strName) {}
};
Q_DECLARE_METATYPE(NameData);


/** QString subclass used to distinguish IP data from simple QString. */
class IpData : public QString
{
public:

    /** Constructs null IP data. */
    IpData() : QString() {}
    /** Constructs name data passing @a strIp to the base-class. */
    IpData(const QString &strIp) : QString(strIp) {}
};
Q_DECLARE_METATYPE(IpData);


/** Wrapper for ushort used to distinguish port data from simple ushort. */
class PortData
{
public:

    /** Constructs null port data. */
    PortData() : m_uValue(0) {}
    /** Constructs port data based on @a uValue. */
    PortData(ushort uValue) : m_uValue(uValue) {}
    /** Constructs port data based on @a other port data value. */
    PortData(const PortData &other) : m_uValue(other.value()) {}

    /** Returns whether this port data is equal to @a another. */
    bool operator==(const PortData &another) const { return m_uValue == another.m_uValue; }

    /** Returns serialized port data value. */
    ushort value() const { return m_uValue; }

private:

    /** Holds the port data value. */
    ushort m_uValue;
};
Q_DECLARE_METATYPE(PortData);


/** Port Forwarding Rule structure. */
struct UIDataPortForwardingRule
{
    /** Constructs data. */
    UIDataPortForwardingRule()
        : name(QString())
        , protocol(KNATProtocol_UDP)
        , hostIp(IpData())
        , hostPort(PortData())
        , guestIp(IpData())
        , guestPort(PortData())
    {}

    /** Constructs data on the basis of passed arguments.
      * @param  strName      Brings the rule name.
      * @param  enmProtocol  Brings the rule protocol.
      * @param  strHostIP    Brings the rule host IP.
      * @param  uHostPort    Brings the rule host port.
      * @param  strGuestIP   Brings the rule guest IP.
      * @param  uGuestPort   Brings the rule guest port. */
    UIDataPortForwardingRule(const NameData &strName,
                             KNATProtocol enmProtocol,
                             const IpData &strHostIP,
                             PortData uHostPort,
                             const IpData &strGuestIP,
                             PortData uGuestPort)
        : name(strName)
        , protocol(enmProtocol)
        , hostIp(strHostIP)
        , hostPort(uHostPort)
        , guestIp(strGuestIP)
        , guestPort(uGuestPort)
    {}

    /** Returns whether the @a other passed data is equal to this one. */
    bool equal(const UIDataPortForwardingRule &other) const
    {
        return true
               && (name == other.name)
               && (protocol == other.protocol)
               && (hostIp == other.hostIp)
               && (hostPort == other.hostPort)
               && (guestIp == other.guestIp)
               && (guestPort == other.guestPort)
               ;
    }

    /** Returns whether the @a other passed data is equal to this one. */
    bool operator==(const UIDataPortForwardingRule &other) const { return equal(other); }
    /** Returns whether the @a other passed data is different from this one. */
    bool operator!=(const UIDataPortForwardingRule &other) const { return !equal(other); }

    /** Holds the rule name. */
    NameData name;
    /** Holds the rule protocol. */
    KNATProtocol protocol;
    /** Holds the rule host IP. */
    IpData hostIp;
    /** Holds the rule host port. */
    PortData hostPort;
    /** Holds the rule guest IP. */
    IpData guestIp;
    /** Holds the rule guest port. */
    PortData guestPort;
};

/** Port Forwarding Data list. */
typedef QList<UIDataPortForwardingRule> UIPortForwardingDataList;


/** Unique part of port forwarding data. */
struct UIPortForwardingDataUnique
{
    /** Constructs unique port forwarding data based on
      * @a enmProtocol, @a uHostPort and @a uHostPort. */
    UIPortForwardingDataUnique(KNATProtocol enmProtocol,
                               PortData uHostPort,
                               const IpData &strHostIp)
        : protocol(enmProtocol)
        , hostPort(uHostPort)
        , hostIp(strHostIp)
    {}

    /** Returns whether this port data is equal to @a another. */
    bool operator==(const UIPortForwardingDataUnique &another)
    {
        return    protocol == another.protocol
               && hostPort == another.hostPort
               && (   hostIp.isEmpty()    || another.hostIp.isEmpty()
                   || hostIp == "0.0.0.0" || another.hostIp == "0.0.0.0"
                   || hostIp              == another.hostIp);
    }

    /** Holds the port forwarding data protocol type. */
    KNATProtocol protocol;
    /** Holds the port forwarding data host port. */
    PortData hostPort;
    /** Holds the port forwarding data host IP. */
    IpData hostIp;
};


/** QWidget subclass representig Port Forwarding table. */
class SHARED_LIBRARY_STUFF UIPortForwardingTable : public QIWithRetranslateUI<QWidget>
{
    Q_OBJECT;

public:

    /** Constructs Port Forwarding table.
      * @param  rules                Brings the current list of Port Forwarding rules.
      * @param  fIPv6                Brings whether this table contains IPv6 rules, not IPv4.
      * @param  fAllowEmptyGuestIPs  Brings whether this table allows empty guest IPs. */
    UIPortForwardingTable(const UIPortForwardingDataList &rules, bool fIPv6, bool fAllowEmptyGuestIPs);

    /** Returns the list of port forwarding rules. */
    const UIPortForwardingDataList rules() const;

    /** Validates the table. */
    bool validate() const;

    /** Returns whether the table data was changed. */
    bool isChanged() const { return m_fTableDataChanged; }

    /** Makes sure current editor data committed. */
    void makeSureEditorDataCommitted();

protected:

    /** Preprocesses any Qt @a pEvent for passed @a pObject. */
    virtual bool eventFilter(QObject *pObject, QEvent *pEvent) /* override */;

    /** Handles translation event. */
    virtual void retranslateUi() /* override */;

private slots:

    /** Adds the rule. */
    void sltAddRule();
    /** Copies the rule. */
    void sltCopyRule();
    /** Removes the rule. */
    void sltRemoveRule();

    /** Marks table data as changed. */
    void sltTableDataChanged() { m_fTableDataChanged = true; }

    /** Handles current item change. */
    void sltCurrentChanged();
    /** Handles request to show context-menu in certain @a position. */
    void sltShowTableContexMenu(const QPoint &position);
    /** Adjusts table column sizes. */
    void sltAdjustTable();

private:

    /** Prepares all. */
    void prepare();
    /** Prepares layout. */
    void prepareLayout();
    /** Prepares table-view. */
    void prepareTableView();
    /** Prepares table-model. */
    void prepareTableModel();
    /** Prepares table-delegates. */
    void prepareTableDelegates();
    /** Prepares toolbar. */
    void prepareToolbar();

    /** Holds the _initial_ list of Port Forwarding rules. */
    const UIPortForwardingDataList &m_rules;

    /** Holds whether this table contains IPv6 rules, not IPv4. */
    bool  m_fIPv6               : 1;
    /** Holds whether this table allows empty guest IPs. */
    bool  m_fAllowEmptyGuestIPs : 1;
    /** Holds whether this table data was changed. */
    bool  m_fTableDataChanged   : 1;

    /** Holds the layout instance. */
    QHBoxLayout *m_pLayout;
    /** Holds the table-view instance. */
    QITableView *m_pTableView;
    /** Holds the tool-bar instance. */
    UIToolBar   *m_pToolBar;

    /** Holds the table-model instance. */
    UIPortForwardingModel *m_pTableModel;

    /** Holds the Add action instance. */
    QAction *m_pActionAdd;
    /** Holds the Copy action instance. */
    QAction *m_pActionCopy;
    /** Holds the Remove action instance. */
    QAction *m_pActionRemove;
};


#endif /* !___UIPortForwardingTable_h___ */
