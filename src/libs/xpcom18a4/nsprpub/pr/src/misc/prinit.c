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

#include "primpl.h"
#include <ctype.h>
#include <string.h>
#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/initterm.h>
#endif

#ifdef VBOX
#include <iprt/err.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/process.h>
#endif

PRLogModuleInfo *_pr_clock_lm;
PRLogModuleInfo *_pr_cmon_lm;
PRLogModuleInfo *_pr_io_lm;
PRLogModuleInfo *_pr_cvar_lm;
PRLogModuleInfo *_pr_mon_lm;
PRLogModuleInfo *_pr_linker_lm;
PRLogModuleInfo *_pr_sched_lm;
PRLogModuleInfo *_pr_thread_lm;
PRLogModuleInfo *_pr_gc_lm;
PRLogModuleInfo *_pr_shm_lm;
PRLogModuleInfo *_pr_shma_lm;

PRFileDesc *_pr_stdin;
PRFileDesc *_pr_stdout;
PRFileDesc *_pr_stderr;

PRBool _pr_initialized = PR_FALSE;


PR_IMPLEMENT(PRBool) PR_Initialized(void)
{
    return _pr_initialized;
}

PRInt32 _native_threads_only = 0;

static void _PR_InitStuff(void)
{

    if (_pr_initialized) return;
    _pr_initialized = PR_TRUE;
#ifdef VBOX_USE_IPRT_IN_NSPR
    RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
#endif

	_pr_clock_lm = PR_NewLogModule("clock");
	_pr_cmon_lm = PR_NewLogModule("cmon");
	_pr_io_lm = PR_NewLogModule("io");
	_pr_mon_lm = PR_NewLogModule("mon");
	_pr_linker_lm = PR_NewLogModule("linker");
	_pr_cvar_lm = PR_NewLogModule("cvar");
	_pr_sched_lm = PR_NewLogModule("sched");
	_pr_thread_lm = PR_NewLogModule("thread");
	_pr_gc_lm = PR_NewLogModule("gc");
	_pr_shm_lm = PR_NewLogModule("shm");
	_pr_shma_lm = PR_NewLogModule("shma");

    /* NOTE: These init's cannot depend on _PR_MD_CURRENT_THREAD() */
    _PR_MD_EARLY_INIT();

    _PR_InitLocks();
    _PR_InitClock();

    _PR_InitThreads(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    _PR_InitIO();
    _PR_InitLog();
    _PR_InitDtoa();

    nspr_InitializePRErrorTable();

    _PR_MD_FINAL_INIT();
}

void _PR_ImplicitInitialization(void)
{
	_PR_InitStuff();
}

PR_IMPLEMENT(void) PR_Init(
    PRThreadType type, PRThreadPriority priority, PRUintn maxPTDs)
{
    _PR_ImplicitInitialization();
}

PR_IMPLEMENT(PRIntn) PR_Initialize(
    PRPrimordialFn prmain, PRIntn argc, char **argv, PRUintn maxPTDs)
{
    PRIntn rv;
    _PR_ImplicitInitialization();
    rv = prmain(argc, argv);
	PR_Cleanup();
    return rv;
}  /* PR_Initialize */

PR_IMPLEMENT(PRProcessAttr *)
PR_NewProcessAttr(void)
{
    PRProcessAttr *attr;

    attr = PR_NEWZAP(PRProcessAttr);
    if (!attr) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }
    return attr;
}

PR_IMPLEMENT(void)
PR_ResetProcessAttr(PRProcessAttr *attr)
{
    PR_FREEIF(attr->currentDirectory);
    PR_FREEIF(attr->fdInheritBuffer);
    memset(attr, 0, sizeof(*attr));
}

PR_IMPLEMENT(void)
PR_DestroyProcessAttr(PRProcessAttr *attr)
{
    PR_FREEIF(attr->currentDirectory);
    PR_FREEIF(attr->fdInheritBuffer);
    PR_DELETE(attr);
}

PR_IMPLEMENT(void)
PR_ProcessAttrSetStdioRedirect(
    PRProcessAttr *attr,
    PRSpecialFD stdioFd,
    PRFileDesc *redirectFd)
{
    switch (stdioFd) {
        case PR_StandardInput:
            attr->stdinFd = redirectFd;
            break;
        case PR_StandardOutput:
            attr->stdoutFd = redirectFd;
            break;
        case PR_StandardError:
            attr->stderrFd = redirectFd;
            break;
        default:
            PR_ASSERT(0);
    }
}

