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
** Thread safe versions of malloc, free, realloc, calloc and cfree.
*/

#include "primpl.h"
#ifdef VBOX_USE_IPRT_IN_NSPR
# include <iprt/mem.h>
#endif

#ifdef _PR_ZONE_ALLOCATOR

#ifdef VBOX_WITH_XPCOM_NAMESPACE_CLEANUP
#define PR_FPrintZoneStats VBoxNsprPR_FPrintZoneStats
#endif /* VBOX_WITH_XPCOM_NAMESPACE_CLEANUP */

/*
** The zone allocator code must use native mutexes and cannot
** use PRLocks because PR_NewLock calls PR_Calloc, resulting
** in cyclic dependency of initialization.
*/

#include <string.h>

union memBlkHdrUn;

typedef struct MemoryZoneStr {
    union memBlkHdrUn    *head;         /* free list */
    pthread_mutex_t       lock;
    size_t                blockSize;    /* size of blocks on this free list */
    PRUint32              locked;       /* current state of lock */
    PRUint32              contention;   /* counter: had to wait for lock */
    PRUint32              hits;         /* allocated from free list */
    PRUint32              misses;       /* had to call malloc */
    PRUint32              elements;     /* on free list */
} MemoryZone;

typedef union memBlkHdrUn {
    unsigned char filler[48];  /* fix the size of this beast */
    struct memBlkHdrStr {
        union memBlkHdrUn    *next;
        MemoryZone           *zone;
        size_t                blockSize;
        size_t                requestedSize;
        PRUint32              magic;
    } s;
} MemBlockHdr;

#define MEM_ZONES     7
#define THREAD_POOLS 11  /* prime number for modulus */
#define ZONE_MAGIC  0x0BADC0DE

static MemoryZone zones[MEM_ZONES][THREAD_POOLS];

static PRBool use_zone_allocator = PR_FALSE;

static void pr_ZoneFree(void *ptr);

void
_PR_DestroyZones(void)
{
    int i, j;

    if (!use_zone_allocator)
        return;

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone *mz = &zones[i][j];
            pthread_mutex_destroy(&mz->lock);
            while (mz->head) {
                MemBlockHdr *hdr = mz->head;
                mz->head = hdr->s.next;  /* unlink it */
#ifdef VBOX_USE_IPRT_IN_NSPR
                RTMemFree(hdr);
#else
                free(hdr);
#endif
                mz->elements--;
            }
        }
    }
    use_zone_allocator = PR_FALSE;
}

/*
** pr_FindSymbolInProg
**
** Find the specified data symbol in the program and return
** its address.
*/

#ifdef USE_DLFCN

#include <dlfcn.h>

static void *
pr_FindSymbolInProg(const char *name)
{
    void *h;
    void *sym;

    h = dlopen(0, RTLD_LAZY);
    if (h == NULL)
        return NULL;
    sym = dlsym(h, name);
    (void)dlclose(h);
    return sym;
}

#elif defined(USE_HPSHL)

#include <dl.h>

static void *
pr_FindSymbolInProg(const char *name)
{
    shl_t h = NULL;
    void *sym;

    if (shl_findsym(&h, name, TYPE_DATA, &sym) == -1)
        return NULL;
    return sym;
}

#elif defined(USE_MACH_DYLD)

static void *
pr_FindSymbolInProg(const char *name)
{
    /* FIXME: not implemented */
    return NULL;
}

#else

#error "The zone allocator is not supported on this platform"

#endif

void
_PR_InitZones(void)
{
    int i, j;
    char *envp;
    PRBool *sym;

    if ((sym = (PRBool *)pr_FindSymbolInProg("nspr_use_zone_allocator")) != NULL) {
        use_zone_allocator = *sym;
    } else if ((envp = getenv("NSPR_USE_ZONE_ALLOCATOR")) != NULL) {
        use_zone_allocator = (atoi(envp) == 1);
    }

    if (!use_zone_allocator)
        return;

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone *mz = &zones[i][j];
            int rv = pthread_mutex_init(&mz->lock, NULL);
            PR_ASSERT(0 == rv);
            if (rv != 0) {
                goto loser;
            }
            mz->blockSize = 16 << ( 2 * i);
        }
    }
    return;

loser:
    _PR_DestroyZones();
    return;
}

PR_IMPLEMENT(void)
PR_FPrintZoneStats(PRFileDesc *debug_out)
{
    int i, j;

    for (j = 0; j < THREAD_POOLS; j++) {
        for (i = 0; i < MEM_ZONES; i++) {
            MemoryZone   *mz   = &zones[i][j];
            MemoryZone    zone = *mz;
            if (zone.elements || zone.misses || zone.hits) {
                PR_fprintf(debug_out,
"pool: %d, zone: %d, size: %d, free: %d, hit: %d, miss: %d, contend: %d\n",
                    j, i, zone.blockSize, zone.elements,
                    zone.hits, zone.misses, zone.contention);
            }
	}
    }
}

