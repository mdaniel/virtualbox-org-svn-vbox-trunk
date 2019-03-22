/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2010 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_atomic.h"
#include "state_tracker/st_gl_api.h" /* for st_gl_api_create */

#include "stw_st.h"
#include "stw_device.h"
#include "stw_framebuffer.h"
#include "stw_pixelformat.h"

struct stw_st_framebuffer {
   struct st_framebuffer_iface base;

   struct stw_framebuffer *fb;
   struct st_visual stvis;

   struct pipe_resource *textures[ST_ATTACHMENT_COUNT];
   unsigned texture_width, texture_height;
   unsigned texture_mask;
};

static uint32_t stwfb_ID = 0;

/**
 * Is the given mutex held by the calling thread?
 */
bool
stw_own_mutex(const CRITICAL_SECTION *cs)
{
   // We can't compare OwningThread with our thread handle/id (see
   // http://stackoverflow.com/a/12675635 ) but we can compare with the
   // OwningThread member of a critical section we know we own.
   CRITICAL_SECTION dummy;
   InitializeCriticalSection(&dummy);
   EnterCriticalSection(&dummy);
   if (0)
      _debug_printf("%p %p\n", cs->OwningThread, dummy.OwningThread);
   bool ret = cs->OwningThread == dummy.OwningThread;
   LeaveCriticalSection(&dummy);
   DeleteCriticalSection(&dummy);
   return ret;
}


/**
 * Remove outdated textures and create the requested ones.
 */
static void
stw_st_framebuffer_validate_locked(struct st_framebuffer_iface *stfb,
                                   unsigned width, unsigned height,
                                   unsigned mask)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   struct pipe_resource templ;
   unsigned i;

   /* remove outdated textures */
   if (stwfb->texture_width != width || stwfb->texture_height != height) {
      for (i = 0; i < ST_ATTACHMENT_COUNT; i++)
         pipe_resource_reference(&stwfb->textures[i], NULL);
   }

   memset(&templ, 0, sizeof(templ));
   templ.target = PIPE_TEXTURE_2D;
   templ.width0 = width;
   templ.height0 = height;
   templ.depth0 = 1;
   templ.array_size = 1;
   templ.last_level = 0;
   templ.nr_samples = stwfb->stvis.samples;

   for (i = 0; i < ST_ATTACHMENT_COUNT; i++) {
      enum pipe_format format;
      unsigned bind;

      /* the texture already exists or not requested */
      if (stwfb->textures[i] || !(mask & (1 << i))) {
         /* remember the texture */
         if (stwfb->textures[i])
            mask |= (1 << i);
         continue;
      }

      switch (i) {
      case ST_ATTACHMENT_FRONT_LEFT:
      case ST_ATTACHMENT_BACK_LEFT:
         format = stwfb->stvis.color_format;
         bind = PIPE_BIND_DISPLAY_TARGET |
                PIPE_BIND_SAMPLER_VIEW |
                PIPE_BIND_RENDER_TARGET;
         break;
      case ST_ATTACHMENT_DEPTH_STENCIL:
         format = stwfb->stvis.depth_stencil_format;
         bind = PIPE_BIND_DEPTH_STENCIL;
         break;
      default:
         format = PIPE_FORMAT_NONE;
         break;
      }

      if (format != PIPE_FORMAT_NONE) {
         templ.format = format;
         templ.bind = bind;

         stwfb->textures[i] =
            stw_dev->screen->resource_create(stw_dev->screen, &templ);
      }
   }

   stwfb->texture_width = width;
   stwfb->texture_height = height;
   stwfb->texture_mask = mask;
}

static boolean 
stw_st_framebuffer_validate(struct st_context_iface *stctx,
                            struct st_framebuffer_iface *stfb,
                            const enum st_attachment_type *statts,
                            unsigned count,
                            struct pipe_resource **out)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   unsigned statt_mask, i;

   statt_mask = 0x0;
   for (i = 0; i < count; i++)
      statt_mask |= 1 << statts[i];

   stw_framebuffer_lock(stwfb->fb);

   if (stwfb->fb->must_resize || (statt_mask & ~stwfb->texture_mask)) {
      stw_st_framebuffer_validate_locked(&stwfb->base,
            stwfb->fb->width, stwfb->fb->height, statt_mask);
      stwfb->fb->must_resize = FALSE;
   }

   for (i = 0; i < count; i++)
      pipe_resource_reference(&out[i], stwfb->textures[statts[i]]);

   stw_framebuffer_unlock(stwfb->fb);

   return TRUE;
}

