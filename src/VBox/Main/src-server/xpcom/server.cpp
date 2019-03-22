/* $Id$ */
/** @file
 * XPCOM server process (VBoxSVC) start point.
 */

/*
 * Copyright (C) 2004-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <ipcIService.h>
#include <ipcCID.h>

#include <nsIComponentRegistrar.h>

#include <nsGenericFactory.h>

#include "prio.h"
#include "prproces.h"

#include "server.h"

#include "Logging.h"

#include <VBox/param.h>

#include <iprt/buildconfig.h>
#include <iprt/initterm.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/timer.h>
#include <iprt/env.h>

#include <signal.h>     // for the signal handler
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

/////////////////////////////////////////////////////////////////////////////
// VirtualBox component instantiation
/////////////////////////////////////////////////////////////////////////////

#include <nsIGenericFactory.h>
#include <VBox/com/VirtualBox.h>

#include "VBox/com/NativeEventQueue.h"

#include "ApplianceImpl.h"
#include "AudioAdapterImpl.h"
#include "BandwidthControlImpl.h"
#include "BandwidthGroupImpl.h"
#include "NetworkServiceRunner.h"
#include "DHCPServerImpl.h"
#include "GuestOSTypeImpl.h"
#include "HostImpl.h"
#include "HostNetworkInterfaceImpl.h"
#include "MachineImpl.h"
#include "MediumFormatImpl.h"
#include "MediumImpl.h"
#include "NATEngineImpl.h"
#include "NetworkAdapterImpl.h"
#include "ParallelPortImpl.h"
#include "ProgressProxyImpl.h"
#include "SerialPortImpl.h"
#include "SharedFolderImpl.h"
#include "SnapshotImpl.h"
#include "StorageControllerImpl.h"
#include "SystemPropertiesImpl.h"
#include "USBControllerImpl.h"
#include "USBDeviceFiltersImpl.h"
#include "VFSExplorerImpl.h"
#include "VirtualBoxImpl.h"
#include "VRDEServerImpl.h"
#ifdef VBOX_WITH_USB
# include "HostUSBDeviceImpl.h"
# include "USBDeviceFilterImpl.h"
# include "USBDeviceImpl.h"
#endif
#ifdef VBOX_WITH_EXTPACK
# include "ExtPackManagerImpl.h"
#endif
# include "NATNetworkImpl.h"

// This needs to stay - it is needed by the service registration below, and
// is defined in the automatically generated VirtualBoxWrap.cpp
extern nsIClassInfo *NS_CLASSINFO_NAME(VirtualBoxWrap);
NS_DECL_CI_INTERFACE_GETTER(VirtualBoxWrap)

////////////////////////////////////////////////////////////////////////////////

static bool gAutoShutdown = false;
/** Delay before shutting down the VirtualBox server after the last
 * VirtualBox instance is released, in ms */
static uint32_t gShutdownDelayMs = 5000;

static com::NativeEventQueue *gEventQ   = NULL;
static PRBool volatile gKeepRunning     = PR_TRUE;
static PRBool volatile gAllowSigUsrQuit = PR_TRUE;

/////////////////////////////////////////////////////////////////////////////

/**
 *  VirtualBox class factory that destroys the created instance right after
 *  the last reference to it is released by the client, and recreates it again
 *  when necessary (so VirtualBox acts like a singleton object).
 */
class VirtualBoxClassFactory : public VirtualBox
{
public:

    virtual ~VirtualBoxClassFactory()
    {
        LogFlowFunc(("Deleting VirtualBox...\n"));

        FinalRelease();
        sInstance = NULL;

        LogFlowFunc(("VirtualBox object deleted.\n"));
        RTPrintf("Informational: VirtualBox object deleted.\n");
    }

