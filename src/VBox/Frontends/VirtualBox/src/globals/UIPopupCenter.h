/* $Id$ */
/** @file
 * VBox Qt GUI - UIPopupCenter class declaration.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIPopupCenter_h___
#define ___UIPopupCenter_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* Qt includes: */
#include <QMap>
#include <QObject>
#include <QPointer>

/* GUI includes: */
#include "UILibraryDefs.h"
#include "UIMediumDefs.h"

/* Forward declaration: */
class QWidget;
class UIPopupStack;
class CAudioAdapter;
class CConsole;
class CEmulatedUSB;
class CMachine;
class CNetworkAdapter;
class CVirtualBox;
class CVirtualBoxErrorInfo;
class CVRDEServer;


/** Popup-stack types. */
enum UIPopupStackType
{
    UIPopupStackType_Embedded,
    UIPopupStackType_Separate
};

/** Popup-stack orientations. */
enum UIPopupStackOrientation
{
    UIPopupStackOrientation_Top,
    UIPopupStackOrientation_Bottom
};


/** Singleton QObject extension
  * providing GUI with various popup messages. */
class SHARED_LIBRARY_STUFF UIPopupCenter: public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about popup-pane with @a strID is closed with @a iResultCode. */
    void sigPopupPaneDone(QString strID, int iResultCode);

public:

    /** Creates popup-center singleton. */
    static void create();
    /** Destroys message-center singleton. */
    static void destroy();

    /** Shows popup-stack for @a pParent. */
    void showPopupStack(QWidget *pParent);
    /** Hides popup-stack for @a pParent. */
    void hidePopupStack(QWidget *pParent);

    /** Defines popup-stack @a enmType for @a pParent. */
    void setPopupStackType(QWidget *pParent, UIPopupStackType enmType);
    /** Defines popup-stack @a enmOrientation for @a pParent. */
    void setPopupStackOrientation(QWidget *pParent, UIPopupStackOrientation enmOrientation);

    /** Shows a general type of 'Message'.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message.
      * @param  strDetails                Brings the details.
      * @param  strButtonText1            Brings the button 1 text.
      * @param  strButtonText2            Brings the button 2 text.
      * @param  fProposeAutoConfirmation  Brings whether auto-confirmation if possible. */
    void message(QWidget *pParent, const QString &strID,
                 const QString &strMessage, const QString &strDetails,
                 const QString &strButtonText1 = QString(),
                 const QString &strButtonText2 = QString(),
                 bool fProposeAutoConfirmation = false);

    /** Shows 'Popup' type of 'Message'.
      * Omits details, provides no buttons.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message. */
    void popup(QWidget *pParent, const QString &strID,
               const QString &strMessage);

    /** Shows 'Alert' type of 'Message'.
      * Omits details, provides one button.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message.
      * @param  fProposeAutoConfirmation  Brings whether auto-confirmation if possible. */
    void alert(QWidget *pParent, const QString &strID,
               const QString &strMessage,
               bool fProposeAutoConfirmation = false);

    /** Shows 'Alert with Details' type of 'Message'.
      * Provides one button.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message.
      * @param  strDetails                Brings the details.
      * @param  fProposeAutoConfirmation  Brings whether auto-confirmation if possible. */
    void alertWithDetails(QWidget *pParent, const QString &strID,
                          const QString &strMessage, const QString &strDetails,
                          bool fProposeAutoConfirmation = false);

    /** Shows 'Question' type of 'Message'.
      * Omits details, provides up to two buttons.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message.
      * @param  strButtonText1            Brings the button 1 text.
      * @param  strButtonText2            Brings the button 2 text.
      * @param  fProposeAutoConfirmation  Brings whether auto-confirmation if possible. */
    void question(QWidget *pParent, const QString &strID,
                  const QString &strMessage,
                  const QString &strButtonText1 = QString(),
                  const QString &strButtonText2 = QString(),
                  bool fProposeAutoConfirmation = false);

    /** Recalls popup with @a strID of passed @a pParent. */
    void recall(QWidget *pParent, const QString &strID);

    /* API: Runtime UI stuff: */
    void cannotSendACPIToMachine(QWidget *pParent);
    void remindAboutAutoCapture(QWidget *pParent);
    void remindAboutMouseIntegration(QWidget *pParent, bool fSupportsAbsolute);
    void remindAboutPausedVMInput(QWidget *pParent);
    void forgetAboutPausedVMInput(QWidget *pParent);
    void remindAboutWrongColorDepth(QWidget *pParent, ulong uRealBPP, ulong uWantedBPP);
    void forgetAboutWrongColorDepth(QWidget *pParent);
    void cannotAttachUSBDevice(QWidget *pParent, const CConsole &comConsole, const QString &strDevice);
    void cannotAttachUSBDevice(QWidget *pParent, const CVirtualBoxErrorInfo &comErrorInfo,
                               const QString &strDevice, const QString &strMachineName);
    void cannotDetachUSBDevice(QWidget *pParent, const CConsole &comConsole, const QString &strDevice);
    void cannotDetachUSBDevice(QWidget *pParent, const CVirtualBoxErrorInfo &comErrorInfo,
                               const QString &strDevice, const QString &strMachineName);
    void cannotAttachWebCam(QWidget *pParent, const CEmulatedUSB &comDispatcher,
                            const QString &strWebCamName, const QString &strMachineName);
    void cannotDetachWebCam(QWidget *pParent, const CEmulatedUSB &comDispatcher,
                            const QString &strWebCamName, const QString &strMachineName);
    void cannotToggleRecording(QWidget *pParent, const CMachine &comMachine, bool fEnable);
    void cannotToggleVRDEServer(QWidget *pParent,  const CVRDEServer &comServer,
                                const QString &strMachineName, bool fEnable);
    void cannotToggleNetworkAdapterCable(QWidget *pParent, const CNetworkAdapter &comAdapter,
                                         const QString &strMachineName, bool fConnect);
    void remindAboutGuestAdditionsAreNotActive(QWidget *pParent);
    void cannotToggleAudioOutput(QWidget *pParent, const CAudioAdapter &comAdapter,
                                 const QString &strMachineName, bool fEnable);
    void cannotToggleAudioInput(QWidget *pParent, const CAudioAdapter &comAdapter,
                                const QString &strMachineName, bool fEnable);
    void cannotMountImage(QWidget *pParent, const QString &strMachineName, const QString &strMediumName);
    void cannotOpenMedium(QWidget *pParent, const CVirtualBox &comVBox, UIMediumDeviceType enmType, const QString &strLocation);
    void cannotSaveMachineSettings(QWidget *pParent, const CMachine &comMachine);

