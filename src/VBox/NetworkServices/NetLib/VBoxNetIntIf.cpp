/* $Id$ */
/** @file
 * VBoxNetIntIf - IntNet Interface Client Routines.
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEFAULT
#include "VBoxNetLib.h"
#include <VBox/intnet.h>
#include <VBox/intnetinline.h>
#include <VBox/sup.h>
#include <VBox/vmm/vmm.h>
#include <iprt/errcore.h>
#include <VBox/log.h>

#include <iprt/string.h>



/**
 * Flushes the send buffer.
 *
 * @returns VBox status code.
 * @param   pSession        The support driver session.
 * @param   hIf             The interface handle to flush.
 */
int VBoxNetIntIfFlush(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf)
{
    INTNETIFSENDREQ SendReq;
    SendReq.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    SendReq.Hdr.cbReq    = sizeof(SendReq);
    SendReq.pSession     = pSession;
    SendReq.hIf          = hIf;
    return SUPR3CallVMMR0Ex(NIL_RTR0PTR, NIL_VMCPUID, VMMR0_DO_INTNET_IF_SEND, 0, &SendReq.Hdr);
}


/**
 * Copys the SG segments into the specified fram.
 *
 * @param   pvFrame     The frame buffer.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 */
static void vboxnetIntIfCopySG(void *pvFrame, size_t cSegs, PCINTNETSEG paSegs)
{
    uint8_t *pbDst = (uint8_t *)pvFrame;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
    {
        memcpy(pbDst, paSegs[iSeg].pv, paSegs[iSeg].cb);
        pbDst += paSegs[iSeg].cb;
    }
}


/**
 * Writes a frame packet to the buffer.
 *
 * @returns VBox status code.
 * @param   pBuf        The buffer.
 * @param   pRingBuf    The ring buffer to read from.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 */
int VBoxNetIntIfRingWriteFrame(PINTNETBUF pBuf, PINTNETRINGBUF pRingBuf, size_t cSegs, PCINTNETSEG paSegs)
{
    RT_NOREF(pBuf);

    /*
     * Validate input.
     */
    AssertPtr(pBuf);
    AssertPtr(pRingBuf);
    AssertPtr(paSegs);
    Assert(cSegs > 0);

    /*
     * Calc frame size.
     */
    uint32_t cbFrame = 0;
    for (size_t iSeg = 0; iSeg < cSegs; iSeg++)
        cbFrame += paSegs[iSeg].cb;
    Assert(cbFrame >= sizeof(RTMAC) * 2);

    /*
     * Allocate a frame, copy the data and commit it.
     */
    PINTNETHDR pHdr = NULL;
    void *pvFrame = NULL;
    int rc = IntNetRingAllocateFrame(pRingBuf, cbFrame, &pHdr, &pvFrame);
    if (RT_SUCCESS(rc))
    {
        vboxnetIntIfCopySG(pvFrame, cSegs, paSegs);
        IntNetRingCommitFrame(pRingBuf, pHdr);
        return VINF_SUCCESS;
    }

    return rc;
}


/**
 * Sends a frame
 *
 * @returns VBox status code.
 * @param   pSession    The support driver session.
 * @param   hIf         The interface handle.
 * @param   pBuf        The interface buffer.
 * @param   cSegs       The number of segments.
 * @param   paSegs      The segments.
 * @param   fFlush      Whether to flush the write.
 */
int VBoxNetIntIfSend(PSUPDRVSESSION pSession, INTNETIFHANDLE hIf, PINTNETBUF pBuf,
                     size_t cSegs, PCINTNETSEG paSegs, bool fFlush)
{
    int rc = VBoxNetIntIfRingWriteFrame(pBuf, &pBuf->Send, cSegs, paSegs);
    if (rc == VERR_BUFFER_OVERFLOW)
    {
        VBoxNetIntIfFlush(pSession, hIf);
        rc = VBoxNetIntIfRingWriteFrame(pBuf, &pBuf->Send, cSegs, paSegs);
    }
    if (RT_SUCCESS(rc) && fFlush)
        rc = VBoxNetIntIfFlush(pSession, hIf);
    return rc;
}
