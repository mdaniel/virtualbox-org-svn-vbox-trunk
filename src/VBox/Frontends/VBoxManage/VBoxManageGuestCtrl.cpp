/* $Id$ */
/** @file
 * VBoxManage - Implementation of guestcontrol command.
 */

/*
 * Copyright (C) 2010-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxManage.h"
#include "VBoxManageGuestCtrl.h"

#ifndef VBOX_ONLY_DOCS

#include <VBox/com/array.h>
#include <VBox/com/com.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/listeners.h>
#include <VBox/com/NativeEventQueue.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <iprt/process.h> /* For RTProcSelf(). */
#include <iprt/thread.h>
#include <iprt/vfs.h>

#include <map>
#include <vector>

#ifdef USE_XPCOM_QUEUE
# include <sys/select.h>
# include <errno.h>
#endif

#include <signal.h>

#ifdef RT_OS_DARWIN
# include <CoreFoundation/CFRunLoop.h>
#endif

using namespace com;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define GCTLCMD_COMMON_OPT_USER             999 /**< The --username option number. */
#define GCTLCMD_COMMON_OPT_PASSWORD         998 /**< The --password option number. */
#define GCTLCMD_COMMON_OPT_PASSWORD_FILE    997 /**< The --password-file option number. */
#define GCTLCMD_COMMON_OPT_DOMAIN           996 /**< The --domain option number. */
/** Common option definitions. */
#define GCTLCMD_COMMON_OPTION_DEFS() \
        { "--username",             GCTLCMD_COMMON_OPT_USER,            RTGETOPT_REQ_STRING  }, \
        { "--passwordfile",         GCTLCMD_COMMON_OPT_PASSWORD_FILE,   RTGETOPT_REQ_STRING  }, \
        { "--password",             GCTLCMD_COMMON_OPT_PASSWORD,        RTGETOPT_REQ_STRING  }, \
        { "--domain",               GCTLCMD_COMMON_OPT_DOMAIN,          RTGETOPT_REQ_STRING  }, \
        { "--quiet",                'q',                                RTGETOPT_REQ_NOTHING }, \
        { "--verbose",              'v',                                RTGETOPT_REQ_NOTHING },

/** Handles common options in the typical option parsing switch. */
#define GCTLCMD_COMMON_OPTION_CASES(a_pCtx, a_ch, a_pValueUnion) \
        case 'v': \
        case 'q': \
        case GCTLCMD_COMMON_OPT_USER: \
        case GCTLCMD_COMMON_OPT_DOMAIN: \
        case GCTLCMD_COMMON_OPT_PASSWORD: \
        case GCTLCMD_COMMON_OPT_PASSWORD_FILE: \
        { \
            RTEXITCODE rcExitCommon = gctlCtxSetOption(a_pCtx, a_ch, a_pValueUnion); \
            if (RT_UNLIKELY(rcExitCommon != RTEXITCODE_SUCCESS)) \
                return rcExitCommon; \
        } break


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Set by the signal handler when current guest control
 *  action shall be aborted. */
static volatile bool g_fGuestCtrlCanceled = false;


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Listener declarations.
 */
VBOX_LISTENER_DECLARE(GuestFileEventListenerImpl)
VBOX_LISTENER_DECLARE(GuestProcessEventListenerImpl)
VBOX_LISTENER_DECLARE(GuestSessionEventListenerImpl)
VBOX_LISTENER_DECLARE(GuestEventListenerImpl)


/**
 * Definition of a guestcontrol command, with handler and various flags.
 */
typedef struct GCTLCMDDEF
{
    /** The command name. */
    const char *pszName;

    /**
     * Actual command handler callback.
     *
     * @param   pCtx            Pointer to command context to use.
     */
    DECLR3CALLBACKMEMBER(RTEXITCODE, pfnHandler, (struct GCTLCMDCTX *pCtx, int argc, char **argv));

    /** The command usage flags. */
    uint32_t    fCmdUsage;
    /** Command context flags (GCTLCMDCTX_F_XXX). */
    uint32_t    fCmdCtx;
} GCTLCMD;
/** Pointer to a const guest control command definition. */
typedef GCTLCMDDEF const *PCGCTLCMDDEF;

/** @name GCTLCMDCTX_F_XXX - Command context flags.
 * @{
 */
/** No flags set. */
#define GCTLCMDCTX_F_NONE               0
/** Don't install a signal handler (CTRL+C trap). */
#define GCTLCMDCTX_F_NO_SIGNAL_HANDLER  RT_BIT(0)
/** No guest session needed. */
#define GCTLCMDCTX_F_SESSION_ANONYMOUS  RT_BIT(1)
/** @} */

/**
 * Context for handling a specific command.
 */
typedef struct GCTLCMDCTX
{
    HandlerArg *pArg;

    /** Pointer to the command definition. */
    PCGCTLCMDDEF pCmdDef;
    /** The VM name or UUID. */
    const char *pszVmNameOrUuid;

    /** Whether we've done the post option parsing init already. */
    bool fPostOptionParsingInited;
    /** Whether we've locked the VM session. */
    bool fLockedVmSession;
    /** Whether to detach (@c true) or close the session. */
    bool fDetachGuestSession;
    /** Set if we've installed the signal handler.   */
    bool fInstalledSignalHandler;
    /** The verbosity level. */
    uint32_t cVerbose;
    /** User name. */
    Utf8Str strUsername;
    /** Password. */
    Utf8Str strPassword;
    /** Domain. */
    Utf8Str strDomain;
    /** Pointer to the IGuest interface. */
    ComPtr<IGuest> pGuest;
    /** Pointer to the to be used guest session. */
    ComPtr<IGuestSession> pGuestSession;
    /** The guest session ID. */
    ULONG uSessionID;

} GCTLCMDCTX, *PGCTLCMDCTX;

/**
 * An entry for a source element, including an optional DOS-like wildcard (*,?).
 */
class SourceFileEntry
{
public:
    SourceFileEntry(const char *pszSource, const char *pszFilter)
        : mSource(pszSource)
        , mFilter(pszFilter)
    {}

    SourceFileEntry(const char *pszSource)
    {
/** @todo Using host parsing rules for guest filenames might be undesirable... */

        /* Look for filter and split off the filter part if present. */
        const char *pszFilename = RTPathFilename(pszSource);
        if (   !pszFilename
            || !strpbrk(pszFilename, "*?"))
            mSource.assign(pszSource);
        else
        {
            mFilter = pszFilename;
            size_t cch = pszFilename - pszSource;
            if (cch > 0 && pszFilename[-1] != ':')
                cch--;
            mSource.assign(pszSource, cch);
        }
    }

    const Utf8Str &GetSource() const
    {
        return mSource;
    }

    const Utf8Str &GetFilter() const
    {
        return mFilter;
    }

protected:
    Utf8Str mSource;
    Utf8Str mFilter;
};
typedef std::vector<SourceFileEntry> SourceVec;

/**
 * An entry for an element which needs to be copied/created to/on the guest.
 */
typedef struct DESTFILEENTRY
{
    DESTFILEENTRY(Utf8Str strFileName) : mFileName(strFileName) {}
    Utf8Str mFileName;
} DESTFILEENTRY, *PDESTFILEENTRY;
/*
 * Map for holding destination entries, whereas the key is the destination
 * directory and the mapped value is a vector holding all elements for this directory.
 */
typedef std::map< Utf8Str, std::vector<DESTFILEENTRY> > DESTDIRMAP, *PDESTDIRMAP;
typedef std::map< Utf8Str, std::vector<DESTFILEENTRY> >::iterator DESTDIRMAPITER, *PDESTDIRMAPITER;


/**
 * RTGetOpt-IDs for the guest execution control command line.
 */
enum GETOPTDEF_EXEC
{
    GETOPTDEF_EXEC_IGNOREORPHANEDPROCESSES = 1000,
    GETOPTDEF_EXEC_NO_PROFILE,
    GETOPTDEF_EXEC_OUTPUTFORMAT,
    GETOPTDEF_EXEC_DOS2UNIX,
    GETOPTDEF_EXEC_UNIX2DOS,
    GETOPTDEF_EXEC_WAITFOREXIT,
    GETOPTDEF_EXEC_WAITFORSTDOUT,
    GETOPTDEF_EXEC_WAITFORSTDERR
};

enum kStreamTransform
{
    kStreamTransform_None = 0,
    kStreamTransform_Dos2Unix,
    kStreamTransform_Unix2Dos
};
#endif /* VBOX_ONLY_DOCS */


void usageGuestControl(PRTSTREAM pStrm, const char *pcszSep1, const char *pcszSep2, uint32_t uSubCmd)
{
    const uint32_t fAnonSubCmds = USAGE_GSTCTRL_CLOSESESSION
                                | USAGE_GSTCTRL_LIST
                                | USAGE_GSTCTRL_CLOSEPROCESS
                                | USAGE_GSTCTRL_CLOSESESSION
                                | USAGE_GSTCTRL_UPDATEGA
                                | USAGE_GSTCTRL_WATCH;

    /*                0         1         2         3         4         5         6         7         8XXXXXXXXXX */
    /*                0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 */
    if (~fAnonSubCmds & uSubCmd)
        RTStrmPrintf(pStrm,
                     "%s guestcontrol %s    <uuid|vmname> [--verbose|-v] [--quiet|-q]\n"
                     "                              [--username <name>] [--domain <domain>]\n"
                     "                              [--passwordfile <file> | --password <password>]\n%s",
                     pcszSep1, pcszSep2, uSubCmd == ~0U ? "\n" : "");
    if (uSubCmd & USAGE_GSTCTRL_RUN)
        RTStrmPrintf(pStrm,
                     "                              run [common-options]\n"
                     "                              [--exe <path to executable>] [--timeout <msec>]\n"
                     "                              [-E|--putenv <NAME>[=<VALUE>]] [--unquoted-args]\n"
                     "                              [--ignore-operhaned-processes] [--profile]\n"
                     "                              [--no-wait-stdout|--wait-stdout]\n"
                     "                              [--no-wait-stderr|--wait-stderr]\n"
                     "                              [--dos2unix] [--unix2dos]\n"
                     "                              -- <program/arg0> [argument1] ... [argumentN]]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_START)
        RTStrmPrintf(pStrm,
                     "                              start [common-options]\n"
                     "                              [--exe <path to executable>] [--timeout <msec>]\n"
                     "                              [-E|--putenv <NAME>[=<VALUE>]] [--unquoted-args]\n"
                     "                              [--ignore-operhaned-processes] [--profile]\n"
                     "                              -- <program/arg0> [argument1] ... [argumentN]]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_COPYFROM)
        RTStrmPrintf(pStrm,
                     "                              copyfrom [common-options]\n"
                     "                              [--follow] [-R|--recursive]\n"
                     "                              <guest-src0> [guest-src1 [...]] <host-dst>\n"
                     "\n"
                     "                              copyfrom [common-options]\n"
                     "                              [--follow] [-R|--recursive]\n"
                     "                              [--target-directory <host-dst-dir>]\n"
                     "                              <guest-src0> [guest-src1 [...]]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_COPYTO)
        RTStrmPrintf(pStrm,
                     "                              copyto [common-options]\n"
                     "                              [--follow] [-R|--recursive]\n"
                     "                              <host-src0> [host-src1 [...]] <guest-dst>\n"
                     "\n"
                     "                              copyto [common-options]\n"
                     "                              [--follow] [-R|--recursive]\n"
                     "                              [--target-directory <guest-dst>]\n"
                     "                              <host-src0> [host-src1 [...]]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_MKDIR)
        RTStrmPrintf(pStrm,
                     "                              mkdir|createdir[ectory] [common-options]\n"
                     "                              [--parents] [--mode <mode>]\n"
                     "                              <guest directory> [...]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_RMDIR)
        RTStrmPrintf(pStrm,
                     "                              rmdir|removedir[ectory] [common-options]\n"
                     "                              [-R|--recursive]\n"
                     "                              <guest directory> [...]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_RM)
        RTStrmPrintf(pStrm,
                     "                              removefile|rm [common-options] [-f|--force]\n"
                     "                              <guest file> [...]\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_MV)
        RTStrmPrintf(pStrm,
                     "                              mv|move|ren[ame] [common-options]\n"
                     "                              <source> [source1 [...]] <dest>\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_MKTEMP)
        RTStrmPrintf(pStrm,
                     "                              mktemp|createtemp[orary] [common-options]\n"
                     "                              [--secure] [--mode <mode>] [--tmpdir <directory>]\n"
                     "                              <template>\n"
                     "\n");
    if (uSubCmd & USAGE_GSTCTRL_STAT)
        RTStrmPrintf(pStrm,
                     "                              stat [common-options]\n"
                     "                              <file> [...]\n"
                     "\n");

    /*
     * Command not requiring authentication.
     */
    if (fAnonSubCmds & uSubCmd)
    {
        /*                0         1         2         3         4         5         6         7         8XXXXXXXXXX */
        /*                0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890 */
        RTStrmPrintf(pStrm,
                     "%s guestcontrol %s    <uuid|vmname> [--verbose|-v] [--quiet|-q]\n%s",
                     pcszSep1, pcszSep2, uSubCmd == ~0U ? "\n" : "");
        if (uSubCmd & USAGE_GSTCTRL_LIST)
            RTStrmPrintf(pStrm,
                     "                              list <all|sessions|processes|files> [common-opts]\n"
                     "\n");
        if (uSubCmd & USAGE_GSTCTRL_CLOSEPROCESS)
            RTStrmPrintf(pStrm,
                     "                              closeprocess [common-options]\n"
                     "                              <   --session-id <ID>\n"
                     "                                | --session-name <name or pattern>\n"
                     "                              <PID1> [PID1 [...]]\n"
                     "\n");
        if (uSubCmd & USAGE_GSTCTRL_CLOSESESSION)
            RTStrmPrintf(pStrm,
                     "                              closesession [common-options]\n"
                     "                              <  --all | --session-id <ID>\n"
                     "                                | --session-name <name or pattern> >\n"
                     "\n");
        if (uSubCmd & USAGE_GSTCTRL_UPDATEGA)
            RTStrmPrintf(pStrm,
                     "                              updatega|updateguestadditions|updateadditions\n"
                     "                              [--source <guest additions .ISO>]\n"
                     "                              [--wait-start] [common-options]\n"
                     "                              [-- [<argument1>] ... [<argumentN>]]\n"
                     "\n");
        if (uSubCmd & USAGE_GSTCTRL_WATCH)
            RTStrmPrintf(pStrm,
                     "                              watch [common-options]\n"
                     "\n");
    }
}

#ifndef VBOX_ONLY_DOCS


#ifdef RT_OS_WINDOWS
static BOOL WINAPI gctlSignalHandler(DWORD dwCtrlType)
{
    bool fEventHandled = FALSE;
    switch (dwCtrlType)
    {
        /* User pressed CTRL+C or CTRL+BREAK or an external event was sent
         * via GenerateConsoleCtrlEvent(). */
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_C_EVENT:
            ASMAtomicWriteBool(&g_fGuestCtrlCanceled, true);
            fEventHandled = TRUE;
            break;
        default:
            break;
        /** @todo Add other events here. */
    }

    return fEventHandled;
}
#else /* !RT_OS_WINDOWS */
/**
 * Signal handler that sets g_fGuestCtrlCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Don't do anything
 * unnecessary here.
 */
static void gctlSignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fGuestCtrlCanceled, true);
}
#endif


/**
 * Installs a custom signal handler to get notified
 * whenever the user wants to intercept the program.
 *
 * @todo Make this handler available for all VBoxManage modules?
 */
static int gctlSignalHandlerInstall(void)
{
    g_fGuestCtrlCanceled = false;

    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)gctlSignalHandler, TRUE /* Add handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to install console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   gctlSignalHandler);
    signal(SIGTERM,  gctlSignalHandler);
# ifdef SIGBREAK
    signal(SIGBREAK, gctlSignalHandler);
# endif
#endif
    return rc;
}


/**
 * Uninstalls a previously installed signal handler.
 */
static int gctlSignalHandlerUninstall(void)
{
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)NULL, FALSE /* Remove handler */))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTMsgError("Unable to uninstall console control handler, rc=%Rrc\n", rc);
    }
#else
    signal(SIGINT,   SIG_DFL);
    signal(SIGTERM,  SIG_DFL);
# ifdef SIGBREAK
    signal(SIGBREAK, SIG_DFL);
# endif
#endif
    return rc;
}


/**
 * Translates a process status to a human readable string.
 */
const char *gctlProcessStatusToText(ProcessStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case ProcessStatus_Starting:
            return "starting";
        case ProcessStatus_Started:
            return "started";
        case ProcessStatus_Paused:
            return "paused";
        case ProcessStatus_Terminating:
            return "terminating";
        case ProcessStatus_TerminatedNormally:
            return "successfully terminated";
        case ProcessStatus_TerminatedSignal:
            return "terminated by signal";
        case ProcessStatus_TerminatedAbnormally:
            return "abnormally aborted";
        case ProcessStatus_TimedOutKilled:
            return "timed out";
        case ProcessStatus_TimedOutAbnormally:
            return "timed out, hanging";
        case ProcessStatus_Down:
            return "killed";
        case ProcessStatus_Error:
            return "error";
        default:
            break;
    }
    return "unknown";
}

/**
 * Translates a guest process wait result to a human readable string.
 */
const char *gctlProcessWaitResultToText(ProcessWaitResult_T enmWaitResult)
{
    switch (enmWaitResult)
    {
        case ProcessWaitResult_Start:
            return "started";
        case ProcessWaitResult_Terminate:
            return "terminated";
        case ProcessWaitResult_Status:
            return "status changed";
        case ProcessWaitResult_Error:
            return "error";
        case ProcessWaitResult_Timeout:
            return "timed out";
        case ProcessWaitResult_StdIn:
            return "stdin ready";
        case ProcessWaitResult_StdOut:
            return "data on stdout";
        case ProcessWaitResult_StdErr:
            return "data on stderr";
        case ProcessWaitResult_WaitFlagNotSupported:
            return "waiting flag not supported";
        default:
            break;
    }
    return "unknown";
}

/**
 * Translates a guest session status to a human readable string.
 */
const char *gctlGuestSessionStatusToText(GuestSessionStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case GuestSessionStatus_Starting:
            return "starting";
        case GuestSessionStatus_Started:
            return "started";
        case GuestSessionStatus_Terminating:
            return "terminating";
        case GuestSessionStatus_Terminated:
            return "terminated";
        case GuestSessionStatus_TimedOutKilled:
            return "timed out";
        case GuestSessionStatus_TimedOutAbnormally:
            return "timed out, hanging";
        case GuestSessionStatus_Down:
            return "killed";
        case GuestSessionStatus_Error:
            return "error";
        default:
            break;
    }
    return "unknown";
}