private slots:

    /** Handles request to close popup-pane with @a strID and @a iResultCode. */
    void sltPopupPaneDone(QString strID, int iResultCode);

    /** Handles request to remove popup-stack with @a strID. */
    void sltRemovePopupStack(QString strID);

private:

    /** Constructs popup-center. */
    UIPopupCenter();
    /** Destructs popup-center. */
    ~UIPopupCenter();

    /** Prepares all. */
    void prepare();
    /** Cleanups all. */
    void cleanup();

    /** Shows popup-pane.
      * @param  pParent                   Brings the popup-pane parent.
      * @param  strID                     Brings the popup-pane ID.
      * @param  strMessage                Brings the message.
      * @param  strDetails                Brings the details.
      * @param  strButtonText1            Brings the button 1 text.
      * @param  strButtonText2            Brings the button 2 text.
      * @param  fProposeAutoConfirmation  Brings whether auto-confirmation if possible. */
    void showPopupPane(QWidget *pParent, const QString &strID,
                       const QString &strMessage, const QString &strDetails,
                       QString strButtonText1 = QString(), QString strButtonText2 = QString(),
                       bool fProposeAutoConfirmation = false);
    /** Hides popup-pane.
      * @param  pParent  Brings the popup-pane parent.
      * @param  strID    Brings the popup-pane ID. */
    void hidePopupPane(QWidget *pParent, const QString &strID);

    /** Returns popup-stack ID for passed @a pParent. */
    static QString popupStackID(QWidget *pParent);
    /** Assigns @a pPopupStack @a pParent of passed @a enmStackType. */
    static void assignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent, UIPopupStackType enmStackType);
    /** Unassigns @a pPopupStack @a pParent. */
    static void unassignPopupStackParent(UIPopupStack *pPopupStack, QWidget *pParent);

    /** Holds the popup-stack type on per stack ID basis. */
    QMap<QString, UIPopupStackType>         m_stackTypes;
    /** Holds the popup-stack orientations on per stack ID basis. */
    QMap<QString, UIPopupStackOrientation>  m_stackOrientations;
    /** Holds the popup-stacks on per stack ID basis. */
    QMap<QString, QPointer<UIPopupStack> >  m_stacks;

    /** Holds the singleton message-center instance. */
    static UIPopupCenter *s_pInstance;
    /** Returns the singleton message-center instance. */
    static UIPopupCenter *instance();
    /** Allows for shortcut access. */
    friend UIPopupCenter &popupCenter();
};

/** Singleton Popup Center 'official' name. */
inline UIPopupCenter &popupCenter() { return *UIPopupCenter::instance(); }


#endif /* !___UIPopupCenter_h___ */

