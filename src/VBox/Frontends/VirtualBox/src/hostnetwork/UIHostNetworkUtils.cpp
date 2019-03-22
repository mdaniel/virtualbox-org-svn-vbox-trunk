/* $Id$ */
/** @file
 * VBox Qt GUI - UIHostNetworkUtils namespace implementation.
 */

/*
 * Copyright (C) 2017-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* GUI includes: */
# include "UIHostNetworkUtils.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


quint32 UIHostNetworkUtils::ipv4FromQStringToQuint32(const QString &strAddress)
{
    quint32 uAddress = 0;
    foreach (const QString &strPart, strAddress.split('.'))
    {
        uAddress = uAddress << 8;
        bool fOk = false;
        uint uPart = strPart.toUInt(&fOk);
        if (fOk)
            uAddress += uPart;
    }
    return uAddress;
}

QString UIHostNetworkUtils::ipv4FromQuint32ToQString(quint32 uAddress)
{
    QStringList address;
    while (uAddress)
    {
        uint uPart = uAddress & 0xFF;
        address.prepend(QString::number(uPart));
        uAddress = uAddress >> 8;
    }
    return address.join('.');
}

quint32 UIHostNetworkUtils::incrementNetworkAddress(quint32 uAddress)
{
    return advanceNetworkAddress(uAddress, true /* forward */);
}

quint32 UIHostNetworkUtils::decrementNetworkAddress(quint32 uAddress)
{
    return advanceNetworkAddress(uAddress, false /* forward */);
}

quint32 UIHostNetworkUtils::advanceNetworkAddress(quint32 uAddress, bool fForward)
{
    /* Success by default: */
    bool fSuccess = true;
    do
    {
        /* Just advance address: */
        if (fForward)
            ++uAddress;
        else
            --uAddress;
        /* And treat it as success initially: */
        fSuccess = true;
        /* Iterate the resulting bytes: */
        uint uByteIndex = 0;
        quint32 uIterator = uAddress;
        while (fSuccess && uIterator)
        {
            /* Get current byte: */
            const quint32 uCurrentByte = uIterator & 0xFF;
            /* Advance iterator early: */
            uIterator = uIterator >> 8;
            // We know that .0. and .255. are legal these days
            // but still prefer to exclude them from
            // being proposed to an end user.
            /* If current byte equal to 255
             * or first byte equal to 0,
             * let's try again: */
            if (   uCurrentByte == 0xFF
                || (uCurrentByte == 0x00 && uByteIndex == 0))
                fSuccess = false;
            /* Advance byte index: */
            ++uByteIndex;
        }
    }
    while (!fSuccess);
    return uAddress;
}

QStringList UIHostNetworkUtils::makeDhcpServerProposal(const QString &strInterfaceAddress, const QString &strInterfaceMask)
{
    /* Convert interface address/mask into digital form and calculate inverted interface mask: */
    const quint32 uAddress = ipv4FromQStringToQuint32(strInterfaceAddress);
    const quint32 uMaskDirect = ipv4FromQStringToQuint32(strInterfaceMask);
    const quint32 uMaskInvert = ~uMaskDirect;
    //printf("Direct   mask: %s (%u)\n", ipv4FromQuint32ToQString(uMaskDirect).toUtf8().constData(), uMaskDirect);
    //printf("Inverted mask: %s (%u)\n", ipv4FromQuint32ToQString(uMaskInvert).toUtf8().constData(), uMaskInvert);

    /* Split the interface address into left and right parts: */
    const quint32 uPartL  = uAddress & uMaskDirect;
    const quint32 uPartR = uAddress & uMaskInvert;
    //printf("Left  part: %s (%u)\n", ipv4FromQuint32ToQString(uPartL).toUtf8().constData(), uPartL);
    //printf("Right part: %s (%u)\n", ipv4FromQuint32ToQString(uPartR).toUtf8().constData(), uPartR);

    /* Prepare DHCP server proposal:" */
    quint32 uServerProposedAddress = 0;
    quint32 uServerProposedAddressL = 0;
    quint32 uServerProposedAddressU = 0;
    if (uPartR < uMaskInvert / 2)
    {
        /* Make DHCP server proposal from right scope: */
        //printf("Make DHCP server proposal from right scope:\n");
        uServerProposedAddress  = uPartL + incrementNetworkAddress(uPartR);
        uServerProposedAddressL = uPartL + incrementNetworkAddress(incrementNetworkAddress(uPartR));
        uServerProposedAddressU = uPartL + (uMaskInvert & 0xFEFEFEFE) /* decrementNetworkAddress(uMaskInvert) */;
    }
    else
    {
        /* Make DHCP server proposal from left scope: */
        //printf("Make DHCP server proposal from left scope:\n");
        uServerProposedAddress  = uPartL + 1 /* incrementNetworkAddress(0) */;
        uServerProposedAddressL = uPartL + 2 /* incrementNetworkAddress(incrementNetworkAddress(0)) */;
        uServerProposedAddressU = uPartL + decrementNetworkAddress(uPartR);
    }
    //printf("DHCP server address: %s (%u)\n", ipv4FromQuint32ToQString(uServerProposedAddress).toUtf8().constData(), uServerProposedAddress);
    //printf("DHCP server lower address: %s (%u)\n", ipv4FromQuint32ToQString(uServerProposedAddressL).toUtf8().constData(), uServerProposedAddressL);
    //printf("DHCP server upper address: %s (%u)\n", ipv4FromQuint32ToQString(uServerProposedAddressU).toUtf8().constData(), uServerProposedAddressU);

    /* Pack and return result: */
    return QStringList() << ipv4FromQuint32ToQString(uServerProposedAddress)
                         << ipv4FromQuint32ToQString(uMaskDirect)
                         << ipv4FromQuint32ToQString(uServerProposedAddressL)
                         << ipv4FromQuint32ToQString(uServerProposedAddressU);
}