    NS_IMETHOD_(nsrefcnt) Release()
    {
        /* we overload Release() to guarantee the VirtualBox destructor is
         * always called on the main thread */

        nsrefcnt count = VirtualBox::Release();

        if (count == 1)
        {
            /* the last reference held by clients is being released
             * (see GetInstance()) */

            bool onMainThread = RTThreadIsMain(RTThreadSelf());
            PRBool timerStarted = PR_FALSE;

            /* sTimer is null if this call originates from FactoryDestructor()*/
            if (sTimer != NULL)
            {
                LogFlowFunc(("Last VirtualBox instance was released.\n"));
                LogFlowFunc(("Scheduling server shutdown in %u ms...\n",
                             gShutdownDelayMs));

                /* make sure the previous timer (if any) is stopped;
                 * otherwise RTTimerStart() will definitely fail. */
                RTTimerLRStop(sTimer);

                int vrc = RTTimerLRStart(sTimer, gShutdownDelayMs * RT_NS_1MS_64);
                AssertRC(vrc);
                timerStarted = !!(SUCCEEDED(vrc));
            }
            else
            {
                LogFlowFunc(("Last VirtualBox instance was released "
                             "on XPCOM shutdown.\n"));
                Assert(onMainThread);
            }

            gAllowSigUsrQuit = PR_TRUE;

            if (!timerStarted)
            {
                if (!onMainThread)
                {
                    /* Failed to start the timer, post the shutdown event
                     * manually if not on the main thread already. */
                    ShutdownTimer(NULL, NULL, 0);
                }
                else
                {
                    /* Here we come if:
                     *
                     * a) gEventQ is 0 which means either FactoryDestructor() is called
                     *    or the IPC/DCONNECT shutdown sequence is initiated by the
                     *    XPCOM shutdown routine (NS_ShutdownXPCOM()), which always
                     *    happens on the main thread.
                     *
                     * b) gEventQ has reported we're on the main thread. This means
                     *    that DestructEventHandler() has been called, but another
                     *    client was faster and requested VirtualBox again.
                     *
                     * In either case, there is nothing to do.
                     *
                     * Note: case b) is actually no more valid since we don't
                     * call Release() from DestructEventHandler() in this case
                     * any more. Thus, we assert below.
                     */

                    Assert(!gEventQ);
                }
            }
        }

        return count;
    }

    class MaybeQuitEvent : public NativeEvent
    {
    public:
        MaybeQuitEvent() :
            m_fSignal(false)
        {
        }

        MaybeQuitEvent(bool fSignal) :
            m_fSignal(fSignal)
        {
        }

    private:
        /* called on the main thread */
        void *handler()
        {
            LogFlowFuncEnter();

            Assert(RTCritSectIsInitialized(&sLock));

            /* stop accepting GetInstance() requests on other threads during
             * possible destruction */
            RTCritSectEnter(&sLock);

            nsrefcnt count = 1;

            /* sInstance is NULL here if it was deleted immediately after
             * creation due to initialization error. See GetInstance(). */
            if (sInstance != NULL)
            {
                /* Safe way to get current refcount is by first increasing and
                 * then decreasing. Keep in mind that the Release is overloaded
                 * (see VirtualBoxClassFactory::Release) and will start the
                 * timer again if the returned count is 1. It won't do harm,
                 * but also serves no purpose, so stop it ASAP. */
                sInstance->AddRef();
                count = sInstance->Release();
                if (count == 1)
                {
                    RTTimerLRStop(sTimer);
                    /* Release the guard reference added in GetInstance() */
                    sInstance->Release();
                }
            }

            if (count == 1)
            {
                if (gAutoShutdown || m_fSignal)
                {
                    Assert(sInstance == NULL);
                    LogFlowFunc(("Terminating the server process...\n"));
                    /* make it leave the event loop */
                    gKeepRunning = PR_FALSE;
                }
                else
                    LogFlowFunc(("No automatic shutdown.\n"));
            }
            else
            {
                /* This condition is quite rare: a new client happened to
                 * connect after this event has been posted to the main queue
                 * but before it started to process it. */
                LogRel(("Destruction is canceled (refcnt=%d).\n", count));
            }

            RTCritSectLeave(&sLock);

            LogFlowFuncLeave();
            return NULL;
        }

        bool m_fSignal;
    };

    static DECLCALLBACK(void) ShutdownTimer(RTTIMERLR hTimerLR, void *pvUser, uint64_t /*iTick*/)
    {
        NOREF(hTimerLR);
        NOREF(pvUser);

        /* A "too late" event is theoretically possible if somebody
         * manually ended the server after a destruction has been scheduled
         * and this method was so lucky that it got a chance to run before
         * the timer was killed. */
        com::NativeEventQueue *q = gEventQ;
        AssertReturnVoid(q);

        /* post a quit event to the main queue */
        MaybeQuitEvent *ev = new MaybeQuitEvent(false /* fSignal */);
        if (!q->postEvent(ev))
            delete ev;

        /* A failure above means we've been already stopped (for example
         * by Ctrl-C). FactoryDestructor() (NS_ShutdownXPCOM())
         * will do the job. Nothing to do. */
    }