/**
 * Translates a guest file status to a human readable string.
 */
const char *gctlFileStatusToText(FileStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case FileStatus_Opening:
            return "opening";
        case FileStatus_Open:
            return "open";
        case FileStatus_Closing:
            return "closing";
        case FileStatus_Closed:
            return "closed";
        case FileStatus_Down:
            return "killed";
        case FileStatus_Error:
            return "error";
        default:
            break;
    }
    return "unknown";
}

static int gctlPrintError(com::ErrorInfo &errorInfo)
{
    if (   errorInfo.isFullAvailable()
        || errorInfo.isBasicAvailable())
    {
        /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
         * because it contains more accurate info about what went wrong. */
        if (errorInfo.getResultCode() == VBOX_E_IPRT_ERROR)
            RTMsgError("%ls.", errorInfo.getText().raw());
        else
        {
            RTMsgError("Error details:");
            GluePrintErrorInfo(errorInfo);
        }
        return VERR_GENERAL_FAILURE; /** @todo */
    }
    AssertMsgFailedReturn(("Object has indicated no error (%Rhrc)!?\n", errorInfo.getResultCode()),
                          VERR_INVALID_PARAMETER);
}

static int gctlPrintError(IUnknown *pObj, const GUID &aIID)
{
    com::ErrorInfo ErrInfo(pObj, aIID);
    return gctlPrintError(ErrInfo);
}

static int gctlPrintProgressError(ComPtr<IProgress> pProgress)
{
    int vrc = VINF_SUCCESS;
    HRESULT rc;

    do
    {
        BOOL fCanceled;
        CHECK_ERROR_BREAK(pProgress, COMGETTER(Canceled)(&fCanceled));
        if (!fCanceled)
        {
            LONG rcProc;
            CHECK_ERROR_BREAK(pProgress, COMGETTER(ResultCode)(&rcProc));
            if (FAILED(rcProc))
            {
                com::ProgressErrorInfo ErrInfo(pProgress);
                vrc = gctlPrintError(ErrInfo);
            }
        }

    } while(0);

    AssertMsgStmt(SUCCEEDED(rc), ("Could not lookup progress information\n"), vrc = VERR_COM_UNEXPECTED);

    return vrc;
}



/*
 *
 *
 * Guest Control Command Context
 * Guest Control Command Context
 * Guest Control Command Context
 * Guest Control Command Context
 *
 *
 *
 */


/**
 * Initializes a guest control command context structure.
 *
 * @returns RTEXITCODE_SUCCESS on success, RTEXITCODE_FAILURE on failure (after
 *           informing the user of course).
 * @param   pCtx                The command context to init.
 * @param   pArg                The handle argument package.
 */
static RTEXITCODE gctrCmdCtxInit(PGCTLCMDCTX pCtx, HandlerArg *pArg)
{
    RT_ZERO(*pCtx);
    pCtx->pArg = pArg;

    /*
     * The user name defaults to the host one, if we can get at it.
     */
    char szUser[1024];
    int rc = RTProcQueryUsername(RTProcSelf(), szUser, sizeof(szUser), NULL);
    if (   RT_SUCCESS(rc)
        && RTStrIsValidEncoding(szUser)) /* paranoia required on posix */
    {
        try
        {
            pCtx->strUsername = szUser;
        }
        catch (std::bad_alloc &)
        {
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory");
        }
    }
    /* else: ignore this failure. */

    return RTEXITCODE_SUCCESS;
}


/**
 * Worker for GCTLCMD_COMMON_OPTION_CASES.
 *
 * @returns RTEXITCODE_SUCCESS if the option was handled successfully.  If not,
 *          an error message is printed and an appropriate failure exit code is
 *          returned.
 * @param   pCtx                The guest control command context.
 * @param   ch                  The option char or ordinal.
 * @param   pValueUnion         The option value union.
 */
static RTEXITCODE gctlCtxSetOption(PGCTLCMDCTX pCtx, int ch, PRTGETOPTUNION pValueUnion)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    switch (ch)
    {
        case GCTLCMD_COMMON_OPT_USER: /* User name */
            if (!pCtx->pCmdDef || !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS))
                pCtx->strUsername = pValueUnion->psz;
            else
                RTMsgWarning("The --username|-u option is ignored by '%s'", pCtx->pCmdDef->pszName);
            break;

        case GCTLCMD_COMMON_OPT_PASSWORD: /* Password */
            if (!pCtx->pCmdDef || !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS))
            {
                if (pCtx->strPassword.isNotEmpty())
                    RTMsgWarning("Password is given more than once.");
                pCtx->strPassword = pValueUnion->psz;
            }
            else
                RTMsgWarning("The --password option is ignored by '%s'", pCtx->pCmdDef->pszName);
            break;

        case GCTLCMD_COMMON_OPT_PASSWORD_FILE: /* Password file */
            if (!pCtx->pCmdDef || !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS))
                rcExit = readPasswordFile(pValueUnion->psz, &pCtx->strPassword);
            else
                RTMsgWarning("The --password-file|-p option is ignored by '%s'", pCtx->pCmdDef->pszName);
            break;

        case GCTLCMD_COMMON_OPT_DOMAIN: /* domain */
            if (!pCtx->pCmdDef || !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS))
                pCtx->strDomain = pValueUnion->psz;
            else
                RTMsgWarning("The --domain option is ignored by '%s'", pCtx->pCmdDef->pszName);
            break;

        case 'v': /* --verbose */
            pCtx->cVerbose++;
            break;

        case 'q': /* --quiet */
            if (pCtx->cVerbose)
                pCtx->cVerbose--;
            break;

        default:
            AssertFatalMsgFailed(("ch=%d (%c)\n", ch, ch));
    }
    return rcExit;
}


/**
 * Initializes the VM for IGuest operation.
 *
 * This opens a shared session to a running VM and gets hold of IGuest.
 *
 * @returns RTEXITCODE_SUCCESS on success.  RTEXITCODE_FAILURE and user message
 *          on failure.
 * @param   pCtx            The guest control command context.
 *                          GCTLCMDCTX::pGuest will be set on success.
 */
static RTEXITCODE gctlCtxInitVmSession(PGCTLCMDCTX pCtx)
{
    HRESULT rc;
    AssertPtr(pCtx);
    AssertPtr(pCtx->pArg);

    /*
     * Find the VM and check if it's running.
     */
    ComPtr<IMachine> machine;
    CHECK_ERROR(pCtx->pArg->virtualBox, FindMachine(Bstr(pCtx->pszVmNameOrUuid).raw(), machine.asOutParam()));
    if (SUCCEEDED(rc))
    {
        MachineState_T enmMachineState;
        CHECK_ERROR(machine, COMGETTER(State)(&enmMachineState));
        if (   SUCCEEDED(rc)
            && enmMachineState == MachineState_Running)
        {
            /*
             * It's running. So, open a session to it and get the IGuest interface.
             */
            CHECK_ERROR(machine, LockMachine(pCtx->pArg->session, LockType_Shared));
            if (SUCCEEDED(rc))
            {
                pCtx->fLockedVmSession = true;
                ComPtr<IConsole> ptrConsole;
                CHECK_ERROR(pCtx->pArg->session, COMGETTER(Console)(ptrConsole.asOutParam()));
                if (SUCCEEDED(rc))
                {
                    if (ptrConsole.isNotNull())
                    {
                        CHECK_ERROR(ptrConsole, COMGETTER(Guest)(pCtx->pGuest.asOutParam()));
                        if (SUCCEEDED(rc))
                            return RTEXITCODE_SUCCESS;
                    }
                    else
                        RTMsgError("Failed to get a IConsole pointer for the machine. Is it still running?\n");
                }
            }
        }
        else if (SUCCEEDED(rc))
            RTMsgError("Machine \"%s\" is not running (currently %s)!\n",
                       pCtx->pszVmNameOrUuid, machineStateToName(enmMachineState, false));
    }
    return RTEXITCODE_FAILURE;
}


/**
 * Creates a guest session with the VM.
 *
 * @retval  RTEXITCODE_SUCCESS on success.
 * @retval  RTEXITCODE_FAILURE and user message on failure.
 * @param   pCtx            The guest control command context.
 *                          GCTCMDCTX::pGuestSession and GCTLCMDCTX::uSessionID
 *                          will be set.
 */
