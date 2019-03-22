/*
 * Copyright © 2016 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

#include "u_queue.h"
#include "util/u_string.h"

static void util_queue_killall_and_wait(struct util_queue *queue);

/****************************************************************************
 * Wait for all queues to assert idle when exit() is called.
 *
 * Otherwise, C++ static variable destructors can be called while threads
 * are using the static variables.
 */

static once_flag atexit_once_flag = ONCE_FLAG_INIT;
static struct list_head queue_list;
static mtx_t exit_mutex = _MTX_INITIALIZER_NP;

static void
atexit_handler(void)
{
   struct util_queue *iter;

   mtx_lock(&exit_mutex);
   /* Wait for all queues to assert idle. */
   LIST_FOR_EACH_ENTRY(iter, &queue_list, head) {
      util_queue_killall_and_wait(iter);
   }
   mtx_unlock(&exit_mutex);
}

static void
global_init(void)
{
   LIST_INITHEAD(&queue_list);
   atexit(atexit_handler);
}

static void
add_to_atexit_list(struct util_queue *queue)
{
   call_once(&atexit_once_flag, global_init);

   mtx_lock(&exit_mutex);
   LIST_ADD(&queue->head, &queue_list);
   mtx_unlock(&exit_mutex);
}

static void
remove_from_atexit_list(struct util_queue *queue)
{
   struct util_queue *iter, *tmp;

   mtx_lock(&exit_mutex);
   LIST_FOR_EACH_ENTRY_SAFE(iter, tmp, &queue_list, head) {
      if (iter == queue) {
         LIST_DEL(&iter->head);
         break;
      }
   }
   mtx_unlock(&exit_mutex);
}

/****************************************************************************
 * util_queue_fence
 */

static void
util_queue_fence_signal(struct util_queue_fence *fence)
{
   mtx_lock(&fence->mutex);
   fence->signalled = true;
   cnd_broadcast(&fence->cond);
   mtx_unlock(&fence->mutex);
}

void
util_queue_fence_wait(struct util_queue_fence *fence)
{
   mtx_lock(&fence->mutex);
   while (!fence->signalled)
      cnd_wait(&fence->cond, &fence->mutex);
   mtx_unlock(&fence->mutex);
}

void
util_queue_fence_init(struct util_queue_fence *fence)
{
   memset(fence, 0, sizeof(*fence));
   (void) mtx_init(&fence->mutex, mtx_plain);
   cnd_init(&fence->cond);
   fence->signalled = true;
}

void
util_queue_fence_destroy(struct util_queue_fence *fence)
{
   assert(fence->signalled);

   /* Ensure that another thread is not in the middle of
    * util_queue_fence_signal (having set the fence to signalled but still
    * holding the fence mutex).
    *
    * A common contract between threads is that as soon as a fence is signalled
    * by thread A, thread B is allowed to destroy it. Since
    * util_queue_fence_is_signalled does not lock the fence mutex (for
    * performance reasons), we must do so here.
    */
   mtx_lock(&fence->mutex);
   mtx_unlock(&fence->mutex);

   cnd_destroy(&fence->cond);
   mtx_destroy(&fence->mutex);
}

/****************************************************************************
 * util_queue implementation
 */

struct thread_input {
   struct util_queue *queue;
   int thread_index;
};

