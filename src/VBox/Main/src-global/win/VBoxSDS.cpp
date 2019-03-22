/* $Id$ */
/** @file
 * VBoxSDS - COM global service main entry (System Directory Service)
 */

/*
 * Copyright (C) 2015-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/** @page pg_VBoxSDS    VBoxSDS - Per user CLSID_VirtualBox coordinater
 *
 * VBoxSDS is short for VirtualBox System Directory Service (SDS).  It's purpose
 * is to make sure there only one CLSID_VirtualBox object running for each user
 * uisng VirtualBox on a Windows host system.
 *
 *
 * @section sec_vboxsds_backgroud   Background
 *
 * COM is desktop oriented when it comes to activate-as-activator (AAA) COM
 * servers.  This means that if the users has two logins to the same box (e.g.
 * physical console, RDP, SSHD) and tries to use an AAA COM server, a new server
 * will be instantiated for each login.  With the introduction of User Account
 * Control (UAC) in Windows Vista, this was taken a step further and a user
 * would talk different AAA COM server instances depending on the elevation
 * level too.
 *
 * VBoxSVC is a service affected by this issue.  Using VirtualBox accross logins
 * or between user elevation levels was impossible to do simultaneously.  This
 * was confusing and illogical to the user.
 *
 *
 * @section sec_vboxsds_how         How it works
 *
 * VBoxSDS assists in working around this problem by tracking which VBoxSVC
 * server is currently providing CLSID_VirtualBox for a user.  Each VBoxSVC
 * instance will register itself with VBoxSDS when the CLSID_VirtualBox object
 * is requested via their class factory.  The first VBoxSVC registering for a
 * given user will be allowed to instantate CLSID_VirtualBox.  We will call this
 * the chosen one.  Subsequent VBoxSVC instance for the given user, regardless
 * of elevation, session, windows station, or whatever else, will be told to use
 * the instance from the first VBoxSVC.
 *
 * The registration call passes along an IVBoxSVCRegistration interface from
 * VBoxSVC.  VBoxSDS keeps this around for the chosen one only.  When other
 * VBoxSVC instances for the same user tries to register, VBoxSDS will ask the
 * choosen one for its CLSID_VirtualBox object and return it to the new
 * registrant.
 *
 * The chosen one will deregister with VBoxSDS before it terminates.  Should it
 * terminate abnormally, VBoxSDS will (probably) notice the next time it tries
 * to request CLSID_VirtualBox from it and replace it as the chosen one with the
 * new registrant.
 *
 *
 * @section sec_vboxsds_locking     Locking
 *
 * VBoxSDS stores data in a map indexed by the stringified secure identifier
 * (SID) for each user.  The map is protected by a shared critical section, so
 * only inserting new users requires exclusive access.
 *
 * Each user data entry has it own lock (regular, not shared), so that it won't
 * be necessary to hold down the map lock while accessing per user data.  Thus
 * preventing a user from blocking all others from using VirtualBox by
 * suspending or debugging their chosen VBoxSVC process.
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>
#include <iprt/win/shlobj.h>

#include "VBox/com/defs.h"
#include "VBox/com/com.h"
#include "VBox/com/VirtualBox.h"

#include "VirtualBoxSDSImpl.h"
#include "Logging.h"

#include <VBox/err.h>
#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/message.h>
#include <iprt/string.h>

#include <VBox/com/microatl.h>

#define _ATL_FREE_THREADED

/**
 * Implements Windows Service
 */
class ATL_NO_VTABLE CWindowsServiceModule
{
protected:
    // data members
    WCHAR                   m_wszServiceName[256];
    WCHAR                   m_wszServiceDisplayName[256];
    WCHAR                   m_wszServiceDescription[256];
    SERVICE_STATUS_HANDLE   m_hServiceStatus;
    SERVICE_STATUS          m_Status;
    DWORD                   m_dwThreadID;