static RTEXITCODE gctlCtxInitGuestSession(PGCTLCMDCTX pCtx)
{
    HRESULT rc;
    AssertPtr(pCtx);
    Assert(!(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS));
    Assert(pCtx->pGuest.isNotNull());

    /*
     * Build up a reasonable guest session name. Useful for identifying
     * a specific session when listing / searching for them.
     */
    char *pszSessionName;
    if (RTStrAPrintf(&pszSessionName,
                     "[%RU32] VBoxManage Guest Control [%s] - %s",
                     RTProcSelf(), pCtx->pszVmNameOrUuid, pCtx->pCmdDef->pszName) < 0)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "No enough memory for session name");

    /*
     * Create a guest session.
     */
    if (pCtx->cVerbose)
        RTPrintf("Creating guest session as user '%s'...\n", pCtx->strUsername.c_str());
    try
    {
        CHECK_ERROR(pCtx->pGuest, CreateSession(Bstr(pCtx->strUsername).raw(),
                                                Bstr(pCtx->strPassword).raw(),
                                                Bstr(pCtx->strDomain).raw(),
                                                Bstr(pszSessionName).raw(),
                                                pCtx->pGuestSession.asOutParam()));
    }
    catch (std::bad_alloc &)
    {
        RTMsgError("Out of memory setting up IGuest::CreateSession call");
        rc = E_OUTOFMEMORY;
    }
    if (SUCCEEDED(rc))
    {
        /*
         * Wait for guest session to start.
         */
        if (pCtx->cVerbose)
            RTPrintf("Waiting for guest session to start...\n");
        GuestSessionWaitResult_T enmWaitResult = GuestSessionWaitResult_None; /* Shut up MSC */
        try
        {
            com::SafeArray<GuestSessionWaitForFlag_T> aSessionWaitFlags;
            aSessionWaitFlags.push_back(GuestSessionWaitForFlag_Start);
            CHECK_ERROR(pCtx->pGuestSession, WaitForArray(ComSafeArrayAsInParam(aSessionWaitFlags),
                                                          /** @todo Make session handling timeouts configurable. */
                                                          30 * 1000, &enmWaitResult));
        }
        catch (std::bad_alloc &)
        {
            RTMsgError("Out of memory setting up IGuestSession::WaitForArray call");
            rc = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(rc))
        {
            /* The WaitFlagNotSupported result may happen with GAs older than 4.3. */
            if (   enmWaitResult == GuestSessionWaitResult_Start
                || enmWaitResult == GuestSessionWaitResult_WaitFlagNotSupported)
            {
                /*
                 * Get the session ID and we're ready to rumble.
                 */
                CHECK_ERROR(pCtx->pGuestSession, COMGETTER(Id)(&pCtx->uSessionID));
                if (SUCCEEDED(rc))
                {
                    if (pCtx->cVerbose)
                        RTPrintf("Successfully started guest session (ID %RU32)\n", pCtx->uSessionID);
                    RTStrFree(pszSessionName);
                    return RTEXITCODE_SUCCESS;
                }
            }
            else
            {
                GuestSessionStatus_T enmSessionStatus;
                CHECK_ERROR(pCtx->pGuestSession, COMGETTER(Status)(&enmSessionStatus));
                RTMsgError("Error starting guest session (current status is: %s)\n",
                           SUCCEEDED(rc) ? gctlGuestSessionStatusToText(enmSessionStatus) : "<unknown>");
            }
        }
    }

    RTStrFree(pszSessionName);
    return RTEXITCODE_FAILURE;
}


/**
 * Completes the guest control context initialization after parsing arguments.
 *
 * Will validate common arguments, open a VM session, and if requested open a
 * guest session and install the CTRL-C signal handler.
 *
 * It is good to validate all the options and arguments you can before making
 * this call.  However, the VM session, IGuest and IGuestSession interfaces are
 * not availabe till after this call, so take care.
 *
 * @retval  RTEXITCODE_SUCCESS on success.
 * @retval  RTEXITCODE_FAILURE and user message on failure.
 * @param   pCtx            The guest control command context.
 *                          GCTCMDCTX::pGuestSession and GCTLCMDCTX::uSessionID
 *                          will be set.
 * @remarks Can safely be called multiple times, will only do work once.
 */
static RTEXITCODE gctlCtxPostOptionParsingInit(PGCTLCMDCTX pCtx)
{
    if (pCtx->fPostOptionParsingInited)
        return RTEXITCODE_SUCCESS;

    /*
     * Check that the user name isn't empty when we need it.
     */
    RTEXITCODE rcExit;
    if (  (pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS)
        || pCtx->strUsername.isNotEmpty())
    {
        /*
         * Open the VM session and if required, a guest session.
         */
        rcExit = gctlCtxInitVmSession(pCtx);
        if (   rcExit == RTEXITCODE_SUCCESS
            && !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS))
            rcExit = gctlCtxInitGuestSession(pCtx);
        if (rcExit == RTEXITCODE_SUCCESS)
        {
            /*
             * Install signal handler if requested (errors are ignored).
             */
            if (!(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_NO_SIGNAL_HANDLER))
            {
                int rc = gctlSignalHandlerInstall();
                pCtx->fInstalledSignalHandler = RT_SUCCESS(rc);
            }
        }
    }
    else
        rcExit = errorSyntaxEx(USAGE_GUESTCONTROL, pCtx->pCmdDef->fCmdUsage, "No user name specified!");

    pCtx->fPostOptionParsingInited = rcExit == RTEXITCODE_SUCCESS;
    return rcExit;
}


/**
 * Cleans up the context when the command returns.
 *
 * This will close any open guest session, unless the DETACH flag is set.
 * It will also close any VM session that may be been established.  Any signal
 * handlers we've installed will also be removed.
 *
 * Un-initializes the VM after guest control usage.
 * @param   pCmdCtx                 Pointer to command context.
 */
static void gctlCtxTerm(PGCTLCMDCTX pCtx)
{
    HRESULT rc;
    AssertPtr(pCtx);

    /*
     * Uninstall signal handler.
     */
    if (pCtx->fInstalledSignalHandler)
    {
        gctlSignalHandlerUninstall();
        pCtx->fInstalledSignalHandler = false;
    }

    /*
     * Close, or at least release, the guest session.
     */
    if (pCtx->pGuestSession.isNotNull())
    {
        if (   !(pCtx->pCmdDef->fCmdCtx & GCTLCMDCTX_F_SESSION_ANONYMOUS)
            && !pCtx->fDetachGuestSession)
        {
            if (pCtx->cVerbose)
                RTPrintf("Closing guest session ...\n");

            CHECK_ERROR(pCtx->pGuestSession, Close());
        }
        else if (   pCtx->fDetachGuestSession
                 && pCtx->cVerbose)
            RTPrintf("Guest session detached\n");

        pCtx->pGuestSession.setNull();
    }

    /*
     * Close the VM session.
     */
    if (pCtx->fLockedVmSession)
    {
        Assert(pCtx->pArg->session.isNotNull());
        CHECK_ERROR(pCtx->pArg->session, UnlockMachine());
        pCtx->fLockedVmSession = false;
    }
}





/*
 *
 *
 * Guest Control Command Handling.
 * Guest Control Command Handling.
 * Guest Control Command Handling.
 * Guest Control Command Handling.
 * Guest Control Command Handling.
 *
 *
 */


/** @name EXITCODEEXEC_XXX - Special run exit codes.
 *
 * Special exit codes for returning errors/information of a started guest
 * process to the command line VBoxManage was started from.  Useful for e.g.
 * scripting.
 *
 * ASSUMING that all platforms have at least 7-bits for the exit code we can do
 * the following mapping:
 *  - Guest exit code 0 is mapped to 0 on the host.
 *  - Guest exit codes 1 thru 93 (0x5d) are displaced by 32, so that 1
 *    becomes 33 (0x21) on the host and 93 becomes 125 (0x7d) on the host.
 *  - Guest exit codes 94 (0x5e) and above are mapped to 126 (0x5e).
 *
 * We ASSUME that all VBoxManage status codes are in the range 0 thru 32.
 *
 * @note    These are frozen as of 4.1.0.
 * @note    The guest exit code mappings was introduced with 5.0 and the 'run'
 *          command, they are/was not supported by 'exec'.
 * @sa      gctlRunCalculateExitCode
 */
/** Process exited normally but with an exit code <> 0. */
#define EXITCODEEXEC_CODE           ((RTEXITCODE)16)
#define EXITCODEEXEC_FAILED         ((RTEXITCODE)17)
#define EXITCODEEXEC_TERM_SIGNAL    ((RTEXITCODE)18)
#define EXITCODEEXEC_TERM_ABEND     ((RTEXITCODE)19)
#define EXITCODEEXEC_TIMEOUT        ((RTEXITCODE)20)
#define EXITCODEEXEC_DOWN           ((RTEXITCODE)21)
/** Execution was interrupt by user (ctrl-c). */
#define EXITCODEEXEC_CANCELED       ((RTEXITCODE)22)
/** The first mapped guest (non-zero) exit code. */
#define EXITCODEEXEC_MAPPED_FIRST           33
/** The last mapped guest (non-zero) exit code value (inclusive). */
#define EXITCODEEXEC_MAPPED_LAST            125
/** The number of exit codes from EXITCODEEXEC_MAPPED_FIRST to
 * EXITCODEEXEC_MAPPED_LAST.  This is also the highest guest exit code number
 * we're able to map. */
#define EXITCODEEXEC_MAPPED_RANGE           (93)
/** The guest exit code displacement value. */
#define EXITCODEEXEC_MAPPED_DISPLACEMENT    32
/** The guest exit code was too big to be mapped. */
#define EXITCODEEXEC_MAPPED_BIG             ((RTEXITCODE)126)
/** @} */

/**
 * Calculates the exit code of VBoxManage.
 *
 * @returns The exit code to return.
 * @param   enmStatus           The guest process status.
 * @param   uExitCode           The associated guest process exit code (where
 *                              applicable).
 * @param   fReturnExitCodes    Set if we're to use the 32-126 range for guest
 *                              exit codes.
 */
static RTEXITCODE gctlRunCalculateExitCode(ProcessStatus_T enmStatus, ULONG uExitCode, bool fReturnExitCodes)
{
    switch (enmStatus)
    {
        case ProcessStatus_TerminatedNormally:
            if (uExitCode == 0)
                return RTEXITCODE_SUCCESS;
            if (!fReturnExitCodes)
                return EXITCODEEXEC_CODE;
            if (uExitCode <= EXITCODEEXEC_MAPPED_RANGE)
                return (RTEXITCODE) (uExitCode + EXITCODEEXEC_MAPPED_DISPLACEMENT);
            return EXITCODEEXEC_MAPPED_BIG;

        case ProcessStatus_TerminatedAbnormally:
            return EXITCODEEXEC_TERM_ABEND;
        case ProcessStatus_TerminatedSignal:
            return EXITCODEEXEC_TERM_SIGNAL;

#if 0  /* see caller! */
        case ProcessStatus_TimedOutKilled:
            return EXITCODEEXEC_TIMEOUT;
        case ProcessStatus_Down:
            return EXITCODEEXEC_DOWN;   /* Service/OS is stopping, process was killed. */
        case ProcessStatus_Error:
            return EXITCODEEXEC_FAILED;

        /* The following is probably for detached? */
        case ProcessStatus_Starting:
            return RTEXITCODE_SUCCESS;
        case ProcessStatus_Started:
            return RTEXITCODE_SUCCESS;
        case ProcessStatus_Paused:
            return RTEXITCODE_SUCCESS;
        case ProcessStatus_Terminating:
            return RTEXITCODE_SUCCESS; /** @todo ???? */
#endif

        default:
            AssertMsgFailed(("Unknown exit status (%u/%u) from guest process returned!\n", enmStatus, uExitCode));
            return RTEXITCODE_FAILURE;
    }
}


/**
 * Pumps guest output to the host.
 *
 * @return  IPRT status code.
 * @param   pProcess        Pointer to appropriate process object.
 * @param   hVfsIosDst      Where to write the data.
 * @param   uHandle         Handle where to read the data from.
 * @param   cMsTimeout      Timeout (in ms) to wait for the operation to
 *                          complete.
 */
static int gctlRunPumpOutput(IProcess *pProcess, RTVFSIOSTREAM hVfsIosDst, ULONG uHandle, RTMSINTERVAL cMsTimeout)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    Assert(hVfsIosDst != NIL_RTVFSIOSTREAM);

    int vrc;

    SafeArray<BYTE> aOutputData;
    HRESULT hrc = pProcess->Read(uHandle, _64K, RT_MAX(cMsTimeout, 1), ComSafeArrayAsOutParam(aOutputData));
    if (SUCCEEDED(hrc))
    {
        size_t cbOutputData = aOutputData.size();
        if (cbOutputData == 0)
            vrc = VINF_SUCCESS;
        else
        {
            BYTE const *pbBuf = aOutputData.raw();
            AssertPtr(pbBuf);

            vrc = RTVfsIoStrmWrite(hVfsIosDst, pbBuf, cbOutputData, true /*fBlocking*/,  NULL);
            if (RT_FAILURE(vrc))
                RTMsgError("Unable to write output, rc=%Rrc\n", vrc);
        }
    }
    else
        vrc = gctlPrintError(pProcess, COM_IIDOF(IProcess));
    return vrc;
}


/**
 * Configures a host handle for pumping guest bits.
 *
 * @returns true if enabled and we successfully configured it.
 * @param   fEnabled            Whether pumping this pipe is configured.
 * @param   enmHandle           The IPRT standard handle designation.
 * @param   pszName             The name for user messages.
 * @param   enmTransformation   The transformation to apply.
 * @param   phVfsIos            Where to return the resulting I/O stream handle.
 */
static bool gctlRunSetupHandle(bool fEnabled, RTHANDLESTD enmHandle, const char *pszName,
                               kStreamTransform enmTransformation, PRTVFSIOSTREAM phVfsIos)
{
    if (fEnabled)
    {
        int vrc = RTVfsIoStrmFromStdHandle(enmHandle, 0, true /*fLeaveOpen*/, phVfsIos);
        if (RT_SUCCESS(vrc))
        {
            if (enmTransformation != kStreamTransform_None)
            {
                RTMsgWarning("Unsupported %s line ending conversion", pszName);
                /** @todo Implement dos2unix and unix2dos stream filters. */
            }
            return true;
        }
        RTMsgWarning("Error getting %s handle: %Rrc", pszName, vrc);
    }
    return false;
}


/**
 * Returns the remaining time (in ms) based on the start time and a set
 * timeout value. Returns RT_INDEFINITE_WAIT if no timeout was specified.
 *
 * @return  RTMSINTERVAL    Time left (in ms).
 * @param   u64StartMs      Start time (in ms).
 * @param   cMsTimeout      Timeout value (in ms).
 */
static RTMSINTERVAL gctlRunGetRemainingTime(uint64_t u64StartMs, RTMSINTERVAL cMsTimeout)
{
    if (!cMsTimeout || cMsTimeout == RT_INDEFINITE_WAIT) /* If no timeout specified, wait forever. */
        return RT_INDEFINITE_WAIT;

    uint64_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
    if (u64ElapsedMs >= cMsTimeout)
        return 0;

    return cMsTimeout - (RTMSINTERVAL)u64ElapsedMs;
}

