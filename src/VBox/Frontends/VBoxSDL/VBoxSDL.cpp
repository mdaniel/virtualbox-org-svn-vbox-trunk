/* $Id$ */
/** @file
 * VBox frontends: VBoxSDL (simple frontend based on SDL):
 * Main code
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GUI

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/NativeEventQueue.h>
#include <VBox/com/VirtualBox.h>

using namespace com;

#if defined(VBOXSDL_WITH_X11)
# include <VBox/VBoxKeyboard.h>

# include <X11/Xlib.h>
# include <X11/cursorfont.h>      /* for XC_left_ptr */
# if !defined(VBOX_WITHOUT_XCURSOR)
#  include <X11/Xcursor/Xcursor.h>
# endif
# include <unistd.h>
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4121) /* warning C4121: 'SDL_SysWMmsg' : alignment of a member was sensitive to packing*/
#endif
#ifndef RT_OS_DARWIN
# include <SDL_syswm.h>          /* for SDL_GetWMInfo() */
#endif
#ifdef _MSC_VER
# pragma warning(pop)
#endif

#include "VBoxSDL.h"
#include "Framebuffer.h"
#include "Helper.h"

#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include <VBoxVideo.h>
#include <VBox/com/listeners.h>

#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/ldr.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/uuid.h>

#include <signal.h>

#include <vector>
#include <list>

/* Xlib would re-define our enums */
#undef True
#undef False


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#ifdef VBOX_SECURELABEL
/** extra data key for the secure label */
#define VBOXSDL_SECURELABEL_EXTRADATA "VBoxSDL/SecureLabel"
/** label area height in pixels */
#define SECURE_LABEL_HEIGHT 20
#endif

/** Enables the rawr[0|3], patm, and casm options. */
#define VBOXSDL_ADVANCED_OPTIONS


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer shape change event data structure */
struct PointerShapeChangeData
{
    PointerShapeChangeData(BOOL aVisible, BOOL aAlpha, ULONG aXHot, ULONG aYHot,
                           ULONG aWidth, ULONG aHeight, ComSafeArrayIn(BYTE,pShape))
        : visible(aVisible), alpha(aAlpha), xHot(aXHot), yHot(aYHot),
          width(aWidth), height(aHeight)
    {
        // make a copy of the shape
        com::SafeArray<BYTE> aShape(ComSafeArrayInArg(pShape));
        size_t cbShapeSize = aShape.size();
        if (cbShapeSize > 0)
        {
            shape.resize(cbShapeSize);
            ::memcpy(shape.raw(), aShape.raw(), cbShapeSize);
        }
    }

    ~PointerShapeChangeData()
    {
    }

    const BOOL visible;
    const BOOL alpha;
    const ULONG xHot;
    const ULONG yHot;
    const ULONG width;
    const ULONG height;
    com::SafeArray<BYTE> shape;
};

enum TitlebarMode
{
    TITLEBAR_NORMAL   = 1,
    TITLEBAR_STARTUP  = 2,
    TITLEBAR_SAVE     = 3,
    TITLEBAR_SNAPSHOT = 4
};


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static bool    UseAbsoluteMouse(void);
static void    ResetKeys(void);
static void    ProcessKey(SDL_KeyboardEvent *ev);
static void    InputGrabStart(void);
static void    InputGrabEnd(void);
static void    SendMouseEvent(VBoxSDLFB *fb, int dz, int button, int down);
static void    UpdateTitlebar(TitlebarMode mode, uint32_t u32User = 0);
static void    SetPointerShape(const PointerShapeChangeData *data);
static void    HandleGuestCapsChanged(void);
static int     HandleHostKey(const SDL_KeyboardEvent *pEv);
static Uint32  StartupTimer(Uint32 interval, void *param);
static Uint32  ResizeTimer(Uint32 interval, void *param);
static Uint32  QuitTimer(Uint32 interval, void *param);
static int     WaitSDLEvent(SDL_Event *event);
static void    SetFullscreen(bool enable);
static RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd);
static RTEXITCODE settingsPasswordFile(ComPtr<IVirtualBox> virtualBox, const char *pszFilename);

#ifdef VBOX_WITH_SDL13
static VBoxSDLFB * getFbFromWinId(SDL_WindowID id);
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static int gHostKeyMod  = KMOD_RCTRL;
static int gHostKeySym1 = SDLK_RCTRL;
static int gHostKeySym2 = SDLK_UNKNOWN;
static const char *gHostKeyDisabledCombinations = "";
static const char *gpszPidFile;
static BOOL gfGrabbed = FALSE;
static BOOL gfGrabOnMouseClick = TRUE;
static BOOL gfFullscreenResize = FALSE;
static BOOL gfIgnoreNextResize = FALSE;
static BOOL gfAllowFullscreenToggle = TRUE;
static BOOL gfAbsoluteMouseHost = FALSE;
static BOOL gfAbsoluteMouseGuest = FALSE;
static BOOL gfRelativeMouseGuest = TRUE;
static BOOL gfGuestNeedsHostCursor = FALSE;
static BOOL gfOffCursorActive = FALSE;
static BOOL gfGuestNumLockPressed = FALSE;
static BOOL gfGuestCapsLockPressed = FALSE;
static BOOL gfGuestScrollLockPressed = FALSE;
static BOOL gfACPITerm = FALSE;
static BOOL gfXCursorEnabled = FALSE;
static int  gcGuestNumLockAdaptions = 2;
static int  gcGuestCapsLockAdaptions = 2;
static uint32_t gmGuestNormalXRes;
static uint32_t gmGuestNormalYRes;

/** modifier keypress status (scancode as index) */
static uint8_t gaModifiersState[256];

static ComPtr<IMachine> gpMachine;
static ComPtr<IConsole> gpConsole;
static ComPtr<IMachineDebugger> gpMachineDebugger;
static ComPtr<IKeyboard> gpKeyboard;
static ComPtr<IMouse> gpMouse;
ComPtr<IDisplay> gpDisplay;
static ComPtr<IVRDEServer> gpVRDEServer;
static ComPtr<IProgress> gpProgress;

static ULONG       gcMonitors = 1;
static ComObjPtr<VBoxSDLFB> gpFramebuffer[64];
static Bstr gaFramebufferId[64];
static SDL_Cursor *gpDefaultCursor = NULL;
#ifdef VBOXSDL_WITH_X11
static Cursor      gpDefaultOrigX11Cursor;
#endif
static SDL_Cursor *gpCustomCursor = NULL;
#ifndef VBOX_WITH_SDL13
static WMcursor   *gpCustomOrigWMcursor = NULL;
#endif
static SDL_Cursor *gpOffCursor = NULL;
static SDL_TimerID gSdlResizeTimer = NULL;
static SDL_TimerID gSdlQuitTimer = NULL;

#if defined(VBOXSDL_WITH_X11) && !defined(VBOX_WITH_SDL13)
static SDL_SysWMinfo gSdlInfo;
#endif

#ifdef VBOX_SECURELABEL
#ifdef RT_OS_WINDOWS
#define LIBSDL_TTF_NAME "SDL_ttf"
#else
#define LIBSDL_TTF_NAME "libSDL_ttf-2.0.so.0"
#endif
RTLDRMOD gLibrarySDL_ttf = NIL_RTLDRMOD;
#endif

static RTSEMEVENT g_EventSemSDLEvents;
static volatile int32_t g_cNotifyUpdateEventsPending;

/**
 * Event handler for VirtualBoxClient events
 */
class VBoxSDLClientEventListener
{
public:
    VBoxSDLClientEventListener()
    {
    }

    virtual ~VBoxSDLClientEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent * aEvent)
    {
        switch (aType)
        {
            case VBoxEventType_OnVBoxSVCAvailabilityChanged:
            {
                ComPtr<IVBoxSVCAvailabilityChangedEvent> pVSACEv = aEvent;
                Assert(pVSACEv);
                BOOL fAvailable = FALSE;
                pVSACEv->COMGETTER(Available)(&fAvailable);
                if (!fAvailable)
                {
                    LogRel(("VBoxSDL: VBoxSVC became unavailable, exiting.\n"));
                    RTPrintf("VBoxSVC became unavailable, exiting.\n");
                    /* Send QUIT event to terminate the VM as cleanly as possible
                     * given that VBoxSVC is no longer present. */
                    SDL_Event event = {0};
                    event.type = SDL_QUIT;
                    PushSDLEventForSure(&event);
                }
                break;
            }

            default:
                AssertFailed();
        }

        return S_OK;
    }
};

/**
 * Event handler for VirtualBox (server) events
 */
class VBoxSDLEventListener
{
public:
    VBoxSDLEventListener()
    {
    }

    virtual ~VBoxSDLEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent * aEvent)
    {
        RT_NOREF(aEvent);
        switch (aType)
        {
            case VBoxEventType_OnExtraDataChanged:
            {
#ifdef VBOX_SECURELABEL
                ComPtr<IExtraDataChangedEvent> pEDCEv = aEvent;
                Assert(pEDCEv);
                Bstr bstrMachineId;
                pEDCEv->COMGETTER(MachineId)(bstrMachineId.asOutParam());
                if (gpMachine)
                {
                    /*
                     * check if we're interested in the message
                     */
                    Bstr bstrOurId;
                    gpMachine->COMGETTER(Id)(bstrOurId.asOutParam());
                    if (bstrOurId == bstrMachineId)
                    {
                        Bstr bstrKey;
                        pEDCEv->COMGETTER(Key)(bstrKey.asOutParam());
                        if (bstrKey == VBOXSDL_SECURELABEL_EXTRADATA)
                        {
                            /*
                             * Notify SDL thread of the string update
                             */
                            SDL_Event event  = {0};
                            event.type       = SDL_USEREVENT;
                            event.user.type  = SDL_USER_EVENT_SECURELABEL_UPDATE;
                            PushSDLEventForSure(&event);
                        }
                    }
                }
#endif
                break;
            }

            default:
                AssertFailed();
        }

        return S_OK;
    }
};

/**
 * Event handler for Console events
 */
class VBoxSDLConsoleEventListener
{
public:
    VBoxSDLConsoleEventListener() : m_fIgnorePowerOffEvents(false)
    {
    }

    virtual ~VBoxSDLConsoleEventListener()
    {
    }

    HRESULT init()
    {
        return S_OK;
    }

    void uninit()
    {
    }

    STDMETHOD(HandleEvent)(VBoxEventType_T aType, IEvent * aEvent)
    {
        // likely all this double copy is now excessive, and we can just use existing event object
        /// @todo eliminate it
        switch (aType)
        {
            case VBoxEventType_OnMousePointerShapeChanged:
            {
                ComPtr<IMousePointerShapeChangedEvent> pMPSCEv = aEvent;
                Assert(pMPSCEv);
                PointerShapeChangeData *data;
                BOOL    visible,  alpha;
                ULONG   xHot, yHot, width, height;
                com::SafeArray<BYTE> shape;

                pMPSCEv->COMGETTER(Visible)(&visible);
                pMPSCEv->COMGETTER(Alpha)(&alpha);
                pMPSCEv->COMGETTER(Xhot)(&xHot);
                pMPSCEv->COMGETTER(Yhot)(&yHot);
                pMPSCEv->COMGETTER(Width)(&width);
                pMPSCEv->COMGETTER(Height)(&height);
                pMPSCEv->COMGETTER(Shape)(ComSafeArrayAsOutParam(shape));
                data = new PointerShapeChangeData(visible, alpha, xHot, yHot, width, height,
                                                  ComSafeArrayAsInParam(shape));
                Assert(data);
                if (!data)
                    break;

                SDL_Event event  = {0};
                event.type       = SDL_USEREVENT;
                event.user.type  = SDL_USER_EVENT_POINTER_CHANGE;
                event.user.data1 = data;

                int rc = PushSDLEventForSure(&event);
                if (rc)
                    delete data;

                break;
            }
            case VBoxEventType_OnMouseCapabilityChanged:
            {
                ComPtr<IMouseCapabilityChangedEvent> pMCCEv = aEvent;
                Assert(pMCCEv);
                pMCCEv->COMGETTER(SupportsAbsolute)(&gfAbsoluteMouseGuest);
                pMCCEv->COMGETTER(SupportsRelative)(&gfRelativeMouseGuest);
                pMCCEv->COMGETTER(NeedsHostCursor)(&gfGuestNeedsHostCursor);
                SDL_Event event = {0};
                event.type      = SDL_USEREVENT;
                event.user.type = SDL_USER_EVENT_GUEST_CAP_CHANGED;

                PushSDLEventForSure(&event);
                break;
            }
            case VBoxEventType_OnKeyboardLedsChanged:
            {
                ComPtr<IKeyboardLedsChangedEvent> pCLCEv = aEvent;
                Assert(pCLCEv);
                BOOL fNumLock, fCapsLock, fScrollLock;
                pCLCEv->COMGETTER(NumLock)(&fNumLock);
                pCLCEv->COMGETTER(CapsLock)(&fCapsLock);
                pCLCEv->COMGETTER(ScrollLock)(&fScrollLock);
                /* Don't bother the guest with NumLock scancodes if he doesn't set the NumLock LED */
                if (gfGuestNumLockPressed != fNumLock)
                    gcGuestNumLockAdaptions = 2;
                if (gfGuestCapsLockPressed != fCapsLock)
                    gcGuestCapsLockAdaptions = 2;
                gfGuestNumLockPressed    = fNumLock;
                gfGuestCapsLockPressed   = fCapsLock;
                gfGuestScrollLockPressed = fScrollLock;
                break;
            }

            case VBoxEventType_OnStateChanged:
            {
                ComPtr<IStateChangedEvent> pSCEv = aEvent;
                Assert(pSCEv);
                MachineState_T machineState;
                pSCEv->COMGETTER(State)(&machineState);
                LogFlow(("OnStateChange: machineState = %d (%s)\n", machineState, GetStateName(machineState)));
                SDL_Event event = {0};

                if (     machineState == MachineState_Aborted
                         ||   machineState == MachineState_Teleported
                         ||  (machineState == MachineState_Saved      && !m_fIgnorePowerOffEvents)
                         ||  (machineState == MachineState_PoweredOff && !m_fIgnorePowerOffEvents)
                         )
                {
                    /*
                     * We have to inform the SDL thread that the application has be terminated
                     */
                    event.type      = SDL_USEREVENT;
                    event.user.type = SDL_USER_EVENT_TERMINATE;
                    event.user.code = machineState == MachineState_Aborted
                            ? VBOXSDL_TERM_ABEND
                            : VBOXSDL_TERM_NORMAL;
                }
                else
                {
                    /*
                     * Inform the SDL thread to refresh the titlebar
                     */
                    event.type      = SDL_USEREVENT;
                    event.user.type = SDL_USER_EVENT_UPDATE_TITLEBAR;
                }

                PushSDLEventForSure(&event);
                break;
            }

            case VBoxEventType_OnRuntimeError:
            {
                ComPtr<IRuntimeErrorEvent> pRTEEv = aEvent;
                Assert(pRTEEv);
                BOOL fFatal;

                pRTEEv->COMGETTER(Fatal)(&fFatal);
                MachineState_T machineState;
                gpMachine->COMGETTER(State)(&machineState);
                const char *pszType;
                bool fPaused = machineState == MachineState_Paused;
                if (fFatal)
                    pszType = "FATAL ERROR";
                else if (machineState == MachineState_Paused)
                    pszType = "Non-fatal ERROR";
                else
                    pszType = "WARNING";
                Bstr bstrId, bstrMessage;
                pRTEEv->COMGETTER(Id)(bstrId.asOutParam());
                pRTEEv->COMGETTER(Message)(bstrMessage.asOutParam());
                RTPrintf("\n%s: ** %ls **\n%ls\n%s\n", pszType, bstrId.raw(), bstrMessage.raw(),
                         fPaused ? "The VM was paused. Continue with HostKey + P after you solved the problem.\n" : "");
                break;
            }

            case VBoxEventType_OnCanShowWindow:
            {
                ComPtr<ICanShowWindowEvent> pCSWEv = aEvent;
                Assert(pCSWEv);
#ifdef RT_OS_DARWIN
                /* SDL feature not available on Quartz */
#else
                SDL_SysWMinfo info;
                SDL_VERSION(&info.version);
                if (!SDL_GetWMInfo(&info))
                    pCSWEv->AddVeto(NULL);
                else
                    pCSWEv->AddApproval(NULL);
#endif
                break;
            }

            case VBoxEventType_OnShowWindow:
            {
                ComPtr<IShowWindowEvent> pSWEv = aEvent;
                Assert(pSWEv);
                LONG64 winId = 0;
                pSWEv->COMGETTER(WinId)(&winId);
                if (winId != 0)
                    break; /* WinId already set by some other listener. */
#ifndef RT_OS_DARWIN
                SDL_SysWMinfo info;
                SDL_VERSION(&info.version);
                if (SDL_GetWMInfo(&info))
                {
# if defined(VBOXSDL_WITH_X11)
                    pSWEv->COMSETTER(WinId)((LONG64)info.info.x11.wmwindow);
# elif defined(RT_OS_WINDOWS)
                    pSWEv->COMSETTER(WinId)((intptr_t)info.window);
# else
                    AssertFailed();
# endif
                }
#endif /* !RT_OS_DARWIN */
                break;
            }

            default:
                AssertFailed();
        }
        return S_OK;
    }

    static const char *GetStateName(MachineState_T machineState)
    {
        switch (machineState)
        {
            case MachineState_Null:                 return "<null>";
            case MachineState_PoweredOff:           return "PoweredOff";
            case MachineState_Saved:                return "Saved";
            case MachineState_Teleported:           return "Teleported";
            case MachineState_Aborted:              return "Aborted";
            case MachineState_Running:              return "Running";
            case MachineState_Teleporting:          return "Teleporting";
            case MachineState_LiveSnapshotting:     return "LiveSnapshotting";
            case MachineState_Paused:               return "Paused";
            case MachineState_Stuck:                return "GuruMeditation";
            case MachineState_Starting:             return "Starting";
            case MachineState_Stopping:             return "Stopping";
            case MachineState_Saving:               return "Saving";
            case MachineState_Restoring:            return "Restoring";
            case MachineState_TeleportingPausedVM:  return "TeleportingPausedVM";
            case MachineState_TeleportingIn:        return "TeleportingIn";
            case MachineState_RestoringSnapshot:    return "RestoringSnapshot";
            case MachineState_DeletingSnapshot:     return "DeletingSnapshot";
            case MachineState_SettingUp:            return "SettingUp";
            default:                                return "no idea";
        }
    }

    void ignorePowerOffEvents(bool fIgnore)
    {
        m_fIgnorePowerOffEvents = fIgnore;
    }

private:
    bool m_fIgnorePowerOffEvents;
};

typedef ListenerImpl<VBoxSDLClientEventListener>  VBoxSDLClientEventListenerImpl;
typedef ListenerImpl<VBoxSDLEventListener>        VBoxSDLEventListenerImpl;
typedef ListenerImpl<VBoxSDLConsoleEventListener> VBoxSDLConsoleEventListenerImpl;

static void show_usage()
{
    RTPrintf("Usage:\n"
             "  --startvm <uuid|name>    Virtual machine to start, either UUID or name\n"
             "  --separate               Run a separate VM process or attach to a running VM\n"
             "  --hda <file>             Set temporary first hard disk to file\n"
             "  --fda <file>             Set temporary first floppy disk to file\n"
             "  --cdrom <file>           Set temporary CDROM/DVD to file/device ('none' to unmount)\n"
             "  --boot <a|c|d|n>         Set temporary boot device (a = floppy, c = 1st HD, d = DVD, n = network)\n"
             "  --memory <size>          Set temporary memory size in megabytes\n"
             "  --vram <size>            Set temporary size of video memory in megabytes\n"
             "  --fullscreen             Start VM in fullscreen mode\n"
             "  --fullscreenresize       Resize the guest on fullscreen\n"
             "  --fixedmode <w> <h> <bpp> Use a fixed SDL video mode with given width, height and bits per pixel\n"
             "  --nofstoggle             Forbid switching to/from fullscreen mode\n"
             "  --noresize               Make the SDL frame non resizable\n"
             "  --nohostkey              Disable all hostkey combinations\n"
             "  --nohostkeys ...         Disable specific hostkey combinations, see below for valid keys\n"
             "  --nograbonclick          Disable mouse/keyboard grabbing on mouse click w/o additions\n"
             "  --detecthostkey          Get the hostkey identifier and modifier state\n"
             "  --hostkey <key> {<key2>} <mod> Set the host key to the values obtained using --detecthostkey\n"
             "  --termacpi               Send an ACPI power button event when closing the window\n"
             "  --vrdp <ports>           Listen for VRDP connections on one of specified ports (default if not specified)\n"
             "  --discardstate           Discard saved state (if present) and revert to last snapshot (if present)\n"
             "  --settingspw <pw>        Specify the settings password\n"
             "  --settingspwfile <file>  Specify a file containing the settings password\n"
#ifdef VBOX_SECURELABEL
             "  --securelabel            Display a secure VM label at the top of the screen\n"
             "  --seclabelfnt            TrueType (.ttf) font file for secure session label\n"
             "  --seclabelsiz            Font point size for secure session label (default 12)\n"
             "  --seclabelofs            Font offset within the secure label (default 0)\n"
             "  --seclabelfgcol <rgb>    Secure label text color RGB value in 6 digit hexadecimal (eg: FFFF00)\n"
             "  --seclabelbgcol <rgb>    Secure label background color RGB value in 6 digit hexadecimal (eg: FF0000)\n"
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
             "  --[no]rawr0              Enable or disable raw ring 3\n"
             "  --[no]rawr3              Enable or disable raw ring 0\n"
             "  --[no]patm               Enable or disable PATM\n"
             "  --[no]csam               Enable or disable CSAM\n"
             "  --[no]hwvirtex           Permit or deny the usage of VT-x/AMD-V\n"
#endif
             "\n"
             "Key bindings:\n"
             "  <hostkey> +  f           Switch to full screen / restore to previous view\n"
             "               h           Press ACPI power button\n"
             "               n           Take a snapshot and continue execution\n"
             "               p           Pause / resume execution\n"
             "               q           Power off\n"
             "               r           VM reset\n"
             "               s           Save state and power off\n"
             "              <del>        Send <ctrl><alt><del>\n"
             "       <F1>...<F12>        Send <ctrl><alt><Fx>\n"
#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
             "\n"
             "Further key bindings useful for debugging:\n"
             "  LCtrl + Alt + F12        Reset statistics counter\n"
             "  LCtrl + Alt + F11        Dump statistics to logfile\n"
             "  Alt         + F12        Toggle R0 recompiler\n"
             "  Alt         + F11        Toggle R3 recompiler\n"
             "  Alt         + F10        Toggle PATM\n"
             "  Alt         + F9         Toggle CSAM\n"
             "  Alt         + F8         Toggle single step mode\n"
             "  LCtrl/RCtrl + F12        Toggle logger\n"
             "  F12                      Write log marker to logfile\n"
#endif
             "\n");
}

