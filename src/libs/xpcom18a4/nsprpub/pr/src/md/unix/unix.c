/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/utsname.h>

#ifdef _PR_POLL_AVAILABLE
#include <poll.h>
#endif

/* To get FIONREAD */
#if defined(NCR) || defined(UNIXWARE) || defined(NEC) || defined(SNI) \
        || defined(SONY)
#include <sys/filio.h>
#endif

#if defined(NTO)
#include <sys/statvfs.h>
#endif

#if defined(FREEBSD)
# include <signal.h>
#endif

#ifdef VBOX
# include <iprt/mem.h>
#endif

/*
 * Make sure _PRSockLen_t is 32-bit, because we will cast a PRUint32* or
 * PRInt32* pointer to a _PRSockLen_t* pointer.
 */
#if defined(HAVE_SOCKLEN_T) \
    || (defined(LINUX) && defined(__GLIBC__) && __GLIBC__ >= 2)
#define _PRSockLen_t socklen_t
#elif defined(IRIX) || defined(HPUX) || defined(OSF1) || defined(SOLARIS) \
    || defined(AIX4_1) || defined(LINUX) || defined(SONY) \
    || defined(BSDI) || defined(SCO) || defined(NEC) || defined(SNI) \
    || defined(SUNOS4) || defined(NCR) || defined(DARWIN) \
    || defined(NEXTSTEP) || defined(QNX)
#define _PRSockLen_t int
#elif (defined(AIX) && !defined(AIX4_1)) || defined(FREEBSD) \
    || defined(NETBSD) || defined(OPENBSD) || defined(UNIXWARE) \
    || defined(DGUX) || defined(VMS) || defined(NTO)
#define _PRSockLen_t size_t
#else
#error "Cannot determine architecture"
#endif

/*
** Global lock variable used to bracket calls into rusty libraries that
** aren't thread safe (like libc, libX, etc).
*/
static PRLock *_pr_rename_lock = NULL;

static PRInt64 minus_one;

sigset_t timer_set;

void _MD_query_fd_inheritable(PRFileDesc *fd)
{
    int flags;

    PR_ASSERT(_PR_TRI_UNKNOWN == fd->secret->inheritable);
    flags = fcntl(fd->secret->md.osfd, F_GETFD, 0);
    PR_ASSERT(-1 != flags);
    fd->secret->inheritable = (flags & FD_CLOEXEC) ?
        _PR_TRI_FALSE : _PR_TRI_TRUE;
}

PROffset32 _MD_lseek(PRFileDesc *fd, PROffset32 offset, PRSeekWhence whence)
{
    PROffset32 rv, where;

    switch (whence) {
        case PR_SEEK_SET:
            where = SEEK_SET;
            break;
        case PR_SEEK_CUR:
            where = SEEK_CUR;
            break;
        case PR_SEEK_END:
            where = SEEK_END;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = -1;
            goto done;
    }
    rv = lseek(fd->secret->md.osfd,offset,where);
    if (rv == -1)
    {
        PRInt32 syserr = _MD_ERRNO();
        _PR_MD_MAP_LSEEK_ERROR(syserr);
    }
done:
    return(rv);
}

PROffset64 _MD_lseek64(PRFileDesc *fd, PROffset64 offset, PRSeekWhence whence)
{
    PRInt32 where;
    PROffset64 rv;

    switch (whence)
    {
        case PR_SEEK_SET:
            where = SEEK_SET;
            break;
        case PR_SEEK_CUR:
            where = SEEK_CUR;
            break;
        case PR_SEEK_END:
            where = SEEK_END;
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            rv = minus_one;
            goto done;
    }
    rv = _md_iovector._lseek64(fd->secret->md.osfd, offset, where);
    if (LL_EQ(rv, minus_one))
    {
        PRInt32 syserr = _MD_ERRNO();
        _PR_MD_MAP_LSEEK_ERROR(syserr);
    }
done:
    return rv;
}  /* _MD_lseek64 */

/*
** _MD_set_fileinfo_times --
**     Set the modifyTime and creationTime of the PRFileInfo
**     structure using the values in struct stat.
**
** _MD_set_fileinfo64_times --
**     Set the modifyTime and creationTime of the PRFileInfo64
**     structure using the values in _MDStat64.
*/