/**
 * Common handler for the 'run' and 'start' commands.
 *
 * @returns Command exit code.
 * @param   pCtx        Guest session context.
 * @param   argc        The argument count.
 * @param   argv        The argument vector for this command.
 * @param   fRunCmd     Set if it's 'run' clear if 'start'.
 * @param   fHelp       The help flag for the command.
 */
static RTEXITCODE gctlHandleRunCommon(PGCTLCMDCTX pCtx, int argc, char **argv, bool fRunCmd, uint32_t fHelp)
{
    RT_NOREF(fHelp);
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    /*
     * Parse arguments.
     */
    enum kGstCtrlRunOpt
    {
        kGstCtrlRunOpt_IgnoreOrphanedProcesses = 1000,
        kGstCtrlRunOpt_NoProfile, /** @todo Deprecated and will be removed soon; use kGstCtrlRunOpt_Profile instead, if needed. */
        kGstCtrlRunOpt_Profile,
        kGstCtrlRunOpt_Dos2Unix,
        kGstCtrlRunOpt_Unix2Dos,
        kGstCtrlRunOpt_WaitForStdOut,
        kGstCtrlRunOpt_NoWaitForStdOut,
        kGstCtrlRunOpt_WaitForStdErr,
        kGstCtrlRunOpt_NoWaitForStdErr
    };
    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--putenv",                       'E',                                      RTGETOPT_REQ_STRING  },
        { "--exe",                          'e',                                      RTGETOPT_REQ_STRING  },
        { "--timeout",                      't',                                      RTGETOPT_REQ_UINT32  },
        { "--unquoted-args",                'u',                                      RTGETOPT_REQ_NOTHING },
        { "--ignore-operhaned-processes",   kGstCtrlRunOpt_IgnoreOrphanedProcesses,   RTGETOPT_REQ_NOTHING },
        { "--no-profile",                   kGstCtrlRunOpt_NoProfile,                 RTGETOPT_REQ_NOTHING }, /** @todo Deprecated. */
        { "--profile",                      kGstCtrlRunOpt_Profile,                   RTGETOPT_REQ_NOTHING },
        /* run only: 6 - options */
        { "--dos2unix",                     kGstCtrlRunOpt_Dos2Unix,                  RTGETOPT_REQ_NOTHING },
        { "--unix2dos",                     kGstCtrlRunOpt_Unix2Dos,                  RTGETOPT_REQ_NOTHING },
        { "--no-wait-stdout",               kGstCtrlRunOpt_NoWaitForStdOut,           RTGETOPT_REQ_NOTHING },
        { "--wait-stdout",                  kGstCtrlRunOpt_WaitForStdOut,             RTGETOPT_REQ_NOTHING },
        { "--no-wait-stderr",               kGstCtrlRunOpt_NoWaitForStdErr,           RTGETOPT_REQ_NOTHING },
        { "--wait-stderr",                  kGstCtrlRunOpt_WaitForStdErr,             RTGETOPT_REQ_NOTHING },
    };

    /** @todo stdin handling.   */

    int                     ch;
    RTGETOPTUNION           ValueUnion;
    RTGETOPTSTATE           GetState;
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions) - (fRunCmd ? 0 : 6),
                           1, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRC(vrc);

    com::SafeArray<ProcessCreateFlag_T>     aCreateFlags;
    com::SafeArray<ProcessWaitForFlag_T>    aWaitFlags;
    com::SafeArray<IN_BSTR>                 aArgs;
    com::SafeArray<IN_BSTR>                 aEnv;
    const char *                            pszImage            = NULL;
    bool                                    fWaitForStdOut      = fRunCmd;
    bool                                    fWaitForStdErr      = fRunCmd;
    RTVFSIOSTREAM                           hVfsStdOut          = NIL_RTVFSIOSTREAM;
    RTVFSIOSTREAM                           hVfsStdErr          = NIL_RTVFSIOSTREAM;
    enum kStreamTransform                   enmStdOutTransform  = kStreamTransform_None;
    enum kStreamTransform                   enmStdErrTransform  = kStreamTransform_None;
    RTMSINTERVAL                            cMsTimeout          = 0;

    try
    {
        /* Wait for process start in any case. This is useful for scripting VBoxManage
         * when relying on its overall exit code. */
        aWaitFlags.push_back(ProcessWaitForFlag_Start);

        while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
        {
            /* For options that require an argument, ValueUnion has received the value. */
            switch (ch)
            {
                GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

                case 'E':
                    if (   ValueUnion.psz[0] == '\0'
                        || ValueUnion.psz[0] == '=')
                        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RUN,
                                             "Invalid argument variable[=value]: '%s'", ValueUnion.psz);
                    aEnv.push_back(Bstr(ValueUnion.psz).raw());
                    break;

                case kGstCtrlRunOpt_IgnoreOrphanedProcesses:
                    aCreateFlags.push_back(ProcessCreateFlag_IgnoreOrphanedProcesses);
                    break;

                case kGstCtrlRunOpt_NoProfile:
                    /** @todo Deprecated, will be removed. */
                    RTPrintf("Warning: Deprecated option \"--no-profile\" specified\n");
                    break;

                case kGstCtrlRunOpt_Profile:
                    aCreateFlags.push_back(ProcessCreateFlag_Profile);
                    break;

                case 'e':
                    pszImage = ValueUnion.psz;
                    break;

                case 'u':
                    aCreateFlags.push_back(ProcessCreateFlag_UnquotedArguments);
                    break;

                /** @todo Add a hidden flag. */

                case 't': /* Timeout */
                    cMsTimeout = ValueUnion.u32;
                    break;

                /* run only options: */
                case kGstCtrlRunOpt_Dos2Unix:
                    Assert(fRunCmd);
                    enmStdErrTransform = enmStdOutTransform = kStreamTransform_Dos2Unix;
                    break;
                case kGstCtrlRunOpt_Unix2Dos:
                    Assert(fRunCmd);
                    enmStdErrTransform = enmStdOutTransform = kStreamTransform_Unix2Dos;
                    break;

                case kGstCtrlRunOpt_WaitForStdOut:
                    Assert(fRunCmd);
                    fWaitForStdOut = true;
                    break;
                case kGstCtrlRunOpt_NoWaitForStdOut:
                    Assert(fRunCmd);
                    fWaitForStdOut = false;
                    break;

                case kGstCtrlRunOpt_WaitForStdErr:
                    Assert(fRunCmd);
                    fWaitForStdErr = true;
                    break;
                case kGstCtrlRunOpt_NoWaitForStdErr:
                    Assert(fRunCmd);
                    fWaitForStdErr = false;
                    break;

                case VINF_GETOPT_NOT_OPTION:
                    aArgs.push_back(Bstr(ValueUnion.psz).raw());
                    if (!pszImage)
                    {
                        Assert(aArgs.size() == 1);
                        pszImage = ValueUnion.psz;
                    }
                    break;

                default:
                    return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RUN, ch, &ValueUnion);

            } /* switch */
        } /* while RTGetOpt */

        /* Must have something to execute. */
        if (!pszImage || !*pszImage)
            return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RUN, "No executable specified!");

        /*
         * Finalize process creation and wait flags and input/output streams.
         */
        if (!fRunCmd)
        {
            aCreateFlags.push_back(ProcessCreateFlag_WaitForProcessStartOnly);
            Assert(!fWaitForStdOut);
            Assert(!fWaitForStdErr);
        }
        else
        {
            aWaitFlags.push_back(ProcessWaitForFlag_Terminate);
            fWaitForStdOut = gctlRunSetupHandle(fWaitForStdOut, RTHANDLESTD_OUTPUT, "stdout", enmStdOutTransform, &hVfsStdOut);
            if (fWaitForStdOut)
            {
                aCreateFlags.push_back(ProcessCreateFlag_WaitForStdOut);
                aWaitFlags.push_back(ProcessWaitForFlag_StdOut);
            }
            fWaitForStdErr = gctlRunSetupHandle(fWaitForStdErr, RTHANDLESTD_ERROR, "stderr", enmStdErrTransform, &hVfsStdErr);
            if (fWaitForStdErr)
            {
                aCreateFlags.push_back(ProcessCreateFlag_WaitForStdErr);
                aWaitFlags.push_back(ProcessWaitForFlag_StdErr);
            }
        }
    }
    catch (std::bad_alloc &)
    {
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "VERR_NO_MEMORY\n");
    }

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    HRESULT rc;

    try
    {
        do
        {
            /* Get current time stamp to later calculate rest of timeout left. */
            uint64_t msStart = RTTimeMilliTS();

            /*
             * Create the process.
             */
            if (pCtx->cVerbose)
            {
                if (cMsTimeout == 0)
                    RTPrintf("Starting guest process ...\n");
                else
                    RTPrintf("Starting guest process (within %ums)\n", cMsTimeout);
            }
            ComPtr<IGuestProcess> pProcess;
            CHECK_ERROR_BREAK(pCtx->pGuestSession, ProcessCreate(Bstr(pszImage).raw(),
                                                                 ComSafeArrayAsInParam(aArgs),
                                                                 ComSafeArrayAsInParam(aEnv),
                                                                 ComSafeArrayAsInParam(aCreateFlags),
                                                                 gctlRunGetRemainingTime(msStart, cMsTimeout),
                                                                 pProcess.asOutParam()));

            /*
             * Explicitly wait for the guest process to be in a started state.
             */
            com::SafeArray<ProcessWaitForFlag_T> aWaitStartFlags;
            aWaitStartFlags.push_back(ProcessWaitForFlag_Start);
            ProcessWaitResult_T waitResult;
            CHECK_ERROR_BREAK(pProcess, WaitForArray(ComSafeArrayAsInParam(aWaitStartFlags),
                                                     gctlRunGetRemainingTime(msStart, cMsTimeout), &waitResult));

            ULONG uPID = 0;
            CHECK_ERROR_BREAK(pProcess, COMGETTER(PID)(&uPID));
            if (fRunCmd && pCtx->cVerbose)
                RTPrintf("Process '%s' (PID %RU32) started\n", pszImage, uPID);
            else if (!fRunCmd && pCtx->cVerbose)
            {
                /* Just print plain PID to make it easier for scripts
                 * invoking VBoxManage. */
                RTPrintf("[%RU32 - Session %RU32]\n", uPID, pCtx->uSessionID);
            }

            /*
             * Wait for process to exit/start...
             */
            RTMSINTERVAL    cMsTimeLeft = 1; /* Will be calculated. */
            bool            fReadStdOut = false;
            bool            fReadStdErr = false;
            bool            fCompleted  = false;
            bool            fCompletedStartCmd = false;

            vrc = VINF_SUCCESS;
            while (   !fCompleted
                   && cMsTimeLeft > 0)
            {
                cMsTimeLeft = gctlRunGetRemainingTime(msStart, cMsTimeout);
                CHECK_ERROR_BREAK(pProcess, WaitForArray(ComSafeArrayAsInParam(aWaitFlags),
                                                         RT_MIN(500 /*ms*/, RT_MAX(cMsTimeLeft, 1 /*ms*/)),
                                                         &waitResult));
                switch (waitResult)
                {
                    case ProcessWaitResult_Start:
                        fCompletedStartCmd = fCompleted = !fRunCmd; /* Only wait for startup if the 'start' command. */
                        break;
                    case ProcessWaitResult_StdOut:
                        fReadStdOut = true;
                        break;
                    case ProcessWaitResult_StdErr:
                        fReadStdErr = true;
                        break;
                    case ProcessWaitResult_Terminate:
                        if (pCtx->cVerbose)
                            RTPrintf("Process terminated\n");
                        /* Process terminated, we're done. */
                        fCompleted = true;
                        break;
                    case ProcessWaitResult_WaitFlagNotSupported:
                        /* The guest does not support waiting for stdout/err, so
                         * yield to reduce the CPU load due to busy waiting. */
                        RTThreadYield();
                        fReadStdOut = fReadStdErr = true;
                        break;
                    case ProcessWaitResult_Timeout:
                    {
                        /** @todo It is really unclear whether we will get stuck with the timeout
                         *        result here if the guest side times out the process and fails to
                         *        kill the process...  To be on the save side, double the IPC and
                         *        check the process status every time we time out.  */
                        ProcessStatus_T enmProcStatus;
                        CHECK_ERROR_BREAK(pProcess, COMGETTER(Status)(&enmProcStatus));
                        if (   enmProcStatus == ProcessStatus_TimedOutKilled
                            || enmProcStatus == ProcessStatus_TimedOutAbnormally)
                            fCompleted = true;
                        fReadStdOut = fReadStdErr = true;
                        break;
                    }
                    case ProcessWaitResult_Status:
                        /* ignore. */
                        break;
                    case ProcessWaitResult_Error:
                        /* waitFor is dead in the water, I think, so better leave the loop. */
                        vrc = VERR_CALLBACK_RETURN;
                        break;

                    case ProcessWaitResult_StdIn:   AssertFailed(); /* did ask for this! */ break;
                    case ProcessWaitResult_None:    AssertFailed(); /* used. */ break;
                    default:                        AssertFailed(); /* huh? */ break;
                }

                if (g_fGuestCtrlCanceled)
                    break;

                /*
                 * Pump output as needed.
                 */
                if (fReadStdOut)
                {
                    cMsTimeLeft = gctlRunGetRemainingTime(msStart, cMsTimeout);
                    int vrc2 = gctlRunPumpOutput(pProcess, hVfsStdOut, 1 /* StdOut */, cMsTimeLeft);
                    if (RT_FAILURE(vrc2) && RT_SUCCESS(vrc))
                        vrc = vrc2;
                    fReadStdOut = false;
                }
                if (fReadStdErr)
                {
                    cMsTimeLeft = gctlRunGetRemainingTime(msStart, cMsTimeout);
                    int vrc2 = gctlRunPumpOutput(pProcess, hVfsStdErr, 2 /* StdErr */, cMsTimeLeft);
                    if (RT_FAILURE(vrc2) && RT_SUCCESS(vrc))
                        vrc = vrc2;
                    fReadStdErr = false;
                }
                if (   RT_FAILURE(vrc)
                    || g_fGuestCtrlCanceled)
                    break;

                /*
                 * Process events before looping.
                 */
                NativeEventQueue::getMainEventQueue()->processEventQueue(0);
            } /* while */

            /*
             * Report status back to the user.
             */
            if (g_fGuestCtrlCanceled)
            {
                if (pCtx->cVerbose)
                    RTPrintf("Process execution aborted!\n");
                rcExit = EXITCODEEXEC_CANCELED;
            }
            else if (fCompletedStartCmd)
            {
                if (pCtx->cVerbose)
                    RTPrintf("Process successfully started!\n");
                rcExit = RTEXITCODE_SUCCESS;
            }
            else if (fCompleted)
            {
                ProcessStatus_T procStatus;
                CHECK_ERROR_BREAK(pProcess, COMGETTER(Status)(&procStatus));
                if (   procStatus == ProcessStatus_TerminatedNormally
                    || procStatus == ProcessStatus_TerminatedAbnormally
                    || procStatus == ProcessStatus_TerminatedSignal)
                {
                    LONG lExitCode;
                    CHECK_ERROR_BREAK(pProcess, COMGETTER(ExitCode)(&lExitCode));
                    if (pCtx->cVerbose)
                        RTPrintf("Exit code=%u (Status=%u [%s])\n",
                                 lExitCode, procStatus, gctlProcessStatusToText(procStatus));

                    rcExit = gctlRunCalculateExitCode(procStatus, lExitCode, true /*fReturnExitCodes*/);
                }
                else if (   procStatus == ProcessStatus_TimedOutKilled
                         || procStatus == ProcessStatus_TimedOutAbnormally)
                {
                    if (pCtx->cVerbose)
                        RTPrintf("Process timed out (guest side) and %s\n",
                                 procStatus == ProcessStatus_TimedOutAbnormally
                                 ? "failed to terminate so far" : "was terminated");
                    rcExit = EXITCODEEXEC_TIMEOUT;
                }
                else
                {
                    if (pCtx->cVerbose)
                        RTPrintf("Process now is in status [%s] (unexpected)\n", gctlProcessStatusToText(procStatus));
                    rcExit = RTEXITCODE_FAILURE;
                }
            }
            else if (RT_FAILURE_NP(vrc))
            {
                if (pCtx->cVerbose)
                    RTPrintf("Process monitor loop quit with vrc=%Rrc\n", vrc);
                rcExit = RTEXITCODE_FAILURE;
            }
            else
            {
                if (pCtx->cVerbose)
                    RTPrintf("Process monitor loop timed out\n");
                rcExit = EXITCODEEXEC_TIMEOUT;
            }

        } while (0);
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    /*
     * Decide what to do with the guest session.
     *
     * If it's the 'start' command where detach the guest process after
     * starting, don't close the guest session it is part of, except on
     * failure or ctrl-c.
     *
     * For the 'run' command the guest process quits with us.
     */
    if (!fRunCmd && SUCCEEDED(rc) && !g_fGuestCtrlCanceled)
        pCtx->fDetachGuestSession = true;

    /* Make sure we return failure on failure. */
    if (FAILED(rc) && rcExit == RTEXITCODE_SUCCESS)
        rcExit = RTEXITCODE_FAILURE;
    return rcExit;
}


