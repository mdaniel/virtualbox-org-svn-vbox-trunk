/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
** File:   ptio.c
** Descritpion:  Implemenation of I/O methods for pthreads
*/

#include <pthread.h>
#include <string.h>  /* for memset() */
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(DARWIN)
#include <sys/utsname.h> /* for uname */
#endif
#if defined(SOLARIS)
#include <sys/filio.h>  /* to pick up FIONREAD */
#endif
#ifdef _PR_POLL_AVAILABLE
#include <poll.h>
#endif
/* To pick up getrlimit() etc. */
#include <sys/time.h>
#include <sys/resource.h>

#include "primpl.h"

#if defined(VBOX) && defined(_PR_POLL_AVAILABLE)
# include <poll.h>
#endif

#include <netinet/tcp.h>  /* TCP_NODELAY, TCP_MAXSEG */
#ifdef LINUX
/* TCP_CORK is not defined in <netinet/tcp.h> on Red Hat Linux 6.0 */
#ifndef TCP_CORK
#define TCP_CORK 3
#endif
#endif

#include <iprt/assert.h>
#include <iprt/thread.h>

#ifdef DARWIN
static PRBool _pr_ipv6_v6only_on_by_default;
/* The IPV6_V6ONLY socket option is not defined on Mac OS X 10.1. */
#ifndef IPV6_V6ONLY
#define IPV6_V6ONLY 27
#endif
#endif

#if defined(SOLARIS)
#define _PRSockOptVal_t char *
#elif defined(IRIX) || defined(OSF1) || defined(AIX) || defined(HPUX) \
    || defined(LINUX) || defined(FREEBSD) || defined(BSDI) || defined(VMS) \
    || defined(NTO) || defined(OPENBSD) || defined(DARWIN) \
    || defined(UNIXWARE) || defined(NETBSD)
#define _PRSockOptVal_t void *
#else
#error "Cannot determine architecture"
#endif

#if (defined(HPUX) && !defined(HPUX10_30) && !defined(HPUX11))
#define _PRSelectFdSetArg_t int *
#elif defined(AIX4_1)
#define _PRSelectFdSetArg_t void *
#elif defined(IRIX) || (defined(AIX) && !defined(AIX4_1)) \
    || defined(OSF1) || defined(SOLARIS) \
    || defined(HPUX10_30) || defined(HPUX11) || defined(LINUX) \
    || defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
    || defined(BSDI) || defined(VMS) || defined(NTO) || defined(DARWIN) \
    || defined(UNIXWARE)
#define _PRSelectFdSetArg_t fd_set *
#else
#error "Cannot determine architecture"
#endif

static PRFileDesc *pt_SetMethods(
    PRIntn osfd, PRDescType type, PRBool isAcceptedSocket, PRBool imported);

static PRLock *_pr_rename_lock;  /* For PR_Rename() */

/**************************************************************************/

/* These two functions are only used in assertions. */
#ifdef RT_STRICT

PRBool IsValidNetAddr(const PRNetAddr *addr)
{
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
            && (addr->raw.family != PR_AF_INET6)
            && (addr->raw.family != AF_INET)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

static PRBool IsValidNetAddrLen(const PRNetAddr *addr, PRInt32 addr_len)
{
    /*
     * The definition of the length of a Unix domain socket address
     * is not uniform, so we don't check it.
     */
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
            && (PR_NETADDR_SIZE(addr) != addr_len)) {
#if defined(LINUX) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 1
        /*
         * In glibc 2.1, struct sockaddr_in6 is 24 bytes.  In glibc 2.2
         * and in the 2.4 kernel, struct sockaddr_in6 has the scope_id
         * field and is 28 bytes.  It is possible for socket functions
         * to return an addr_len greater than sizeof(struct sockaddr_in6).
         * We need to allow that.  (Bugzilla bug #77264)
         */
        if ((PR_AF_INET6 == addr->raw.family)
                && (sizeof(addr->ipv6) == addr_len)) {
            return PR_TRUE;
        }
#endif
        return PR_FALSE;
    }
    return PR_TRUE;
}

#endif /* DEBUG */

/*****************************************************************************/
/************************* I/O Continuation machinery ************************/
/*****************************************************************************/

/*
 * The polling interval defines the maximum amount of time that a thread
 * might hang up before an interrupt is noticed.
 */
#define PT_DEFAULT_POLL_MSEC 5000

/*
 * pt_SockLen is the type for the length of a socket address
 * structure, used in the address length argument to bind,
 * connect, accept, getsockname, getpeername, etc.  Posix.1g
 * defines this type as socklen_t.  It is size_t or int on
 * most current systems.
 */
#if defined(HAVE_SOCKLEN_T) \
    || (defined(LINUX) && defined(__GLIBC__) && __GLIBC__ >= 2)
typedef socklen_t pt_SockLen;
#elif (defined(AIX) && !defined(AIX4_1)) \
    || defined(VMS)
typedef PRSize pt_SockLen;
#else
typedef PRIntn pt_SockLen;
#endif

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revents);

typedef enum pr_ContuationStatus
{
    pt_continuation_pending,
    pt_continuation_done
} pr_ContuationStatus;

struct pt_Continuation
{
    /* The building of the continuation operation */
    ContinuationFn function;                /* what function to continue */
    union { PRIntn osfd; } arg1;            /* #1 - the op's fd */
    union { void* buffer; } arg2;           /* #2 - primary transfer buffer */
    union {
        PRSize amount;                      /* #3 - size of 'buffer', or */
        pt_SockLen *addr_len;                  /*    - length of address */
    } arg3;
    union { PRIntn flags; } arg4;           /* #4 - read/write flags */
    union { PRNetAddr *addr; } arg5;        /* #5 - send/recv address */

    PRIntervalTime timeout;                 /* client (relative) timeout */

    PRInt16 event;                           /* flags for poll()'s events */

    /*
    ** The representation and notification of the results of the operation.
    ** These function can either return an int return code or a pointer to
    ** some object.
    */
    union { PRSize code; void *object; } result;

    PRIntn syserrno;                        /* in case it failed, why (errno) */
    pr_ContuationStatus status;             /* the status of the operation */
};

#if defined(DEBUG)

PTDebug pt_debug;  /* this is shared between several modules */

PR_IMPLEMENT(void) PT_FPrintStats(PRFileDesc *debug_out, const char *msg)
{
    PTDebug stats;
    char buffer[100];
    PRExplodedTime tod;
    PRInt64 elapsed, aMil;
    stats = pt_debug;  /* a copy */
    PR_ExplodeTime(stats.timeStarted, PR_LocalTimeParameters, &tod);
    (void)PR_FormatTime(buffer, sizeof(buffer), "%T", &tod);

    LL_SUB(elapsed, PR_Now(), stats.timeStarted);
    LL_I2L(aMil, 1000000);
    LL_DIV(elapsed, elapsed, aMil);

    if (NULL != msg) PR_fprintf(debug_out, "%s", msg);
    PR_fprintf(
        debug_out, "\tstarted: %s[%lld]\n", buffer, elapsed);
    PR_fprintf(
        debug_out, "\tlocks [created: %u, destroyed: %u]\n",
        stats.locks_created, stats.locks_destroyed);
    PR_fprintf(
        debug_out, "\tlocks [acquired: %u, released: %u]\n",
        stats.locks_acquired, stats.locks_released);
    PR_fprintf(
        debug_out, "\tcvars [created: %u, destroyed: %u]\n",
        stats.cvars_created, stats.cvars_destroyed);
    PR_fprintf(
        debug_out, "\tcvars [notified: %u, delayed_delete: %u]\n",
        stats.cvars_notified, stats.delayed_cv_deletes);
}  /* PT_FPrintStats */