#if defined(_PR_STAT_HAS_ST_ATIM)
/*
** struct stat has st_atim, st_mtim, and st_ctim fields of
** type timestruc_t.
*/
static void _MD_set_fileinfo_times(
    const struct stat *sb,
    PRFileInfo *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtim.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtim.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctim.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctim.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}

static void _MD_set_fileinfo64_times(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtim.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtim.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctim.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctim.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}
#elif defined(_PR_STAT_HAS_ST_ATIM_UNION)
/*
** The st_atim, st_mtim, and st_ctim fields in struct stat are
** unions with a st__tim union member of type timestruc_t.
*/
static void _MD_set_fileinfo_times(
    const struct stat *sb,
    PRFileInfo *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtim.st__tim.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtim.st__tim.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctim.st__tim.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctim.st__tim.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}

static void _MD_set_fileinfo64_times(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtim.st__tim.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtim.st__tim.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctim.st__tim.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctim.st__tim.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}
#elif defined(_PR_STAT_HAS_ST_ATIMESPEC)
/*
** struct stat has st_atimespec, st_mtimespec, and st_ctimespec
** fields of type struct timespec.
*/
#if defined(_PR_TIMESPEC_HAS_TS_SEC)
static void _MD_set_fileinfo_times(
    const struct stat *sb,
    PRFileInfo *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtimespec.ts_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtimespec.ts_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctimespec.ts_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctimespec.ts_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}

static void _MD_set_fileinfo64_times(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtimespec.ts_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtimespec.ts_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctimespec.ts_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctimespec.ts_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}
#else /* _PR_TIMESPEC_HAS_TS_SEC */
/*
** The POSIX timespec structure has tv_sec and tv_nsec.
*/
static void _MD_set_fileinfo_times(
    const struct stat *sb,
    PRFileInfo *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtimespec.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtimespec.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctimespec.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctimespec.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}

static void _MD_set_fileinfo64_times(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    PRInt64 us, s2us;

    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(info->modifyTime, sb->st_mtimespec.tv_sec);
    LL_MUL(info->modifyTime, info->modifyTime, s2us);
    LL_I2L(us, sb->st_mtimespec.tv_nsec / 1000);
    LL_ADD(info->modifyTime, info->modifyTime, us);
    LL_I2L(info->creationTime, sb->st_ctimespec.tv_sec);
    LL_MUL(info->creationTime, info->creationTime, s2us);
    LL_I2L(us, sb->st_ctimespec.tv_nsec / 1000);
    LL_ADD(info->creationTime, info->creationTime, us);
}
#endif /* _PR_TIMESPEC_HAS_TS_SEC */
#elif defined(_PR_STAT_HAS_ONLY_ST_ATIME)
/*
** struct stat only has st_atime, st_mtime, and st_ctime fields
** of type time_t.
*/
static void _MD_set_fileinfo_times(
    const struct stat *sb,
    PRFileInfo *info)
{
    PRInt64 s, s2us;
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(s, sb->st_mtime);
    LL_MUL(s, s, s2us);
    info->modifyTime = s;
    LL_I2L(s, sb->st_ctime);
    LL_MUL(s, s, s2us);
    info->creationTime = s;
}

static void _MD_set_fileinfo64_times(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    PRInt64 s, s2us;
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(s, sb->st_mtime);
    LL_MUL(s, s, s2us);
    info->modifyTime = s;
    LL_I2L(s, sb->st_ctime);
    LL_MUL(s, s, s2us);
    info->creationTime = s;
}
#else
#error "I don't know yet"
#endif

static int _MD_convert_stat_to_fileinfo(
    const struct stat *sb,
    PRFileInfo *info)
{
    if (S_IFREG & sb->st_mode)
        info->type = PR_FILE_FILE;
    else if (S_IFDIR & sb->st_mode)
        info->type = PR_FILE_DIRECTORY;
    else
        info->type = PR_FILE_OTHER;

#if defined(_PR_HAVE_LARGE_OFF_T)
    if (0x7fffffffL < sb->st_size)
    {
        PR_SetError(PR_FILE_TOO_BIG_ERROR, 0);
        return -1;
    }
#endif /* defined(_PR_HAVE_LARGE_OFF_T) */
    info->size = sb->st_size;

    _MD_set_fileinfo_times(sb, info);
    return 0;
}  /* _MD_convert_stat_to_fileinfo */

