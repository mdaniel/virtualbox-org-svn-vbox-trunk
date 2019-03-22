/* $Id$ */
/** @file
 * VBox Qt GUI - UIMessageCenter class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
# include <QDir>
# include <QFileInfo>
# include <QLocale>
# include <QThread>
# include <QProcess>
# ifdef VBOX_WS_MAC
#  include <QPushButton>
# endif /* VBOX_WS_MAC */

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIConverter.h"
# include "UIMessageCenter.h"
# include "UISelectorWindow.h"
# include "UIProgressDialog.h"
# ifdef VBOX_GUI_WITH_NETWORK_MANAGER
#  include "UINetworkManager.h"
#  include "UINetworkManagerDialog.h"
# endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
# include "UIModalWindowManager.h"
# include "UIExtraDataManager.h"
# include "UIMedium.h"
# ifdef VBOX_OSE
#  include "UIDownloaderUserManual.h"
# endif /* VBOX_OSE */
# include "UIMachine.h"
# include "VBoxAboutDlg.h"
# include "UIHostComboEditor.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif /* VBOX_WS_MAC */
# ifdef VBOX_WS_WIN
#  include <Htmlhelp.h>
# endif /* VBOX_WS_WIN */

/* COM includes: */
# include "CAudioAdapter.h"
# include "CNATEngine.h"
# include "CParallelPort.h"
# include "CSerialPort.h"
# include "CSharedFolder.h"
# include "CStorageController.h"
# include "CUSBController.h"
# include "CUSBDeviceFilters.h"
# include "CUSBDeviceFilter.h"
# include "CConsole.h"
# include "CMachine.h"
# include "CSystemProperties.h"
# include "CVirtualBoxErrorInfo.h"
# include "CMediumAttachment.h"
# include "CMediumFormat.h"
# include "CAppliance.h"
# include "CExtPackManager.h"
# include "CExtPackFile.h"
# include "CHostNetworkInterface.h"
# include "CVRDEServer.h"
# include "CNetworkAdapter.h"
# include "CEmulatedUSB.h"
# ifdef VBOX_WITH_DRAG_AND_DROP
#  include "CGuest.h"
#  include "CDnDSource.h"
#  include "CDnDTarget.h"
# endif /* VBOX_WITH_DRAG_AND_DROP */

/* Other VBox includes: */
# include <iprt/param.h>
# include <iprt/path.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

#include <iprt/err.h>


/* static */
UIMessageCenter* UIMessageCenter::m_spInstance = 0;
UIMessageCenter* UIMessageCenter::instance() { return m_spInstance; }

/* static */
void UIMessageCenter::create()
{
    /* Make sure instance is NOT created yet: */
    if (m_spInstance)
    {
        AssertMsgFailed(("UIMessageCenter instance is already created!"));
        return;
    }

    /* Create instance: */
    new UIMessageCenter;
    /* Prepare instance: */
    m_spInstance->prepare();
}

/* static */
void UIMessageCenter::destroy()
{
    /* Make sure instance is NOT destroyed yet: */
    if (!m_spInstance)
    {
        AssertMsgFailed(("UIMessageCenter instance is already destroyed!"));
        return;
    }

    /* Cleanup instance: */
    m_spInstance->cleanup();
    /* Destroy instance: */
    delete m_spInstance;
}

bool UIMessageCenter::warningShown(const QString &strWarningName) const
{
    return m_warnings.contains(strWarningName);
}

void UIMessageCenter::setWarningShown(const QString &strWarningName, bool fWarningShown) const
{
    if (fWarningShown && !m_warnings.contains(strWarningName))
        m_warnings.append(strWarningName);
    else if (!fWarningShown && m_warnings.contains(strWarningName))
        m_warnings.removeAll(strWarningName);
}

int UIMessageCenter::message(QWidget *pParent, MessageType type,
                             const QString &strMessage,
                             const QString &strDetails,
                             const char *pcszAutoConfirmId /* = 0*/,
                             int iButton1 /* = 0*/,
                             int iButton2 /* = 0*/,
                             int iButton3 /* = 0*/,
                             const QString &strButtonText1 /* = QString() */,
                             const QString &strButtonText2 /* = QString() */,
                             const QString &strButtonText3 /* = QString() */) const
{
    /* If this is NOT a GUI thread: */
    if (thread() != QThread::currentThread())
    {
        /* We have to throw a blocking signal
         * to show a message-box in the GUI thread: */
        emit sigToShowMessageBox(pParent, type,
                                 strMessage, strDetails,
                                 iButton1, iButton2, iButton3,
                                 strButtonText1, strButtonText2, strButtonText3,
                                 QString(pcszAutoConfirmId));
        /* Inter-thread communications are not yet implemented: */
        return 0;
    }
    /* In usual case we can chow a message-box directly: */
    return showMessageBox(pParent, type,
                          strMessage, strDetails,
                          iButton1, iButton2, iButton3,
                          strButtonText1, strButtonText2, strButtonText3,
                          QString(pcszAutoConfirmId));
}

void UIMessageCenter::error(QWidget *pParent, MessageType type,
                           const QString &strMessage,
                           const QString &strDetails,
                           const char *pcszAutoConfirmId /* = 0*/) const
{
    message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
            AlertButton_Ok | AlertButtonOption_Default | AlertButtonOption_Escape);
}

bool UIMessageCenter::errorWithQuestion(QWidget *pParent, MessageType type,
                                        const QString &strMessage,
                                        const QString &strDetails,
                                        const char *pcszAutoConfirmId /* = 0*/,
                                        const QString &strOkButtonText /* = QString()*/,
                                        const QString &strCancelButtonText /* = QString()*/) const
{
    return (message(pParent, type, strMessage, strDetails, pcszAutoConfirmId,
                    AlertButton_Ok | AlertButtonOption_Default,
                    AlertButton_Cancel | AlertButtonOption_Escape,
                    0 /* third button */,
                    strOkButtonText,
                    strCancelButtonText,
                    QString() /* third button */) &
            AlertButtonMask) == AlertButton_Ok;
}

void UIMessageCenter::alert(QWidget *pParent, MessageType type,
                           const QString &strMessage,
                           const char *pcszAutoConfirmId /* = 0*/) const
{
    error(pParent, type, strMessage, QString(), pcszAutoConfirmId);
}

int UIMessageCenter::question(QWidget *pParent, MessageType type,
                              const QString &strMessage,
                              const char *pcszAutoConfirmId/* = 0*/,
                              int iButton1 /* = 0*/,
                              int iButton2 /* = 0*/,
                              int iButton3 /* = 0*/,
                              const QString &strButtonText1 /* = QString()*/,
                              const QString &strButtonText2 /* = QString()*/,
                              const QString &strButtonText3 /* = QString()*/) const
{
    return message(pParent, type, strMessage, QString(), pcszAutoConfirmId,
                   iButton1, iButton2, iButton3, strButtonText1, strButtonText2, strButtonText3);
}

bool UIMessageCenter::questionBinary(QWidget *pParent, MessageType type,
                                     const QString &strMessage,
                                     const char *pcszAutoConfirmId /* = 0*/,
                                     const QString &strOkButtonText /* = QString()*/,
                                     const QString &strCancelButtonText /* = QString()*/,
                                     bool fDefaultFocusForOk /* = true*/) const
{
    return fDefaultFocusForOk ?
           ((question(pParent, type, strMessage, pcszAutoConfirmId,
                      AlertButton_Ok | AlertButtonOption_Default,
                      AlertButton_Cancel | AlertButtonOption_Escape,
                      0 /* third button */,
                      strOkButtonText,
                      strCancelButtonText,
                      QString() /* third button */) &
             AlertButtonMask) == AlertButton_Ok) :
           ((question(pParent, type, strMessage, pcszAutoConfirmId,
                      AlertButton_Ok,
                      AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape,
                      0 /* third button */,
                      strOkButtonText,
                      strCancelButtonText,
                      QString() /* third button */) &
             AlertButtonMask) == AlertButton_Ok);
}

int UIMessageCenter::questionTrinary(QWidget *pParent, MessageType type,
                                     const QString &strMessage,
                                     const char *pcszAutoConfirmId /* = 0*/,
                                     const QString &strChoice1ButtonText /* = QString()*/,
                                     const QString &strChoice2ButtonText /* = QString()*/,
                                     const QString &strCancelButtonText /* = QString()*/) const
{
    return question(pParent, type, strMessage, pcszAutoConfirmId,
                    AlertButton_Choice1,
                    AlertButton_Choice2 | AlertButtonOption_Default,
                    AlertButton_Cancel | AlertButtonOption_Escape,
                    strChoice1ButtonText,
                    strChoice2ButtonText,
                    strCancelButtonText);
}

int UIMessageCenter::messageWithOption(QWidget *pParent, MessageType type,
                                       const QString &strMessage,
                                       const QString &strOptionText,
                                       bool fDefaultOptionValue /* = true */,
                                       int iButton1 /* = 0*/,
                                       int iButton2 /* = 0*/,
                                       int iButton3 /* = 0*/,
                                       const QString &strButtonName1 /* = QString() */,
                                       const QString &strButtonName2 /* = QString() */,
                                       const QString &strButtonName3 /* = QString() */) const
{
    /* If no buttons are set, using single 'OK' button: */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default;

    /* Assign corresponding title and icon: */
    QString strTitle;
    AlertIconType icon;
    switch (type)
    {
        default:
        case MessageType_Info:
            strTitle = tr("VirtualBox - Information", "msg box title");
            icon = AlertIconType_Information;
            break;
        case MessageType_Question:
            strTitle = tr("VirtualBox - Question", "msg box title");
            icon = AlertIconType_Question;
            break;
        case MessageType_Warning:
            strTitle = tr("VirtualBox - Warning", "msg box title");
            icon = AlertIconType_Warning;
            break;
        case MessageType_Error:
            strTitle = tr("VirtualBox - Error", "msg box title");
            icon = AlertIconType_Critical;
            break;
        case MessageType_Critical:
            strTitle = tr("VirtualBox - Critical Error", "msg box title");
            icon = AlertIconType_Critical;
            break;
        case MessageType_GuruMeditation:
            strTitle = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = AlertIconType_GuruMeditation;
            break;
    }

    /* Create message-box: */
    QWidget *pBoxParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    QPointer<QIMessageBox> pBox = new QIMessageBox(strTitle, strMessage, icon,
                                                   iButton1, iButton2, iButton3, pBoxParent);
    windowManager().registerNewParent(pBox, pBoxParent);

    /* Load option: */
    if (!strOptionText.isNull())
    {
        pBox->setFlagText(strOptionText);
        pBox->setFlagChecked(fDefaultOptionValue);
    }

    /* Configure button-text: */
    if (!strButtonName1.isNull())
        pBox->setButtonText(0, strButtonName1);
    if (!strButtonName2.isNull())
        pBox->setButtonText(1, strButtonName2);
    if (!strButtonName3.isNull())
        pBox->setButtonText(2, strButtonName3);

    /* Show box: */
    int rc = pBox->exec();

    /* Make sure box still valid: */
    if (!pBox)
        return rc;

    /* Save option: */
    if (pBox->flagChecked())
        rc |= AlertOption_CheckBox;

    /* Delete message-box: */
    if (pBox)
        delete pBox;

    return rc;
}

bool UIMessageCenter::showModalProgressDialog(CProgress &progress,
                                              const QString &strTitle,
                                              const QString &strImage /* = "" */,
                                              QWidget *pParent /* = 0*/,
                                              int cMinDuration /* = 2000 */)
{
    /* Prepare pixmap: */
    QPixmap *pPixmap = NULL;
    if (!strImage.isEmpty())
        pPixmap = new QPixmap(strImage);

    /* Create progress-dialog: */
    QWidget *pDlgParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    QPointer<UIProgressDialog> pProgressDlg = new UIProgressDialog(progress, strTitle, pPixmap, cMinDuration, pDlgParent);
    windowManager().registerNewParent(pProgressDlg, pDlgParent);

    /* Run the dialog with the 350 ms refresh interval. */
    pProgressDlg->run(350);

    /* Make sure progress-dialog still valid: */
    bool fRc;
    if (pProgressDlg)
    {
        /* Delete progress-dialog: */
        delete pProgressDlg;

        fRc = true;
    }
    else
        fRc = false;

    /* Cleanup pixmap: */
    if (pPixmap)
        delete pPixmap;

    return fRc;
}

#ifdef RT_OS_LINUX
void UIMessageCenter::warnAboutWrongUSBMounted() const
{
    alert(0, MessageType_Warning,
          tr("You seem to have the USBFS filesystem mounted at /sys/bus/usb/drivers. "
             "We strongly recommend that you change this, as it is a severe mis-configuration of "
             "your system which could cause USB devices to fail in unexpected ways."),
          "warnAboutWrongUSBMounted");
}
#endif /* RT_OS_LINUX */

void UIMessageCenter::cannotStartSelector() const
{
    alert(0, MessageType_Critical,
          tr("<p>Cannot start the VirtualBox Manager due to local restrictions.</p>"
             "<p>The application will now terminate.</p>"));
}

void UIMessageCenter::showBetaBuildWarning() const
{
    alert(0, MessageType_Warning,
          tr("You are running a prerelease version of VirtualBox. "
             "This version is not suitable for production use."));
}

void UIMessageCenter::showExperimentalBuildWarning() const
{
    alert(0, MessageType_Warning,
          tr("You are running an EXPERIMENTAL build of VirtualBox. "
             "This version is not suitable for production use."));
}

void UIMessageCenter::cannotInitUserHome(const QString &strUserHome) const
{
    error(0, MessageType_Critical,
          tr("<p>Failed to initialize COM because the VirtualBox global "
             "configuration directory <b><nobr>%1</nobr></b> is not accessible. "
             "Please check the permissions of this directory and of its parent directory.</p>"
             "<p>The application will now terminate.</p>")
             .arg(strUserHome),
          formatErrorInfo(COMErrorInfo()));
}

