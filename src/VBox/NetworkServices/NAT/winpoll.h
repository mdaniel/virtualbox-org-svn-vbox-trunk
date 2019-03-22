/* $Id$ */
/** @file
 * NAT Network - poll(2) for winsock, definitions and declarations.
 */

/*
 * Copyright (C) 2013-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef _WINPOLL_H_
#define _WINPOLL_H_
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif
# include <iprt/cdefs.h>
/**
 * WinSock2 has definition for POLL* and pollfd, but it defined for _WIN32_WINNT > 0x0600
 * and used in WSAPoll, which has very unclear history.
 */
# if(_WIN32_WINNT < 0x0600)
#  define POLLRDNORM  0x0100
#  define POLLRDBAND  0x0200
#  define POLLIN      (POLLRDNORM | POLLRDBAND)
#  define POLLPRI     0x0400

#  define POLLWRNORM  0x0010
#  define POLLOUT     (POLLWRNORM)
#  define POLLWRBAND  0x0020

#  define POLLERR     0x0001
#  define POLLHUP     0x0002
#  define POLLNVAL    0x0004

struct pollfd {

    SOCKET  fd;
    SHORT   events;
    SHORT   revents;

};
#endif
RT_C_DECLS_BEGIN
int RTWinPoll(struct pollfd *pFds, unsigned int nfds, int timeout, int *pNready);
RT_C_DECLS_END
#endif