static DECLCALLBACK(RTEXITCODE) gctlHandleRun(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    return gctlHandleRunCommon(pCtx, argc, argv, true /*fRunCmd*/, USAGE_GSTCTRL_RUN);
}


static DECLCALLBACK(RTEXITCODE) gctlHandleStart(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    return gctlHandleRunCommon(pCtx, argc, argv, false /*fRunCmd*/, USAGE_GSTCTRL_START);
}


static RTEXITCODE gctlHandleCopy(PGCTLCMDCTX pCtx, int argc, char **argv, bool fHostToGuest)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    /** @todo r=bird: This command isn't very unix friendly in general. mkdir
     * is much better (partly because it is much simpler of course).  The main
     * arguments against this is that (1) all but two options conflicts with
     * what 'man cp' tells me on a GNU/Linux system, (2) wildchar matching is
     * done windows CMD style (though not in a 100% compatible way), and (3)
     * that only one source is allowed - efficiently sabotaging default
     * wildcard expansion by a unix shell.  The best solution here would be
     * two different variant, one windowsy (xcopy) and one unixy (gnu cp). */

    /*
     * IGuest::CopyToGuest is kept as simple as possible to let the developer choose
     * what and how to implement the file enumeration/recursive lookup, like VBoxManage
     * does in here.
     */
    enum GETOPTDEF_COPY
    {
        GETOPTDEF_COPY_FOLLOW = 1000,
        GETOPTDEF_COPY_TARGETDIR
    };
    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--follow",              GETOPTDEF_COPY_FOLLOW,           RTGETOPT_REQ_NOTHING },
        { "--recursive",           'R',                             RTGETOPT_REQ_NOTHING },
        { "--target-directory",    GETOPTDEF_COPY_TARGETDIR,        RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    const char *pszDst = NULL;
    bool fFollow = false;
    bool fRecursive = false;
    uint32_t uUsage = fHostToGuest ? USAGE_GSTCTRL_COPYTO : USAGE_GSTCTRL_COPYFROM;

    SourceVec vecSources;

    int vrc = VINF_SUCCESS;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case GETOPTDEF_COPY_FOLLOW:
                fFollow = true;
                break;

            case 'R': /* Recursive processing */
                fRecursive = true;
                break;

            case GETOPTDEF_COPY_TARGETDIR:
                pszDst = ValueUnion.psz;
                break;

            case VINF_GETOPT_NOT_OPTION:
                /* Last argument and no destination specified with --target-directory yet?
                   Then use the current (= last) argument as destination. */
                if (   GetState.argc == GetState.iNext
                    && pszDst == NULL)
                    pszDst = ValueUnion.psz;
                else
                {
                    try
                    {
                        vecSources.push_back(SourceFileEntry(ValueUnion.psz));
                    }
                    catch (std::bad_alloc &)
                    {
                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory");
                    }
                }
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, uUsage, ch, &ValueUnion);
        }
    }

    if (!vecSources.size())
        return errorSyntaxEx(USAGE_GUESTCONTROL, uUsage, "No sources specified!");

    if (pszDst == NULL)
        return errorSyntaxEx(USAGE_GUESTCONTROL, uUsage, "No destination specified!");

    char szAbsDst[RTPATH_MAX];
    if (!fHostToGuest)
    {
        vrc = RTPathAbs(pszDst, szAbsDst, sizeof(szAbsDst));
        if (RT_SUCCESS(vrc))
            pszDst = szAbsDst;
        else
            return RTMsgErrorExitFailure("RTPathAbs failed on '%s': %Rrc", pszDst, vrc);
    }

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /*
     * Done parsing arguments, do some more preparations.
     */
    if (pCtx->cVerbose)
    {
        if (fHostToGuest)
            RTPrintf("Copying from host to guest ...\n");
        else
            RTPrintf("Copying from guest to host ...\n");
    }

    HRESULT           rc = S_OK;
    ComPtr<IProgress> pProgress;

/** @todo r=bird: This codes does nothing to handle the case where there are
 * multiple sources.  You need to do serveral thing before thats handled
 * correctly.  For starters the progress object handling needs to be moved
 * inside the loop.  Next you need to check what the destination is, because you
 * can only copy multiple source files/directories to another directory.  You
 * actually need to check whether the target exists and is a directory
 * regardless of you have 1 or 10 source files/dirs.
 *
 * Btw. the original approach to error handling here was APPALING.  If some file
 * couldn't be stat'ed or if it was a file/directory, you only spat out messages
 * in verbose mode and never set the status code.
 *
 * The handling of the wildcard filtering expressions in sources was also just
 * skipped.   I've corrected this, but you still need to make up your mind wrt
 * wildcards or not.
 */
    if (vecSources.size() != 1)
        return RTMsgErrorExitFailure("Only one source file or directory at the moment.");
    for (size_t s = 0; s < vecSources.size(); s++)
    {
        Utf8Str const &strSrc    = vecSources[s].GetSource();
        Utf8Str const &strFilter = vecSources[s].GetFilter();

        RT_NOREF(strFilter);
        if (strFilter.isNotEmpty())
            rcExit = RTMsgErrorExitFailure("Skipping '%s/%s' because wildcard expansion isn't implemented yet\n");
        else if (fHostToGuest)
        {
            /*
             * Source is host, destiation guest.
             */
            char szAbsSrc[RTPATH_MAX];
            vrc = RTPathAbs(strSrc.c_str(), szAbsSrc, sizeof(szAbsSrc));
            if (RT_SUCCESS(vrc))
            {
                RTFSOBJINFO ObjInfo;
                vrc = RTPathQueryInfo(szAbsSrc, &ObjInfo, RTFSOBJATTRADD_NOTHING);
                if (RT_SUCCESS(vrc))
                {
                    if (RTFS_IS_FILE(ObjInfo.Attr.fMode))
                    {
                        if (pCtx->cVerbose)
                            RTPrintf("File '%s' -> '%s'\n", szAbsSrc, pszDst);

                        SafeArray<FileCopyFlag_T> copyFlags;
                        rc = pCtx->pGuestSession->FileCopyToGuest(Bstr(szAbsSrc).raw(), Bstr(pszDst).raw(),
                                                                  ComSafeArrayAsInParam(copyFlags), pProgress.asOutParam());
                    }
                    else if (RTFS_IS_DIRECTORY(ObjInfo.Attr.fMode))
                    {
                        if (pCtx->cVerbose)
                            RTPrintf("Directory '%s' -> '%s'\n", szAbsSrc, pszDst);

                        SafeArray<DirectoryCopyFlag_T> copyFlags;
                        copyFlags.push_back(DirectoryCopyFlag_CopyIntoExisting);
                        rc = pCtx->pGuestSession->DirectoryCopyToGuest(Bstr(szAbsSrc).raw(), Bstr(pszDst).raw(),
                                                                       ComSafeArrayAsInParam(copyFlags), pProgress.asOutParam());
                    }
                    else
                        rcExit = RTMsgErrorExitFailure("Not a file or directory: %s\n", szAbsSrc);
                }
                else
                    rcExit = RTMsgErrorExitFailure("RTPathQueryInfo failed on '%s': %Rrc", szAbsSrc, vrc);
            }
            else
                rcExit = RTMsgErrorExitFailure("RTPathAbs failed on '%s': %Rrc", strSrc.c_str());
        }
        else
        {
            /*
             * Source guest, destination host.
             */
            /* We need to query the source type on the guest first in order to know which copy flavor we need. */
            ComPtr<IGuestFsObjInfo> pFsObjInfo;
            rc = pCtx->pGuestSession->FsObjQueryInfo(Bstr(strSrc).raw(), TRUE  /* fFollowSymlinks */, pFsObjInfo.asOutParam());
            if (SUCCEEDED(rc))
            {
                FsObjType_T enmObjType;
                CHECK_ERROR(pFsObjInfo,COMGETTER(Type)(&enmObjType));
                if (SUCCEEDED(rc))
                {
                    /* Take action according to source file. */
                    if (enmObjType == FsObjType_Directory)
                    {
                        if (pCtx->cVerbose)
                            RTPrintf("Directory '%s' -> '%s'\n", strSrc.c_str(), pszDst);

                        SafeArray<DirectoryCopyFlag_T> aCopyFlags;
                        aCopyFlags.push_back(DirectoryCopyFlag_CopyIntoExisting);
                        rc = pCtx->pGuestSession->DirectoryCopyFromGuest(Bstr(strSrc).raw(), Bstr(pszDst).raw(),
                                                                         ComSafeArrayAsInParam(aCopyFlags), pProgress.asOutParam());
                    }
                    else if (enmObjType == FsObjType_File)
                    {
                        if (pCtx->cVerbose)
                            RTPrintf("File '%s' -> '%s'\n", strSrc.c_str(), pszDst);

                        SafeArray<FileCopyFlag_T> aCopyFlags;
                        rc = pCtx->pGuestSession->FileCopyFromGuest(Bstr(strSrc).raw(), Bstr(pszDst).raw(),
                                                                    ComSafeArrayAsInParam(aCopyFlags), pProgress.asOutParam());
                    }
                    else
                        rcExit = RTMsgErrorExitFailure("Not a file or directory: %s\n", strSrc.c_str());
                }
                else
                    rcExit = RTEXITCODE_FAILURE;
            }
            else
                rcExit = RTMsgErrorExitFailure("FsObjQueryInfo failed on '%s': %Rrc", strSrc.c_str());
        }
    }

    if (FAILED(rc))
    {
        vrc = gctlPrintError(pCtx->pGuestSession, COM_IIDOF(IGuestSession));
    }
    else if (pProgress.isNotNull())
    {
        if (pCtx->cVerbose)
            rc = showProgress(pProgress);
        else
            rc = pProgress->WaitForCompletion(-1 /* No timeout */);
        if (SUCCEEDED(rc))
            CHECK_PROGRESS_ERROR(pProgress, ("File copy failed"));
        vrc = gctlPrintProgressError(pProgress);
    }
    if (RT_FAILURE(vrc))
        rcExit = RTEXITCODE_FAILURE;

    return rcExit;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleCopyFrom(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    return gctlHandleCopy(pCtx, argc, argv, false /* Guest to host */);
}

static DECLCALLBACK(RTEXITCODE) gctlHandleCopyTo(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    return gctlHandleCopy(pCtx, argc, argv, true /* Host to guest */);
}