PR_IMPLEMENT(PRStatus)
PR_ProcessAttrSetCurrentDirectory(
    PRProcessAttr *attr,
    const char *dir)
{
    PR_FREEIF(attr->currentDirectory);
    attr->currentDirectory = (char *) PR_MALLOC(strlen(dir) + 1);
    if (!attr->currentDirectory) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return PR_FAILURE;
    }
    strcpy(attr->currentDirectory, dir);
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRStatus)
PR_ProcessAttrSetInheritableFD(
    PRProcessAttr *attr,
    PRFileDesc *fd,
    const char *name)
{
    /* We malloc the fd inherit buffer in multiples of this number. */
#define FD_INHERIT_BUFFER_INCR 128
    /* The length of "NSPR_INHERIT_FDS=" */
#define NSPR_INHERIT_FDS_STRLEN 17
    /* The length of osfd (PRInt32) printed in hexadecimal with 0x prefix */
#define OSFD_STRLEN 10
    /* The length of fd type (PRDescType) printed in decimal */
#define FD_TYPE_STRLEN 1
    PRSize newSize;
    int remainder;
    char *newBuffer;
    int nwritten;
    char *cur;
    int freeSize;

    if (fd->identity != PR_NSPR_IO_LAYER) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (fd->secret->inheritable == _PR_TRI_UNKNOWN) {
        _PR_MD_QUERY_FD_INHERITABLE(fd);
    }
    if (fd->secret->inheritable != _PR_TRI_TRUE) {
        PR_SetError(PR_NO_ACCESS_RIGHTS_ERROR, 0);
        return PR_FAILURE;
    }

    /*
     * We also need to account for the : separators and the
     * terminating null byte.
     */
    if (NULL == attr->fdInheritBuffer) {
        /* The first time, we print "NSPR_INHERIT_FDS=<name>:<type>:<val>" */
        newSize = NSPR_INHERIT_FDS_STRLEN + strlen(name)
                + FD_TYPE_STRLEN + OSFD_STRLEN + 2 + 1;
    } else {
        /* At other times, we print ":<name>:<type>:<val>" */
        newSize = attr->fdInheritBufferUsed + strlen(name)
                + FD_TYPE_STRLEN + OSFD_STRLEN + 3 + 1;
    }
    if (newSize > attr->fdInheritBufferSize) {
        /* Make newSize a multiple of FD_INHERIT_BUFFER_INCR */
        remainder = newSize % FD_INHERIT_BUFFER_INCR;
        if (remainder != 0) {
            newSize += (FD_INHERIT_BUFFER_INCR - remainder);
        }
        if (NULL == attr->fdInheritBuffer) {
            newBuffer = (char *) PR_MALLOC(newSize);
        } else {
            newBuffer = (char *) PR_REALLOC(attr->fdInheritBuffer, newSize);
        }
        if (NULL == newBuffer) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return PR_FAILURE;
        }
        attr->fdInheritBuffer = newBuffer;
        attr->fdInheritBufferSize = newSize;
    }
    cur = attr->fdInheritBuffer + attr->fdInheritBufferUsed;
    freeSize = attr->fdInheritBufferSize - attr->fdInheritBufferUsed;
    if (0 == attr->fdInheritBufferUsed) {
        nwritten = PR_snprintf(cur, freeSize,
                "NSPR_INHERIT_FDS=%s:%d:0x%lx",
                name, (PRIntn)fd->methods->file_type, fd->secret->md.osfd);
    } else {
        nwritten = PR_snprintf(cur, freeSize, ":%s:%d:0x%lx",
                name, (PRIntn)fd->methods->file_type, fd->secret->md.osfd);
    }
    attr->fdInheritBufferUsed += nwritten;
    return PR_SUCCESS;
}

