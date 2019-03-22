/* $Id$ */

/** @file
 * VirtualBox Driver interface to the virtual Usb Card Reader.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_USBCARDREADER
#define ____H_USBCARDREADER

#include <VBox/vmm/pdmcardreaderinfs.h>
#include <VBox/vmm/pdmdrv.h>

class Console;
typedef struct USBCARDREADER USBCARDREADER;
typedef struct UCRREMOTE UCRREMOTE;

class UsbCardReader
{
    public:
        UsbCardReader(Console *console);
        virtual ~UsbCardReader();

        static const PDMDRVREG DrvReg;
        USBCARDREADER *mpDrv;

        Console *getParent(void) { return mParent; }

        int VRDENotify(uint32_t u32Id, void *pvData, uint32_t cbData);
        int VRDEResponse(int rcRequest, void *pvUser, uint32_t u32Function, void *pvData, uint32_t cbData);

        int EstablishContext(USBCARDREADER *pDrv);
        int ReleaseContext(USBCARDREADER *pDrv);
        int GetStatusChange(USBCARDREADER *pDrv, void *pvUser, uint32_t u32Timeout,
                            PDMICARDREADER_READERSTATE *paReaderStats, uint32_t cReaderStats);
        int Connect(USBCARDREADER *pDrv, void *pvUser, const char *pszReaderName,
                    uint32_t u32ShareMode, uint32_t u32PreferredProtocols);
        int Disconnect(USBCARDREADER *pDrv, void *pvUser, uint32_t u32Mode);
        int Status(USBCARDREADER *pDrv, void *pvUser);
        int Transmit(USBCARDREADER *pDrv, void *pvUser, PDMICARDREADER_IO_REQUEST *pioSendRequest,
                     uint8_t *pu8SendBuffer, uint32_t cbSendBuffer, uint32_t cbRecvBuffer);
        int Control(USBCARDREADER *pDrv, void *pvUser, uint32_t u32ControlCode,
                    uint8_t *pu8InBuffer, uint32_t cbInBuffer, uint32_t cbOutBuffer);
        int GetAttrib(USBCARDREADER *pDrv, void *pvUser, uint32_t u32AttrId, uint32_t cbAttrib);
        int SetAttrib(USBCARDREADER *pDrv, void *pvUser, uint32_t u32AttrId,
                      uint8_t *pu8Attrib, uint32_t cbAttrib);

    private:
        static DECLCALLBACK(void *) drvQueryInterface(PPDMIBASE pInterface, const char *pszIID);
        static DECLCALLBACK(int)    drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags);
        static DECLCALLBACK(void)   drvDestruct(PPDMDRVINS pDrvIns);

        int vrdeSCardRequest(void *pvUser, uint32_t u32Function, const void *pvData, uint32_t cbData);

        Console * const mParent;

        UCRREMOTE *m_pRemote;
};

#endif /* !____H_USBCARDREADER */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
