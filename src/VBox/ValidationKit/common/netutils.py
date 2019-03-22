# -*- coding: utf-8 -*-
# $Id$
# pylint: disable=C0302

"""
Common Network Utility Functions.
"""

from __future__ import print_function;

__copyright__ = \
"""
Copyright (C) 2012-2017 Oracle Corporation

This file is part of VirtualBox Open Source Edition (OSE), as
available from http://www.virtualbox.org. This file is free software;
you can redistribute it and/or modify it under the terms of the GNU
General Public License (GPL) as published by the Free Software
Foundation, in version 2 as it comes in the "COPYING" file of the
VirtualBox OSE distribution. VirtualBox OSE is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.

The contents of this file may alternatively be used under the terms
of the Common Development and Distribution License Version 1.0
(CDDL) only, as it comes in the "COPYING.CDDL" file of the
VirtualBox OSE distribution, in which case the provisions of the
CDDL are applicable instead of those of the GPL.

You may elect to license modified versions of this file under the
terms and conditions of either the GPL or the CDDL or both.
"""
__version__ = "$Revision$"


# Standard Python imports.
import socket;


def getPrimaryHostIpByUdp(sPeerIp = '255.255.255.255'):
    """
    Worker for getPrimaryHostIp.

    The method is opening a UDP socket targetting a random port on a
    limited (local LAN) broadcast address.  We then use getsockname() to
    obtain our own IP address, which should then be the primary IP.

    Unfortunately, this doesn't always work reliably on Solaris.  When for
    instance our host only is configured, which interface we end up on seems
    to be totally random.
    """

    try:    oSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
    except: oSocket = None;
    if oSocket is not None:
        try:
            oSocket.connect((sPeerIp, 1984));
            sHostIp = oSocket.getsockname()[0];
        except:
            sHostIp = None;
        oSocket.close();
        if sHostIp is not None:
            return sHostIp;
    return '127.0.0.1';


def getPrimaryHostIpByHostname():
    """
    Worker for getPrimaryHostIp.

    Attempts to resolve the hostname.
    """
    try:
        return socket.gethostbyname(getHostnameFqdn());
    except:
        return '127.0.0.1';


def getPrimaryHostIp():
    """
    Tries to figure out the primary (the one with default route), local
    IPv4 address.

    Returns the IP address on success and otherwise '127.0.0.1'.
    """

    #
    # This isn't quite as easy as one would think.  Doing a UDP connect to
    # 255.255.255.255 turns out to be problematic on solaris with more than one
    # network interface (IP is random selected it seems), as well as linux
    # where we've seen 127.0.1.1 being returned on some hosts.
    #
    # So a modified algorithm first try a known public IP address, ASSUMING
    # that the primary interface is the one that gets us onto the internet.
    # If that fails, due to routing or whatever, we try 255.255.255.255 and
    # then finally hostname resolution.
    #
    sHostIp = getPrimaryHostIpByUdp('8.8.8.8');
    if sHostIp.startswith('127.'):
        sHostIp = getPrimaryHostIpByUdp('255.255.255.255');
        if sHostIp.startswith('127.'):
            sHostIp = getPrimaryHostIpByHostname();
    return sHostIp;


def getHostnameFqdn():
    """
    Wrapper around getfqdn.

    Returns the fully qualified hostname, None if not found.
    """

    try:
        sHostname = socket.getfqdn();
    except:
        return None;

    if '.' in sHostname or sHostname.startswith('localhost'):
        return sHostname;

    #
    # Somewhat misconfigured system, needs expensive approach to guessing FQDN.
    # Get address information on the hostname and do a reverse lookup from that.
    #
    try:
        aAddressInfo = socket.getaddrinfo(sHostname, None);
    except:
        return sHostname;

    for aAI in aAddressInfo:
        try:    sName, _ = socket.getnameinfo(aAI[4], 0);
        except: continue;
        if '.' in sName and not set(sName).issubset(set('0123456789.')) and not sName.startswith('localhost'):
            return sName;

    return sHostname;