static void *
pr_ZoneMalloc(PRUint32 size)
{
    void         *rv;
    unsigned int  zone;
    size_t        blockSize;
    MemBlockHdr  *mb, *mt;
    MemoryZone   *mz;

    /* Always allocate a non-zero amount of bytes */
    if (size < 1) {
        size = 1;
    }
    for (zone = 0, blockSize = 16; zone < MEM_ZONES; ++zone, blockSize <<= 2) {
        if (size <= blockSize) {
            break;
        }
    }
    if (zone < MEM_ZONES) {
        pthread_t me = pthread_self();
        unsigned int pool = (PRUptrdiff)me % THREAD_POOLS;
        PRUint32     wasLocked;
        mz = &zones[zone][pool];
        wasLocked = mz->locked;
        pthread_mutex_lock(&mz->lock);
        mz->locked = 1;
        if (wasLocked)
            mz->contention++;
        if (mz->head) {
            mb = mz->head;
            PR_ASSERT(mb->s.magic == ZONE_MAGIC);
            PR_ASSERT(mb->s.zone  == mz);
            PR_ASSERT(mb->s.blockSize == blockSize);
            PR_ASSERT(mz->blockSize == blockSize);

            mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
            PR_ASSERT(mt->s.magic == ZONE_MAGIC);
            PR_ASSERT(mt->s.zone  == mz);
            PR_ASSERT(mt->s.blockSize == blockSize);

            mz->hits++;
            mz->elements--;
            mz->head = mb->s.next;    /* take off free list */
            mz->locked = 0;
            pthread_mutex_unlock(&mz->lock);

            mt->s.next          = mb->s.next          = NULL;
            mt->s.requestedSize = mb->s.requestedSize = size;

            rv = (void *)(mb + 1);
            return rv;
        }

        mz->misses++;
        mz->locked = 0;
        pthread_mutex_unlock(&mz->lock);

#ifdef VBOX_USE_IPRT_IN_NSPR
        mb = (MemBlockHdr *)RTMemAlloc(blockSize + 2 * (sizeof *mb));
#else
        mb = (MemBlockHdr *)malloc(blockSize + 2 * (sizeof *mb));
#endif
        if (!mb) {
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return NULL;
        }
        mb->s.next          = NULL;
        mb->s.zone          = mz;
        mb->s.magic         = ZONE_MAGIC;
        mb->s.blockSize     = blockSize;
        mb->s.requestedSize = size;

        mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
        memcpy(mt, mb, sizeof *mb);

        rv = (void *)(mb + 1);
        return rv;
    }

    /* size was too big.  Create a block with no zone */
    blockSize = (size & 15) ? size + 16 - (size & 15) : size;
#ifdef VBOX_USE_IPRT_IN_NSPR
    mb = (MemBlockHdr *)RTMemAlloc(blockSize + 2 * (sizeof *mb));
#else
    mb = (MemBlockHdr *)malloc(blockSize + 2 * (sizeof *mb));
#endif
    if (!mb) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }
    mb->s.next          = NULL;
    mb->s.zone          = NULL;
    mb->s.magic         = ZONE_MAGIC;
    mb->s.blockSize     = blockSize;
    mb->s.requestedSize = size;

    mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
    memcpy(mt, mb, sizeof *mb);

    rv = (void *)(mb + 1);
    return rv;
}


static void *
pr_ZoneCalloc(PRUint32 nelem, PRUint32 elsize)
{
    PRUint32 size = nelem * elsize;
    void *p = pr_ZoneMalloc(size);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

static void *
pr_ZoneRealloc(void *oldptr, PRUint32 bytes)
{
    void         *rv;
    MemBlockHdr  *mb;
    int           ours;
    MemBlockHdr   phony;

    if (!oldptr)
        return pr_ZoneMalloc(bytes);
    mb = (MemBlockHdr *)((char *)oldptr - (sizeof *mb));
    PR_ASSERT(mb->s.magic == ZONE_MAGIC);
    if (mb->s.magic != ZONE_MAGIC) {
        /* Maybe this just came from ordinary malloc */
#ifdef DEBUG
        fprintf(stderr,
            "Warning: reallocing memory block %p from ordinary malloc\n",
            oldptr);
#endif
        /* We don't know how big it is.  But we can fix that. */
#ifdef VBOX_USE_IPRT_IN_NSPR
        oldptr = RTMemRealloc(oldptr, bytes);
#else
        oldptr = realloc(oldptr, bytes);
#endif
        if (!oldptr) {
            if (bytes) {
                PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                return oldptr;
            }
        }
        phony.s.requestedSize = bytes;
        mb = &phony;
        ours = 0;
    } else {
        size_t blockSize = mb->s.blockSize;
        MemBlockHdr *mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);

        PR_ASSERT(mt->s.magic == ZONE_MAGIC);
        PR_ASSERT(mt->s.zone  == mb->s.zone);
        PR_ASSERT(mt->s.blockSize == blockSize);

        if (bytes <= blockSize) {
            /* The block is already big enough. */
            mt->s.requestedSize = mb->s.requestedSize = bytes;
            return oldptr;
        }
        ours = 1;
    }

    rv = pr_ZoneMalloc(bytes);
    if (rv) {
        if (oldptr && mb->s.requestedSize)
            memcpy(rv, oldptr, mb->s.requestedSize);
        if (ours)
            pr_ZoneFree(oldptr);
        else if (oldptr)
#ifdef VBOX_USE_IPRT_IN_NSPR
            RTMemFree(oldptr);
#else
            free(oldptr);
#endif
    }
    return rv;
}