void UIMessageCenter::cannotInitCOM(HRESULT rc) const
{
    error(0, MessageType_Critical,
          tr("<p>Failed to initialize COM or to find the VirtualBox COM server. "
             "Most likely, the VirtualBox server is not running or failed to start.</p>"
             "<p>The application will now terminate.</p>"),
          formatErrorInfo(COMErrorInfo(), rc));
}

void UIMessageCenter::cannotCreateVirtualBoxClient(const CVirtualBoxClient &client) const
{
    error(0, MessageType_Critical,
          tr("<p>Failed to create the VirtualBoxClient COM object.</p>"
             "<p>The application will now terminate.</p>"),
          formatErrorInfo(client));
}

void UIMessageCenter::cannotAcquireVirtualBox(const CVirtualBoxClient &client) const
{
    QString err = tr("<p>Failed to acquire the VirtualBox COM object.</p>"
                     "<p>The application will now terminate.</p>");
#if defined(VBOX_WS_X11) || defined(VBOX_WS_MAC)
    if (client.lastRC() == NS_ERROR_SOCKET_FAIL)
        err += tr("<p>The reason for this error are most likely wrong permissions of the IPC "
                  "daemon socket due to an installation problem. Please check the permissions of "
                  "<font color=blue>'/tmp'</font> and <font color=blue>'/tmp/.vbox-*-ipc/'</font></p>");
#endif
    error(0, MessageType_Critical, err, formatErrorInfo(client));
}

void UIMessageCenter::cannotFindLanguage(const QString &strLangId, const QString &strNlsPath) const
{
    alert(0, MessageType_Error,
          tr("<p>Could not find a language file for the language <b>%1</b> in the directory <b><nobr>%2</nobr></b>.</p>"
             "<p>The language will be temporarily reset to the system default language. "
             "Please go to the <b>Preferences</b> window which you can open from the <b>File</b> menu of the "
             "VirtualBox Manager window, and select one of the existing languages on the <b>Language</b> page.</p>")
             .arg(strLangId).arg(strNlsPath));
}

void UIMessageCenter::cannotLoadLanguage(const QString &strLangFile) const
{
    alert(0, MessageType_Error,
          tr("<p>Could not load the language file <b><nobr>%1</nobr></b>. "
             "<p>The language will be temporarily reset to English (built-in). "
             "Please go to the <b>Preferences</b> window which you can open from the <b>File</b> menu of the "
             "VirtualBox Manager window, and select one of the existing languages on the <b>Language</b> page.</p>")
             .arg(strLangFile));
}

void UIMessageCenter::cannotLoadGlobalConfig(const CVirtualBox &vbox, const QString &strError) const
{
    error(0, MessageType_Critical,
          tr("<p>Failed to load the global GUI configuration from <b><nobr>%1</nobr></b>.</p>"
             "<p>The application will now terminate.</p>")
             .arg(CVirtualBox(vbox).GetSettingsFilePath()),
          !vbox.isOk() ? formatErrorInfo(vbox) : QString("<!--EOM--><p>%1</p>").arg(vboxGlobal().emphasize(strError)));
}

