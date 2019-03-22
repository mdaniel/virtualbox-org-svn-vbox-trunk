/**************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#ifndef STW_DEVICE_H_
#define STW_DEVICE_H_


#include "pipe/p_compiler.h"
#include "util/u_handle_table.h"
#include "stw_icd.h"
#include "stw_pixelformat.h"


#define STW_MAX_PIXELFORMATS   256


struct pipe_screen;
struct st_api;
struct st_manager;
struct stw_framebuffer;

struct stw_device
{
   const struct stw_winsys *stw_winsys;
   
   struct pipe_screen *screen;
   
   /* Cache some PIPE_CAP_* */
   unsigned max_2d_levels;
   unsigned max_2d_length;

   struct st_api *stapi;
   struct st_manager *smapi;

   LUID AdapterLuid;

   struct stw_pixelformat_info pixelformats[STW_MAX_PIXELFORMATS];
   unsigned pixelformat_count;
   unsigned pixelformat_extended_count;

   GLCALLBACKTABLE callbacks;

   CRITICAL_SECTION ctx_mutex;
   struct handle_table *ctx_table;
   
   /* TODO: use an atomic counter to track the number of locked
    * stw_framebuffer objects.  Assert that the counter is zero when
    * trying to lock this mutex.
    */
   CRITICAL_SECTION fb_mutex;
   struct stw_framebuffer *fb_head;
   
#ifdef DEBUG
   unsigned long memdbg_no;
#endif

   /** WGL_EXT_swap_control */
   int refresh_rate;
   int swap_interval;

   bool initialized;
};


extern struct stw_device *stw_dev;


static inline struct stw_context *
stw_lookup_context_locked( DHGLRC dhglrc )
{
   if (dhglrc == 0 || stw_dev == NULL)
      return NULL;
   return (struct stw_context *) handle_table_get(stw_dev->ctx_table, dhglrc);
}


static inline void
stw_lock_contexts(struct stw_device *stw_dev)
{
   EnterCriticalSection(&stw_dev->ctx_mutex);
}


static inline void
stw_unlock_contexts(struct stw_device *stw_dev)
{
   LeaveCriticalSection(&stw_dev->ctx_mutex);
}


static inline void
stw_lock_framebuffers(struct stw_device *stw_dev)
{
   EnterCriticalSection(&stw_dev->fb_mutex);
}


static inline void
stw_unlock_framebuffers(struct stw_device *stw_dev)
{
   LeaveCriticalSection(&stw_dev->fb_mutex);
}


#endif /* STW_DEVICE_H_ */