PR_IMPLEMENT(PRFileDesc *) PR_GetInheritedFD(
    const char *name)
{
    PRFileDesc *fd;
    const char *envVar;
    const char *ptr;
    int len = strlen(name);
    PRInt32 osfd;
    int nColons;
    PRIntn fileType;

    envVar = RTEnvGet("NSPR_INHERIT_FDS");
    if (NULL == envVar || '\0' == envVar[0]) {
        PR_SetError(PR_UNKNOWN_ERROR, 0);
        return NULL;
    }

    ptr = envVar;
    while (1) {
        if ((strncmp(ptr, name, len) == 0) && (ptr[len] == ':')) {
            ptr += len + 1;
            PR_sscanf(ptr, "%d:0x%lx", &fileType, &osfd);
            switch ((PRDescType)fileType) {
                case PR_DESC_FILE:
                    fd = PR_ImportFile(osfd);
                    break;
                case PR_DESC_PIPE:
                    fd = PR_ImportPipe(osfd);
                    break;
                case PR_DESC_SOCKET_TCP:
                    fd = PR_ImportTCPSocket(osfd);
                    break;
                case PR_DESC_SOCKET_UDP:
                    fd = PR_ImportUDPSocket(osfd);
                    break;
                default:
                    PR_ASSERT(0);
                    PR_SetError(PR_UNKNOWN_ERROR, 0);
                    fd = NULL;
                    break;
            }
            if (fd) {
                /*
                 * An inherited FD is inheritable by default.
                 * The child process needs to call PR_SetFDInheritable
                 * to make it non-inheritable if so desired.
                 */
                fd->secret->inheritable = _PR_TRI_TRUE;
            }
            return fd;
        }
        /* Skip three colons */
        nColons = 0;
        while (*ptr) {
            if (*ptr == ':') {
                if (++nColons == 3) {
                    break;
                }
            }
            ptr++;
        }
        if (*ptr == '\0') {
            PR_SetError(PR_UNKNOWN_ERROR, 0);
            return NULL;
        }
        ptr++;
    }
}

PR_IMPLEMENT(PRStatus) PR_CreateProcessDetached(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    int vrc;
    int nEnv, idx;
    RTENV childEnv;
    RTENV newEnv = RTENV_DEFAULT;

    /* this code doesn't support all attributes */
    PR_ASSERT(!attr || !attr->currentDirectory);
    /* no custom environment, please */
    PR_ASSERT(!envp);

    childEnv = RTENV_DEFAULT;
    if (attr && attr->fdInheritBuffer) {
        vrc = RTEnvClone(&newEnv, childEnv);
        if (RT_FAILURE(vrc))
            return PR_FAILURE;
        vrc = RTEnvPutEx(newEnv, attr->fdInheritBuffer);
        if (RT_FAILURE(vrc))
        {
            RTEnvDestroy(newEnv);
            return PR_FAILURE;
        }
        childEnv = newEnv;
    }

    PRTHANDLE pStdIn = NULL, pStdOut = NULL, pStdErr = NULL;
    RTHANDLE hStdIn, hStdOut, hStdErr;
    if (attr && attr->stdinFd)
    {
        hStdIn.enmType = RTHANDLETYPE_FILE;
        RTFileFromNative(&hStdIn.u.hFile, attr->stdinFd->secret->md.osfd);
        pStdIn = &hStdIn;
    }
    if (attr && attr->stdoutFd)
    {
        hStdOut.enmType = RTHANDLETYPE_FILE;
        RTFileFromNative(&hStdOut.u.hFile, attr->stdoutFd->secret->md.osfd);
        pStdOut = &hStdOut;
    }
    if (attr && attr->stderrFd)
    {
        hStdErr.enmType = RTHANDLETYPE_FILE;
        RTFileFromNative(&hStdErr.u.hFile, attr->stderrFd->secret->md.osfd);
        pStdErr = &hStdErr;
    }

    vrc = RTProcCreateEx(path, (const char **)argv, childEnv,
                         RTPROC_FLAGS_DETACHED, pStdIn, pStdOut, pStdErr,
                         NULL /* pszAsUser */, NULL /* pszPassword */, NULL /* pExtraData */,
                         NULL /* phProcess */);
    if (newEnv != RTENV_DEFAULT) {
        RTEnvDestroy(newEnv);
    }
    if (RT_SUCCESS(vrc))
        return PR_SUCCESS;
    else
        return PR_FAILURE;
}

/* prinit.c */