#else

PR_IMPLEMENT(void) PT_FPrintStats(PRFileDesc *debug_out, const char *msg)
{
    /* do nothing */
}  /* PT_FPrintStats */

#endif  /* DEBUG */

/*
** Allocate a file descriptor from the heap.
*/
static PRFileDesc *_PR_Getfd(void)
{
    PRFileDesc *fd = PR_NEW(PRFileDesc);
    if (NULL != fd)
    {
        fd->secret = PR_NEW(PRFilePrivate);
        if (NULL == fd->secret) PR_DELETE(fd);
    }
    if (fd == NULL)
        return NULL;

    fd->dtor = NULL;
    fd->lower = fd->higher = NULL;
    fd->identity = PR_NSPR_IO_LAYER;
    memset(fd->secret, 0, sizeof(PRFilePrivate));
    return fd;
}  /* _PR_Getfd */

static void _PR_Putfd(PRFileDesc *fd)
{
    PR_Free(fd->secret);
    PR_Free(fd);
}  /* _PR_Putfd */

static void pt_poll_now(pt_Continuation *op)
{
    PRInt32 msecs;
	PRIntervalTime epoch, now, elapsed, remaining;
	PRBool wait_for_remaining;
    PRThread *self = PR_GetCurrentThread();

	Assert(PR_INTERVAL_NO_WAIT != op->timeout);

    switch (op->timeout) {
        case PR_INTERVAL_NO_TIMEOUT:
			msecs = PT_DEFAULT_POLL_MSEC;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

				rv = poll(&tmp_pfd, 1, msecs);

				if (self->state & PT_THREAD_ABORTED)
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if ((-1 == rv) && ((errno == EINTR) || (errno == EAGAIN)))
					continue; /* go around the loop again */

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
					|| ((events & POLLOUT) && (revents & POLLHUP)))
						/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
							op->status = pt_continuation_done;
					}
				} else if (rv == -1) {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
				/* else, poll timed out */
			} while (pt_continuation_done != op->status);
			break;
        default:
            now = epoch = PR_IntervalNow();
            remaining = op->timeout;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

    			wait_for_remaining = PR_TRUE;
    			msecs = (PRInt32)PR_IntervalToMilliseconds(remaining);
				if (msecs > PT_DEFAULT_POLL_MSEC)
				{
					wait_for_remaining = PR_FALSE;
					msecs = PT_DEFAULT_POLL_MSEC;
				}
				rv = poll(&tmp_pfd, 1, msecs);

				if (self->state & PT_THREAD_ABORTED)
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
						|| ((events & POLLOUT) && (revents & POLLHUP)))
											/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
						{
							op->status = pt_continuation_done;
						}
					}
				} else if ((rv == 0) ||
						((errno == EINTR) || (errno == EAGAIN))) {
					if (rv == 0)	/* poll timed out */
					{
						if (wait_for_remaining)
							now += remaining;
						else
							now += PR_MillisecondsToInterval(msecs);
					}
					else
						now = PR_IntervalNow();
					elapsed = (PRIntervalTime) (now - epoch);
					if (elapsed >= op->timeout) {
						op->result.code = -1;
						op->syserrno = ETIMEDOUT;
						op->status = pt_continuation_done;
					} else
						remaining = op->timeout - elapsed;
				} else {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
			} while (pt_continuation_done != op->status);
            break;
    }

}  /* pt_poll_now */

static PRIntn pt_Continue(pt_Continuation *op)
{
    op->status = pt_continuation_pending;  /* set default value */
	/*
	 * let each thread call poll directly
	 */
	pt_poll_now(op);
	Assert(pt_continuation_done == op->status);
    return op->result.code;
}  /* pt_Continue */

/*****************************************************************************/
/*********************** specific continuation functions *********************/
/*****************************************************************************/
static PRBool pt_connect_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = _MD_unix_get_nonblocking_connect_error(op->arg1.osfd);
    if (op->syserrno != 0) {
        op->result.code = -1;
    } else {
        op->result.code = 0;
    }
    return PR_TRUE; /* this one is cooked */
}  /* pt_connect_cont */

static PRBool pt_accept_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = 0;
    op->result.code = accept(
        op->arg1.osfd, op->arg2.buffer, op->arg3.addr_len);
    if (-1 == op->result.code)
    {
        op->syserrno = errno;
        if (EWOULDBLOCK == errno || EAGAIN == errno || ECONNABORTED == errno)
            return PR_FALSE;  /* do nothing - this one ain't finished */
    }
    return PR_TRUE;
}  /* pt_accept_cont */