void UIMessageCenter::cannotSaveGlobalConfig(const CVirtualBox &vbox) const
{
    error(0, MessageType_Critical,
          tr("<p>Failed to save the global GUI configuration to <b><nobr>%1</nobr></b>.</p>"
             "<p>The application will now terminate.</p>")
             .arg(CVirtualBox(vbox).GetSettingsFilePath()),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotFindMachineByName(const CVirtualBox &vbox, const QString &strName) const
{
    error(0, MessageType_Error,
          tr("There is no virtual machine named <b>%1</b>.")
             .arg(strName),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotFindMachineById(const CVirtualBox &vbox, const QString &strId) const
{
    error(0, MessageType_Error,
          tr("There is no virtual machine with the identifier <b>%1</b>.")
             .arg(strId),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotOpenSession(const CSession &session) const
{
    error(0, MessageType_Error,
          tr("Failed to create a new session."),
          formatErrorInfo(session));
}

void UIMessageCenter::cannotOpenSession(const CMachine &machine) const
{
    error(0, MessageType_Error,
          tr("Failed to open a session for the virtual machine <b>%1</b>.")
             .arg(CMachine(machine).GetName()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotOpenSession(const CProgress &progress, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to open a session for the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotGetMediaAccessibility(const UIMedium &medium) const
{
    error(0, MessageType_Error,
          tr("Failed to access the disk image file <nobr><b>%1</b></nobr>.")
             .arg(medium.location()),
          formatErrorInfo(medium.result()));
}

void UIMessageCenter::cannotOpenURL(const QString &strUrl) const
{
    alert(0, MessageType_Error,
          tr("Failed to open <tt>%1</tt>. "
             "Make sure your desktop environment can properly handle URLs of this type.")
             .arg(strUrl));
}

void UIMessageCenter::cannotSetExtraData(const CVirtualBox &vbox, const QString &strKey, const QString &strValue)
{
    error(0, MessageType_Error,
          tr("Failed to set the global VirtualBox extra data for key <i>%1</i> to value <i>{%2}</i>.")
             .arg(strKey, strValue),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotSetExtraData(const CMachine &machine, const QString &strKey, const QString &strValue)
{
    error(0, MessageType_Error,
          tr("Failed to set the extra data for key <i>%1</i> of machine <i>%2</i> to value <i>{%3}</i>.")
             .arg(strKey, CMachine(machine).GetName(), strValue),
          formatErrorInfo(machine));
}

void UIMessageCenter::warnAboutInvalidEncryptionPassword(const QString &strPasswordId, QWidget *pParent /* = 0 */)
{
    alert(pParent, MessageType_Error,
          tr("Encryption password for <nobr>ID = '%1'</nobr> is invalid.")
             .arg(strPasswordId));
}

void UIMessageCenter::cannotOpenMachine(const CVirtualBox &vbox, const QString &strMachinePath) const
{
    error(0, MessageType_Error,
          tr("Failed to open virtual machine located in %1.")
             .arg(strMachinePath),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotReregisterExistingMachine(const QString &strMachinePath, const QString &strMachineName) const
{
    alert(0, MessageType_Error,
          tr("Failed to add virtual machine <b>%1</b> located in <i>%2</i> because its already present.")
             .arg(strMachineName, strMachinePath));
}

void UIMessageCenter::cannotResolveCollisionAutomatically(const QString &strCollisionName, const QString &strGroupName) const
{
    alert(0, MessageType_Error,
          tr("<p>You are trying to move machine <nobr><b>%1</b></nobr> "
             "to group <nobr><b>%2</b></nobr> which already have sub-group <nobr><b>%1</b></nobr>.</p>"
             "<p>Please resolve this name-conflict and try again.</p>")
             .arg(strCollisionName, strGroupName));
}

bool UIMessageCenter::confirmAutomaticCollisionResolve(const QString &strName, const QString &strGroupName) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>You are trying to move group <nobr><b>%1</b></nobr> "
                             "to group <nobr><b>%2</b></nobr> which already have another item with the same name.</p>"
                             "<p>Would you like to automatically rename it?</p>")
                             .arg(strName, strGroupName),
                          0 /* auto-confirm id */,
                          tr("Rename"));
}

void UIMessageCenter::cannotSetGroups(const CMachine &machine) const
{
    /* Compose machine name: */
    QString strName = CMachine(machine).GetName();
    if (strName.isEmpty())
        strName = QFileInfo(CMachine(machine).GetSettingsFilePath()).baseName();
    /* Show the error: */
    error(0, MessageType_Error,
          tr("Failed to set groups of the virtual machine <b>%1</b>.")
             .arg(strName),
          formatErrorInfo(machine));
}

bool UIMessageCenter::confirmMachineItemRemoval(const QStringList &names) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>You are about to remove following virtual machine items from the machine list:</p>"
                             "<p><b>%1</b></p><p>Do you wish to proceed?</p>")
                             .arg(names.join(", ")),
                          0 /* auto-confirm id */,
                          tr("Remove") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

int UIMessageCenter::confirmMachineRemoval(const QList<CMachine> &machines) const
{
    /* Enumerate the machines: */
    int cInacessibleMachineCount = 0;
    bool fMachineWithHardDiskPresent = false;
    QString strMachineNames;
    foreach (const CMachine &machine, machines)
    {
        /* Prepare machine name: */
        QString strMachineName;
        if (machine.GetAccessible())
        {
            /* Just get machine name: */
            strMachineName = machine.GetName();
            /* Enumerate the attachments: */
            const CMediumAttachmentVector &attachments = machine.GetMediumAttachments();
            foreach (const CMediumAttachment &attachment, attachments)
            {
                /* Check if the medium is a hard disk: */
                if (attachment.GetType() == KDeviceType_HardDisk)
                {
                    /* Check if that hard disk isn't shared.
                     * If hard disk is shared, it will *never* be deleted: */
                    QVector<QString> usedMachineList = attachment.GetMedium().GetMachineIds();
                    if (usedMachineList.size() == 1)
                    {
                        fMachineWithHardDiskPresent = true;
                        break;
                    }
                }
            }
        }
        else
        {
            /* Compose machine name: */
            QFileInfo fi(machine.GetSettingsFilePath());
            strMachineName = VBoxGlobal::hasAllowedExtension(fi.completeSuffix(), VBoxFileExts) ? fi.completeBaseName() : fi.fileName();
            /* Increment inacessible machine count: */
            ++cInacessibleMachineCount;
        }
        /* Append machine name to the full name string: */
        strMachineNames += QString(strMachineNames.isEmpty() ? "<b>%1</b>" : ", <b>%1</b>").arg(strMachineName);
    }

    /* Prepare message text: */
    QString strText = cInacessibleMachineCount == machines.size() ?
                      tr("<p>You are about to remove following inaccessible virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Do you wish to proceed?</p>")
                         .arg(strMachineNames) :
                      fMachineWithHardDiskPresent ?
                      tr("<p>You are about to remove following virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Would you like to delete the files containing the virtual machine from your hard disk as well? "
                         "Doing this will also remove the files containing the machine's virtual hard disks "
                         "if they are not in use by another machine.</p>")
                         .arg(strMachineNames) :
                      tr("<p>You are about to remove following virtual machines from the machine list:</p>"
                         "<p>%1</p>"
                         "<p>Would you like to delete the files containing the virtual machine from your hard disk as well?</p>")
                         .arg(strMachineNames);

    /* Prepare message itself: */
    return cInacessibleMachineCount == machines.size() ?
           message(0, MessageType_Question,
                   strText, QString(),
                   0 /* auto-confirm id */,
                   AlertButton_Ok,
                   AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape,
                   0,
                   tr("Remove")) :
           message(0, MessageType_Question,
                   strText, QString(),
                   0 /* auto-confirm id */,
                   AlertButton_Choice1,
                   AlertButton_Choice2,
                   AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape,
                   tr("Delete all files"),
                   tr("Remove only"));
}

void UIMessageCenter::cannotRemoveMachine(const CMachine &machine) const
{
    error(0, MessageType_Error,
          tr("Failed to remove the virtual machine <b>%1</b>.")
             .arg(CMachine(machine).GetName()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotRemoveMachine(const CMachine &machine, const CProgress &progress) const
{
    error(0, MessageType_Error,
          tr("Failed to remove the virtual machine <b>%1</b>.")
             .arg(CMachine(machine).GetName()),
          formatErrorInfo(progress));
}

bool UIMessageCenter::warnAboutInaccessibleMedia() const
{
    return questionBinary(0, MessageType_Warning,
                          tr("<p>One or more disk image files are not currently accessible. As a result, you will "
                             "not be able to operate virtual machines that use these files until "
                             "they become accessible later.</p>"
                             "<p>Press <b>Check</b> to open the Virtual Media Manager window and "
                             "see which files are inaccessible, or press <b>Ignore</b> to "
                             "ignore this message.</p>"),
                          "warnAboutInaccessibleMedia",
                          tr("Ignore"), tr("Check", "inaccessible media message box"));
}

bool UIMessageCenter::confirmDiscardSavedState(const QString &strNames) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Are you sure you want to discard the saved state of "
                             "the following virtual machines?</p><p><b>%1</b></p>"
                             "<p>This operation is equivalent to resetting or powering off "
                             "the machine without doing a proper shutdown of the guest OS.</p>")
                             .arg(strNames),
                          0 /* auto-confirm id */,
                          tr("Discard", "saved state"));
}

bool UIMessageCenter::confirmResetMachine(const QString &strNames) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Do you really want to reset the following virtual machines?</p>"
                             "<p><b>%1</b></p><p>This will cause any unsaved data "
                             "in applications running inside it to be lost.</p>")
                             .arg(strNames),
                          "confirmResetMachine" /* auto-confirm id */,
                          tr("Reset", "machine"));
}

bool UIMessageCenter::confirmACPIShutdownMachine(const QString &strNames) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Do you really want to send an ACPI shutdown signal "
                             "to the following virtual machines?</p><p><b>%1</b></p>")
                             .arg(strNames),
                          "confirmACPIShutdownMachine" /* auto-confirm id */,
                          tr("ACPI Shutdown", "machine"));
}

bool UIMessageCenter::confirmPowerOffMachine(const QString &strNames) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Do you really want to power off the following virtual machines?</p>"
                             "<p><b>%1</b></p><p>This will cause any unsaved data in applications "
                             "running inside it to be lost.</p>")
                             .arg(strNames),
                          "confirmPowerOffMachine" /* auto-confirm id */,
                          tr("Power Off", "machine"));
}

void UIMessageCenter::cannotPauseMachine(const CConsole &console) const
{
    error(0, MessageType_Error,
          tr("Failed to pause the execution of the virtual machine <b>%1</b>.")
             .arg(CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotResumeMachine(const CConsole &console) const
{
    error(0, MessageType_Error,
          tr("Failed to resume the execution of the virtual machine <b>%1</b>.")
             .arg(CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotDiscardSavedState(const CMachine &machine) const
{
    error(0, MessageType_Error,
          tr("Failed to discard the saved state of the virtual machine <b>%1</b>.")
             .arg(machine.GetName()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotSaveMachineState(const CMachine &machine)
{
    error(0, MessageType_Error,
          tr("Failed to save the state of the virtual machine <b>%1</b>.")
             .arg(machine.GetName()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotSaveMachineState(const CProgress &progress, const QString &strMachineName)
{
    error(0, MessageType_Error,
          tr("Failed to save the state of the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotACPIShutdownMachine(const CConsole &console) const
{
    error(0, MessageType_Error,
          tr("Failed to send the ACPI Power Button press event to the virtual machine <b>%1</b>.")
             .arg(CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotPowerDownMachine(const CConsole &console) const
{
    error(0, MessageType_Error,
          tr("Failed to stop the virtual machine <b>%1</b>.")
             .arg(CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotPowerDownMachine(const CProgress &progress, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to stop the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(progress));
}

bool UIMessageCenter::confirmStartMultipleMachines(const QString &strNames) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>You are about to start all of the following virtual machines:</p>"
                             "<p><b>%1</b></p><p>This could take some time and consume a lot of "
                             "host system resources. Do you wish to proceed?</p>").arg(strNames),
                          "confirmStartMultipleMachines" /* auto-confirm id */);
}

int UIMessageCenter::confirmSnapshotRestoring(const QString &strSnapshotName, bool fAlsoCreateNewSnapshot) const
{
    return fAlsoCreateNewSnapshot ?
           messageWithOption(0, MessageType_Question,
                             tr("<p>You are about to restore snapshot <nobr><b>%1</b></nobr>.</p>"
                                "<p>You can create a snapshot of the current state of the virtual machine first by checking the box below; "
                                "if you do not do this the current state will be permanently lost. Do you wish to proceed?</p>")
                                .arg(strSnapshotName),
                             tr("Create a snapshot of the current machine state"),
                             !gEDataManager->messagesWithInvertedOption().contains("confirmSnapshotRestoring"),
                             AlertButton_Ok,
                             AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape,
                             0 /* 3rd button */,
                             tr("Restore"), tr("Cancel"), QString() /* 3rd button text */) :
           message(0, MessageType_Question,
                   tr("<p>Are you sure you want to restore snapshot <nobr><b>%1</b></nobr>?</p>")
                      .arg(strSnapshotName),
                   QString() /* details */,
                   0 /* auto-confirm id */,
                   AlertButton_Ok,
                   AlertButton_Cancel | AlertButtonOption_Default | AlertButtonOption_Escape,
                   0 /* 3rd button */,
                   tr("Restore"), tr("Cancel"), QString() /* 3rd button text */);
}

bool UIMessageCenter::confirmSnapshotRemoval(const QString &strSnapshotName) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Deleting the snapshot will cause the state information saved in it to be lost, and storage data spread over "
                             "several image files that VirtualBox has created together with the snapshot will be merged into one file. "
                             "This can be a lengthy process, and the information in the snapshot cannot be recovered.</p>"
                             "</p>Are you sure you want to delete the selected snapshot <b>%1</b>?</p>")
                             .arg(strSnapshotName),
                          0 /* auto-confirm id */,
                          tr("Delete") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

bool UIMessageCenter::warnAboutSnapshotRemovalFreeSpace(const QString &strSnapshotName,
                                                        const QString &strTargetImageName,
                                                        const QString &strTargetImageMaxSize,
                                                        const QString &strTargetFileSystemFree) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Deleting the snapshot %1 will temporarily need more storage space. In the worst case the size of image %2 will grow by %3, "
                              "however on this filesystem there is only %4 free.</p><p>Running out of storage space during the merge operation can result in "
                              "corruption of the image and the VM configuration, i.e. loss of the VM and its data.</p><p>You may continue with deleting "
                              "the snapshot at your own risk.</p>")
                              .arg(strSnapshotName, strTargetImageName, strTargetImageMaxSize, strTargetFileSystemFree),
                          0 /* auto-confirm id */,
                          tr("Delete") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

void UIMessageCenter::cannotTakeSnapshot(const CMachine &machine, const QString &strMachineName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create a snapshot of the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotTakeSnapshot(const CProgress &progress, const QString &strMachineName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create a snapshot of the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(progress));
}

bool UIMessageCenter::cannotRestoreSnapshot(const CMachine &machine, const QString &strSnapshotName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
             .arg(strSnapshotName, strMachineName),
          formatErrorInfo(machine));
    return false;
}

bool UIMessageCenter::cannotRestoreSnapshot(const CProgress &progress, const QString &strSnapshotName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to restore the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
             .arg(strSnapshotName, strMachineName),
          formatErrorInfo(progress));
    return false;
}

void UIMessageCenter::cannotRemoveSnapshot(const CMachine &machine, const QString &strSnapshotName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
             .arg(strSnapshotName, strMachineName),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotRemoveSnapshot(const CProgress &progress, const QString &strSnapshotName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to delete the snapshot <b>%1</b> of the virtual machine <b>%2</b>.")
             .arg(strSnapshotName).arg(strMachineName),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotSaveSettings(const QString strDetails, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Failed to save the settings."),
          strDetails);
}

bool UIMessageCenter::confirmNATNetworkRemoval(const QString &strName, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>Do you want to remove the NAT network <nobr><b>%1</b>?</nobr></p>"
                             "<p>If this network is in use by one or more virtual "
                             "machine network adapters these adapters will no longer be "
                             "usable until you correct their settings by either choosing "
                             "a different network name or a different adapter attachment "
                             "type.</p>")
                             .arg(strName),
                          0 /* auto-confirm id */,
                          tr("Remove") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

bool UIMessageCenter::confirmHostOnlyInterfaceRemoval(const QString &strName, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>Deleting this host-only network will remove "
                             "the host-only interface this network is based on. Do you want to "
                             "remove the (host-only network) interface <nobr><b>%1</b>?</nobr></p>"
                             "<p><b>Note:</b> this interface may be in use by one or more "
                             "virtual network adapters belonging to one of your VMs. "
                             "After it is removed, these adapters will no longer be usable until "
                             "you correct their settings by either choosing a different interface "
                             "name or a different adapter attachment type.</p>")
                             .arg(strName),
                          0 /* auto-confirm id */,
                          tr("Remove") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

void UIMessageCenter::cannotCreateNATNetwork(const CVirtualBox &vbox, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to create NAT network."),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotRemoveNATNetwork(const CVirtualBox &vbox, const QString &strNetworkName, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to remove NAT network <b>%1</b>.")
             .arg(strNetworkName),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCreateDHCPServer(const CVirtualBox &vbox, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to create DHCP server."),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotRemoveDHCPServer(const CVirtualBox &vbox, const QString &strInterfaceName, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to remove DHCP server for network interface <b>%1</b>.")
             .arg(strInterfaceName),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCreateHostInterface(const CHost &host, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to create the host network interface."),
          formatErrorInfo(host));
}

void UIMessageCenter::cannotCreateHostInterface(const CProgress &progress, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to create the host network interface."),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotRemoveHostInterface(const CHost &host, const QString &strInterfaceName, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to remove the host network interface <b>%1</b>.")
             .arg(strInterfaceName),
          formatErrorInfo(host));
}

void UIMessageCenter::cannotRemoveHostInterface(const CProgress &progress, const QString &strInterfaceName, QWidget *pParent /* = 0*/)
{
    error(pParent, MessageType_Error,
          tr("Failed to remove the host network interface <b>%1</b>.")
             .arg(strInterfaceName),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotSetSystemProperties(const CSystemProperties &properties, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Critical,
          tr("Failed to set global VirtualBox properties."),
          formatErrorInfo(properties));
}

void UIMessageCenter::warnAboutUnaccessibleUSB(const COMBaseWithEI &object, QWidget *pParent /* = 0*/) const
{
    /* If IMachine::GetUSBController(), IHost::GetUSBDevices() etc. return
     * E_NOTIMPL, it means the USB support is intentionally missing
     * (as in the OSE version). Don't show the error message in this case. */
    COMResult res(object);
    if (res.rc() == E_NOTIMPL)
        return;
    /* Show the error: */
    error(pParent, res.isWarning() ? MessageType_Warning : MessageType_Error,
          tr("Failed to access the USB subsystem."),
          formatErrorInfo(res),
          "warnAboutUnaccessibleUSB");
}

void UIMessageCenter::warnAboutStateChange(QWidget *pParent /* = 0*/) const
{
    if (warningShown("warnAboutStateChange"))
        return;
    setWarningShown("warnAboutStateChange", true);

    alert(pParent, MessageType_Warning,
          tr("The virtual machine that you are changing has been started. "
             "Only certain settings can be changed while a machine is running. "
             "All other changes will be lost if you close this window now."));

    setWarningShown("warnAboutStateChange", false);
}

bool UIMessageCenter::confirmSettingsReloading(QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>The machine settings were changed while you were editing them. "
                             "You currently have unsaved setting changes.</p>"
                             "<p>Would you like to reload the changed settings or to keep your own changes?</p>"),
                          0 /* auto-confirm id */,
                          tr("Reload settings"), tr("Keep changes"));
}

int UIMessageCenter::confirmHardDiskAttachmentCreation(const QString &strControllerName, QWidget *pParent /* = 0*/) const
{
    return questionTrinary(pParent, MessageType_Question,
                           tr("<p>You are about to add a virtual hard disk to controller <b>%1</b>.</p>"
                              "<p>Would you like to create a new, empty file to hold the disk contents or select an existing one?</p>")
                              .arg(strControllerName),
                           0 /* auto-confirm id */,
                           tr("Create &new disk"), tr("&Choose existing disk"));
}

int UIMessageCenter::confirmOpticalAttachmentCreation(const QString &strControllerName, QWidget *pParent /* = 0*/) const
{
    return questionTrinary(pParent, MessageType_Question,
                           tr("<p>You are about to add a new optical drive to controller <b>%1</b>.</p>"
                              "<p>Would you like to choose a virtual optical disk to put in the drive "
                              "or to leave it empty for now?</p>")
                              .arg(strControllerName),
                           0 /* auto-confirm id */,
                           tr("Leave &empty"), tr("&Choose disk"));
}

int UIMessageCenter::confirmFloppyAttachmentCreation(const QString &strControllerName, QWidget *pParent /* = 0*/) const
{
    return questionTrinary(pParent, MessageType_Question,
                           tr("<p>You are about to add a new floppy drive to controller <b>%1</b>.</p>"
                              "<p>Would you like to choose a virtual floppy disk to put in the drive "
                              "or to leave it empty for now?</p>")
                              .arg(strControllerName),
                           0 /* auto-confirm id */,
                           tr("Leave &empty"), tr("&Choose disk"));
}

int UIMessageCenter::confirmRemovingOfLastDVDDevice(QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Info,
                          tr("<p>Are you sure you want to delete the optical drive?</p>"
                             "<p>You will not be able to insert any optical disks or ISO images "
                             "or install the Guest Additions without it!</p>"),
                          0 /* auto-confirm id */,
                          tr("&Remove", "medium") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

void UIMessageCenter::cannotSaveAudioSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save audio settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveAudioAdapterSettings(const CAudioAdapter &comAdapter, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save audio adapter settings."),
          formatErrorInfo(comAdapter));
}

void UIMessageCenter::cannotSaveDisplaySettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save display settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveRemoteDisplayServerSettings(const CVRDEServer &comServer, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save remote display server settings."),
          formatErrorInfo(comServer));
}

void UIMessageCenter::cannotSaveGeneralSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save general settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveStorageAttachmentSettings(const CMediumAttachment &comAttachment, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save storage attachment settings."),
          formatErrorInfo(comAttachment));
}

void UIMessageCenter::cannotSaveStorageMediumSettings(const CMedium &comMedium, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save storage medium settings."),
          formatErrorInfo(comMedium));
}

void UIMessageCenter::cannotSaveInterfaceSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save user interface settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveNetworkSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save network settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveNetworkAdapterSettings(const CNetworkAdapter &comAdapter, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save network adapter settings."),
          formatErrorInfo(comAdapter));
}

void UIMessageCenter::cannotSaveNATEngineSettings(const CNATEngine &comEngine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save NAT engine settings."),
          formatErrorInfo(comEngine));
}

void UIMessageCenter::cannotSaveParallelSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save parallel ports settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveParallelPortSettings(const CParallelPort &comPort, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save parallel port settings."),
          formatErrorInfo(comPort));
}

void UIMessageCenter::cannotSaveSerialSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save serial ports settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveSerialPortSettings(const CSerialPort &comPort, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save serial port settings."),
          formatErrorInfo(comPort));
}

void UIMessageCenter::cannotLoadFoldersSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot load shared folders settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveFoldersSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save shared folders settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotLoadFoldersSettings(const CConsole &comConsole, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot load shared folders settings."),
          formatErrorInfo(comConsole));
}

void UIMessageCenter::cannotSaveFoldersSettings(const CConsole &comConsole, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save shared folders settings."),
          formatErrorInfo(comConsole));
}

void UIMessageCenter::cannotLoadFolderSettings(const CSharedFolder &comFolder, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot load shared folder settings."),
          formatErrorInfo(comFolder));
}

void UIMessageCenter::cannotSaveFolderSettings(const CSharedFolder &comFolder, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save shared folder settings."),
          formatErrorInfo(comFolder));
}

void UIMessageCenter::cannotSaveStorageSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save storage settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveStorageControllerSettings(const CStorageController &comController, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save storage controller settings."),
          formatErrorInfo(comController));
}

void UIMessageCenter::cannotSaveSystemSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save system settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveUSBSettings(const CMachine &comMachine, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save USB settings."),
          formatErrorInfo(comMachine));
}

void UIMessageCenter::cannotSaveUSBControllerSettings(const CUSBController &comController, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save USB controller settings."),
          formatErrorInfo(comController));
}

