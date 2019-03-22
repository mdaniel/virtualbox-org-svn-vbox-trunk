/* $Id$ */
/** @file
 * Lock.h - Haiku, private locking internals.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * VirtualBox Guest Additions for Haiku.
 *
 * Copyright 2008-2010, Ingo Weinhold, ingo_weinhold@gmx.de.
 * Copyright 2002-2009, Axel D�rfler, axeld@pinc-software.de.
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/** @todo r=ramshankar: Eventually this file should be shipped by Haiku and
 *        should be removed from the VBox tree. */

#ifndef GA_INCLUDED_HAIKU_lock_h
#define GA_INCLUDED_HAIKU_lock_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <OS.h>


struct mutex_waiter;

typedef struct mutex {
        const char*                             name;
        struct mutex_waiter*    waiters;
#if KDEBUG
        thread_id                               holder;
#else
        int32                                   count;
        uint16                                  ignore_unlock_count;
#endif
        uint8                                   flags;
} mutex;

#define MUTEX_FLAG_CLONE_NAME   0x1


typedef struct recursive_lock {
        mutex           lock;
#if !KDEBUG
        thread_id       holder;
#endif
        int                     recursion;
} recursive_lock;


struct rw_lock_waiter;

typedef struct rw_lock {
        const char*                             name;
        struct rw_lock_waiter*  waiters;
        thread_id                               holder;
        vint32                                  count;
        int32                                   owner_count;
        int16                                   active_readers;
                                                                // Only > 0 while a writer is waiting: number
                                                                // of active readers when the first waiting
                                                                // writer started waiting.
        int16                                   pending_readers;
                                                                // Number of readers that have already
                                                                // incremented "count", but have not yet started
                                                                // to wait at the time the last writer unlocked.
        uint32                                  flags;
} rw_lock;

#define RW_LOCK_WRITER_COUNT_BASE       0x10000

#define RW_LOCK_FLAG_CLONE_NAME 0x1


#if KDEBUG
#       define KDEBUG_RW_LOCK_DEBUG 0
                // Define to 1 if you want to use ASSERT_READ_LOCKED_RW_LOCK().
                // The rw_lock will just behave like a recursive locker then.
#       define ASSERT_LOCKED_RECURSIVE(r) \
                { ASSERT(find_thread(NULL) == (r)->lock.holder); }
#       define ASSERT_LOCKED_MUTEX(m) { ASSERT(find_thread(NULL) == (m)->holder); }
#       define ASSERT_WRITE_LOCKED_RW_LOCK(l) \
                { ASSERT(find_thread(NULL) == (l)->holder); }
#       if KDEBUG_RW_LOCK_DEBUG
#               define ASSERT_READ_LOCKED_RW_LOCK(l) \
                        { ASSERT(find_thread(NULL) == (l)->holder); }
#       else
#               define ASSERT_READ_LOCKED_RW_LOCK(l) do {} while (false)
#       endif
#else
#       define ASSERT_LOCKED_RECURSIVE(r)               do {} while (false)
#       define ASSERT_LOCKED_MUTEX(m)                   do {} while (false)
#       define ASSERT_WRITE_LOCKED_RW_LOCK(m)   do {} while (false)
#       define ASSERT_READ_LOCKED_RW_LOCK(l)    do {} while (false)
#endif


// static initializers
#if KDEBUG
#       define MUTEX_INITIALIZER(name)                  { name, NULL, -1, 0 }
#       define RECURSIVE_LOCK_INITIALIZER(name) { MUTEX_INITIALIZER(name), 0 }
#else
#       define MUTEX_INITIALIZER(name)                  { name, NULL, 0, 0, 0 }
#       define RECURSIVE_LOCK_INITIALIZER(name) { MUTEX_INITIALIZER(name), -1, 0 }
#endif

#define RW_LOCK_INITIALIZER(name)                       { name, NULL, -1, 0, 0, 0 }


#if KDEBUG
#       define RECURSIVE_LOCK_HOLDER(recursiveLock)     ((recursiveLock)->lock.holder)
#else
#       define RECURSIVE_LOCK_HOLDER(recursiveLock)     ((recursiveLock)->holder)
#endif