static PRBool pt_read_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
    op->result.code = read(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    return ((-1 == op->result.code) &&
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_read_cont */

static PRBool pt_recv_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
#if defined(SOLARIS)
    if (0 == op->arg4.flags)
        op->result.code = read(
            op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    else
        op->result.code = recv(
            op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#else
    op->result.code = recv(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
    op->syserrno = errno;
    return ((-1 == op->result.code) &&
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recv_cont */

static PRBool pt_send_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
#if defined(SOLARIS)
    PRInt32 tmp_amount = op->arg3.amount;
#endif
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
#if defined(SOLARIS)
retry:
    bytes = write(op->arg1.osfd, op->arg2.buffer, tmp_amount);
#else
    bytes = send(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
    op->syserrno = errno;

#if defined(SOLARIS)
    /*
     * The write system call has been reported to return the ERANGE error
     * on occasion. Try to write in smaller chunks to workaround this bug.
     */
    if ((bytes == -1) && (op->syserrno == ERANGE))
    {
        if (tmp_amount > 1)
        {
            tmp_amount = tmp_amount/2;  /* half the bytes */
            goto retry;
        }
    }
#endif

    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_send_cont */

static PRBool pt_write_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    bytes = write(op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_write_cont */

static PRBool pt_writev_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    struct iovec *iov = (struct iovec*)op->arg2.buffer;
    /*
     * Same rules as write, but continuing seems to be a bit more
     * complicated. As the number of bytes sent grows, we have to
     * redefine the vector we're pointing at. We might have to
     * modify an individual vector parms or we might have to eliminate
     * a pair altogether.
     */
    bytes = writev(op->arg1.osfd, iov, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        PRIntn iov_index;
        op->result.code += bytes;  /* accumulate the number sent */
        for (iov_index = 0; iov_index < op->arg3.amount; ++iov_index)
        {
            /* how much progress did we make in the i/o vector? */
            if (bytes < iov[iov_index].iov_len)
            {
                /* this element's not done yet */
                char **bp = (char**)&(iov[iov_index].iov_base);
                iov[iov_index].iov_len -= bytes;  /* there's that much left */
                *bp += bytes;  /* starting there */
                break;  /* go off and do that */
            }
            bytes -= iov[iov_index].iov_len;  /* that element's consumed */
        }
        op->arg2.buffer = &iov[iov_index];  /* new start of array */
        op->arg3.amount -= iov_index;  /* and array length */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_writev_cont */

void _PR_InitIO(void)
{
#if defined(DEBUG)
    memset(&pt_debug, 0, sizeof(PTDebug));
    pt_debug.timeStarted = PR_Now();
#endif

    _pr_rename_lock = PR_NewLock();
    Assert(NULL != _pr_rename_lock);

    _pr_stdin = pt_SetMethods(0, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    _pr_stdout = pt_SetMethods(1, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    _pr_stderr = pt_SetMethods(2, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    Assert(_pr_stdin && _pr_stdout && _pr_stderr);

#ifdef DARWIN
    /* In Mac OS X v10.3 Panther Beta the IPV6_V6ONLY socket option
     * is turned on by default, contrary to what RFC 3493, Section
     * 5.3 says.  So we have to turn it off.  Find out whether we
     * are running on such a system.
     */
    {
        int osfd;
        osfd = socket(AF_INET6, SOCK_STREAM, 0);
        if (osfd != -1) {
            int on;
            int optlen = sizeof(on);
            if (getsockopt(osfd, IPPROTO_IPV6, IPV6_V6ONLY,
                    &on, &optlen) == 0) {
                _pr_ipv6_v6only_on_by_default = on;
            }
            close(osfd);
        }
    }
#endif
}  /* _PR_InitIO */

void _PR_CleanupIO(void)
{
    _PR_Putfd(_pr_stdin);
    _pr_stdin = NULL;
    _PR_Putfd(_pr_stdout);
    _pr_stdout = NULL;
    _PR_Putfd(_pr_stderr);
    _pr_stderr = NULL;

    if (_pr_rename_lock)
    {
        PR_DestroyLock(_pr_rename_lock);
        _pr_rename_lock = NULL;
    }
}  /* _PR_CleanupIO */

PR_IMPLEMENT(PRFileDesc*) PR_GetSpecialFD(PRSpecialFD osfd)
{
    PRFileDesc *result = NULL;
    Assert(osfd >= PR_StandardInput && osfd <= PR_StandardError);

    if (!_pr_initialized) _PR_ImplicitInitialization();

    switch (osfd)
    {
        case PR_StandardInput: result = _pr_stdin; break;
        case PR_StandardOutput: result = _pr_stdout; break;
        case PR_StandardError: result = _pr_stderr; break;
        default:
            (void)PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    }
    return result;
}  /* PR_GetSpecialFD */

/*****************************************************************************/
/***************************** I/O private methods ***************************/
/*****************************************************************************/

static PRBool pt_TestAbort(void)
{
    PRThread *me = PR_CurrentThread();
    if(_PT_THREAD_INTERRUPTED(me))
    {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        me->state &= ~PT_THREAD_ABORTED;
        return PR_TRUE;
    }
    return PR_FALSE;
}  /* pt_TestAbort */

static void pt_MapError(void (*mapper)(PRIntn), PRIntn syserrno)
{
    switch (syserrno)
    {
        case EINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0); break;
        case ETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0); break;
        default:
            mapper(syserrno);
    }
}  /* pt_MapError */

static PRStatus pt_Close(PRFileDesc *fd)
{
    if ((NULL == fd) || (NULL == fd->secret)
        || ((_PR_FILEDESC_OPEN != fd->secret->state)
        && (_PR_FILEDESC_CLOSED != fd->secret->state)))
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if (pt_TestAbort()) return PR_FAILURE;

    if (_PR_FILEDESC_OPEN == fd->secret->state)
    {
        if (-1 == close(fd->secret->md.osfd))
        {
#ifdef OSF1
            /*
             * Bug 86941: On Tru64 UNIX V5.0A and V5.1, the close()
             * system call, when called to close a TCP socket, may
             * return -1 with errno set to EINVAL but the system call
             * does close the socket successfully.  An application
             * may safely ignore the EINVAL error.  This bug is fixed
             * on Tru64 UNIX V5.1A and later.  The defect tracking
             * number is QAR 81431.
             */
            if (PR_DESC_SOCKET_TCP != fd->methods->file_type
            || EINVAL != errno)
            {
                pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
                return PR_FAILURE;
            }
#else
            pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
            return PR_FAILURE;
#endif
        }
        fd->secret->state = _PR_FILEDESC_CLOSED;
    }
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* pt_Close */

static PRInt32 pt_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return bytes;

    bytes = read(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.function = pt_read_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_READ_ERROR, syserrno);
    return bytes;
}  /* pt_Read */

static PRInt32 pt_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    if (pt_TestAbort()) return bytes;

    bytes = write(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        buf = (char *) buf + bytes;
        amount -= bytes;
        fNeedContinue = PR_TRUE;
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        bytes = 0;
        fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_write_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_WRITE_ERROR, syserrno);
    return bytes;
}  /* pt_Write */

static PRInt32 pt_Writev(
    PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_len, PRIntervalTime timeout)
{
    PRIntn iov_index;
    PRBool fNeedContinue = PR_FALSE;
    PRInt32 syserrno, bytes, rv = -1;
    struct iovec osiov_local[PR_MAX_IOVECTOR_SIZE], *osiov;
    int osiov_len;

    if (pt_TestAbort()) return rv;

    /* Ensured by PR_Writev */
    Assert(iov_len <= PR_MAX_IOVECTOR_SIZE);

    /*
     * We can't pass iov to writev because PRIOVec and struct iovec
     * may not be binary compatible.  Make osiov a copy of iov and
     * pass osiov to writev.  We can modify osiov if we need to
     * continue the operation.
     */
    osiov = osiov_local;
    osiov_len = iov_len;
    for (iov_index = 0; iov_index < osiov_len; iov_index++)
    {
        osiov[iov_index].iov_base = iov[iov_index].iov_base;
        osiov[iov_index].iov_len = iov[iov_index].iov_len;
    }

    rv = bytes = writev(fd->secret->md.osfd, osiov, osiov_len);
    syserrno = errno;

    if (!fd->secret->nonblocking)
    {
        if (bytes >= 0)
        {
            /*
             * If we moved some bytes, how does that implicate the
             * i/o vector list?  In other words, exactly where are
             * we within that array?  What are the parameters for
             * resumption?  Maybe we're done!
             */
            for ( ;osiov_len > 0; osiov++, osiov_len--)
            {
                if (bytes < osiov->iov_len)
                {
                    /* this one's not done yet */
                    osiov->iov_base = (char*)osiov->iov_base + bytes;
                    osiov->iov_len -= bytes;
                    break;  /* go off and do that */
                }
                bytes -= osiov->iov_len;  /* this one's done cooked */
            }
            Assert(osiov_len > 0 || bytes == 0);
            if (osiov_len > 0)
            {
                if (PR_INTERVAL_NO_WAIT == timeout)
                {
                    rv = -1;
                    syserrno = ETIMEDOUT;
                }
                else fNeedContinue = PR_TRUE;
            }
        }
        else if (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        {
            if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
            else
            {
                rv = 0;
                fNeedContinue = PR_TRUE;
            }
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;

        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)osiov;
        op.arg3.amount = osiov_len;
        op.timeout = timeout;
        op.result.code = rv;
        op.function = pt_writev_cont;
        op.event = POLLOUT | POLLPRI;
        rv = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (rv == -1) pt_MapError(_PR_MD_MAP_WRITEV_ERROR, syserrno);
    return rv;
}  /* pt_Writev */

static PRInt32 pt_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK(fd, offset, whence);
}  /* pt_Seek */

static PRInt64 pt_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK64(fd, offset, whence);
}  /* pt_Seek64 */

static PRInt32 pt_Available_f(PRFileDesc *fd)
{
    PRInt32 result, cur, end;

    cur = _PR_MD_LSEEK(fd, 0, PR_SEEK_CUR);

    if (cur >= 0)
        end = _PR_MD_LSEEK(fd, 0, PR_SEEK_END);

    if ((cur < 0) || (end < 0)) {
        return -1;
    }

    result = end - cur;
    _PR_MD_LSEEK(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available_f */

static PRInt32 pt_Available_s(PRFileDesc *fd)
{
    PRInt32 rv, bytes = -1;
    if (pt_TestAbort()) return bytes;

    rv = ioctl(fd->secret->md.osfd, FIONREAD, &bytes);

    if (rv == -1)
        pt_MapError(_PR_MD_MAP_SOCKETAVAILABLE_ERROR, errno);
    return bytes;
}  /* pt_Available_s */

static PRStatus pt_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo */

static PRStatus pt_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO64(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo64 */

static PRStatus pt_Synch(PRFileDesc *fd)
{
    return (NULL == fd) ? PR_FAILURE : PR_SUCCESS;
} /* pt_Synch */

static PRStatus pt_Fsync(PRFileDesc *fd)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = fsync(fd->secret->md.osfd);
    if (rv < 0) {
        pt_MapError(_PR_MD_MAP_FSYNC_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Fsync */

static PRStatus pt_Connect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PRIntn rv = -1, syserrno;
    pt_SockLen addr_len;
	const PRNetAddr *addrp = addr;
#if defined(_PR_HAVE_SOCKADDR_LEN)
	PRUint16 md_af = addr->raw.family;
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    Assert(IsValidNetAddr(addr) == PR_TRUE);
    addr_len = PR_NETADDR_SIZE(addr);

#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = md_af;
    rv = connect(fd->secret->md.osfd, (struct sockaddr*)&addrCopy, addr_len);
#else
    rv = connect(fd->secret->md.osfd, (struct sockaddr*)addrp, addr_len);
#endif
    syserrno = errno;
    if ((-1 == rv) && (EINPROGRESS == syserrno) && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
#ifdef _PR_HAVE_SOCKADDR_LEN
            op.arg2.buffer = (void*)&addrCopy;
#else
            op.arg2.buffer = (void*)addr;
#endif
            op.arg3.amount = addr_len;
            op.timeout = timeout;
            op.function = pt_connect_cont;
            op.event = POLLOUT | POLLPRI;
            rv = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (-1 == rv) {
        pt_MapError(_PR_MD_MAP_CONNECT_ERROR, syserrno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Connect */

static PRStatus pt_ConnectContinue(
    PRFileDesc *fd, PRInt16 out_flags)
{
    int err;
    PRInt32 osfd;

    if (out_flags & PR_POLL_NVAL)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR)) == 0)
    {
        Assert(out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
    }

    osfd = fd->secret->md.osfd;

    err = _MD_unix_get_nonblocking_connect_error(osfd);
    if (err != 0)
    {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_ConnectContinue */

PR_IMPLEMENT(PRStatus) PR_GetConnectStatus(const PRPollDesc *pd)
{
    /* Find the NSPR layer and invoke its connectcontinue method */
    PRFileDesc *bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);

    if (NULL == bottom)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    return pt_ConnectContinue(bottom, pd->out_flags);
}  /* PR_GetConnectStatus */

static PRFileDesc* pt_Accept(
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRFileDesc *newfd = NULL;
    PRIntn syserrno, osfd = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return newfd;

#ifdef _PR_STRICT_ADDR_LEN
    if (addr)
    {
        /*
         * Set addr->raw.family just so that we can use the
         * PR_NETADDR_SIZE macro.
         */
        addr->raw.family = fd->secret->af;
        addr_len = PR_NETADDR_SIZE(addr);
    }
#endif

    osfd = accept(fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
    syserrno = errno;

    if (osfd == -1)
    {
        if (fd->secret->nonblocking) goto failed;

        if (EWOULDBLOCK != syserrno && EAGAIN != syserrno
        && ECONNABORTED != syserrno)
            goto failed;
        else
        {
            if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
            else
            {
                pt_Continuation op;
                op.arg1.osfd = fd->secret->md.osfd;
                op.arg2.buffer = addr;
                op.arg3.addr_len = &addr_len;
                op.timeout = timeout;
                op.function = pt_accept_cont;
                op.event = POLLIN | POLLPRI;
                osfd = pt_Continue(&op);
                syserrno = op.syserrno;
            }
            if (osfd < 0) goto failed;
        }
    }
#ifdef _PR_HAVE_SOCKADDR_LEN
    /* ignore the sa_len field of struct sockaddr */
    if (addr)
    {
        addr->raw.family = ((struct sockaddr*)addr)->sa_family;
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
    newfd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP, PR_TRUE, PR_FALSE);
    if (newfd == NULL) close(osfd);  /* $$$ whoops! this doesn't work $$$ */
    else
    {
        Assert(IsValidNetAddr(addr) == PR_TRUE);
        Assert(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
#ifdef LINUX
        /*
         * On Linux, experiments showed that the accepted sockets
         * inherit the TCP_NODELAY socket option of the listening
         * socket.
         */
        newfd->secret->md.tcp_nodelay = fd->secret->md.tcp_nodelay;
#endif
    }
    return newfd;

failed:
    pt_MapError(_PR_MD_MAP_ACCEPT_ERROR, syserrno);
    return NULL;
}  /* pt_Accept */

static PRStatus pt_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    PRIntn rv;
    pt_SockLen addr_len;
	const PRNetAddr *addrp = addr;
#if defined(_PR_HAVE_SOCKADDR_LEN)
	PRUint16 md_af = addr->raw.family;
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    Assert(IsValidNetAddr(addr) == PR_TRUE);
    if (addr->raw.family == AF_UNIX)
    {
        /* Disallow relative pathnames */
        if (addr->local.path[0] != '/')
        {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
        }
    }

    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = md_af;
    rv = bind(fd->secret->md.osfd, (struct sockaddr*)&addrCopy, addr_len);
#else
    rv = bind(fd->secret->md.osfd, (struct sockaddr*)addrp, addr_len);
#endif

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_BIND_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Bind */

static PRStatus pt_Listen(PRFileDesc *fd, PRIntn backlog)
{
    PRIntn rv;

    if (pt_TestAbort()) return PR_FAILURE;

    rv = listen(fd->secret->md.osfd, backlog);
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_LISTEN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Listen */

static PRStatus pt_Shutdown(PRFileDesc *fd, PRIntn how)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = shutdown(fd->secret->md.osfd, how);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SHUTDOWN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Shutdown */

static PRInt16 pt_Poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}  /* pt_Poll */

static PRInt32 pt_Recv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRIntn osflags;

    if (0 == flags)
        osflags = 0;
    else if (PR_MSG_PEEK == flags)
        osflags = MSG_PEEK;
    else
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return bytes;
    }

    if (pt_TestAbort()) return bytes;

    /* recv() is a much slower call on pre-2.6 Solaris than read(). */
#if defined(SOLARIS)
    if (0 == osflags)
        bytes = read(fd->secret->md.osfd, buf, amount);
    else
        bytes = recv(fd->secret->md.osfd, buf, amount, osflags);
#else
    bytes = recv(fd->secret->md.osfd, buf, amount, osflags);
#endif
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
            op.arg2.buffer = buf;
            op.arg3.amount = amount;
            op.arg4.flags = osflags;
            op.timeout = timeout;
            op.function = pt_recv_cont;
            op.event = POLLIN | POLLPRI;
            bytes = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_RECV_ERROR, syserrno);
    return bytes;
}  /* pt_Recv */

static PRInt32 pt_SocketRead(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    return pt_Recv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}  /* pt_SocketRead */

static PRInt32 pt_Send(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;
#if defined(SOLARIS)
	PRInt32 tmp_amount = amount;
#endif

    /*
     * Under HP-UX DCE threads, pthread.h includes dce/cma_ux.h,
     * which has the following:
     *     #  define send        cma_send
     *     extern int  cma_send (int , void *, int, int );
     * So we need to cast away the 'const' of argument #2 for send().
     */
#if defined (HPUX) && defined(_PR_DCETHREADS)
#define PT_SENDBUF_CAST (void *)
#else
#define PT_SENDBUF_CAST
#endif

    if (pt_TestAbort()) return bytes;

    /*
     * On pre-2.6 Solaris, send() is much slower than write().
     * On 2.6 and beyond, with in-kernel sockets, send() and
     * write() are fairly equivalent in performance.
     */
#if defined(SOLARIS)
    Assert(0 == flags);
retry:
    bytes = write(fd->secret->md.osfd, PT_SENDBUF_CAST buf, tmp_amount);
#else
    bytes = send(fd->secret->md.osfd, PT_SENDBUF_CAST buf, amount, flags);
#endif
    syserrno = errno;

#if defined(SOLARIS)
    /*
     * The write system call has been reported to return the ERANGE error
     * on occasion. Try to write in smaller chunks to workaround this bug.
     */
    if ((bytes == -1) && (syserrno == ERANGE))
    {
        if (tmp_amount > 1)
        {
            tmp_amount = tmp_amount/2;  /* half the bytes */
            goto retry;
        }
    }
#endif

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout)
        {
            bytes = -1;
            syserrno = ETIMEDOUT;
        }
        else
        {
            buf = (char *) buf + bytes;
            amount -= bytes;
            fNeedContinue = PR_TRUE;
        }
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            bytes = 0;
            fNeedContinue = PR_TRUE;
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.timeout = timeout;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_send_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_SEND_ERROR, syserrno);
    return bytes;
}  /* pt_Send */

static PRInt32 pt_SocketWrite(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    return pt_Send(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}  /* pt_SocketWrite */

static PRStatus pt_GetSockName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getsockname(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETSOCKNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */

        Assert(IsValidNetAddr(addr) == PR_TRUE);
        Assert(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
        return PR_SUCCESS;
    }
}  /* pt_GetSockName */

static PRStatus pt_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getpeername(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETPEERNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */

        Assert(IsValidNetAddr(addr) == PR_TRUE);
        Assert(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
        return PR_SUCCESS;
    }
}  /* pt_GetPeerName */

static PRStatus pt_GetSocketOption(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRIntn rv;
    pt_SockLen length;
    PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a getsockopt() call
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        data->value.non_blocking = fd->secret->nonblocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                length = sizeof(linger);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char *) &linger, &length);
                Assert((-1 == rv) || (sizeof(linger) == length));
                data->value.linger.polarity =
                    (linger.l_onoff) ? PR_TRUE : PR_FALSE;
                data->value.linger.linger =
                    PR_SecondsToInterval(linger.l_linger);
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            case PR_SockOpt_Broadcast:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                Assert((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.reuse_addr = (0 == value) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 xbool;
                length = sizeof(xbool);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, &length);
                Assert((-1 == rv) || (sizeof(xbool) == length));
                data->value.mcast_loopback = (0 == xbool) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                Assert((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.recv_buffer_size = value;
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                length = sizeof(PRUintn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, &length);
                Assert((-1 == rv) || (sizeof(PRIntn) == length));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl;
                length = sizeof(ttl);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, &length);
                Assert((-1 == rv) || (sizeof(ttl) == length));
                data->value.mcast_ttl = ttl;
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                length = sizeof(mreq);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&mreq, &length);
                Assert((-1 == rv) || (sizeof(mreq) == length));
                data->value.add_member.mcaddr.inet.ip =
                    mreq.imr_multiaddr.s_addr;
                data->value.add_member.ifaddr.inet.ip =
                    mreq.imr_interface.s_addr;
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                length = sizeof(data->value.mcast_if.inet.ip);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip, &length);
                Assert((-1 == rv)
                    || (sizeof(data->value.mcast_if.inet.ip) == length));
                break;
            }
            default:
                break;
        }
        if (-1 == rv) _PR_MD_MAP_GETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_GetSocketOption */

static PRStatus pt_SetSocketOption(PRFileDesc *fd, const PRSocketOptionData *data)
{
    PRIntn rv;
    PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a setsockopt call.
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        fd->secret->nonblocking = data->value.non_blocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                linger.l_onoff = data->value.linger.polarity;
                linger.l_linger = PR_IntervalToSeconds(data->value.linger.linger);
                rv = setsockopt(
                    fd->secret->md.osfd, level, name, (char*)&linger, sizeof(linger));
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            case PR_SockOpt_Broadcast:
            {
                PRIntn value = (data->value.reuse_addr) ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
#ifdef LINUX
                /* for pt_Linux1 */
                if (name == TCP_NODELAY && rv == 0) {
                    fd->secret->md.tcp_nodelay = value;
                }
#endif
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 xbool = data->value.mcast_loopback ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, sizeof(xbool));
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value = data->value.recv_buffer_size;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, sizeof(PRUintn));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl = data->value.mcast_ttl;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, sizeof(ttl));
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                mreq.imr_multiaddr.s_addr =
                    data->value.add_member.mcaddr.inet.ip;
                mreq.imr_interface.s_addr =
                    data->value.add_member.ifaddr.inet.ip;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&mreq, sizeof(mreq));
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip,
                    sizeof(data->value.mcast_if.inet.ip));
                break;
            }
            default:
                break;
        }
        if (-1 == rv) _PR_MD_MAP_SETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_SetSocketOption */

/*****************************************************************************/
/****************************** I/O method objects ***************************/
/*****************************************************************************/

static PRIOMethods _pr_file_methods = {
    PR_DESC_FILE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_f,
    pt_Fsync,
    pt_Seek,
    pt_Seek64,
    pt_FileInfo,
    pt_FileInfo64,
    (PRWritevFN)_PR_InvalidInt,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    (PRShutdownFN)_PR_InvalidStatus,
    (PRRecvFN)_PR_InvalidInt,
    (PRSendFN)_PR_InvalidInt,
    pt_Poll,
    (PRGetsocknameFN)_PR_InvalidStatus,
    (PRGetpeernameFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_pipe_methods = {
    PR_DESC_PIPE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_s,
    pt_Synch,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    (PRShutdownFN)_PR_InvalidStatus,
    (PRRecvFN)_PR_InvalidInt,
    (PRSendFN)_PR_InvalidInt,
    pt_Poll,
    (PRGetsocknameFN)_PR_InvalidStatus,
    (PRGetpeernameFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_tcp_methods = {
    PR_DESC_SOCKET_TCP,
    pt_Close,
    pt_SocketRead,
    pt_SocketWrite,
    pt_Available_s,
    pt_Synch,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    pt_Writev,
    pt_Connect,
    pt_Accept,
    pt_Bind,
    pt_Listen,
    pt_Shutdown,
    pt_Recv,
    pt_Send,
    pt_Poll,
    pt_GetSockName,
    pt_GetPeerName,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    pt_GetSocketOption,
    pt_SetSocketOption,
    pt_ConnectContinue,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_socketpollfd_methods = {
    (PRDescType) 0,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,
    (PRConnectFN)_PR_InvalidStatus,
    (PRAcceptFN)_PR_InvalidDesc,
    (PRBindFN)_PR_InvalidStatus,
    (PRListenFN)_PR_InvalidStatus,
    (PRShutdownFN)_PR_InvalidStatus,
    (PRRecvFN)_PR_InvalidInt,
    (PRSendFN)_PR_InvalidInt,
	pt_Poll,
    (PRGetsocknameFN)_PR_InvalidStatus,
    (PRGetpeernameFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRConnectcontinueFN)_PR_InvalidStatus,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt
};

#if defined(HPUX) || defined(OSF1) || defined(SOLARIS) || defined (IRIX) \
    || defined(AIX) || defined(LINUX) || defined(FREEBSD) || defined(NETBSD) \
    || defined(OPENBSD) || defined(BSDI) || defined(VMS) || defined(NTO) \
    || defined(DARWIN) || defined(UNIXWARE)
#define _PR_FCNTL_FLAGS O_NONBLOCK
#else
#error "Can't determine architecture"
#endif

/*
 * Put a Unix file descriptor in non-blocking mode.
 */
static void pt_MakeFdNonblock(PRIntn osfd)
{
    PRIntn flags;
    flags = fcntl(osfd, F_GETFL, 0);
    flags |= _PR_FCNTL_FLAGS;
    (void)fcntl(osfd, F_SETFL, flags);
}

/*
 * Put a Unix socket fd in non-blocking mode that can
 * ideally be inherited by an accepted socket.
 *
 * Why doesn't pt_MakeFdNonblock do?  This is to deal with
 * the special case of HP-UX.  HP-UX has three kinds of
 * non-blocking modes for sockets: the fcntl() O_NONBLOCK
 * and O_NDELAY flags and ioctl() FIOSNBIO request.  Only
 * the ioctl() FIOSNBIO form of non-blocking mode is
 * inherited by an accepted socket.
 *
 * Other platforms just use the generic pt_MakeFdNonblock
 * to put a socket in non-blocking mode.
 */
#ifdef HPUX
static void pt_MakeSocketNonblock(PRIntn osfd)
{
    PRIntn one = 1;
    (void)ioctl(osfd, FIOSNBIO, &one);
}
#else
#define pt_MakeSocketNonblock pt_MakeFdNonblock
#endif

static PRFileDesc *pt_SetMethods(
    PRIntn osfd, PRDescType type, PRBool isAcceptedSocket, PRBool imported)
{
    PRFileDesc *fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->state = _PR_FILEDESC_OPEN;
        if (imported) fd->secret->inheritable = _PR_TRI_UNKNOWN;
        else
        {
            /* By default, a Unix fd is not closed on exec. */
            PRIntn flags;
            flags = fcntl(osfd, F_GETFD, 0);
            fd->secret->inheritable = flags & FD_CLOEXEC ? _PR_TRI_FALSE : _PR_TRI_TRUE;
        }
        switch (type)
        {
            case PR_DESC_FILE:
                fd->methods = PR_GetFileMethods();
                break;
            case PR_DESC_SOCKET_TCP:
                fd->methods = PR_GetTCPMethods();
#ifdef _PR_ACCEPT_INHERIT_NONBLOCK
                if (!isAcceptedSocket) pt_MakeSocketNonblock(osfd);
#else
                pt_MakeSocketNonblock(osfd);
#endif
                break;
            case PR_DESC_PIPE:
                fd->methods = PR_GetPipeMethods();
                pt_MakeFdNonblock(osfd);
                break;
            default:
                break;
        }
    }
    return fd;
}  /* pt_SetMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetFileMethods(void)
{
    return &_pr_file_methods;
}  /* PR_GetFileMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetPipeMethods(void)
{
    return &_pr_pipe_methods;
}  /* PR_GetPipeMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetTCPMethods(void)
{
    return &_pr_tcp_methods;
}  /* PR_GetTCPMethods */

static const PRIOMethods* PR_GetSocketPollFdMethods(void)
{
    return &_pr_socketpollfd_methods;
}  /* PR_GetSocketPollFdMethods */

PR_IMPLEMENT(PRFileDesc*) PR_AllocFileDesc(
    PRInt32 osfd, const PRIOMethods *methods)
{
    PRFileDesc *fd = _PR_Getfd();

    if (NULL == fd) goto failed;

    fd->methods = methods;
    fd->secret->md.osfd = osfd;
    /* Make fd non-blocking */
    if (osfd > 2)
    {
        /* Don't mess around with stdin, stdout or stderr */
        if (&_pr_tcp_methods == methods) pt_MakeSocketNonblock(osfd);
        else pt_MakeFdNonblock(osfd);
    }
    fd->secret->state = _PR_FILEDESC_OPEN;
    fd->secret->inheritable = _PR_TRI_UNKNOWN;
    return fd;

failed:
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return fd;
}  /* PR_AllocFileDesc */

#ifdef VBOX
PRUintn _PR_NetAddrSize(const PRNetAddr* addr)
{
    PRUintn addrsize;

#if defined(XP_UNIX) || defined(XP_OS2_EMX)
    if (AF_UNIX == addr->raw.family)
        addrsize = sizeof(addr->local);
#endif
    else addrsize = 0;

    return addrsize;
}  /* _PR_NetAddrSize */
#endif

PR_IMPLEMENT(PRFileDesc*) PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
    PRIntn osfd;
    PRDescType ftype;
    PRFileDesc *fd = NULL;
	PRInt32 tmp_domain = domain;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (PF_UNIX != domain)
    {
        PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
        return fd;
    }
	if (type == SOCK_STREAM) ftype = PR_DESC_SOCKET_TCP;
	else if (type == SOCK_DGRAM) ftype = PR_DESC_SOCKET_UDP;
	else
	{
		(void)PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
		return fd;
	}

    osfd = socket(domain, type, proto);
    if (osfd == -1) pt_MapError(_PR_MD_MAP_SOCKET_ERROR, errno);
    else
    {
        fd = pt_SetMethods(osfd, ftype, PR_FALSE, PR_FALSE);
        if (fd == NULL) close(osfd);
    }
#ifdef _PR_STRICT_ADDR_LEN
    if (fd != NULL) fd->secret->af = domain;
#endif
    return fd;
}  /* PR_Socket */

/*****************************************************************************/
/****************************** I/O public methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_OpenFile(
    const char *name, PRIntn flags, PRIntn mode)
{
    PRFileDesc *fd = NULL;
    PRIntn syserrno, osfd = -1, osflags = 0;;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (flags & PR_RDONLY) osflags |= O_RDONLY;
    if (flags & PR_WRONLY) osflags |= O_WRONLY;
    if (flags & PR_RDWR) osflags |= O_RDWR;
    if (flags & PR_APPEND) osflags |= O_APPEND;
    if (flags & PR_TRUNCATE) osflags |= O_TRUNC;
    if (flags & PR_EXCL) osflags |= O_EXCL;
    if (flags & PR_SYNC)
    {
#if defined(O_SYNC)
        osflags |= O_SYNC;
#elif defined(O_FSYNC)
        osflags |= O_FSYNC;
#else
#error "Neither O_SYNC nor O_FSYNC is defined on this platform"
#endif
    }

    /*
    ** We have to hold the lock across the creation in order to
    ** enforce the sematics of PR_Rename(). (see the latter for
    ** more details)
    */
    if (flags & PR_CREATE_FILE)
    {
        osflags |= O_CREAT;
        if (NULL !=_pr_rename_lock)
            PR_Lock(_pr_rename_lock);
    }

    osfd = _md_iovector._open64(name, osflags, mode);
    syserrno = errno;

    if ((flags & PR_CREATE_FILE) && (NULL !=_pr_rename_lock))
        PR_Unlock(_pr_rename_lock);

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_OPEN_ERROR, syserrno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_FILE, PR_FALSE, PR_FALSE);
        if (fd == NULL) close(osfd);  /* $$$ whoops! this is bad $$$ */
    }
    return fd;
}  /* PR_OpenFile */

PR_IMPLEMENT(PRFileDesc*) PR_Open(const char *name, PRIntn flags, PRIntn mode)
{
    return PR_OpenFile(name, flags, mode);
}  /* PR_Open */

PR_IMPLEMENT(PRStatus) PR_Delete(const char *name)
{
    PRIntn rv = -1;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return PR_FAILURE;

    rv = unlink(name);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_UNLINK_ERROR, errno);
        return PR_FAILURE;
    } else
        return PR_SUCCESS;
}  /* PR_Delete */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo(const char *fn, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETFILEINFO(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo64(const char *fn, PRFileInfo64 *info)
{
    PRInt32 rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    rv = _PR_MD_GETFILEINFO64(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo64 */

static PRInt32 _pr_poll_with_poll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 ready = 0;
    /*
     * For restarting poll() if it is interrupted by a signal.
     * We use these variables to figure out how much time has
     * elapsed and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    if (pt_TestAbort()) return -1;

    if (0 == npds) RTThreadSleep(PR_IntervalToMilliseconds(timeout));
    else
    {
#define STACK_POLL_DESC_COUNT 64
        struct pollfd stack_syspoll[STACK_POLL_DESC_COUNT];
        struct pollfd *syspoll;
        PRIntn index, msecs;

        if (npds <= STACK_POLL_DESC_COUNT)
        {
            syspoll = stack_syspoll;
        }
        else
        {
            PRThread *me = PR_GetCurrentThread();
            if (npds > me->syspoll_count)
            {
                PR_Free(me->syspoll_list);
                me->syspoll_list =
                    (struct pollfd*)PR_MALLOC(npds * sizeof(struct pollfd));
                if (NULL == me->syspoll_list)
                {
                    me->syspoll_count = 0;
                    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                    return -1;
                }
                me->syspoll_count = npds;
            }
            syspoll = me->syspoll_list;
        }

        for (index = 0; index < npds; ++index)
        {
            PRInt16 in_flags_read = 0, in_flags_write = 0;
            PRInt16 out_flags_read = 0, out_flags_write = 0;

            if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
            {
                if (pds[index].in_flags & PR_POLL_READ)
                {
                    in_flags_read = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_WRITE,
                        &out_flags_read);
                }
                if (pds[index].in_flags & PR_POLL_WRITE)
                {
                    in_flags_write = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_READ,
                        &out_flags_write);
                }
                if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
                {
                    /* this one is ready right now */
                    if (0 == ready)
                    {
                        /*
                         * We will return without calling the system
                         * poll function.  So zero the out_flags
                         * fields of all the poll descriptors before
                         * this one.
                         */
                        int i;
                        for (i = 0; i < index; i++)
                        {
                            pds[i].out_flags = 0;
                        }
                    }
                    ready += 1;
                    pds[index].out_flags = out_flags_read | out_flags_write;
                }
                else
                {
                    /* now locate the NSPR layer at the bottom of the stack */
                    PRFileDesc *bottom = PR_GetIdentitiesLayer(
                        pds[index].fd, PR_NSPR_IO_LAYER);
                    Assert(NULL != bottom);  /* what to do about that? */
                    pds[index].out_flags = 0;  /* pre-condition */
                    if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                    {
                        if (0 == ready)
                        {
                            syspoll[index].fd = bottom->secret->md.osfd;
                            syspoll[index].events = 0;
                            if (in_flags_read & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_read & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (in_flags_write & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_write & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (pds[index].in_flags & PR_POLL_EXCEPT)
                                syspoll[index].events |= POLLPRI;
                        }
                    }
                    else
                    {
                        if (0 == ready)
                        {
                            int i;
                            for (i = 0; i < index; i++)
                            {
                                pds[i].out_flags = 0;
                            }
                        }
                        ready += 1;  /* this will cause an abrupt return */
                        pds[index].out_flags = PR_POLL_NVAL;  /* bogii */
                    }
                }
            }
            else
            {
                /* make poll() ignore this entry */
                syspoll[index].fd = -1;
                syspoll[index].events = 0;
                pds[index].out_flags = 0;
            }
        }
        if (0 == ready)
        {
            switch (timeout)
            {
            case PR_INTERVAL_NO_WAIT: msecs = 0; break;
            case PR_INTERVAL_NO_TIMEOUT: msecs = -1; break;
            default:
                msecs = PR_IntervalToMilliseconds(timeout);
                start = PR_IntervalNow();
            }

retry:
            ready = poll(syspoll, npds, msecs);
            if (-1 == ready)
            {
                PRIntn oserror = errno;

                if (EINTR == oserror)
                {
                    if (timeout == PR_INTERVAL_NO_TIMEOUT)
                        goto retry;
                    else if (timeout == PR_INTERVAL_NO_WAIT)
                        ready = 0;  /* don't retry, just time out */
                    else
                    {
                        elapsed = (PRIntervalTime) (PR_IntervalNow()
                                - start);
                        if (elapsed > timeout)
                            ready = 0;  /* timed out */
                        else
                        {
                            remaining = timeout - elapsed;
                            msecs = PR_IntervalToMilliseconds(remaining);
                            goto retry;
                        }
                    }
                }
                else
                {
                    _PR_MD_MAP_POLL_ERROR(oserror);
                }
            }
            else if (ready > 0)
            {
                for (index = 0; index < npds; ++index)
                {
                    PRInt16 out_flags = 0;
                    if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
                    {
                        if (0 != syspoll[index].revents)
                        {
                            if (syspoll[index].revents & POLLIN)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_READ)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_READ)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLOUT)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLPRI)
                                out_flags |= PR_POLL_EXCEPT;
                            if (syspoll[index].revents & POLLERR)
                                out_flags |= PR_POLL_ERR;
                            if (syspoll[index].revents & POLLNVAL)
                                out_flags |= PR_POLL_NVAL;
                            if (syspoll[index].revents & POLLHUP)
                                out_flags |= PR_POLL_HUP;
                        }
                    }
                    pds[index].out_flags = out_flags;
                }
            }
        }
    }
    return ready;

} /* _pr_poll_with_poll */

PR_IMPLEMENT(PRInt32) PR_Poll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
	return(_pr_poll_with_poll(pds, npds, timeout));
}

