/* $Id$ */
/** @file
 * VBox Qt GUI - UIGuestControlInterface class declaration.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___UIGuestControlInterface_h___
#define ___UIGuestControlInterface_h___


/* Qt includes: */
#include <QObject>
#include <QMap>

/* COM includes: */
#include "COMEnums.h"
#include "CGuest.h"

class UIGuestControlSubCommandBase;
class CommandData;

/** UIGuestControlInterface parses a command string and  issues API calls
    accordingly to achive guest control related operations */
class UIGuestControlInterface : public QObject
{

    Q_OBJECT;

signals:

    void sigOutputString(const QString &strOutput);

public:

    UIGuestControlInterface(QObject *parent, const CGuest &comGeust);
    ~UIGuestControlInterface();

    /** Receives a command string */
    void putCommand(const QString &strCommand);

    /** @name Some utility functions
     * @{ */
        /** Pass a non-const ref since for some reason CGuest::GetAdditionsStatus
            is non-const?! */
       static bool    isGuestAdditionsAvailable(const CGuest &guest);
       static QString getFsObjTypeString(KFsObjType type);
    /** @} */


private slots:

private:

    typedef bool (UIGuestControlInterface::*HandleFuncPtr)(int, char**);

    /** findOrCreateSession parses command options and determines if an existing session
        to be returned or a new one to be created */
    bool findOrCreateSession(const CommandData &commandData, CGuestSession &outGuestSession);
    /** Search a valid gurst session among existing ones, assign @p outGuestSession if found and return true */
    bool findAValidGuestSession(CGuestSession &outGuestSession);
    bool findSession(const QString& strSessionName, CGuestSession& outSession);
    bool findSession(ULONG strSessionId, CGuestSession& outSession);
    bool createSession(const CommandData &commandData, CGuestSession &outSession);

    void prepareSubCommandHandlers();
    bool startProcess(const CommandData &commandData, CGuestSession &guestSession);
    bool createDirectory(const CommandData &commandData, CGuestSession &guestSession);

    /** Handles the 'start' process command */
    bool handleStart(int, char**);
        /* Handles the 'help' process command */
    bool handleHelp(int, char**);
    /** Handles the 'create' session command */
    bool handleCreateSession(int, char**);
    /** Handles the 'mkdir' session command to create guest directories */
    bool handleMkdir(int, char**);
    bool handleStat(int, char**);
    template<typename T>
    QString getFsObjInfoString(const T &fsObjectInfo) const;


    CGuest        m_comGuest;
    const QString m_strHelp;
    QString       m_strStatus;
    /** A map of function pointers to handleXXXX functions */
    QMap<QString, HandleFuncPtr> m_subCommandHandlers;
};

#endif /* !___UIGuestControlInterface_h___ */