#ifdef __cplusplus
extern "C" {
#endif

extern void     recursive_lock_init(recursive_lock *lock, const char *name);
        // name is *not* cloned nor freed in recursive_lock_destroy()
extern void recursive_lock_init_etc(recursive_lock *lock, const char *name,
        uint32 flags);
extern void recursive_lock_destroy(recursive_lock *lock);
extern status_t recursive_lock_lock(recursive_lock *lock);
extern status_t recursive_lock_trylock(recursive_lock *lock);
extern void recursive_lock_unlock(recursive_lock *lock);
extern int32 recursive_lock_get_recursion(recursive_lock *lock);

extern void rw_lock_init(rw_lock* lock, const char* name);
        // name is *not* cloned nor freed in rw_lock_destroy()
extern void rw_lock_init_etc(rw_lock* lock, const char* name, uint32 flags);
extern void rw_lock_destroy(rw_lock* lock);
extern status_t rw_lock_write_lock(rw_lock* lock);

extern void mutex_init(mutex* lock, const char* name);
        // name is *not* cloned nor freed in mutex_destroy()
extern void mutex_init_etc(mutex* lock, const char* name, uint32 flags);
extern void mutex_destroy(mutex* lock);
extern status_t mutex_switch_lock(mutex* from, mutex* to);
        // Unlocks "from" and locks "to" such that unlocking and starting to wait
        // for the lock is atomically. I.e. if "from" guards the object "to" belongs
        // to, the operation is safe as long as "from" is held while destroying
        // "to".
extern status_t mutex_switch_from_read_lock(rw_lock* from, mutex* to);
        // Like mutex_switch_lock(), just for a switching from a read-locked
        // rw_lock.


// implementation private:

extern status_t _rw_lock_read_lock(rw_lock* lock);
extern status_t _rw_lock_read_lock_with_timeout(rw_lock* lock,
        uint32 timeoutFlags, bigtime_t timeout);
extern void _rw_lock_read_unlock(rw_lock* lock, bool threadsLocked);
extern void _rw_lock_write_unlock(rw_lock* lock, bool threadsLocked);

extern status_t _mutex_lock(mutex* lock, bool threadsLocked);
extern void _mutex_unlock(mutex* lock, bool threadsLocked);
extern status_t _mutex_trylock(mutex* lock);
extern status_t _mutex_lock_with_timeout(mutex* lock, uint32 timeoutFlags,
        bigtime_t timeout);


static inline status_t
rw_lock_read_lock(rw_lock* lock)
{
#if KDEBUG_RW_LOCK_DEBUG
        return rw_lock_write_lock(lock);
#else
        int32 oldCount = atomic_add(&lock->count, 1);
        if (oldCount >= RW_LOCK_WRITER_COUNT_BASE)
                return _rw_lock_read_lock(lock);
        return B_OK;
#endif
}


static inline status_t
rw_lock_read_lock_with_timeout(rw_lock* lock, uint32 timeoutFlags,
        bigtime_t timeout)
{
#if KDEBUG_RW_LOCK_DEBUG
        return mutex_lock_with_timeout(lock, timeoutFlags, timeout);
#else
        int32 oldCount = atomic_add(&lock->count, 1);
        if (oldCount >= RW_LOCK_WRITER_COUNT_BASE)
                return _rw_lock_read_lock_with_timeout(lock, timeoutFlags, timeout);
        return B_OK;
#endif
}


static inline void
rw_lock_read_unlock(rw_lock* lock)
{
#if KDEBUG_RW_LOCK_DEBUG
        rw_lock_write_unlock(lock);
#else
        int32 oldCount = atomic_add(&lock->count, -1);
        if (oldCount >= RW_LOCK_WRITER_COUNT_BASE)
                _rw_lock_read_unlock(lock, false);
#endif
}


static inline void
rw_lock_write_unlock(rw_lock* lock)
{
        _rw_lock_write_unlock(lock, false);
}


static inline status_t
mutex_lock(mutex* lock)
{
#if KDEBUG
        return _mutex_lock(lock, false);
#else
        if (atomic_add(&lock->count, -1) < 0)
                return _mutex_lock(lock, false);
        return B_OK;
#endif
}


static inline status_t
mutex_lock_threads_locked(mutex* lock)
{
#if KDEBUG
        return _mutex_lock(lock, true);
#else
        if (atomic_add(&lock->count, -1) < 0)
                return _mutex_lock(lock, true);
        return B_OK;
#endif
}


static inline status_t
mutex_trylock(mutex* lock)
{
#if KDEBUG
        return _mutex_trylock(lock);
#else
        if (atomic_test_and_set(&lock->count, -1, 0) != 0)
                return B_WOULD_BLOCK;
        return B_OK;
#endif
}


static inline status_t
mutex_lock_with_timeout(mutex* lock, uint32 timeoutFlags, bigtime_t timeout)
{
#if KDEBUG
        return _mutex_lock_with_timeout(lock, timeoutFlags, timeout);
#else
        if (atomic_add(&lock->count, -1) < 0)
                return _mutex_lock_with_timeout(lock, timeoutFlags, timeout);
        return B_OK;
#endif
}


static inline void
mutex_unlock(mutex* lock)
{
#if !KDEBUG
        if (atomic_add(&lock->count, 1) < -1)
#endif
                _mutex_unlock(lock, false);
}


static inline void
mutex_transfer_lock(mutex* lock, thread_id thread)
{
#if KDEBUG
        lock->holder = thread;
#endif
}


extern void lock_debug_init();

#ifdef __cplusplus
}
#endif

#endif /* !GA_INCLUDED_HAIKU_lock_h */