    /** Pointer to the instance, for use by staticServiceMain and staticHandler.  */
    static CWindowsServiceModule *s_pInstance;

public:
    CWindowsServiceModule() throw()
    {
        // set up the initial service status
        m_hServiceStatus = NULL;
        m_Status.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
        m_Status.dwCurrentState            = SERVICE_STOPPED;
        m_Status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
        m_Status.dwWin32ExitCode           = 0;
        m_Status.dwServiceSpecificExitCode = 0;
        m_Status.dwCheckPoint              = 0;
        m_Status.dwWaitHint                = 3000;

        s_pInstance = this;
    }

    virtual ~CWindowsServiceModule()
    {
        s_pInstance = NULL;
    }

    /** @todo Nobody calls this method and it doesn't do anythin...   */
    virtual HRESULT InitializeSecurity() throw()
    {
        return S_OK;
    }

    HRESULT startService(int /*nShowCmd*/) throw()
    {
        SERVICE_TABLE_ENTRY aServiceTable[] =
        {
            { m_wszServiceName, staticServiceMain },
            { NULL, NULL }
        };

        if (::StartServiceCtrlDispatcher(aServiceTable) == 0)
        {
            m_Status.dwWin32ExitCode = ::GetLastError();
            LogRelFunc(("Error: Cannot start service in console mode. Code: %u\n", m_Status.dwWin32ExitCode));
        }

        return m_Status.dwWin32ExitCode;
    }

    virtual HRESULT registerService() throw()
    {
        HRESULT hrc;
        if (uninstallService())
        {
            hrc = onRegisterService();
            if (SUCCEEDED(hrc))
            {
                if (installService())
                    hrc = S_OK;
                else
                    hrc = E_FAIL;
            }
        }
        else
            hrc = E_FAIL;
        return hrc;
    }

    virtual HRESULT unregisterService() throw()
    {
        HRESULT hrc = E_FAIL;
        if (uninstallService())
            hrc = onUnregisterService();
        return hrc;
    }

private:
    void serviceMain( DWORD , LPTSTR* ) throw()
    {
        LogFunc(("Enter into serviceMain\n"));
        // Register the control request handler
        m_Status.dwCurrentState = SERVICE_START_PENDING;
        m_dwThreadID = ::GetCurrentThreadId();
        m_hServiceStatus = ::RegisterServiceCtrlHandler(m_wszServiceName, staticHandler);
        if (m_hServiceStatus == NULL)
        {
            LogWarnFunc(("Handler not installed\n"));
            return;
        }
        setServiceStatus(SERVICE_START_PENDING);

        m_Status.dwWin32ExitCode = S_OK;
        m_Status.dwCheckPoint = 0;
        m_Status.dwWaitHint = 0;

        // When the Run function returns, the service has stopped.
        m_Status.dwWin32ExitCode = runService(SW_HIDE);

        setServiceStatus(SERVICE_STOPPED);
        LogFunc(("Windows Service stopped\n"));
    }

    /** Service table callback. */
    static void WINAPI staticServiceMain(DWORD cArgs, LPTSTR *papwszArgs) throw()
    {
        AssertPtrReturnVoid(s_pInstance);
        s_pInstance->serviceMain(cArgs, papwszArgs);
    }