void UIMessageCenter::cannotSaveUSBDeviceFiltersSettings(const CUSBDeviceFilters &comFilters, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save USB device filters settings."),
          formatErrorInfo(comFilters));
}

void UIMessageCenter::cannotSaveUSBDeviceFilterSettings(const CUSBDeviceFilter &comFilter, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Cannot save USB device filter settings."),
          formatErrorInfo(comFilter));
}

void UIMessageCenter::cannotAttachDevice(const CMachine &machine, UIMediumType type,
                                         const QString &strLocation, const StorageSlot &storageSlot,
                                         QWidget *pParent /* = 0*/)
{
    QString strMessage;
    switch (type)
    {
        case UIMediumType_HardDisk:
        {
            strMessage = tr("Failed to attach the hard disk (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_DVD:
        {
            strMessage = tr("Failed to attach the optical drive (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        case UIMediumType_Floppy:
        {
            strMessage = tr("Failed to attach the floppy drive (<nobr><b>%1</b></nobr>) to the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation).arg(gpConverter->toString(storageSlot)).arg(CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    error(pParent, MessageType_Error,
          strMessage, formatErrorInfo(machine));
}

bool UIMessageCenter::warnAboutIncorrectPort(QWidget *pParent /* = 0 */) const
{
    alert(pParent, MessageType_Error,
          tr("The current port forwarding rules are not valid. "
             "None of the host or guest port values may be set to zero."));
    return false;
}

bool UIMessageCenter::warnAboutIncorrectAddress(QWidget *pParent /* = 0 */) const
{
    alert(pParent, MessageType_Error,
          tr("The current port forwarding rules are not valid. "
             "All of the host or guest address values should be correct or empty."));
    return false;
}

bool UIMessageCenter::warnAboutEmptyGuestAddress(QWidget *pParent /* = 0 */) const
{
    alert(pParent, MessageType_Error,
          tr("The current port forwarding rules are not valid. "
             "None of the guest address values may be empty."));
    return false;
}

bool UIMessageCenter::warnAboutNameShouldBeUnique(QWidget *pParent /* = 0 */) const
{
    alert(pParent, MessageType_Error,
          tr("The current port forwarding rules are not valid. "
             "Rule names should be unique."));
    return false;
}

bool UIMessageCenter::warnAboutRulesConflict(QWidget *pParent /* = 0 */) const
{
    alert(pParent, MessageType_Error,
          tr("The current port forwarding rules are not valid. "
             "Few rules have same host ports and conflicting IP addresses."));
    return false;
}

bool UIMessageCenter::confirmCancelingPortForwardingDialog(QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>There are unsaved changes in the port forwarding configuration.</p>"
                             "<p>If you proceed your changes will be discarded.</p>"),
                          0 /* auto-confirm id */,
                          QString() /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

void UIMessageCenter::cannotSaveMachineSettings(const CMachine &machine, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to save the settings of the virtual machine <b>%1</b> to <b><nobr>%2</nobr></b>.")
             .arg(machine.GetName(), CMachine(machine).GetSettingsFilePath()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotChangeMediumType(const CMedium &medium, KMediumType oldMediumType, KMediumType newMediumType, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("<p>Error changing disk image mode from <b>%1</b> to <b>%2</b>.</p>")
             .arg(gpConverter->toString(oldMediumType)).arg(gpConverter->toString(newMediumType)),
          formatErrorInfo(medium));
}

bool UIMessageCenter::confirmMediumRelease(const UIMedium &medium, QWidget *pParent /* = 0*/) const
{
    /* Prepare the usage: */
    QStringList usage;
    CVirtualBox vbox = vboxGlobal().virtualBox();
    foreach (const QString &strMachineID, medium.curStateMachineIds())
    {
        CMachine machine = vbox.FindMachine(strMachineID);
        if (!vbox.isOk() || machine.isNull())
            continue;
        usage << machine.GetName();
    }
    /* Show the question: */
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>Are you sure you want to release the disk image file <nobr><b>%1</b></nobr>?</p>"
                             "<p>This will detach it from the following virtual machine(s): <b>%2</b>.</p>")
                             .arg(medium.location(), usage.join(", ")),
                          0 /* auto-confirm id */,
                          tr("Release", "detach medium"));
}

bool UIMessageCenter::confirmMediumRemoval(const UIMedium &medium, QWidget *pParent /* = 0*/) const
{
    /* Prepare the message: */
    QString strMessage;
    switch (medium.type())
    {
        case UIMediumType_HardDisk:
        {
            strMessage = tr("<p>Are you sure you want to remove the virtual hard disk "
                            "<nobr><b>%1</b></nobr> from the list of known disk image files?</p>");
            /* Compose capabilities flag: */
            qulonglong caps = 0;
            QVector<KMediumFormatCapabilities> capabilities;
            capabilities = medium.medium().GetMediumFormat().GetCapabilities();
            for (int i = 0; i < capabilities.size(); ++i)
                caps |= capabilities[i];
            /* Check capabilities for additional options: */
            if (caps & KMediumFormatCapabilities_File)
            {
                if (medium.state() == KMediumState_Inaccessible)
                    strMessage += tr("<p>As this hard disk is inaccessible its image file"
                                     " can not be deleted.</p>");
            }
            break;
        }
        case UIMediumType_DVD:
        {
            strMessage = tr("<p>Are you sure you want to remove the virtual optical disk "
                            "<nobr><b>%1</b></nobr> from the list of known disk image files?</p>");
            strMessage += tr("<p>Note that the storage unit of this medium will not be "
                             "deleted and that it will be possible to use it later again.</p>");
            break;
        }
        case UIMediumType_Floppy:
        {
            strMessage = tr("<p>Are you sure you want to remove the virtual floppy disk "
                            "<nobr><b>%1</b></nobr> from the list of known disk image files?</p>");
            strMessage += tr("<p>Note that the storage unit of this medium will not be "
                             "deleted and that it will be possible to use it later again.</p>");
            break;
        }
        default:
            break;
    }
    /* Show the question: */
    return questionBinary(pParent, MessageType_Question,
                          strMessage.arg(medium.location()),
                          0 /* auto-confirm id */,
                          tr("Remove", "medium") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

int UIMessageCenter::confirmDeleteHardDiskStorage(const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    return questionTrinary(pParent, MessageType_Question,
                           tr("<p>Do you want to delete the storage unit of the virtual hard disk "
                              "<nobr><b>%1</b></nobr>?</p>"
                              "<p>If you select <b>Delete</b> then the specified storage unit "
                              "will be permanently deleted. This operation <b>cannot be "
                              "undone</b>.</p>"
                              "<p>If you select <b>Keep</b> then the hard disk will be only "
                              "removed from the list of known hard disks, but the storage unit "
                              "will be left untouched which makes it possible to add this hard "
                              "disk to the list later again.</p>")
                              .arg(strLocation),
                           0 /* auto-confirm id */,
                           tr("Delete", "hard disk storage"),
                           tr("Keep", "hard disk storage"));
}

void UIMessageCenter::cannotDeleteHardDiskStorage(const CMedium &medium, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to delete the storage unit of the hard disk <b>%1</b>.")
             .arg(strLocation),
          formatErrorInfo(medium));
}

void UIMessageCenter::cannotDeleteHardDiskStorage(const CProgress &progress, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to delete the storage unit of the hard disk <b>%1</b>.")
             .arg(strLocation),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotDetachDevice(const CMachine &machine, UIMediumType type, const QString &strLocation, const StorageSlot &storageSlot, QWidget *pParent /* = 0*/) const
{
    /* Prepare the message: */
    QString strMessage;
    switch (type)
    {
        case UIMediumType_HardDisk:
        {
            strMessage = tr("Failed to detach the hard disk (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation, gpConverter->toString(storageSlot), CMachine(machine).GetName());
            break;
        }
        case UIMediumType_DVD:
        {
            strMessage = tr("Failed to detach the optical drive (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation, gpConverter->toString(storageSlot), CMachine(machine).GetName());
            break;
        }
        case UIMediumType_Floppy:
        {
            strMessage = tr("Failed to detach the floppy drive (<nobr><b>%1</b></nobr>) from the slot <i>%2</i> of the machine <b>%3</b>.")
                            .arg(strLocation, gpConverter->toString(storageSlot), CMachine(machine).GetName());
            break;
        }
        default:
            break;
    }
    /* Show the error: */
    error(pParent, MessageType_Error, strMessage, formatErrorInfo(machine));
}

bool UIMessageCenter::cannotRemountMedium(const CMachine &machine, const UIMedium &medium, bool fMount, bool fRetry, QWidget *pParent /* = 0*/) const
{
    /* Compose the message: */
    QString strMessage;
    switch (medium.type())
    {
        case UIMediumType_DVD:
        {
            if (fMount)
            {
                strMessage = tr("<p>Unable to insert the virtual optical disk <nobr><b>%1</b></nobr> into the machine <b>%2</b>.</p>");
                if (fRetry)
                    strMessage += tr("<p>Would you like to try to force insertion of this disk?</p>");
            }
            else
            {
                strMessage = tr("<p>Unable to eject the virtual optical disk <nobr><b>%1</b></nobr> from the machine <b>%2</b>.</p>");
                if (fRetry)
                    strMessage += tr("<p>Would you like to try to force ejection of this disk?</p>");
            }
            break;
        }
        case UIMediumType_Floppy:
        {
            if (fMount)
            {
                strMessage = tr("<p>Unable to insert the virtual floppy disk <nobr><b>%1</b></nobr> into the machine <b>%2</b>.</p>");
                if (fRetry)
                    strMessage += tr("<p>Would you like to try to force insertion of this disk?</p>");
            }
            else
            {
                strMessage = tr("<p>Unable to eject the virtual floppy disk <nobr><b>%1</b></nobr> from the machine <b>%2</b>.</p>");
                if (fRetry)
                    strMessage += tr("<p>Would you like to try to force ejection of this disk?</p>");
            }
            break;
        }
        default:
            break;
    }
    /* Show the messsage: */
    if (fRetry)
        return errorWithQuestion(pParent, MessageType_Question,
                                 strMessage.arg(medium.isHostDrive() ? medium.name() : medium.location(), CMachine(machine).GetName()),
                                 formatErrorInfo(machine),
                                 0 /* Auto Confirm ID */,
                                 tr("Force Unmount"));
    error(pParent, MessageType_Error,
          strMessage.arg(medium.isHostDrive() ? medium.name() : medium.location(), CMachine(machine).GetName()),
          formatErrorInfo(machine));
    return false;
}

void UIMessageCenter::cannotOpenMedium(const CVirtualBox &vbox, UIMediumType /* type */, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    /* Show the error: */
    error(pParent, MessageType_Error,
          tr("Failed to open the disk image file <nobr><b>%1</b></nobr>.").arg(strLocation), formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCloseMedium(const UIMedium &medium, const COMResult &rc, QWidget *pParent /* = 0*/) const
{
    /* Show the error: */
    error(pParent, MessageType_Error,
          tr("Failed to close the disk image file <nobr><b>%1</b></nobr>.").arg(medium.location()), formatErrorInfo(rc));
}

bool UIMessageCenter::confirmHardDisklessMachine(QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Warning,
                          tr("You are about to create a new virtual machine without a hard disk. "
                             "You will not be able to install an operating system on the machine "
                             "until you add one. In the mean time you will only be able to start the "
                             "machine using a virtual optical disk or from the network."),
                          0 /* auto-confirm id */,
                          tr("Continue", "no hard disk attached"),
                          tr("Go Back", "no hard disk attached"));
}

void UIMessageCenter::cannotCreateMachine(const CVirtualBox &vbox, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create a new virtual machine."),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotRegisterMachine(const CVirtualBox &vbox, const QString &strMachineName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to register the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCreateClone(const CMachine &machine, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to clone the virtual machine <b>%1</b>.")
             .arg(CMachine(machine).GetName()),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotCreateClone(const CProgress &progress, const QString &strMachineName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to clone the virtual machine <b>%1</b>.")
             .arg(strMachineName),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotOverwriteHardDiskStorage(const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    alert(pParent, MessageType_Info,
          tr("<p>The hard disk storage unit at location <b>%1</b> already exists. "
             "You cannot create a new virtual hard disk that uses this location "
             "because it can be already used by another virtual hard disk.</p>"
             "<p>Please specify a different location.</p>")
             .arg(strLocation));
}

void UIMessageCenter::cannotCreateHardDiskStorage(const CVirtualBox &vbox, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
             .arg(strLocation),
          formatErrorInfo(vbox));
}

void UIMessageCenter::cannotCreateHardDiskStorage(const CMedium &medium, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
             .arg(strLocation),
          formatErrorInfo(medium));
}

void UIMessageCenter::cannotCreateHardDiskStorage(const CProgress &progress, const QString &strLocation, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to create the hard disk storage <nobr><b>%1</b>.</nobr>")
             .arg(strLocation),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotRemoveMachineFolder(const QString &strFolderName, QWidget *pParent /* = 0*/) const
{
    alert(pParent, MessageType_Critical,
          tr("<p>Cannot remove the machine folder <nobr><b>%1</b>.</nobr></p>"
             "<p>Please check that this folder really exists and that you have permissions to remove it.</p>")
             .arg(QFileInfo(strFolderName).fileName()));
}

void UIMessageCenter::cannotRewriteMachineFolder(const QString &strFolderName, QWidget *pParent /* = 0*/) const
{
    QFileInfo fi(strFolderName);
    alert(pParent, MessageType_Critical,
          tr("<p>Cannot create the machine folder <b>%1</b> in the parent folder <nobr><b>%2</b>.</nobr></p>"
             "<p>This folder already exists and possibly belongs to another machine.</p>")
             .arg(fi.fileName()).arg(fi.absolutePath()));
}

void UIMessageCenter::cannotCreateMachineFolder(const QString &strFolderName, QWidget *pParent /* = 0*/) const
{
    QFileInfo fi(strFolderName);
    alert(pParent, MessageType_Critical,
          tr("<p>Cannot create the machine folder <b>%1</b> in the parent folder <nobr><b>%2</b>.</nobr></p>"
             "<p>Please check that the parent really exists and that you have permissions to create the machine folder.</p>")
             .arg(fi.fileName()).arg(fi.absolutePath()));
}

void UIMessageCenter::cannotImportAppliance(CAppliance &appliance, QWidget *pParent /* = 0*/) const
{
    /* Preserve error-info: */
    QString strErrorInfo = formatErrorInfo(appliance);
    /* Add the warnings in the case of an early error: */
    QString strWarningInfo;
    foreach(const QString &strWarning, appliance.GetWarnings())
        strWarningInfo += QString("<br />Warning: %1").arg(strWarning);
    if (!strWarningInfo.isEmpty())
        strWarningInfo = "<br />" + strWarningInfo;
    /* Show the error: */
    error(pParent, MessageType_Error,
          tr("Failed to open/interpret appliance <b>%1</b>.")
             .arg(appliance.GetPath()),
          strWarningInfo + strErrorInfo);
}

void UIMessageCenter::cannotImportAppliance(const CProgress &progress, const QString &strPath, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to import appliance <b>%1</b>.")
             .arg(strPath),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotCheckFiles(const CProgress &progress, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to check files."),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotRemoveFiles(const CProgress &progress, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to remove file."),
          formatErrorInfo(progress));
}

bool UIMessageCenter::confirmExportMachinesInSaveState(const QStringList &machineNames, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Warning,
                          tr("<p>The %n following virtual machine(s) are currently in a saved state: <b>%1</b></p>"
                             "<p>If you continue the runtime state of the exported machine(s) will be discarded. "
                             "The other machine(s) will not be changed.</p>",
                             "This text is never used with n == 0. Feel free to drop the %n where possible, "
                             "we only included it because of problems with Qt Linguist (but the user can see "
                             "how many machines are in the list and doesn't need to be told).", machineNames.size())
                             .arg(machineNames.join(", ")),
                          0 /* auto-confirm id */,
                          tr("Continue"));
}

void UIMessageCenter::cannotExportAppliance(const CAppliance &appliance, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to prepare the export of the appliance <b>%1</b>.")
             .arg(CAppliance(appliance).GetPath()),
          formatErrorInfo(appliance));
}

void UIMessageCenter::cannotExportAppliance(const CMachine &machine, const QString &strPath, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to prepare the export of the appliance <b>%1</b>.")
             .arg(strPath),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotExportAppliance(const CProgress &progress, const QString &strPath, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to export appliance <b>%1</b>.")
             .arg(strPath),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotFindSnapshotByName(const CMachine &machine, const QString &strName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Can't find snapshot named <b>%1</b>.")
             .arg(strName),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotAddDiskEncryptionPassword(const CAppliance &appliance, QWidget *pParent /* = 0 */)
{
    error(pParent, MessageType_Error,
          tr("Bad password or authentication failure."),
          formatErrorInfo(appliance));
}

void UIMessageCenter::showRuntimeError(const CConsole &console, bool fFatal, const QString &strErrorId, const QString &strErrorMsg) const
{
    /* Prepare auto-confirm id: */
    QByteArray autoConfimId = "showRuntimeError.";

    /* Prepare variables: */
    CConsole console1 = console;
    KMachineState state = console1.GetState();
    MessageType type;
    QString severity;

    /// @todo Move to Runtime UI!
    /* Preprocessing: */
    if (fFatal)
    {
        /* The machine must be paused on fFatal errors: */
        Assert(state == KMachineState_Paused);
        if (state != KMachineState_Paused)
            console1.Pause();
    }

    /* Compose type, severity, advance confirm id: */
    if (fFatal)
    {
        type = MessageType_Critical;
        severity = tr("<nobr>Fatal Error</nobr>", "runtime error info");
        autoConfimId += "fatal.";
    }
    else if (state == KMachineState_Paused)
    {
        type = MessageType_Error;
        severity = tr("<nobr>Non-Fatal Error</nobr>", "runtime error info");
        autoConfimId += "error.";
    }
    else
    {
        type = MessageType_Warning;
        severity = tr("<nobr>Warning</nobr>", "runtime error info");
        autoConfimId += "warning.";
    }
    /* Advance auto-confirm id: */
    autoConfimId += strErrorId.toUtf8();

    /* Format error-details: */
    QString formatted("<!--EOM-->");
    if (!strErrorMsg.isEmpty())
        formatted.prepend(QString("<p>%1.</p>").arg(vboxGlobal().emphasize(strErrorMsg)));
    if (!strErrorId.isEmpty())
        formatted += QString("<table bgcolor=#EEEEEE border=0 cellspacing=5 "
                             "cellpadding=0 width=100%>"
                             "<tr><td>%1</td><td>%2</td></tr>"
                             "<tr><td>%3</td><td>%4</td></tr>"
                             "</table>")
                             .arg(tr("<nobr>Error ID: </nobr>", "runtime error info"), strErrorId)
                             .arg(tr("Severity: ", "runtime error info"), severity);
    if (!formatted.isEmpty())
        formatted = "<qt>" + formatted + "</qt>";

    /* Show the error: */
    if (type == MessageType_Critical)
    {
        error(0, type,
              tr("<p>A fatal error has occurred during virtual machine execution! "
                 "The virtual machine will be powered off. Please copy the following error message "
                 "using the clipboard to help diagnose the problem:</p>"),
              formatted, autoConfimId.data());
    }
    else if (type == MessageType_Error)
    {
        error(0, type,
              tr("<p>An error has occurred during virtual machine execution! "
                 "The error details are shown below. You may try to correct the error "
                 "and resume the virtual machine execution.</p>"),
              formatted, autoConfimId.data());
    }
    else
    {
        error(0, type,
              tr("<p>The virtual machine execution may run into an error condition as described below. "
                 "We suggest that you take an appropriate action to avert the error.</p>"),
              formatted, autoConfimId.data());
    }

    /// @todo Move to Runtime UI!
    /* Postprocessing: */
    if (fFatal)
    {
        /* Power down after a fFatal error: */
        LogRel(("GUI: Powering VM down after a fatal runtime error...\n"));
        console1.PowerDown();
    }
}

bool UIMessageCenter::remindAboutGuruMeditation(const QString &strLogFolder)
{
    return questionBinary(0, MessageType_GuruMeditation,
                          tr("<p>A critical error has occurred while running the virtual "
                             "machine and the machine execution has been stopped.</p>"
                             ""
                             "<p>For help, please see the Community section on "
                             "<a href=https://www.virtualbox.org>https://www.virtualbox.org</a> "
                             "or your support contract. Please provide the contents of the "
                             "log file <tt>VBox.log</tt> and the image file <tt>VBox.png</tt>, "
                             "which you can find in the <nobr><b>%1</b></nobr> directory, "
                             "as well as a description of what you were doing when this error happened. "
                             ""
                             "Note that you can also access the above files by selecting <b>Show Log</b> "
                             "from the <b>Machine</b> menu of the main VirtualBox window.</p>"
                             ""
                             "<p>Press <b>OK</b> if you want to power off the machine "
                             "or press <b>Ignore</b> if you want to leave it as is for debugging. "
                             "Please note that debugging requires special knowledge and tools, "
                             "so it is recommended to press <b>OK</b> now.</p>")
                             .arg(strLogFolder),
                          0 /* auto-confirm id */,
                          QIMessageBox::tr("OK"),
                          tr("Ignore"));
}

void UIMessageCenter::warnAboutVBoxSVCUnavailable() const
{
    alert(0, MessageType_Critical,
          tr("<p>A critical error has occurred while running the virtual "
             "machine and the machine execution should be stopped.</p>"
             ""
             "<p>For help, please see the Community section on "
             "<a href=https://www.virtualbox.org>https://www.virtualbox.org</a> "
             "or your support contract. Please provide the contents of the "
             "log file <tt>VBox.log</tt>, "
             "which you can find in the virtual machine log directory, "
             "as well as a description of what you were doing when this error happened. "
             ""
             "Note that you can also access the above file by selecting <b>Show Log</b> "
             "from the <b>Machine</b> menu of the main VirtualBox window.</p>"
             ""
             "<p>Press <b>OK</b> to power off the machine.</p>"),
          0 /* auto-confirm id */);
}

bool UIMessageCenter::warnAboutVirtExInactiveFor64BitsGuest(bool fHWVirtExSupported) const
{
    if (fHWVirtExSupported)
        return questionBinary(0, MessageType_Error,
                              tr("<p>VT-x/AMD-V hardware acceleration has been enabled, but is not operational. "
                                 "Your 64-bit guest will fail to detect a 64-bit CPU and will not be able to boot.</p>"
                                 "<p>Please ensure that you have enabled VT-x/AMD-V properly in the BIOS of your host computer.</p>"),
                              0 /* auto-confirm id */,
                              tr("Close VM"), tr("Continue"));
    else
        return questionBinary(0, MessageType_Error,
                              tr("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                                 "Your 64-bit guest will fail to detect a 64-bit CPU and will not be able to boot."),
                              0 /* auto-confirm id */,
                              tr("Close VM"), tr("Continue"));
}

bool UIMessageCenter::warnAboutVirtExInactiveForRecommendedGuest(bool fHWVirtExSupported) const
{
    if (fHWVirtExSupported)
        return questionBinary(0, MessageType_Error,
                              tr("<p>VT-x/AMD-V hardware acceleration has been enabled, but is not operational. "
                                 "Certain guests (e.g. OS/2 and QNX) require this feature.</p>"
                                 "<p>Please ensure that you have enabled VT-x/AMD-V properly in the BIOS of your host computer.</p>"),
                              0 /* auto-confirm id */,
                              tr("Close VM"), tr("Continue"));
    else
        return questionBinary(0, MessageType_Error,
                              tr("<p>VT-x/AMD-V hardware acceleration is not available on your system. "
                                 "Certain guests (e.g. OS/2 and QNX) require this feature and will fail to boot without it.</p>"),
                              0 /* auto-confirm id */,
                              tr("Close VM"), tr("Continue"));
}

bool UIMessageCenter::cannotStartWithoutNetworkIf(const QString &strMachineName, const QString &strIfNames) const
{
    return questionBinary(0, MessageType_Error,
                          tr("<p>Could not start the machine <b>%1</b> because the following "
                             "physical network interfaces were not found:</p><p><b>%2</b></p>"
                             "<p>You can either change the machine's network settings or stop the machine.</p>")
                             .arg(strMachineName, strIfNames),
                          0 /* auto-confirm id */,
                          tr("Change Network Settings"), tr("Close VM"));
}

void UIMessageCenter::cannotStartMachine(const CConsole &console, const QString &strName) const
{
    error(0, MessageType_Error,
          tr("Failed to start the virtual machine <b>%1</b>.")
             .arg(strName),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotStartMachine(const CProgress &progress, const QString &strName) const
{
    error(0, MessageType_Error,
          tr("Failed to start the virtual machine <b>%1</b>.")
             .arg(strName),
          formatErrorInfo(progress));
}

bool UIMessageCenter::confirmInputCapture(bool &fAutoConfirmed) const
{
    int rc = question(0, MessageType_Info,
                      tr("<p>You have <b>clicked the mouse</b> inside the Virtual Machine display or pressed the <b>host key</b>. "
                         "This will cause the Virtual Machine to <b>capture</b> the host mouse pointer (only if the mouse pointer "
                         "integration is not currently supported by the guest OS) and the keyboard, which will make them "
                         "unavailable to other applications running on your host machine.</p>"
                         "<p>You can press the <b>host key</b> at any time to <b>uncapture</b> the keyboard and mouse "
                         "(if it is captured) and return them to normal operation. "
                         "The currently assigned host key is shown on the status bar at the bottom of the Virtual Machine window, "
                         "next to the&nbsp;<img src=:/hostkey_16px.png/>&nbsp;icon. "
                         "This icon, together with the mouse icon placed nearby, indicate the current keyboard and mouse capture state.</p>") +
                      tr("<p>The host key is currently defined as <b>%1</b>.</p>", "additional message box paragraph")
                         .arg(UIHostCombo::toReadableString(gEDataManager->hostKeyCombination())),
                      "confirmInputCapture",
                      AlertButton_Ok | AlertButtonOption_Default,
                      AlertButton_Cancel | AlertButtonOption_Escape,
                      0,
                      tr("Capture", "do input capture"));
    /* Was the message auto-confirmed? */
    fAutoConfirmed = (rc & AlertOption_AutoConfirmed);
    /* True if "Ok" was pressed: */
    return (rc & AlertButtonMask) == AlertButton_Ok;
}

bool UIMessageCenter::confirmGoingFullscreen(const QString &strHotKey) const
{
    return questionBinary(0, MessageType_Info,
                          tr("<p>The virtual machine window will be now switched to <b>full-screen</b> mode. "
                             "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
                             "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
                             "<p>Note that the main menu bar is hidden in full-screen mode. "
                             "You can access it by pressing <b>Host+Home</b>.</p>")
                             .arg(strHotKey, UIHostCombo::toReadableString(gEDataManager->hostKeyCombination())),
                          "confirmGoingFullscreen",
                          tr("Switch"));
}

bool UIMessageCenter::confirmGoingSeamless(const QString &strHotKey) const
{
    return questionBinary(0, MessageType_Info,
                          tr("<p>The virtual machine window will be now switched to <b>Seamless</b> mode. "
                             "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
                             "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
                             "<p>Note that the main menu bar is hidden in seamless mode. "
                             "You can access it by pressing <b>Host+Home</b>.</p>")
                             .arg(strHotKey, UIHostCombo::toReadableString(gEDataManager->hostKeyCombination())),
                          "confirmGoingSeamless",
                          tr("Switch"));
}

bool UIMessageCenter::confirmGoingScale(const QString &strHotKey) const
{
    return questionBinary(0, MessageType_Info,
                          tr("<p>The virtual machine window will be now switched to <b>Scale</b> mode. "
                             "You can go back to windowed mode at any time by pressing <b>%1</b>.</p>"
                             "<p>Note that the <i>Host</i> key is currently defined as <b>%2</b>.</p>"
                             "<p>Note that the main menu bar is hidden in scaled mode. "
                             "You can access it by pressing <b>Host+Home</b>.</p>")
                             .arg(strHotKey, UIHostCombo::toReadableString(gEDataManager->hostKeyCombination())),
                          "confirmGoingScale",
                          tr("Switch"));
}

bool UIMessageCenter::cannotEnterFullscreenMode(ULONG /* uWidth */, ULONG /* uHeight */, ULONG /* uBpp */, ULONG64 uMinVRAM) const
{
    return questionBinary(0, MessageType_Warning,
                          tr("<p>Could not switch the guest display to full-screen mode due to insufficient guest video memory.</p>"
                             "<p>You should configure the virtual machine to have at least <b>%1</b> of video memory.</p>"
                             "<p>Press <b>Ignore</b> to switch to full-screen mode anyway or press <b>Cancel</b> to cancel the operation.</p>")
                             .arg(VBoxGlobal::formatSize(uMinVRAM)),
                          0 /* auto-confirm id */,
                          tr("Ignore"));
}

void UIMessageCenter::cannotEnterSeamlessMode(ULONG /* uWidth */, ULONG /* uHeight */, ULONG /* uBpp */, ULONG64 uMinVRAM) const
{
    alert(0, MessageType_Error,
          tr("<p>Could not enter seamless mode due to insufficient guest "
             "video memory.</p>"
             "<p>You should configure the virtual machine to have at "
             "least <b>%1</b> of video memory.</p>")
             .arg(VBoxGlobal::formatSize(uMinVRAM)));
}

bool UIMessageCenter::cannotSwitchScreenInFullscreen(quint64 uMinVRAM) const
{
    return questionBinary(0, MessageType_Warning,
                          tr("<p>Could not change the guest screen to this host screen due to insufficient guest video memory.</p>"
                             "<p>You should configure the virtual machine to have at least <b>%1</b> of video memory.</p>"
                             "<p>Press <b>Ignore</b> to switch the screen anyway or press <b>Cancel</b> to cancel the operation.</p>")
                             .arg(VBoxGlobal::formatSize(uMinVRAM)),
                          0 /* auto-confirm id */,
                          tr("Ignore"));
}

void UIMessageCenter::cannotSwitchScreenInSeamless(quint64 uMinVRAM) const
{
    alert(0, MessageType_Error,
          tr("<p>Could not change the guest screen to this host screen "
             "due to insufficient guest video memory.</p>"
             "<p>You should configure the virtual machine to have at "
             "least <b>%1</b> of video memory.</p>")
             .arg(VBoxGlobal::formatSize(uMinVRAM)));
}

void UIMessageCenter::cannotAttachUSBDevice(const CConsole &console, const QString &strDevice) const
{
    error(0, MessageType_Error,
          tr("Failed to attach the USB device <b>%1</b> to the virtual machine <b>%2</b>.")
             .arg(strDevice, CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotAttachUSBDevice(const CVirtualBoxErrorInfo &errorInfo, const QString &strDevice, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to attach the USB device <b>%1</b> to the virtual machine <b>%2</b>.")
             .arg(strDevice, strMachineName),
          formatErrorInfo(errorInfo));
}

void UIMessageCenter::cannotDetachUSBDevice(const CConsole &console, const QString &strDevice) const
{
    error(0, MessageType_Error,
          tr("Failed to detach the USB device <b>%1</b> from the virtual machine <b>%2</b>.")
             .arg(strDevice, CConsole(console).GetMachine().GetName()),
          formatErrorInfo(console));
}

void UIMessageCenter::cannotDetachUSBDevice(const CVirtualBoxErrorInfo &errorInfo, const QString &strDevice, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to detach the USB device <b>%1</b> from the virtual machine <b>%2</b>.")
             .arg(strDevice, strMachineName),
          formatErrorInfo(errorInfo));
}

void UIMessageCenter::cannotAttachWebCam(const CEmulatedUSB &dispatcher, const QString &strWebCamName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to attach the webcam <b>%1</b> to the virtual machine <b>%2</b>.")
             .arg(strWebCamName, strMachineName),
          formatErrorInfo(dispatcher));
}

void UIMessageCenter::cannotDetachWebCam(const CEmulatedUSB &dispatcher, const QString &strWebCamName, const QString &strMachineName) const
{
    error(0, MessageType_Error,
          tr("Failed to detach the webcam <b>%1</b> from the virtual machine <b>%2</b>.")
             .arg(strWebCamName, strMachineName),
          formatErrorInfo(dispatcher));
}

void UIMessageCenter::cannotToggleVideoCapture(const CMachine &machine, bool fEnable)
{
    /* Get machine-name preserving error-info: */
    QString strMachineName(CMachine(machine).GetName());
    error(0, MessageType_Error,
          fEnable ?
              tr("Failed to enable video capturing for the virtual machine <b>%1</b>.").arg(strMachineName) :
              tr("Failed to disable video capturing for the virtual machine <b>%1</b>.").arg(strMachineName),
          formatErrorInfo(machine));
}

void UIMessageCenter::cannotToggleVRDEServer(const CVRDEServer &server, const QString &strMachineName, bool fEnable)
{
    error(0, MessageType_Error,
          fEnable ?
              tr("Failed to enable the remote desktop server for the virtual machine <b>%1</b>.").arg(strMachineName) :
              tr("Failed to disable the remote desktop server for the virtual machine <b>%1</b>.").arg(strMachineName),
          formatErrorInfo(server));
}

void UIMessageCenter::cannotToggleNetworkAdapterCable(const CNetworkAdapter &adapter, const QString &strMachineName, bool fConnect)
{
    error(0, MessageType_Error,
          fConnect ?
              tr("Failed to connect the network adapter cable of the virtual machine <b>%1</b>.").arg(strMachineName) :
              tr("Failed to disconnect the network adapter cable of the virtual machine <b>%1</b>.").arg(strMachineName),
          formatErrorInfo(adapter));
}

void UIMessageCenter::remindAboutGuestAdditionsAreNotActive() const
{
    alert(0, MessageType_Warning,
          tr("<p>The VirtualBox Guest Additions do not appear to be available on this virtual machine, "
             "and shared folders cannot be used without them. To use shared folders inside the virtual machine, "
             "please install the Guest Additions if they are not installed, or re-install them if they are "
             "not working correctly, by selecting <b>Insert Guest Additions CD image</b> from the <b>Devices</b> menu. "
             "If they are installed but the machine is not yet fully started then shared folders will be available once it is.</p>"),
          "remindAboutGuestAdditionsAreNotActive");
}

void UIMessageCenter::cannotMountGuestAdditions(const QString &strMachineName) const
{
    alert(0, MessageType_Error,
          tr("<p>Could not insert the <b>VirtualBox Guest Additions</b> disk image file into the virtual machine <b>%1</b>, "
             "as the machine has no optical drives. Please add a drive using the storage page of the "
             "virtual machine settings window.</p>")
             .arg(strMachineName));
}

void UIMessageCenter::cannotAddDiskEncryptionPassword(const CConsole &console)
{
    error(0, MessageType_Error,
          tr("Bad password or authentication failure."),
          formatErrorInfo(console));
}

#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
bool UIMessageCenter::confirmCancelingAllNetworkRequests() const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("Do you wish to cancel all current network operations?"));
}

void UIMessageCenter::showUpdateSuccess(const QString &strVersion, const QString &strLink) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Info,
          tr("<p>A new version of VirtualBox has been released! Version <b>%1</b> is available "
             "at <a href=\"https://www.virtualbox.org/\">virtualbox.org</a>.</p>"
             "<p>You can download this version using the link:</p>"
             "<p><a href=%2>%3</a></p>")
             .arg(strVersion, strLink, strLink));
}

void UIMessageCenter::showUpdateNotFound() const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Info,
          tr("You are already running the most recent version of VirtualBox."));
}

void UIMessageCenter::askUserToDownloadExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion, const QString &strVBoxVersion) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Info,
          tr("<p>You have version %1 of the <b><nobr>%2</nobr></b> installed.</p>"
             "<p>You should download and install version %3 of this extension pack from Oracle!</p>")
             .arg(strExtPackVersion).arg(strExtPackName).arg(strVBoxVersion));
}

bool UIMessageCenter::cannotFindGuestAdditions() const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Could not find the <b>VirtualBox Guest Additions</b> disk image file.</p>"
                             "<p>Do you wish to download this disk image file from the Internet?</p>"),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

bool UIMessageCenter::confirmDownloadGuestAdditions(const QString &strUrl, qulonglong uSize) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>Are you sure you want to download the <b>VirtualBox Guest Additions</b> disk image file "
                             "from <nobr><a href=\"%1\">%1</a></nobr> (size %2 bytes)?</p>")
                             .arg(strUrl, QLocale(VBoxGlobal::languageId()).toString(uSize)),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

void UIMessageCenter::cannotSaveGuestAdditions(const QString &strURL, const QString &strTarget) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Error,
          tr("<p>The <b>VirtualBox Guest Additions</b> disk image file has been successfully downloaded "
             "from <nobr><a href=\"%1\">%1</a></nobr> "
             "but can't be saved locally as <nobr><b>%2</b>.</nobr></p>"
             "<p>Please choose another location for that file.</p>")
             .arg(strURL, strTarget));
}

