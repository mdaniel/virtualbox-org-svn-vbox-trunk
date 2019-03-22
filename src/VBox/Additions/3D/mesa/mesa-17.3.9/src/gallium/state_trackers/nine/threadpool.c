/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "swapchain9.h"
#include "surface9.h"
#include "device9.h"

#include "nine_helpers.h"
#include "nine_pipe.h"
#include "nine_dump.h"

#include "util/u_inlines.h"
#include "util/u_surface.h"
#include "hud/hud_context.h"
#include "state_tracker/drm_driver.h"

#include "os/os_thread.h"
#include "threadpool.h"

#ifndef VBOX_WITH_MESA3D_D3D_THREADPOOL
static void *
threadpool_worker(void *data)
{
    struct threadpool *pool = data;

    pthread_mutex_lock(&pool->m);

    while (!pool->shutdown) {
        struct threadpool_task *task;

        /* Block (dropping the lock) until new work arrives for us. */
        while (!pool->workqueue && !pool->shutdown)
            pthread_cond_wait(&pool->new_work, &pool->m);

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->m);
            return NULL;
        }

        /* Pull the first task from the list.  We don't free it -- it now lacks
         * a reference other than the worker creator's, whose responsibility it
         * is to call threadpool_wait_for_work() to free it.
         */
        task = pool->workqueue;
        pool->workqueue = task->next;

        /* Call the task's work func. */
        pthread_mutex_unlock(&pool->m);
        task->work(task->data);
        pthread_mutex_lock(&pool->m);
        task->finished = TRUE;
        pthread_cond_broadcast(&task->finish);
    }

    pthread_mutex_unlock(&pool->m);

    return NULL;
}

struct threadpool *
_mesa_threadpool_create(struct NineSwapChain9 *swapchain)
{
    struct threadpool *pool = calloc(1, sizeof(*pool));

    if (!pool)
        return NULL;

    pthread_mutex_init(&pool->m, NULL);
    pthread_cond_init(&pool->new_work, NULL);

    pool->wthread = NineSwapChain9_CreateThread(swapchain, threadpool_worker, pool);
    if (!pool->wthread) {
        /* using pthread as fallback */
        pthread_create(&pool->pthread, NULL, threadpool_worker, pool);
    }
    return pool;
}

void
_mesa_threadpool_destroy(struct NineSwapChain9 *swapchain, struct threadpool *pool)
{
    if (!pool)
        return;

    pthread_mutex_lock(&pool->m);
    pool->shutdown = TRUE;
    pthread_cond_broadcast(&pool->new_work);
    pthread_mutex_unlock(&pool->m);

    if (pool->wthread) {
        NineSwapChain9_WaitForThread(swapchain, pool->wthread);
    } else {
        pthread_join(pool->pthread, NULL);
    }

    pthread_cond_destroy(&pool->new_work);
    pthread_mutex_destroy(&pool->m);
    free(pool);
}

/**
 * Queues a request for the work function to be asynchronously executed by the
 * thread pool.
 *
 * The work func will get the "data" argument as its parameter -- any
 * communication between the caller and the work function will occur through
 * that.
 *
 * If there is an error, the work function is called immediately and NULL is
 * returned.
 */
struct threadpool_task *
_mesa_threadpool_queue_task(struct threadpool *pool,
                            threadpool_task_func work, void *data)
{
    struct threadpool_task *task, *previous;

    if (!pool) {
        work(data);
        return NULL;
    }

    task = calloc(1, sizeof(*task));
    if (!task) {
        work(data);
        return NULL;
    }

    task->work = work;
    task->data = data;
    task->next = NULL;
    pthread_cond_init(&task->finish, NULL);

    pthread_mutex_lock(&pool->m);

    if (!pool->workqueue) {
        pool->workqueue = task;
    } else {
        previous = pool->workqueue;
        while (previous && previous->next)
            previous = previous->next;

        previous->next = task;
    }
    pthread_cond_signal(&pool->new_work);
    pthread_mutex_unlock(&pool->m);

    return task;
}

/**
 * Blocks on the completion of the given task and frees the task.
 */
void
_mesa_threadpool_wait_for_task(struct threadpool *pool,
                               struct threadpool_task **task_handle)
{
    struct threadpool_task *task = *task_handle;

    if (!pool || !task)
        return;

    pthread_mutex_lock(&pool->m);
    while (!task->finished)
        pthread_cond_wait(&task->finish, &pool->m);
    pthread_mutex_unlock(&pool->m);

    pthread_cond_destroy(&task->finish);
    free(task);
    *task_handle = NULL;
}
#else /* VBOX_WITH_MESA3D_D3D_THREADPOOL */
struct threadpool *
_mesa_threadpool_create(struct NineSwapChain9 *swapchain)
{
    return NULL;
}
void
_mesa_threadpool_destroy(struct NineSwapChain9 *swapchain, struct threadpool *pool)
{
}
struct threadpool_task *
_mesa_threadpool_queue_task(struct threadpool *pool,
                            threadpool_task_func work, void *data)
{
    return NULL;
}
void
_mesa_threadpool_wait_for_task(struct threadpool *pool,
                               struct threadpool_task **task_handle)
{
}
#endif /* VBOX_WITH_MESA3D_D3D_THREADPOOL */