    static NS_IMETHODIMP FactoryConstructor()
    {
        LogFlowFunc(("\n"));

        /* create a critsect to protect object construction */
        if (RT_FAILURE(RTCritSectInit(&sLock)))
            return NS_ERROR_OUT_OF_MEMORY;

        int vrc = RTTimerLRCreateEx(&sTimer, 0, 0, ShutdownTimer, NULL);
        if (RT_FAILURE(vrc))
        {
            LogFlowFunc(("Failed to create a timer! (vrc=%Rrc)\n", vrc));
            return NS_ERROR_FAILURE;
        }

        return NS_OK;
    }

    static NS_IMETHODIMP FactoryDestructor()
    {
        LogFlowFunc(("\n"));

        RTTimerLRDestroy(sTimer);
        sTimer = NULL;

        if (sInstance != NULL)
        {
            /* Either posting a destruction event failed for some reason (most
             * likely, the quit event has been received before the last release),
             * or the client has terminated abnormally w/o releasing its
             * VirtualBox instance (so NS_ShutdownXPCOM() is doing a cleanup).
             * Release the guard reference we added in GetInstance(). */
            sInstance->Release();
        }

        /* Destroy lock after releasing the VirtualBox instance, otherwise
         * there are races with cleanup. */
        RTCritSectDelete(&sLock);

        return NS_OK;
    }