/**
 * Present an attachment of the framebuffer.
 */
static boolean
stw_st_framebuffer_present_locked(HDC hdc,
                                  struct st_framebuffer_iface *stfb,
                                  enum st_attachment_type statt)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   struct pipe_resource *resource;

   assert(stw_own_mutex(&stwfb->fb->mutex));

   resource = stwfb->textures[statt];
   if (resource) {
      stw_framebuffer_present_locked(hdc, stwfb->fb, resource);
   }
   else {
      stw_framebuffer_unlock(stwfb->fb);
   }

   assert(!stw_own_mutex(&stwfb->fb->mutex));

   return TRUE;
}

static boolean
stw_st_framebuffer_flush_front(struct st_context_iface *stctx,
                               struct st_framebuffer_iface *stfb,
                               enum st_attachment_type statt)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   boolean ret;
   HDC hDC;

   stw_framebuffer_lock(stwfb->fb);

   /* We must not cache HDCs anywhere, as they can be invalidated by the
    * application, or screen resolution changes. */

   hDC = GetDC(stwfb->fb->hWnd);

   ret = stw_st_framebuffer_present_locked(hDC, &stwfb->base, statt);

   ReleaseDC(stwfb->fb->hWnd, hDC);

   return ret;
}

/**
 * Create a framebuffer interface.
 */
struct st_framebuffer_iface *
stw_st_create_framebuffer(struct stw_framebuffer *fb)
{
   struct stw_st_framebuffer *stwfb;

   stwfb = CALLOC_STRUCT(stw_st_framebuffer);
   if (!stwfb)
      return NULL;

   stwfb->fb = fb;
   stwfb->stvis = fb->pfi->stvis;
   stwfb->base.ID = p_atomic_inc_return(&stwfb_ID);
   stwfb->base.state_manager = stw_dev->smapi;

   stwfb->base.visual = &stwfb->stvis;
   p_atomic_set(&stwfb->base.stamp, 1);
   stwfb->base.flush_front = stw_st_framebuffer_flush_front;
   stwfb->base.validate = stw_st_framebuffer_validate;

   return &stwfb->base;
}

/**
 * Destroy a framebuffer interface.
 */
void
stw_st_destroy_framebuffer_locked(struct st_framebuffer_iface *stfb)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   int i;

   for (i = 0; i < ST_ATTACHMENT_COUNT; i++)
      pipe_resource_reference(&stwfb->textures[i], NULL);

   /* Notify the st manager that the framebuffer interface is no
    * longer valid.
    */
   stw_dev->stapi->destroy_drawable(stw_dev->stapi, &stwfb->base);

   FREE(stwfb);
}

/**
 * Swap the buffers of the given framebuffer.
 */
boolean
stw_st_swap_framebuffer_locked(HDC hdc, struct st_framebuffer_iface *stfb)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   unsigned front = ST_ATTACHMENT_FRONT_LEFT, back = ST_ATTACHMENT_BACK_LEFT;
   struct pipe_resource *ptex;
   unsigned mask;

   /* swap the textures */
   ptex = stwfb->textures[front];
   stwfb->textures[front] = stwfb->textures[back];
   stwfb->textures[back] = ptex;

   /* convert to mask */
   front = 1 << front;
   back = 1 << back;

   /* swap the bits in mask */
   mask = stwfb->texture_mask & ~(front | back);
   if (stwfb->texture_mask & front)
      mask |= back;
   if (stwfb->texture_mask & back)
      mask |= front;
   stwfb->texture_mask = mask;

   front = ST_ATTACHMENT_FRONT_LEFT;
   return stw_st_framebuffer_present_locked(hdc, &stwfb->base, front);
}


/**
 * Return the pipe_resource that correspond to given buffer.
 */
struct pipe_resource *
stw_get_framebuffer_resource(struct st_framebuffer_iface *stfb,
                             enum st_attachment_type att)
{
   struct stw_st_framebuffer *stwfb = stw_st_framebuffer(stfb);
   return stwfb->textures[att];
}


/**
 * Create an st_api of the state tracker.
 */
struct st_api *
stw_st_create_api(void)
{
   return st_gl_api_create();
}