bool UIMessageCenter::proposeMountGuestAdditions(const QString &strUrl, const QString &strSrc) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>The <b>VirtualBox Guest Additions</b> disk image file has been successfully downloaded "
                             "from <nobr><a href=\"%1\">%1</a></nobr> "
                             "and saved locally as <nobr><b>%2</b>.</nobr></p>"
                             "<p>Do you wish to register this disk image file and insert it into the virtual optical drive?</p>")
                             .arg(strUrl, strSrc),
                          0 /* auto-confirm id */,
                          tr("Insert", "additions"));
}

void UIMessageCenter::cannotValidateGuestAdditionsSHA256Sum(const QString &strUrl, const QString &strSrc) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Error,
          tr("<p>The <b>VirtualBox Guest Additions</b> disk image file has been successfully downloaded "
             "from <nobr><a href=\"%1\">%1</a></nobr> "
             "and saved locally as <nobr><b>%2</b>, </nobr>"
             "but the SHA-256 checksum verification failed.</p>"
             "<p>Please do the download, installation and verification manually.</p>")
             .arg(strUrl, strSrc));
}

void UIMessageCenter::cannotUpdateGuestAdditions(const CProgress &progress) const
{
    error(0, MessageType_Error,
          tr("Failed to update Guest Additions. "
             "The Guest Additions disk image file will be inserted for user installation."),
          formatErrorInfo(progress));
}