static int
util_queue_thread_func(void *input)
{
   struct util_queue *queue = ((struct thread_input*)input)->queue;
   int thread_index = ((struct thread_input*)input)->thread_index;

   free(input);

   if (queue->name) {
      char name[16];
      util_snprintf(name, sizeof(name), "%s:%i", queue->name, thread_index);
      u_thread_setname(name);
   }

   while (1) {
      struct util_queue_job job;

      mtx_lock(&queue->lock);
      assert(queue->num_queued >= 0 && queue->num_queued <= queue->max_jobs);

      /* wait if the queue is empty */
      while (!queue->kill_threads && queue->num_queued == 0)
         cnd_wait(&queue->has_queued_cond, &queue->lock);

      if (queue->kill_threads) {
         mtx_unlock(&queue->lock);
         break;
      }

      job = queue->jobs[queue->read_idx];
      memset(&queue->jobs[queue->read_idx], 0, sizeof(struct util_queue_job));
      queue->read_idx = (queue->read_idx + 1) % queue->max_jobs;

      queue->num_queued--;
      cnd_signal(&queue->has_space_cond);
      mtx_unlock(&queue->lock);

      if (job.job) {
         job.execute(job.job, thread_index);
         util_queue_fence_signal(job.fence);
         if (job.cleanup)
            job.cleanup(job.job, thread_index);
      }
   }

   /* signal remaining jobs before terminating */
   mtx_lock(&queue->lock);
   for (unsigned i = queue->read_idx; i != queue->write_idx;
        i = (i + 1) % queue->max_jobs) {
      if (queue->jobs[i].job) {
         util_queue_fence_signal(queue->jobs[i].fence);
         queue->jobs[i].job = NULL;
      }
   }
   queue->read_idx = queue->write_idx;
   queue->num_queued = 0;
   mtx_unlock(&queue->lock);
   return 0;
}

bool
util_queue_init(struct util_queue *queue,
                const char *name,
                unsigned max_jobs,
                unsigned num_threads,
                unsigned flags)
{
   unsigned i;

   memset(queue, 0, sizeof(*queue));
   queue->name = name;
   queue->flags = flags;
   queue->num_threads = num_threads;
   queue->max_jobs = max_jobs;

   queue->jobs = (struct util_queue_job*)
                 calloc(max_jobs, sizeof(struct util_queue_job));
   if (!queue->jobs)
      goto fail;

   (void) mtx_init(&queue->lock, mtx_plain);

   queue->num_queued = 0;
   cnd_init(&queue->has_queued_cond);
   cnd_init(&queue->has_space_cond);

   queue->threads = (thrd_t*) calloc(num_threads, sizeof(thrd_t));
   if (!queue->threads)
      goto fail;

   /* start threads */
   for (i = 0; i < num_threads; i++) {
      struct thread_input *input =
         (struct thread_input *) malloc(sizeof(struct thread_input));
      input->queue = queue;
      input->thread_index = i;

      queue->threads[i] = u_thread_create(util_queue_thread_func, input);

      if (!queue->threads[i]) {
         free(input);

         if (i == 0) {
            /* no threads created, fail */
            goto fail;
         } else {
            /* at least one thread created, so use it */
            queue->num_threads = i;
            break;
         }
      }

      if (flags & UTIL_QUEUE_INIT_USE_MINIMUM_PRIORITY) {
   #if defined(__linux__) && defined(SCHED_IDLE)
         struct sched_param sched_param = {0};

         /* The nice() function can only set a maximum of 19.
          * SCHED_IDLE is the same as nice = 20.
          *
          * Note that Linux only allows decreasing the priority. The original
          * priority can't be restored.
          */
         pthread_setschedparam(queue->threads[i], SCHED_IDLE, &sched_param);
   #endif
      }
   }

   add_to_atexit_list(queue);
   return true;

fail:
   free(queue->threads);

   if (queue->jobs) {
      cnd_destroy(&queue->has_space_cond);
      cnd_destroy(&queue->has_queued_cond);
      mtx_destroy(&queue->lock);
      free(queue->jobs);
   }
   /* also util_queue_is_initialized can be used to check for success */
   memset(queue, 0, sizeof(*queue));
   return false;
}

static void
util_queue_killall_and_wait(struct util_queue *queue)
{
   unsigned i;

   /* Signal all threads to terminate. */
   mtx_lock(&queue->lock);
   queue->kill_threads = 1;
   cnd_broadcast(&queue->has_queued_cond);
   mtx_unlock(&queue->lock);

   for (i = 0; i < queue->num_threads; i++)
      thrd_join(queue->threads[i], NULL);
   queue->num_threads = 0;
}