static DECLCALLBACK(RTEXITCODE) gctrlHandleMkDir(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--mode",                'm',                             RTGETOPT_REQ_UINT32  },
        { "--parents",             'P',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    SafeArray<DirectoryCreateFlag_T> aDirCreateFlags;
    uint32_t    fDirMode     = 0; /* Default mode. */
    uint32_t    cDirsCreated = 0;
    RTEXITCODE  rcExit       = RTEXITCODE_SUCCESS;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'm': /* Mode */
                fDirMode = ValueUnion.u32;
                break;

            case 'P': /* Create parents */
                aDirCreateFlags.push_back(DirectoryCreateFlag_Parents);
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (cDirsCreated == 0)
                {
                    /*
                     * First non-option - no more options now.
                     */
                    rcExit = gctlCtxPostOptionParsingInit(pCtx);
                    if (rcExit != RTEXITCODE_SUCCESS)
                        return rcExit;
                    if (pCtx->cVerbose)
                        RTPrintf("Creating %RU32 directories...\n", argc - GetState.iNext + 1);
                }
                if (g_fGuestCtrlCanceled)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "mkdir was interrupted by Ctrl-C (%u left)\n",
                                          argc - GetState.iNext + 1);

                /*
                 * Create the specified directory.
                 *
                 * On failure we'll change the exit status to failure and
                 * continue with the next directory that needs creating. We do
                 * this because we only create new things, and because this is
                 * how /bin/mkdir works on unix.
                 */
                cDirsCreated++;
                if (pCtx->cVerbose)
                    RTPrintf("Creating directory \"%s\" ...\n", ValueUnion.psz);
                try
                {
                    HRESULT rc;
                    CHECK_ERROR(pCtx->pGuestSession, DirectoryCreate(Bstr(ValueUnion.psz).raw(),
                                                                     fDirMode, ComSafeArrayAsInParam(aDirCreateFlags)));
                    if (FAILED(rc))
                        rcExit = RTEXITCODE_FAILURE;
                }
                catch (std::bad_alloc &)
                {
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory\n");
                }
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKDIR, ch, &ValueUnion);
        }
    }

    if (!cDirsCreated)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKDIR, "No directory to create specified!");
    return rcExit;
}


static DECLCALLBACK(RTEXITCODE) gctlHandleRmDir(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--recursive",           'R',                             RTGETOPT_REQ_NOTHING },
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    bool        fRecursive  = false;
    uint32_t    cDirRemoved = 0;
    RTEXITCODE  rcExit      = RTEXITCODE_SUCCESS;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'R':
                fRecursive = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (cDirRemoved == 0)
                {
                    /*
                     * First non-option - no more options now.
                     */
                    rcExit = gctlCtxPostOptionParsingInit(pCtx);
                    if (rcExit != RTEXITCODE_SUCCESS)
                        return rcExit;
                    if (pCtx->cVerbose)
                        RTPrintf("Removing %RU32 directorie%s(s)...\n", argc - GetState.iNext + 1, fRecursive ? "tree" : "");
                }
                if (g_fGuestCtrlCanceled)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "rmdir was interrupted by Ctrl-C (%u left)\n",
                                          argc - GetState.iNext + 1);

                cDirRemoved++;
                HRESULT rc;
                if (!fRecursive)
                {
                    /*
                     * Remove exactly one directory.
                     */
                    if (pCtx->cVerbose)
                        RTPrintf("Removing directory \"%s\" ...\n", ValueUnion.psz);
                    try
                    {
                        CHECK_ERROR(pCtx->pGuestSession, DirectoryRemove(Bstr(ValueUnion.psz).raw()));
                    }
                    catch (std::bad_alloc &)
                    {
                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory\n");
                    }
                }
                else
                {
                    /*
                     * Remove the directory and anything under it, that means files
                     * and everything.  This is in the tradition of the Windows NT
                     * CMD.EXE "rmdir /s" operation, a tradition which jpsoft's TCC
                     * strongly warns against (and half-ways questions the sense of).
                     */
                    if (pCtx->cVerbose)
                        RTPrintf("Recursively removing directory \"%s\" ...\n", ValueUnion.psz);
                    try
                    {
                        /** @todo Make flags configurable. */
                        com::SafeArray<DirectoryRemoveRecFlag_T> aRemRecFlags;
                        aRemRecFlags.push_back(DirectoryRemoveRecFlag_ContentAndDir);

                        ComPtr<IProgress> ptrProgress;
                        CHECK_ERROR(pCtx->pGuestSession, DirectoryRemoveRecursive(Bstr(ValueUnion.psz).raw(),
                                                                                  ComSafeArrayAsInParam(aRemRecFlags),
                                                                                  ptrProgress.asOutParam()));
                        if (SUCCEEDED(rc))
                        {
                            if (pCtx->cVerbose)
                                rc = showProgress(ptrProgress);
                            else
                                rc = ptrProgress->WaitForCompletion(-1 /* indefinitely */);
                            if (SUCCEEDED(rc))
                                CHECK_PROGRESS_ERROR(ptrProgress, ("Directory deletion failed"));
                            ptrProgress.setNull();
                        }
                    }
                    catch (std::bad_alloc &)
                    {
                        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory during recursive rmdir\n");
                    }
                }

                /*
                 * This command returns immediately on failure since it's destructive in nature.
                 */
                if (FAILED(rc))
                    return RTEXITCODE_FAILURE;
                break;
            }

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RMDIR, ch, &ValueUnion);
        }
    }

    if (!cDirRemoved)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RMDIR, "No directory to remove specified!");
    return rcExit;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleRm(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--force",                         'f',                                       RTGETOPT_REQ_NOTHING, },
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    uint32_t    cFilesDeleted   = 0;
    RTEXITCODE  rcExit          = RTEXITCODE_SUCCESS;
    bool        fForce         = true;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case VINF_GETOPT_NOT_OPTION:
                if (cFilesDeleted == 0)
                {
                    /*
                     * First non-option - no more options now.
                     */
                    rcExit = gctlCtxPostOptionParsingInit(pCtx);
                    if (rcExit != RTEXITCODE_SUCCESS)
                        return rcExit;
                    if (pCtx->cVerbose)
                        RTPrintf("Removing %RU32 file(s)...\n", argc - GetState.iNext + 1);
                }
                if (g_fGuestCtrlCanceled)
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "rm was interrupted by Ctrl-C (%u left)\n",
                                          argc - GetState.iNext + 1);

                /*
                 * Remove the specified file.
                 *
                 * On failure we will by default stop, however, the force option will
                 * by unix traditions force us to ignore errors and continue.
                 */
                cFilesDeleted++;
                if (pCtx->cVerbose)
                    RTPrintf("Removing file \"%s\" ...\n", ValueUnion.psz);
                try
                {
                    /** @todo How does IGuestSession::FsObjRemove work with read-only files? Do we
                     *        need to do some chmod or whatever to better emulate the --force flag? */
                    HRESULT rc;
                    CHECK_ERROR(pCtx->pGuestSession, FsObjRemove(Bstr(ValueUnion.psz).raw()));
                    if (FAILED(rc) && !fForce)
                        return RTEXITCODE_FAILURE;
                }
                catch (std::bad_alloc &)
                {
                    return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory\n");
                }
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RM, ch, &ValueUnion);
        }
    }

    if (!cFilesDeleted && !fForce)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_RM, "No file to remove specified!");
    return rcExit;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleMv(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
/** @todo Missing --force/-f flag.   */
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int vrc = VINF_SUCCESS;

    bool fDryrun = false;
    std::vector< Utf8Str > vecSources;
    const char *pszDst = NULL;
    com::SafeArray<FsObjRenameFlag_T> aRenameFlags;

    try
    {
        /** @todo Make flags configurable. */
        aRenameFlags.push_back(FsObjRenameFlag_NoReplace);

        while (   (ch = RTGetOpt(&GetState, &ValueUnion))
               && RT_SUCCESS(vrc))
        {
            /* For options that require an argument, ValueUnion has received the value. */
            switch (ch)
            {
                GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

                /** @todo Implement a --dryrun command. */
                /** @todo Implement rename flags. */

                case VINF_GETOPT_NOT_OPTION:
                    vecSources.push_back(Utf8Str(ValueUnion.psz));
                    pszDst = ValueUnion.psz;
                    break;

                default:
                    return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MV, ch, &ValueUnion);
            }
        }
    }
    catch (std::bad_alloc &)
    {
        vrc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(vrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize, rc=%Rrc\n", vrc);

    size_t cSources = vecSources.size();
    if (!cSources)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MV,
                             "No source(s) to move specified!");
    if (cSources < 2)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MV,
                             "No destination specified!");

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /* Delete last element, which now is the destination. */
    vecSources.pop_back();
    cSources = vecSources.size();

    HRESULT rc = S_OK;

    if (cSources > 1)
    {
        BOOL fExists = FALSE;
        rc = pCtx->pGuestSession->DirectoryExists(Bstr(pszDst).raw(), FALSE /*followSymlinks*/, &fExists);
        if (FAILED(rc) || !fExists)
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Destination must be a directory when specifying multiple sources\n");
    }

    /*
     * Rename (move) the entries.
     */
    if (pCtx->cVerbose)
        RTPrintf("Renaming %RU32 %s ...\n", cSources, cSources > 1 ? "entries" : "entry");

    std::vector< Utf8Str >::iterator it = vecSources.begin();
    while (   it != vecSources.end()
           && !g_fGuestCtrlCanceled)
    {
        Utf8Str strCurSource = (*it);

        ComPtr<IGuestFsObjInfo> pFsObjInfo;
        FsObjType_T enmObjType = FsObjType_Unknown; /* Shut up MSC */
        rc = pCtx->pGuestSession->FsObjQueryInfo(Bstr(strCurSource).raw(), FALSE /*followSymlinks*/, pFsObjInfo.asOutParam());
        if (SUCCEEDED(rc))
            rc = pFsObjInfo->COMGETTER(Type)(&enmObjType);
        if (FAILED(rc))
        {
            if (pCtx->cVerbose)
                RTPrintf("Warning: Cannot stat for element \"%s\": No such file or directory\n", strCurSource.c_str());
            ++it;
            continue; /* Skip. */
        }

        if (pCtx->cVerbose)
            RTPrintf("Renaming %s \"%s\" to \"%s\" ...\n",
                     enmObjType == FsObjType_Directory ? "directory" : "file",
                     strCurSource.c_str(), pszDst);

        if (!fDryrun)
        {
            if (enmObjType == FsObjType_Directory)
            {
                CHECK_ERROR_BREAK(pCtx->pGuestSession, FsObjRename(Bstr(strCurSource).raw(),
                                                                   Bstr(pszDst).raw(),
                                                                   ComSafeArrayAsInParam(aRenameFlags)));

                /* Break here, since it makes no sense to rename mroe than one source to
                 * the same directory. */
/** @todo r=bird: You are being kind of windowsy (or just DOSish) about the 'sense' part here,
 * while being totaly buggy about the behavior. 'VBoxManage guestcontrol ren dir1 dir2 dstdir' will
 * stop after 'dir1' and SILENTLY ignore dir2.  If you tried this on Windows, you'd see an error
 * being displayed.  If you 'man mv' on a nearby unixy system, you'd see that they've made perfect
 * sense out of any situation with more than one source. */
                it = vecSources.end();
                break;
            }
            else
                CHECK_ERROR_BREAK(pCtx->pGuestSession, FsObjRename(Bstr(strCurSource).raw(),
                                                                   Bstr(pszDst).raw(),
                                                                   ComSafeArrayAsInParam(aRenameFlags)));
        }

        ++it;
    }

    if (   (it != vecSources.end())
        && pCtx->cVerbose)
    {
        RTPrintf("Warning: Not all sources were renamed\n");
    }

    return FAILED(rc) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleMkTemp(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--mode",                'm',                             RTGETOPT_REQ_UINT32  },
        { "--directory",           'D',                             RTGETOPT_REQ_NOTHING },
        { "--secure",              's',                             RTGETOPT_REQ_NOTHING },
        { "--tmpdir",              't',                             RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str strTemplate;
    uint32_t fMode = 0; /* Default mode. */
    bool fDirectory = false;
    bool fSecure = false;
    Utf8Str strTempDir;

    DESTDIRMAP mapDirs;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'm': /* Mode */
                fMode = ValueUnion.u32;
                break;

            case 'D': /* Create directory */
                fDirectory = true;
                break;

            case 's': /* Secure */
                fSecure = true;
                break;

            case 't': /* Temp directory */
                strTempDir = ValueUnion.psz;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (strTemplate.isEmpty())
                    strTemplate = ValueUnion.psz;
                else
                    return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKTEMP,
                                         "More than one template specified!\n");
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKTEMP, ch, &ValueUnion);
        }
    }

    if (strTemplate.isEmpty())
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKTEMP,
                             "No template specified!");

    if (!fDirectory)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_MKTEMP,
                             "Creating temporary files is currently not supported!");

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /*
     * Create the directories.
     */
    if (pCtx->cVerbose)
    {
        if (fDirectory && !strTempDir.isEmpty())
            RTPrintf("Creating temporary directory from template '%s' in directory '%s' ...\n",
                     strTemplate.c_str(), strTempDir.c_str());
        else if (fDirectory)
            RTPrintf("Creating temporary directory from template '%s' in default temporary directory ...\n",
                     strTemplate.c_str());
        else if (!fDirectory && !strTempDir.isEmpty())
            RTPrintf("Creating temporary file from template '%s' in directory '%s' ...\n",
                     strTemplate.c_str(), strTempDir.c_str());
        else if (!fDirectory)
            RTPrintf("Creating temporary file from template '%s' in default temporary directory ...\n",
                     strTemplate.c_str());
    }

    HRESULT rc = S_OK;
    if (fDirectory)
    {
        Bstr bstrDirectory;
        CHECK_ERROR(pCtx->pGuestSession, DirectoryCreateTemp(Bstr(strTemplate).raw(),
                                                             fMode, Bstr(strTempDir).raw(),
                                                             fSecure,
                                                             bstrDirectory.asOutParam()));
        if (SUCCEEDED(rc))
            RTPrintf("Directory name: %ls\n", bstrDirectory.raw());
    }
    else
    {
        // else - temporary file not yet implemented
        /** @todo implement temporary file creation (we fend it off above, no
         *        worries). */
        rc = E_FAIL;
    }

    return FAILED(rc) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleStat(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--dereference",         'L',                             RTGETOPT_REQ_NOTHING },
        { "--file-system",         'f',                             RTGETOPT_REQ_NOTHING },
        { "--format",              'c',                             RTGETOPT_REQ_STRING },
        { "--terse",               't',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    while (  (ch = RTGetOpt(&GetState, &ValueUnion)) != 0
           && ch != VINF_GETOPT_NOT_OPTION)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'L': /* Dereference */
            case 'f': /* File-system */
            case 'c': /* Format */
            case 't': /* Terse */
                return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_STAT,
                                     "Command \"%s\" not implemented yet!", ValueUnion.psz);

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_STAT, ch, &ValueUnion);
        }
    }

    if (ch != VINF_GETOPT_NOT_OPTION)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_STAT, "Nothing to stat!");

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;


    /*
     * Do the file stat'ing.
     */
    while (ch == VINF_GETOPT_NOT_OPTION)
    {
        if (pCtx->cVerbose)
            RTPrintf("Checking for element \"%s\" ...\n", ValueUnion.psz);

        ComPtr<IGuestFsObjInfo> pFsObjInfo;
        HRESULT hrc = pCtx->pGuestSession->FsObjQueryInfo(Bstr(ValueUnion.psz).raw(), FALSE /*followSymlinks*/,
                                                          pFsObjInfo.asOutParam());
        if (FAILED(hrc))
        {
            /* If there's at least one element which does not exist on the guest,
             * drop out with exitcode 1. */
            if (pCtx->cVerbose)
                RTPrintf("Cannot stat for element \"%s\": No such element\n", ValueUnion.psz);
            rcExit = RTEXITCODE_FAILURE;
        }
        else
        {
            FsObjType_T objType;
            pFsObjInfo->COMGETTER(Type)(&objType); /** @todo What about error checking? */
            switch (objType)
            {
                case FsObjType_File:
                    RTPrintf("Element \"%s\" found: Is a file\n", ValueUnion.psz);
                    break;

                case FsObjType_Directory:
                    RTPrintf("Element \"%s\" found: Is a directory\n", ValueUnion.psz);
                    break;

                case FsObjType_Symlink:
                    RTPrintf("Element \"%s\" found: Is a symlink\n", ValueUnion.psz);
                    break;

                default:
                    RTPrintf("Element \"%s\" found, type unknown (%ld)\n", ValueUnion.psz, objType);
                    break;
            }

            /** @todo Show more information about this element. */
        }

        /* Next file. */
        ch = RTGetOpt(&GetState, &ValueUnion);
    }

    return rcExit;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleUpdateAdditions(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    Utf8Str strSource;
    com::SafeArray<IN_BSTR> aArgs;
    bool fWaitStartOnly = false;

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--source",              's',         RTGETOPT_REQ_STRING  },
        { "--wait-start",          'w',         RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    int vrc = VINF_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 's':
                strSource = ValueUnion.psz;
                break;

            case 'w':
                fWaitStartOnly = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (aArgs.size() == 0 && strSource.isEmpty())
                    strSource = ValueUnion.psz;
                else
                    aArgs.push_back(Bstr(ValueUnion.psz).raw());
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_UPDATEGA, ch, &ValueUnion);
        }
    }

    if (pCtx->cVerbose)
        RTPrintf("Updating Guest Additions ...\n");

    HRESULT rc = S_OK;
    while (strSource.isEmpty())
    {
        ComPtr<ISystemProperties> pProperties;
        CHECK_ERROR_BREAK(pCtx->pArg->virtualBox, COMGETTER(SystemProperties)(pProperties.asOutParam()));
        Bstr strISO;
        CHECK_ERROR_BREAK(pProperties, COMGETTER(DefaultAdditionsISO)(strISO.asOutParam()));
        strSource = strISO;
        break;
    }

    /* Determine source if not set yet. */
    if (strSource.isEmpty())
    {
        RTMsgError("No Guest Additions source found or specified, aborting\n");
        vrc = VERR_FILE_NOT_FOUND;
    }
    else if (!RTFileExists(strSource.c_str()))
    {
        RTMsgError("Source \"%s\" does not exist!\n", strSource.c_str());
        vrc = VERR_FILE_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
    {
        if (pCtx->cVerbose)
            RTPrintf("Using source: %s\n", strSource.c_str());


        RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
        if (rcExit != RTEXITCODE_SUCCESS)
            return rcExit;


        com::SafeArray<AdditionsUpdateFlag_T> aUpdateFlags;
        if (fWaitStartOnly)
        {
            aUpdateFlags.push_back(AdditionsUpdateFlag_WaitForUpdateStartOnly);
            if (pCtx->cVerbose)
                RTPrintf("Preparing and waiting for Guest Additions installer to start ...\n");
        }

        ComPtr<IProgress> pProgress;
        CHECK_ERROR(pCtx->pGuest, UpdateGuestAdditions(Bstr(strSource).raw(),
                                                       ComSafeArrayAsInParam(aArgs),
                                                       /* Wait for whole update process to complete. */
                                                       ComSafeArrayAsInParam(aUpdateFlags),
                                                       pProgress.asOutParam()));
        if (FAILED(rc))
            vrc = gctlPrintError(pCtx->pGuest, COM_IIDOF(IGuest));
        else
        {
            if (pCtx->cVerbose)
                rc = showProgress(pProgress);
            else
                rc = pProgress->WaitForCompletion(-1 /* No timeout */);

            if (SUCCEEDED(rc))
                CHECK_PROGRESS_ERROR(pProgress, ("Guest additions update failed"));
            vrc = gctlPrintProgressError(pProgress);
            if (   RT_SUCCESS(vrc)
                && pCtx->cVerbose)
            {
                RTPrintf("Guest Additions update successful\n");
            }
        }
    }

    return RT_SUCCESS(vrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleList(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    bool fSeenListArg   = false;
    bool fListAll       = false;
    bool fListSessions  = false;
    bool fListProcesses = false;
    bool fListFiles     = false;

    int vrc = VINF_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case VINF_GETOPT_NOT_OPTION:
                if (   !RTStrICmp(ValueUnion.psz, "sessions")
                    || !RTStrICmp(ValueUnion.psz, "sess"))
                    fListSessions = true;
                else if (   !RTStrICmp(ValueUnion.psz, "processes")
                         || !RTStrICmp(ValueUnion.psz, "procs"))
                    fListSessions = fListProcesses = true; /* Showing processes implies showing sessions. */
                else if (!RTStrICmp(ValueUnion.psz, "files"))
                    fListSessions = fListFiles = true;     /* Showing files implies showing sessions. */
                else if (!RTStrICmp(ValueUnion.psz, "all"))
                    fListAll = true;
                else
                    return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_LIST,
                                         "Unknown list: '%s'", ValueUnion.psz);
                fSeenListArg = true;
                break;

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_UPDATEGA, ch, &ValueUnion);
        }
    }

    if (!fSeenListArg)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_LIST, "Missing list name");
    Assert(fListAll || fListSessions);

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;


    /** @todo Do we need a machine-readable output here as well? */

    HRESULT rc;
    size_t cTotalProcs = 0;
    size_t cTotalFiles = 0;

    SafeIfaceArray <IGuestSession> collSessions;
    CHECK_ERROR(pCtx->pGuest, COMGETTER(Sessions)(ComSafeArrayAsOutParam(collSessions)));
    if (SUCCEEDED(rc))
    {
        size_t const cSessions = collSessions.size();
        if (cSessions)
        {
            RTPrintf("Active guest sessions:\n");

            /** @todo Make this output a bit prettier. No time now. */

            for (size_t i = 0; i < cSessions; i++)
            {
                ComPtr<IGuestSession> pCurSession = collSessions[i];
                if (!pCurSession.isNull())
                {
                    do
                    {
                        ULONG uID;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(Id)(&uID));
                        Bstr strName;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(Name)(strName.asOutParam()));
                        Bstr strUser;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(User)(strUser.asOutParam()));
                        GuestSessionStatus_T sessionStatus;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(Status)(&sessionStatus));
                        RTPrintf("\n\tSession #%-3zu ID=%-3RU32 User=%-16ls Status=[%s] Name=%ls",
                                 i, uID, strUser.raw(), gctlGuestSessionStatusToText(sessionStatus), strName.raw());
                    } while (0);

                    if (   fListAll
                        || fListProcesses)
                    {
                        SafeIfaceArray <IGuestProcess> collProcesses;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(Processes)(ComSafeArrayAsOutParam(collProcesses)));
                        for (size_t a = 0; a < collProcesses.size(); a++)
                        {
                            ComPtr<IGuestProcess> pCurProcess = collProcesses[a];
                            if (!pCurProcess.isNull())
                            {
                                do
                                {
                                    ULONG uPID;
                                    CHECK_ERROR_BREAK(pCurProcess, COMGETTER(PID)(&uPID));
                                    Bstr strExecPath;
                                    CHECK_ERROR_BREAK(pCurProcess, COMGETTER(ExecutablePath)(strExecPath.asOutParam()));
                                    ProcessStatus_T procStatus;
                                    CHECK_ERROR_BREAK(pCurProcess, COMGETTER(Status)(&procStatus));

                                    RTPrintf("\n\t\tProcess #%-03zu PID=%-6RU32 Status=[%s] Command=%ls",
                                             a, uPID, gctlProcessStatusToText(procStatus), strExecPath.raw());
                                } while (0);
                            }
                        }

                        cTotalProcs += collProcesses.size();
                    }

                    if (   fListAll
                        || fListFiles)
                    {
                        SafeIfaceArray <IGuestFile> collFiles;
                        CHECK_ERROR_BREAK(pCurSession, COMGETTER(Files)(ComSafeArrayAsOutParam(collFiles)));
                        for (size_t a = 0; a < collFiles.size(); a++)
                        {
                            ComPtr<IGuestFile> pCurFile = collFiles[a];
                            if (!pCurFile.isNull())
                            {
                                do
                                {
                                    ULONG idFile;
                                    CHECK_ERROR_BREAK(pCurFile, COMGETTER(Id)(&idFile));
                                    Bstr strName;
                                    CHECK_ERROR_BREAK(pCurFile, COMGETTER(FileName)(strName.asOutParam()));
                                    FileStatus_T fileStatus;
                                    CHECK_ERROR_BREAK(pCurFile, COMGETTER(Status)(&fileStatus));

                                    RTPrintf("\n\t\tFile #%-03zu ID=%-6RU32 Status=[%s] Name=%ls",
                                             a, idFile, gctlFileStatusToText(fileStatus), strName.raw());
                                } while (0);
                            }
                        }

                        cTotalFiles += collFiles.size();
                    }
                }
            }

            RTPrintf("\n\nTotal guest sessions: %zu\n", collSessions.size());
            if (fListAll || fListProcesses)
                RTPrintf("Total guest processes: %zu\n", cTotalProcs);
            if (fListAll || fListFiles)
                RTPrintf("Total guest files: %zu\n", cTotalFiles);
        }
        else
            RTPrintf("No active guest sessions found\n");
    }

    if (FAILED(rc)) /** @todo yeah, right... Only the last error? */
        rcExit = RTEXITCODE_FAILURE;

    return rcExit;
}

