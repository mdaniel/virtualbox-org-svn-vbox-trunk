/** @file
 * PDM - Pluggable Device Manager, Interfaces.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmm_pdmifs_h
#define ___VBox_vmm_pdmifs_h

#include <iprt/sg.h>
#include <VBox/types.h>
#include <VBox/hgcmsvc.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_interfaces    The PDM Interface Definitions
 * @ingroup grp_pdm
 *
 * For historical reasons (the PDMINTERFACE enum) a lot of interface was stuffed
 * together in this group instead, dragging stuff into global space that didn't
 * need to be there and making this file huge (>2500 lines).  Since we're using
 * UUIDs as interface identifiers (IIDs) now, no only generic PDM interface will
 * be added to this file.  Component specific interface should be defined in the
 * header file of that component.
 *
 * Interfaces consists of a method table (typedef'ed struct) and an interface
 * ID.  The typename of the method table should have an 'I' in it, be all
 * capitals and according to the rules, no underscores.  The interface ID is a
 * \#define constructed by appending '_IID' to the typename. The IID value is a
 * UUID string on the form "a2299c0d-b709-4551-aa5a-73f59ffbed74".  If you stick
 * to these rules, you can make use of the PDMIBASE_QUERY_INTERFACE and
 * PDMIBASE_RETURN_INTERFACE when querying interface and implementing
 * PDMIBASE::pfnQueryInterface respectively.
 *
 * In most interface descriptions the orientation of the interface is given as
 * 'down' or 'up'.  This refers to a model with the device on the top and the
 * drivers stacked below it.  Sometimes there is mention of 'main' or 'external'
 * which normally means the same, i.e. the Main or VBoxBFE API.  Picture the
 * orientation of 'main' as horizontal.
 *
 * @{
 */


/** @name PDMIBASE
 * @{
 */

/**
 * PDM Base Interface.
 *
 * Everyone implements this.
 */
