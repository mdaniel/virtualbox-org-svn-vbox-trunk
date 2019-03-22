/* $Id$ */
/** @file
 * VirtualBox COM class implementation
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

#define LOG_GROUP LOG_GROUP_MAIN_KEYBOARD
#include "LoggingNew.h"

#include "KeyboardImpl.h"
#include "ConsoleImpl.h"

#include "AutoCaller.h"

#include <VBox/com/array.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/err.h>

#include <iprt/cpp/utils.h>


// defines
////////////////////////////////////////////////////////////////////////////////

// globals
////////////////////////////////////////////////////////////////////////////////

/** @name Keyboard device capabilities bitfield
 * @{ */
enum
{
    /** The keyboard device does not wish to receive keystrokes. */
    KEYBOARD_DEVCAP_DISABLED = 0,
    /** The keyboard device does wishes to receive keystrokes. */
    KEYBOARD_DEVCAP_ENABLED  = 1
};

/**
 * Keyboard driver instance data.
 */
typedef struct DRVMAINKEYBOARD
{
    /** Pointer to the keyboard object. */
    Keyboard                   *pKeyboard;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;
    /** Pointer to the keyboard port interface of the driver/device above us. */
    PPDMIKEYBOARDPORT           pUpPort;
    /** Our keyboard connector interface. */
    PDMIKEYBOARDCONNECTOR       IConnector;
    /** The capabilities of this device. */
    uint32_t                    u32DevCaps;
} DRVMAINKEYBOARD, *PDRVMAINKEYBOARD;


// constructor / destructor
////////////////////////////////////////////////////////////////////////////////

Keyboard::Keyboard()
    : mParent(NULL)
{
}

Keyboard::~Keyboard()
{
}

HRESULT Keyboard::FinalConstruct()
{
    RT_ZERO(mpDrv);
    menmLeds = PDMKEYBLEDS_NONE;
    return BaseFinalConstruct();
}

void Keyboard::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

// public methods
////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the keyboard object.
 *
 * @returns COM result indicator
 * @param aParent   handle of our parent object
 */
HRESULT Keyboard::init(Console *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    unconst(mEventSource).createObject();
    HRESULT rc = mEventSource->init();
    AssertComRCReturnRC(rc);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/**
 *  Uninitializes the instance and sets the ready flag to FALSE.
 *  Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Keyboard::uninit()
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    for (unsigned i = 0; i < KEYBOARD_MAX_DEVICES; ++i)
    {
        if (mpDrv[i])
            mpDrv[i]->pKeyboard = NULL;
        mpDrv[i] = NULL;
    }

    menmLeds = PDMKEYBLEDS_NONE;

    unconst(mParent) = NULL;
    unconst(mEventSource).setNull();
}

/**
 * Sends a scancode to the keyboard.
 *
 * @returns COM status code
 * @param aScancode The scancode to send
 */
HRESULT Keyboard::putScancode(LONG aScancode)
{
    std::vector<LONG> scancodes;
    scancodes.resize(1);
    scancodes[0] = aScancode;
    return putScancodes(scancodes, NULL);
}

/**
 * Sends a list of scancodes to the keyboard.
 *
 * @returns COM status code
 * @param aScancodes   Pointer to the first scancode
 * @param aCodesStored Address of variable to store the number
 *                     of scancodes that were sent to the keyboard.
                       This value can be NULL.
 */
HRESULT Keyboard::putScancodes(const std::vector<LONG> &aScancodes,
                               ULONG *aCodesStored)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    CHECK_CONSOLE_DRV(mpDrv[0]);

    /* Send input to the last enabled device. Relies on the fact that
     * the USB keyboard is always initialized after the PS/2 keyboard.
     */
    PPDMIKEYBOARDPORT pUpPort = NULL;
    for (int i = KEYBOARD_MAX_DEVICES - 1; i >= 0 ; --i)
    {
        if (mpDrv[i] && (mpDrv[i]->u32DevCaps & KEYBOARD_DEVCAP_ENABLED))
        {
            pUpPort = mpDrv[i]->pUpPort;
            break;
        }
    }

    /* No enabled keyboard - throw the input away. */
    if (!pUpPort)
    {
        if (aCodesStored)
            *aCodesStored = (uint32_t)aScancodes.size();
        return S_OK;
    }

    int vrc = VINF_SUCCESS;

    uint32_t sent;
    for (sent = 0; (sent < aScancodes.size()) && RT_SUCCESS(vrc); ++sent)
        vrc = pUpPort->pfnPutEventScan(pUpPort, (uint8_t)aScancodes[sent]);

    if (aCodesStored)
        *aCodesStored = sent;

    com::SafeArray<LONG> keys(aScancodes.size());
    for (size_t i = 0; i < aScancodes.size(); ++i)
        keys[i] = aScancodes[i];

    VBoxEventDesc evDesc;
    evDesc.init(mEventSource, VBoxEventType_OnGuestKeyboard, ComSafeArrayAsInParam(keys));
    evDesc.fire(0);

    if (RT_FAILURE(vrc))
        return setErrorBoth(VBOX_E_IPRT_ERROR, vrc,
                            tr("Could not send all scan codes to the virtual keyboard (%Rrc)"),
                            vrc);

    return S_OK;
}

/**
 * Sends Control-Alt-Delete to the keyboard. This could be done otherwise
 * but it's so common that we'll be nice and supply a convenience API.
 *
 * @returns COM status code
 *
 */
