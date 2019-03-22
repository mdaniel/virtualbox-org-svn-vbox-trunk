/* $Id$ */
/** @file
 * Missing description.
 */

/*
 * Copyright (C) 2010-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "SHGSMIHost.h"
#include <VBoxVideo.h>

/*
 * VBOXSHGSMI made on top HGSMI and allows receiving notifications
 * about G->H command completion
 */

static int vboxSHGSMICommandCompleteAsynch (PHGSMIINSTANCE pIns, VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr)
{
    bool fDoIrq = !!(pHdr->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ)
                || !!(pHdr->fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_IRQ_FORCE);
    return HGSMICompleteGuestCommand(pIns, pHdr, fDoIrq);
}

void VBoxSHGSMICommandMarkAsynchCompletion(void RT_UNTRUSTED_VOLATILE_GUEST *pvData)
{
    VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr = VBoxSHGSMIBufferHeader(pvData);
    Assert(!(pHdr->fFlags & VBOXSHGSMI_FLAG_HG_ASYNCH));
    pHdr->fFlags |= VBOXSHGSMI_FLAG_HG_ASYNCH;
}

int VBoxSHGSMICommandComplete(PHGSMIINSTANCE pIns, void RT_UNTRUSTED_VOLATILE_GUEST *pvData)
{
    VBOXSHGSMIHEADER RT_UNTRUSTED_VOLATILE_GUEST *pHdr = VBoxSHGSMIBufferHeader(pvData);
    uint32_t fFlags = pHdr->fFlags;
    ASMCompilerBarrier();
    if (   !(fFlags & VBOXSHGSMI_FLAG_HG_ASYNCH)        /* <- check if synchronous completion */
        && !(fFlags & VBOXSHGSMI_FLAG_GH_ASYNCH_FORCE)) /* <- check if can complete synchronously */
        return VINF_SUCCESS;

    pHdr->fFlags = fFlags | VBOXSHGSMI_FLAG_HG_ASYNCH;
    return vboxSHGSMICommandCompleteAsynch(pIns, pHdr);
}