static void PrintError(const char *pszName, CBSTR pwszDescr, CBSTR pwszComponent=NULL)
{
    const char *pszFile, *pszFunc, *pszStat;
    char  pszBuffer[1024];
    com::ErrorInfo info;

    RTStrPrintf(pszBuffer, sizeof(pszBuffer), "%ls", pwszDescr);

    RTPrintf("\n%s! Error info:\n", pszName);
    if (   (pszFile = strstr(pszBuffer, "At '"))
        && (pszFunc = strstr(pszBuffer, ") in "))
        && (pszStat = strstr(pszBuffer, "VBox status code: ")))
        RTPrintf("  %.*s  %.*s\n  In%.*s  %s",
                 pszFile-pszBuffer, pszBuffer,
                 pszFunc-pszFile+1, pszFile,
                 pszStat-pszFunc-4, pszFunc+4,
                 pszStat);
    else
        RTPrintf("%s\n", pszBuffer);

    if (pwszComponent)
        RTPrintf("(component %ls).\n", pwszComponent);

    RTPrintf("\n");
}

#ifdef VBOXSDL_WITH_X11
/**
 * Custom signal handler. Currently it is only used to release modifier
 * keys when receiving the USR1 signal. When switching VTs, we might not
 * get release events for Ctrl-Alt and in case a savestate is performed
 * on the new VT, the VM will be saved with modifier keys stuck. This is
 * annoying enough for introducing this hack.
 */
void signal_handler_SIGUSR1(int sig, siginfo_t *info, void *secret)
{
    RT_NOREF(info, secret);

    /* only SIGUSR1 is interesting */
    if (sig == SIGUSR1)
    {
        /* just release the modifiers */
        ResetKeys();
    }
}

/**
 * Custom signal handler for catching exit events.
 */
void signal_handler_SIGINT(int sig)
{
    if (gpszPidFile)
        RTFileDelete(gpszPidFile);
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    kill(getpid(), sig);
}
#endif /* VBOXSDL_WITH_X11 */


/** entry point */
extern "C"
DECLEXPORT(int) TrustedMain(int argc, char **argv, char **envp)
{
    RT_NOREF(envp);
#ifdef RT_OS_WINDOWS
    ATL::CComModule _Module; /* Required internally by ATL (constructor records instance in global variable). */
#endif

#ifdef Q_WS_X11
    if (!XInitThreads())
        return 1;
#endif
#ifdef VBOXSDL_WITH_X11
    /*
     * Lock keys on SDL behave different from normal keys: A KeyPress event is generated
     * if the lock mode gets active and a keyRelease event is generated if the lock mode
     * gets inactive, that is KeyPress and KeyRelease are sent when pressing the lock key
     * to change the mode. The current lock mode is reflected in SDL_GetModState().
     *
     * Debian patched libSDL to make the lock keys behave like normal keys
     * generating a KeyPress/KeyRelease event if the lock key was
     * pressed/released.  With the new behaviour, the lock status is not
     * reflected in the mod status anymore, but the user can request the old
     * behaviour by setting an environment variable.  To confuse matters further
     * version 1.2.14 (fortunately including the Debian packaged versions)
     * adopted the Debian behaviour officially, but inverted the meaning of the
     * environment variable to select the new behaviour, keeping the old as the
     * default.  We disable the new behaviour to ensure a defined environment
     * and work around the missing KeyPress/KeyRelease events in ProcessKeys().
     */
    {
        const SDL_version *pVersion = SDL_Linked_Version();
        if (  SDL_VERSIONNUM(pVersion->major, pVersion->minor, pVersion->patch)
            < SDL_VERSIONNUM(1, 2, 14))
            RTEnvSet("SDL_DISABLE_LOCK_KEYS", "1");
    }
#endif

    /*
     * the hostkey detection mode is unrelated to VM processing, so handle it before
     * we initialize anything COM related
     */
    if (argc == 2 && (   !strcmp(argv[1], "-detecthostkey")
                      || !strcmp(argv[1], "--detecthostkey")))
    {
        int rc = SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE);
        if (rc != 0)
        {
            RTPrintf("Error: SDL_InitSubSystem failed with message '%s'\n", SDL_GetError());
            return 1;
        }
        /* we need a video window for the keyboard stuff to work */
        if (!SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE))
        {
            RTPrintf("Error: could not set SDL video mode\n");
            return 1;
        }

        RTPrintf("Please hit one or two function key(s) to get the --hostkey value...\n");

        SDL_Event event1;
        while (SDL_WaitEvent(&event1))
        {
            if (event1.type == SDL_KEYDOWN)
            {
                SDL_Event event2;
                unsigned  mod = SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED);
                while (SDL_WaitEvent(&event2))
                {
                    if (event2.type == SDL_KEYDOWN || event2.type == SDL_KEYUP)
                    {
                        /* pressed additional host key */
                        RTPrintf("--hostkey %d", event1.key.keysym.sym);
                        if (event2.type == SDL_KEYDOWN)
                        {
                            RTPrintf(" %d", event2.key.keysym.sym);
                            RTPrintf(" %d\n", SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED));
                        }
                        else
                        {
                            RTPrintf(" %d\n", mod);
                        }
                        /* we're done */
                        break;
                    }
                }
                /* we're down */
                break;
            }
        }
        SDL_Quit();
        return 1;
    }

    HRESULT rc;
    int vrc;
    Guid uuidVM;
    char *vmName = NULL;
    bool fSeparate = false;
    DeviceType_T bootDevice = DeviceType_Null;
    uint32_t memorySize = 0;
    uint32_t vramSize = 0;
    ComPtr<IEventListener> pVBoxClientListener;
    ComPtr<IEventListener> pVBoxListener;
    ComObjPtr<VBoxSDLConsoleEventListenerImpl> pConsoleListener;

    bool fFullscreen = false;
    bool fResizable = true;
#ifdef USE_XPCOM_QUEUE_THREAD
    bool fXPCOMEventThreadSignaled = false;
#endif
    const char *pcszHdaFile   = NULL;
    const char *pcszCdromFile = NULL;
    const char *pcszFdaFile   = NULL;
    const char *pszPortVRDP = NULL;
    bool fDiscardState = false;
    const char *pcszSettingsPw = NULL;
    const char *pcszSettingsPwFile = NULL;
#ifdef VBOX_SECURELABEL
    BOOL fSecureLabel = false;
    uint32_t secureLabelPointSize = 12;
    uint32_t secureLabelFontOffs = 0;
    char *secureLabelFontFile = NULL;
    uint32_t secureLabelColorFG = 0x0000FF00;
    uint32_t secureLabelColorBG = 0x00FFFF00;
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
    unsigned fRawR0 = ~0U;
    unsigned fRawR3 = ~0U;
    unsigned fPATM  = ~0U;
    unsigned fCSAM  = ~0U;
    unsigned fHWVirt = ~0U;
    uint32_t u32WarpDrive = 0;
#endif
#ifdef VBOX_WIN32_UI
    bool fWin32UI = true;
    int64_t winId = 0;
#endif
    bool fShowSDLConfig    = false;
    uint32_t fixedWidth    = ~(uint32_t)0;
    uint32_t fixedHeight   = ~(uint32_t)0;
    uint32_t fixedBPP      = ~(uint32_t)0;
    uint32_t uResizeWidth  = ~(uint32_t)0;
    uint32_t uResizeHeight = ~(uint32_t)0;

    /* The damned GOTOs forces this to be up here - totally out of place. */
    /*
     * Host key handling.
     *
     * The golden rule is that host-key combinations should not be seen
     * by the guest. For instance a CAD should not have any extra RCtrl down
     * and RCtrl up around itself. Nor should a resume be followed by a Ctrl-P
     * that could encourage applications to start printing.
     *
     * We must not confuse the hostkey processing into any release sequences
     * either, the host key is supposed to be explicitly pressing one key.
     *
     * Quick state diagram:
     *
     *            host key down alone
     *  (Normal) ---------------
     *    ^ ^                  |
     *    | |                  v          host combination key down
     *    | |            (Host key down) ----------------
     *    | | host key up v    |                        |
     *    | |--------------    | other key down         v           host combination key down
     *    |                    |                  (host key used) -------------
     *    |                    |                        |      ^              |
     *    |              (not host key)--               |      |---------------
     *    |                    |     |  |               |
     *    |                    |     ---- other         |
     *    |  modifiers = 0     v                        v
     *    -----------------------------------------------
     */
    enum HKEYSTATE
    {
        /** The initial and most common state, pass keystrokes to the guest.
         * Next state: HKEYSTATE_DOWN
         * Prev state: Any */
        HKEYSTATE_NORMAL = 1,
        /** The first host key was pressed down
         */
        HKEYSTATE_DOWN_1ST,
        /** The second host key was pressed down (if gHostKeySym2 != SDLK_UNKNOWN)
         */
        HKEYSTATE_DOWN_2ND,
        /** The host key has been pressed down.
         * Prev state: HKEYSTATE_NORMAL
         * Next state: HKEYSTATE_NORMAL - host key up, capture toggle.
         * Next state: HKEYSTATE_USED   - host key combination down.
         * Next state: HKEYSTATE_NOT_IT - non-host key combination down.
         */
        HKEYSTATE_DOWN,
        /** A host key combination was pressed.
         * Prev state: HKEYSTATE_DOWN
         * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
         */
        HKEYSTATE_USED,
        /** A non-host key combination was attempted. Send hostkey down to the
         * guest and continue until all modifiers have been released.
         * Prev state: HKEYSTATE_DOWN
         * Next state: HKEYSTATE_NORMAL - when modifiers are all 0
         */
        HKEYSTATE_NOT_IT
    } enmHKeyState = HKEYSTATE_NORMAL;
    /** The host key down event which we have been hiding from the guest.
     * Used when going from HKEYSTATE_DOWN to HKEYSTATE_NOT_IT. */
    SDL_Event EvHKeyDown1;
    SDL_Event EvHKeyDown2;

    LogFlow(("SDL GUI started\n"));
    RTPrintf(VBOX_PRODUCT " SDL GUI version %s\n"
             "(C) 2005-" VBOX_C_YEAR " " VBOX_VENDOR "\n"
             "All rights reserved.\n\n",
             VBOX_VERSION_STRING);

    // less than one parameter is not possible
    if (argc < 2)
    {
        show_usage();
        return 1;
    }

    // command line argument parsing stuff
    for (int curArg = 1; curArg < argc; curArg++)
    {
        if (   !strcmp(argv[curArg], "--vm")
            || !strcmp(argv[curArg], "-vm")
            || !strcmp(argv[curArg], "--startvm")
            || !strcmp(argv[curArg], "-startvm")
            || !strcmp(argv[curArg], "-s")
            )
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: VM not specified (UUID or name)!\n");
                return 1;
            }
            // first check if a UUID was supplied
            uuidVM = argv[curArg];

            if (!uuidVM.isValid())
            {
                LogFlow(("invalid UUID format, assuming it's a VM name\n"));
                vmName = argv[curArg];
            }
            else if (uuidVM.isZero())
            {
                RTPrintf("Error: UUID argument is zero!\n");
                return 1;
            }
        }
        else if (   !strcmp(argv[curArg], "--separate")
                 || !strcmp(argv[curArg], "-separate"))
        {
            fSeparate = true;
        }
        else if (   !strcmp(argv[curArg], "--comment")
                 || !strcmp(argv[curArg], "-comment"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for comment!\n");
                return 1;
            }
        }
        else if (   !strcmp(argv[curArg], "--boot")
                 || !strcmp(argv[curArg], "-boot"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for boot drive!\n");
                return 1;
            }
            switch (argv[curArg][0])
            {
                case 'a':
                {
                    bootDevice = DeviceType_Floppy;
                    break;
                }

                case 'c':
                {
                    bootDevice = DeviceType_HardDisk;
                    break;
                }

                case 'd':
                {
                    bootDevice = DeviceType_DVD;
                    break;
                }

                case 'n':
                {
                    bootDevice = DeviceType_Network;
                    break;
                }

                default:
                {
                    RTPrintf("Error: wrong argument for boot drive!\n");
                    return 1;
                }
            }
        }
        else if (   !strcmp(argv[curArg], "--detecthostkey")
                 || !strcmp(argv[curArg], "-detecthostkey"))
        {
            RTPrintf("Error: please specify \"%s\" without any additional parameters!\n",
                     argv[curArg]);
            return 1;
        }
        else if (   !strcmp(argv[curArg], "--memory")
                 || !strcmp(argv[curArg], "-memory")
                 || !strcmp(argv[curArg], "-m"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for memory size!\n");
                return 1;
            }
            memorySize = atoi(argv[curArg]);
        }
        else if (   !strcmp(argv[curArg], "--vram")
                 || !strcmp(argv[curArg], "-vram"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing argument for vram size!\n");
                return 1;
            }
            vramSize = atoi(argv[curArg]);
        }
        else if (   !strcmp(argv[curArg], "--fullscreen")
                 || !strcmp(argv[curArg], "-fullscreen"))
        {
            fFullscreen = true;
        }
        else if (   !strcmp(argv[curArg], "--fullscreenresize")
                 || !strcmp(argv[curArg], "-fullscreenresize"))
        {
            gfFullscreenResize = true;
#ifdef VBOXSDL_WITH_X11
            RTEnvSet("SDL_VIDEO_X11_VIDMODE", "0");
#endif
        }
        else if (   !strcmp(argv[curArg], "--fixedmode")
                 || !strcmp(argv[curArg], "-fixedmode"))
        {
            /* three parameters follow */
            if (curArg + 3 >= argc)
            {
                RTPrintf("Error: missing arguments for fixed video mode!\n");
                return 1;
            }
            fixedWidth  = atoi(argv[++curArg]);
            fixedHeight = atoi(argv[++curArg]);
            fixedBPP    = atoi(argv[++curArg]);
        }
        else if (   !strcmp(argv[curArg], "--nofstoggle")
                 || !strcmp(argv[curArg], "-nofstoggle"))
        {
            gfAllowFullscreenToggle = FALSE;
        }
        else if (   !strcmp(argv[curArg], "--noresize")
                 || !strcmp(argv[curArg], "-noresize"))
        {
            fResizable = false;
        }
        else if (   !strcmp(argv[curArg], "--nohostkey")
                 || !strcmp(argv[curArg], "-nohostkey"))
        {
            gHostKeyMod  = 0;
            gHostKeySym1 = 0;
        }
        else if (   !strcmp(argv[curArg], "--nohostkeys")
                 || !strcmp(argv[curArg], "-nohostkeys"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing a string of disabled hostkey combinations\n");
                return 1;
            }
            gHostKeyDisabledCombinations = argv[curArg];
            size_t cch = strlen(gHostKeyDisabledCombinations);
            for (size_t i = 0; i < cch; i++)
            {
                if (!strchr("fhnpqrs", gHostKeyDisabledCombinations[i]))
                {
                    RTPrintf("Error: <hostkey> + '%c' is not a valid combination\n",
                             gHostKeyDisabledCombinations[i]);
                    return 1;
                }
            }
        }
        else if (   !strcmp(argv[curArg], "--nograbonclick")
                 || !strcmp(argv[curArg], "-nograbonclick"))
        {
            gfGrabOnMouseClick = FALSE;
        }
        else if (   !strcmp(argv[curArg], "--termacpi")
                 || !strcmp(argv[curArg], "-termacpi"))
        {
            gfACPITerm = TRUE;
        }
        else if (   !strcmp(argv[curArg], "--pidfile")
                 || !strcmp(argv[curArg], "-pidfile"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file name for --pidfile!\n");
                return 1;
            }
            gpszPidFile = argv[curArg];
        }
        else if (   !strcmp(argv[curArg], "--hda")
                 || !strcmp(argv[curArg], "-hda"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file name for first hard disk!\n");
                return 1;
            }
            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                pcszHdaFile = RTPathRealDup(argv[curArg]);
            if (!pcszHdaFile)
            {
                RTPrintf("Error: The path to the specified harddisk, '%s', could not be resolved.\n", argv[curArg]);
                return 1;
            }
        }
        else if (   !strcmp(argv[curArg], "--fda")
                 || !strcmp(argv[curArg], "-fda"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file/device name for first floppy disk!\n");
                return 1;
            }
            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                pcszFdaFile = RTPathRealDup(argv[curArg]);
            if (!pcszFdaFile)
            {
                RTPrintf("Error: The path to the specified floppy disk, '%s', could not be resolved.\n", argv[curArg]);
                return 1;
            }
        }
        else if (   !strcmp(argv[curArg], "--cdrom")
                 || !strcmp(argv[curArg], "-cdrom"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing file/device name for cdrom!\n");
                return 1;
            }
            /* resolve it. */
            if (RTPathExists(argv[curArg]))
                pcszCdromFile = RTPathRealDup(argv[curArg]);
            if (!pcszCdromFile)
            {
                RTPrintf("Error: The path to the specified cdrom, '%s', could not be resolved.\n", argv[curArg]);
                return 1;
            }
        }
        else if (   !strcmp(argv[curArg], "--vrdp")
                 || !strcmp(argv[curArg], "-vrdp"))
        {
            // start with the standard VRDP port
            pszPortVRDP = "0";

            // is there another argument
            if (argc > (curArg + 1))
            {
                curArg++;
                pszPortVRDP = argv[curArg];
                LogFlow(("Using non standard VRDP port %s\n", pszPortVRDP));
            }
        }
        else if (   !strcmp(argv[curArg], "--discardstate")
                 || !strcmp(argv[curArg], "-discardstate"))
        {
            fDiscardState = true;
        }
        else if (!strcmp(argv[curArg], "--settingspw"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing password");
                return 1;
            }
            pcszSettingsPw = argv[curArg];
        }
        else if (!strcmp(argv[curArg], "--settingspwfile"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing password file\n");
                return 1;
            }
            pcszSettingsPwFile = argv[curArg];
        }
#ifdef VBOX_SECURELABEL
        else if (   !strcmp(argv[curArg], "--securelabel")
                 || !strcmp(argv[curArg], "-securelabel"))
        {
            fSecureLabel = true;
            LogFlow(("Secure labelling turned on\n"));
        }
        else if (   !strcmp(argv[curArg], "--seclabelfnt")
                 || !strcmp(argv[curArg], "-seclabelfnt"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing font file name for secure label!\n");
                return 1;
            }
            secureLabelFontFile = argv[curArg];
        }
        else if (   !strcmp(argv[curArg], "--seclabelsiz")
                 || !strcmp(argv[curArg], "-seclabelsiz"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing font point size for secure label!\n");
                return 1;
            }
            secureLabelPointSize = atoi(argv[curArg]);
        }
        else if (   !strcmp(argv[curArg], "--seclabelofs")
                 || !strcmp(argv[curArg], "-seclabelofs"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing font pixel offset for secure label!\n");
                return 1;
            }
            secureLabelFontOffs = atoi(argv[curArg]);
        }
        else if (   !strcmp(argv[curArg], "--seclabelfgcol")
                 || !strcmp(argv[curArg], "-seclabelfgcol"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing text color value for secure label!\n");
                return 1;
            }
            sscanf(argv[curArg], "%X", &secureLabelColorFG);
        }
        else if (   !strcmp(argv[curArg], "--seclabelbgcol")
                 || !strcmp(argv[curArg], "-seclabelbgcol"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing background color value for secure label!\n");
                return 1;
            }
            sscanf(argv[curArg], "%X", &secureLabelColorBG);
        }
#endif
#ifdef VBOXSDL_ADVANCED_OPTIONS
        else if (   !strcmp(argv[curArg], "--rawr0")
                 || !strcmp(argv[curArg], "-rawr0"))
            fRawR0 = true;
        else if (   !strcmp(argv[curArg], "--norawr0")
                 || !strcmp(argv[curArg], "-norawr0"))
            fRawR0 = false;
        else if (   !strcmp(argv[curArg], "--rawr3")
                 || !strcmp(argv[curArg], "-rawr3"))
            fRawR3 = true;
        else if (   !strcmp(argv[curArg], "--norawr3")
                 || !strcmp(argv[curArg], "-norawr3"))
            fRawR3 = false;
        else if (   !strcmp(argv[curArg], "--patm")
                 || !strcmp(argv[curArg], "-patm"))
            fPATM = true;
        else if (   !strcmp(argv[curArg], "--nopatm")
                 || !strcmp(argv[curArg], "-nopatm"))
            fPATM = false;
        else if (   !strcmp(argv[curArg], "--csam")
                 || !strcmp(argv[curArg], "-csam"))
            fCSAM = true;
        else if (   !strcmp(argv[curArg], "--nocsam")
                 || !strcmp(argv[curArg], "-nocsam"))
            fCSAM = false;
        else if (   !strcmp(argv[curArg], "--hwvirtex")
                 || !strcmp(argv[curArg], "-hwvirtex"))
            fHWVirt = true;
        else if (   !strcmp(argv[curArg], "--nohwvirtex")
                 || !strcmp(argv[curArg], "-nohwvirtex"))
            fHWVirt = false;
        else if (   !strcmp(argv[curArg], "--warpdrive")
                 || !strcmp(argv[curArg], "-warpdrive"))
        {
            if (++curArg >= argc)
            {
                RTPrintf("Error: missing the rate value for the --warpdrive option!\n");
                return 1;
            }
            u32WarpDrive = RTStrToUInt32(argv[curArg]);
            if (u32WarpDrive < 2 || u32WarpDrive > 20000)
            {
                RTPrintf("Error: the warp drive rate is restricted to [2..20000]. (%d)\n", u32WarpDrive);
                return 1;
            }
        }