PR_IMPLEMENT(PRFileDesc*) PR_OpenTCPSocket(PRIntn af)
{
    return PR_Socket(af, SOCK_STREAM, 0);
}  /* PR_NewTCPSocket */

PR_IMPLEMENT(PRStatus) PR_CreatePipe(
    PRFileDesc **readPipe,
    PRFileDesc **writePipe
)
{
    int pipefd[2];

    if (pt_TestAbort()) return PR_FAILURE;

    if (pipe(pipefd) == -1)
    {
    /* XXX map pipe error */
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return PR_FAILURE;
    }
    fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
    *readPipe = pt_SetMethods(pipefd[0], PR_DESC_PIPE, PR_FALSE, PR_FALSE);
    if (NULL == *readPipe)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    *writePipe = pt_SetMethods(pipefd[1], PR_DESC_PIPE, PR_FALSE, PR_FALSE);
    if (NULL == *writePipe)
    {
        PR_Close(*readPipe);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/*
** Set the inheritance attribute of a file descriptor.
*/
PR_IMPLEMENT(PRStatus) PR_SetFDInheritable(
    PRFileDesc *fd,
    PRBool inheritable)
{
    /*
     * Only a non-layered, NSPR file descriptor can be inherited
     * by a child process.
     */
    if (fd->identity != PR_NSPR_IO_LAYER)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (fd->secret->inheritable != inheritable)
    {
        if (fcntl(fd->secret->md.osfd, F_SETFD,
        inheritable ? 0 : FD_CLOEXEC) == -1)
        {
            return PR_FAILURE;
        }
        fd->secret->inheritable = (_PRTriStateBool) inheritable;
    }
    return PR_SUCCESS;
}

/*****************************************************************************/
/***************************** I/O friends methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_ImportFile(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportFile */

PR_IMPLEMENT(PRFileDesc*) PR_ImportPipe(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_PIPE, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportPipe */

PR_IMPLEMENT(PRFileDesc*) PR_ImportTCPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
#ifdef _PR_STRICT_ADDR_LEN
    if (NULL != fd) fd->secret->af = PF_INET;
#endif
    return fd;
}  /* PR_ImportTCPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_ImportUDPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_UDP, PR_FALSE, PR_TRUE);
    if (NULL != fd) close(osfd);
    return fd;
}  /* PR_ImportUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_CreateSocketPollFd(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->inheritable = _PR_TRI_FALSE;
    	fd->secret->state = _PR_FILEDESC_OPEN;
        fd->methods = PR_GetSocketPollFdMethods();
    }

    return fd;
}  /* PR_CreateSocketPollFD */

PR_IMPLEMENT(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd)
{
    if (NULL == fd)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    fd->secret->state = _PR_FILEDESC_CLOSED;
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* PR_DestroySocketPollFd */

#ifdef VBOX
PR_IMPLEMENT(PRFileDesc*) PR_GetIdentitiesLayer(PRFileDesc* fd, PRDescIdentity id)
{
    PRFileDesc *layer = fd;

    if (PR_TOP_IO_LAYER == id) {
    	if (PR_IO_LAYER_HEAD == fd->identity)
			return fd->lower;
		else 
			return fd;
	}

    for (layer = fd; layer != NULL; layer = layer->lower)
    {
        if (id == layer->identity) return layer;
    }
    for (layer = fd; layer != NULL; layer = layer->higher)
    {
        if (id == layer->identity) return layer;
    }
    return NULL;
}  /* PR_GetIdentitiesLayer */
#endif

PR_IMPLEMENT(PRInt32) PR_FileDesc2NativeHandle(PRFileDesc *bottom)
{
    PRInt32 osfd = -1;
    bottom = (NULL == bottom) ?
        NULL : PR_GetIdentitiesLayer(bottom, PR_NSPR_IO_LAYER);
    if (NULL == bottom) PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    else osfd = bottom->secret->md.osfd;
    return osfd;
}  /* PR_FileDesc2NativeHandle */

PR_IMPLEMENT(void) PR_ChangeFileDescNativeHandle(PRFileDesc *fd,
    PRInt32 handle)
{
    if (fd) fd->secret->md.osfd = handle;
}  /*  PR_ChangeFileDescNativeHandle*/

/* ptio.c */