bool UIMessageCenter::cannotFindUserManual(const QString &strMissedLocation) const
{
    return questionBinary(0, MessageType_Question,
                          tr("<p>Could not find the <b>VirtualBox User Manual</b> <nobr><b>%1</b>.</nobr></p>"
                             "<p>Do you wish to download this file from the Internet?</p>")
                             .arg(strMissedLocation),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

bool UIMessageCenter::confirmDownloadUserManual(const QString &strURL, qulonglong uSize) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>Are you sure you want to download the <b>VirtualBox User Manual</b> "
                             "from <nobr><a href=\"%1\">%1</a></nobr> (size %2 bytes)?</p>")
                             .arg(strURL, QLocale(VBoxGlobal::languageId()).toString(uSize)),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

void UIMessageCenter::cannotSaveUserManual(const QString &strURL, const QString &strTarget) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Error,
          tr("<p>The VirtualBox User Manual has been successfully downloaded "
             "from <nobr><a href=\"%1\">%1</a></nobr> "
             "but can't be saved locally as <nobr><b>%2</b>.</nobr></p>"
             "<p>Please choose another location for that file.</p>")
             .arg(strURL, strTarget));
}

void UIMessageCenter::warnAboutUserManualDownloaded(const QString &strURL, const QString &strTarget) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Warning,
          tr("<p>The VirtualBox User Manual has been successfully downloaded "
             "from <nobr><a href=\"%1\">%1</a></nobr> "
             "and saved locally as <nobr><b>%2</b>.</nobr></p>")
             .arg(strURL, strTarget));
}

bool UIMessageCenter::warAboutOutdatedExtensionPack(const QString &strExtPackName, const QString &strExtPackVersion) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>You have an old version (%1) of the <b><nobr>%2</nobr></b> installed.</p>"
                             "<p>Do you wish to download latest one from the Internet?</p>")
                             .arg(strExtPackVersion).arg(strExtPackName),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

bool UIMessageCenter::confirmDownloadExtensionPack(const QString &strExtPackName, const QString &strURL, qulonglong uSize) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>Are you sure you want to download the <b><nobr>%1</nobr></b> "
                             "from <nobr><a href=\"%2\">%2</a></nobr> (size %3 bytes)?</p>")
                             .arg(strExtPackName, strURL, QLocale(VBoxGlobal::languageId()).toString(uSize)),
                          0 /* auto-confirm id */,
                          tr("Download"));
}

void UIMessageCenter::cannotSaveExtensionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Error,
          tr("<p>The <b><nobr>%1</nobr></b> has been successfully downloaded "
             "from <nobr><a href=\"%2\">%2</a></nobr> "
             "but can't be saved locally as <nobr><b>%3</b>.</nobr></p>"
             "<p>Please choose another location for that file.</p>")
             .arg(strExtPackName, strFrom, strTo));
}