#endif /* VBOXSDL_ADVANCED_OPTIONS */
#ifdef VBOX_WIN32_UI
        else if (   !strcmp(argv[curArg], "--win32ui")
                 || !strcmp(argv[curArg], "-win32ui"))
            fWin32UI = true;
#endif
        else if (   !strcmp(argv[curArg], "--showsdlconfig")
                 || !strcmp(argv[curArg], "-showsdlconfig"))
            fShowSDLConfig = true;
        else if (   !strcmp(argv[curArg], "--hostkey")
                 || !strcmp(argv[curArg], "-hostkey"))
        {
            if (++curArg + 1 >= argc)
            {
                RTPrintf("Error: not enough arguments for host keys!\n");
                return 1;
            }
            gHostKeySym1 = atoi(argv[curArg++]);
            if (curArg + 1 < argc && (argv[curArg+1][0] == '0' || atoi(argv[curArg+1]) > 0))
            {
                /* two-key sequence as host key specified */
                gHostKeySym2 = atoi(argv[curArg++]);
            }
            gHostKeyMod = atoi(argv[curArg]);
        }
        /* just show the help screen */
        else
        {
            if (   strcmp(argv[curArg], "-h")
                && strcmp(argv[curArg], "-help")
                && strcmp(argv[curArg], "--help"))
                RTPrintf("Error: unrecognized switch '%s'\n", argv[curArg]);
            show_usage();
            return 1;
        }
    }

    rc = com::Initialize();
#ifdef VBOX_WITH_XPCOM
    if (rc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        RTPrintf("Failed to initialize COM because the global settings directory '%s' is not accessible!\n", szHome);
        return 1;
    }
#endif
    if (FAILED(rc))
    {
        RTPrintf("Error: COM initialization failed (rc=%Rhrc)!\n", rc);
        return 1;
    }

    /* NOTE: do not convert the following scope to a "do {} while (0);", as
     * this would make it all too tempting to use "break;" incorrectly - it
     * would skip over the cleanup. */
    {
    // scopes all the stuff till shutdown
    ////////////////////////////////////////////////////////////////////////////

    ComPtr<IVirtualBoxClient> pVirtualBoxClient;
    ComPtr<IVirtualBox> pVirtualBox;
    ComPtr<ISession> pSession;
    bool sessionOpened = false;
    NativeEventQueue* eventQ = com::NativeEventQueue::getMainEventQueue();

    ComPtr<IMachine> pMachine;

    rc = pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Failed to create VirtualBoxClient object",
                       info.getText().raw(), info.getComponent().raw());
        else
            RTPrintf("Failed to create VirtualBoxClient object! No error information available (rc=%Rhrc).\n", rc);
        goto leave;
    }

    rc = pVirtualBoxClient->COMGETTER(VirtualBox)(pVirtualBox.asOutParam());
    if (FAILED(rc))
    {
        RTPrintf("Failed to get VirtualBox object (rc=%Rhrc)!\n", rc);
        goto leave;
    }
    rc = pVirtualBoxClient->COMGETTER(Session)(pSession.asOutParam());
    if (FAILED(rc))
    {
        RTPrintf("Failed to get session object (rc=%Rhrc)!\n", rc);
        goto leave;
    }

    if (pcszSettingsPw)
    {
        CHECK_ERROR(pVirtualBox, SetSettingsSecret(Bstr(pcszSettingsPw).raw()));
        if (FAILED(rc))
            goto leave;
    }
    else if (pcszSettingsPwFile)
    {
        int rcExit = settingsPasswordFile(pVirtualBox, pcszSettingsPwFile);
        if (rcExit != RTEXITCODE_SUCCESS)
            goto leave;
    }

    /*
     * Do we have a UUID?
     */
    if (uuidVM.isValid())
    {
        rc = pVirtualBox->FindMachine(uuidVM.toUtf16().raw(), pMachine.asOutParam());
        if (FAILED(rc) || !pMachine)
        {
            RTPrintf("Error: machine with the given ID not found!\n");
            goto leave;
        }
    }
    else if (vmName)
    {
        /*
         * Do we have a name but no UUID?
         */
        rc = pVirtualBox->FindMachine(Bstr(vmName).raw(), pMachine.asOutParam());
        if ((rc == S_OK) && pMachine)
        {
            Bstr bstrId;
            pMachine->COMGETTER(Id)(bstrId.asOutParam());
            uuidVM = Guid(bstrId);
        }
        else
        {
            RTPrintf("Error: machine with the given name not found!\n");
            RTPrintf("Check if this VM has been corrupted and is now inaccessible.");
            goto leave;
        }
    }

    /* create SDL event semaphore */
    vrc = RTSemEventCreate(&g_EventSemSDLEvents);
    AssertReleaseRC(vrc);

    rc = pVirtualBoxClient->CheckMachineError(pMachine);
    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("The VM has errors",
                       info.getText().raw(), info.getComponent().raw());
        else
            RTPrintf("Failed to check for VM errors! No error information available (rc=%Rhrc).\n", rc);
        goto leave;
    }

    if (fSeparate)
    {
        MachineState_T machineState = MachineState_Null;
        pMachine->COMGETTER(State)(&machineState);
        if (   machineState == MachineState_Running
            || machineState == MachineState_Teleporting
            || machineState == MachineState_LiveSnapshotting
            || machineState == MachineState_Paused
            || machineState == MachineState_TeleportingPausedVM
           )
        {
            RTPrintf("VM is already running.\n");
        }
        else
        {
            ComPtr<IProgress> progress;
            rc = pMachine->LaunchVMProcess(pSession, Bstr("headless").raw(), NULL, progress.asOutParam());
            if (SUCCEEDED(rc) && !progress.isNull())
            {
                RTPrintf("Waiting for VM to power on...\n");
                rc = progress->WaitForCompletion(-1);
                if (SUCCEEDED(rc))
                {
                    BOOL completed = true;
                    rc = progress->COMGETTER(Completed)(&completed);
                    if (SUCCEEDED(rc))
                    {
                        LONG iRc;
                        rc = progress->COMGETTER(ResultCode)(&iRc);
                        if (SUCCEEDED(rc))
                        {
                            if (FAILED(iRc))
                            {
                                ProgressErrorInfo info(progress);
                                com::GluePrintErrorInfo(info);
                            }
                            else
                            {
                                RTPrintf("VM has been successfully started.\n");
                                /* LaunchVMProcess obtains a shared lock on the machine.
                                 * Unlock it here, because the lock will be obtained below
                                 * in the common code path as for already running VM.
                                 */
                                pSession->UnlockMachine();
                            }
                        }
                    }
                }
            }
        }
        if (FAILED(rc))
        {
            RTPrintf("Error: failed to power up VM! No error text available.\n");
            goto leave;
        }

        rc = pMachine->LockMachine(pSession, LockType_Shared);
    }
    else
    {
        pSession->COMSETTER(Name)(Bstr("GUI/SDL").raw());
        rc = pMachine->LockMachine(pSession, LockType_VM);
    }

    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Could not open VirtualBox session",
                       info.getText().raw(), info.getComponent().raw());
        goto leave;
    }
    if (!pSession)
    {
        RTPrintf("Could not open VirtualBox session!\n");
        goto leave;
    }
    sessionOpened = true;
    // get the mutable VM we're dealing with
    pSession->COMGETTER(Machine)(gpMachine.asOutParam());
    if (!gpMachine)
    {
        com::ErrorInfo info;
        if (info.isFullAvailable())
            PrintError("Cannot start VM!",
                       info.getText().raw(), info.getComponent().raw());
        else
            RTPrintf("Error: given machine not found!\n");
        goto leave;
    }

    // get the VM console
    pSession->COMGETTER(Console)(gpConsole.asOutParam());
    if (!gpConsole)
    {
        RTPrintf("Given console not found!\n");
        goto leave;
    }

    /*
     * Are we supposed to use a different hard disk file?
     */
    if (pcszHdaFile)
    {
        ComPtr<IMedium> pMedium;

        /*
         * Strategy: if any registered hard disk points to the same file,
         * assign it. If not, register a new image and assign it to the VM.
         */
        Bstr bstrHdaFile(pcszHdaFile);
        pVirtualBox->OpenMedium(bstrHdaFile.raw(), DeviceType_HardDisk,
                                AccessMode_ReadWrite, FALSE /* fForceNewUuid */,
                                pMedium.asOutParam());
        if (!pMedium)
        {
            /* we've not found the image */
            RTPrintf("Adding hard disk '%s'...\n", pcszHdaFile);
            pVirtualBox->OpenMedium(bstrHdaFile.raw(), DeviceType_HardDisk,
                                    AccessMode_ReadWrite, FALSE /* fForceNewUuid */,
                                    pMedium.asOutParam());
        }
        /* do we have the right image now? */
        if (pMedium)
        {
            Bstr bstrSCName;

            /* get the first IDE controller to attach the harddisk to
             * and if there is none, add one temporarily */
            {
                ComPtr<IStorageController> pStorageCtl;
                com::SafeIfaceArray<IStorageController> aStorageControllers;
                CHECK_ERROR(gpMachine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(aStorageControllers)));
                for (size_t i = 0; i < aStorageControllers.size(); ++ i)
                {
                    StorageBus_T storageBus = StorageBus_Null;

                    CHECK_ERROR(aStorageControllers[i], COMGETTER(Bus)(&storageBus));
                    if (storageBus == StorageBus_IDE)
                    {
                        pStorageCtl = aStorageControllers[i];
                        break;
                    }
                }

                if (pStorageCtl)
                {
                    CHECK_ERROR(pStorageCtl, COMGETTER(Name)(bstrSCName.asOutParam()));
                    gpMachine->DetachDevice(bstrSCName.raw(), 0, 0);
                }
                else
                {
                    bstrSCName = "IDE Controller";
                    CHECK_ERROR(gpMachine, AddStorageController(bstrSCName.raw(),
                                                                StorageBus_IDE,
                                                                pStorageCtl.asOutParam()));
                }
            }

            CHECK_ERROR(gpMachine, AttachDevice(bstrSCName.raw(), 0, 0,
                                                DeviceType_HardDisk, pMedium));
            /// @todo why is this attachment saved?
        }
        else
        {
            RTPrintf("Error: failed to mount the specified hard disk image!\n");
            goto leave;
        }
    }

    /*
     * Mount a floppy if requested.
     */
    if (pcszFdaFile)
    do
    {
        ComPtr<IMedium> pMedium;

        /* unmount? */
        if (!strcmp(pcszFdaFile, "none"))
        {
            /* nothing to do, NULL object will cause unmount */
        }
        else
        {
            Bstr bstrFdaFile(pcszFdaFile);

            /* Assume it's a host drive name */
            ComPtr<IHost> pHost;
            CHECK_ERROR_BREAK(pVirtualBox, COMGETTER(Host)(pHost.asOutParam()));
            rc = pHost->FindHostFloppyDrive(bstrFdaFile.raw(),
                                            pMedium.asOutParam());
            if (FAILED(rc))
            {
                /* try to find an existing one */
                rc = pVirtualBox->OpenMedium(bstrFdaFile.raw(),
                                             DeviceType_Floppy,
                                             AccessMode_ReadWrite,
                                             FALSE /* fForceNewUuid */,
                                             pMedium.asOutParam());
                if (FAILED(rc))
                {
                    /* try to add to the list */
                    RTPrintf("Adding floppy image '%s'...\n", pcszFdaFile);
                    CHECK_ERROR_BREAK(pVirtualBox,
                                      OpenMedium(bstrFdaFile.raw(),
                                                 DeviceType_Floppy,
                                                 AccessMode_ReadWrite,
                                                 FALSE /* fForceNewUuid */,
                                                 pMedium.asOutParam()));
                }
            }
        }

        Bstr bstrSCName;

        /* get the first floppy controller to attach the floppy to
         * and if there is none, add one temporarily */
        {
            ComPtr<IStorageController> pStorageCtl;
            com::SafeIfaceArray<IStorageController> aStorageControllers;
            CHECK_ERROR(gpMachine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(aStorageControllers)));
            for (size_t i = 0; i < aStorageControllers.size(); ++ i)
            {
                StorageBus_T storageBus = StorageBus_Null;

                CHECK_ERROR(aStorageControllers[i], COMGETTER(Bus)(&storageBus));
                if (storageBus == StorageBus_Floppy)
                {
                    pStorageCtl = aStorageControllers[i];
                    break;
                }
            }

            if (pStorageCtl)
            {
                CHECK_ERROR(pStorageCtl, COMGETTER(Name)(bstrSCName.asOutParam()));
                gpMachine->DetachDevice(bstrSCName.raw(), 0, 0);
            }
            else
            {
                bstrSCName = "Floppy Controller";
                CHECK_ERROR(gpMachine, AddStorageController(bstrSCName.raw(),
                                                            StorageBus_Floppy,
                                                            pStorageCtl.asOutParam()));
            }
        }

        CHECK_ERROR(gpMachine, AttachDevice(bstrSCName.raw(), 0, 0,
                                            DeviceType_Floppy, pMedium));
    }
    while (0);
    if (FAILED(rc))
        goto leave;

    /*
     * Mount a CD-ROM if requested.
     */
    if (pcszCdromFile)
    do
    {
        ComPtr<IMedium> pMedium;

        /* unmount? */
        if (!strcmp(pcszCdromFile, "none"))
        {
            /* nothing to do, NULL object will cause unmount */
        }
        else
        {
            Bstr bstrCdromFile(pcszCdromFile);

            /* Assume it's a host drive name */
            ComPtr<IHost> pHost;
            CHECK_ERROR_BREAK(pVirtualBox, COMGETTER(Host)(pHost.asOutParam()));
            rc = pHost->FindHostDVDDrive(bstrCdromFile.raw(), pMedium.asOutParam());
            if (FAILED(rc))
            {
                /* try to find an existing one */
                rc = pVirtualBox->OpenMedium(bstrCdromFile.raw(),
                                            DeviceType_DVD,
                                            AccessMode_ReadWrite,
                                            FALSE /* fForceNewUuid */,
                                            pMedium.asOutParam());
                if (FAILED(rc))
                {
                    /* try to add to the list */
                    RTPrintf("Adding ISO image '%s'...\n", pcszCdromFile);
                    CHECK_ERROR_BREAK(pVirtualBox,
                                      OpenMedium(bstrCdromFile.raw(),
                                                 DeviceType_DVD,
                                                 AccessMode_ReadWrite,
                                                 FALSE /* fForceNewUuid */,
                                                 pMedium.asOutParam()));
                }
            }
        }

        Bstr bstrSCName;

        /* get the first IDE controller to attach the DVD drive to
         * and if there is none, add one temporarily */
        {
            ComPtr<IStorageController> pStorageCtl;
            com::SafeIfaceArray<IStorageController> aStorageControllers;
            CHECK_ERROR(gpMachine, COMGETTER(StorageControllers)(ComSafeArrayAsOutParam(aStorageControllers)));
            for (size_t i = 0; i < aStorageControllers.size(); ++ i)
            {
                StorageBus_T storageBus = StorageBus_Null;

                CHECK_ERROR(aStorageControllers[i], COMGETTER(Bus)(&storageBus));
                if (storageBus == StorageBus_IDE)
                {
                    pStorageCtl = aStorageControllers[i];
                    break;
                }
            }

            if (pStorageCtl)
            {
                CHECK_ERROR(pStorageCtl, COMGETTER(Name)(bstrSCName.asOutParam()));
                gpMachine->DetachDevice(bstrSCName.raw(), 1, 0);
            }
            else
            {
                bstrSCName = "IDE Controller";
                CHECK_ERROR(gpMachine, AddStorageController(bstrSCName.raw(),
                                                            StorageBus_IDE,
                                                            pStorageCtl.asOutParam()));
            }
        }

        CHECK_ERROR(gpMachine, AttachDevice(bstrSCName.raw(), 1, 0,
                                            DeviceType_DVD, pMedium));
    }
    while (0);
    if (FAILED(rc))
        goto leave;

    if (fDiscardState)
    {
        /*
         * If the machine is currently saved,
         * discard the saved state first.
         */
        MachineState_T machineState;
        gpMachine->COMGETTER(State)(&machineState);
        if (machineState == MachineState_Saved)
        {
            CHECK_ERROR(gpMachine, DiscardSavedState(true /* fDeleteFile */));
        }
        /*
         * If there are snapshots, discard the current state,
         * i.e. revert to the last snapshot.
         */
        ULONG cSnapshots;
        gpMachine->COMGETTER(SnapshotCount)(&cSnapshots);
        if (cSnapshots)
        {
            gpProgress = NULL;

            ComPtr<ISnapshot> pCurrentSnapshot;
            CHECK_ERROR(gpMachine, COMGETTER(CurrentSnapshot)(pCurrentSnapshot.asOutParam()));
            if (FAILED(rc))
                goto leave;

            CHECK_ERROR(gpMachine, RestoreSnapshot(pCurrentSnapshot, gpProgress.asOutParam()));
            rc = gpProgress->WaitForCompletion(-1);
        }
    }

    // get the machine debugger (does not have to be there)
    gpConsole->COMGETTER(Debugger)(gpMachineDebugger.asOutParam());
    if (gpMachineDebugger)
    {
        Log(("Machine debugger available!\n"));
    }
    gpConsole->COMGETTER(Display)(gpDisplay.asOutParam());
    if (!gpDisplay)
    {
        RTPrintf("Error: could not get display object!\n");
        goto leave;
    }

    // set the boot drive
    if (bootDevice != DeviceType_Null)
    {
        rc = gpMachine->SetBootOrder(1, bootDevice);
        if (rc != S_OK)
        {
            RTPrintf("Error: could not set boot device, using default.\n");
        }
    }

    // set the memory size if not default
    if (memorySize)
    {
        rc = gpMachine->COMSETTER(MemorySize)(memorySize);
        if (rc != S_OK)
        {
            ULONG ramSize = 0;
            gpMachine->COMGETTER(MemorySize)(&ramSize);
            RTPrintf("Error: could not set memory size, using current setting of %d MBytes\n", ramSize);
        }
    }

    if (vramSize)
    {
        rc = gpMachine->COMSETTER(VRAMSize)(vramSize);
        if (rc != S_OK)
        {
            gpMachine->COMGETTER(VRAMSize)((ULONG*)&vramSize);
            RTPrintf("Error: could not set VRAM size, using current setting of %d MBytes\n", vramSize);
        }
    }

    // we're always able to process absolute mouse events and we prefer that
    gfAbsoluteMouseHost = TRUE;

#ifdef VBOX_WIN32_UI
    if (fWin32UI)
    {
        /* initialize the Win32 user interface inside which SDL will be embedded */
        if (initUI(fResizable, winId))
            return 1;
    }
#endif

    /* static initialization of the SDL stuff */
    if (!VBoxSDLFB::init(fShowSDLConfig))
        goto leave;

    gpMachine->COMGETTER(MonitorCount)(&gcMonitors);
    if (gcMonitors > 64)
        gcMonitors = 64;

    for (unsigned i = 0; i < gcMonitors; i++)
    {
        // create our SDL framebuffer instance
        gpFramebuffer[i].createObject();
        rc = gpFramebuffer[i]->init(i, fFullscreen, fResizable, fShowSDLConfig, false,
                                    fixedWidth, fixedHeight, fixedBPP, fSeparate);
        if (FAILED(rc))
        {
            RTPrintf("Error: could not create framebuffer object!\n");
            goto leave;
        }
    }

#ifdef VBOX_WIN32_UI
    gpFramebuffer[0]->setWinId(winId);
#endif

    for (unsigned i = 0; i < gcMonitors; i++)
    {
        if (!gpFramebuffer[i]->initialized())
            goto leave;
        gpFramebuffer[i]->AddRef();
        if (fFullscreen)
            SetFullscreen(true);
    }

#ifdef VBOX_SECURELABEL
    if (fSecureLabel)
    {
        if (!secureLabelFontFile)
        {
            RTPrintf("Error: no font file specified for secure label!\n");
            goto leave;
        }
        /* load the SDL_ttf library and get the required imports */
        vrc = RTLdrLoadSystem(LIBSDL_TTF_NAME, true /*fNoUnload*/, &gLibrarySDL_ttf);
        if (RT_SUCCESS(vrc))
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_Init", (void**)&pTTF_Init);
        if (RT_SUCCESS(vrc))
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_OpenFont", (void**)&pTTF_OpenFont);
        if (RT_SUCCESS(vrc))
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_RenderUTF8_Solid", (void**)&pTTF_RenderUTF8_Solid);
        if (RT_SUCCESS(vrc))
        {
            /* silently ignore errors here */
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_RenderUTF8_Blended", (void**)&pTTF_RenderUTF8_Blended);
            if (RT_FAILURE(vrc))
                pTTF_RenderUTF8_Blended = NULL;
            vrc = VINF_SUCCESS;
        }
        if (RT_SUCCESS(vrc))
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_CloseFont", (void**)&pTTF_CloseFont);
        if (RT_SUCCESS(vrc))
            vrc = RTLdrGetSymbol(gLibrarySDL_ttf, "TTF_Quit", (void**)&pTTF_Quit);
        if (RT_SUCCESS(vrc))
            vrc = gpFramebuffer[0]->initSecureLabel(SECURE_LABEL_HEIGHT, secureLabelFontFile, secureLabelPointSize, secureLabelFontOffs);
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Error: could not initialize secure labeling: rc = %Rrc\n", vrc);
            goto leave;
        }
        Bstr bstrLabel;
        gpMachine->GetExtraData(Bstr(VBOXSDL_SECURELABEL_EXTRADATA).raw(), bstrLabel.asOutParam());
        Utf8Str labelUtf8(bstrLabel);
        /*
         * Now update the label
         */
        gpFramebuffer[0]->setSecureLabelColor(secureLabelColorFG, secureLabelColorBG);
        gpFramebuffer[0]->setSecureLabelText(labelUtf8.c_str());
    }
#endif