typedef struct PDMIBASE
{
    /**
     * Queries an interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(void *, pfnQueryInterface,(struct PDMIBASE *pInterface, const char *pszIID));
} PDMIBASE;
/** PDMIBASE interface ID. */
#define PDMIBASE_IID                            "a2299c0d-b709-4551-aa5a-73f59ffbed74"

/**
 * Helper macro for querying an interface from PDMIBASE.
 *
 * @returns Correctly typed PDMIBASE::pfnQueryInterface return value.
 *
 * @param   pIBase          Pointer to the base interface.
 * @param   InterfaceType   The interface type name.  The interface ID is
 *                          derived from this by appending _IID.
 */
#define PDMIBASE_QUERY_INTERFACE(pIBase, InterfaceType)  \
    ( (InterfaceType *)(pIBase)->pfnQueryInterface(pIBase, InterfaceType##_IID ) )

/**
 * Helper macro for implementing PDMIBASE::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type name.  The interface ID is
 *                          derived from this by appending _IID.
 * @param   pInterface      The interface address expression.
 */
#define PDMIBASE_RETURN_INTERFACE(pszIID, InterfaceType, pInterface)  \
    do { \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            P##InterfaceType pReturnInterfaceTypeCheck = (pInterface); \
            return pReturnInterfaceTypeCheck; \
        } \
    } while (0)

/** @} */


/** @name PDMIBASERC
 * @{
 */

/**
 * PDM Base Interface for querying ring-mode context interfaces in
 * ring-3.
 *
 * This is mandatory for drivers present in raw-mode context.
 */
typedef struct PDMIBASERC
{
    /**
     * Queries an ring-mode context interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(RTRCPTR, pfnQueryInterface,(struct PDMIBASERC *pInterface, const char *pszIID));
} PDMIBASERC;
/** Pointer to a PDM Base Interface for query ring-mode context interfaces. */
typedef PDMIBASERC *PPDMIBASERC;
/** PDMIBASERC interface ID. */
#define PDMIBASERC_IID                          "f6a6c649-6cb3-493f-9737-4653f221aeca"

/**
 * Helper macro for querying an interface from PDMIBASERC.
 *
 * @returns PDMIBASERC::pfnQueryInterface return value.
 *
 * @param   pIBaseRC        Pointer to the base raw-mode context interface.  Can
 *                          be NULL.
 * @param   InterfaceType   The interface type base name, no trailing RC.  The
 *                          interface ID is derived from this by appending _IID.
 *
 * @remarks Unlike PDMIBASE_QUERY_INTERFACE, this macro is not able to do any
 *          implicit type checking for you.
 */
#define PDMIBASERC_QUERY_INTERFACE(pIBaseRC, InterfaceType)  \
    ( (P##InterfaceType##RC)((pIBaseRC) ? (pIBaseRC)->pfnQueryInterface(pIBaseRC, InterfaceType##_IID) : NIL_RTRCPTR) )

/**
 * Helper macro for implementing PDMIBASERC::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pIns            Pointer to the instance data.
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type base name, no trailing RC.  The
 *                          interface ID is derived from this by appending _IID.
 * @param   pInterface      The interface address expression.  This must resolve
 *                          to some address within the instance data.
 * @remarks Don't use with PDMIBASE.
 */
#define PDMIBASERC_RETURN_INTERFACE(pIns, pszIID, InterfaceType, pInterface)  \
    do { \
        Assert((uintptr_t)pInterface - PDMINS_2_DATA(pIns, uintptr_t) < _4M); \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            InterfaceType##RC *pReturnInterfaceTypeCheck = (pInterface); \
            return (uintptr_t)pReturnInterfaceTypeCheck \
                 - PDMINS_2_DATA(pIns, uintptr_t) \
                 + PDMINS_2_DATA_RCPTR(pIns); \
        } \
    } while (0)

/** @} */


/** @name PDMIBASER0
 * @{
 */

/**
 * PDM Base Interface for querying ring-0 interfaces in ring-3.
 *
 * This is mandatory for drivers present in ring-0 context.
 */
typedef struct PDMIBASER0
{
    /**
     * Queries an ring-0 interface to the driver.
     *
     * @returns Pointer to interface.
     * @returns NULL if the interface was not supported by the driver.
     * @param   pInterface          Pointer to this interface structure.
     * @param   pszIID              The interface ID, a UUID string.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(RTR0PTR, pfnQueryInterface,(struct PDMIBASER0 *pInterface, const char *pszIID));
} PDMIBASER0;
/** Pointer to a PDM Base Interface for query ring-0 context interfaces. */
typedef PDMIBASER0 *PPDMIBASER0;
/** PDMIBASER0 interface ID. */
#define PDMIBASER0_IID                          "9c9b99b8-7f53-4f59-a3c2-5bc9659c7944"

/**
 * Helper macro for querying an interface from PDMIBASER0.
 *
 * @returns PDMIBASER0::pfnQueryInterface return value.
 *
 * @param   pIBaseR0        Pointer to the base ring-0 interface.  Can be NULL.
 * @param   InterfaceType   The interface type base name, no trailing R0.  The
 *                          interface ID is derived from this by appending _IID.
 *
 * @remarks Unlike PDMIBASE_QUERY_INTERFACE, this macro is not able to do any
 *          implicit type checking for you.
 */
#define PDMIBASER0_QUERY_INTERFACE(pIBaseR0, InterfaceType)  \
    ( (P##InterfaceType##R0)((pIBaseR0) ? (pIBaseR0)->pfnQueryInterface(pIBaseR0, InterfaceType##_IID) : NIL_RTR0PTR) )

/**
 * Helper macro for implementing PDMIBASER0::pfnQueryInterface.
 *
 * Return @a pInterface if @a pszIID matches the @a InterfaceType.  This will
 * perform basic type checking.
 *
 * @param   pIns            Pointer to the instance data.
 * @param   pszIID          The ID of the interface that is being queried.
 * @param   InterfaceType   The interface type base name, no trailing R0.  The
 *                          interface ID is derived from this by appending _IID.
 * @param   pInterface      The interface address expression.  This must resolve
 *                          to some address within the instance data.
 * @remarks Don't use with PDMIBASE.
 */
#define PDMIBASER0_RETURN_INTERFACE(pIns, pszIID, InterfaceType, pInterface)  \
    do { \
        Assert((uintptr_t)pInterface - PDMINS_2_DATA(pIns, uintptr_t) < _4M); \
        if (RTUuidCompare2Strs((pszIID), InterfaceType##_IID) == 0) \
        { \
            InterfaceType##R0 *pReturnInterfaceTypeCheck = (pInterface); \
            return (uintptr_t)pReturnInterfaceTypeCheck \
                 - PDMINS_2_DATA(pIns, uintptr_t) \
                 + PDMINS_2_DATA_R0PTR(pIns); \
        } \
    } while (0)

/** @} */


/**
 * Dummy interface.
 *
 * This is used to typedef other dummy interfaces. The purpose of a dummy
 * interface is to validate the logical function of a driver/device and
 * full a natural interface pair.
 */
typedef struct PDMIDUMMY
{
    RTHCPTR pvDummy;
} PDMIDUMMY;


/** Pointer to a mouse port interface. */
typedef struct PDMIMOUSEPORT *PPDMIMOUSEPORT;
/**
 * Mouse port interface (down).
 * Pair with PDMIMOUSECONNECTOR.
 */
typedef struct PDMIMOUSEPORT
{
    /**
     * Puts a mouse event.
     *
     * This is called by the source of mouse events.  The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface     Pointer to this interface structure.
     * @param   dx             The X delta.
     * @param   dy             The Y delta.
     * @param   dz             The Z delta.
     * @param   dw             The W (horizontal scroll button) delta.
     * @param   fButtons       The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEvent,(PPDMIMOUSEPORT pInterface,
                                           int32_t dx, int32_t dy, int32_t dz,
                                           int32_t dw, uint32_t fButtons));
    /**
     * Puts an absolute mouse event.
     *
     * This is called by the source of mouse events.  The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface     Pointer to this interface structure.
     * @param   x              The X value, in the range 0 to 0xffff.
     * @param   y              The Y value, in the range 0 to 0xffff.
     * @param   dz             The Z delta.
     * @param   dw             The W (horizontal scroll button) delta.
     * @param   fButtons       The button states, see the PDMIMOUSEPORT_BUTTON_* \#defines.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEventAbs,(PPDMIMOUSEPORT pInterface,
                                              uint32_t x, uint32_t y,
                                              int32_t dz, int32_t dw,
                                              uint32_t fButtons));
    /**
     * Puts a multi-touch event.
     *
     * @returns VBox status code. Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   cContacts           How many touch contacts in this event.
     * @param   pau64Contacts       Pointer to array of packed contact information.
     *                              Each 64bit element contains:
     *                              Bits 0..15:  X coordinate in pixels (signed).
     *                              Bits 16..31: Y coordinate in pixels (signed).
     *                              Bits 32..39: contact identifier.
     *                              Bit 40:      "in contact" flag, which indicates that
     *                                           there is a contact with the touch surface.
     *                              Bit 41:      "in range" flag, the contact is close enough
     *                                           to the touch surface.
     *                              All other bits are reserved for future use and must be set to 0.
     * @param   u32ScanTime         Timestamp of this event in milliseconds. Only relative
     *                              time between event is important.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEventMultiTouch,(PPDMIMOUSEPORT pInterface,
                                                     uint8_t cContacts,
                                                     const uint64_t *pau64Contacts,
                                                     uint32_t u32ScanTime));
} PDMIMOUSEPORT;
/** PDMIMOUSEPORT interface ID. */
#define PDMIMOUSEPORT_IID                       "359364f0-9fa3-4490-a6b4-7ed771901c93"

/** Mouse button defines for PDMIMOUSEPORT::pfnPutEvent.
 * @{ */
#define PDMIMOUSEPORT_BUTTON_LEFT   RT_BIT(0)
#define PDMIMOUSEPORT_BUTTON_RIGHT  RT_BIT(1)
#define PDMIMOUSEPORT_BUTTON_MIDDLE RT_BIT(2)
#define PDMIMOUSEPORT_BUTTON_X1     RT_BIT(3)
#define PDMIMOUSEPORT_BUTTON_X2     RT_BIT(4)
/** @} */


/** Pointer to a mouse connector interface. */
typedef struct PDMIMOUSECONNECTOR *PPDMIMOUSECONNECTOR;
/**
 * Mouse connector interface (up).
 * Pair with PDMIMOUSEPORT.
 */
typedef struct PDMIMOUSECONNECTOR
{
    /**
     * Notifies the the downstream driver of changes to the reporting modes
     * supported by the driver
     *
     * @param   pInterface      Pointer to this interface structure.
     * @param   fRelative       Whether relative mode is currently supported.
     * @param   fAbsolute       Whether absolute mode is currently supported.
     * @param   fMultiTouch     Whether multi-touch mode is currently supported.
     */
    DECLR3CALLBACKMEMBER(void, pfnReportModes,(PPDMIMOUSECONNECTOR pInterface, bool fRelative, bool fAbsolute, bool fMultiTouch));

    /**
     * Flushes the mouse queue if it contains pending events.
     *
     * @param   pInterface      Pointer to this interface structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnFlushQueue,(PPDMIMOUSECONNECTOR pInterface));

} PDMIMOUSECONNECTOR;
/** PDMIMOUSECONNECTOR interface ID.  */
#define PDMIMOUSECONNECTOR_IID                  "ce64d7bd-fa8f-41d1-a6fb-d102a2d6bffe"


/** Pointer to a keyboard port interface. */
typedef struct PDMIKEYBOARDPORT *PPDMIKEYBOARDPORT;
/**
 * Keyboard port interface (down).
 * Pair with PDMIKEYBOARDCONNECTOR.
 */
typedef struct PDMIKEYBOARDPORT
{
    /**
     * Puts a scan code based keyboard event.
     *
     * This is called by the source of keyboard events. The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   u8ScanCode          The scan code to queue.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEventScan,(PPDMIKEYBOARDPORT pInterface, uint8_t u8KeyCode));

    /**
     * Puts a USB HID usage ID based keyboard event.
     *
     * This is called by the source of keyboard events. The event will be passed up
     * until the topmost driver, which then calls the registered event handler.
     *
     * @returns VBox status code.  Return VERR_TRY_AGAIN if you cannot process the
     *          event now and want it to be repeated at a later point.
     *
     * @param   pInterface          Pointer to this interface structure.
     * @param   u32UsageID          The HID usage code event to queue.
     */
    DECLR3CALLBACKMEMBER(int, pfnPutEventHid,(PPDMIKEYBOARDPORT pInterface, uint32_t u32UsageID));
} PDMIKEYBOARDPORT;
/** PDMIKEYBOARDPORT interface ID. */
#define PDMIKEYBOARDPORT_IID                    "2a0844f0-410b-40ab-a6ed-6575f3aa3e29"


/**
 * Keyboard LEDs.
 */
typedef enum PDMKEYBLEDS
{
    /** No leds. */
    PDMKEYBLEDS_NONE             = 0x0000,
    /** Num Lock */
    PDMKEYBLEDS_NUMLOCK          = 0x0001,
    /** Caps Lock */
    PDMKEYBLEDS_CAPSLOCK         = 0x0002,
    /** Scroll Lock */
    PDMKEYBLEDS_SCROLLLOCK       = 0x0004
} PDMKEYBLEDS;

/** Pointer to keyboard connector interface. */
typedef struct PDMIKEYBOARDCONNECTOR *PPDMIKEYBOARDCONNECTOR;
/**
 * Keyboard connector interface (up).
 * Pair with PDMIKEYBOARDPORT
 */
typedef struct PDMIKEYBOARDCONNECTOR
{
    /**
     * Notifies the the downstream driver about an LED change initiated by the guest.
     *
     * @param   pInterface      Pointer to this interface structure.
     * @param   enmLeds         The new led mask.
     */
    DECLR3CALLBACKMEMBER(void, pfnLedStatusChange,(PPDMIKEYBOARDCONNECTOR pInterface, PDMKEYBLEDS enmLeds));

    /**
     * Notifies the the downstream driver of changes in driver state.
     *
     * @param   pInterface      Pointer to this interface structure.
     * @param   fActive         Whether interface wishes to get "focus".
     */
    DECLR3CALLBACKMEMBER(void, pfnSetActive,(PPDMIKEYBOARDCONNECTOR pInterface, bool fActive));

    /**
     * Flushes the keyboard queue if it contains pending events.
     *
     * @param   pInterface      Pointer to this interface structure.
     */
    DECLR3CALLBACKMEMBER(void, pfnFlushQueue,(PPDMIKEYBOARDCONNECTOR pInterface));

} PDMIKEYBOARDCONNECTOR;
/** PDMIKEYBOARDCONNECTOR interface ID. */
#define PDMIKEYBOARDCONNECTOR_IID               "db3f7bd5-953e-436f-9f8e-077905a92d82"



/** Pointer to a display port interface. */
typedef struct PDMIDISPLAYPORT *PPDMIDISPLAYPORT;
/**
 * Display port interface (down).
 * Pair with PDMIDISPLAYCONNECTOR.
 */
typedef struct PDMIDISPLAYPORT
{
    /**
     * Update the display with any changed regions.
     *
     * Flushes any display changes to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect()
     * while doing so.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateDisplay,(PPDMIDISPLAYPORT pInterface));

    /**
     * Update the entire display.
     *
     * Flushes the entire display content to the memory pointed to by the
     * PDMIDISPLAYCONNECTOR interface and calles PDMIDISPLAYCONNECTOR::pfnUpdateRect().
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   fFailOnResize       Fail is a resize is pending.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateDisplayAll,(PPDMIDISPLAYPORT pInterface, bool fFailOnResize));

    /**
     * Return the current guest resolution and color depth in bits per pixel (bpp).
     *
     * As the graphics card is able to provide display updates with the bpp
     * requested by the host, this method can be used to query the actual
     * guest color depth.
     *
     * @returns VBox status code.
     * @param   pInterface         Pointer to this interface.
     * @param   pcBits             Where to store the current guest color depth.
     * @param   pcx                Where to store the horizontal resolution.
     * @param   pcy                Where to store the vertical resolution.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryVideoMode,(PPDMIDISPLAYPORT pInterface, uint32_t *pcBits, uint32_t *pcx, uint32_t *pcy));

    /**
     * Sets the refresh rate and restart the timer.
     * The rate is defined as the minimum interval between the return of
     * one PDMIDISPLAYPORT::pfnRefresh() call to the next one.
     *
     * The interval timer will be restarted by this call. So at VM startup
     * this function must be called to start the refresh cycle. The refresh
     * rate is not saved, but have to be when resuming a loaded VM state.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   cMilliesInterval    Number of millis between two refreshes.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetRefreshRate,(PPDMIDISPLAYPORT pInterface, uint32_t cMilliesInterval));

    /**
     * Create a 32-bbp screenshot of the display.
     *
     * This will allocate and return a 32-bbp bitmap. Size of the bitmap scanline in bytes is 4*width.
     *
     * The allocated bitmap buffer must be freed with pfnFreeScreenshot.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   ppbData             Where to store the pointer to the allocated
     *                              buffer.
     * @param   pcbData             Where to store the actual size of the bitmap.
     * @param   pcx                 Where to store the width of the bitmap.
     * @param   pcy                 Where to store the height of the bitmap.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnTakeScreenshot,(PPDMIDISPLAYPORT pInterface, uint8_t **ppbData, size_t *pcbData, uint32_t *pcx, uint32_t *pcy));

    /**
     * Free screenshot buffer.
     *
     * This will free the memory buffer allocated by pfnTakeScreenshot.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pbData              Pointer to the buffer returned by
     *                              pfnTakeScreenshot.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(void, pfnFreeScreenshot,(PPDMIDISPLAYPORT pInterface, uint8_t *pbData));

    /**
     * Copy bitmap to the display.
     *
     * This will convert and copy a 32-bbp bitmap (with dword aligned scanline length) to
     * the memory pointed to by the PDMIDISPLAYCONNECTOR interface.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvData              Pointer to the bitmap bits.
     * @param   x                   The upper left corner x coordinate of the destination rectangle.
     * @param   y                   The upper left corner y coordinate of the destination rectangle.
     * @param   cx                  The width of the source and destination rectangles.
     * @param   cy                  The height of the source and destination rectangles.
     * @thread  The emulation thread.
     * @remark  This is just a convenience for using the bitmap conversions of the
     *          graphics device.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisplayBlt,(PPDMIDISPLAYPORT pInterface, const void *pvData, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Render a rectangle from guest VRAM to Framebuffer.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle to be updated.
     * @param   y                   The upper left corner y coordinate of the rectangle to be updated.
     * @param   cx                  The width of the rectangle to be updated.
     * @param   cy                  The height of the rectangle to be updated.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateDisplayRect,(PPDMIDISPLAYPORT pInterface, int32_t x, int32_t y, uint32_t cx, uint32_t cy));

    /**
     * Inform the VGA device whether the Display is directly using the guest VRAM and there is no need
     * to render the VRAM to the framebuffer memory.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fRender             Whether the VRAM content must be rendered to the framebuffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetRenderVRAM,(PPDMIDISPLAYPORT pInterface, bool fRender));

    /**
     * Render a bitmap rectangle from source to target buffer.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   cx                  The width of the rectangle to be copied.
     * @param   cy                  The height of the rectangle to be copied.
     * @param   pbSrc               Source frame buffer 0,0.
     * @param   xSrc                The upper left corner x coordinate of the source rectangle.
     * @param   ySrc                The upper left corner y coordinate of the source rectangle.
     * @param   cxSrc               The width of the source frame buffer.
     * @param   cySrc               The height of the source frame buffer.
     * @param   cbSrcLine           The line length of the source frame buffer.
     * @param   cSrcBitsPerPixel    The pixel depth of the source.
     * @param   pbDst               Destination frame buffer 0,0.
     * @param   xDst                The upper left corner x coordinate of the destination rectangle.
     * @param   yDst                The upper left corner y coordinate of the destination rectangle.
     * @param   cxDst               The width of the destination frame buffer.
     * @param   cyDst               The height of the destination frame buffer.
     * @param   cbDstLine           The line length of the destination frame buffer.
     * @param   cDstBitsPerPixel    The pixel depth of the destination.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCopyRect,(PPDMIDISPLAYPORT pInterface, uint32_t cx, uint32_t cy,
        const uint8_t *pbSrc, int32_t xSrc, int32_t ySrc, uint32_t cxSrc, uint32_t cySrc, uint32_t cbSrcLine, uint32_t cSrcBitsPerPixel,
        uint8_t       *pbDst, int32_t xDst, int32_t yDst, uint32_t cxDst, uint32_t cyDst, uint32_t cbDstLine, uint32_t cDstBitsPerPixel));

    /**
     * Inform the VGA device of viewport changes (as a result of e.g. scrolling).
     *
     * @param   pInterface          Pointer to this interface.
     * @param   idScreen            The screen updates are for.
     * @param   x                   The upper left corner x coordinate of the new viewport rectangle
     * @param   y                   The upper left corner y coordinate of the new viewport rectangle
     * @param   cx                  The width of the new viewport rectangle
     * @param   cy                  The height of the new viewport rectangle
     * @thread  GUI thread?
     *
     * @remarks Is allowed to be NULL.
     */
    DECLR3CALLBACKMEMBER(void, pfnSetViewport,(PPDMIDISPLAYPORT pInterface,
                                               uint32_t idScreen, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Send a video mode hint to the VGA device.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   cx                  The X resolution.
     * @param   cy                  The Y resolution.
     * @param   cBPP                The bit count.
     * @param   iDisplay            The screen number.
     * @param   dx                  X offset into the virtual framebuffer or ~0.
     * @param   dy                  Y offset into the virtual framebuffer or ~0.
     * @param   fEnabled            Is this screen currently enabled?
     * @param   fNotifyGuest        Should the device send the guest an IRQ?
     *                              Set for the last hint of a series.
     * @thread  Schedules on the emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendModeHint, (PPDMIDISPLAYPORT pInterface, uint32_t cx, uint32_t cy,
                                                uint32_t cBPP, uint32_t iDisplay, uint32_t dx,
                                                uint32_t dy, uint32_t fEnabled, uint32_t fNotifyGuest));

    /**
     * Send the guest a notification about host cursor capabilities changes.
     *
     * @param   pInterface            Pointer to this interface.
     * @param   fCapabilitiesAdded    New supported capabilities.
     * @param   fCapabilitiesRemoved  No longer supported capabilities.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(void, pfnReportHostCursorCapabilities, (PPDMIDISPLAYPORT pInterface, uint32_t fCapabilitiesAdded,
                                                                 uint32_t fCapabilitiesRemoved));

    /**
     * Tell the graphics device about the host cursor position.
     *
     * @param   pInterface  Pointer to this interface.
     * @param   x           X offset into the cursor range.
     * @param   y           Y offset into the cursor range.
     * @thread  Any.
     */
    DECLR3CALLBACKMEMBER(void, pfnReportHostCursorPosition, (PPDMIDISPLAYPORT pInterface, uint32_t x, uint32_t y));
} PDMIDISPLAYPORT;
/** PDMIDISPLAYPORT interface ID. */
#ifdef VBOX_WITH_VMSVGA
#define PDMIDISPLAYPORT_IID                     "9672e2b0-1aef-4c4d-9108-864cdb28333f"
#else
#define PDMIDISPLAYPORT_IID                     "323f3412-8903-4564-b04c-cbfe0d2d1596"
#endif


/** Pointer to a 2D graphics acceleration command. */
typedef struct VBOXVHWACMD *PVBOXVHWACMD;
/** Pointer to a VBVA command header. */
typedef struct VBVACMDHDR *PVBVACMDHDR;
/** Pointer to a const VBVA command header. */
typedef const struct VBVACMDHDR *PCVBVACMDHDR;
/** Pointer to a VBVA screen information. */
typedef struct VBVAINFOSCREEN *PVBVAINFOSCREEN;
/** Pointer to a const VBVA screen information. */
typedef const struct VBVAINFOSCREEN *PCVBVAINFOSCREEN;
/** Pointer to a VBVA guest VRAM area information. */
typedef struct VBVAINFOVIEW *PVBVAINFOVIEW;
/** Pointer to a const VBVA guest VRAM area information. */
typedef const struct VBVAINFOVIEW *PCVBVAINFOVIEW;
typedef struct VBVAHOSTFLAGS *PVBVAHOSTFLAGS;
struct VBOXVDMACMD_CHROMIUM_CMD; /* <- chromium [hgsmi] command */
struct VBOXVDMACMD_CHROMIUM_CTL; /* <- chromium [hgsmi] command */


/** Pointer to a display connector interface. */
typedef struct PDMIDISPLAYCONNECTOR *PPDMIDISPLAYCONNECTOR;
struct VBOXCRCMDCTL;
typedef DECLCALLBACKPTR(void, PFNCRCTLCOMPLETION)(struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd, int rc, void *pvCompletion);
/**
 * Display connector interface (up).
 * Pair with PDMIDISPLAYPORT.
 */
typedef struct PDMIDISPLAYCONNECTOR
{
    /**
     * Resize the display.
     * This is called when the resolution changes. This usually happens on
     * request from the guest os, but may also happen as the result of a reset.
     * If the callback returns VINF_VGA_RESIZE_IN_PROGRESS, the caller (VGA device)
     * must not access the connector and return.
     *
     * @returns VINF_SUCCESS if the framebuffer resize was completed,
     *          VINF_VGA_RESIZE_IN_PROGRESS if resize takes time and not yet finished.
     * @param   pInterface          Pointer to this interface.
     * @param   cBits               Color depth (bits per pixel) of the new video mode.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   cbLine              Size in bytes of a single scan line.
     * @param   cx                  New display width.
     * @param   cy                  New display height.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnResize,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t cBits, void *pvVRAM, uint32_t cbLine,
                                         uint32_t cx, uint32_t cy));

    /**
     * Update a rectangle of the display.
     * PDMIDISPLAYPORT::pfnUpdateDisplay is the caller.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   x                   The upper left corner x coordinate of the rectangle.
     * @param   y                   The upper left corner y coordinate of the rectangle.
     * @param   cx                  The width of the rectangle.
     * @param   cy                  The height of the rectangle.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateRect,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t x, uint32_t y, uint32_t cx, uint32_t cy));

    /**
     * Refresh the display.
     *
     * The interval between these calls is set by
     * PDMIDISPLAYPORT::pfnSetRefreshRate(). The driver should call
     * PDMIDISPLAYPORT::pfnUpdateDisplay() if it wishes to refresh the
     * display. PDMIDISPLAYPORT::pfnUpdateDisplay calls pfnUpdateRect with
     * the changed rectangles.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnRefresh,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * Reset the display.
     *
     * Notification message when the graphics card has been reset.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnReset,(PPDMIDISPLAYCONNECTOR pInterface));

    /**
     * LFB video mode enter/exit.
     *
     * Notification message when LinearFrameBuffer video mode is enabled/disabled.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnabled            false - LFB mode was disabled,
     *                              true -  an LFB mode was disabled
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnLFBModeChange,(PPDMIDISPLAYCONNECTOR pInterface, bool fEnabled));

    /**
     * Process the guest graphics adapter information.
     *
     * Direct notification from guest to the display connector.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   u32VRAMSize         Size of the guest VRAM.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnProcessAdapterData,(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, uint32_t u32VRAMSize));

    /**
     * Process the guest display information.
     *
     * Direct notification from guest to the display connector.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   uScreenId           The index of the guest display to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnProcessDisplayData,(PPDMIDISPLAYCONNECTOR pInterface, void *pvVRAM, unsigned uScreenId));

    /**
     * Process the guest Video HW Acceleration command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @returns VINF_SUCCESS - command is completed,
     * VINF_CALLBACK_RETURN - command will by asynchronously completed via complete callback
     * VERR_INVALID_STATE - the command could not be processed (most likely because the framebuffer was disconnected) - the post should be retried later
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVHWACommandProcess,(PPDMIDISPLAYCONNECTOR pInterface, PVBOXVHWACMD pCmd));

    /**
     * Process the guest chromium command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnCrHgsmiCommandProcess,(PPDMIDISPLAYCONNECTOR pInterface, struct VBOXVDMACMD_CHROMIUM_CMD* pCmd, uint32_t cbCmd));

    /**
     * Process the guest chromium control command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnCrHgsmiControlProcess,(PPDMIDISPLAYCONNECTOR pInterface, struct VBOXVDMACMD_CHROMIUM_CTL* pCtl, uint32_t cbCtl));

    /**
     * Process the guest chromium control command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @param   cbCmd               Undocumented!
     * @param   pfnCompletion       Undocumented!
     * @param   pvCompletion        Undocumented!
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCrHgcmCtlSubmit,(PPDMIDISPLAYCONNECTOR pInterface, struct VBOXCRCMDCTL *pCmd, uint32_t cbCmd,
                                                  PFNCRCTLCOMPLETION pfnCompletion, void *pvCompletion));

    /**
     * The specified screen enters VBVA mode.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @param   pHostFlags          Undocumented!
     * @param   fRenderThreadMode   if true - the graphics device has a separate thread that does all rendering.
     *                              This means that:
     *                              1. most pfnVBVAXxx callbacks (see the individual documentation for each one)
     *                                 will be called in the context of the render thread rather than the emulation thread
     *                              2. PDMIDISPLAYCONNECTOR implementor (i.e. DisplayImpl) must NOT notify crogl backend
     *                                 about vbva-originated events (e.g. resize), because crogl is working in CrCmd mode,
     *                                 in the context of the render thread as part of the Graphics device, and gets notified about those events directly
     * @thread  if fRenderThreadMode is TRUE - the render thread, otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAEnable,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId,
                                             PVBVAHOSTFLAGS pHostFlags, bool fRenderThreadMode));

    /**
     * The specified screen leaves VBVA mode.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @thread  if render thread mode is on (fRenderThreadMode that was passed to pfnVBVAEnable is TRUE) - the render thread pfnVBVAEnable was called in,
     *          otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVADisable,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId));

    /**
     * A sequence of pfnVBVAUpdateProcess calls begins.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @thread  if render thread mode is on (fRenderThreadMode that was passed to pfnVBVAEnable is TRUE) - the render thread pfnVBVAEnable was called in,
     *          otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateBegin,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId));

    /**
     * Process the guest VBVA command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @param   pCmd                Video HW Acceleration Command to be processed.
     * @param   cbCmd               Undocumented!
     * @thread  if render thread mode is on (fRenderThreadMode that was passed to pfnVBVAEnable is TRUE) - the render thread pfnVBVAEnable was called in,
     *          otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateProcess,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId,
                                                     PCVBVACMDHDR pCmd, size_t cbCmd));

    /**
     * A sequence of pfnVBVAUpdateProcess calls ends.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uScreenId           The screen updates are for.
     * @param   x                   The upper left corner x coordinate of the combined rectangle of all VBVA updates.
     * @param   y                   The upper left corner y coordinate of the rectangle.
     * @param   cx                  The width of the rectangle.
     * @param   cy                  The height of the rectangle.
     * @thread  if render thread mode is on (fRenderThreadMode that was passed to pfnVBVAEnable is TRUE) - the render thread pfnVBVAEnable was called in,
     *          otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAUpdateEnd,(PPDMIDISPLAYCONNECTOR pInterface, unsigned uScreenId, int32_t x, int32_t y, uint32_t cx, uint32_t cy));

    /**
     * Resize the display.
     * This is called when the resolution changes. This usually happens on
     * request from the guest os, but may also happen as the result of a reset.
     * If the callback returns VINF_VGA_RESIZE_IN_PROGRESS, the caller (VGA device)
     * must not access the connector and return.
     *
     * @todo Merge with pfnResize.
     *
     * @returns VINF_SUCCESS if the framebuffer resize was completed,
     *          VINF_VGA_RESIZE_IN_PROGRESS if resize takes time and not yet finished.
     * @param   pInterface          Pointer to this interface.
     * @param   pView               The description of VRAM block for this screen.
     * @param   pScreen             The data of screen being resized.
     * @param   pvVRAM              Address of the guest VRAM.
     * @param   fResetInputMapping  Whether to reset the absolute pointing device to screen position co-ordinate
     *                              mapping.  Needed for real resizes, as the caller on the guest may not know how
     *                              to set the mapping.  Not wanted when we restore a saved state and are resetting
     *                              the mode.
     * @thread  if render thread mode is on (fRenderThreadMode that was passed to pfnVBVAEnable is TRUE) - the render thread pfnVBVAEnable was called in,
     *          otherwise - the emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAResize,(PPDMIDISPLAYCONNECTOR pInterface, PCVBVAINFOVIEW pView, PCVBVAINFOSCREEN pScreen, void *pvVRAM, bool fResetInputMapping));

    /**
     * Update the pointer shape.
     * This is called when the mouse pointer shape changes. The new shape
     * is passed as a caller allocated buffer that will be freed after returning
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fVisible            Visibility indicator (if false, the other parameters are undefined).
     * @param   fAlpha              Flag whether alpha channel is being passed.
     * @param   xHot                Pointer hot spot x coordinate.
     * @param   yHot                Pointer hot spot y coordinate.
     * @param   x                   Pointer new x coordinate on screen.
     * @param   y                   Pointer new y coordinate on screen.
     * @param   cx                  Pointer width in pixels.
     * @param   cy                  Pointer height in pixels.
     * @param   cbScanline          Size of one scanline in bytes.
     * @param   pvShape             New shape buffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVBVAMousePointerShape,(PPDMIDISPLAYCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                        uint32_t xHot, uint32_t yHot, uint32_t cx, uint32_t cy,
                                                        const void *pvShape));

    /**
     * The guest capabilities were updated.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fCapabilities       The new capability flag state.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAGuestCapabilityUpdate,(PPDMIDISPLAYCONNECTOR pInterface, uint32_t fCapabilities));

    /** Read-only attributes.
     * For preformance reasons some readonly attributes are kept in the interface.
     * We trust the interface users to respect the readonlyness of these.
     * @{
     */
    /** Pointer to the display data buffer. */
    uint8_t        *pbData;
    /** Size of a scanline in the data buffer. */
    uint32_t        cbScanline;
    /** The color depth (in bits) the graphics card is supposed to provide. */
    uint32_t        cBits;
    /** The display width. */
    uint32_t        cx;
    /** The display height. */
    uint32_t        cy;
    /** @} */

    /**
     * The guest display input mapping rectangle was updated.
     *
     * @param   pInterface  Pointer to this interface.
     * @param   xOrigin     Upper left X co-ordinate relative to the first screen.
     * @param   yOrigin     Upper left Y co-ordinate relative to the first screen.
     * @param   cx          Rectangle width.
     * @param   cy          Rectangle height.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAInputMappingUpdate,(PPDMIDISPLAYCONNECTOR pInterface, int32_t xOrigin, int32_t yOrigin, uint32_t cx, uint32_t cy));
} PDMIDISPLAYCONNECTOR;
/** PDMIDISPLAYCONNECTOR interface ID. */
#define PDMIDISPLAYCONNECTOR_IID                "e883a720-85fb-11e4-a307-0b06689c9661"


/** Pointer to a secret key interface. */
typedef struct PDMISECKEY *PPDMISECKEY;

/**
 * Secret key interface to retrieve secret keys.
 */
typedef struct PDMISECKEY
{
    /**
     * Retains a key identified by the ID. The caller will only hold a reference
     * to the key and must not modify the key buffer in any way.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pszId           The alias/id for the key to retrieve.
     * @param   ppbKey          Where to store the pointer to the key buffer on success.
     * @param   pcbKey          Where to store the size of the key in bytes on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnKeyRetain, (PPDMISECKEY pInterface, const char *pszId,
                                             const uint8_t **pbKey, size_t *pcbKey));

    /**
     * Releases one reference of the key identified by the given identifier.
     * The caller must not access the key buffer after calling this operation.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pszId          The alias/id for the key to release.
     *
     * @note: It is advised to release the key whenever it is not used anymore so the entity
     *        storing the key can do anything to make retrieving the key from memory more
     *        difficult like scrambling the memory buffer for instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnKeyRelease, (PPDMISECKEY pInterface, const char *pszId));

    /**
     * Retains a password identified by the ID. The caller will only hold a reference
     * to the password and must not modify the buffer in any way.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pszId           The alias/id for the password to retrieve.
     * @param   ppszPassword    Where to store the pointer to the password on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnPasswordRetain, (PPDMISECKEY pInterface, const char *pszId,
                                                  const char **ppszPassword));

    /**
     * Releases one reference of the password identified by the given identifier.
     * The caller must not access the password after calling this operation.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pszId           The alias/id for the password to release.
     *
     * @note: It is advised to release the password whenever it is not used anymore so the entity
     *        storing the password can do anything to make retrieving the password from memory more
     *        difficult like scrambling the memory buffer for instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnPasswordRelease, (PPDMISECKEY pInterface, const char *pszId));
} PDMISECKEY;
/** PDMISECKEY interface ID. */
#define PDMISECKEY_IID                           "3d698355-d995-453d-960f-31566a891df2"

/** Pointer to a secret key helper interface. */
typedef struct PDMISECKEYHLP *PPDMISECKEYHLP;

/**
 * Secret key helper interface for non critical functionality.
 */
typedef struct PDMISECKEYHLP
{
    /**
     * Notifies the interface provider that a key couldn't be retrieved from the key store.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     */
    DECLR3CALLBACKMEMBER(int, pfnKeyMissingNotify, (PPDMISECKEYHLP pInterface));

} PDMISECKEYHLP;
/** PDMISECKEY interface ID. */
#define PDMISECKEYHLP_IID                        "7be96168-4156-40ac-86d2-3073bf8b318e"


/** Pointer to a char port interface. */
typedef struct PDMICHARPORT *PPDMICHARPORT;
/**
 * Char port interface (down).
 * Pair with PDMICHARCONNECTOR.
 */
typedef struct PDMICHARPORT
{
    /**
     * Deliver data read to the device/driver.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where the read bits are stored.
     * @param   pcbRead         Number of bytes available for reading/having been read.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyRead,(PPDMICHARPORT pInterface, const void *pvBuf, size_t *pcbRead));

    /**
     * Notify the device/driver when the status lines changed.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fNewStatusLine  New state of the status line pins.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyStatusLinesChanged,(PPDMICHARPORT pInterface, uint32_t fNewStatusLines));

    /**
     * Notify the device when the driver buffer is full.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fFull           Buffer full.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyBufferFull,(PPDMICHARPORT pInterface, bool fFull));

    /**
     * Notify the device/driver that a break occurred.
     *
     * @returns VBox statsus code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyBreak,(PPDMICHARPORT pInterface));
} PDMICHARPORT;
/** PDMICHARPORT interface ID. */
#define PDMICHARPORT_IID                        "22769834-ea8b-4a6d-ade1-213dcdbd1228"

/** @name Bit mask definitions for status line type.
 * @{ */
#define PDMICHARPORT_STATUS_LINES_DCD   RT_BIT(0)
#define PDMICHARPORT_STATUS_LINES_RI    RT_BIT(1)
#define PDMICHARPORT_STATUS_LINES_DSR   RT_BIT(2)
#define PDMICHARPORT_STATUS_LINES_CTS   RT_BIT(3)
/** @} */


/** Pointer to a char interface. */
typedef struct PDMICHARCONNECTOR *PPDMICHARCONNECTOR;
/**
 * Char connector interface (up).
 * Pair with PDMICHARPORT.
 */
typedef struct PDMICHARCONNECTOR
{
    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMICHARCONNECTOR pInterface, const void *pvBuf, size_t cbWrite));

    /**
     * Set device parameters.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   Bps             Speed of the serial connection. (bits per second)
     * @param   chParity        Parity method: 'E' - even, 'O' - odd, 'N' - none.
     * @param   cDataBits       Number of data bits.
     * @param   cStopBits       Number of stop bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetParameters,(PPDMICHARCONNECTOR pInterface, unsigned Bps, char chParity,
                                                unsigned cDataBits, unsigned cStopBits));

    /**
     * Set the state of the modem lines.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   fRequestToSend      Set to true to make the Request to Send line active otherwise to 0.
     * @param   fDataTerminalReady  Set to true to make the Data Terminal Ready line active otherwise 0.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetModemLines,(PPDMICHARCONNECTOR pInterface, bool fRequestToSend, bool fDataTerminalReady));

    /**
     * Sets the TD line into break condition.
     *
     * @returns VBox status code.
     * @param   pInterface  Pointer to the interface structure containing the called function pointer.
     * @param   fBreak      Set to true to let the device send a break false to put into normal operation.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetBreak,(PPDMICHARCONNECTOR pInterface, bool fBreak));
} PDMICHARCONNECTOR;
/** PDMICHARCONNECTOR interface ID. */
#define PDMICHARCONNECTOR_IID                   "4ad5c190-b408-4cef-926f-fbffce0dc5cc"


/** Pointer to a stream interface. */
typedef struct PDMISTREAM *PPDMISTREAM;
/**
 * Stream interface (up).
 * Makes up the foundation for PDMICHARCONNECTOR.  No pair interface.
 */
typedef struct PDMISTREAM
{
    /**
     * Polls for the specified events.
     *
     * @returns VBox status code.
     * @retval  VERR_INTERRUPTED if the poll was interrupted.
     * @retval  VERR_TIMEOUT     if the maximum waiting time was reached.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fEvts           The events to poll for, see RTPOLL_EVT_XXX.
     * @param   *pfEvts         Where to return details about the events that occurred.
     * @param   cMillies        Number of milliseconds to wait.  Use
     *                          RT_INDEFINITE_WAIT to wait for ever.
     */
    DECLR3CALLBACKMEMBER(int, pfnPoll,(PPDMISTREAM pInterface, uint32_t fEvts, uint32_t *pfEvts, RTMSINTERVAL cMillies));

    /**
     * Interrupts the current poll call.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnPollInterrupt,(PPDMISTREAM pInterface));

    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   pcbRead         Number of bytes to read/bytes actually read.
     * @thread  Any thread.
     *
     * @note: This is non blocking, use the poll callback to block when there is nothing to read.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMISTREAM pInterface, void *pvBuf, size_t *pcbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   pcbWrite        Number of bytes to write/bytes actually written.
     * @thread  Any thread.
     *
     * @note: This is non blocking, use the poll callback to block until there is room to write.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMISTREAM pInterface, const void *pvBuf, size_t *pcbWrite));
} PDMISTREAM;
/** PDMISTREAM interface ID. */
#define PDMISTREAM_IID                          "f9bd1ba6-c134-44cc-8259-febe14393952"


/** Mode of the parallel port */
typedef enum PDMPARALLELPORTMODE
{
    /** First invalid mode. */
    PDM_PARALLEL_PORT_MODE_INVALID = 0,
    /** SPP (Compatibility mode). */
    PDM_PARALLEL_PORT_MODE_SPP,
    /** EPP Data mode. */
    PDM_PARALLEL_PORT_MODE_EPP_DATA,
    /** EPP Address mode. */
    PDM_PARALLEL_PORT_MODE_EPP_ADDR,
    /** ECP mode (not implemented yet). */
    PDM_PARALLEL_PORT_MODE_ECP,
    /** 32bit hack. */
    PDM_PARALLEL_PORT_MODE_32BIT_HACK = 0x7fffffff
} PDMPARALLELPORTMODE;

/** Pointer to a host parallel port interface. */
typedef struct PDMIHOSTPARALLELPORT *PPDMIHOSTPARALLELPORT;
/**
 * Host parallel port interface (down).
 * Pair with PDMIHOSTPARALLELCONNECTOR.
 */
typedef struct PDMIHOSTPARALLELPORT
{
    /**
     * Notify device/driver that an interrupt has occurred.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnNotifyInterrupt,(PPDMIHOSTPARALLELPORT pInterface));
} PDMIHOSTPARALLELPORT;
/** PDMIHOSTPARALLELPORT interface ID. */
#define PDMIHOSTPARALLELPORT_IID                "f24b8668-e7f6-4eaa-a14c-4aa2a5f7048e"



/** Pointer to a Host Parallel connector interface. */
typedef struct PDMIHOSTPARALLELCONNECTOR *PPDMIHOSTPARALLELCONNECTOR;
/**
 * Host parallel connector interface (up).
 * Pair with PDMIHOSTPARALLELPORT.
 */
typedef struct PDMIHOSTPARALLELCONNECTOR
{
    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write.
     * @param   enmMode         Mode to write the data.
     * @thread  Any thread.
     * @todo r=klaus cbWrite only defines buffer length, method needs a way top return actually written amount of data.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIHOSTPARALLELCONNECTOR pInterface, const void *pvBuf,
                                        size_t cbWrite, PDMPARALLELPORTMODE enmMode));

    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   enmMode         Mode to read the data.
     * @thread  Any thread.
     * @todo r=klaus cbRead only defines buffer length, method needs a way top return actually read amount of data.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIHOSTPARALLELCONNECTOR pInterface, void *pvBuf,
                                       size_t cbRead, PDMPARALLELPORTMODE enmMode));

    /**
     * Set data direction of the port (forward/reverse).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fForward        Flag whether to indicate whether the port is operated in forward or reverse mode.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetPortDirection,(PPDMIHOSTPARALLELCONNECTOR pInterface, bool fForward));

    /**
     * Write control register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fReg            The new control register value.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t fReg));

    /**
     * Read control register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfReg           Where to store the control register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadControl,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg));

    /**
     * Read status register bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfReg           Where to store the status register bits.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadStatus,(PPDMIHOSTPARALLELCONNECTOR pInterface, uint8_t *pfReg));

} PDMIHOSTPARALLELCONNECTOR;
/** PDMIHOSTPARALLELCONNECTOR interface ID. */
#define PDMIHOSTPARALLELCONNECTOR_IID           "7c532602-7438-4fbc-9265-349d9f0415f9"


/** ACPI power source identifier */
typedef enum PDMACPIPOWERSOURCE
{
    PDM_ACPI_POWER_SOURCE_UNKNOWN  =   0,
    PDM_ACPI_POWER_SOURCE_OUTLET,
    PDM_ACPI_POWER_SOURCE_BATTERY
} PDMACPIPOWERSOURCE;
/** Pointer to ACPI battery state. */
typedef PDMACPIPOWERSOURCE *PPDMACPIPOWERSOURCE;

/** ACPI battey capacity */
typedef enum PDMACPIBATCAPACITY
{
    PDM_ACPI_BAT_CAPACITY_MIN      =   0,
    PDM_ACPI_BAT_CAPACITY_MAX      = 100,
    PDM_ACPI_BAT_CAPACITY_UNKNOWN  = 255
} PDMACPIBATCAPACITY;
/** Pointer to ACPI battery capacity. */
typedef PDMACPIBATCAPACITY *PPDMACPIBATCAPACITY;

/** ACPI battery state. See ACPI 3.0 spec '_BST (Battery Status)' */
typedef enum PDMACPIBATSTATE
{
    PDM_ACPI_BAT_STATE_CHARGED     = 0x00,
    PDM_ACPI_BAT_STATE_DISCHARGING = 0x01,
    PDM_ACPI_BAT_STATE_CHARGING    = 0x02,
    PDM_ACPI_BAT_STATE_CRITICAL    = 0x04
} PDMACPIBATSTATE;
/** Pointer to ACPI battery state. */
typedef PDMACPIBATSTATE *PPDMACPIBATSTATE;

/** Pointer to an ACPI port interface. */
typedef struct PDMIACPIPORT *PPDMIACPIPORT;
/**
 * ACPI port interface (down). Used by both the ACPI driver and (grumble) main.
 * Pair with PDMIACPICONNECTOR.
 */
typedef struct PDMIACPIPORT
{
    /**
     * Send an ACPI power off event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnPowerButtonPress,(PPDMIACPIPORT pInterface));

    /**
     * Send an ACPI sleep button event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnSleepButtonPress,(PPDMIACPIPORT pInterface));

    /**
     * Check if the last power button event was handled by the guest.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfHandled       Is set to true if the last power button event was handled, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetPowerButtonHandled,(PPDMIACPIPORT pInterface, bool *pfHandled));

    /**
     * Check if the guest entered the ACPI mode.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfEntered       Is set to true if the guest entered the ACPI mode, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetGuestEnteredACPIMode,(PPDMIACPIPORT pInterface, bool *pfEntered));

    /**
     * Check if the given CPU is still locked by the guest.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   uCpu            The CPU to check for.
     * @param   pfLocked        Is set to true if the CPU is still locked by the guest, false otherwise.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetCpuStatus,(PPDMIACPIPORT pInterface, unsigned uCpu, bool *pfLocked));

    /**
     * Send an ACPI monitor hot-plug event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing
     *                          the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnMonitorHotPlugEvent,(PPDMIACPIPORT pInterface));

    /**
     * Send a battery status change event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing
     *                          the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnBatteryStatusChangeEvent,(PPDMIACPIPORT pInterface));
} PDMIACPIPORT;
/** PDMIACPIPORT interface ID. */
#define PDMIACPIPORT_IID                        "974cb8fb-7fda-408c-f9b4-7ff4e3b2a699"


/** Pointer to an ACPI connector interface. */
typedef struct PDMIACPICONNECTOR *PPDMIACPICONNECTOR;
/**
 * ACPI connector interface (up).
 * Pair with PDMIACPIPORT.
 */
typedef struct PDMIACPICONNECTOR
{
    /**
     * Get the current power source of the host system.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   penmPowerSource Pointer to the power source result variable.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryPowerSource,(PPDMIACPICONNECTOR, PPDMACPIPOWERSOURCE penmPowerSource));

    /**
     * Query the current battery status of the host system.
     *
     * @returns VBox status code?
     * @param   pInterface              Pointer to the interface structure containing the called function pointer.
     * @param   pfPresent               Is set to true if battery is present, false otherwise.
     * @param   penmRemainingCapacity   Pointer to the battery remaining capacity (0 - 100 or 255 for unknown).
     * @param   penmBatteryState        Pointer to the battery status.
     * @param   pu32PresentRate         Pointer to the present rate (0..1000 of the total capacity).
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryBatteryStatus,(PPDMIACPICONNECTOR, bool *pfPresent, PPDMACPIBATCAPACITY penmRemainingCapacity,
                                                     PPDMACPIBATSTATE penmBatteryState, uint32_t *pu32PresentRate));
} PDMIACPICONNECTOR;
/** PDMIACPICONNECTOR interface ID. */
#define PDMIACPICONNECTOR_IID                   "5f14bf8d-1edf-4e3a-a1e1-cca9fd08e359"


/** Pointer to a VMMDevice port interface. */
typedef struct PDMIVMMDEVPORT *PPDMIVMMDEVPORT;
/**
 * VMMDevice port interface (down).
 * Pair with PDMIVMMDEVCONNECTOR.
 */
typedef struct PDMIVMMDEVPORT
{
    /**
     * Return the current absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pxAbs           Pointer of result value, can be NULL
     * @param   pyAbs           Pointer of result value, can be NULL
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, int32_t *pxAbs, int32_t *pyAbs));

    /**
     * Set the new absolute mouse position in pixels
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   xAbs            New absolute X position
     * @param   yAbs            New absolute Y position
     */
    DECLR3CALLBACKMEMBER(int, pfnSetAbsoluteMouse,(PPDMIVMMDEVPORT pInterface, int32_t xAbs, int32_t yAbs));

    /**
     * Return the current mouse capability flags
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfCapabilities  Pointer of result value
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t *pfCapabilities));

    /**
     * Set the current mouse capability flag (host side)
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fCapsAdded      Mask of capabilities to add to the flag
     * @param   fCapsRemoved    Mask of capabilities to remove from the flag
     */
    DECLR3CALLBACKMEMBER(int, pfnUpdateMouseCapabilities,(PPDMIVMMDEVPORT pInterface, uint32_t fCapsAdded, uint32_t fCapsRemoved));

    /**
     * Issue a display resolution change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cx              Horizontal pixel resolution (0 = do not change).
     * @param   cy              Vertical pixel resolution (0 = do not change).
     * @param   cBits           Bits per pixel (0 = do not change).
     * @param   idxDisplay      The display index.
     * @param   xOrigin         The X coordinate of the lower left
     *                          corner of the secondary display with
     *                          ID = idxDisplay
     * @param   yOrigin         The Y coordinate of the lower left
     *                          corner of the secondary display with
     *                          ID = idxDisplay
     * @param   fEnabled        Whether the display is enabled or not. (Guessing
     *                          again.)
     * @param   fChangeOrigin   Whether the display origin point changed. (Guess)
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestDisplayChange,(PPDMIVMMDEVPORT pInterface, uint32_t cx,
                         uint32_t cy, uint32_t cBits, uint32_t idxDisplay,
                         int32_t xOrigin, int32_t yOrigin, bool fEnabled, bool fChangeOrigin));

    /**
     * Pass credentials to guest.
     *
     * Note that there can only be one set of credentials and the guest may or may not
     * query them and may do whatever it wants with them.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pszUsername     User name, may be empty (UTF-8).
     * @param   pszPassword     Password, may be empty (UTF-8).
     * @param   pszDomain       Domain name, may be empty (UTF-8).
     * @param   fFlags          VMMDEV_SETCREDENTIALS_*.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetCredentials,(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                                 const char *pszPassword, const char *pszDomain,
                                                 uint32_t fFlags));

    /**
     * Notify the driver about a VBVA status change.
     *
     * @returns Nothing. Because it is informational callback.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fEnabled        Current VBVA status.
     */
    DECLR3CALLBACKMEMBER(void, pfnVBVAChange, (PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a seamless mode change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   fEnabled        Seamless mode enabled or not
     */
    DECLR3CALLBACKMEMBER(int, pfnRequestSeamlessChange,(PPDMIVMMDEVPORT pInterface, bool fEnabled));

    /**
     * Issue a memory balloon change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cMbBalloon      Balloon size in megabytes
     */
    DECLR3CALLBACKMEMBER(int, pfnSetMemoryBalloon,(PPDMIVMMDEVPORT pInterface, uint32_t cMbBalloon));

    /**
     * Issue a statistcs interval change request.
     *
     * Note that there can only one request in the queue and that in case the guest does
     * not process it, issuing another request will overwrite the previous.
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   cSecsStatInterval   Statistics query interval in seconds
     *                              (0=disable).
     */
    DECLR3CALLBACKMEMBER(int, pfnSetStatisticsInterval,(PPDMIVMMDEVPORT pInterface, uint32_t cSecsStatInterval));

    /**
     * Notify the guest about a VRDP status change.
     *
     * @returns VBox status code
     * @param   pInterface              Pointer to the interface structure containing the called function pointer.
     * @param   fVRDPEnabled            Current VRDP status.
     * @param   uVRDPExperienceLevel    Which visual effects to be disabled in
     *                                  the guest.
     */
    DECLR3CALLBACKMEMBER(int, pfnVRDPChange, (PPDMIVMMDEVPORT pInterface, bool fVRDPEnabled, uint32_t uVRDPExperienceLevel));

    /**
     * Notify the guest of CPU hot-unplug event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   idCpuCore       The core id of the CPU to remove.
     * @param   idCpuPackage    The package id of the CPU to remove.
     */
    DECLR3CALLBACKMEMBER(int, pfnCpuHotUnplug, (PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage));

    /**
     * Notify the guest of CPU hot-plug event.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   idCpuCore       The core id of the CPU to add.
     * @param   idCpuPackage    The package id of the CPU to add.
     */
    DECLR3CALLBACKMEMBER(int, pfnCpuHotPlug, (PPDMIVMMDEVPORT pInterface, uint32_t idCpuCore, uint32_t idCpuPackage));

} PDMIVMMDEVPORT;
/** PDMIVMMDEVPORT interface ID. */
#define PDMIVMMDEVPORT_IID                      "d7e52035-3b6c-422e-9215-2a75646a945d"


/** Pointer to a HPET legacy notification interface. */
typedef struct PDMIHPETLEGACYNOTIFY *PPDMIHPETLEGACYNOTIFY;
/**
 * HPET legacy notification interface.
 */
typedef struct PDMIHPETLEGACYNOTIFY
{
    /**
     * Notify about change of HPET legacy mode.
     *
     * @param   pInterface      Pointer to the interface structure containing the
     *                          called function pointer.
     * @param   fActivated      If HPET legacy mode is activated (@c true) or
     *                          deactivated (@c false).
     */
    DECLR3CALLBACKMEMBER(void, pfnModeChanged,(PPDMIHPETLEGACYNOTIFY pInterface, bool fActivated));
} PDMIHPETLEGACYNOTIFY;
/** PDMIHPETLEGACYNOTIFY interface ID. */
#define PDMIHPETLEGACYNOTIFY_IID                "c9ada595-4b65-4311-8b21-b10498997774"


/** @name Flags for PDMIVMMDEVPORT::pfnSetCredentials.
 * @{ */
/** The guest should perform a logon with the credentials. */
#define VMMDEV_SETCREDENTIALS_GUESTLOGON                    RT_BIT(0)
/** The guest should prevent local logons. */
#define VMMDEV_SETCREDENTIALS_NOLOCALLOGON                  RT_BIT(1)
/** The guest should verify the credentials. */
#define VMMDEV_SETCREDENTIALS_JUDGE                         RT_BIT(15)
/** @} */

/** Forward declaration of the guest information structure. */
struct VBoxGuestInfo;
/** Forward declaration of the guest information-2 structure. */
struct VBoxGuestInfo2;
/** Forward declaration of the guest statistics structure */
struct VBoxGuestStatistics;
/** Forward declaration of the guest status structure */
struct VBoxGuestStatus;

/** Forward declaration of the video accelerator command memory. */
struct VBVAMEMORY;
/** Pointer to video accelerator command memory. */
typedef struct VBVAMEMORY *PVBVAMEMORY;

/** Pointer to a VMMDev connector interface. */
typedef struct PDMIVMMDEVCONNECTOR *PPDMIVMMDEVCONNECTOR;
/**
 * VMMDev connector interface (up).
 * Pair with PDMIVMMDEVPORT.
 */
typedef struct PDMIVMMDEVCONNECTOR
{
    /**
     * Update guest facility status.
     *
     * Called in response to VMMDevReq_ReportGuestStatus, reset or state restore.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uFacility           The facility.
     * @param   uStatus             The status.
     * @param   fFlags              Flags assoicated with the update. Currently
     *                              reserved and should be ignored.
     * @param   pTimeSpecTS         Pointer to the timestamp of this report.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestStatus,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t uFacility, uint16_t uStatus,
                                                     uint32_t fFlags, PCRTTIMESPEC pTimeSpecTS));

    /**
     * Updates a guest user state.
     *
     * Called in response to VMMDevReq_ReportGuestUserState.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pszUser             Guest user name to update status for.
     * @param   pszDomain           Domain the guest user is bound to. Optional.
     * @param   uState              New guest user state to notify host about.
     * @param   pabDetails          Pointer to optional state data.
     * @param   cbDetails           Size (in bytes) of optional state data.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestUserState,(PPDMIVMMDEVCONNECTOR pInterface, const char *pszUser,
                                                        const char *pszDomain, uint32_t uState,
                                                        const uint8_t *pabDetails, uint32_t cbDetails));

    /**
     * Reports the guest API and OS version.
     * Called whenever the Additions issue a guest info report request.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestInfo          Pointer to guest information structure
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestInfo,(PPDMIVMMDEVCONNECTOR pInterface, const struct VBoxGuestInfo *pGuestInfo));

    /**
     * Reports the detailed Guest Additions version.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   uFullVersion        The guest additions version as a full version.
     *                              Use VBOX_FULL_VERSION_GET_MAJOR,
     *                              VBOX_FULL_VERSION_GET_MINOR and
     *                              VBOX_FULL_VERSION_GET_BUILD to access it.
     *                              (This will not be zero, so turn down the
     *                              paranoia level a notch.)
     * @param   pszName             Pointer to the sanitized version name.  This can
     *                              be empty, but will not be NULL.  If not empty,
     *                              it will contain a build type tag and/or a
     *                              publisher tag.  If both, then they are separated
     *                              by an underscore (VBOX_VERSION_STRING fashion).
     * @param   uRevision           The SVN revision.  Can be 0.
     * @param   fFeatures           Feature mask, currently none are defined.
     *
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestInfo2,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t uFullVersion,
                                                    const char *pszName, uint32_t uRevision, uint32_t fFeatures));

    /**
     * Update the guest additions capabilities.
     * This is called when the guest additions capabilities change. The new capabilities
     * are given and the connector should update its internal state.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   newCapabilities     New capabilities.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateGuestCapabilities,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities));

    /**
     * Update the mouse capabilities.
     * This is called when the mouse capabilities change. The new capabilities
     * are given and the connector should update its internal state.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   newCapabilities     New capabilities.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdateMouseCapabilities,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t newCapabilities));

    /**
     * Update the pointer shape.
     * This is called when the mouse pointer shape changes. The new shape
     * is passed as a caller allocated buffer that will be freed after returning
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fVisible            Visibility indicator (if false, the other parameters are undefined).
     * @param   fAlpha              Flag whether alpha channel is being passed.
     * @param   xHot                Pointer hot spot x coordinate.
     * @param   yHot                Pointer hot spot y coordinate.
     * @param   x                   Pointer new x coordinate on screen.
     * @param   y                   Pointer new y coordinate on screen.
     * @param   cx                  Pointer width in pixels.
     * @param   cy                  Pointer height in pixels.
     * @param   cbScanline          Size of one scanline in bytes.
     * @param   pvShape             New shape buffer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUpdatePointerShape,(PPDMIVMMDEVCONNECTOR pInterface, bool fVisible, bool fAlpha,
                                                      uint32_t xHot, uint32_t yHot,
                                                      uint32_t cx, uint32_t cy,
                                                      void *pvShape));

    /**
     * Enable or disable video acceleration on behalf of guest.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   fEnable             Whether to enable acceleration.
     * @param   pVbvaMemory         Video accelerator memory.

     * @return  VBox rc. VINF_SUCCESS if VBVA was enabled.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoAccelEnable,(PPDMIVMMDEVCONNECTOR pInterface, bool fEnable, PVBVAMEMORY pVbvaMemory));

    /**
     * Force video queue processing.
     *
     * @param   pInterface          Pointer to this interface.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnVideoAccelFlush,(PPDMIVMMDEVCONNECTOR pInterface));

    /**
     * Return whether the given video mode is supported/wanted by the host.
     *
     * @returns VBox status code
     * @param   pInterface      Pointer to this interface.
     * @param   display         The guest monitor, 0 for primary.
     * @param   cy              Video mode horizontal resolution in pixels.
     * @param   cx              Video mode vertical resolution in pixels.
     * @param   cBits           Video mode bits per pixel.
     * @param   pfSupported     Where to put the indicator for whether this mode is supported. (output)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnVideoModeSupported,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t display, uint32_t cx, uint32_t cy, uint32_t cBits, bool *pfSupported));

    /**
     * Queries by how many pixels the height should be reduced when calculating video modes
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   pcyReduction        Pointer to the result value.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetHeightReduction,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcyReduction));

    /**
     * Informs about a credentials judgement result from the guest.
     *
     * @returns VBox status code
     * @param   pInterface          Pointer to this interface.
     * @param   fFlags              Judgement result flags.
     * @thread  The emulation thread.
     */
     DECLR3CALLBACKMEMBER(int, pfnSetCredentialsJudgementResult,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t fFlags));

    /**
     * Set the visible region of the display
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   cRect               Number of rectangles in pRect
     * @param   pRect               Rectangle array
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetVisibleRegion,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t cRect, PRTRECT pRect));

    /**
     * Query the visible region of the display
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pcRects             Where to return the number of rectangles in
     *                              paRects.
     * @param   paRects             Rectangle array (set to NULL to query the number
     *                              of rectangles)
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryVisibleRegion,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcRects, PRTRECT paRects));

    /**
     * Request the statistics interval
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pulInterval         Pointer to interval in seconds
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStatisticsInterval,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pulInterval));

    /**
     * Report new guest statistics
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pGuestStats         Guest statistics
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnReportStatistics,(PPDMIVMMDEVCONNECTOR pInterface, struct VBoxGuestStatistics *pGuestStats));

    /**
     * Query the current balloon size
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pcbBalloon          Balloon size
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryBalloonSize,(PPDMIVMMDEVCONNECTOR pInterface, uint32_t *pcbBalloon));

    /**
     * Query the current page fusion setting
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to this interface.
     * @param   pfPageFusionEnabled Pointer to boolean
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIsPageFusionEnabled,(PPDMIVMMDEVCONNECTOR pInterface, bool *pfPageFusionEnabled));

} PDMIVMMDEVCONNECTOR;
/** PDMIVMMDEVCONNECTOR interface ID. */
#define PDMIVMMDEVCONNECTOR_IID                 "aff90240-a443-434e-9132-80c186ab97d4"


/**
 * Generic status LED core.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef union PDMLEDCORE
{
    /** 32-bit view. */
    uint32_t volatile u32;
    /** Bit view. */
    struct
    {
        /** Reading/Receiving indicator. */
        uint32_t    fReading : 1;
        /** Writing/Sending indicator. */
        uint32_t    fWriting : 1;
        /** Busy indicator. */
        uint32_t    fBusy : 1;
        /** Error indicator. */
        uint32_t    fError : 1;
    }           s;
} PDMLEDCORE;

/** LED bit masks for the u32 view.
 * @{ */
/** Reading/Receiving indicator. */
#define PDMLED_READING  RT_BIT(0)
/** Writing/Sending indicator. */
#define PDMLED_WRITING  RT_BIT(1)
/** Busy indicator. */
#define PDMLED_BUSY     RT_BIT(2)
/** Error indicator. */
#define PDMLED_ERROR    RT_BIT(3)
/** @} */


/**
 * Generic status LED.
 * Note that a unit doesn't have to support all the indicators.
 */
typedef struct PDMLED
{
    /** Just a magic for sanity checking. */
    uint32_t    u32Magic;
    uint32_t    u32Alignment;           /**< structure size alignment. */
    /** The actual LED status.
     * Only the device is allowed to change this. */
    PDMLEDCORE  Actual;
    /** The asserted LED status which is cleared by the reader.
     * The device will assert the bits but never clear them.
     * The driver clears them as it sees fit. */
    PDMLEDCORE  Asserted;
} PDMLED;

/** Pointer to an LED. */
typedef PDMLED *PPDMLED;
/** Pointer to a const LED. */
typedef const PDMLED *PCPDMLED;

/** Magic value for PDMLED::u32Magic. */
#define PDMLED_MAGIC    UINT32_C(0x11335577)

/** Pointer to an LED ports interface. */
typedef struct PDMILEDPORTS      *PPDMILEDPORTS;
/**
 * Interface for exporting LEDs (down).
 * Pair with PDMILEDCONNECTORS.
 */
typedef struct PDMILEDPORTS
{
    /**
     * Gets the pointer to the status LED of a unit.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit which status LED we desire.
     * @param   ppLed           Where to store the LED pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryStatusLed,(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed));

} PDMILEDPORTS;
/** PDMILEDPORTS interface ID. */
#define PDMILEDPORTS_IID                        "435e0cec-8549-4ca0-8c0d-98e52f1dc038"


/** Pointer to an LED connectors interface. */
typedef struct PDMILEDCONNECTORS *PPDMILEDCONNECTORS;
/**
 * Interface for reading LEDs (up).
 * Pair with PDMILEDPORTS.
 */
typedef struct PDMILEDCONNECTORS
{
    /**
     * Notification about a unit which have been changed.
     *
     * The driver must discard any pointers to data owned by
     * the unit and requery it.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit number.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnitChanged,(PPDMILEDCONNECTORS pInterface, unsigned iLUN));
} PDMILEDCONNECTORS;
/** PDMILEDCONNECTORS interface ID. */
#define PDMILEDCONNECTORS_IID                   "8ed63568-82a7-4193-b57b-db8085ac4495"


/** Pointer to a Media Notification interface. */
typedef struct PDMIMEDIANOTIFY  *PPDMIMEDIANOTIFY;
/**
 * Interface for exporting Medium eject information (up).  No interface pair.
 */
typedef struct PDMIMEDIANOTIFY
{
    /**
     * Signals that the medium was ejected.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   iLUN            The unit which had the medium ejected.
     */
    DECLR3CALLBACKMEMBER(int, pfnEjected,(PPDMIMEDIANOTIFY pInterface, unsigned iLUN));

} PDMIMEDIANOTIFY;
/** PDMIMEDIANOTIFY interface ID. */
#define PDMIMEDIANOTIFY_IID                     "fc22d53e-feb1-4a9c-b9fb-0a990a6ab288"


/** The special status unit number */
#define PDM_STATUS_LUN      999


#ifdef VBOX_WITH_HGCM

/** Abstract HGCM command structure. Used only to define a typed pointer. */
struct VBOXHGCMCMD;

/** Pointer to HGCM command structure. This pointer is unique and identifies
 *  the command being processed. The pointer is passed to HGCM connector methods,
 *  and must be passed back to HGCM port when command is completed.
 */
typedef struct VBOXHGCMCMD *PVBOXHGCMCMD;

/** Pointer to a HGCM port interface. */
typedef struct PDMIHGCMPORT *PPDMIHGCMPORT;
/**
 * Host-Guest communication manager port interface (down). Normally implemented
 * by VMMDev.
 * Pair with PDMIHGCMCONNECTOR.
 */
typedef struct PDMIHGCMPORT
{
    /**
     * Notify the guest on a command completion.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   rc                  The return code (VBox error code).
     * @param   pCmd                A pointer that identifies the completed command.
     *
     * @returns VBox status code
     */
    DECLR3CALLBACKMEMBER(void, pfnCompleted,(PPDMIHGCMPORT pInterface, int32_t rc, PVBOXHGCMCMD pCmd));

} PDMIHGCMPORT;
/** PDMIHGCMPORT interface ID. */
# define PDMIHGCMPORT_IID                       "e00a0cbf-b75a-45c3-87f4-41cddbc5ae0b"


/** Pointer to a HGCM service location structure. */
typedef struct HGCMSERVICELOCATION *PHGCMSERVICELOCATION;

/** Pointer to a HGCM connector interface. */
typedef struct PDMIHGCMCONNECTOR *PPDMIHGCMCONNECTOR;
/**
 * The Host-Guest communication manager connector interface (up). Normally
 * implemented by Main::VMMDevInterface.
 * Pair with PDMIHGCMPORT.
 */
typedef struct PDMIHGCMCONNECTOR
{
    /**
     * Locate a service and inform it about a client connection.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   pServiceLocation    Pointer to the service location structure.
     * @param   pu32ClientID        Where to store the client id for the connection.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnConnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, PHGCMSERVICELOCATION pServiceLocation, uint32_t *pu32ClientID));

    /**
     * Disconnect from service.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDisconnect,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID));

    /**
     * Process a guest issued command.
     *
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                A pointer that identifies the command.
     * @param   u32ClientID         The client id returned by the pfnConnect call.
     * @param   u32Function         Function to be performed by the service.
     * @param   cParms              Number of parameters in the array pointed to by paParams.
     * @param   paParms             Pointer to an array of parameters.
     * @return  VBox status code.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnCall,(PPDMIHGCMCONNECTOR pInterface, PVBOXHGCMCMD pCmd, uint32_t u32ClientID, uint32_t u32Function,
                                       uint32_t cParms, PVBOXHGCMSVCPARM paParms));

} PDMIHGCMCONNECTOR;
/** PDMIHGCMCONNECTOR interface ID. */
# define PDMIHGCMCONNECTOR_IID                  "a1104758-c888-4437-8f2a-7bac17865b5c"

#endif /* VBOX_WITH_HGCM */


/** Pointer to a display VBVA callbacks interface. */
typedef struct PDMIDISPLAYVBVACALLBACKS *PPDMIDISPLAYVBVACALLBACKS;
/**
 * Display VBVA callbacks interface (up).
 */
typedef struct PDMIDISPLAYVBVACALLBACKS
{

    /**
     * Informs guest about completion of processing the given Video HW Acceleration
     * command, does not wait for the guest to process the command.
     *
     * @returns ???
     * @param   pInterface          Pointer to this interface.
     * @param   pCmd                The Video HW Acceleration Command that was
     *                              completed.
     */
    DECLR3CALLBACKMEMBER(int, pfnVHWACommandCompleteAsync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                             PVBOXVHWACMD pCmd));

    DECLR3CALLBACKMEMBER(int, pfnCrHgsmiCommandCompleteAsync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                               struct VBOXVDMACMD_CHROMIUM_CMD* pCmd, int rc));

    DECLR3CALLBACKMEMBER(int, pfnCrHgsmiControlCompleteAsync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                               struct VBOXVDMACMD_CHROMIUM_CTL* pCmd, int rc));

    DECLR3CALLBACKMEMBER(int, pfnCrCtlSubmit, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                                   struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd,
                                                                   PFNCRCTLCOMPLETION pfnCompletion,
                                                                   void *pvCompletion));

    DECLR3CALLBACKMEMBER(int, pfnCrCtlSubmitSync, (PPDMIDISPLAYVBVACALLBACKS pInterface,
                                                                   struct VBOXCRCMDCTL* pCmd, uint32_t cbCmd));
} PDMIDISPLAYVBVACALLBACKS;
/** PDMIDISPLAYVBVACALLBACKS  */
#define PDMIDISPLAYVBVACALLBACKS_IID            "ddac0bd0-332d-4671-8853-732921a80216"

/** Pointer to a PCI raw connector interface. */
typedef struct PDMIPCIRAWCONNECTOR *PPDMIPCIRAWCONNECTOR;
/**
 * PCI raw connector interface (up).
 */
typedef struct PDMIPCIRAWCONNECTOR
{

    /**
     *
     */
    DECLR3CALLBACKMEMBER(int, pfnDeviceConstructComplete, (PPDMIPCIRAWCONNECTOR pInterface, const char *pcszName,
                                                           uint32_t uHostPciAddress, uint32_t uGuestPciAddress,
                                                           int rc));

} PDMIPCIRAWCONNECTOR;
/** PDMIPCIRAWCONNECTOR interface ID. */
#define PDMIPCIRAWCONNECTOR_IID                 "14aa9c6c-8869-4782-9dfc-910071a6aebf"

/** @} */

RT_C_DECLS_END

#endif