    static nsresult GetInstance(VirtualBox **inst)
    {
        LogFlowFunc(("Getting VirtualBox object...\n"));

        RTCritSectEnter(&sLock);

        if (!gKeepRunning)
        {
            LogFlowFunc(("Process termination requested first. Refusing.\n"));

            RTCritSectLeave(&sLock);

            /* this rv is what CreateInstance() on the client side returns
             * when the server process stops accepting events. Do the same
             * here. The client wrapper should attempt to start a new process in
             * response to a failure from us. */
            return NS_ERROR_ABORT;
        }

        nsresult rv = NS_OK;

        if (sInstance == NULL)
        {
            LogFlowFunc(("Creating new VirtualBox object...\n"));
            sInstance = new VirtualBoxClassFactory();
            if (sInstance != NULL)
            {
                /* make an extra AddRef to take the full control
                 * on the VirtualBox destruction (see FinalRelease()) */
                sInstance->AddRef();

                sInstance->AddRef(); /* protect FinalConstruct() */
                rv = sInstance->FinalConstruct();
                RTPrintf("Informational: VirtualBox object created (rc=%Rhrc).\n", rv);
                if (NS_FAILED(rv))
                {
                    /* On failure diring VirtualBox initialization, delete it
                     * immediately on the current thread by releasing all
                     * references in order to properly schedule the server
                     * shutdown. Since the object is fully deleted here, there
                     * is a chance to fix the error and request a new
                     * instantiation before the server terminates. However,
                     * the main reason to maintain the shutdown delay on
                     * failure is to let the front-end completely fetch error
                     * info from a server-side IVirtualBoxErrorInfo object. */
                    sInstance->Release();
                    sInstance->Release();
                    Assert(sInstance == NULL);
                }
                else
                {
                    /* On success, make sure the previous timer is stopped to
                     * cancel a scheduled server termination (if any). */
                    gAllowSigUsrQuit = PR_FALSE;
                    RTTimerLRStop(sTimer);
                }
            }
            else
            {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
        else
        {
            LogFlowFunc(("Using existing VirtualBox object...\n"));
            nsrefcnt count = sInstance->AddRef();
            Assert(count > 1);

            if (count >= 2)
            {
                LogFlowFunc(("Another client has requested a reference to VirtualBox, canceling destruction...\n"));

                /* make sure the previous timer is stopped */
                gAllowSigUsrQuit = PR_FALSE;
                RTTimerLRStop(sTimer);
            }
        }

        *inst = sInstance;

        RTCritSectLeave(&sLock);

        return rv;
    }

private:

    /* Don't be confused that sInstance is of the *ClassFactory type. This is
     * actually a singleton instance (*ClassFactory inherits the singleton
     * class; we combined them just for "simplicity" and used "static" for
     * factory methods. *ClassFactory here is necessary for a couple of extra
     * methods. */

    static VirtualBoxClassFactory *sInstance;
    static RTCRITSECT sLock;

    static RTTIMERLR sTimer;
};

VirtualBoxClassFactory *VirtualBoxClassFactory::sInstance = NULL;
RTCRITSECT VirtualBoxClassFactory::sLock;

RTTIMERLR VirtualBoxClassFactory::sTimer = NIL_RTTIMERLR;

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR_WITH_RC(VirtualBox, VirtualBoxClassFactory::GetInstance)

////////////////////////////////////////////////////////////////////////////////

typedef NSFactoryDestructorProcPtr NSFactoryConstructorProcPtr;

/**
 *  Enhanced module component information structure.
 *
 *  nsModuleComponentInfo lacks the factory construction callback, here we add
 *  it. This callback is called straight after a nsGenericFactory instance is
 *  successfully created in RegisterSelfComponents.
 */
struct nsModuleComponentInfoPlusFactoryConstructor
{
    /** standard module component information */
    const nsModuleComponentInfo *mpModuleComponentInfo;
    /** (optional) Factory Construction Callback */
    NSFactoryConstructorProcPtr mFactoryConstructor;
};

/////////////////////////////////////////////////////////////////////////////

/**
 * Helper function to register self components upon start-up
 * of the out-of-proc server.
 */
static nsresult
RegisterSelfComponents(nsIComponentRegistrar *registrar,
                       const nsModuleComponentInfoPlusFactoryConstructor *aComponents,
                       PRUint32 count)
{
    nsresult rc = NS_OK;
    const nsModuleComponentInfoPlusFactoryConstructor *info = aComponents;
    for (PRUint32 i = 0; i < count && NS_SUCCEEDED(rc); i++, info++)
    {
        /* skip components w/o a constructor */
        if (!info->mpModuleComponentInfo->mConstructor)
            continue;
        /* create a new generic factory for a component and register it */
        nsIGenericFactory *factory;
        rc = NS_NewGenericFactory(&factory, info->mpModuleComponentInfo);
        if (NS_SUCCEEDED(rc) && info->mFactoryConstructor)
        {
            rc = info->mFactoryConstructor();
            if (NS_FAILED(rc))
                NS_RELEASE(factory);
        }
        if (NS_SUCCEEDED(rc))
        {
            rc = registrar->RegisterFactory(info->mpModuleComponentInfo->mCID,
                                            info->mpModuleComponentInfo->mDescription,
                                            info->mpModuleComponentInfo->mContractID,
                                            factory);
            NS_RELEASE(factory);
        }
    }
    return rc;
}

/////////////////////////////////////////////////////////////////////////////

static ipcIService *gIpcServ = nsnull;
static const char *g_pszPidFile = NULL;

class ForceQuitEvent : public NativeEvent
{
    void *handler()
    {
        LogFlowFunc(("\n"));

        gKeepRunning = PR_FALSE;

        if (g_pszPidFile)
            RTFileDelete(g_pszPidFile);

        return NULL;
    }
};

static void signal_handler(int sig)
{
    com::NativeEventQueue *q = gEventQ;
    if (q && gKeepRunning)
    {
        if (sig == SIGUSR1)
        {
            if (gAllowSigUsrQuit)
            {
                /* terminate the server process if it is idle */
                VirtualBoxClassFactory::MaybeQuitEvent *ev = new VirtualBoxClassFactory::MaybeQuitEvent(true /* fSignal */);
                if (!q->postEvent(ev))
                    delete ev;
            }
            /* else do nothing */
        }
        else
        {
            /* post a force quit event to the queue */
            ForceQuitEvent *ev = new ForceQuitEvent();
            if (!q->postEvent(ev))
                delete ev;
        }
    }
}

static nsresult vboxsvcSpawnDaemonByReExec(const char *pszPath, bool fAutoShutdown, const char *pszPidFile)
{
    PRFileDesc *readable = nsnull, *writable = nsnull;
    PRProcessAttr *attr = nsnull;
    nsresult rv = NS_ERROR_FAILURE;
    PRFileDesc *devNull;
    unsigned args_index = 0;
    // The ugly casts are necessary because the PR_CreateProcessDetached has
    // a const array of writable strings as a parameter. It won't write. */
    char * args[1 + 1 + 2 + 1];
    args[args_index++] = (char *)pszPath;
    if (fAutoShutdown)
        args[args_index++] = (char *)"--auto-shutdown";
    if (pszPidFile)
    {
        args[args_index++] = (char *)"--pidfile";
        args[args_index++] = (char *)pszPidFile;
    }
    args[args_index++] = 0;

    // Use a pipe to determine when the daemon process is in the position
    // to actually process requests. The daemon will write "READY" to the pipe.
    if (PR_CreatePipe(&readable, &writable) != PR_SUCCESS)
        goto end;
    PR_SetFDInheritable(writable, PR_TRUE);

    attr = PR_NewProcessAttr();
    if (!attr)
        goto end;

    if (PR_ProcessAttrSetInheritableFD(attr, writable, VBOXSVC_STARTUP_PIPE_NAME) != PR_SUCCESS)
        goto end;

    devNull = PR_Open("/dev/null", PR_RDWR, 0);
    if (!devNull)
        goto end;

    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardInput, devNull);
    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardOutput, devNull);
    PR_ProcessAttrSetStdioRedirect(attr, PR_StandardError, devNull);