#ifdef VBOXSDL_WITH_X11
    /* NOTE1: We still want Ctrl-C to work, so we undo the SDL redirections.
     * NOTE2: We have to remove the PidFile if this file exists. */
    signal(SIGINT,  signal_handler_SIGINT);
    signal(SIGQUIT, signal_handler_SIGINT);
    signal(SIGSEGV, signal_handler_SIGINT);
#endif


    for (ULONG i = 0; i < gcMonitors; i++)
    {
        // register our framebuffer
        rc = gpDisplay->AttachFramebuffer(i, gpFramebuffer[i], gaFramebufferId[i].asOutParam());
        if (FAILED(rc))
        {
            RTPrintf("Error: could not register framebuffer object!\n");
            goto leave;
        }
        ULONG dummy;
        LONG xOrigin, yOrigin;
        GuestMonitorStatus_T monitorStatus;
        rc = gpDisplay->GetScreenResolution(i, &dummy, &dummy, &dummy, &xOrigin, &yOrigin, &monitorStatus);
        gpFramebuffer[i]->setOrigin(xOrigin, yOrigin);
    }

    {
        // register listener for VirtualBoxClient events
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        ComObjPtr<VBoxSDLClientEventListenerImpl> listener;
        listener.createObject();
        listener->init(new VBoxSDLClientEventListener());
        pVBoxClientListener = listener;
        com::SafeArray<VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnVBoxSVCAvailabilityChanged);
        CHECK_ERROR(pES, RegisterListener(pVBoxClientListener, ComSafeArrayAsInParam(eventTypes), true));
    }

    {
        // register listener for VirtualBox (server) events
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualBox, COMGETTER(EventSource)(pES.asOutParam()));
        ComObjPtr<VBoxSDLEventListenerImpl> listener;
        listener.createObject();
        listener->init(new VBoxSDLEventListener());
        pVBoxListener = listener;
        com::SafeArray<VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnExtraDataChanged);
        CHECK_ERROR(pES, RegisterListener(pVBoxListener, ComSafeArrayAsInParam(eventTypes), true));
    }

    {
        // register listener for Console events
        ComPtr<IEventSource> pES;
        CHECK_ERROR(gpConsole, COMGETTER(EventSource)(pES.asOutParam()));
        pConsoleListener.createObject();
        pConsoleListener->init(new VBoxSDLConsoleEventListener());
        com::SafeArray<VBoxEventType_T> eventTypes;
        eventTypes.push_back(VBoxEventType_OnMousePointerShapeChanged);
        eventTypes.push_back(VBoxEventType_OnMouseCapabilityChanged);
        eventTypes.push_back(VBoxEventType_OnKeyboardLedsChanged);
        eventTypes.push_back(VBoxEventType_OnStateChanged);
        eventTypes.push_back(VBoxEventType_OnRuntimeError);
        eventTypes.push_back(VBoxEventType_OnCanShowWindow);
        eventTypes.push_back(VBoxEventType_OnShowWindow);
        CHECK_ERROR(pES, RegisterListener(pConsoleListener, ComSafeArrayAsInParam(eventTypes), true));
        // until we've tried to to start the VM, ignore power off events
        pConsoleListener->getWrapped()->ignorePowerOffEvents(true);
    }

    if (pszPortVRDP)
    {
        rc = gpMachine->COMGETTER(VRDEServer)(gpVRDEServer.asOutParam());
        AssertMsg((rc == S_OK) && gpVRDEServer, ("Could not get VRDP Server! rc = 0x%x\n", rc));
        if (gpVRDEServer)
        {
            // has a non standard VRDP port been requested?
            if (strcmp(pszPortVRDP, "0"))
            {
                rc = gpVRDEServer->SetVRDEProperty(Bstr("TCP/Ports").raw(), Bstr(pszPortVRDP).raw());
                if (rc != S_OK)
                {
                    RTPrintf("Error: could not set VRDP port! rc = 0x%x\n", rc);
                    goto leave;
                }
            }
            // now enable VRDP
            rc = gpVRDEServer->COMSETTER(Enabled)(TRUE);
            if (rc != S_OK)
            {
                RTPrintf("Error: could not enable VRDP server! rc = 0x%x\n", rc);
                goto leave;
            }
        }
    }

    rc = E_FAIL;
#ifdef VBOXSDL_ADVANCED_OPTIONS
    if (fRawR0 != ~0U)
    {
        if (!gpMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%srawr0 cannot be executed!\n", fRawR0 ? "" : "no");
            goto leave;
        }
        gpMachineDebugger->COMSETTER(RecompileSupervisor)(!fRawR0);
    }
    if (fRawR3 != ~0U)
    {
        if (!gpMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%srawr3 cannot be executed!\n", fRawR3 ? "" : "no");
            goto leave;
        }
        gpMachineDebugger->COMSETTER(RecompileUser)(!fRawR3);
    }
    if (fPATM != ~0U)
    {
        if (!gpMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%spatm cannot be executed!\n", fPATM ? "" : "no");
            goto leave;
        }
        gpMachineDebugger->COMSETTER(PATMEnabled)(fPATM);
    }
    if (fCSAM != ~0U)
    {
        if (!gpMachineDebugger)
        {
            RTPrintf("Error: No debugger object; -%scsam cannot be executed!\n", fCSAM ? "" : "no");
            goto leave;
        }
        gpMachineDebugger->COMSETTER(CSAMEnabled)(fCSAM);
    }
    if (fHWVirt != ~0U)
    {
        gpMachine->SetHWVirtExProperty(HWVirtExPropertyType_Enabled, fHWVirt);
    }
    if (u32WarpDrive != 0)
    {
        if (!gpMachineDebugger)
        {
            RTPrintf("Error: No debugger object; --warpdrive %d cannot be executed!\n", u32WarpDrive);
            goto leave;
        }
        gpMachineDebugger->COMSETTER(VirtualTimeRate)(u32WarpDrive);
    }
#endif /* VBOXSDL_ADVANCED_OPTIONS */

    /* start with something in the titlebar */
    UpdateTitlebar(TITLEBAR_NORMAL);

    /* memorize the default cursor */
    gpDefaultCursor = SDL_GetCursor();

#if !defined(VBOX_WITH_SDL13)
# if defined(VBOXSDL_WITH_X11)
    /* Get Window Manager info. We only need the X11 display. */
    SDL_VERSION(&gSdlInfo.version);
    if (!SDL_GetWMInfo(&gSdlInfo))
        RTPrintf("Error: could not get SDL Window Manager info -- no Xcursor support!\n");
    else
        gfXCursorEnabled = TRUE;

#  if !defined(VBOX_WITHOUT_XCURSOR)
    /* SDL uses its own (plain) default cursor. Use the left arrow cursor instead which might look
     * much better if a mouse cursor theme is installed. */
    if (gfXCursorEnabled)
    {
        gpDefaultOrigX11Cursor = *(Cursor*)gpDefaultCursor->wm_cursor;
        *(Cursor*)gpDefaultCursor->wm_cursor = XCreateFontCursor(gSdlInfo.info.x11.display, XC_left_ptr);
        SDL_SetCursor(gpDefaultCursor);
    }
#  endif
    /* Initialise the keyboard */
    X11DRV_InitKeyboard(gSdlInfo.info.x11.display, NULL, NULL, NULL, NULL);
# endif /* VBOXSDL_WITH_X11 */

    /* create a fake empty cursor */
    {
        uint8_t cursorData[1] = {0};
        gpCustomCursor = SDL_CreateCursor(cursorData, cursorData, 8, 1, 0, 0);
        gpCustomOrigWMcursor = gpCustomCursor->wm_cursor;
        gpCustomCursor->wm_cursor = NULL;
    }
#endif /* !VBOX_WITH_SDL13 */

    /*
     * Register our user signal handler.
     */
#ifdef VBOXSDL_WITH_X11
    struct sigaction sa;
    sa.sa_sigaction = signal_handler_SIGUSR1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGUSR1, &sa, NULL);
#endif /* VBOXSDL_WITH_X11 */

    /*
     * Start the VM execution thread. This has to be done
     * asynchronously as powering up can take some time
     * (accessing devices such as the host DVD drive). In
     * the meantime, we have to service the SDL event loop.
     */
    SDL_Event event;

    if (!fSeparate)
    {
        LogFlow(("Powering up the VM...\n"));
        rc = gpConsole->PowerUp(gpProgress.asOutParam());
        if (rc != S_OK)
        {
            com::ErrorInfo info(gpConsole, COM_IIDOF(IConsole));
            if (info.isBasicAvailable())
                PrintError("Failed to power up VM", info.getText().raw());
            else
                RTPrintf("Error: failed to power up VM! No error text available.\n");
            goto leave;
        }
    }

#ifdef USE_XPCOM_QUEUE_THREAD
    /*
     * Before we starting to do stuff, we have to launch the XPCOM
     * event queue thread. It will wait for events and send messages
     * to the SDL thread. After having done this, we should fairly
     * quickly start to process the SDL event queue as an XPCOM
     * event storm might arrive. Stupid SDL has a ridiculously small
     * event queue buffer!
     */
    startXPCOMEventQueueThread(eventQ->getSelectFD());
#endif /* USE_XPCOM_QUEUE_THREAD */

    /* termination flag */
    bool fTerminateDuringStartup;
    fTerminateDuringStartup = false;

    LogRel(("VBoxSDL: NUM lock initially %s, CAPS lock initially %s\n",
            !!(SDL_GetModState() & KMOD_NUM)  ? "ON" : "OFF",
            !!(SDL_GetModState() & KMOD_CAPS) ? "ON" : "OFF"));

    /* start regular timer so we don't starve in the event loop */
    SDL_TimerID sdlTimer;
    sdlTimer = SDL_AddTimer(100, StartupTimer, NULL);

    /* loop until the powerup processing is done */
    MachineState_T machineState;
    do
    {
        rc = gpMachine->COMGETTER(State)(&machineState);
        if (    rc == S_OK
            &&  (   machineState == MachineState_Starting
                 || machineState == MachineState_Restoring
                 || machineState == MachineState_TeleportingIn
                )
            )
        {
            /*
             * wait for the next event. This is uncritical as
             * power up guarantees to change the machine state
             * to either running or aborted and a machine state
             * change will send us an event. However, we have to
             * service the XPCOM event queue!
             */
#ifdef USE_XPCOM_QUEUE_THREAD
            if (!fXPCOMEventThreadSignaled)
            {
                signalXPCOMEventQueueThread();
                fXPCOMEventThreadSignaled = true;
            }
#endif
            /*
             * Wait for SDL events.
             */
            if (WaitSDLEvent(&event))
            {
                switch (event.type)
                {
                    /*
                     * Timer event. Used to have the titlebar updated.
                     */
                    case SDL_USER_EVENT_TIMER:
                    {
                        /*
                         * Update the title bar.
                         */
                        UpdateTitlebar(TITLEBAR_STARTUP);
                        break;
                    }

                    /*
                     * User specific framebuffer change event.
                     */
                    case SDL_USER_EVENT_NOTIFYCHANGE:
                    {
                        LogFlow(("SDL_USER_EVENT_NOTIFYCHANGE\n"));
                        LONG xOrigin, yOrigin;
                        gpFramebuffer[event.user.code]->notifyChange(event.user.code);
                        /* update xOrigin, yOrigin -> mouse */
                        ULONG dummy;
                        GuestMonitorStatus_T monitorStatus;
                        rc = gpDisplay->GetScreenResolution(event.user.code, &dummy, &dummy, &dummy, &xOrigin, &yOrigin, &monitorStatus);
                        gpFramebuffer[event.user.code]->setOrigin(xOrigin, yOrigin);
                        break;
                    }

#ifdef USE_XPCOM_QUEUE_THREAD
                    /*
                     * User specific XPCOM event queue event
                     */
                    case SDL_USER_EVENT_XPCOM_EVENTQUEUE:
                    {
                        LogFlow(("SDL_USER_EVENT_XPCOM_EVENTQUEUE: processing XPCOM event queue...\n"));
                        eventQ->processEventQueue(0);
                        signalXPCOMEventQueueThread();
                        break;
                    }
#endif /* USE_XPCOM_QUEUE_THREAD */

                    /*
                     * Termination event from the on state change callback.
                     */
                    case SDL_USER_EVENT_TERMINATE:
                    {
                        if (event.user.code != VBOXSDL_TERM_NORMAL)
                        {
                            com::ProgressErrorInfo info(gpProgress);
                            if (info.isBasicAvailable())
                                PrintError("Failed to power up VM", info.getText().raw());
                            else
                                RTPrintf("Error: failed to power up VM! No error text available.\n");
                        }
                        fTerminateDuringStartup = true;
                        break;
                    }

                    default:
                    {
                        Log8(("VBoxSDL: Unknown SDL event %d (pre)\n", event.type));
                        break;
                    }
                }

            }
        }
        eventQ->processEventQueue(0);
    } while (   rc == S_OK
             && (   machineState == MachineState_Starting
                 || machineState == MachineState_Restoring
                 || machineState == MachineState_TeleportingIn
                )
            );

    /* kill the timer again */
    SDL_RemoveTimer(sdlTimer);
    sdlTimer = 0;

    /* are we supposed to terminate the process? */
    if (fTerminateDuringStartup)
        goto leave;

    /* did the power up succeed? */
    if (machineState != MachineState_Running)
    {
        com::ProgressErrorInfo info(gpProgress);
        if (info.isBasicAvailable())
            PrintError("Failed to power up VM", info.getText().raw());
        else
            RTPrintf("Error: failed to power up VM! No error text available (rc = 0x%x state = %d)\n", rc, machineState);
        goto leave;
    }

    // accept power off events from now on because we're running
    // note that there's a possible race condition here...
    pConsoleListener->getWrapped()->ignorePowerOffEvents(false);

    rc = gpConsole->COMGETTER(Keyboard)(gpKeyboard.asOutParam());
    if (!gpKeyboard)
    {
        RTPrintf("Error: could not get keyboard object!\n");
        goto leave;
    }
    gpConsole->COMGETTER(Mouse)(gpMouse.asOutParam());
    if (!gpMouse)
    {
        RTPrintf("Error: could not get mouse object!\n");
        goto leave;
    }

    if (fSeparate && gpMouse)
    {
        LogFlow(("Fetching mouse caps\n"));

        /* Fetch current mouse status, etc */
        gpMouse->COMGETTER(AbsoluteSupported)(&gfAbsoluteMouseGuest);
        gpMouse->COMGETTER(RelativeSupported)(&gfRelativeMouseGuest);
        gpMouse->COMGETTER(NeedsHostCursor)(&gfGuestNeedsHostCursor);

        HandleGuestCapsChanged();

        ComPtr<IMousePointerShape> mps;
        gpMouse->COMGETTER(PointerShape)(mps.asOutParam());
        if (!mps.isNull())
        {
            BOOL  visible,  alpha;
            ULONG hotX, hotY, width, height;
            com::SafeArray <BYTE> shape;

            mps->COMGETTER(Visible)(&visible);
            mps->COMGETTER(Alpha)(&alpha);
            mps->COMGETTER(HotX)(&hotX);
            mps->COMGETTER(HotY)(&hotY);
            mps->COMGETTER(Width)(&width);
            mps->COMGETTER(Height)(&height);
            mps->COMGETTER(Shape)(ComSafeArrayAsOutParam(shape));

            if (shape.size() > 0)
            {
                PointerShapeChangeData data(visible, alpha, hotX, hotY, width, height,
                                            ComSafeArrayAsInParam(shape));
                SetPointerShape(&data);
            }
        }
    }

    UpdateTitlebar(TITLEBAR_NORMAL);

    /*
     * Enable keyboard repeats
     */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /*
     * Create PID file.
     */
    if (gpszPidFile)
    {
        char szBuf[32];
        const char *pcszLf = "\n";
        RTFILE PidFile;
        RTFileOpen(&PidFile, gpszPidFile, RTFILE_O_WRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE);
        RTStrFormatNumber(szBuf, RTProcSelf(), 10, 0, 0, 0);
        RTFileWrite(PidFile, szBuf, strlen(szBuf), NULL);
        RTFileWrite(PidFile, pcszLf, strlen(pcszLf), NULL);
        RTFileClose(PidFile);
    }

    /*
     * Main event loop
     */
#ifdef USE_XPCOM_QUEUE_THREAD
    if (!fXPCOMEventThreadSignaled)
    {
        signalXPCOMEventQueueThread();
    }
#endif
    LogFlow(("VBoxSDL: Entering big event loop\n"));
    while (WaitSDLEvent(&event))
    {
        switch (event.type)
        {
            /*
             * The screen needs to be repainted.
             */
#ifdef VBOX_WITH_SDL13
            case SDL_WINDOWEVENT:
            {
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        VBoxSDLFB *fb = getFbFromWinId(event.window.windowID);
                        if (fb)
                            fb->repaint();
                        break;
                    }
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                    {
                        break;
                    }
                    default:
                        break;
                }
            }
#else
            case SDL_VIDEOEXPOSE:
            {
                gpFramebuffer[0]->repaint();
                break;
            }