HRESULT Keyboard::putCAD()
{
    std::vector<LONG> cadSequence;
    cadSequence.resize(8);

    cadSequence[0] = 0x1d; // Ctrl down
    cadSequence[1] = 0x38; // Alt down
    cadSequence[2] = 0xe0; // Del down 1
    cadSequence[3] = 0x53; // Del down 2
    cadSequence[4] = 0xe0; // Del up 1
    cadSequence[5] = 0xd3; // Del up 2
    cadSequence[6] = 0xb8; // Alt up
    cadSequence[7] = 0x9d; // Ctrl up

    return putScancodes(cadSequence, NULL);
}

/**
 * Releases all currently held keys in the virtual keyboard.
 *
 * @returns COM status code
 *
 */
HRESULT Keyboard::releaseKeys()
{
    std::vector<LONG> scancodes;
    scancodes.resize(1);
    scancodes[0] = 0xFC;    /* Magic scancode, see PS/2 and USB keyboard devices. */
    return putScancodes(scancodes, NULL);
}

HRESULT Keyboard::getKeyboardLEDs(std::vector<KeyboardLED_T> &aKeyboardLEDs)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aKeyboardLEDs.resize(0);

    if (menmLeds & PDMKEYBLEDS_NUMLOCK)    aKeyboardLEDs.push_back(KeyboardLED_NumLock);
    if (menmLeds & PDMKEYBLEDS_CAPSLOCK)   aKeyboardLEDs.push_back(KeyboardLED_CapsLock);
    if (menmLeds & PDMKEYBLEDS_SCROLLLOCK) aKeyboardLEDs.push_back(KeyboardLED_ScrollLock);

    return S_OK;
}

HRESULT Keyboard::getEventSource(ComPtr<IEventSource> &aEventSource)
{
    // No need to lock - lifetime constant
    mEventSource.queryInterfaceTo(aEventSource.asOutParam());

    return S_OK;
}

//
// private methods
//
void Keyboard::onKeyboardLedsChange(PDMKEYBLEDS enmLeds)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* Save the current status. */
    menmLeds = enmLeds;

    alock.release();

    i_getParent()->i_onKeyboardLedsChange(RT_BOOL(enmLeds & PDMKEYBLEDS_NUMLOCK),
                                          RT_BOOL(enmLeds & PDMKEYBLEDS_CAPSLOCK),
                                          RT_BOOL(enmLeds & PDMKEYBLEDS_SCROLLLOCK));
}

DECLCALLBACK(void) Keyboard::i_keyboardLedStatusChange(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds)
{
    PDRVMAINKEYBOARD pDrv = RT_FROM_MEMBER(pInterface, DRVMAINKEYBOARD, IConnector);
    pDrv->pKeyboard->onKeyboardLedsChange(enmLeds);
}

/**
 * @interface_method_impl{PDMIKEYBOARDCONNECTOR,pfnSetActive}
 */
DECLCALLBACK(void) Keyboard::i_keyboardSetActive(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive)
{
    PDRVMAINKEYBOARD pDrv = RT_FROM_MEMBER(pInterface, DRVMAINKEYBOARD, IConnector);
    if (fActive)
        pDrv->u32DevCaps |= KEYBOARD_DEVCAP_ENABLED;
    else
        pDrv->u32DevCaps &= ~KEYBOARD_DEVCAP_ENABLED;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) Keyboard::i_drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS          pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVMAINKEYBOARD    pDrv    = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIKEYBOARDCONNECTOR, &pDrv->IConnector);
    return NULL;
}


/**
 * Destruct a keyboard driver instance.
 *
 * @returns VBox status code.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) Keyboard::i_drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVMAINKEYBOARD pThis = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));

    if (pThis->pKeyboard)
    {
        AutoWriteLock kbdLock(pThis->pKeyboard COMMA_LOCKVAL_SRC_POS);
        for (unsigned cDev = 0; cDev < KEYBOARD_MAX_DEVICES; ++cDev)
            if (pThis->pKeyboard->mpDrv[cDev] == pThis)
            {
                pThis->pKeyboard->mpDrv[cDev] = NULL;
                break;
            }
    }
}

/**
 * Construct a keyboard driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) Keyboard::i_drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVMAINKEYBOARD pThis = PDMINS_2_DATA(pDrvIns, PDRVMAINKEYBOARD);
    LogFlow(("Keyboard::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface      = Keyboard::i_drvQueryInterface;

    pThis->IConnector.pfnLedStatusChange  = i_keyboardLedStatusChange;
    pThis->IConnector.pfnSetActive        = Keyboard::i_keyboardSetActive;

    /*
     * Get the IKeyboardPort interface of the above driver/device.
     */
    pThis->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIKEYBOARDPORT);
    if (!pThis->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No keyboard port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Keyboard object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    pThis->pKeyboard = (Keyboard *)pv;        /** @todo Check this cast! */
    unsigned cDev;
    for (cDev = 0; cDev < KEYBOARD_MAX_DEVICES; ++cDev)
        if (!pThis->pKeyboard->mpDrv[cDev])
        {
            pThis->pKeyboard->mpDrv[cDev] = pThis;
            break;
        }
    if (cDev == KEYBOARD_MAX_DEVICES)
        return VERR_NO_MORE_HANDLES;

    return VINF_SUCCESS;
}


/**
 * Keyboard driver registration record.
 */
const PDMDRVREG Keyboard::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainKeyboard",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main keyboard driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_KEYBOARD,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVMAINKEYBOARD),
    /* pfnConstruct */
    Keyboard::i_drvConstruct,
    /* pfnDestruct */
    Keyboard::i_drvDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