static DECLCALLBACK(RTEXITCODE) gctlHandleCloseProcess(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--session-id",          'i',                             RTGETOPT_REQ_UINT32  },
        { "--session-name",        'n',                             RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    std::vector < uint32_t > vecPID;
    ULONG idSession = UINT32_MAX;
    Utf8Str strSessionName;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'n': /* Session name (or pattern) */
                strSessionName = ValueUnion.psz;
                break;

            case 'i': /* Session ID */
                idSession = ValueUnion.u32;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                /* Treat every else specified as a PID to kill. */
                uint32_t uPid;
                int rc = RTStrToUInt32Ex(ValueUnion.psz, NULL, 0, &uPid);
                if (   RT_SUCCESS(rc)
                    && rc != VWRN_TRAILING_CHARS
                    && rc != VWRN_NUMBER_TOO_BIG
                    && rc != VWRN_NEGATIVE_UNSIGNED)
                {
                    if (uPid != 0)
                    {
                        try
                        {
                            vecPID.push_back(uPid);
                        }
                        catch (std::bad_alloc &)
                        {
                            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Out of memory");
                        }
                    }
                    else
                        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS, "Invalid PID value: 0");
                }
                else
                    return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS, "Error parsing PID value: %Rrc", rc);
                break;
            }

            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS, ch, &ValueUnion);
        }
    }

    if (vecPID.empty())
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS,
                             "At least one PID must be specified to kill!");

    if (   strSessionName.isEmpty()
        && idSession == UINT32_MAX)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS, "No session ID specified!");

    if (   strSessionName.isNotEmpty()
        && idSession != UINT32_MAX)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSEPROCESS,
                             "Either session ID or name (pattern) must be specified");

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    HRESULT rc = S_OK;

    ComPtr<IGuestSession> pSession;
    ComPtr<IGuestProcess> pProcess;
    do
    {
        uint32_t uProcsTerminated = 0;

        SafeIfaceArray <IGuestSession> collSessions;
        CHECK_ERROR_BREAK(pCtx->pGuest, COMGETTER(Sessions)(ComSafeArrayAsOutParam(collSessions)));
        size_t cSessions = collSessions.size();

        uint32_t cSessionsHandled = 0;
        for (size_t i = 0; i < cSessions; i++)
        {
            pSession = collSessions[i];
            Assert(!pSession.isNull());

            ULONG uID; /* Session ID */
            CHECK_ERROR_BREAK(pSession, COMGETTER(Id)(&uID));
            Bstr strName;
            CHECK_ERROR_BREAK(pSession, COMGETTER(Name)(strName.asOutParam()));
            Utf8Str strNameUtf8(strName); /* Session name */

            bool fSessionFound;
            if (strSessionName.isEmpty()) /* Search by ID. Slow lookup. */
                fSessionFound = uID == idSession;
            else /* ... or by naming pattern. */
                fSessionFound = RTStrSimplePatternMatch(strSessionName.c_str(), strNameUtf8.c_str());
            if (fSessionFound)
            {
                AssertStmt(!pSession.isNull(), break);
                cSessionsHandled++;

                SafeIfaceArray <IGuestProcess> collProcs;
                CHECK_ERROR_BREAK(pSession, COMGETTER(Processes)(ComSafeArrayAsOutParam(collProcs)));

                size_t cProcs = collProcs.size();
                for (size_t p = 0; p < cProcs; p++)
                {
                    pProcess = collProcs[p];
                    Assert(!pProcess.isNull());

                    ULONG uPID; /* Process ID */
                    CHECK_ERROR_BREAK(pProcess, COMGETTER(PID)(&uPID));

                    bool fProcFound = false;
                    for (size_t a = 0; a < vecPID.size(); a++) /* Slow, but works. */
                    {
                        fProcFound = vecPID[a] == uPID;
                        if (fProcFound)
                            break;
                    }

                    if (fProcFound)
                    {
                        if (pCtx->cVerbose)
                            RTPrintf("Terminating process (PID %RU32) (session ID %RU32) ...\n",
                                     uPID, uID);
                        CHECK_ERROR_BREAK(pProcess, Terminate());
                        uProcsTerminated++;
                    }
                    else
                    {
                        if (idSession != UINT32_MAX)
                            RTPrintf("No matching process(es) for session ID %RU32 found\n",
                                     idSession);
                    }

                    pProcess.setNull();
                }

                pSession.setNull();
            }
        }

        if (!cSessionsHandled)
            RTPrintf("No matching session(s) found\n");

        if (uProcsTerminated)
            RTPrintf("%RU32 %s terminated\n",
                     uProcsTerminated, uProcsTerminated == 1 ? "process" : "processes");

    } while (0);

    pProcess.setNull();
    pSession.setNull();

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static DECLCALLBACK(RTEXITCODE) gctlHandleCloseSession(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    enum GETOPTDEF_SESSIONCLOSE
    {
        GETOPTDEF_SESSIONCLOSE_ALL = 2000
    };
    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
        { "--all",                 GETOPTDEF_SESSIONCLOSE_ALL,      RTGETOPT_REQ_NOTHING  },
        { "--session-id",          'i',                             RTGETOPT_REQ_UINT32  },
        { "--session-name",        'n',                             RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    ULONG idSession = UINT32_MAX;
    Utf8Str strSessionName;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case 'n': /* Session name pattern */
                strSessionName = ValueUnion.psz;
                break;

            case 'i': /* Session ID */
                idSession = ValueUnion.u32;
                break;

            case GETOPTDEF_SESSIONCLOSE_ALL:
                strSessionName = "*";
                break;

            case VINF_GETOPT_NOT_OPTION:
                /** @todo Supply a CSV list of IDs or patterns to close?
                 *  break; */
            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSESESSION, ch, &ValueUnion);
        }
    }

    if (   strSessionName.isEmpty()
        && idSession == UINT32_MAX)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSESESSION,
                             "No session ID specified!");

    if (   !strSessionName.isEmpty()
        && idSession != UINT32_MAX)
        return errorSyntaxEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_CLOSESESSION,
                             "Either session ID or name (pattern) must be specified");

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    HRESULT rc = S_OK;

    do
    {
        size_t cSessionsHandled = 0;

        SafeIfaceArray <IGuestSession> collSessions;
        CHECK_ERROR_BREAK(pCtx->pGuest, COMGETTER(Sessions)(ComSafeArrayAsOutParam(collSessions)));
        size_t cSessions = collSessions.size();

        for (size_t i = 0; i < cSessions; i++)
        {
            ComPtr<IGuestSession> pSession = collSessions[i];
            Assert(!pSession.isNull());

            ULONG uID; /* Session ID */
            CHECK_ERROR_BREAK(pSession, COMGETTER(Id)(&uID));
            Bstr strName;
            CHECK_ERROR_BREAK(pSession, COMGETTER(Name)(strName.asOutParam()));
            Utf8Str strNameUtf8(strName); /* Session name */

            bool fSessionFound;
            if (strSessionName.isEmpty()) /* Search by ID. Slow lookup. */
                fSessionFound = uID == idSession;
            else /* ... or by naming pattern. */
                fSessionFound = RTStrSimplePatternMatch(strSessionName.c_str(), strNameUtf8.c_str());
            if (fSessionFound)
            {
                cSessionsHandled++;

                Assert(!pSession.isNull());
                if (pCtx->cVerbose)
                    RTPrintf("Closing guest session ID=#%RU32 \"%s\" ...\n",
                             uID, strNameUtf8.c_str());
                CHECK_ERROR_BREAK(pSession, Close());
                if (pCtx->cVerbose)
                    RTPrintf("Guest session successfully closed\n");

                pSession.setNull();
            }
        }

        if (!cSessionsHandled)
        {
            RTPrintf("No guest session(s) found\n");
            rc = E_ABORT; /* To set exit code accordingly. */
        }

    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static DECLCALLBACK(RTEXITCODE) gctlHandleWatch(PGCTLCMDCTX pCtx, int argc, char **argv)
{
    AssertPtrReturn(pCtx, RTEXITCODE_FAILURE);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        GCTLCMD_COMMON_OPTION_DEFS()
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            GCTLCMD_COMMON_OPTION_CASES(pCtx, ch, &ValueUnion);

            case VINF_GETOPT_NOT_OPTION:
            default:
                return errorGetOptEx(USAGE_GUESTCONTROL, USAGE_GSTCTRL_WATCH, ch, &ValueUnion);
        }
    }

    /** @todo Specify categories to watch for. */
    /** @todo Specify a --timeout for waiting only for a certain amount of time? */

    RTEXITCODE rcExit = gctlCtxPostOptionParsingInit(pCtx);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    HRESULT rc;

    try
    {
        ComObjPtr<GuestEventListenerImpl> pGuestListener;
        do
        {
            /* Listener creation. */
            pGuestListener.createObject();
            pGuestListener->init(new GuestEventListener());

            /* Register for IGuest events. */
            ComPtr<IEventSource> es;
            CHECK_ERROR_BREAK(pCtx->pGuest, COMGETTER(EventSource)(es.asOutParam()));
            com::SafeArray<VBoxEventType_T> eventTypes;
            eventTypes.push_back(VBoxEventType_OnGuestSessionRegistered);
            /** @todo Also register for VBoxEventType_OnGuestUserStateChanged on demand? */
            CHECK_ERROR_BREAK(es, RegisterListener(pGuestListener, ComSafeArrayAsInParam(eventTypes),
                                                   true /* Active listener */));
            /* Note: All other guest control events have to be registered
             *       as their corresponding objects appear. */

        } while (0);

        if (pCtx->cVerbose)
            RTPrintf("Waiting for events ...\n");

/** @todo r=bird: This are-we-there-yet approach to things could easily be
 *        replaced by a global event semaphore that gets signalled from the
 *        signal handler and the callback event.  Please fix! */
        while (!g_fGuestCtrlCanceled)
        {
            /** @todo Timeout handling (see above)? */
            RTThreadSleep(10);
        }

        if (pCtx->cVerbose)
            RTPrintf("Signal caught, exiting ...\n");

        if (!pGuestListener.isNull())
        {
            /* Guest callback unregistration. */
            ComPtr<IEventSource> pES;
            CHECK_ERROR(pCtx->pGuest, COMGETTER(EventSource)(pES.asOutParam()));
            if (!pES.isNull())
                CHECK_ERROR(pES, UnregisterListener(pGuestListener));
            pGuestListener.setNull();
        }
    }
    catch (std::bad_alloc &)
    {
        rc = E_OUTOFMEMORY;
    }

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Access the guest control store.
 *
 * @returns program exit code.
 * @note see the command line API description for parameters
 */