#endif

            /*
             * Keyboard events.
             */
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                SDLKey ksym = event.key.keysym.sym;

                switch (enmHKeyState)
                {
                    case HKEYSTATE_NORMAL:
                    {
                        if (   event.type == SDL_KEYDOWN
                            && ksym != SDLK_UNKNOWN
                            && (ksym == gHostKeySym1 || ksym == gHostKeySym2))
                        {
                            EvHKeyDown1  = event;
                            enmHKeyState = ksym == gHostKeySym1 ? HKEYSTATE_DOWN_1ST
                                                                : HKEYSTATE_DOWN_2ND;
                            break;
                        }
                        ProcessKey(&event.key);
                        break;
                    }

                    case HKEYSTATE_DOWN_1ST:
                    case HKEYSTATE_DOWN_2ND:
                    {
                        if (gHostKeySym2 != SDLK_UNKNOWN)
                        {
                            if (   event.type == SDL_KEYDOWN
                                && ksym != SDLK_UNKNOWN
                                && (   (enmHKeyState == HKEYSTATE_DOWN_1ST && ksym == gHostKeySym2)
                                    || (enmHKeyState == HKEYSTATE_DOWN_2ND && ksym == gHostKeySym1)))
                            {
                                EvHKeyDown2  = event;
                                enmHKeyState = HKEYSTATE_DOWN;
                                break;
                            }
                            enmHKeyState = event.type == SDL_KEYUP ? HKEYSTATE_NORMAL
                                                                 : HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            /* ugly hack: Some guests (e.g. mstsc.exe on Windows XP)
                             * expect a small delay between two key events. 5ms work
                             * reliable here so use 10ms to be on the safe side. A
                             * better but more complicated fix would be to introduce
                             * a new state and don't wait here. */
                            RTThreadSleep(10);
                            ProcessKey(&event.key);
                            break;
                        }
                    }
                    RT_FALL_THRU();

                    case HKEYSTATE_DOWN:
                    {
                        if (event.type == SDL_KEYDOWN)
                        {
                            /* potential host key combination, try execute it */
                            int irc = HandleHostKey(&event.key);
                            if (irc == VINF_SUCCESS)
                            {
                                enmHKeyState = HKEYSTATE_USED;
                                break;
                            }
                            if (RT_SUCCESS(irc))
                                goto leave;
                        }
                        else /* SDL_KEYUP */
                        {
                            if (   ksym != SDLK_UNKNOWN
                                && (ksym == gHostKeySym1 || ksym == gHostKeySym2))
                            {
                                /* toggle grabbing state */
                                if (!gfGrabbed)
                                    InputGrabStart();
                                else
                                    InputGrabEnd();

                                /* SDL doesn't always reset the keystates, correct it */
                                ResetKeys();
                                enmHKeyState = HKEYSTATE_NORMAL;
                                break;
                            }
                        }

                        /* not host key */
                        enmHKeyState = HKEYSTATE_NOT_IT;
                        ProcessKey(&EvHKeyDown1.key);
                        /* see the comment for the 2-key case above */
                        RTThreadSleep(10);
                        if (gHostKeySym2 != SDLK_UNKNOWN)
                        {
                            ProcessKey(&EvHKeyDown2.key);
                            /* see the comment for the 2-key case above */
                            RTThreadSleep(10);
                        }
                        ProcessKey(&event.key);
                        break;
                    }

                    case HKEYSTATE_USED:
                    {
                        if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) == 0)
                            enmHKeyState = HKEYSTATE_NORMAL;
                        if (event.type == SDL_KEYDOWN)
                        {
                            int irc = HandleHostKey(&event.key);
                            if (RT_SUCCESS(irc) && irc != VINF_SUCCESS)
                                goto leave;
                        }
                        break;
                    }

                    default:
                        AssertMsgFailed(("enmHKeyState=%d\n", enmHKeyState));
                        RT_FALL_THRU();
                    case HKEYSTATE_NOT_IT:
                    {
                        if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) == 0)
                            enmHKeyState = HKEYSTATE_NORMAL;
                        ProcessKey(&event.key);
                        break;
                    }
                } /* state switch */
                break;
            }

            /*
             * The window was closed.
             */
            case SDL_QUIT:
            {
                if (!gfACPITerm || gSdlQuitTimer)
                    goto leave;
                if (gpConsole)
                    gpConsole->PowerButton();
                gSdlQuitTimer = SDL_AddTimer(1000, QuitTimer, NULL);
                break;
            }

            /*
             * The mouse has moved
             */
            case SDL_MOUSEMOTION:
            {
                if (gfGrabbed || UseAbsoluteMouse())
                {
                    VBoxSDLFB *fb;
#ifdef VBOX_WITH_SDL13
                    fb = getFbFromWinId(event.motion.windowID);
#else
                    fb = gpFramebuffer[0];
#endif
                    SendMouseEvent(fb, 0, 0, 0);
                }
                break;
            }

            /*
             * A mouse button has been clicked or released.
             */
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                SDL_MouseButtonEvent *bev = &event.button;
                /* don't grab on mouse click if we have guest additions */
                if (!gfGrabbed && !UseAbsoluteMouse() && gfGrabOnMouseClick)
                {
                    if (event.type == SDL_MOUSEBUTTONDOWN && (bev->state & SDL_BUTTON_LMASK))
                    {
                        /* start grabbing all events */
                        InputGrabStart();
                    }
                }
                else if (gfGrabbed || UseAbsoluteMouse())
                {
                    int dz = bev->button == SDL_BUTTON_WHEELUP
                                         ? -1
                                         : bev->button == SDL_BUTTON_WHEELDOWN
                                                       ? +1
                                                       :  0;

                    /* end host key combination (CTRL+MouseButton) */
                    switch (enmHKeyState)
                    {
                        case HKEYSTATE_DOWN_1ST:
                        case HKEYSTATE_DOWN_2ND:
                            enmHKeyState = HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            /* ugly hack: small delay to ensure that the key event is
                             * actually handled _prior_ to the mouse click event */
                            RTThreadSleep(20);
                            break;
                        case HKEYSTATE_DOWN:
                            enmHKeyState = HKEYSTATE_NOT_IT;
                            ProcessKey(&EvHKeyDown1.key);
                            if (gHostKeySym2 != SDLK_UNKNOWN)
                                ProcessKey(&EvHKeyDown2.key);
                            /* ugly hack: small delay to ensure that the key event is
                             * actually handled _prior_ to the mouse click event */
                            RTThreadSleep(20);
                            break;
                        default:
                            break;
                    }

                    VBoxSDLFB *fb;
#ifdef VBOX_WITH_SDL13
                    fb = getFbFromWinId(event.button.windowID);
#else
                    fb = gpFramebuffer[0];
#endif
                    SendMouseEvent(fb, dz, event.type == SDL_MOUSEBUTTONDOWN, bev->button);
                }
                break;
            }

            /*
             * The window has gained or lost focus.
             */
            case SDL_ACTIVEEVENT:
            {
                /*
                 * There is a strange behaviour in SDL when running without a window
                 * manager: When SDL_WM_GrabInput(SDL_GRAB_ON) is called we receive two
                 * consecutive events SDL_ACTIVEEVENTs (input lost, input gained).
                 * Asking SDL_GetAppState() seems the better choice.
                 */
                if (gfGrabbed && (SDL_GetAppState() & SDL_APPINPUTFOCUS) == 0)
                {
                    /*
                     * another window has stolen the (keyboard) input focus
                     */
                    InputGrabEnd();
                }
                break;
            }

            /*
             * The SDL window was resized
             */
            case SDL_VIDEORESIZE:
            {
                if (gpDisplay)
                {
                    if (gfIgnoreNextResize)
                    {
                        gfIgnoreNextResize = FALSE;
                        break;
                    }
                    uResizeWidth  = event.resize.w;
#ifdef VBOX_SECURELABEL
                    if (fSecureLabel)
                        uResizeHeight = RT_MAX(0, event.resize.h - SECURE_LABEL_HEIGHT);
                    else
#endif
                        uResizeHeight = event.resize.h;
                    if (gSdlResizeTimer)
                        SDL_RemoveTimer(gSdlResizeTimer);
                    gSdlResizeTimer = SDL_AddTimer(300, ResizeTimer, NULL);
                }
                break;
            }

            /*
             * User specific update event.
             */
            /** @todo use a common user event handler so that SDL_PeepEvents() won't
             * possibly remove other events in the queue!
             */
            case SDL_USER_EVENT_UPDATERECT:
            {
                /*
                 * Decode event parameters.
                 */
                ASMAtomicDecS32(&g_cNotifyUpdateEventsPending);
                #define DECODEX(event) (int)((intptr_t)(event).user.data1 >> 16)
                #define DECODEY(event) (int)((intptr_t)(event).user.data1 & 0xFFFF)
                #define DECODEW(event) (int)((intptr_t)(event).user.data2 >> 16)
                #define DECODEH(event) (int)((intptr_t)(event).user.data2 & 0xFFFF)
                int x = DECODEX(event);
                int y = DECODEY(event);
                int w = DECODEW(event);
                int h = DECODEH(event);
                LogFlow(("SDL_USER_EVENT_UPDATERECT: x = %d, y = %d, w = %d, h = %d\n",
                         x, y, w, h));

                Assert(gpFramebuffer[event.user.code]);
                gpFramebuffer[event.user.code]->update(x, y, w, h, true /* fGuestRelative */);

                #undef DECODEX
                #undef DECODEY
                #undef DECODEW
                #undef DECODEH
                break;
            }

            /*
             * User event: Window resize done
             */
            case SDL_USER_EVENT_WINDOW_RESIZE_DONE:
            {
                /**
                 * @todo This is a workaround for synchronization problems between EMT and the
                 *       SDL main thread. It can happen that the SDL thread already starts a
                 *       new resize operation while the EMT is still busy with the old one
                 *       leading to a deadlock. Therefore we call SetVideoModeHint only once
                 *       when the mouse button was released.
                 */
                /* communicate the resize event to the guest */
                gpDisplay->SetVideoModeHint(0 /*=display*/, true /*=enabled*/, false /*=changeOrigin*/,
                                            0 /*=originX*/, 0 /*=originY*/,
                                            uResizeWidth, uResizeHeight, 0 /*=don't change bpp*/);
                break;

            }

            /*
             * User specific framebuffer change event.
             */
            case SDL_USER_EVENT_NOTIFYCHANGE:
            {
                LogFlow(("SDL_USER_EVENT_NOTIFYCHANGE\n"));
                LONG xOrigin, yOrigin;
                gpFramebuffer[event.user.code]->notifyChange(event.user.code);
                /* update xOrigin, yOrigin -> mouse */
                ULONG dummy;
                GuestMonitorStatus_T monitorStatus;
                rc = gpDisplay->GetScreenResolution(event.user.code, &dummy, &dummy, &dummy, &xOrigin, &yOrigin, &monitorStatus);
                gpFramebuffer[event.user.code]->setOrigin(xOrigin, yOrigin);
                break;
            }

#ifdef USE_XPCOM_QUEUE_THREAD
            /*
             * User specific XPCOM event queue event
             */
            case SDL_USER_EVENT_XPCOM_EVENTQUEUE:
            {
                LogFlow(("SDL_USER_EVENT_XPCOM_EVENTQUEUE: processing XPCOM event queue...\n"));
                eventQ->processEventQueue(0);
                signalXPCOMEventQueueThread();
                break;
            }
#endif /* USE_XPCOM_QUEUE_THREAD */

            /*
             * User specific update title bar notification event
             */
            case SDL_USER_EVENT_UPDATE_TITLEBAR:
            {
                UpdateTitlebar(TITLEBAR_NORMAL);
                break;
            }

            /*
             * User specific termination event
             */
            case SDL_USER_EVENT_TERMINATE:
            {
                if (event.user.code != VBOXSDL_TERM_NORMAL)
                    RTPrintf("Error: VM terminated abnormally!\n");
                goto leave;
            }

#ifdef VBOX_SECURELABEL
            /*
             * User specific secure label update event
             */
            case SDL_USER_EVENT_SECURELABEL_UPDATE:
            {
                /*
                 * Query the new label text
                 */
                Bstr bstrLabel;
                gpMachine->GetExtraData(Bstr(VBOXSDL_SECURELABEL_EXTRADATA).raw(), bstrLabel.asOutParam());
                Utf8Str labelUtf8(bstrLabel);
                /*
                 * Now update the label
                 */
                gpFramebuffer[0]->setSecureLabelText(labelUtf8.c_str());
                break;
            }
#endif /* VBOX_SECURELABEL */

            /*
             * User specific pointer shape change event
             */
            case SDL_USER_EVENT_POINTER_CHANGE:
            {
                PointerShapeChangeData *data = (PointerShapeChangeData *)event.user.data1;
                SetPointerShape (data);
                delete data;
                break;
            }

            /*
             * User specific guest capabilities changed
             */
            case SDL_USER_EVENT_GUEST_CAP_CHANGED:
            {
                HandleGuestCapsChanged();
                break;
            }

            default:
            {
                Log8(("unknown SDL event %d\n", event.type));
                break;
            }
        }
    }

leave:
    if (gpszPidFile)
        RTFileDelete(gpszPidFile);

    LogFlow(("leaving...\n"));
#if defined(VBOX_WITH_XPCOM) && !defined(RT_OS_DARWIN) && !defined(RT_OS_OS2)
    /* make sure the XPCOM event queue thread doesn't do anything harmful */
    terminateXPCOMQueueThread();
#endif /* VBOX_WITH_XPCOM */

    if (gpVRDEServer)
        rc = gpVRDEServer->COMSETTER(Enabled)(FALSE);

    /*
     * Get the machine state.
     */
    if (gpMachine)
        gpMachine->COMGETTER(State)(&machineState);
    else
        machineState = MachineState_Aborted;

    if (!fSeparate)
    {
        /*
         * Turn off the VM if it's running
         */
        if (   gpConsole
            && (   machineState == MachineState_Running
                || machineState == MachineState_Teleporting
                || machineState == MachineState_LiveSnapshotting
                /** @todo power off paused VMs too? */
               )
           )
        do
        {
            pConsoleListener->getWrapped()->ignorePowerOffEvents(true);
            ComPtr<IProgress> pProgress;
            CHECK_ERROR_BREAK(gpConsole, PowerDown(pProgress.asOutParam()));
            CHECK_ERROR_BREAK(pProgress, WaitForCompletion(-1));
            BOOL completed;
            CHECK_ERROR_BREAK(pProgress, COMGETTER(Completed)(&completed));
            ASSERT(completed);
            LONG hrc;
            CHECK_ERROR_BREAK(pProgress, COMGETTER(ResultCode)(&hrc));
            if (FAILED(hrc))
            {
                com::ErrorInfo info;
                if (info.isFullAvailable())
                    PrintError("Failed to power down VM",
                               info.getText().raw(), info.getComponent().raw());
                else
                    RTPrintf("Failed to power down virtual machine! No error information available (rc = 0x%x).\n", hrc);
                break;
            }
        } while (0);
    }

    /* unregister Console listener */
    if (pConsoleListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(gpConsole, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(pConsoleListener));
        pConsoleListener.setNull();
    }

    /*
     * Now we discard all settings so that our changes will
     * not be flushed to the permanent configuration
     */
    if (   gpMachine
        && machineState != MachineState_Saved)
    {
        rc = gpMachine->DiscardSettings();
        AssertMsg(SUCCEEDED(rc), ("DiscardSettings %Rhrc, machineState %d\n", rc, machineState));
    }

    /* close the session */
    if (sessionOpened)
    {
        rc = pSession->UnlockMachine();
        AssertComRC(rc);
    }

#ifndef VBOX_WITH_SDL13
    /* restore the default cursor and free the custom one if any */
    if (gpDefaultCursor)
    {
# ifdef VBOXSDL_WITH_X11
        Cursor pDefaultTempX11Cursor = 0;
        if (gfXCursorEnabled)
        {
            pDefaultTempX11Cursor = *(Cursor*)gpDefaultCursor->wm_cursor;
            *(Cursor*)gpDefaultCursor->wm_cursor = gpDefaultOrigX11Cursor;
        }
# endif /* VBOXSDL_WITH_X11 */
        SDL_SetCursor(gpDefaultCursor);
# if defined(VBOXSDL_WITH_X11) && !defined(VBOX_WITHOUT_XCURSOR)
        if (gfXCursorEnabled)
            XFreeCursor(gSdlInfo.info.x11.display, pDefaultTempX11Cursor);
# endif /* VBOXSDL_WITH_X11 && !VBOX_WITHOUT_XCURSOR */
    }

    if (gpCustomCursor)
    {
        WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;
        gpCustomCursor->wm_cursor = gpCustomOrigWMcursor;
        SDL_FreeCursor(gpCustomCursor);
        if (pCustomTempWMCursor)
        {
# if defined(RT_OS_WINDOWS)
            ::DestroyCursor(*(HCURSOR *)pCustomTempWMCursor);
# elif defined(VBOXSDL_WITH_X11) && !defined(VBOX_WITHOUT_XCURSOR)
            if (gfXCursorEnabled)
                XFreeCursor(gSdlInfo.info.x11.display, *(Cursor *)pCustomTempWMCursor);
# endif /* VBOXSDL_WITH_X11 && !VBOX_WITHOUT_XCURSOR */
            free(pCustomTempWMCursor);
        }
    }
#endif

    LogFlow(("Releasing mouse, keyboard, remote desktop server, display, console...\n"));
    if (gpDisplay)
    {
        for (unsigned i = 0; i < gcMonitors; i++)
            gpDisplay->DetachFramebuffer(i, gaFramebufferId[i].raw());
    }

    gpMouse = NULL;
    gpKeyboard = NULL;
    gpVRDEServer = NULL;
    gpDisplay = NULL;
    gpConsole = NULL;
    gpMachineDebugger = NULL;
    gpProgress = NULL;
    // we can only uninitialize SDL here because it is not threadsafe

    for (unsigned i = 0; i < gcMonitors; i++)
    {
        if (gpFramebuffer[i])
        {
            LogFlow(("Releasing framebuffer...\n"));
            gpFramebuffer[i]->Release();
            gpFramebuffer[i] = NULL;
        }
    }

    VBoxSDLFB::uninit();

#ifdef VBOX_SECURELABEL
    /* must do this after destructing the framebuffer */
    if (gLibrarySDL_ttf)
        RTLdrClose(gLibrarySDL_ttf);
#endif

    /* VirtualBox (server) listener unregistration. */
    if (pVBoxListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualBox, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(pVBoxListener));
        pVBoxListener.setNull();
    }

    /* VirtualBoxClient listener unregistration. */
    if (pVBoxClientListener)
    {
        ComPtr<IEventSource> pES;
        CHECK_ERROR(pVirtualBoxClient, COMGETTER(EventSource)(pES.asOutParam()));
        if (!pES.isNull())
            CHECK_ERROR(pES, UnregisterListener(pVBoxClientListener));
        pVBoxClientListener.setNull();
    }

    LogFlow(("Releasing machine, session...\n"));
    gpMachine = NULL;
    pSession = NULL;
    LogFlow(("Releasing VirtualBox object...\n"));
    pVirtualBox = NULL;
    LogFlow(("Releasing VirtualBoxClient object...\n"));
    pVirtualBoxClient = NULL;

    // end "all-stuff" scope
    ////////////////////////////////////////////////////////////////////////////
    }

    /* Must be before com::Shutdown() */
    LogFlow(("Uninitializing COM...\n"));
    com::Shutdown();

    LogFlow(("Returning from main()!\n"));
    RTLogFlush(NULL);
    return FAILED(rc) ? 1 : 0;
}

static RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd)
{
    size_t cbFile;
    char szPasswd[512];
    int vrc = VINF_SUCCESS;
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    bool fStdIn = !strcmp(pszFilename, "stdin");
    PRTSTREAM pStrm;
    if (!fStdIn)
        vrc = RTStrmOpen(pszFilename, "r", &pStrm);
    else
        pStrm = g_pStdIn;
    if (RT_SUCCESS(vrc))
    {
        vrc = RTStrmReadEx(pStrm, szPasswd, sizeof(szPasswd)-1, &cbFile);
        if (RT_SUCCESS(vrc))
        {
            if (cbFile >= sizeof(szPasswd)-1)
            {
                RTPrintf("Provided password in file '%s' is too long\n", pszFilename);
                rcExit = RTEXITCODE_FAILURE;
            }
            else
            {
                unsigned i;
                for (i = 0; i < cbFile && !RT_C_IS_CNTRL(szPasswd[i]); i++)
                    ;
                szPasswd[i] = '\0';
                *pPasswd = szPasswd;
            }
        }
        else
        {
            RTPrintf("Cannot read password from file '%s': %Rrc\n", pszFilename, vrc);
            rcExit = RTEXITCODE_FAILURE;
        }
        if (!fStdIn)
            RTStrmClose(pStrm);
    }
    else
    {
        RTPrintf("Cannot open password file '%s' (%Rrc)\n", pszFilename, vrc);
        rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

static RTEXITCODE settingsPasswordFile(ComPtr<IVirtualBox> virtualBox, const char *pszFilename)
{
    com::Utf8Str passwd;
    RTEXITCODE rcExit = readPasswordFile(pszFilename, &passwd);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        int rc;
        CHECK_ERROR(virtualBox, SetSettingsSecret(com::Bstr(passwd).raw()));
        if (FAILED(rc))
            rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

#ifndef VBOX_WITH_HARDENING
/**
 * Main entry point
 */
int main(int argc, char **argv)
{
#ifdef Q_WS_X11
    if (!XInitThreads())
        return 1;
#endif
    /*
     * Before we do *anything*, we initialize the runtime.
     */
    int rc = RTR3InitExe(argc, &argv, RTR3INIT_FLAGS_SUPLIB);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);
    return TrustedMain(argc, argv, NULL);
}
#endif /* !VBOX_WITH_HARDENING */


/**
 * Returns whether the absolute mouse is in use, i.e. both host
 * and guest have opted to enable it.
 *
 * @returns bool Flag whether the absolute mouse is in use
 */
static bool UseAbsoluteMouse(void)
{
    return (gfAbsoluteMouseHost && gfAbsoluteMouseGuest);
}

#if defined(RT_OS_DARWIN) || defined(RT_OS_OS2)
/**
 * Fallback keycode conversion using SDL symbols.
 *
 * This is used to catch keycodes that's missing from the translation table.
 *
 * @returns XT scancode
 * @param   ev SDL scancode
 */
static uint16_t Keyevent2KeycodeFallback(const SDL_KeyboardEvent *ev)
{
    const SDLKey sym = ev->keysym.sym;
    Log(("SDL key event: sym=%d scancode=%#x unicode=%#x\n",
         sym, ev->keysym.scancode, ev->keysym.unicode));
    switch (sym)
    {                               /* set 1 scan code */
        case SDLK_ESCAPE:           return 0x01;
        case SDLK_EXCLAIM:
        case SDLK_1:                return 0x02;
        case SDLK_AT:
        case SDLK_2:                return 0x03;
        case SDLK_HASH:
        case SDLK_3:                return 0x04;
        case SDLK_DOLLAR:
        case SDLK_4:                return 0x05;
        /* % */
        case SDLK_5:                return 0x06;
        case SDLK_CARET:
        case SDLK_6:                return 0x07;
        case SDLK_AMPERSAND:
        case SDLK_7:                return 0x08;
        case SDLK_ASTERISK:
        case SDLK_8:                return 0x09;
        case SDLK_LEFTPAREN:
        case SDLK_9:                return 0x0a;
        case SDLK_RIGHTPAREN:
        case SDLK_0:                return 0x0b;
        case SDLK_UNDERSCORE:
        case SDLK_MINUS:            return 0x0c;
        case SDLK_EQUALS:
        case SDLK_PLUS:             return 0x0d;
        case SDLK_BACKSPACE:        return 0x0e;
        case SDLK_TAB:              return 0x0f;
        case SDLK_q:                return 0x10;
        case SDLK_w:                return 0x11;
        case SDLK_e:                return 0x12;
        case SDLK_r:                return 0x13;
        case SDLK_t:                return 0x14;
        case SDLK_y:                return 0x15;
        case SDLK_u:                return 0x16;
        case SDLK_i:                return 0x17;
        case SDLK_o:                return 0x18;
        case SDLK_p:                return 0x19;
        case SDLK_LEFTBRACKET:      return 0x1a;
        case SDLK_RIGHTBRACKET:     return 0x1b;
        case SDLK_RETURN:           return 0x1c;
        case SDLK_KP_ENTER:         return 0x1c | 0x100;
        case SDLK_LCTRL:            return 0x1d;
        case SDLK_RCTRL:            return 0x1d | 0x100;
        case SDLK_a:                return 0x1e;
        case SDLK_s:                return 0x1f;
        case SDLK_d:                return 0x20;
        case SDLK_f:                return 0x21;
        case SDLK_g:                return 0x22;
        case SDLK_h:                return 0x23;
        case SDLK_j:                return 0x24;
        case SDLK_k:                return 0x25;
        case SDLK_l:                return 0x26;
        case SDLK_COLON:
        case SDLK_SEMICOLON:        return 0x27;
        case SDLK_QUOTEDBL:
        case SDLK_QUOTE:            return 0x28;
        case SDLK_BACKQUOTE:        return 0x29;
        case SDLK_LSHIFT:           return 0x2a;
        case SDLK_BACKSLASH:        return 0x2b;
        case SDLK_z:                return 0x2c;
        case SDLK_x:                return 0x2d;
        case SDLK_c:                return 0x2e;
        case SDLK_v:                return 0x2f;
        case SDLK_b:                return 0x30;
        case SDLK_n:                return 0x31;
        case SDLK_m:                return 0x32;
        case SDLK_LESS:
        case SDLK_COMMA:            return 0x33;
        case SDLK_GREATER:
        case SDLK_PERIOD:           return 0x34;
        case SDLK_KP_DIVIDE:        /*??*/
        case SDLK_QUESTION:
        case SDLK_SLASH:            return 0x35;
        case SDLK_RSHIFT:           return 0x36;
        case SDLK_KP_MULTIPLY:
        case SDLK_PRINT:            return 0x37; /* fixme */
        case SDLK_LALT:             return 0x38;
        case SDLK_MODE: /* alt gr*/
        case SDLK_RALT:             return 0x38 | 0x100;
        case SDLK_SPACE:            return 0x39;
        case SDLK_CAPSLOCK:         return 0x3a;
        case SDLK_F1:               return 0x3b;
        case SDLK_F2:               return 0x3c;
        case SDLK_F3:               return 0x3d;
        case SDLK_F4:               return 0x3e;
        case SDLK_F5:               return 0x3f;
        case SDLK_F6:               return 0x40;
        case SDLK_F7:               return 0x41;
        case SDLK_F8:               return 0x42;
        case SDLK_F9:               return 0x43;
        case SDLK_F10:              return 0x44;
        case SDLK_PAUSE:            return 0x45; /* not right */
        case SDLK_NUMLOCK:          return 0x45;
        case SDLK_SCROLLOCK:        return 0x46;
        case SDLK_KP7:              return 0x47;
        case SDLK_HOME:             return 0x47 | 0x100;
        case SDLK_KP8:              return 0x48;
        case SDLK_UP:               return 0x48 | 0x100;
        case SDLK_KP9:              return 0x49;
        case SDLK_PAGEUP:           return 0x49 | 0x100;
        case SDLK_KP_MINUS:         return 0x4a;
        case SDLK_KP4:              return 0x4b;
        case SDLK_LEFT:             return 0x4b | 0x100;
        case SDLK_KP5:              return 0x4c;
        case SDLK_KP6:              return 0x4d;
        case SDLK_RIGHT:            return 0x4d | 0x100;
        case SDLK_KP_PLUS:          return 0x4e;
        case SDLK_KP1:              return 0x4f;
        case SDLK_END:              return 0x4f | 0x100;
        case SDLK_KP2:              return 0x50;
        case SDLK_DOWN:             return 0x50 | 0x100;
        case SDLK_KP3:              return 0x51;
        case SDLK_PAGEDOWN:         return 0x51 | 0x100;
        case SDLK_KP0:              return 0x52;
        case SDLK_INSERT:           return 0x52 | 0x100;
        case SDLK_KP_PERIOD:        return 0x53;
        case SDLK_DELETE:           return 0x53 | 0x100;
        case SDLK_SYSREQ:           return 0x54;
        case SDLK_F11:              return 0x57;
        case SDLK_F12:              return 0x58;
        case SDLK_F13:              return 0x5b;
        case SDLK_LMETA:
        case SDLK_LSUPER:           return 0x5b | 0x100;
        case SDLK_F14:              return 0x5c;
        case SDLK_RMETA:
        case SDLK_RSUPER:           return 0x5c | 0x100;
        case SDLK_F15:              return 0x5d;
        case SDLK_MENU:             return 0x5d | 0x100;
#if 0
        case SDLK_CLEAR:            return 0x;
        case SDLK_KP_EQUALS:        return 0x;
        case SDLK_COMPOSE:          return 0x;
        case SDLK_HELP:             return 0x;
        case SDLK_BREAK:            return 0x;
        case SDLK_POWER:            return 0x;
        case SDLK_EURO:             return 0x;
        case SDLK_UNDO:             return 0x;
#endif
        default:
            Log(("Unhandled sdl key event: sym=%d scancode=%#x unicode=%#x\n",
                 ev->keysym.sym, ev->keysym.scancode, ev->keysym.unicode));
            return 0;
    }
}
#endif /* RT_OS_DARWIN */

/**
 * Converts an SDL keyboard eventcode to a XT scancode.
 *
 * @returns XT scancode
 * @param   ev SDL scancode
 */
static uint16_t Keyevent2Keycode(const SDL_KeyboardEvent *ev)
{
    // start with the scancode determined by SDL
    int keycode = ev->keysym.scancode;

#ifdef VBOXSDL_WITH_X11
# ifdef VBOX_WITH_SDL13

    switch (ev->keysym.sym)
    {
        case SDLK_ESCAPE:           return 0x01;
        case SDLK_EXCLAIM:
        case SDLK_1:                return 0x02;
        case SDLK_AT:
        case SDLK_2:                return 0x03;
        case SDLK_HASH:
        case SDLK_3:                return 0x04;
        case SDLK_DOLLAR:
        case SDLK_4:                return 0x05;
        /* % */
        case SDLK_5:                return 0x06;
        case SDLK_CARET:
        case SDLK_6:                return 0x07;
        case SDLK_AMPERSAND:
        case SDLK_7:                return 0x08;
        case SDLK_ASTERISK:
        case SDLK_8:                return 0x09;
        case SDLK_LEFTPAREN:
        case SDLK_9:                return 0x0a;
        case SDLK_RIGHTPAREN:
        case SDLK_0:                return 0x0b;
        case SDLK_UNDERSCORE:
        case SDLK_MINUS:            return 0x0c;
        case SDLK_PLUS:             return 0x0d;
        case SDLK_BACKSPACE:        return 0x0e;
        case SDLK_TAB:              return 0x0f;
        case SDLK_q:                return 0x10;
        case SDLK_w:                return 0x11;
        case SDLK_e:                return 0x12;
        case SDLK_r:                return 0x13;
        case SDLK_t:                return 0x14;
        case SDLK_y:                return 0x15;
        case SDLK_u:                return 0x16;
        case SDLK_i:                return 0x17;
        case SDLK_o:                return 0x18;
        case SDLK_p:                return 0x19;
        case SDLK_RETURN:           return 0x1c;
        case SDLK_KP_ENTER:         return 0x1c | 0x100;
        case SDLK_LCTRL:            return 0x1d;
        case SDLK_RCTRL:            return 0x1d | 0x100;
        case SDLK_a:                return 0x1e;
        case SDLK_s:                return 0x1f;
        case SDLK_d:                return 0x20;
        case SDLK_f:                return 0x21;
        case SDLK_g:                return 0x22;
        case SDLK_h:                return 0x23;
        case SDLK_j:                return 0x24;
        case SDLK_k:                return 0x25;
        case SDLK_l:                return 0x26;
        case SDLK_COLON:            return 0x27;
        case SDLK_QUOTEDBL:
        case SDLK_QUOTE:            return 0x28;
        case SDLK_BACKQUOTE:        return 0x29;
        case SDLK_LSHIFT:           return 0x2a;
        case SDLK_z:                return 0x2c;
        case SDLK_x:                return 0x2d;
        case SDLK_c:                return 0x2e;
        case SDLK_v:                return 0x2f;
        case SDLK_b:                return 0x30;
        case SDLK_n:                return 0x31;
        case SDLK_m:                return 0x32;
        case SDLK_LESS:             return 0x33;
        case SDLK_GREATER:          return 0x34;
        case SDLK_KP_DIVIDE:        /*??*/
        case SDLK_QUESTION:         return 0x35;
        case SDLK_RSHIFT:           return 0x36;
        case SDLK_KP_MULTIPLY:
        case SDLK_PRINT:            return 0x37; /* fixme */
        case SDLK_LALT:             return 0x38;
        case SDLK_MODE: /* alt gr*/
        case SDLK_RALT:             return 0x38 | 0x100;
        case SDLK_SPACE:            return 0x39;
        case SDLK_CAPSLOCK:         return 0x3a;
        case SDLK_F1:               return 0x3b;
        case SDLK_F2:               return 0x3c;
        case SDLK_F3:               return 0x3d;
        case SDLK_F4:               return 0x3e;
        case SDLK_F5:               return 0x3f;
        case SDLK_F6:               return 0x40;
        case SDLK_F7:               return 0x41;
        case SDLK_F8:               return 0x42;
        case SDLK_F9:               return 0x43;
        case SDLK_F10:              return 0x44;
        case SDLK_PAUSE:            return 0x45; /* not right */
        case SDLK_NUMLOCK:          return 0x45;
        case SDLK_SCROLLOCK:        return 0x46;
        case SDLK_KP7:              return 0x47;
        case SDLK_HOME:             return 0x47 | 0x100;
        case SDLK_KP8:              return 0x48;
        case SDLK_UP:               return 0x48 | 0x100;
        case SDLK_KP9:              return 0x49;
        case SDLK_PAGEUP:           return 0x49 | 0x100;
        case SDLK_KP_MINUS:         return 0x4a;
        case SDLK_KP4:              return 0x4b;
        case SDLK_LEFT:             return 0x4b | 0x100;
        case SDLK_KP5:              return 0x4c;
        case SDLK_KP6:              return 0x4d;
        case SDLK_RIGHT:            return 0x4d | 0x100;
        case SDLK_KP_PLUS:          return 0x4e;
        case SDLK_KP1:              return 0x4f;
        case SDLK_END:              return 0x4f | 0x100;
        case SDLK_KP2:              return 0x50;
        case SDLK_DOWN:             return 0x50 | 0x100;
        case SDLK_KP3:              return 0x51;
        case SDLK_PAGEDOWN:         return 0x51 | 0x100;
        case SDLK_KP0:              return 0x52;
        case SDLK_INSERT:           return 0x52 | 0x100;
        case SDLK_KP_PERIOD:        return 0x53;
        case SDLK_DELETE:           return 0x53 | 0x100;
        case SDLK_SYSREQ:           return 0x54;
        case SDLK_F11:              return 0x57;
        case SDLK_F12:              return 0x58;
        case SDLK_F13:              return 0x5b;
        case SDLK_F14:              return 0x5c;
        case SDLK_F15:              return 0x5d;
        case SDLK_MENU:             return 0x5d | 0x100;
        default:
                                    return 0;
    }
# else
    keycode = X11DRV_KeyEvent(gSdlInfo.info.x11.display, keycode);
# endif
#elif defined(RT_OS_DARWIN)
    /* This is derived partially from SDL_QuartzKeys.h and partially from testing. */
    static const uint16_t s_aMacToSet1[] =
    {
     /*  set-1            SDL_QuartzKeys.h    */
        0x1e,        /* QZ_a            0x00 */
        0x1f,        /* QZ_s            0x01 */
        0x20,        /* QZ_d            0x02 */
        0x21,        /* QZ_f            0x03 */
        0x23,        /* QZ_h            0x04 */
        0x22,        /* QZ_g            0x05 */
        0x2c,        /* QZ_z            0x06 */
        0x2d,        /* QZ_x            0x07 */
        0x2e,        /* QZ_c            0x08 */
        0x2f,        /* QZ_v            0x09 */
        0x56,        /* between lshift and z. 'INT 1'? */
        0x30,        /* QZ_b            0x0B */
        0x10,        /* QZ_q            0x0C */
        0x11,        /* QZ_w            0x0D */
        0x12,        /* QZ_e            0x0E */
        0x13,        /* QZ_r            0x0F */
        0x15,        /* QZ_y            0x10 */
        0x14,        /* QZ_t            0x11 */
        0x02,        /* QZ_1            0x12 */
        0x03,        /* QZ_2            0x13 */
        0x04,        /* QZ_3            0x14 */
        0x05,        /* QZ_4            0x15 */
        0x07,        /* QZ_6            0x16 */
        0x06,        /* QZ_5            0x17 */
        0x0d,        /* QZ_EQUALS       0x18 */
        0x0a,        /* QZ_9            0x19 */
        0x08,        /* QZ_7            0x1A */
        0x0c,        /* QZ_MINUS        0x1B */
        0x09,        /* QZ_8            0x1C */
        0x0b,        /* QZ_0            0x1D */
        0x1b,        /* QZ_RIGHTBRACKET 0x1E */
        0x18,        /* QZ_o            0x1F */
        0x16,        /* QZ_u            0x20 */
        0x1a,        /* QZ_LEFTBRACKET  0x21 */
        0x17,        /* QZ_i            0x22 */
        0x19,        /* QZ_p            0x23 */
        0x1c,        /* QZ_RETURN       0x24 */
        0x26,        /* QZ_l            0x25 */
        0x24,        /* QZ_j            0x26 */
        0x28,        /* QZ_QUOTE        0x27 */
        0x25,        /* QZ_k            0x28 */
        0x27,        /* QZ_SEMICOLON    0x29 */
        0x2b,        /* QZ_BACKSLASH    0x2A */
        0x33,        /* QZ_COMMA        0x2B */
        0x35,        /* QZ_SLASH        0x2C */
        0x31,        /* QZ_n            0x2D */
        0x32,        /* QZ_m            0x2E */
        0x34,        /* QZ_PERIOD       0x2F */
        0x0f,        /* QZ_TAB          0x30 */
        0x39,        /* QZ_SPACE        0x31 */
        0x29,        /* QZ_BACKQUOTE    0x32 */
        0x0e,        /* QZ_BACKSPACE    0x33 */
        0x9c,        /* QZ_IBOOK_ENTER  0x34 */
        0x01,        /* QZ_ESCAPE       0x35 */
        0x5c|0x100,  /* QZ_RMETA        0x36 */
        0x5b|0x100,  /* QZ_LMETA        0x37 */
        0x2a,        /* QZ_LSHIFT       0x38 */
        0x3a,        /* QZ_CAPSLOCK     0x39 */
        0x38,        /* QZ_LALT         0x3A */
        0x1d,        /* QZ_LCTRL        0x3B */
        0x36,        /* QZ_RSHIFT       0x3C */
        0x38|0x100,  /* QZ_RALT         0x3D */
        0x1d|0x100,  /* QZ_RCTRL        0x3E */
           0,        /*                      */
           0,        /*                      */
        0x53,        /* QZ_KP_PERIOD    0x41 */
           0,        /*                      */
        0x37,        /* QZ_KP_MULTIPLY  0x43 */
           0,        /*                      */
        0x4e,        /* QZ_KP_PLUS      0x45 */
           0,        /*                      */
        0x45,        /* QZ_NUMLOCK      0x47 */
           0,        /*                      */
           0,        /*                      */
           0,        /*                      */
        0x35|0x100,  /* QZ_KP_DIVIDE    0x4B */
        0x1c|0x100,  /* QZ_KP_ENTER     0x4C */
           0,        /*                      */
        0x4a,        /* QZ_KP_MINUS     0x4E */
           0,        /*                      */
           0,        /*                      */
        0x0d/*?*/,   /* QZ_KP_EQUALS    0x51 */
        0x52,        /* QZ_KP0          0x52 */
        0x4f,        /* QZ_KP1          0x53 */
        0x50,        /* QZ_KP2          0x54 */
        0x51,        /* QZ_KP3          0x55 */
        0x4b,        /* QZ_KP4          0x56 */
        0x4c,        /* QZ_KP5          0x57 */
        0x4d,        /* QZ_KP6          0x58 */
        0x47,        /* QZ_KP7          0x59 */
           0,        /*                      */
        0x48,        /* QZ_KP8          0x5B */
        0x49,        /* QZ_KP9          0x5C */
           0,        /*                      */
           0,        /*                      */
           0,        /*                      */
        0x3f,        /* QZ_F5           0x60 */
        0x40,        /* QZ_F6           0x61 */
        0x41,        /* QZ_F7           0x62 */
        0x3d,        /* QZ_F3           0x63 */
        0x42,        /* QZ_F8           0x64 */
        0x43,        /* QZ_F9           0x65 */
           0,        /*                      */
        0x57,        /* QZ_F11          0x67 */
           0,        /*                      */
        0x37|0x100,  /* QZ_PRINT / F13  0x69 */
        0x63,        /* QZ_F16          0x6A */
        0x46,        /* QZ_SCROLLOCK    0x6B */
           0,        /*                      */
        0x44,        /* QZ_F10          0x6D */
        0x5d|0x100,  /*                      */
        0x58,        /* QZ_F12          0x6F */
           0,        /*                      */
           0/* 0xe1,0x1d,0x45*/, /* QZ_PAUSE        0x71 */
        0x52|0x100,  /* QZ_INSERT / HELP 0x72 */
        0x47|0x100,  /* QZ_HOME         0x73 */
        0x49|0x100,   /* QZ_PAGEUP       0x74 */
        0x53|0x100,  /* QZ_DELETE       0x75 */
        0x3e,        /* QZ_F4           0x76 */
        0x4f|0x100,  /* QZ_END          0x77 */
        0x3c,        /* QZ_F2           0x78 */
        0x51|0x100,  /* QZ_PAGEDOWN     0x79 */
        0x3b,        /* QZ_F1           0x7A */
        0x4b|0x100,  /* QZ_LEFT         0x7B */
        0x4d|0x100,  /* QZ_RIGHT        0x7C */
        0x50|0x100,  /* QZ_DOWN         0x7D */
        0x48|0x100,  /* QZ_UP           0x7E */
        0x5e|0x100,  /* QZ_POWER        0x7F */ /* have different break key! */
    };

    if (keycode == 0)
    {
        /* This could be a modifier or it could be 'a'. */
        switch (ev->keysym.sym)
        {
            case SDLK_LSHIFT:           keycode = 0x2a; break;
            case SDLK_RSHIFT:           keycode = 0x36; break;
            case SDLK_LCTRL:            keycode = 0x1d; break;
            case SDLK_RCTRL:            keycode = 0x1d | 0x100; break;
            case SDLK_LALT:             keycode = 0x38; break;
            case SDLK_MODE: /* alt gr */
            case SDLK_RALT:             keycode = 0x38 | 0x100; break;
            case SDLK_RMETA:
            case SDLK_RSUPER:           keycode = 0x5c | 0x100; break;
            case SDLK_LMETA:
            case SDLK_LSUPER:           keycode = 0x5b | 0x100; break;
            /* Assumes normal key. */
            default:                    keycode = s_aMacToSet1[keycode]; break;
        }
    }
    else
    {
        if ((unsigned)keycode < RT_ELEMENTS(s_aMacToSet1))
            keycode = s_aMacToSet1[keycode];
        else
            keycode = 0;
        if (!keycode)
        {
# ifdef DEBUG_bird
            RTPrintf("Untranslated: keycode=%#x (%d)\n", keycode, keycode);
# endif
            keycode = Keyevent2KeycodeFallback(ev);
        }
    }
# ifdef DEBUG_bird
    RTPrintf("scancode=%#x -> %#x\n", ev->keysym.scancode, keycode);
# endif

#elif defined(RT_OS_OS2)
    keycode = Keyevent2KeycodeFallback(ev);
#endif /* RT_OS_DARWIN */
    return keycode;
}

/**
 * Releases any modifier keys that are currently in pressed state.
 */
static void ResetKeys(void)
{
    int i;

    if (!gpKeyboard)
        return;

    for(i = 0; i < 256; i++)
    {
        if (gaModifiersState[i])
        {
            if (i & 0x80)
                gpKeyboard->PutScancode(0xe0);
            gpKeyboard->PutScancode(i | 0x80);
            gaModifiersState[i] = 0;
        }
    }
}

/**
 * Keyboard event handler.
 *
 * @param ev SDL keyboard event.
 */
static void ProcessKey(SDL_KeyboardEvent *ev)
{
#if (defined(DEBUG) || defined(VBOX_WITH_STATISTICS)) && !defined(VBOX_WITH_SDL13)
    if (gpMachineDebugger && ev->type == SDL_KEYDOWN)
    {
        // first handle the debugger hotkeys
        uint8_t *keystate = SDL_GetKeyState(NULL);
#if 0
        // CTRL+ALT+Fn is not free on Linux hosts with Xorg ..
        if (keystate[SDLK_LALT] && !keystate[SDLK_LCTRL])
#else
        if (keystate[SDLK_LALT] && keystate[SDLK_LCTRL])
#endif
        {
            switch (ev->keysym.sym)
            {
                // pressing CTRL+ALT+F11 dumps the statistics counter
                case SDLK_F12:
                    RTPrintf("ResetStats\n"); /* Visual feedback in console window */
                    gpMachineDebugger->ResetStats(NULL);
                    break;
                // pressing CTRL+ALT+F12 resets all statistics counter
                case SDLK_F11:
                    gpMachineDebugger->DumpStats(NULL);
                    RTPrintf("DumpStats\n");  /* Vistual feedback in console window */
                    break;
                default:
                    break;
            }
        }
#if 1
        else if (keystate[SDLK_LALT] && !keystate[SDLK_LCTRL])
        {
            switch (ev->keysym.sym)
            {
                // pressing Alt-F12 toggles the supervisor recompiler
                case SDLK_F12:
                    {
                        BOOL recompileSupervisor;
                        gpMachineDebugger->COMGETTER(RecompileSupervisor)(&recompileSupervisor);
                        gpMachineDebugger->COMSETTER(RecompileSupervisor)(!recompileSupervisor);
                        break;
                    }
                    // pressing Alt-F11 toggles the user recompiler
                case SDLK_F11:
                    {
                        BOOL recompileUser;
                        gpMachineDebugger->COMGETTER(RecompileUser)(&recompileUser);
                        gpMachineDebugger->COMSETTER(RecompileUser)(!recompileUser);
                        break;
                    }
                    // pressing Alt-F10 toggles the patch manager
                case SDLK_F10:
                    {
                        BOOL patmEnabled;
                        gpMachineDebugger->COMGETTER(PATMEnabled)(&patmEnabled);
                        gpMachineDebugger->COMSETTER(PATMEnabled)(!patmEnabled);
                        break;
                    }
                    // pressing Alt-F9 toggles CSAM
                case SDLK_F9:
                    {
                        BOOL csamEnabled;
                        gpMachineDebugger->COMGETTER(CSAMEnabled)(&csamEnabled);
                        gpMachineDebugger->COMSETTER(CSAMEnabled)(!csamEnabled);
                        break;
                    }
                    // pressing Alt-F8 toggles singlestepping mode
                case SDLK_F8:
                    {
                        BOOL singlestepEnabled;
                        gpMachineDebugger->COMGETTER(SingleStep)(&singlestepEnabled);
                        gpMachineDebugger->COMSETTER(SingleStep)(!singlestepEnabled);
                        break;
                    }
                default:
                    break;
            }
        }
#endif
        // pressing Ctrl-F12 toggles the logger
        else if ((keystate[SDLK_RCTRL] || keystate[SDLK_LCTRL]) && ev->keysym.sym == SDLK_F12)
        {
            BOOL logEnabled = TRUE;
            gpMachineDebugger->COMGETTER(LogEnabled)(&logEnabled);
            gpMachineDebugger->COMSETTER(LogEnabled)(!logEnabled);
#ifdef DEBUG_bird
            return;
#endif
        }
        // pressing F12 sets a logmark
        else if (ev->keysym.sym == SDLK_F12)
        {
            RTLogPrintf("****** LOGGING MARK ******\n");
            RTLogFlush(NULL);
        }
        // now update the titlebar flags
        UpdateTitlebar(TITLEBAR_NORMAL);
    }
#endif // DEBUG || VBOX_WITH_STATISTICS

    // the pause key is the weirdest, needs special handling
    if (ev->keysym.sym == SDLK_PAUSE)
    {
        int v = 0;
        if (ev->type == SDL_KEYUP)
            v |= 0x80;
        gpKeyboard->PutScancode(0xe1);
        gpKeyboard->PutScancode(0x1d | v);
        gpKeyboard->PutScancode(0x45 | v);
        return;
    }

    /*
     * Perform SDL key event to scancode conversion
     */
    int keycode = Keyevent2Keycode(ev);

    switch(keycode)
    {
        case 0x00:
        {
            /* sent when leaving window: reset the modifiers state */
            ResetKeys();
            return;
        }

        case 0x2a:        /* Left Shift */
        case 0x36:        /* Right Shift */
        case 0x1d:        /* Left CTRL */
        case 0x1d|0x100:  /* Right CTRL */
        case 0x38:        /* Left ALT */
        case 0x38|0x100:  /* Right ALT */
        {
            if (ev->type == SDL_KEYUP)
                gaModifiersState[keycode & ~0x100] = 0;
            else
                gaModifiersState[keycode & ~0x100] = 1;
            break;
        }

        case 0x45: /* Num Lock */
        case 0x3a: /* Caps Lock */
        {
            /*
             * SDL generates a KEYDOWN event if the lock key is active and a  KEYUP event
             * if the lock key is inactive. See SDL_DISABLE_LOCK_KEYS.
             */
            if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP)
            {
                gpKeyboard->PutScancode(keycode);
                gpKeyboard->PutScancode(keycode | 0x80);
            }
            return;
        }
    }

    if (ev->type != SDL_KEYDOWN)
    {
        /*
         * Some keyboards (e.g. the one of mine T60) don't send a NumLock scan code on every
         * press of the key. Both the guest and the host should agree on the NumLock state.
         * If they differ, we try to alter the guest NumLock state by sending the NumLock key
         * scancode. We will get a feedback through the KBD_CMD_SET_LEDS command if the guest
         * tries to set/clear the NumLock LED. If a (silly) guest doesn't change the LED, don't
         * bother him with NumLock scancodes. At least our BIOS, Linux and Windows handle the
         * NumLock LED well.
         */
        if (   gcGuestNumLockAdaptions
            && (gfGuestNumLockPressed ^ !!(SDL_GetModState() & KMOD_NUM)))
        {
            gcGuestNumLockAdaptions--;
            gpKeyboard->PutScancode(0x45);
            gpKeyboard->PutScancode(0x45 | 0x80);
        }
        if (   gcGuestCapsLockAdaptions
            && (gfGuestCapsLockPressed ^ !!(SDL_GetModState() & KMOD_CAPS)))
        {
            gcGuestCapsLockAdaptions--;
            gpKeyboard->PutScancode(0x3a);
            gpKeyboard->PutScancode(0x3a | 0x80);
        }
    }

    /*
     * Now we send the event. Apply extended and release prefixes.
     */
    if (keycode & 0x100)
        gpKeyboard->PutScancode(0xe0);

    gpKeyboard->PutScancode(ev->type == SDL_KEYUP ? (keycode & 0x7f) | 0x80
                                                 : (keycode & 0x7f));
}

#ifdef RT_OS_DARWIN
#include <Carbon/Carbon.h>
RT_C_DECLS_BEGIN
/* Private interface in 10.3 and later. */
typedef int CGSConnection;
typedef enum
{
    kCGSGlobalHotKeyEnable = 0,
    kCGSGlobalHotKeyDisable,
    kCGSGlobalHotKeyInvalid = -1 /* bird */
} CGSGlobalHotKeyOperatingMode;
extern CGSConnection _CGSDefaultConnection(void);
extern CGError CGSGetGlobalHotKeyOperatingMode(CGSConnection Connection, CGSGlobalHotKeyOperatingMode *enmMode);
extern CGError CGSSetGlobalHotKeyOperatingMode(CGSConnection Connection, CGSGlobalHotKeyOperatingMode enmMode);
RT_C_DECLS_END

/** Keeping track of whether we disabled the hotkeys or not. */
static bool g_fHotKeysDisabled = false;
/** Whether we've connected or not. */
static bool g_fConnectedToCGS = false;
/** Cached connection. */
static CGSConnection g_CGSConnection;

/**
 * Disables or enabled global hot keys.
 */
static void DisableGlobalHotKeys(bool fDisable)
{
    if (!g_fConnectedToCGS)
    {
        g_CGSConnection = _CGSDefaultConnection();
        g_fConnectedToCGS = true;
    }

    /* get current mode. */
    CGSGlobalHotKeyOperatingMode enmMode = kCGSGlobalHotKeyInvalid;
    CGSGetGlobalHotKeyOperatingMode(g_CGSConnection, &enmMode);

    /* calc new mode. */
    if (fDisable)
    {
        if (enmMode != kCGSGlobalHotKeyEnable)
            return;
        enmMode = kCGSGlobalHotKeyDisable;
    }
    else
    {
        if (    enmMode != kCGSGlobalHotKeyDisable
            /*||  !g_fHotKeysDisabled*/)
            return;
        enmMode = kCGSGlobalHotKeyEnable;
    }

    /* try set it and check the actual result. */
    CGSSetGlobalHotKeyOperatingMode(g_CGSConnection, enmMode);
    CGSGlobalHotKeyOperatingMode enmNewMode = kCGSGlobalHotKeyInvalid;
    CGSGetGlobalHotKeyOperatingMode(g_CGSConnection, &enmNewMode);
    if (enmNewMode == enmMode)
        g_fHotKeysDisabled = enmMode == kCGSGlobalHotKeyDisable;
}
#endif /* RT_OS_DARWIN */

/**
 * Start grabbing the mouse.
 */
static void InputGrabStart(void)
{
#ifdef RT_OS_DARWIN
    DisableGlobalHotKeys(true);
#endif
    if (!gfGuestNeedsHostCursor && gfRelativeMouseGuest)
        SDL_ShowCursor(SDL_DISABLE);
    SDL_WM_GrabInput(SDL_GRAB_ON);
    // dummy read to avoid moving the mouse
    SDL_GetRelativeMouseState(
#ifdef VBOX_WITH_SDL13
                              0,
#endif
                              NULL, NULL);
    gfGrabbed = TRUE;
    UpdateTitlebar(TITLEBAR_NORMAL);
}

/**
 * End mouse grabbing.
 */
static void InputGrabEnd(void)
{
    SDL_WM_GrabInput(SDL_GRAB_OFF);
    if (!gfGuestNeedsHostCursor && gfRelativeMouseGuest)
        SDL_ShowCursor(SDL_ENABLE);
#ifdef RT_OS_DARWIN
    DisableGlobalHotKeys(false);
#endif
    gfGrabbed = FALSE;
    UpdateTitlebar(TITLEBAR_NORMAL);
}

/**
 * Query mouse position and button state from SDL and send to the VM
 *
 * @param dz  Relative mouse wheel movement
 */
static void SendMouseEvent(VBoxSDLFB *fb, int dz, int down, int button)
{
    int  x, y, state, buttons;
    bool abs;

#ifdef VBOX_WITH_SDL13
    if (!fb)
    {
        SDL_GetMouseState(0, &x, &y);
        RTPrintf("MouseEvent: Cannot find fb mouse = %d,%d\n", x, y);
        return;
    }
#else
    AssertRelease(fb != NULL);
#endif

    /*
     * If supported and we're not in grabbed mode, we'll use the absolute mouse.
     * If we are in grabbed mode and the guest is not able to draw the mouse cursor
     * itself, or can't handle relative reporting, we have to use absolute
     * coordinates, otherwise the host cursor and
     * the coordinates the guest thinks the mouse is at could get out-of-sync. From
     * the SDL mailing list:
     *
     * "The event processing is usually asynchronous and so somewhat delayed, and
     * SDL_GetMouseState is returning the immediate mouse state. So at the time you
     * call SDL_GetMouseState, the "button" is already up."
     */
    abs =    (UseAbsoluteMouse() && !gfGrabbed)
          || gfGuestNeedsHostCursor
          || !gfRelativeMouseGuest;

    /* only used if abs == TRUE */
    int  xOrigin = fb->getOriginX();
    int  yOrigin = fb->getOriginY();
    int  xMin = fb->getXOffset() + xOrigin;
    int  yMin = fb->getYOffset() + yOrigin;
    int  xMax = xMin + (int)fb->getGuestXRes();
    int  yMax = yMin + (int)fb->getGuestYRes();

    state = abs ? SDL_GetMouseState(
#ifdef VBOX_WITH_SDL13
                                    0,
#endif
                                    &x, &y)
                : SDL_GetRelativeMouseState(
#ifdef VBOX_WITH_SDL13
                                            0,
#endif
                                            &x, &y);

    /*
     * process buttons
     */
    buttons = 0;
    if (state & SDL_BUTTON(SDL_BUTTON_LEFT))
        buttons |= MouseButtonState_LeftButton;
    if (state & SDL_BUTTON(SDL_BUTTON_RIGHT))
        buttons |= MouseButtonState_RightButton;
    if (state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
        buttons |= MouseButtonState_MiddleButton;

    if (abs)
    {
        x += xOrigin;
        y += yOrigin;

        /*
         * Check if the mouse event is inside the guest area. This solves the
         * following problem: Some guests switch off the VBox hardware mouse
         * cursor and draw the mouse cursor itself instead. Moving the mouse
         * outside the guest area then leads to annoying mouse hangs if we
         * don't pass mouse motion events into the guest.
         */
        if (x < xMin || y < yMin || x > xMax || y > yMax)
        {
            /*
             * Cursor outside of valid guest area (outside window or in secure
             * label area. Don't allow any mouse button press.
             */
            button   = 0;

            /*
             * Release any pressed button.
             */
#if 0
            /* disabled on customers request */
            buttons &= ~(MouseButtonState_LeftButton   |
                         MouseButtonState_MiddleButton |
                         MouseButtonState_RightButton);
#endif

            /*
             * Prevent negative coordinates.
             */
            if (x < xMin) x = xMin;
            if (x > xMax) x = xMax;
            if (y < yMin) y = yMin;
            if (y > yMax) y = yMax;

            if (!gpOffCursor)
            {
                gpOffCursor       = SDL_GetCursor();    /* Cursor image */
                gfOffCursorActive = SDL_ShowCursor(-1); /* enabled / disabled */
                SDL_SetCursor(gpDefaultCursor);
                SDL_ShowCursor(SDL_ENABLE);
            }
        }
        else
        {
            if (gpOffCursor)
            {
                /*
                 * We just entered the valid guest area. Restore the guest mouse
                 * cursor.
                 */
                SDL_SetCursor(gpOffCursor);
                SDL_ShowCursor(gfOffCursorActive ? SDL_ENABLE : SDL_DISABLE);
                gpOffCursor = NULL;
            }
        }
    }

    /*
     * Button was pressed but that press is not reflected in the button state?
     */
    if (down && !(state & SDL_BUTTON(button)))
    {
        /*
         * It can happen that a mouse up event follows a mouse down event immediately
         * and we see the events when the bit in the button state is already cleared
         * again. In that case we simulate the mouse down event.
         */
        int tmp_button = 0;
        switch (button)
        {
            case SDL_BUTTON_LEFT:   tmp_button = MouseButtonState_LeftButton;   break;
            case SDL_BUTTON_MIDDLE: tmp_button = MouseButtonState_MiddleButton; break;
            case SDL_BUTTON_RIGHT:  tmp_button = MouseButtonState_RightButton;  break;
        }

        if (abs)
        {
            /**
             * @todo
             * PutMouseEventAbsolute() expects x and y starting from 1,1.
             * should we do the increment internally in PutMouseEventAbsolute()
             * or state it in PutMouseEventAbsolute() docs?
             */
            gpMouse->PutMouseEventAbsolute(x + 1 - xMin + xOrigin,
                                           y + 1 - yMin + yOrigin,
                                           dz, 0 /* horizontal scroll wheel */,
                                           buttons | tmp_button);
        }
        else
        {
            gpMouse->PutMouseEvent(0, 0, dz,
                                   0 /* horizontal scroll wheel */,
                                   buttons | tmp_button);
        }
    }

    // now send the mouse event
    if (abs)
    {
        /**
         * @todo
         * PutMouseEventAbsolute() expects x and y starting from 1,1.
         * should we do the increment internally in PutMouseEventAbsolute()
         * or state it in PutMouseEventAbsolute() docs?
         */
        gpMouse->PutMouseEventAbsolute(x + 1 - xMin + xOrigin,
                                       y + 1 - yMin + yOrigin,
                                       dz, 0 /* Horizontal wheel */, buttons);
    }
    else
    {
        gpMouse->PutMouseEvent(x, y, dz, 0 /* Horizontal wheel */, buttons);
    }
}

/**
 * Resets the VM
 */
void ResetVM(void)
{
    if (gpConsole)
        gpConsole->Reset();
}

/**
 * Initiates a saved state and updates the titlebar with progress information
 */
void SaveState(void)
{
    ResetKeys();
    RTThreadYield();
    if (gfGrabbed)
        InputGrabEnd();
    RTThreadYield();
    UpdateTitlebar(TITLEBAR_SAVE);
    gpProgress = NULL;
    HRESULT rc = gpMachine->SaveState(gpProgress.asOutParam());
    if (FAILED(rc))
    {
        RTPrintf("Error saving state! rc = 0x%x\n", rc);
        return;
    }
    Assert(gpProgress);

    /*
     * Wait for the operation to be completed and work
     * the title bar in the mean while.
     */
    ULONG    cPercent = 0;
#ifndef RT_OS_DARWIN /* don't break the other guys yet. */
    for (;;)
    {
        BOOL fCompleted = false;
        rc = gpProgress->COMGETTER(Completed)(&fCompleted);
        if (FAILED(rc) || fCompleted)
            break;
        ULONG cPercentNow;
        rc = gpProgress->COMGETTER(Percent)(&cPercentNow);
        if (FAILED(rc))
            break;
        if (cPercentNow != cPercent)
        {
            UpdateTitlebar(TITLEBAR_SAVE, cPercent);
            cPercent = cPercentNow;
        }

        /* wait */
        rc = gpProgress->WaitForCompletion(100);
        if (FAILED(rc))
            break;
        /// @todo process gui events.
    }

#else /* new loop which processes GUI events while saving. */

    /* start regular timer so we don't starve in the event loop */
    SDL_TimerID sdlTimer;
    sdlTimer = SDL_AddTimer(100, StartupTimer, NULL);

    for (;;)
    {
        /*
         * Check for completion.
         */
        BOOL fCompleted = false;
        rc = gpProgress->COMGETTER(Completed)(&fCompleted);
        if (FAILED(rc) || fCompleted)
            break;
        ULONG cPercentNow;
        rc = gpProgress->COMGETTER(Percent)(&cPercentNow);
        if (FAILED(rc))
            break;
        if (cPercentNow != cPercent)
        {
            UpdateTitlebar(TITLEBAR_SAVE, cPercent);
            cPercent = cPercentNow;
        }

        /*
         * Wait for and process GUI a event.
         * This is necessary for XPCOM IPC and for updating the
         * title bar on the Mac.
         */
        SDL_Event event;
        if (WaitSDLEvent(&event))
        {
            switch (event.type)
            {
                /*
                 * Timer event preventing us from getting stuck.
                 */
                case SDL_USER_EVENT_TIMER:
                    break;

#ifdef USE_XPCOM_QUEUE_THREAD
                /*
                 * User specific XPCOM event queue event
                 */
                case SDL_USER_EVENT_XPCOM_EVENTQUEUE:
                {
                    LogFlow(("SDL_USER_EVENT_XPCOM_EVENTQUEUE: processing XPCOM event queue...\n"));
                    eventQ->ProcessPendingEvents();
                    signalXPCOMEventQueueThread();
                    break;
                }
#endif /* USE_XPCOM_QUEUE_THREAD */


                /*
                 * Ignore all other events.
                 */
                case SDL_USER_EVENT_NOTIFYCHANGE:
                case SDL_USER_EVENT_TERMINATE:
                default:
                    break;
            }
        }
    }

    /* kill the timer */
    SDL_RemoveTimer(sdlTimer);
    sdlTimer = 0;

#endif /* RT_OS_DARWIN */

    /*
     * What's the result of the operation?
     */
    LONG lrc;
    rc = gpProgress->COMGETTER(ResultCode)(&lrc);
    if (FAILED(rc))
        lrc = ~0;
    if (!lrc)
    {
        UpdateTitlebar(TITLEBAR_SAVE, 100);
        RTThreadYield();
        RTPrintf("Saved the state successfully.\n");
    }
    else
        RTPrintf("Error saving state, lrc=%d (%#x)\n", lrc, lrc);
}

/**
 * Build the titlebar string
 */
static void UpdateTitlebar(TitlebarMode mode, uint32_t u32User)
{
    static char szTitle[1024] = {0};

    /* back up current title */
    char szPrevTitle[1024];
    strcpy(szPrevTitle, szTitle);

    Bstr bstrName;
    gpMachine->COMGETTER(Name)(bstrName.asOutParam());

    RTStrPrintf(szTitle, sizeof(szTitle), "%s - " VBOX_PRODUCT,
                !bstrName.isEmpty() ? Utf8Str(bstrName).c_str() : "<noname>");

    /* which mode are we in? */
    switch (mode)
    {
        case TITLEBAR_NORMAL:
        {
            MachineState_T machineState;
            gpMachine->COMGETTER(State)(&machineState);
            if (machineState == MachineState_Paused)
                RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle), " - [Paused]");

            if (gfGrabbed)
                RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle), " - [Input captured]");

#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
            // do we have a debugger interface
            if (gpMachineDebugger)
            {
                // query the machine state
                BOOL recompileSupervisor = FALSE;
                BOOL recompileUser = FALSE;
                BOOL patmEnabled = FALSE;
                BOOL csamEnabled = FALSE;
                BOOL singlestepEnabled = FALSE;
                BOOL logEnabled = FALSE;
                BOOL hwVirtEnabled = FALSE;
                ULONG virtualTimeRate = 100;
                gpMachineDebugger->COMGETTER(RecompileSupervisor)(&recompileSupervisor);
                gpMachineDebugger->COMGETTER(RecompileUser)(&recompileUser);
                gpMachineDebugger->COMGETTER(PATMEnabled)(&patmEnabled);
                gpMachineDebugger->COMGETTER(CSAMEnabled)(&csamEnabled);
                gpMachineDebugger->COMGETTER(LogEnabled)(&logEnabled);
                gpMachineDebugger->COMGETTER(SingleStep)(&singlestepEnabled);
                gpMachineDebugger->COMGETTER(HWVirtExEnabled)(&hwVirtEnabled);
                gpMachineDebugger->COMGETTER(VirtualTimeRate)(&virtualTimeRate);
                RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                            " [STEP=%d CS=%d PAT=%d RR0=%d RR3=%d LOG=%d HWVirt=%d",
                            singlestepEnabled == TRUE, csamEnabled == TRUE, patmEnabled == TRUE,
                            recompileSupervisor == FALSE, recompileUser == FALSE,
                            logEnabled == TRUE, hwVirtEnabled == TRUE);
                char *psz = strchr(szTitle, '\0');
                if (virtualTimeRate != 100)
                    RTStrPrintf(psz, &szTitle[sizeof(szTitle)] - psz, " WD=%d%%]", virtualTimeRate);
                else
                    RTStrPrintf(psz, &szTitle[sizeof(szTitle)] - psz, "]");
            }