static int _MD_convert_stat64_to_fileinfo64(
    const _MDStat64 *sb,
    PRFileInfo64 *info)
{
    if (S_IFREG & sb->st_mode)
        info->type = PR_FILE_FILE;
    else if (S_IFDIR & sb->st_mode)
        info->type = PR_FILE_DIRECTORY;
    else
        info->type = PR_FILE_OTHER;

    LL_I2L(info->size, sb->st_size);

    _MD_set_fileinfo64_times(sb, info);
    return 0;
}  /* _MD_convert_stat64_to_fileinfo64 */

PRInt32 _MD_getfileinfo(const char *fn, PRFileInfo *info)
{
    PRInt32 rv;
    struct stat sb;

    rv = stat(fn, &sb);
    if (rv < 0)
        _PR_MD_MAP_STAT_ERROR(_MD_ERRNO());
    else if (NULL != info)
        rv = _MD_convert_stat_to_fileinfo(&sb, info);
    return rv;
}

PRInt32 _MD_getfileinfo64(const char *fn, PRFileInfo64 *info)
{
    _MDStat64 sb;
    PRInt32 rv = _md_iovector._stat64(fn, &sb);
    if (rv < 0)
        _PR_MD_MAP_STAT_ERROR(_MD_ERRNO());
    else if (NULL != info)
        rv = _MD_convert_stat64_to_fileinfo64(&sb, info);
    return rv;
}

PRInt32 _MD_getopenfileinfo(const PRFileDesc *fd, PRFileInfo *info)
{
    struct stat sb;
    PRInt32 rv = fstat(fd->secret->md.osfd, &sb);
    if (rv < 0)
        _PR_MD_MAP_FSTAT_ERROR(_MD_ERRNO());
    else if (NULL != info)
        rv = _MD_convert_stat_to_fileinfo(&sb, info);
    return rv;
}

PRInt32 _MD_getopenfileinfo64(const PRFileDesc *fd, PRFileInfo64 *info)
{
    _MDStat64 sb;
    PRInt32 rv = _md_iovector._fstat64(fd->secret->md.osfd, &sb);
    if (rv < 0)
        _PR_MD_MAP_FSTAT_ERROR(_MD_ERRNO());
    else if (NULL != info)
        rv = _MD_convert_stat64_to_fileinfo64(&sb, info);
    return rv;
}

struct _MD_IOVector _md_iovector = { open };

/*
** These implementations are to emulate large file routines on systems that
** don't have them. Their goal is to check in case overflow occurs. Otherwise
** they will just operate as normal using 32-bit file routines.
**
** The checking might be pre- or post-op, depending on the semantics.
*/

#if defined(_PR_NO_LARGE_FILES)

static PROffset64 _MD_Unix_lseek64(PRIntn osfd, PROffset64 offset, PRIntn whence)
{
    PRUint64 maxoff;
    PROffset64 rv = minus_one;
    LL_I2L(maxoff, 0x7fffffff);
    if (LL_CMP(offset, <=, maxoff))
    {
        off_t off;
        LL_L2I(off, offset);
        LL_I2L(rv, lseek(osfd, off, whence));
    }
    else errno = EFBIG;  /* we can't go there */
    return rv;
}  /* _MD_Unix_lseek64 */

static void* _MD_Unix_mmap64(
    void *addr, PRSize len, PRIntn prot, PRIntn flags,
    PRIntn fildes, PRInt64 offset)
{
    PR_SetError(PR_FILE_TOO_BIG_ERROR, 0);
    return NULL;
}  /* _MD_Unix_mmap64 */
#endif /* defined(_PR_NO_LARGE_FILES) */