RTEXITCODE handleGuestControl(HandlerArg *pArg)
{
    AssertPtr(pArg);

#ifdef DEBUG_andy_disabled
    if (RT_FAILURE(tstTranslatePath()))
        return RTEXITCODE_FAILURE;
#endif

    /*
     * Command definitions.
     */
    static const GCTLCMDDEF s_aCmdDefs[] =
    {
        { "run",                gctlHandleRun,              USAGE_GSTCTRL_RUN,       0, },
        { "start",              gctlHandleStart,            USAGE_GSTCTRL_START,     0, },
        { "copyfrom",           gctlHandleCopyFrom,         USAGE_GSTCTRL_COPYFROM,  0, },
        { "copyto",             gctlHandleCopyTo,           USAGE_GSTCTRL_COPYTO,    0, },

        { "mkdir",              gctrlHandleMkDir,           USAGE_GSTCTRL_MKDIR,     0, },
        { "md",                 gctrlHandleMkDir,           USAGE_GSTCTRL_MKDIR,     0, },
        { "createdirectory",    gctrlHandleMkDir,           USAGE_GSTCTRL_MKDIR,     0, },
        { "createdir",          gctrlHandleMkDir,           USAGE_GSTCTRL_MKDIR,     0, },

        { "rmdir",              gctlHandleRmDir,            USAGE_GSTCTRL_RMDIR,     0, },
        { "removedir",          gctlHandleRmDir,            USAGE_GSTCTRL_RMDIR,     0, },
        { "removedirectory",    gctlHandleRmDir,            USAGE_GSTCTRL_RMDIR,     0, },

        { "rm",                 gctlHandleRm,               USAGE_GSTCTRL_RM,        0, },
        { "removefile",         gctlHandleRm,               USAGE_GSTCTRL_RM,        0, },
        { "erase",              gctlHandleRm,               USAGE_GSTCTRL_RM,        0, },
        { "del",                gctlHandleRm,               USAGE_GSTCTRL_RM,        0, },
        { "delete",             gctlHandleRm,               USAGE_GSTCTRL_RM,        0, },

        { "mv",                 gctlHandleMv,               USAGE_GSTCTRL_MV,        0, },
        { "move",               gctlHandleMv,               USAGE_GSTCTRL_MV,        0, },
        { "ren",                gctlHandleMv,               USAGE_GSTCTRL_MV,        0, },
        { "rename",             gctlHandleMv,               USAGE_GSTCTRL_MV,        0, },

        { "mktemp",             gctlHandleMkTemp,           USAGE_GSTCTRL_MKTEMP,    0, },
        { "createtemp",         gctlHandleMkTemp,           USAGE_GSTCTRL_MKTEMP,    0, },
        { "createtemporary",    gctlHandleMkTemp,           USAGE_GSTCTRL_MKTEMP,    0, },

        { "stat",               gctlHandleStat,             USAGE_GSTCTRL_STAT,      0, },

        { "closeprocess",       gctlHandleCloseProcess,     USAGE_GSTCTRL_CLOSEPROCESS, GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
        { "closesession",       gctlHandleCloseSession,     USAGE_GSTCTRL_CLOSESESSION, GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
        { "list",               gctlHandleList,             USAGE_GSTCTRL_LIST,         GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
        { "watch",              gctlHandleWatch,            USAGE_GSTCTRL_WATCH,        GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },

        {"updateguestadditions",gctlHandleUpdateAdditions,  USAGE_GSTCTRL_UPDATEGA,     GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
        { "updateadditions",    gctlHandleUpdateAdditions,  USAGE_GSTCTRL_UPDATEGA,     GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
        { "updatega",           gctlHandleUpdateAdditions,  USAGE_GSTCTRL_UPDATEGA,     GCTLCMDCTX_F_SESSION_ANONYMOUS | GCTLCMDCTX_F_NO_SIGNAL_HANDLER, },
    };

    /*
     * VBoxManage guestcontrol [common-options] <VM> [common-options] <sub-command> ...
     *
     * Parse common options and VM name until we find a sub-command.  Allowing
     * the user  to put the user and password related options before the
     * sub-command makes it easier to edit the command line when doing several
     * operations with the same guest user account.  (Accidentally, it also
     * makes the syntax diagram shorter and easier to read.)
     */
    GCTLCMDCTX CmdCtx;
    RTEXITCODE rcExit = gctrCmdCtxInit(&CmdCtx, pArg);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        static const RTGETOPTDEF s_CommonOptions[] = { GCTLCMD_COMMON_OPTION_DEFS() };

        int ch;
        RTGETOPTUNION ValueUnion;
        RTGETOPTSTATE GetState;
        RTGetOptInit(&GetState, pArg->argc, pArg->argv, s_CommonOptions, RT_ELEMENTS(s_CommonOptions), 0, 0 /* No sorting! */);

        while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
        {
            switch (ch)
            {
                GCTLCMD_COMMON_OPTION_CASES(&CmdCtx, ch, &ValueUnion);

                case VINF_GETOPT_NOT_OPTION:
                    /* First comes the VM name or UUID. */
                    if (!CmdCtx.pszVmNameOrUuid)
                        CmdCtx.pszVmNameOrUuid = ValueUnion.psz;
                    /*
                     * The sub-command is next.  Look it up and invoke it.
                     * Note! Currently no warnings about user/password options (like we'll do later on)
                     *       for GCTLCMDCTX_F_SESSION_ANONYMOUS commands. No reason to be too pedantic.
                     */
                    else
                    {
                        const char *pszCmd = ValueUnion.psz;
                        uint32_t    iCmd;
                        for (iCmd = 0; iCmd < RT_ELEMENTS(s_aCmdDefs); iCmd++)
                            if (strcmp(s_aCmdDefs[iCmd].pszName, pszCmd) == 0)
                            {
                                CmdCtx.pCmdDef = &s_aCmdDefs[iCmd];

                                rcExit = s_aCmdDefs[iCmd].pfnHandler(&CmdCtx, pArg->argc - GetState.iNext + 1,
                                                                     &pArg->argv[GetState.iNext - 1]);

                                gctlCtxTerm(&CmdCtx);
                                return rcExit;
                            }
                        return errorSyntax(USAGE_GUESTCONTROL, "Unknown sub-command: '%s'", pszCmd);
                    }
                    break;

                default:
                    return errorGetOpt(USAGE_GUESTCONTROL, ch, &ValueUnion);
            }
        }
        if (CmdCtx.pszVmNameOrUuid)
            rcExit = errorSyntax(USAGE_GUESTCONTROL, "Missing sub-command");
        else
            rcExit = errorSyntax(USAGE_GUESTCONTROL, "Missing VM name and sub-command");
    }
    return rcExit;
}
#endif /* !VBOX_ONLY_DOCS */