#endif /* DEBUG || VBOX_WITH_STATISTICS */
            break;
        }

        case TITLEBAR_STARTUP:
        {
            /*
             * Format it.
             */
            MachineState_T machineState;
            gpMachine->COMGETTER(State)(&machineState);
            if (machineState == MachineState_Starting)
                RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                            " - Starting...");
            else if (machineState == MachineState_Restoring)
            {
                ULONG cPercentNow;
                HRESULT rc = gpProgress->COMGETTER(Percent)(&cPercentNow);
                if (SUCCEEDED(rc))
                    RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                                " - Restoring %d%%...", (int)cPercentNow);
                else
                    RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                                " - Restoring...");
            }
            else if (machineState == MachineState_TeleportingIn)
            {
                ULONG cPercentNow;
                HRESULT rc = gpProgress->COMGETTER(Percent)(&cPercentNow);
                if (SUCCEEDED(rc))
                    RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                                " - Teleporting %d%%...", (int)cPercentNow);
                else
                    RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                                " - Teleporting...");
            }
            /* ignore other states, we could already be in running or aborted state */
            break;
        }

        case TITLEBAR_SAVE:
        {
            AssertMsg(u32User <= 100, ("%d\n", u32User));
            RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                        " - Saving %d%%...", u32User);
            break;
        }

        case TITLEBAR_SNAPSHOT:
        {
            AssertMsg(u32User <= 100, ("%d\n", u32User));
            RTStrPrintf(szTitle + strlen(szTitle), sizeof(szTitle) - strlen(szTitle),
                        " - Taking snapshot %d%%...", u32User);
            break;
        }

        default:
            RTPrintf("Error: Invalid title bar mode %d!\n", mode);
            return;
    }

    /*
     * Don't update if it didn't change.
     */
    if (!strcmp(szTitle, szPrevTitle))
        return;

    /*
     * Set the new title
     */
#ifdef VBOX_WIN32_UI
    setUITitle(szTitle);
#else
    SDL_WM_SetCaption(szTitle, VBOX_PRODUCT);
#endif
}

#if 0
static void vbox_show_shape(unsigned short w, unsigned short h,
                            uint32_t bg, const uint8_t *image)
{
    size_t x, y;
    unsigned short pitch;
    const uint32_t *color;
    const uint8_t *mask;
    size_t size_mask;

    mask = image;
    pitch = (w + 7) / 8;
    size_mask = (pitch * h + 3) & ~3;

    color = (const uint32_t *)(image + size_mask);

    printf("show_shape %dx%d pitch %d size mask %d\n",
           w, h, pitch, size_mask);
    for (y = 0; y < h; ++y, mask += pitch, color += w)
    {
        for (x = 0; x < w; ++x) {
            if (mask[x / 8] & (1 << (7 - (x % 8))))
                printf(" ");
            else
            {
                uint32_t c = color[x];
                if (c == bg)
                    printf("Y");
                else
                    printf("X");
            }
        }
        printf("\n");
    }
}
#endif

/**
 *  Sets the pointer shape according to parameters.
 *  Must be called only from the main SDL thread.
 */