void
util_queue_destroy(struct util_queue *queue)
{
   util_queue_killall_and_wait(queue);
   remove_from_atexit_list(queue);

   cnd_destroy(&queue->has_space_cond);
   cnd_destroy(&queue->has_queued_cond);
   mtx_destroy(&queue->lock);
   free(queue->jobs);
   free(queue->threads);
}

void
util_queue_add_job(struct util_queue *queue,
                   void *job,
                   struct util_queue_fence *fence,
                   util_queue_execute_func execute,
                   util_queue_execute_func cleanup)
{
   struct util_queue_job *ptr;

   assert(fence->signalled);

   mtx_lock(&queue->lock);
   if (queue->kill_threads) {
      mtx_unlock(&queue->lock);
      /* well no good option here, but any leaks will be
       * short-lived as things are shutting down..
       */
      return;
   }

   fence->signalled = false;

   assert(queue->num_queued >= 0 && queue->num_queued <= queue->max_jobs);

   if (queue->num_queued == queue->max_jobs) {
      if (queue->flags & UTIL_QUEUE_INIT_RESIZE_IF_FULL) {
         /* If the queue is full, make it larger to avoid waiting for a free
          * slot.
          */
         unsigned new_max_jobs = queue->max_jobs + 8;
         struct util_queue_job *jobs =
            (struct util_queue_job*)calloc(new_max_jobs,
                                           sizeof(struct util_queue_job));
         assert(jobs);

         /* Copy all queued jobs into the new list. */
         unsigned num_jobs = 0;
         unsigned i = queue->read_idx;

         do {
            jobs[num_jobs++] = queue->jobs[i];
            i = (i + 1) % queue->max_jobs;
         } while (i != queue->write_idx);

         assert(num_jobs == queue->num_queued);

         free(queue->jobs);
         queue->jobs = jobs;
         queue->read_idx = 0;
         queue->write_idx = num_jobs;
         queue->max_jobs = new_max_jobs;
      } else {
         /* Wait until there is a free slot. */
         while (queue->num_queued == queue->max_jobs)
            cnd_wait(&queue->has_space_cond, &queue->lock);
      }
   }

   ptr = &queue->jobs[queue->write_idx];
   assert(ptr->job == NULL);
   ptr->job = job;
   ptr->fence = fence;
   ptr->execute = execute;
   ptr->cleanup = cleanup;
   queue->write_idx = (queue->write_idx + 1) % queue->max_jobs;

   queue->num_queued++;
   cnd_signal(&queue->has_queued_cond);
   mtx_unlock(&queue->lock);
}

/**
 * Remove a queued job. If the job hasn't started execution, it's removed from
 * the queue. If the job has started execution, the function waits for it to
 * complete.
 *
 * In all cases, the fence is signalled when the function returns.
 *
 * The function can be used when destroying an object associated with the job
 * when you don't care about the job completion state.
 */
void
util_queue_drop_job(struct util_queue *queue, struct util_queue_fence *fence)
{
   bool removed = false;

   if (util_queue_fence_is_signalled(fence))
      return;

   mtx_lock(&queue->lock);
   for (unsigned i = queue->read_idx; i != queue->write_idx;
        i = (i + 1) % queue->max_jobs) {
      if (queue->jobs[i].fence == fence) {
         if (queue->jobs[i].cleanup)
            queue->jobs[i].cleanup(queue->jobs[i].job, -1);

         /* Just clear it. The threads will treat as a no-op job. */
         memset(&queue->jobs[i], 0, sizeof(queue->jobs[i]));
         removed = true;
         break;
      }
   }
   mtx_unlock(&queue->lock);

   if (removed)
      util_queue_fence_signal(fence);
   else
      util_queue_fence_wait(fence);
}

int64_t
util_queue_get_thread_time_nano(struct util_queue *queue, unsigned thread_index)
{
   /* Allow some flexibility by not raising an error. */
   if (thread_index >= queue->num_threads)
      return 0;

   return u_thread_get_time_nano(queue->threads[thread_index]);
}