static void
pr_ZoneFree(void *ptr)
{
    MemBlockHdr  *mb, *mt;
    MemoryZone   *mz;
    size_t        blockSize;
    PRUint32      wasLocked;

    if (!ptr)
        return;

    mb = (MemBlockHdr *)((char *)ptr - (sizeof *mb));

    if (mb->s.magic != ZONE_MAGIC) {
        /* maybe this came from ordinary malloc */
#ifdef DEBUG
        fprintf(stderr,
            "Warning: freeing memory block %p from ordinary malloc\n", ptr);
#endif
#ifdef VBOX_USE_IPRT_IN_NSPR
        RTMemFree(ptr);
#else
        free(ptr);
#endif
        return;
    }

    blockSize = mb->s.blockSize;
    mz        = mb->s.zone;
    mt = (MemBlockHdr *)(((char *)(mb + 1)) + blockSize);
    PR_ASSERT(mt->s.magic == ZONE_MAGIC);
    PR_ASSERT(mt->s.zone  == mz);
    PR_ASSERT(mt->s.blockSize == blockSize);
    if (!mz) {
        PR_ASSERT(blockSize > 65536);
        /* This block was not in any zone.  Just free it. */
#ifdef VBOX_USE_IPRT_IN_NSPR
        RTMemFree(mb);
#else
        free(mb);
#endif
        return;
    }
    PR_ASSERT(mz->blockSize == blockSize);
    wasLocked = mz->locked;
    pthread_mutex_lock(&mz->lock);
    mz->locked = 1;
    if (wasLocked)
        mz->contention++;
    mt->s.next = mb->s.next = mz->head;        /* put on head of list */
    mz->head = mb;
    mz->elements++;
    mz->locked = 0;
    pthread_mutex_unlock(&mz->lock);
}

PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

#ifdef VBOX_USE_IPRT_IN_NSPR
    return use_zone_allocator ? pr_ZoneMalloc(size) : RTMemAlloc(RT_MAX(size, 1));
#else
    return use_zone_allocator ? pr_ZoneMalloc(size) : malloc(size);
#endif
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

    return use_zone_allocator ?
#ifdef VBOX_USE_IPRT_IN_NSPR
        pr_ZoneCalloc(nelem, elsize) : RTMemAllocZ(RT_MAX(nelem * (size_t)elsize, 1));
#else
        pr_ZoneCalloc(nelem, elsize) : calloc(nelem, elsize);
#endif
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
    if (!_pr_initialized) _PR_ImplicitInitialization();

#ifdef VBOX_USE_IPRT_IN_NSPR
    return use_zone_allocator ? pr_ZoneRealloc(ptr, size) : RTMemRealloc(ptr, size);
#else
    return use_zone_allocator ? pr_ZoneRealloc(ptr, size) : realloc(ptr, size);
#endif
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
    if (use_zone_allocator)
        pr_ZoneFree(ptr);
    else
#ifdef VBOX_USE_IPRT_IN_NSPR
        RTMemFree(ptr);
#else
        free(ptr);
#endif
}

#else /* !defined(_PR_ZONE_ALLOCATOR) */

/*
** The PR_Malloc, PR_Calloc, PR_Realloc, and PR_Free functions simply
** call their libc equivalents now.  This may seem redundant, but it
** ensures that we are calling into the same runtime library.  On
** Win32, it is possible to have multiple runtime libraries (e.g.,
** objects compiled with /MD and /MDd) in the same process, and
** they maintain separate heaps, which cannot be mixed.
*/
PR_IMPLEMENT(void *) PR_Malloc(PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_malloc( (size_t) size);
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemAlloc(RT_MAX(size, 1));
# else
    return malloc(size);
# endif
#endif
}

PR_IMPLEMENT(void *) PR_Calloc(PRUint32 nelem, PRUint32 elsize)
{
#if defined (WIN16)
    return PR_MD_calloc( (size_t)nelem, (size_t)elsize );

#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemAllocZ(RT_MAX(nelem * (size_t)elsize, 1));
# else
    return calloc(nelem, elsize);
# endif
#endif
}

PR_IMPLEMENT(void *) PR_Realloc(void *ptr, PRUint32 size)
{
#if defined (WIN16)
    return PR_MD_realloc( ptr, (size_t) size);
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    return RTMemRealloc(ptr, size);
# else
    return realloc(ptr, size);
# endif
#endif
}

PR_IMPLEMENT(void) PR_Free(void *ptr)
{
#if defined (WIN16)
    PR_MD_free( ptr );
#else
# ifdef VBOX_USE_IPRT_IN_NSPR
    RTMemFree(ptr);
# else
    free(ptr);
# endif
#endif
}

#endif /* _PR_ZONE_ALLOCATOR */