    if (PR_CreateProcessDetached(pszPath, (char * const *)args, nsnull, attr) != PR_SUCCESS)
        goto end;

    // Close /dev/null
    PR_Close(devNull);
    // Close the child end of the pipe to make it the only owner of the
    // file descriptor, so that unexpected closing can be detected.
    PR_Close(writable);
    writable = nsnull;

    char msg[10];
    memset(msg, '\0', sizeof(msg));
    if (   PR_Read(readable, msg, sizeof(msg)-1) != 5
        || strcmp(msg, "READY"))
        goto end;

    rv = NS_OK;

end:
    if (readable)
        PR_Close(readable);
    if (writable)
        PR_Close(writable);
    if (attr)
        PR_DestroyProcessAttr(attr);
    return rv;
}

int main(int argc, char **argv)
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver
     */
    int vrc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(vrc))
        return RTMsgInitFailure(vrc);

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--automate",         'a', RTGETOPT_REQ_NOTHING },
        { "--auto-shutdown",    'A', RTGETOPT_REQ_NOTHING },
        { "--daemonize",        'd', RTGETOPT_REQ_NOTHING },
        { "--shutdown-delay",   'D', RTGETOPT_REQ_UINT32 },
        { "--pidfile",          'p', RTGETOPT_REQ_STRING },
        { "--logfile",          'F', RTGETOPT_REQ_STRING },
        { "--logrotate",        'R', RTGETOPT_REQ_UINT32 },
        { "--logsize",          'S', RTGETOPT_REQ_UINT64 },
        { "--loginterval",      'I', RTGETOPT_REQ_UINT32 }
    };

    const char      *pszLogFile = NULL;
    uint32_t        cHistory = 10;                  // enable log rotation, 10 files
    uint32_t        uHistoryFileTime = RT_SEC_1DAY; // max 1 day per file
    uint64_t        uHistoryFileSize = 100 * _1M;   // max 100MB per file
    bool            fDaemonize = false;
    PRFileDesc      *daemon_pipe_wr = nsnull;

    RTGETOPTSTATE   GetOptState;
    vrc = RTGetOptInit(&GetOptState, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    AssertRC(vrc);

    RTGETOPTUNION   ValueUnion;
    while ((vrc = RTGetOpt(&GetOptState, &ValueUnion)))
    {
        switch (vrc)
        {
            case 'a':
                /* --automate mode means we are started by XPCOM on
                 * demand. Daemonize ourselves and activate
                 * auto-shutdown. */
                gAutoShutdown = true;
                fDaemonize = true;
                break;

            case 'A':
                /* --auto-shutdown mode means we're already daemonized. */
                gAutoShutdown = true;
                break;

            case 'd':
                fDaemonize = true;
                break;

            case 'D':
                gShutdownDelayMs = ValueUnion.u32;
                break;

            case 'p':
                g_pszPidFile = ValueUnion.psz;
                break;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                cHistory = ValueUnion.u32;
                break;

            case 'S':
                uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                uHistoryFileTime = ValueUnion.u32;
                break;

            case 'h':
                RTPrintf("no help\n");
                return RTEXITCODE_SYNTAX;

            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return RTEXITCODE_SUCCESS;

            default:
                return RTGetOptPrintError(vrc, &ValueUnion);
        }
    }

    if (fDaemonize)
    {
        vboxsvcSpawnDaemonByReExec(argv[0], gAutoShutdown, g_pszPidFile);
        exit(126);
    }

    nsresult rc;

    /** @todo Merge this code with svcmain.cpp (use Logging.cpp?). */
    char szLogFile[RTPATH_MAX];
    if (!pszLogFile)
    {
        vrc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
        if (RT_SUCCESS(vrc))
            vrc = RTPathAppend(szLogFile, sizeof(szLogFile), "VBoxSVC.log");
    }
    else
    {
        if (!RTStrPrintf(szLogFile, sizeof(szLogFile), "%s", pszLogFile))
            vrc = VERR_NO_MEMORY;
    }
    if (RT_FAILURE(vrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to create logging file name, rc=%Rrc", vrc);

    RTERRINFOSTATIC ErrInfo;
    vrc = com::VBoxLogRelCreate("XPCOM Server", szLogFile,
                                RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                                VBOXSVC_LOG_DEFAULT, "VBOXSVC_RELEASE_LOG",
                                RTLOGDEST_FILE, UINT32_MAX /* cMaxEntriesPerGroup */,
                                cHistory, uHistoryFileTime, uHistoryFileSize,
                                RTErrInfoInitStatic(&ErrInfo));
    if (RT_FAILURE(vrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", ErrInfo.Core.pszMsg, vrc);

    /* Set up a build identifier so that it can be seen from core dumps what
     * exact build was used to produce the core. Same as in Console::i_powerUpThread(). */
    static char saBuildID[48];
    RTStrPrintf(saBuildID, sizeof(saBuildID), "%s%s%s%s VirtualBox %s r%u %s%s%s%s",
                "BU", "IL", "DI", "D", RTBldCfgVersion(), RTBldCfgRevision(), "BU", "IL", "DI", "D");

    daemon_pipe_wr = PR_GetInheritedFD(VBOXSVC_STARTUP_PIPE_NAME);
    RTEnvUnset("NSPR_INHERIT_FDS");

    const nsModuleComponentInfo VirtualBoxInfo = {
        "VirtualBox component",
        NS_VIRTUALBOX_CID,
        NS_VIRTUALBOX_CONTRACTID,
        VirtualBoxConstructor, // constructor function
        NULL, // registration function
        NULL, // deregistration function
        VirtualBoxClassFactory::FactoryDestructor, // factory destructor function
        NS_CI_INTERFACE_GETTER_NAME(VirtualBoxWrap),
        NULL, // language helper
        &NS_CLASSINFO_NAME(VirtualBoxWrap),
        0 // flags
    };

    const nsModuleComponentInfoPlusFactoryConstructor components[] = {
        {
            &VirtualBoxInfo,
            VirtualBoxClassFactory::FactoryConstructor // factory constructor function
        }
    };

    do /* goto avoidance only */
    {
        rc = com::Initialize();
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to initialize XPCOM! (rc=%Rhrc)\n", rc);
            break;
        }

        nsCOMPtr<nsIComponentRegistrar> registrar;
        rc = NS_GetComponentRegistrar(getter_AddRefs(registrar));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to get component registrar! (rc=%Rhrc)", rc);
            break;
        }

        registrar->AutoRegister(nsnull);
        rc = RegisterSelfComponents(registrar, components,
                                    NS_ARRAY_LENGTH(components));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to register server components! (rc=%Rhrc)", rc);
            break;
        }

        nsCOMPtr<ipcIService> ipcServ(do_GetService(IPC_SERVICE_CONTRACTID, &rc));
        if (NS_FAILED(rc))
        {
            RTMsgError("Failed to get IPC service! (rc=%Rhrc)", rc);
            break;
        }

        NS_ADDREF(gIpcServ = ipcServ);

        LogFlowFunc(("Will use \"%s\" as server name.\n", VBOXSVC_IPC_NAME));

        rc = gIpcServ->AddName(VBOXSVC_IPC_NAME);
        if (NS_FAILED(rc))
        {
            LogFlowFunc(("Failed to register the server name (rc=%Rhrc (%08X))!\n"
                         "Is another server already running?\n", rc, rc));

            RTMsgError("Failed to register the server name \"%s\" (rc=%Rhrc)!\n"
                       "Is another server already running?\n",
                       VBOXSVC_IPC_NAME, rc);
            NS_RELEASE(gIpcServ);
            break;
        }

        {
            /* setup signal handling to convert some signals to a quit event */
            struct sigaction sa;
            sa.sa_handler = signal_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGQUIT, &sa, NULL);
            sigaction(SIGTERM, &sa, NULL);
// XXX Temporary allow release assertions to terminate VBoxSVC
//            sigaction(SIGTRAP, &sa, NULL);
            sigaction(SIGUSR1, &sa, NULL);
        }

        {
            char szBuf[80];
            size_t cSize;

            cSize = RTStrPrintf(szBuf, sizeof(szBuf),
                                VBOX_PRODUCT" XPCOM Server Version "
                                VBOX_VERSION_STRING);
            for (size_t i = cSize; i > 0; i--)
                putchar('*');
            RTPrintf("\n%s\n", szBuf);
            RTPrintf("(C) 2004-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
                     "All rights reserved.\n");
#ifdef DEBUG
            RTPrintf("Debug version.\n");
#endif
        }

        if (daemon_pipe_wr != nsnull)
        {
            RTPrintf("\nStarting event loop....\n[send TERM signal to quit]\n");
            /* now we're ready, signal the parent process */
            PR_Write(daemon_pipe_wr, RT_STR_TUPLE("READY"));
            /* close writing end of the pipe, its job is done */
            PR_Close(daemon_pipe_wr);
        }
        else
            RTPrintf("\nStarting event loop....\n[press Ctrl-C to quit]\n");

        if (g_pszPidFile)
        {
            RTFILE hPidFile = NIL_RTFILE;
            vrc = RTFileOpen(&hPidFile, g_pszPidFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE);
            if (RT_SUCCESS(vrc))
            {
                char szBuf[32];
                const char *lf = "\n";
                RTStrFormatNumber(szBuf, getpid(), 10, 0, 0, 0);
                RTFileWrite(hPidFile, szBuf, strlen(szBuf), NULL);
                RTFileWrite(hPidFile, lf, strlen(lf), NULL);
                RTFileClose(hPidFile);
            }
        }

        // Increase the file table size to 10240 or as high as possible.
        struct rlimit lim;
        if (getrlimit(RLIMIT_NOFILE, &lim) == 0)
        {
            if (    lim.rlim_cur < 10240
                &&  lim.rlim_cur < lim.rlim_max)
            {
                lim.rlim_cur = RT_MIN(lim.rlim_max, 10240);
                if (setrlimit(RLIMIT_NOFILE, &lim) == -1)
                    RTPrintf("WARNING: failed to increase file descriptor limit. (%d)\n", errno);
            }
        }
        else
            RTPrintf("WARNING: failed to obtain per-process file-descriptor limit (%d).\n", errno);

        /* get the main thread's event queue */
        gEventQ = com::NativeEventQueue::getMainEventQueue();
        if (!gEventQ)
        {
            RTMsgError("Failed to get the main event queue! (rc=%Rhrc)", rc);
            break;
        }

        while (gKeepRunning)
        {
            vrc = gEventQ->processEventQueue(RT_INDEFINITE_WAIT);
            if (RT_FAILURE(vrc) && vrc != VERR_TIMEOUT)
            {
                LogRel(("Failed to wait for events! (rc=%Rrc)", vrc));
                break;
            }
        }

        gEventQ = NULL;
        RTPrintf("Terminated event loop.\n");

        /* unregister ourselves. After this point, clients will start a new
         * process because they won't be able to resolve the server name.*/
        gIpcServ->RemoveName(VBOXSVC_IPC_NAME);
    }
    while (0); // this scopes the nsCOMPtrs

    NS_IF_RELEASE(gIpcServ);

    /* no nsCOMPtrs are allowed to be alive when you call com::Shutdown(). */

    LogFlowFunc(("Calling com::Shutdown()...\n"));
    rc = com::Shutdown();
    LogFlowFunc(("Finished com::Shutdown() (rc=%Rhrc)\n", rc));

    if (NS_FAILED(rc))
        RTMsgError("Failed to shutdown XPCOM! (rc=%Rhrc)", rc);

    RTPrintf("XPCOM server has shutdown.\n");

    if (g_pszPidFile)
        RTFileDelete(g_pszPidFile);

    return RTEXITCODE_SUCCESS;
}