    HRESULT runService(int nShowCmd = SW_HIDE) throw()
    {
        HRESULT hr = preMessageLoop(nShowCmd);

        if (hr == S_OK)
            runMessageLoop();

        if (SUCCEEDED(hr))
            hr = postMessageLoop();

        return hr;
    }

protected:
    /** Hook that's called before the message loop starts.
     * Must return S_OK for it to start. */
    virtual HRESULT preMessageLoop(int /*nShowCmd*/) throw()
    {
        LogFunc(("Enter\n"));
        if (::InterlockedCompareExchange(&m_Status.dwCurrentState, SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
        {
            LogFunc(("VBoxSDS Service started/resumed without delay\n"));
            ::SetServiceStatus(m_hServiceStatus, &m_Status);
        }
        return S_OK;
    }

    /** Your typical windows message loop. */
    virtual void runMessageLoop()
    {
        MSG msg;
        while (::GetMessage(&msg, 0, 0, 0) > 0)
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    /** Hook that's called after the message loop ends. */
    virtual HRESULT postMessageLoop()
    {
        return S_OK;
    }

    /** @name Overridable status change handlers
     * @{ */
    virtual void onStop() throw()
    {
        setServiceStatus(SERVICE_STOP_PENDING);
        ::PostThreadMessage(m_dwThreadID, WM_QUIT, 0, 0);
        LogFunc(("Windows Service stopped\n"));
    }

    virtual void onPause() throw()
    {
    }

    virtual void onContinue() throw()
    {
    }

    virtual void onInterrogate() throw()
    {
    }

    virtual void onShutdown() throw()
    {
    }

    virtual void onUnknownRequest(DWORD dwOpcode) throw()
    {
        LogRelFunc(("Bad service request: %u (%#x)\n", dwOpcode, dwOpcode));
    }

    virtual HRESULT onRegisterService()
    {
        return S_OK;
    }

    virtual HRESULT onUnregisterService()
    {
        return S_OK;
    }
    /** @} */

private:
    void handler(DWORD dwOpcode) throw()
    {

        switch (dwOpcode)
        {
            case SERVICE_CONTROL_STOP:
                onStop();
                break;
            case SERVICE_CONTROL_PAUSE:
                onPause();
                break;
            case SERVICE_CONTROL_CONTINUE:
                onContinue();
                break;
            case SERVICE_CONTROL_INTERROGATE:
                onInterrogate();
                break;
            case SERVICE_CONTROL_SHUTDOWN:
                onShutdown();
                break;
            default:
                onUnknownRequest(dwOpcode);
        }
    }

    static void WINAPI staticHandler(DWORD dwOpcode) throw()
    {
        AssertPtrReturnVoid(s_pInstance);
        s_pInstance->handler(dwOpcode);
    }

protected:
    void setServiceStatus(DWORD dwState) throw()
    {
        uint32_t const uPrevState = ASMAtomicXchgU32((uint32_t volatile *)&m_Status.dwCurrentState, dwState);
        if (!::SetServiceStatus(m_hServiceStatus, &m_Status))
            LogRel(("Error: SetServiceStatus(%p, %u) failed: %u (uPrevState=%u)\n",
                    m_hServiceStatus, dwState, GetLastError(), uPrevState));
    }


public:
    /** @note unused */
    BOOL IsInstalled() throw()
    {
        BOOL fResult = FALSE;

        SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        if (hSCM != NULL)
        {
            SC_HANDLE hService = ::OpenService(hSCM, m_wszServiceName, SERVICE_QUERY_CONFIG);
            if (hService != NULL)
            {
                fResult = TRUE;
                ::CloseServiceHandle(hService);
            }
            ::CloseServiceHandle(hSCM);
        }

        return fResult;
    }

    BOOL installService() throw()
    {
        BOOL fResult = FALSE;
        SC_HANDLE hSCM = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
        if (hSCM != NULL)
        {
            SC_HANDLE hService = ::OpenService(hSCM, m_wszServiceName, SERVICE_QUERY_CONFIG);
            if (hService != NULL)
            {
                fResult = TRUE; /* Already installed. */

                ::CloseServiceHandle(hService);
            }
            else
            {
                // Get the executable file path and quote it.
                const int QUOTES_SPACE = 2;
                WCHAR wszFilePath[MAX_PATH + QUOTES_SPACE];
                DWORD cwcFilePath = ::GetModuleFileNameW(NULL, wszFilePath + 1, MAX_PATH);
                if (cwcFilePath != 0 && cwcFilePath < MAX_PATH)
                {
                    wszFilePath[0] = L'\"';
                    wszFilePath[cwcFilePath + 1] = L'\"';
                    wszFilePath[cwcFilePath + 2] = L'\0';

                    SC_HANDLE hService = ::CreateServiceW(hSCM, m_wszServiceName, m_wszServiceDisplayName,
                                                          SERVICE_CHANGE_CONFIG,
                                                          SERVICE_WIN32_OWN_PROCESS,
                                                          SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                                          wszFilePath, NULL, NULL, L"RPCSS\0", NULL, NULL);
                    if (hService != NULL)
                    {
                        SERVICE_DESCRIPTIONW sd;
                        sd.lpDescription = m_wszServiceDescription;
                        if (!::ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &sd))
                            AssertLogRelMsgFailed(("Error: could not set service description: %u\n", GetLastError()));

                        fResult = TRUE;

                        ::CloseServiceHandle(hService);
                    }
                    else
                        AssertLogRelMsgFailed(("Error: Could not create service '%ls': %u\n", m_wszServiceName, GetLastError()));
                }
                else
                    AssertLogRelMsgFailed(("Error: GetModuleFileNameW returned %u: %u\n", cwcFilePath, GetLastError()));
            }
        }
        else
            AssertLogRelMsgFailed(("Error: Could not open the service control manager: %u\n", GetLastError()));
        return fResult;
    }

    BOOL uninstallService() throw()
    {
        BOOL fResult = FALSE;
        SC_HANDLE hSCM = ::OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
        if (hSCM != NULL)
        {
            SC_HANDLE hService = ::OpenService(hSCM, m_wszServiceName, SERVICE_STOP | DELETE);
            if (hService == NULL)
            {
                DWORD dwErr = GetLastError();
                hService = ::OpenService(hSCM, m_wszServiceName, SERVICE_QUERY_CONFIG);
                if (hService == NULL)
                    fResult = TRUE; /* Probably not installed or some access problem. */
                else
                {
                    ::CloseServiceHandle(hService);
                    AssertLogRelMsgFailed(("Error: Failed to open '%ls' for stopping and deletion: %u\n", m_wszServiceName, dwErr));
                }
            }
            else
            {
                /* Try stop it. */
                SERVICE_STATUS status;
                RT_ZERO(status);
                if (!::ControlService(hService, SERVICE_CONTROL_STOP, &status))
                {
                    DWORD dwErr = GetLastError();
                    AssertLogRelMsg(       dwErr == ERROR_SERVICE_NOT_ACTIVE
                                    || (   dwErr == ERROR_SERVICE_CANNOT_ACCEPT_CTRL
                                        && status.dwCurrentState == SERVICE_STOP_PENDING)
                                    , ("Error: Failed to stop serive '%ls': dwErr=%u dwCurrentState=%u\n",
                                       m_wszServiceName, dwErr, status.dwCurrentState));
                }

                /* Try delete it. */
                fResult = ::DeleteService(hService);
                AssertLogRelMsg(fResult, ("Error: Failed to delete serivce '%ls': %u\n", m_wszServiceName, GetLastError()));

                ::CloseServiceHandle(hService);
            }
            ::CloseServiceHandle(hSCM);
        }
        else
            AssertLogRelMsgFailed(("Error: Could not open the service control manager: %u\n", GetLastError()));
        return fResult;
    }
};

/*static*/ CWindowsServiceModule *CWindowsServiceModule::s_pInstance = NULL;


/**
 * Implements COM Module that used within Windows Service.
 *
 * It is derived from ComModule to intercept Unlock() and derived from
 * CWindowsServiceModule to implement Windows Service
 */
class CComServiceModule : public CWindowsServiceModule, public ATL::CComModule
{
private:
    /** Tracks whether Init() has been called for debug purposes. */
    bool m_fInitialized;
    /** Tracks COM init status for no visible purpose other than debugging. */
    bool m_fComInitialized;
    /** Part of the shutdown monitoring logic. */
    bool volatile m_fActivity;
    /** Auto reset event for communicating with the shutdown thread.
     * This is created by startMonitor(). */
    HANDLE m_hEventShutdown;
    /** The main thread ID.
     * The monitorShutdown code needs this to post a WM_QUIT message. */
    DWORD m_dwMainThreadID;

public:
    /** Time for EXE to be idle before shutting down.
     * Can be decreased at system shutdown phase. */
    volatile uint32_t m_cMsShutdownTimeOut;

public:
    /**
     * Constructor.
     *
     * @param cMsShutdownTimeout  Number of milliseconds to idle without clients
     *                            before autoamtically shutting down the service.
     *
     *                            The default is 2 seconds, because VBoxSVC (our
     *                            only client) already does 5 seconds making the
     *                            effective idle time 7 seconds from clients like
     *                            VBoxManage's point of view.  We consider single
     *                            user and development as the dominant usage
     *                            patterns here, not configuration activity by
     *                            multiple users via VBoxManage.
     */
    CComServiceModule(DWORD cMsShutdownTimeout = 2000)
        : m_fInitialized(false)
        , m_fComInitialized(false)
        , m_fActivity(false)
        , m_cMsShutdownTimeOut(cMsShutdownTimeout)
        , m_hEventShutdown(INVALID_HANDLE_VALUE)
        , m_dwMainThreadID(~(DWORD)42)
    {
    }

    /**
     * Initialization function.
     */
    HRESULT init(ATL::_ATL_OBJMAP_ENTRY *p, HINSTANCE h, const GUID *pLibID,
                 wchar_t const *p_wszServiceName, wchar_t const *p_wszDisplayName, wchar_t const *p_wszDescription)
    {
        HRESULT hrc = ATL::CComModule::Init(p, h, pLibID);
        if (SUCCEEDED(hrc))
        {

            // copy service name
            int rc = ::RTUtf16Copy(m_wszServiceName, sizeof(m_wszServiceName), p_wszServiceName);
            AssertRCReturn(rc, E_NOT_SUFFICIENT_BUFFER);
            rc = ::RTUtf16Copy(m_wszServiceDisplayName, sizeof(m_wszServiceDisplayName), p_wszDisplayName);
            AssertRCReturn(rc, E_NOT_SUFFICIENT_BUFFER);
            rc = ::RTUtf16Copy(m_wszServiceDescription, sizeof(m_wszServiceDescription), p_wszDescription);
            AssertRCReturn(rc, E_NOT_SUFFICIENT_BUFFER);

            m_fInitialized = true;
        }

        return hrc;
    }

    /**
     * Overload CAtlModule::Unlock to trigger delayed automatic shutdown action.
     */
    virtual LONG Unlock() throw()
    {
        LONG cLocks = ATL::CComModule::Unlock();
        LogFunc(("Unlock() called. Ref=%d\n", cLocks));
        if (cLocks == 0)
        {
            m_fActivity = true;
            ::SetEvent(m_hEventShutdown); // tell monitor that we transitioned to zero
        }
        return cLocks;
    }

protected:

    bool hasActiveConnection()
    {
        return m_fActivity || GetLockCount() > 0;
    }

    void monitorShutdown() throw()
    {
        for (;;)
        {
            ::WaitForSingleObject(m_hEventShutdown, INFINITE);
            DWORD dwWait;
            do
            {
                m_fActivity = false;
                dwWait = ::WaitForSingleObject(m_hEventShutdown, m_cMsShutdownTimeOut);
            } while (dwWait == WAIT_OBJECT_0);

            /* timed out */
            if (!hasActiveConnection()) /* if no activity let's really bail */
            {
                /*
                 * Disable log rotation at this point, worst case a log file
                 * becomes slightly bigger than it should. Avoids quirks with
                 * log rotation: there might be another API service process
                 * running at this point which would rotate the logs concurrently,
                 * creating a mess.
                 */
                PRTLOGGER pReleaseLogger = ::RTLogRelGetDefaultInstance();
                if (pReleaseLogger)
                {
                    char szDest[1024];
                    int rc = ::RTLogGetDestinations(pReleaseLogger, szDest, sizeof(szDest));
                    if (RT_SUCCESS(rc))
                    {
                        rc = ::RTStrCat(szDest, sizeof(szDest), " nohistory");
                        if (RT_SUCCESS(rc))
                        {
                            rc = ::RTLogDestinations(pReleaseLogger, szDest);
                            AssertRC(rc);
                        }
                    }
                }
                ::CoSuspendClassObjects();
                if (!hasActiveConnection())
                    break;
                LogRel(("Still got active connection(s)...\n"));
            }
        }

        LogRel(("Shutting down\n"));
        if (m_hEventShutdown)
        {
            ::CloseHandle(m_hEventShutdown);
            m_hEventShutdown = NULL;
        }
        ::PostThreadMessage(m_dwMainThreadID, WM_QUIT, 0, 0);
    }

    static DECLCALLBACK(int) monitorThreadProc(RTTHREAD hThreadSelf, void *pvUser) throw()
    {
        RT_NOREF(hThreadSelf);
        CComServiceModule *p = static_cast<CComServiceModule *>(pvUser);
        p->monitorShutdown();
        return VINF_SUCCESS;
    }

    void startMonitor()
    {
        m_dwMainThreadID = ::GetCurrentThreadId();
        m_hEventShutdown = ::CreateEvent(NULL, false, false, NULL);
        AssertLogRelMsg(m_hEventShutdown != NULL, ("GetLastError => %u\n", GetLastError()));

        int vrc = RTThreadCreate(NULL, monitorThreadProc, this, 0 /*cbStack*/, RTTHREADTYPE_DEFAULT, 0 /*fFlags*/, "MonShdwn");
        if (RT_FAILURE(vrc))
        {
            ::CloseHandle(m_hEventShutdown);
            m_hEventShutdown = NULL;
            LogRel(("Error: RTThreadCreate failed to create shutdown monitor thread: %Rrc\n", vrc));
        }
    }

    virtual HRESULT preMessageLoop(int nShowCmd)
    {
        Assert(m_fInitialized);
        LogFunc(("Enter\n"));

        HRESULT hrc = com::Initialize();
        if (SUCCEEDED(hrc))
        {
            m_fComInitialized = true;
            hrc = ATL::CComModule::RegisterClassObjects(CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED);
            if (SUCCEEDED(hrc))
            {
                // Start Shutdown monitor here
                startMonitor();

                hrc = CWindowsServiceModule::preMessageLoop(nShowCmd);
                if (FAILED(hrc))
                    LogRelFunc(("Warning: preMessageLoop failed: %Rhrc\n", hrc));

                hrc = CoResumeClassObjects();
                if (FAILED(hrc))
                {
                    ATL::CComModule::RevokeClassObjects();
                    LogRelFunc(("Error: CoResumeClassObjects failed: %Rhrc\n", hrc));
                }
            }
            else
                LogRel(("Error: ATL::CComModule::RegisterClassObjects: %Rhrc\n", hrc));
        }
        else
            LogRel(("Error: com::Initialize failed\n", hrc));
        return hrc;
    }

    virtual HRESULT postMessageLoop()
    {
        com::Shutdown();
        m_fComInitialized = false;
        return S_OK;
    }
};


/**
 * Main function for the VBoxSDS process.
 *
 * @param   hInstance       The process instance.
 * @param   hPrevInstance   Previous instance (not used here).
 * @param   nShowCmd        The show flags.
 * @param   lpCmdLine       The command line (not used here, we get it from the
 *                          C runtime library).
 *
 * @return  Exit code
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    RT_NOREF(hPrevInstance, lpCmdLine);
    int    argc = __argc;
    char **argv = __argv;

    /*
     * Initialize the VBox runtime without loading the support driver.
     */
    RTR3InitExe(argc, &argv, 0);

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--embedding",    'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-embedding",     'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/embedding",     'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--unregservice", 'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-unregservice",  'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/unregservice",  'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--regservice",   'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-regservice",    'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/regservice",    'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--reregservice", 'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-reregservice",  'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/reregservice",  'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--logfile",      'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "-logfile",       'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "/logfile",       'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "--logrotate",    'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "-logrotate",     'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "/logrotate",     'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "--logsize",      'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "-logsize",       'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "/logsize",       'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "--loginterval",  'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "-loginterval",   'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "/loginterval",   'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
    };