bool UIMessageCenter::proposeInstallExtentionPack(const QString &strExtPackName, const QString &strFrom, const QString &strTo) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("<p>The <b><nobr>%1</nobr></b> has been successfully downloaded "
                             "from <nobr><a href=\"%2\">%2</a></nobr> "
                             "and saved locally as <nobr><b>%3</b>.</nobr></p>"
                             "<p>Do you wish to install this extension pack?</p>")
                             .arg(strExtPackName, strFrom, strTo),
                          0 /* auto-confirm id */,
                          tr("Install", "extension pack"));
}

void UIMessageCenter::cannotValidateExtentionPackSHA256Sum(const QString &strExtPackName, const QString &strFrom, const QString &strTo) const
{
    alert(windowManager().networkManagerOrMainWindowShown(), MessageType_Error,
          tr("<p>The <b><nobr>%1</nobr></b> has been successfully downloaded "
             "from <nobr><a href=\"%2\">%2</a></nobr> "
             "and saved locally as <nobr><b>%3</b>, </nobr>"
             "but the SHA-256 checksum verification failed.</p>"
             "<p>Please do the download, installation and verification manually.</p>")
             .arg(strExtPackName, strFrom, strTo));
}

bool UIMessageCenter::proposeDeleteExtentionPack(const QString &strTo) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("Do you want to delete the downloaded file <nobr><b>%1</b></nobr>?")
                             .arg(strTo),
                          0 /* auto-confirm id */,
                          tr("Delete", "extension pack"));
}

bool UIMessageCenter::proposeDeleteOldExtentionPacks(const QStringList &strFiles) const
{
    return questionBinary(windowManager().networkManagerOrMainWindowShown(), MessageType_Question,
                          tr("Do you want to delete following list of files <nobr><b>%1</b></nobr>?")
                             .arg(strFiles.join(",")),
                          0 /* auto-confirm id */,
                          tr("Delete", "extension pack"));
}
#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */

bool UIMessageCenter::confirmInstallExtensionPack(const QString &strPackName, const QString &strPackVersion,
                                                  const QString &strPackDescription, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>You are about to install a VirtualBox extension pack. "
                             "Extension packs complement the functionality of VirtualBox and can contain system level software "
                             "that could be potentially harmful to your system. Please review the description below and only proceed "
                             "if you have obtained the extension pack from a trusted source.</p>"
                             "<p><table cellpadding=0 cellspacing=5>"
                             "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%1</td></tr>"
                             "<tr><td><b>Version:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                             "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                             "</table></p>")
                             .arg(strPackName).arg(strPackVersion).arg(strPackDescription),
                          0 /* auto-confirm id */,
                          tr("Install", "extension pack"));
}

bool UIMessageCenter::confirmReplaceExtensionPack(const QString &strPackName, const QString &strPackVersionNew,
                                                  const QString &strPackVersionOld, const QString &strPackDescription,
                                                  QWidget *pParent /* = 0*/) const
{
    /* Prepare initial message: */
    QString strBelehrung = tr("Extension packs complement the functionality of VirtualBox and can contain "
                              "system level software that could be potentially harmful to your system. "
                              "Please review the description below and only proceed if you have obtained "
                              "the extension pack from a trusted source.");

    /* Compare versions: */
    QByteArray  ba1     = strPackVersionNew.toUtf8();
    QByteArray  ba2     = strPackVersionOld.toUtf8();
    int         iVerCmp = RTStrVersionCompare(ba1.constData(), ba2.constData());

    /* Show the question: */
    bool fRc;
    if (iVerCmp > 0)
        fRc = questionBinary(pParent, MessageType_Question,
                             tr("<p>An older version of the extension pack is already installed, would you like to upgrade? "
                                "<p>%1</p>"
                                "<p><table cellpadding=0 cellspacing=5>"
                                "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                "<tr><td><b>New Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                "<tr><td><b>Current Version:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%5</td></tr>"
                                "</table></p>")
                                .arg(strBelehrung).arg(strPackName).arg(strPackVersionNew).arg(strPackVersionOld).arg(strPackDescription),
                             0 /* auto-confirm id */,
                             tr("&Upgrade"));
    else if (iVerCmp < 0)
        fRc = questionBinary(pParent, MessageType_Question,
                             tr("<p>An newer version of the extension pack is already installed, would you like to downgrade? "
                                "<p>%1</p>"
                                "<p><table cellpadding=0 cellspacing=5>"
                                "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                "<tr><td><b>New Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                "<tr><td><b>Current Version:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%5</td></tr>"
                                "</table></p>")
                                .arg(strBelehrung).arg(strPackName).arg(strPackVersionNew).arg(strPackVersionOld).arg(strPackDescription),
                             0 /* auto-confirm id */,
                             tr("&Downgrade"));
    else
        fRc = questionBinary(pParent, MessageType_Question,
                             tr("<p>The extension pack is already installed with the same version, would you like reinstall it? "
                                "<p>%1</p>"
                                "<p><table cellpadding=0 cellspacing=5>"
                                "<tr><td><b>Name:&nbsp;&nbsp;</b></td><td>%2</td></tr>"
                                "<tr><td><b>Version:&nbsp;&nbsp;</b></td><td>%3</td></tr>"
                                "<tr><td><b>Description:&nbsp;&nbsp;</b></td><td>%4</td></tr>"
                                "</table></p>")
                                .arg(strBelehrung).arg(strPackName).arg(strPackVersionOld).arg(strPackDescription),
                             0 /* auto-confirm id */,
                             tr("&Reinstall"));
    return fRc;
}

bool UIMessageCenter::confirmRemoveExtensionPack(const QString &strPackName, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("<p>You are about to remove the VirtualBox extension pack <b>%1</b>.</p>"
                             "<p>Are you sure you want to proceed?</p>")
                             .arg(strPackName),
                          0 /* auto-confirm id */,
                          tr("&Remove") /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

void UIMessageCenter::cannotOpenExtPack(const QString &strFilename, const CExtPackManager &extPackManager, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
          formatErrorInfo(extPackManager));
}

void UIMessageCenter::warnAboutBadExtPackFile(const QString &strFilename, const CExtPackFile &extPackFile, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to open the Extension Pack <b>%1</b>.").arg(strFilename),
          "<!--EOM-->" + extPackFile.GetWhyUnusable());
}

void UIMessageCenter::cannotInstallExtPack(const CExtPackFile &extPackFile, const QString &strFilename, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to install the Extension Pack <b>%1</b>.")
             .arg(strFilename),
          formatErrorInfo(extPackFile));
}

void UIMessageCenter::cannotInstallExtPack(const CProgress &progress, const QString &strFilename, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to install the Extension Pack <b>%1</b>.")
             .arg(strFilename),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotUninstallExtPack(const CExtPackManager &extPackManager, const QString &strPackName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to uninstall the Extension Pack <b>%1</b>.")
             .arg(strPackName),
          formatErrorInfo(extPackManager));
}

void UIMessageCenter::cannotUninstallExtPack(const CProgress &progress, const QString &strPackName, QWidget *pParent /* = 0*/) const
{
    error(pParent, MessageType_Error,
          tr("Failed to uninstall the Extension Pack <b>%1</b>.")
             .arg(strPackName),
          formatErrorInfo(progress));
}

void UIMessageCenter::warnAboutExtPackInstalled(const QString &strPackName, QWidget *pParent /* = 0*/) const
{
    alert(pParent, MessageType_Info,
          tr("The extension pack <br><nobr><b>%1</b><nobr><br> was installed successfully.")
             .arg(strPackName));
}

#ifdef VBOX_WITH_DRAG_AND_DROP
void UIMessageCenter::cannotDropDataToGuest(const CDnDTarget &dndTarget, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Drag and drop operation from host to guest failed."),
          formatErrorInfo(dndTarget));
}

void UIMessageCenter::cannotDropDataToGuest(const CProgress &progress, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Drag and drop operation from host to guest failed."),
          formatErrorInfo(progress));
}

void UIMessageCenter::cannotCancelDropToGuest(const CDnDTarget &dndTarget, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Unable to cancel host to guest drag and drop operation."),
          formatErrorInfo(dndTarget));
}

void UIMessageCenter::cannotDropDataToHost(const CDnDSource &dndSource, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Drag and drop operation from guest to host failed."),
          formatErrorInfo(dndSource));
}

void UIMessageCenter::cannotDropDataToHost(const CProgress &progress, QWidget *pParent /* = 0 */) const
{
    error(pParent, MessageType_Error,
          tr("Drag and drop operation from guest to host failed."),
          formatErrorInfo(progress));
}
#endif /* VBOX_WITH_DRAG_AND_DROP */

void UIMessageCenter::cannotOpenLicenseFile(const QString &strPath, QWidget *pParent /* = 0*/) const
{
    alert(pParent, MessageType_Error,
          tr("Failed to open the license file <nobr><b>%1</b></nobr>. Check file permissions.")
             .arg(strPath));
}

bool UIMessageCenter::confirmOverridingFile(const QString &strPath, QWidget *pParent /* = 0*/) const
{
    return questionBinary(pParent, MessageType_Question,
                          tr("A file named <b>%1</b> already exists. "
                             "Are you sure you want to replace it?<br /><br />"
                             "Replacing it will overwrite its contents.")
                             .arg(strPath),
                          0 /* auto-confirm id */,
                          QString() /* ok button text */,
                          QString() /* cancel button text */,
                          false /* ok button by default? */);
}

bool UIMessageCenter::confirmOverridingFiles(const QVector<QString> &strPaths, QWidget *pParent /* = 0*/) const
{
    /* If it is only one file use the single question versions above: */
    if (strPaths.size() == 1)
        return confirmOverridingFile(strPaths.at(0), pParent);
    else if (strPaths.size() > 1)
        return questionBinary(pParent, MessageType_Question,
                              tr("The following files already exist:<br /><br />%1<br /><br />"
                                 "Are you sure you want to replace them? "
                                 "Replacing them will overwrite their contents.")
                                 .arg(QStringList(strPaths.toList()).join("<br />")),
                              0 /* auto-confirm id */,
                              QString() /* ok button text */,
                              QString() /* cancel button text */,
                              false /* ok button by default? */);
    else
        return true;
}

bool UIMessageCenter::confirmOverridingFileIfExists(const QString &strPath, QWidget *pParent /* = 0*/) const
{
    QFileInfo fi(strPath);
    if (fi.exists())
        return confirmOverridingFile(strPath, pParent);
    else
        return true;
}

bool UIMessageCenter::confirmOverridingFilesIfExists(const QVector<QString> &strPaths, QWidget *pParent /* = 0*/) const
{
    QVector<QString> existingFiles;
    foreach(const QString &file, strPaths)
    {
        QFileInfo fi(file);
        if (fi.exists())
            existingFiles << fi.absoluteFilePath();
    }
    /* If it is only one file use the single question versions above: */
    if (existingFiles.size() == 1)
        return confirmOverridingFileIfExists(existingFiles.at(0), pParent);
    else if (existingFiles.size() > 1)
        return confirmOverridingFiles(existingFiles, pParent);
    else
        return true;
}

/* static */
QString UIMessageCenter::formatRC(HRESULT rc)
{
    QString str;

    PCRTCOMERRMSG msg = NULL;
    const char *errMsg = NULL;

    /* First, try as is (only set bit 31 bit for warnings): */
    if (SUCCEEDED_WARNING(rc))
        msg = RTErrCOMGet(rc | 0x80000000);
    else
        msg = RTErrCOMGet(rc);

    if (msg != NULL)
        errMsg = msg->pszDefine;

#ifdef VBOX_WS_WIN
    PCRTWINERRMSG winMsg = NULL;

    /* If not found, try again using RTErrWinGet with masked off top 16bit: */
    if (msg == NULL)
    {
        winMsg = RTErrWinGet(rc & 0xFFFF);

        if (winMsg != NULL)
            errMsg = winMsg->pszDefine;
    }
#endif /* VBOX_WS_WIN */

    if (errMsg != NULL && *errMsg != '\0')
        str.sprintf("%s", errMsg);

    return str;
}

/* static */
QString UIMessageCenter::formatRCFull(HRESULT rc)
{
    QString str;

    PCRTCOMERRMSG msg = NULL;
    const char *errMsg = NULL;

    /* First, try as is (only set bit 31 bit for warnings): */
    if (SUCCEEDED_WARNING(rc))
        msg = RTErrCOMGet(rc | 0x80000000);
    else
        msg = RTErrCOMGet(rc);

    if (msg != NULL)
        errMsg = msg->pszDefine;

#ifdef VBOX_WS_WIN
    PCRTWINERRMSG winMsg = NULL;

    /* If not found, try again using RTErrWinGet with masked off top 16bit: */
    if (msg == NULL)
    {
        winMsg = RTErrWinGet(rc & 0xFFFF);

        if (winMsg != NULL)
            errMsg = winMsg->pszDefine;
    }
#endif /* VBOX_WS_WIN */

    if (errMsg != NULL && *errMsg != '\0')
        str.sprintf("%s (0x%08X)", errMsg, rc);
    else
        str.sprintf("0x%08X", rc);

    return str;
}

/* static */
QString UIMessageCenter::formatErrorInfo(const CProgress &progress)
{
    /* Check for API errors first: */
    if (!progress.isOk())
        return formatErrorInfo(static_cast<COMBaseWithEI>(progress));

    /* For progress errors otherwise: */
    CVirtualBoxErrorInfo errorInfo = progress.GetErrorInfo();
    /* Handle valid error-info first: */
    if (!errorInfo.isNull())
        return formatErrorInfo(errorInfo);
    /* Handle NULL error-info otherwise: */
    return QString("<table bgcolor=#EEEEEE border=0 cellspacing=5 cellpadding=0 width=100%>"
                   "<tr><td>%1</td><td><tt>%2</tt></td></tr></table>")
                   .arg(tr("Result&nbsp;Code: ", "error info"))
                   .arg(formatRCFull(progress.GetResultCode()))
                   .prepend("<!--EOM-->") /* move to details */;
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMErrorInfo &info, HRESULT wrapperRC /* = S_OK */)
{
    QString formatted = errorInfoToString(info, wrapperRC);
    return QString("<qt>%1</qt>").arg(formatted);
}

