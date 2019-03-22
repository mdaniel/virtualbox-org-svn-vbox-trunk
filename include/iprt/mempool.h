/** @file
 * IPRT - Memory Allocation Pool.
 */

/*
 * Copyright (C) 2009-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_mempool_h
#define ___iprt_mempool_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/**
 * Creates a new memory pool.
 *
 * @returns IPRT status code.
 *
 * @param   phMemPool       Where to return the handle to the new memory
 *                          pool.
 * @param   pszName         The name of the pool (for debug purposes).
 */
RTDECL(int) RTMemPoolCreate(PRTMEMPOOL phMemPool, const char *pszName);

/**
 * Destroys the specified pool, freeing all the memory it contains.
 *
 * @returns IPRT status code.
 *
 * @param   hMemPool        The handle to the pool. The nil handle and
 *                          RTMEMPOOL_DEFAULT are quietly ignored (retval
 *                          VINF_SUCCESS).
 */
RTDECL(int) RTMemPoolDestroy(RTMEMPOOL hMemPool);

/**
 * Allocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 *
 * @param   hMemPool        Handle to the pool to allocate the memory from.
 * @param   cb              Size in bytes of the memory block to allocated.
 */
RTDECL(void *) RTMemPoolAlloc(RTMEMPOOL hMemPool, size_t cb) RT_NO_THROW_PROTO;

/**
 * Allocates zero'd memory.
 *
 * Instead of memset(pv, 0, sizeof()) use this when you want zero'd
 * memory. This keeps the code smaller and the heap can skip the memset
 * in about 0.42% of calls :-).
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 *
 * @param   hMemPool        Handle to the pool to allocate the memory from.
 * @param   cb              Size in bytes of the memory block to allocated.
 */
RTDECL(void *) RTMemPoolAllocZ(RTMEMPOOL hMemPool, size_t cb) RT_NO_THROW_PROTO;

/**
 * Duplicates a chunk of memory into a new heap block.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 *
 * @param   hMemPool        Handle to the pool to allocate the memory from.
 * @param   pvSrc           The memory to duplicate.
 * @param   cb              The amount of memory to duplicate.
 */
RTDECL(void *) RTMemPoolDup(RTMEMPOOL hMemPool, const void *pvSrc, size_t cb) RT_NO_THROW_PROTO;

/**
 * Duplicates a chunk of memory into a new heap block with some
 * additional zeroed memory.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 *
 * @param   hMemPool        Handle to the pool to allocate the memory from.
 * @param   pvSrc           The memory to duplicate.
 * @param   cbSrc           The amount of memory to duplicate.
 * @param   cbExtra         The amount of extra memory to allocate and zero.
 */
RTDECL(void *) RTMemPoolDupEx(RTMEMPOOL hMemPool, const void *pvSrc, size_t cbSrc, size_t cbExtra) RT_NO_THROW_PROTO;

/**
 * Reallocates memory.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 *
 * @param   hMemPool        Handle to the pool containing the old memory.
 * @param   pvOld           The memory block to reallocate.
 * @param   cbNew           The new block size (in bytes).
 */
RTDECL(void *) RTMemPoolRealloc(RTMEMPOOL hMemPool, void *pvOld, size_t cbNew) RT_NO_THROW_PROTO;

/**
 * Frees memory allocated from a pool.
 *
 * @param   hMemPool        Handle to the pool containing the memory.  Passing
 *                          NIL here is fine, but it may come at a slight
 *                          performance cost.
 * @param   pv              Pointer to memory block.
 *
 * @remarks This is the same a RTMemPoolRelease but included here as a separate
 *          function to simplify code migration.
 */
RTDECL(void) RTMemPoolFree(RTMEMPOOL hMemPool, void *pv) RT_NO_THROW_PROTO;

/**
 * Retains a reference to a memory block in a pool.
 *
 * @returns New reference count, UINT32_MAX on error (asserted).
 *
 * @param   pv              Pointer to memory block.
 */
RTDECL(uint32_t) RTMemPoolRetain(void *pv) RT_NO_THROW_PROTO;

/**
 * Releases a reference to a memory block in a pool.
 *
 * @returns New reference count, UINT32_MAX on error (asserted).
 *
 * @param   hMemPool        Handle to the pool containing the memory.  Passing
 *                          NIL here is fine, but it may come at a slight
 *                          performance cost.
 * @param   pv              Pointer to memory block.
 */
RTDECL(uint32_t) RTMemPoolRelease(RTMEMPOOL hMemPool, void *pv) RT_NO_THROW_PROTO;

/**
 * Get the current reference count.
 *
 * @returns The reference count, UINT32_MAX on error (asserted).
 * @param   pv              Pointer to memory block.
 */
RTDECL(uint32_t) RTMemPoolRefCount(void *pv) RT_NO_THROW_PROTO;


RT_C_DECLS_END

#endif