static void _PR_InitIOV(void)
{
#if defined(_PR_NO_LARGE_FILES)
    _md_iovector._open64 = open;
    _md_iovector._mmap64 = _MD_Unix_mmap64;
    _md_iovector._fstat64 = fstat;
    _md_iovector._stat64 = stat;
    _md_iovector._lseek64 = _MD_Unix_lseek64;
#elif defined(_PR_HAVE_OFF64_T)
    _md_iovector._open64 = open64;
    _md_iovector._mmap64 = mmap64;
    _md_iovector._fstat64 = fstat64;
    _md_iovector._stat64 = stat64;
    _md_iovector._lseek64 = lseek64;
#elif defined(_PR_HAVE_LARGE_OFF_T)
    _md_iovector._open64 = open;
    _md_iovector._mmap64 = mmap;
    _md_iovector._fstat64 = fstat;
    _md_iovector._stat64 = stat;
    _md_iovector._lseek64 = lseek;
#else
#error "I don't know yet"
#endif
    LL_I2L(minus_one, -1);
}  /* _PR_InitIOV */

void _PR_UnixInit(void)
{
    struct sigaction sigact;
    int rv;

    sigemptyset(&timer_set);

    sigact.sa_handler = SIG_IGN;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    rv = sigaction(SIGPIPE, &sigact, 0);
    PR_ASSERT(0 == rv);

    _pr_rename_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_rename_lock);

    _PR_InitIOV();  /* one last hack */
}

/*
 *-----------------------------------------------------------------------
 *
 * PR_Now --
 *
 *     Returns the current time in microseconds since the epoch.
 *     The epoch is midnight January 1, 1970 GMT.
 *     The implementation is machine dependent.  This is the Unix
 *     implementation.
 *     Cf. time_t time(time_t *tp)
 *
 *-----------------------------------------------------------------------
 */

PR_IMPLEMENT(PRTime)
PR_Now(void)
{
    struct timeval tv;
    PRInt64 s, us, s2us;

    GETTIMEOFDAY(&tv);
    LL_I2L(s2us, PR_USEC_PER_SEC);
    LL_I2L(s, tv.tv_sec);
    LL_I2L(us, tv.tv_usec);
    LL_MUL(s, s, s2us);
    LL_ADD(s, s, us);
    return s;
}

PRIntervalTime _PR_UNIX_GetInterval()
{
    struct timeval time;
    PRIntervalTime ticks;

    (void)GETTIMEOFDAY(&time);  /* fallicy of course */
    ticks = (PRUint32)time.tv_sec * PR_MSEC_PER_SEC;  /* that's in milliseconds */
    ticks += (PRUint32)time.tv_usec / PR_USEC_PER_MSEC;  /* so's that */
    return ticks;
}  /* _PR_SUNOS_GetInterval */

PRIntervalTime _PR_UNIX_TicksPerSecond()
{
    return 1000;  /* this needs some work :) */
}

/*
 * When a nonblocking connect has completed, determine whether it
 * succeeded or failed, and if it failed, what the error code is.
 *
 * The function returns the error code.  An error code of 0 means
 * that the nonblocking connect succeeded.
 */

int _MD_unix_get_nonblocking_connect_error(int osfd)
{
    int err;
    _PRSockLen_t optlen = sizeof(err);
    if (getsockopt(osfd, SOL_SOCKET, SO_ERROR, (char *) &err, &optlen) == -1) {
        return errno;
    } else {
        return err;
    }
}

/************************************************************************/

PR_IMPLEMENT(PRStatus) _MD_gethostname(char *name, PRUint32 namelen)
{
    PRIntn rv;

    rv = gethostname(name, namelen);
    if (0 == rv) {
        return PR_SUCCESS;
    }
    _PR_MD_MAP_GETHOSTNAME_ERROR(_MD_ERRNO());
    return PR_FAILURE;
}

#if defined(_PR_NEED_FAKE_POLL)

/*
 * Some platforms don't have poll().  For easier porting of code
 * that calls poll(), we emulate poll() using select().
 */