static void SetPointerShape(const PointerShapeChangeData *data)
{
    /*
     * don't allow to change the pointer shape if we are outside the valid
     * guest area. In that case set standard mouse pointer is set and should
     * not get overridden.
     */
    if (gpOffCursor)
        return;

    if (data->shape.size() > 0)
    {
        bool ok = false;

        uint32_t andMaskSize = (data->width + 7) / 8 * data->height;
        uint32_t srcShapePtrScan = data->width * 4;

        const uint8_t* shape = data->shape.raw();
        const uint8_t *srcAndMaskPtr = shape;
        const uint8_t *srcShapePtr = shape + ((andMaskSize + 3) & ~3);

#if 0
        /* pointer debugging code */
        // vbox_show_shape(data->width, data->height, 0, data->shape);
        uint32_t shapeSize = ((((data->width + 7) / 8) * data->height + 3) & ~3) + data->width * 4 * data->height;
        printf("visible: %d\n", data->visible);
        printf("width = %d\n", data->width);
        printf("height = %d\n", data->height);
        printf("alpha = %d\n", data->alpha);
        printf("xhot = %d\n", data->xHot);
        printf("yhot = %d\n", data->yHot);
        printf("uint8_t pointerdata[] = { ");
        for (uint32_t i = 0; i < shapeSize; i++)
        {
            printf("0x%x, ", data->shape[i]);
        }
        printf("};\n");
#endif

#if defined(RT_OS_WINDOWS)

        BITMAPV5HEADER bi;
        HBITMAP hBitmap;
        void *lpBits;
        HCURSOR hAlphaCursor = NULL;

        ::ZeroMemory(&bi, sizeof(BITMAPV5HEADER));
        bi.bV5Size = sizeof(BITMAPV5HEADER);
        bi.bV5Width = data->width;
        bi.bV5Height = -(LONG)data->height;
        bi.bV5Planes = 1;
        bi.bV5BitCount = 32;
        bi.bV5Compression = BI_BITFIELDS;
        // specify a supported 32 BPP alpha format for Windows XP
        bi.bV5RedMask   = 0x00FF0000;
        bi.bV5GreenMask = 0x0000FF00;
        bi.bV5BlueMask  = 0x000000FF;
        if (data->alpha)
            bi.bV5AlphaMask = 0xFF000000;
        else
            bi.bV5AlphaMask = 0;

        HDC hdc = ::GetDC(NULL);

        // create the DIB section with an alpha channel
        hBitmap = ::CreateDIBSection(hdc, (BITMAPINFO *)&bi, DIB_RGB_COLORS,
                                     (void **)&lpBits, NULL, (DWORD)0);

        ::ReleaseDC(NULL, hdc);

        HBITMAP hMonoBitmap = NULL;
        if (data->alpha)
        {
            // create an empty mask bitmap
            hMonoBitmap = ::CreateBitmap(data->width, data->height, 1, 1, NULL);
        }
        else
        {
            /* Word aligned AND mask. Will be allocated and created if necessary. */
            uint8_t *pu8AndMaskWordAligned = NULL;

            /* Width in bytes of the original AND mask scan line. */
            uint32_t cbAndMaskScan = (data->width + 7) / 8;

            if (cbAndMaskScan & 1)
            {
                /* Original AND mask is not word aligned. */

                /* Allocate memory for aligned AND mask. */
                pu8AndMaskWordAligned = (uint8_t *)RTMemTmpAllocZ((cbAndMaskScan + 1) * data->height);

                Assert(pu8AndMaskWordAligned);

                if (pu8AndMaskWordAligned)
                {
                    /* According to MSDN the padding bits must be 0.
                     * Compute the bit mask to set padding bits to 0 in the last byte of original AND mask.
                     */
                    uint32_t u32PaddingBits = cbAndMaskScan * 8  - data->width;
                    Assert(u32PaddingBits < 8);
                    uint8_t u8LastBytesPaddingMask = (uint8_t)(0xFF << u32PaddingBits);

                    Log(("u8LastBytesPaddingMask = %02X, aligned w = %d, width = %d, cbAndMaskScan = %d\n",
                          u8LastBytesPaddingMask, (cbAndMaskScan + 1) * 8, data->width, cbAndMaskScan));

                    uint8_t *src = (uint8_t *)srcAndMaskPtr;
                    uint8_t *dst = pu8AndMaskWordAligned;

                    unsigned i;
                    for (i = 0; i < data->height; i++)
                    {
                        memcpy(dst, src, cbAndMaskScan);

                        dst[cbAndMaskScan - 1] &= u8LastBytesPaddingMask;

                        src += cbAndMaskScan;
                        dst += cbAndMaskScan + 1;
                    }
                }
            }

            // create the AND mask bitmap
            hMonoBitmap = ::CreateBitmap(data->width, data->height, 1, 1,
                                         pu8AndMaskWordAligned? pu8AndMaskWordAligned: srcAndMaskPtr);

            if (pu8AndMaskWordAligned)
            {
                RTMemTmpFree(pu8AndMaskWordAligned);
            }
        }

        Assert(hBitmap);
        Assert(hMonoBitmap);
        if (hBitmap && hMonoBitmap)
        {
            DWORD *dstShapePtr = (DWORD *)lpBits;

            for (uint32_t y = 0; y < data->height; y ++)
            {
                memcpy(dstShapePtr, srcShapePtr, srcShapePtrScan);
                srcShapePtr += srcShapePtrScan;
                dstShapePtr += data->width;
            }

            ICONINFO ii;
            ii.fIcon = FALSE;
            ii.xHotspot = data->xHot;
            ii.yHotspot = data->yHot;
            ii.hbmMask = hMonoBitmap;
            ii.hbmColor = hBitmap;

            hAlphaCursor = ::CreateIconIndirect(&ii);
            Assert(hAlphaCursor);
            if (hAlphaCursor)
            {
                // here we do a dirty trick by substituting a Window Manager's
                // cursor handle with the handle we created

                WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;

                // see SDL12/src/video/wincommon/SDL_sysmouse.c
                void *wm_cursor = malloc(sizeof(HCURSOR) + sizeof(uint8_t *) * 2);
                *(HCURSOR *)wm_cursor = hAlphaCursor;

                gpCustomCursor->wm_cursor = (WMcursor *)wm_cursor;
                SDL_SetCursor(gpCustomCursor);
                SDL_ShowCursor(SDL_ENABLE);

                if (pCustomTempWMCursor)
                {
                    ::DestroyCursor(*(HCURSOR *)pCustomTempWMCursor);
                    free(pCustomTempWMCursor);
                }

                ok = true;
            }
        }

        if (hMonoBitmap)
            ::DeleteObject(hMonoBitmap);
        if (hBitmap)
            ::DeleteObject(hBitmap);

#elif defined(VBOXSDL_WITH_X11) && !defined(VBOX_WITHOUT_XCURSOR)

        if (gfXCursorEnabled)
        {
            XcursorImage *img = XcursorImageCreate(data->width, data->height);
            Assert(img);
            if (img)
            {
                img->xhot = data->xHot;
                img->yhot = data->yHot;

                XcursorPixel *dstShapePtr = img->pixels;

                for (uint32_t y = 0; y < data->height; y ++)
                {
                    memcpy(dstShapePtr, srcShapePtr, srcShapePtrScan);

                    if (!data->alpha)
                    {
                        // convert AND mask to the alpha channel
                        uint8_t byte = 0;
                        for (uint32_t x = 0; x < data->width; x ++)
                        {
                            if (!(x % 8))
                                byte = *(srcAndMaskPtr ++);
                            else
                                byte <<= 1;

                            if (byte & 0x80)
                            {
                                // Linux doesn't support inverted pixels (XOR ops,
                                // to be exact) in cursor shapes, so we detect such
                                // pixels and always replace them with black ones to
                                // make them visible at least over light colors
                                if (dstShapePtr [x] & 0x00FFFFFF)
                                    dstShapePtr [x] = 0xFF000000;
                                else
                                    dstShapePtr [x] = 0x00000000;
                            }
                            else
                                dstShapePtr [x] |= 0xFF000000;
                        }
                    }

                    srcShapePtr += srcShapePtrScan;
                    dstShapePtr += data->width;
                }

#ifndef VBOX_WITH_SDL13
                Cursor cur = XcursorImageLoadCursor(gSdlInfo.info.x11.display, img);
                Assert(cur);
                if (cur)
                {
                    // here we do a dirty trick by substituting a Window Manager's
                    // cursor handle with the handle we created

                    WMcursor *pCustomTempWMCursor = gpCustomCursor->wm_cursor;

                    // see SDL12/src/video/x11/SDL_x11mouse.c
                    void *wm_cursor = malloc(sizeof(Cursor));
                    *(Cursor *)wm_cursor = cur;

                    gpCustomCursor->wm_cursor = (WMcursor *)wm_cursor;
                    SDL_SetCursor(gpCustomCursor);
                    SDL_ShowCursor(SDL_ENABLE);

                    if (pCustomTempWMCursor)
                    {
                        XFreeCursor(gSdlInfo.info.x11.display, *(Cursor *)pCustomTempWMCursor);
                        free(pCustomTempWMCursor);
                    }

                    ok = true;
                }
#endif
            }
            XcursorImageDestroy(img);
        }

#endif /* VBOXSDL_WITH_X11 && !VBOX_WITHOUT_XCURSOR */

        if (!ok)
        {
            SDL_SetCursor(gpDefaultCursor);
            SDL_ShowCursor(SDL_ENABLE);
        }
    }
    else
    {
        if (data->visible)
            SDL_ShowCursor(SDL_ENABLE);
        else if (gfAbsoluteMouseGuest)
            /* Don't disable the cursor if the guest additions are not active (anymore) */
            SDL_ShowCursor(SDL_DISABLE);
    }
}

/**
 * Handle changed mouse capabilities
 */
static void HandleGuestCapsChanged(void)
{
    if (!gfAbsoluteMouseGuest)
    {
        // Cursor could be overwritten by the guest tools
        SDL_SetCursor(gpDefaultCursor);
        SDL_ShowCursor(SDL_ENABLE);
        gpOffCursor = NULL;
    }
    if (gpMouse && UseAbsoluteMouse())
    {
        // Actually switch to absolute coordinates
        if (gfGrabbed)
            InputGrabEnd();
        gpMouse->PutMouseEventAbsolute(-1, -1, 0, 0, 0);
    }
}

/**
 * Handles a host key down event
 */
static int HandleHostKey(const SDL_KeyboardEvent *pEv)
{
    /*
     * Revalidate the host key modifier
     */
    if ((SDL_GetModState() & ~(KMOD_MODE | KMOD_NUM | KMOD_RESERVED)) != gHostKeyMod)
        return VERR_NOT_SUPPORTED;

    /*
     * What was pressed?
     */
    switch (pEv->keysym.sym)
    {
        /* Control-Alt-Delete */
        case SDLK_DELETE:
        {
            gpKeyboard->PutCAD();
            break;
        }

        /*
         * Fullscreen / Windowed toggle.
         */
        case SDLK_f:
        {
            if (   strchr(gHostKeyDisabledCombinations, 'f')
                || !gfAllowFullscreenToggle)
                return VERR_NOT_SUPPORTED;

            /*
             * We have to pause/resume the machine during this
             * process because there might be a short moment
             * without a valid framebuffer
             */
            MachineState_T machineState;
            gpMachine->COMGETTER(State)(&machineState);
            bool fPauseIt = machineState == MachineState_Running
                         || machineState == MachineState_Teleporting
                         || machineState == MachineState_LiveSnapshotting;
            if (fPauseIt)
                gpConsole->Pause();
            SetFullscreen(!gpFramebuffer[0]->getFullscreen());
            if (fPauseIt)
                gpConsole->Resume();

            /*
             * We have switched from/to fullscreen, so request a full
             * screen repaint, just to be sure.
             */
            gpDisplay->InvalidateAndUpdate();
            break;
        }

        /*
         * Pause / Resume toggle.
         */
        case SDLK_p:
        {
            if (strchr(gHostKeyDisabledCombinations, 'p'))
                return VERR_NOT_SUPPORTED;

            MachineState_T machineState;
            gpMachine->COMGETTER(State)(&machineState);
            if (   machineState == MachineState_Running
                || machineState == MachineState_Teleporting
                || machineState == MachineState_LiveSnapshotting
               )
            {
                if (gfGrabbed)
                    InputGrabEnd();
                gpConsole->Pause();
            }
            else if (machineState == MachineState_Paused)
            {
                gpConsole->Resume();
            }
            UpdateTitlebar(TITLEBAR_NORMAL);
            break;
        }

        /*
         * Reset the VM
         */
        case SDLK_r:
        {
            if (strchr(gHostKeyDisabledCombinations, 'r'))
                return VERR_NOT_SUPPORTED;

            ResetVM();
            break;
        }

        /*
         * Terminate the VM
         */
        case SDLK_q:
        {
            if (strchr(gHostKeyDisabledCombinations, 'q'))
                return VERR_NOT_SUPPORTED;

            return VINF_EM_TERMINATE;
        }

        /*
         * Save the machine's state and exit
         */
        case SDLK_s:
        {
            if (strchr(gHostKeyDisabledCombinations, 's'))
                return VERR_NOT_SUPPORTED;

            SaveState();
            return VINF_EM_TERMINATE;
        }

        case SDLK_h:
        {
            if (strchr(gHostKeyDisabledCombinations, 'h'))
                return VERR_NOT_SUPPORTED;

            if (gpConsole)
                gpConsole->PowerButton();
            break;
        }

        /*
         * Perform an online snapshot. Continue operation.
         */
        case SDLK_n:
        {
            if (strchr(gHostKeyDisabledCombinations, 'n'))
                return VERR_NOT_SUPPORTED;

            RTThreadYield();
            ULONG cSnapshots = 0;
            gpMachine->COMGETTER(SnapshotCount)(&cSnapshots);
            char pszSnapshotName[20];
            RTStrPrintf(pszSnapshotName, sizeof(pszSnapshotName), "Snapshot %d", cSnapshots + 1);
            gpProgress = NULL;
            HRESULT rc;
            Bstr snapId;
            CHECK_ERROR(gpMachine, TakeSnapshot(Bstr(pszSnapshotName).raw(),
                                                Bstr("Taken by VBoxSDL").raw(),
                                                TRUE, snapId.asOutParam(),
                                                gpProgress.asOutParam()));
            if (FAILED(rc))
            {
                RTPrintf("Error taking snapshot! rc = 0x%x\n", rc);
                /* continue operation */
                return VINF_SUCCESS;
            }
            /*
             * Wait for the operation to be completed and work
             * the title bar in the mean while.
             */
            ULONG    cPercent = 0;
            for (;;)
            {
                BOOL fCompleted = false;
                rc = gpProgress->COMGETTER(Completed)(&fCompleted);
                if (FAILED(rc) || fCompleted)
                    break;
                ULONG cPercentNow;
                rc = gpProgress->COMGETTER(Percent)(&cPercentNow);
                if (FAILED(rc))
                    break;
                if (cPercentNow != cPercent)
                {
                    UpdateTitlebar(TITLEBAR_SNAPSHOT, cPercent);
                    cPercent = cPercentNow;
                }

                /* wait */
                rc = gpProgress->WaitForCompletion(100);
                if (FAILED(rc))
                    break;
                /// @todo process gui events.
            }

            /* continue operation */
            return VINF_SUCCESS;
        }

        case SDLK_F1: case SDLK_F2: case SDLK_F3:
        case SDLK_F4: case SDLK_F5: case SDLK_F6:
        case SDLK_F7: case SDLK_F8: case SDLK_F9:
        case SDLK_F10: case SDLK_F11: case SDLK_F12:
        {
            // /* send Ctrl-Alt-Fx to guest */
            com::SafeArray<LONG> keys(6);

            keys[0] = 0x1d; // Ctrl down
            keys[1] = 0x38; // Alt down
            keys[2] = Keyevent2Keycode(pEv); // Fx down
            keys[3] = keys[2] + 0x80; // Fx up
            keys[4] = 0xb8; // Alt up
            keys[5] = 0x9d;  // Ctrl up

            gpKeyboard->PutScancodes(ComSafeArrayAsInParam(keys), NULL);
            return VINF_SUCCESS;
        }

        /*
         * Not a host key combination.
         * Indicate this by returning false.
         */
        default:
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

/**
 * Timer callback function for startup processing
 */
static Uint32 StartupTimer(Uint32 interval, void *param)
{
    RT_NOREF(param);

    /* post message so we can do something in the startup loop */
    SDL_Event event = {0};
    event.type      = SDL_USEREVENT;
    event.user.type = SDL_USER_EVENT_TIMER;
    SDL_PushEvent(&event);
    RTSemEventSignal(g_EventSemSDLEvents);
    return interval;
}

/**
 * Timer callback function to check if resizing is finished
 */
static Uint32 ResizeTimer(Uint32 interval, void *param)
{
    RT_NOREF(interval, param);

    /* post message so the window is actually resized */
    SDL_Event event = {0};
    event.type      = SDL_USEREVENT;
    event.user.type = SDL_USER_EVENT_WINDOW_RESIZE_DONE;
    PushSDLEventForSure(&event);
    /* one-shot */
    return 0;
}

/**
 * Timer callback function to check if an ACPI power button event was handled by the guest.
 */
static Uint32 QuitTimer(Uint32 interval, void *param)
{
    RT_NOREF(interval, param);

    BOOL fHandled = FALSE;

    gSdlQuitTimer = NULL;
    if (gpConsole)
    {
        int rc = gpConsole->GetPowerButtonHandled(&fHandled);
        LogRel(("QuitTimer: rc=%d handled=%d\n", rc, fHandled));
        if (RT_FAILURE(rc) || !fHandled)
        {
            /* event was not handled, power down the guest */
            gfACPITerm = FALSE;
            SDL_Event event = {0};
            event.type = SDL_QUIT;
            PushSDLEventForSure(&event);
        }
    }
    /* one-shot */
    return 0;
}

/**
 * Wait for the next SDL event. Don't use SDL_WaitEvent since this function
 * calls SDL_Delay(10) if the event queue is empty.
 */
static int WaitSDLEvent(SDL_Event *event)
{
    for (;;)
    {
        int rc = SDL_PollEvent(event);
        if (rc == 1)
        {
#ifdef USE_XPCOM_QUEUE_THREAD
            if (event->type == SDL_USER_EVENT_XPCOM_EVENTQUEUE)
                consumedXPCOMUserEvent();
#endif
            return 1;
        }
        /* Immediately wake up if new SDL events are available. This does not
         * work for internal SDL events. Don't wait more than 10ms. */
        RTSemEventWait(g_EventSemSDLEvents, 10);
    }
}

/**
 * Ensure that an SDL event is really enqueued. Try multiple times if necessary.
 */
int PushSDLEventForSure(SDL_Event *event)
{
    int ntries = 10;
    for (; ntries > 0; ntries--)
    {
        int rc = SDL_PushEvent(event);
        RTSemEventSignal(g_EventSemSDLEvents);
#ifdef VBOX_WITH_SDL13
        if (rc == 1)
#else
        if (rc == 0)
#endif
            return 0;
        Log(("PushSDLEventForSure: waiting for 2ms (rc = %d)\n", rc));
        RTThreadSleep(2);
    }
    LogRel(("WARNING: Failed to enqueue SDL event %d.%d!\n",
            event->type, event->type == SDL_USEREVENT ? event->user.type : 0));
    return -1;
}

#ifdef VBOXSDL_WITH_X11
/**
 * Special SDL_PushEvent function for NotifyUpdate events. These events may occur in bursts
 * so make sure they don't flood the SDL event queue.
 */
void PushNotifyUpdateEvent(SDL_Event *event)
{
    int rc = SDL_PushEvent(event);
#ifdef VBOX_WITH_SDL13
    bool fSuccess = (rc == 1);
#else
    bool fSuccess = (rc == 0);
#endif

    RTSemEventSignal(g_EventSemSDLEvents);
    AssertMsg(fSuccess, ("SDL_PushEvent returned SDL error\n"));
    /* A global counter is faster than SDL_PeepEvents() */
    if (fSuccess)
        ASMAtomicIncS32(&g_cNotifyUpdateEventsPending);
    /* In order to not flood the SDL event queue, yield the CPU or (if there are already many
     * events queued) even sleep */
    if (g_cNotifyUpdateEventsPending > 96)
    {
        /* Too many NotifyUpdate events, sleep for a small amount to give the main thread time
         * to handle these events. The SDL queue can hold up to 128 events. */
        Log(("PushNotifyUpdateEvent: Sleep 1ms\n"));
        RTThreadSleep(1);
    }
    else
        RTThreadYield();
}
#endif /* VBOXSDL_WITH_X11 */

/**
 *
 */
static void SetFullscreen(bool enable)
{
    if (enable == gpFramebuffer[0]->getFullscreen())
        return;

    if (!gfFullscreenResize)
    {
        /*
         * The old/default way: SDL will resize the host to fit the guest screen resolution.
         */
        gpFramebuffer[0]->setFullscreen(enable);
    }
    else
    {
        /*
         * The alternate way: Switch to fullscreen with the host screen resolution and adapt
         * the guest screen resolution to the host window geometry.
         */
        uint32_t NewWidth = 0, NewHeight = 0;
        if (enable)
        {
            /* switch to fullscreen */
            gmGuestNormalXRes = gpFramebuffer[0]->getGuestXRes();
            gmGuestNormalYRes = gpFramebuffer[0]->getGuestYRes();
            gpFramebuffer[0]->getFullscreenGeometry(&NewWidth, &NewHeight);
        }
        else
        {
            /* switch back to saved geometry */
            NewWidth  = gmGuestNormalXRes;
            NewHeight = gmGuestNormalYRes;
        }
        if (NewWidth != 0 && NewHeight != 0)
        {
            gpFramebuffer[0]->setFullscreen(enable);
            gfIgnoreNextResize = TRUE;
            gpDisplay->SetVideoModeHint(0 /*=display*/, true /*=enabled*/,
                                        false /*=changeOrigin*/, 0 /*=originX*/, 0 /*=originY*/,
                                        NewWidth, NewHeight, 0 /*don't change bpp*/);
        }
    }
}

#ifdef VBOX_WITH_SDL13
static VBoxSDLFB * getFbFromWinId(SDL_WindowID id)
{
    for (unsigned i = 0; i < gcMonitors; i++)
        if (gpFramebuffer[i]->hasWindow(id))
            return gpFramebuffer[i];

    return NULL;
}
#endif
