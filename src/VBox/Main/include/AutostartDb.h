/* $Id$ */
/** @file
 * Main - Autostart database Interfaces.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___autostart_h
#define ___autostart_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>

class AutostartDb
{
    public:

        AutostartDb();
        ~AutostartDb();

        /**
         * Sets the path to the autostart database.
         *
         * @returns VBox status code.
         * @param   pszAutostartDbPathNew    Path to the autostart database.
         */
        int setAutostartDbPath(const char *pszAutostartDbPathNew);

        /**
         * Add a autostart VM to the global database.
         *
         * @returns VBox status code.
         * @retval VERR_PATH_NOT_FOUND if the autostart database directory is not set.
         * @param   pszVMId    ID of the VM to add.
         */
        int addAutostartVM(const char *pszVMId);

        /**
         * Remove a autostart VM from the global database.
         *
         * @returns VBox status code.
         * @retval VERR_PATH_NOT_FOUND if the autostart database directory is not set.
         * @param   pszVMId    ID of the VM to remove.
         */
        int removeAutostartVM(const char *pszVMId);

        /**
         * Add a autostop VM to the global database.
         *
         * @returns VBox status code.
         * @retval VERR_PATH_NOT_FOUND if the autostart database directory is not set.
         * @param   pszVMId    ID of the VM to add.
         */
        int addAutostopVM(const char *pszVMId);

        /**
         * Remove a autostop VM from the global database.
         *
         * @returns VBox status code.
         * @retval VERR_PATH_NOT_FOUND if the autostart database directory is not set.
         * @param   pszVMId    ID of the VM to remove.
         */
        int removeAutostopVM(const char *pszVMId);

    private:

#ifdef RT_OS_LINUX
        /** Critical section protecting the database against concurrent access. */
        RTCRITSECT      CritSect;
        /** Path to the autostart database. */
        char           *m_pszAutostartDbPath;

        /**
         * Autostart database modification worker.
         *
         * @returns VBox status code.
         * @param   fAutostart    Flag whether the autostart or autostop database is modified.
         * @param   fAddVM        Flag whether a VM is added or removed from the database.
         */
        int autostartModifyDb(bool fAutostart, bool fAddVM);
#endif
};

#endif  /* !___autostart_h */