    bool            fRun = true;
    bool            fRegister = false;
    bool            fUnregister = false;
    const char      *pszLogFile = NULL;
    uint32_t        cHistory = 10;                  // enable log rotation, 10 files
    uint32_t        uHistoryFileTime = RT_SEC_1DAY; // max 1 day per file
    uint64_t        uHistoryFileSize = 100 * _1M;   // max 100MB per file

    RTGETOPTSTATE   GetOptState;
    int vrc = RTGetOptInit(&GetOptState, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    AssertRC(vrc);

    RTGETOPTUNION   ValueUnion;
    while ((vrc = RTGetOpt(&GetOptState, &ValueUnion)))
    {
        switch (vrc)
        {
            case 'e':
                break;

            case 'u':
                fUnregister = true;
                fRun = false;
                break;

            case 'r':
                fRegister = true;
                fRun = false;
                break;

            case 'f':
                fUnregister = true;
                fRegister = true;
                fRun = false;
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
            {
                static WCHAR const s_wszHelpText[] =
                    L"Options:\n"
                    L"\n"
                    L"/RegService\t"   L"register COM out-of-proc service\n"
                    L"/UnregService\t" L"unregister COM out-of-proc service\n"
                    L"/ReregService\t" L"unregister and register COM service\n"
                    L"no options\t"    L"run the service";
                MessageBoxW(NULL, s_wszHelpText, L"VBoxSDS - Usage", MB_OK);
                return 0;
            }

            case 'V':
            {
                char *pszText = NULL;
                RTStrAPrintf(&pszText, "%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());

                PRTUTF16 pwszText = NULL;
                RTStrToUtf16(pszText, &pwszText);

                MessageBoxW(NULL, pwszText, L"VBoxSDS - Version", MB_OK);

                RTStrFree(pszText);
                RTUtf16Free(pwszText);
                return 0;
            }

            default:
            {
                char szTmp[256];
                RTGetOptFormatError(szTmp, sizeof(szTmp), vrc, &ValueUnion);

                PRTUTF16 pwszText = NULL;
                RTStrToUtf16(szTmp, &pwszText);

                MessageBoxW(NULL, pwszText, L"VBoxSDS - Syntax error", MB_OK | MB_ICONERROR);

                RTUtf16Free(pwszText);
                return RTEXITCODE_SYNTAX;
            }
        }
    }

    /*
     * Default log location is %ProgramData%\VirtualBox\VBoxSDS.log, falling back
     * on %_CWD%\VBoxSDS.log (where _CWD typicaly is 'C:\Windows\System32').
     *
     * We change the current directory to %ProgramData%\VirtualBox\ if possible.
     *
     * We only create the log file when running VBoxSDS normally, but not
     * when registering/unregistering, at least for now.
     */
    if (fRun)
    {
        char szLogFile[RTPATH_MAX];
        if (!pszLogFile || !*pszLogFile)
        {
            WCHAR wszAppData[MAX_PATH + 16];
            if (SHGetSpecialFolderPathW(NULL, wszAppData, CSIDL_COMMON_APPDATA, TRUE /*fCreate*/))
            {
                char *pszConv = szLogFile;
                vrc = RTUtf16ToUtf8Ex(wszAppData, RTSTR_MAX, &pszConv, sizeof(szLogFile) - 12, NULL);
            }
            else
                vrc = RTEnvGetUtf8("ProgramData", szLogFile, sizeof(szLogFile) - sizeof("VBoxSDS.log"), NULL);
            if (RT_SUCCESS(vrc))
            {
                vrc = RTPathAppend(szLogFile, sizeof(szLogFile), "VirtualBox\\");
                if (RT_SUCCESS(vrc))
                {
                    /* Make sure it exists. */
                    if (!RTDirExists(szLogFile))
                        vrc = RTDirCreate(szLogFile, 0755, RTDIRCREATE_FLAGS_NOT_CONTENT_INDEXED_DONT_SET);
                    if (RT_SUCCESS(vrc))
                    {
                        /* Change into it. */
                        RTPathSetCurrent(szLogFile);
                    }
                }
            }
            if (RT_FAILURE(vrc))     /* ignore any failure above */
                szLogFile[0] = '\0';
            vrc = RTStrCat(szLogFile, sizeof(szLogFile), "VBoxSDS.log");
            if (RT_FAILURE(vrc))
                return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to construct release log filename: %Rrc", vrc);
            pszLogFile = szLogFile;
        }

        RTERRINFOSTATIC ErrInfo;
        vrc = com::VBoxLogRelCreate("COM Service", pszLogFile,
                                    RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                                    VBOXSDS_LOG_DEFAULT, "VBOXSDS_RELEASE_LOG",
                                    RTLOGDEST_FILE, UINT32_MAX /* cMaxEntriesPerGroup */,
                                    cHistory, uHistoryFileTime, uHistoryFileSize,
                                    RTErrInfoInitStatic(&ErrInfo));
        if (RT_FAILURE(vrc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", ErrInfo.Core.pszMsg, vrc);
    }


    /*
     * Initialize COM.
     */
    HRESULT hrcExit = com::Initialize();
    if (SUCCEEDED(hrcExit))
    {
        HRESULT hrcSec = CoInitializeSecurity(NULL,
                                              -1,
                                              NULL,
                                              NULL,
                                              RPC_C_AUTHN_LEVEL_DEFAULT,
                                              RPC_C_IMP_LEVEL_IMPERSONATE,//RPC_C_IMP_LEVEL_IMPERSONATE, RPC_C_IMP_LEVEL_DELEGATE
                                              NULL,
                                              EOAC_NONE, //EOAC_DYNAMIC_CLOAKING,//EOAC_STATIC_CLOAKING, //EOAC_NONE,
                                              NULL);
        LogRelFunc(("VBoxSDS: InitializeSecurity: %x\n", hrcSec));

        /*
         * Instantiate our COM service class.
         */
        CComServiceModule *pServiceModule = new CComServiceModule();
        if (pServiceModule)
        {
            BEGIN_OBJECT_MAP(s_aObjectMap)
                OBJECT_ENTRY(CLSID_VirtualBoxSDS, VirtualBoxSDS)
            END_OBJECT_MAP()
            hrcExit = pServiceModule->init(s_aObjectMap, hInstance, &LIBID_VirtualBox,
                                           L"VBoxSDS",
                                           L"VirtualBox system service",
                                           L"Used as a COM server for VirtualBox API.");

            if (SUCCEEDED(hrcExit))
            {
                if (!fRun)
                {
                    /*
                     * Do registration work and quit.
                     */
                    // The VBoxProxyStub should do all work for COM registration
                    if (fUnregister)
                        hrcExit = pServiceModule->unregisterService();
                    if (fRegister)
                        hrcExit = pServiceModule->registerService();
                }
                else
                {
                    /*
                     * Run service.
                     */
                    hrcExit = pServiceModule->startService(nShowCmd);
                    LogRelFunc(("VBoxSDS: Calling _ServiceModule.RevokeClassObjects()...\n"));
                    pServiceModule->RevokeClassObjects();
                }

                LogRelFunc(("VBoxSDS: Calling _ServiceModule.Term()...\n"));
                pServiceModule->Term();
            }
            else
                LogRelFunc(("VBoxSDS: new CComServiceModule::Init failed: %Rhrc\n", hrcExit));

            LogRelFunc(("VBoxSDS: deleting pServiceModule (%p)\n", pServiceModule));
            delete pServiceModule;
            pServiceModule = NULL;
        }
        else
            LogRelFunc(("VBoxSDS: new CComServiceModule() failed\n"));

        LogRelFunc(("VBoxSDS: Calling com::Shutdown\n"));
        com::Shutdown();
    }
    else
        LogRelFunc(("VBoxSDS: COM initialization failed: %Rrc\n", hrcExit));

    LogRelFunc(("VBoxSDS: COM service process ends: hrcExit=%Rhrc (%#x)\n", hrcExit, hrcExit));
    return (int)hrcExit;
}