/* static */
QString UIMessageCenter::formatErrorInfo(const CVirtualBoxErrorInfo &info)
{
    return formatErrorInfo(COMErrorInfo(info));
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMBaseWithEI &wrapper)
{
    Assert(wrapper.lastRC() != S_OK);
    return formatErrorInfo(wrapper.errorInfo(), wrapper.lastRC());
}

/* static */
QString UIMessageCenter::formatErrorInfo(const COMResult &rc)
{
    Assert(rc.rc() != S_OK);
    return formatErrorInfo(rc.errorInfo(), rc.rc());
}

void UIMessageCenter::sltShowHelpWebDialog()
{
    vboxGlobal().openURL("https://www.virtualbox.org");
}

void UIMessageCenter::sltShowBugTracker()
{
    vboxGlobal().openURL("https://www.virtualbox.org/wiki/Bugtracker");
}

void UIMessageCenter::sltShowForums()
{
    vboxGlobal().openURL("https://forums.virtualbox.org/");
}

void UIMessageCenter::sltShowOracle()
{
    vboxGlobal().openURL("http://www.oracle.com/us/technologies/virtualization/virtualbox/overview/index.html");
}

void UIMessageCenter::sltShowHelpAboutDialog()
{
    CVirtualBox vbox = vboxGlobal().virtualBox();
    QString strFullVersion;
    if (vboxGlobal().brandingIsActive())
    {
        strFullVersion = QString("%1 r%2 - %3").arg(vbox.GetVersion())
                                               .arg(vbox.GetRevision())
                                               .arg(vboxGlobal().brandingGetKey("Name"));
    }
    else
    {
        strFullVersion = QString("%1 r%2").arg(vbox.GetVersion())
                                          .arg(vbox.GetRevision());
    }
    AssertWrapperOk(vbox);

    (new VBoxAboutDlg(windowManager().mainWindowShown(), strFullVersion))->show();
}

void UIMessageCenter::sltShowHelpHelpDialog()
{
#ifndef VBOX_OSE
    /* For non-OSE version we just open it: */
    sltShowUserManual(vboxGlobal().helpFile());
#else /* #ifndef VBOX_OSE */
    /* For OSE version we have to check if it present first: */
    QString strUserManualFileName1 = vboxGlobal().helpFile();
    QString strShortFileName = QFileInfo(strUserManualFileName1).fileName();
    QString strUserManualFileName2 = QDir(vboxGlobal().homeFolder()).absoluteFilePath(strShortFileName);
    /* Show if user manual already present: */
    if (QFile::exists(strUserManualFileName1))
        sltShowUserManual(strUserManualFileName1);
    else if (QFile::exists(strUserManualFileName2))
        sltShowUserManual(strUserManualFileName2);
    /* If downloader is running already: */
    else if (UIDownloaderUserManual::current())
    {
        /* Just show network access manager: */
        gNetworkManager->show();
    }
    /* Else propose to download user manual: */
    else if (cannotFindUserManual(strUserManualFileName1))
    {
        /* Create User Manual downloader: */
        UIDownloaderUserManual *pDl = UIDownloaderUserManual::create();
        /* After downloading finished => show User Manual: */
        connect(pDl, SIGNAL(sigDownloadFinished(const QString&)), this, SLOT(sltShowUserManual(const QString&)));
        /* Start downloading: */
        pDl->start();
    }
#endif /* #ifdef VBOX_OSE */
}

void UIMessageCenter::sltResetSuppressedMessages()
{
    /* Nullify suppressed message list: */
    gEDataManager->setSuppressedMessages(QStringList());
}

void UIMessageCenter::sltShowUserManual(const QString &strLocation)
{
#if defined (VBOX_WS_WIN)
    HtmlHelp(GetDesktopWindow(), strLocation.utf16(), HH_DISPLAY_TOPIC, NULL);
#elif defined (VBOX_WS_X11)
# ifndef VBOX_OSE
    char szViewerPath[RTPATH_MAX];
    int rc;
    rc = RTPathAppPrivateArch(szViewerPath, sizeof(szViewerPath));
    AssertRC(rc);
    QProcess::startDetached(QString(szViewerPath) + "/kchmviewer", QStringList(strLocation));
# else /* #ifndef VBOX_OSE */
    vboxGlobal().openURL("file://" + strLocation);
# endif /* #ifdef VBOX_OSE */
#elif defined (VBOX_WS_MAC)
    vboxGlobal().openURL("file://" + strLocation);
#endif
}

void UIMessageCenter::sltShowMessageBox(QWidget *pParent, MessageType type,
                                        const QString &strMessage, const QString &strDetails,
                                        int iButton1, int iButton2, int iButton3,
                                        const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                                        const QString &strAutoConfirmId) const
{
    /* Now we can show a message-box directly: */
    showMessageBox(pParent, type,
                   strMessage, strDetails,
                   iButton1, iButton2, iButton3,
                   strButtonText1, strButtonText2, strButtonText3,
                   strAutoConfirmId);
}

UIMessageCenter::UIMessageCenter()
{
    /* Assign instance: */
    m_spInstance = this;
}

UIMessageCenter::~UIMessageCenter()
{
    /* Unassign instance: */
    m_spInstance = 0;
}

void UIMessageCenter::prepare()
{
    /* Register required objects as meta-types: */
    qRegisterMetaType<CProgress>();
    qRegisterMetaType<CHost>();
    qRegisterMetaType<CMachine>();
    qRegisterMetaType<CConsole>();
    qRegisterMetaType<CHostNetworkInterface>();
    qRegisterMetaType<UIMediumType>();
    qRegisterMetaType<StorageSlot>();

    /* Prepare interthread connection: */
    qRegisterMetaType<MessageType>();
    connect(this, SIGNAL(sigToShowMessageBox(QWidget*, MessageType,
                                             const QString&, const QString&,
                                             int, int, int,
                                             const QString&, const QString&, const QString&,
                                             const QString&)),
            this, SLOT(sltShowMessageBox(QWidget*, MessageType,
                                         const QString&, const QString&,
                                         int, int, int,
                                         const QString&, const QString&, const QString&,
                                         const QString&)),
            Qt::BlockingQueuedConnection);

    /* Translations for Main.
     * Please make sure they corresponds to the strings coming from Main one-by-one symbol! */
    tr("Could not load the Host USB Proxy Service (VERR_FILE_NOT_FOUND). The service might not be installed on the host computer");
    tr("VirtualBox is not currently allowed to access USB devices.  You can change this by adding your user to the 'vboxusers' group.  Please see the user manual for a more detailed explanation");
    tr("VirtualBox is not currently allowed to access USB devices.  You can change this by allowing your user to access the 'usbfs' folder and files.  Please see the user manual for a more detailed explanation");
    tr("The USB Proxy Service has not yet been ported to this host");
    tr("Could not load the Host USB Proxy service");
}

void UIMessageCenter::cleanup()
{
     /* Nothing for now... */
}

QString UIMessageCenter::errorInfoToString(const COMErrorInfo &info,
                                           HRESULT wrapperRC /* = S_OK */)
{
    /* Compose complex details string with internal <!--EOM--> delimiter to
     * make it possible to split string into info & details parts which will
     * be used separately in QIMessageBox */
    QString formatted;

    /* Check if details text is NOT empty: */
    QString strDetailsInfo = info.text();
    if (!strDetailsInfo.isEmpty())
    {
        /* Check if details text written in English (latin1) and translated: */
        if (strDetailsInfo == QString::fromLatin1(strDetailsInfo.toLatin1()) &&
            strDetailsInfo != tr(strDetailsInfo.toLatin1().constData()))
            formatted += QString("<p>%1.</p>").arg(vboxGlobal().emphasize(tr(strDetailsInfo.toLatin1().constData())));
        else
            formatted += QString("<p>%1.</p>").arg(vboxGlobal().emphasize(strDetailsInfo));
    }

    formatted += "<!--EOM--><table bgcolor=#EEEEEE border=0 cellspacing=5 "
                 "cellpadding=0 width=100%>";

    bool haveResultCode = false;

    if (info.isBasicAvailable())
    {
#if defined (VBOX_WS_WIN)
        haveResultCode = info.isFullAvailable();
        bool haveComponent = true;
        bool haveInterfaceID = true;
#else /* defined (VBOX_WS_WIN) */
        haveResultCode = true;
        bool haveComponent = info.isFullAvailable();
        bool haveInterfaceID = info.isFullAvailable();
#endif

        if (haveResultCode)
        {
            formatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
                .arg(tr("Result&nbsp;Code: ", "error info"))
                .arg(formatRCFull(info.resultCode()));
        }

        if (haveComponent)
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Component: ", "error info"), info.component());

        if (haveInterfaceID)
        {
            QString s = info.interfaceID().toString();
            if (!info.interfaceName().isEmpty())
                s = info.interfaceName() + ' ' + s;
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Interface: ", "error info"), s);
        }

        if (!info.calleeIID().isNull() && info.calleeIID() != info.interfaceID())
        {
            QString s = info.calleeIID().toString();
            if (!info.calleeName().isEmpty())
                s = info.calleeName() + ' ' + s;
            formatted += QString("<tr><td>%1</td><td>%2</td></tr>")
                .arg(tr("Callee: ", "error info"), s);
        }
    }

    if (FAILED (wrapperRC) &&
        (!haveResultCode || wrapperRC != info.resultCode()))
    {
        formatted += QString("<tr><td>%1</td><td><tt>%2</tt></td></tr>")
            .arg(tr("Callee&nbsp;RC: ", "error info"))
            .arg(formatRCFull(wrapperRC));
    }

    formatted += "</table>";

    if (info.next())
        formatted = formatted + "<!--EOP-->" + errorInfoToString(*info.next());

    return formatted;
}

int UIMessageCenter::showMessageBox(QWidget *pParent, MessageType type,
                                    const QString &strMessage, const QString &strDetails,
                                    int iButton1, int iButton2, int iButton3,
                                    const QString &strButtonText1, const QString &strButtonText2, const QString &strButtonText3,
                                    const QString &strAutoConfirmId) const
{
    /* Choose the 'default' button: */
    if (iButton1 == 0 && iButton2 == 0 && iButton3 == 0)
        iButton1 = AlertButton_Ok | AlertButtonOption_Default;

    /* Check if message-box was auto-confirmed before: */
    QStringList confirmedMessageList;
    if (!strAutoConfirmId.isEmpty())
    {
        const QString strID = vboxGlobal().isVMConsoleProcess() ? vboxGlobal().managedVMUuid() : UIExtraDataManager::GlobalID;
        confirmedMessageList = gEDataManager->suppressedMessages(strID);
        if (   confirmedMessageList.contains(strAutoConfirmId)
            || confirmedMessageList.contains("allMessageBoxes")
            || confirmedMessageList.contains("all") )
        {
            int iResultCode = AlertOption_AutoConfirmed;
            if (iButton1 & AlertButtonOption_Default)
                iResultCode |= (iButton1 & AlertButtonMask);
            if (iButton2 & AlertButtonOption_Default)
                iResultCode |= (iButton2 & AlertButtonMask);
            if (iButton3 & AlertButtonOption_Default)
                iResultCode |= (iButton3 & AlertButtonMask);
            return iResultCode;
        }
    }

    /* Choose title and icon: */
    QString title;
    AlertIconType icon;
    switch (type)
    {
        default:
        case MessageType_Info:
            title = tr("VirtualBox - Information", "msg box title");
            icon = AlertIconType_Information;
            break;
        case MessageType_Question:
            title = tr("VirtualBox - Question", "msg box title");
            icon = AlertIconType_Question;
            break;
        case MessageType_Warning:
            title = tr("VirtualBox - Warning", "msg box title");
            icon = AlertIconType_Warning;
            break;
        case MessageType_Error:
            title = tr("VirtualBox - Error", "msg box title");
            icon = AlertIconType_Critical;
            break;
        case MessageType_Critical:
            title = tr("VirtualBox - Critical Error", "msg box title");
            icon = AlertIconType_Critical;
            break;
        case MessageType_GuruMeditation:
            title = "VirtualBox - Guru Meditation"; /* don't translate this */
            icon = AlertIconType_GuruMeditation;
            break;
    }

    /* Create message-box: */
    QWidget *pMessageBoxParent = windowManager().realParentWindow(pParent ? pParent : windowManager().mainWindowShown());
    QPointer<QIMessageBox> pMessageBox = new QIMessageBox(title, strMessage, icon,
                                                          iButton1, iButton2, iButton3,
                                                          pMessageBoxParent);
    windowManager().registerNewParent(pMessageBox, pMessageBoxParent);

    /* Prepare auto-confirmation check-box: */
    if (!strAutoConfirmId.isEmpty())
    {
        pMessageBox->setFlagText(tr("Do not show this message again", "msg box flag"));
        pMessageBox->setFlagChecked(false);
    }

    /* Configure details: */
    if (!strDetails.isEmpty())
        pMessageBox->setDetailsText(strDetails);

    /* Configure button-text: */
    if (!strButtonText1.isNull())
        pMessageBox->setButtonText(0, strButtonText1);
    if (!strButtonText2.isNull())
        pMessageBox->setButtonText(1, strButtonText2);
    if (!strButtonText3.isNull())
        pMessageBox->setButtonText(2, strButtonText3);

    /* Show message-box: */
    int iResultCode = pMessageBox->exec();

    /* Make sure message-box still valid: */
    if (!pMessageBox)
        return iResultCode;

    /* Remember auto-confirmation check-box value: */
    if (!strAutoConfirmId.isEmpty())
    {
        if (pMessageBox->flagChecked())
        {
            confirmedMessageList << strAutoConfirmId;
            gEDataManager->setSuppressedMessages(confirmedMessageList);
        }
    }

    /* Delete message-box: */
    delete pMessageBox;

    /* Return result-code: */
    return iResultCode;
}