int poll(struct pollfd *filedes, unsigned long nfds, int timeout)
{
    int i;
    int rv;
    int maxfd;
    fd_set rd, wr, ex;
    struct timeval tv, *tvp;

    if (timeout < 0 && timeout != -1) {
        errno = EINVAL;
        return -1;
    }

    if (timeout == -1) {
        tvp = NULL;
    } else {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        tvp = &tv;
    }

    maxfd = -1;
    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);

    for (i = 0; i < nfds; i++) {
        int osfd = filedes[i].fd;
        int events = filedes[i].events;
        PRBool fdHasEvent = PR_FALSE;

        if (osfd < 0) {
            continue;  /* Skip this osfd. */
        }

        /*
         * Map the poll events to the select fd_sets.
         *     POLLIN, POLLRDNORM  ===> readable
         *     POLLOUT, POLLWRNORM ===> writable
         *     POLLPRI, POLLRDBAND ===> exception
         *     POLLNORM, POLLWRBAND (and POLLMSG on some platforms)
         *     are ignored.
         *
         * The output events POLLERR and POLLHUP are never turned on.
         * POLLNVAL may be turned on.
         */

        if (events & (POLLIN | POLLRDNORM)) {
            FD_SET(osfd, &rd);
            fdHasEvent = PR_TRUE;
        }
        if (events & (POLLOUT | POLLWRNORM)) {
            FD_SET(osfd, &wr);
            fdHasEvent = PR_TRUE;
        }
        if (events & (POLLPRI | POLLRDBAND)) {
            FD_SET(osfd, &ex);
            fdHasEvent = PR_TRUE;
        }
        if (fdHasEvent && osfd > maxfd) {
            maxfd = osfd;
        }
    }

    rv = select(maxfd + 1, &rd, &wr, &ex, tvp);

    /* Compute poll results */
    if (rv > 0) {
        rv = 0;
        for (i = 0; i < nfds; i++) {
            PRBool fdHasEvent = PR_FALSE;

            filedes[i].revents = 0;
            if (filedes[i].fd < 0) {
                continue;
            }
            if (FD_ISSET(filedes[i].fd, &rd)) {
                if (filedes[i].events & POLLIN) {
                    filedes[i].revents |= POLLIN;
                }
                if (filedes[i].events & POLLRDNORM) {
                    filedes[i].revents |= POLLRDNORM;
                }
                fdHasEvent = PR_TRUE;
            }
            if (FD_ISSET(filedes[i].fd, &wr)) {
                if (filedes[i].events & POLLOUT) {
                    filedes[i].revents |= POLLOUT;
                }
                if (filedes[i].events & POLLWRNORM) {
                    filedes[i].revents |= POLLWRNORM;
                }
                fdHasEvent = PR_TRUE;
            }
            if (FD_ISSET(filedes[i].fd, &ex)) {
                if (filedes[i].events & POLLPRI) {
                    filedes[i].revents |= POLLPRI;
                }
                if (filedes[i].events & POLLRDBAND) {
                    filedes[i].revents |= POLLRDBAND;
                }
                fdHasEvent = PR_TRUE;
            }
            if (fdHasEvent) {
                rv++;
            }
        }
        PR_ASSERT(rv > 0);
    } else if (rv == -1 && errno == EBADF) {
        rv = 0;
        for (i = 0; i < nfds; i++) {
            filedes[i].revents = 0;
            if (filedes[i].fd < 0) {
                continue;
            }
            if (fcntl(filedes[i].fd, F_GETFL, 0) == -1) {
                filedes[i].revents = POLLNVAL;
                rv++;
            }
        }
        PR_ASSERT(rv > 0);
    }
    PR_ASSERT(-1 != timeout || rv != 0);

    return rv;
}
#endif /* _PR_NEED_FAKE_POLL */

void _MD_EarlyInit(void)
{
#if defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD)
    /*
     * Ignore FPE because coercion of a NaN to an int causes SIGFPE
     * to be raised.
     */
    struct sigaction act;

    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    sigaction(SIGFPE, &act, 0);
#endif
}

#if defined(SOLARIS)
PRIntervalTime _MD_Solaris_TicksPerSecond(void)
{
    /*
     * Ticks have a 10-microsecond resolution.  So there are
     * 100000 ticks per second.
     */
    return 100000UL;
}

/* Interval timers, implemented using gethrtime() */

PRIntervalTime _MD_Solaris_GetInterval(void)
{
    union {
	hrtime_t hrt;  /* hrtime_t is a 64-bit (long long) integer */
	PRInt64 pr64;
    } time;
    PRInt64 resolution;
    PRIntervalTime ticks;

    time.hrt = gethrtime();  /* in nanoseconds */
    /*
     * Convert from nanoseconds to ticks.  A tick's resolution is
     * 10 microseconds, or 10000 nanoseconds.
     */
    LL_I2L(resolution, 10000);
    LL_DIV(time.pr64, time.pr64, resolution);
    LL_L2UI(ticks, time.pr64);
    return ticks;
}
#endif

