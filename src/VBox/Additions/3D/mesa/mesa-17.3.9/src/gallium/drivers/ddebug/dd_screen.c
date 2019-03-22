/**************************************************************************
 *
 * Copyright 2015 Advanced Micro Devices, Inc.
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "dd_pipe.h"
#include "dd_public.h"
#include "util/u_memory.h"
#include <stdio.h>


static const char *
dd_screen_get_name(struct pipe_screen *_screen)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_name(screen);
}

static const char *
dd_screen_get_vendor(struct pipe_screen *_screen)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_vendor(screen);
}

static const char *
dd_screen_get_device_vendor(struct pipe_screen *_screen)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_device_vendor(screen);
}

static const void *
dd_screen_get_compiler_options(struct pipe_screen *_screen,
                               enum pipe_shader_ir ir,
                               enum pipe_shader_type shader)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_compiler_options(screen, ir, shader);
}

static struct disk_cache *
dd_screen_get_disk_shader_cache(struct pipe_screen *_screen)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_disk_shader_cache(screen);
}

static int
dd_screen_get_param(struct pipe_screen *_screen,
                    enum pipe_cap param)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_param(screen, param);
}

static float
dd_screen_get_paramf(struct pipe_screen *_screen,
                     enum pipe_capf param)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_paramf(screen, param);
}

static int
dd_screen_get_compute_param(struct pipe_screen *_screen,
                            enum pipe_shader_ir ir_type,
                            enum pipe_compute_cap param,
                            void *ret)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_compute_param(screen, ir_type, param, ret);
}

static int
dd_screen_get_shader_param(struct pipe_screen *_screen,
                           enum pipe_shader_type shader,
                           enum pipe_shader_cap param)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_shader_param(screen, shader, param);
}

static uint64_t
dd_screen_get_timestamp(struct pipe_screen *_screen)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_timestamp(screen);
}

static void dd_screen_query_memory_info(struct pipe_screen *_screen,
                                        struct pipe_memory_info *info)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->query_memory_info(screen, info);
}

static struct pipe_context *
dd_screen_context_create(struct pipe_screen *_screen, void *priv,
                         unsigned flags)
{
   struct dd_screen *dscreen = dd_screen(_screen);
   struct pipe_screen *screen = dscreen->screen;

   flags |= PIPE_CONTEXT_DEBUG;

   return dd_context_create(dscreen,
                            screen->context_create(screen, priv, flags));
}

static boolean
dd_screen_is_format_supported(struct pipe_screen *_screen,
                              enum pipe_format format,
                              enum pipe_texture_target target,
                              unsigned sample_count,
                              unsigned tex_usage)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->is_format_supported(screen, format, target, sample_count,
                                      tex_usage);
}

static boolean
dd_screen_can_create_resource(struct pipe_screen *_screen,
                              const struct pipe_resource *templat)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->can_create_resource(screen, templat);
}

static void
dd_screen_flush_frontbuffer(struct pipe_screen *_screen,
                            struct pipe_resource *resource,
                            unsigned level, unsigned layer,
                            void *context_private,
                            struct pipe_box *sub_box)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->flush_frontbuffer(screen, resource, level, layer, context_private,
                             sub_box);
}

static int
dd_screen_get_driver_query_info(struct pipe_screen *_screen,
                                unsigned index,
                                struct pipe_driver_query_info *info)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_driver_query_info(screen, index, info);
}

static int
dd_screen_get_driver_query_group_info(struct pipe_screen *_screen,
                                      unsigned index,
                                      struct pipe_driver_query_group_info *info)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->get_driver_query_group_info(screen, index, info);
}


static void
dd_screen_get_driver_uuid(struct pipe_screen *_screen, char *uuid)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->get_driver_uuid(screen, uuid);
}

static void
dd_screen_get_device_uuid(struct pipe_screen *_screen, char *uuid)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->get_device_uuid(screen, uuid);
}

/********************************************************************
 * resource
 */

static struct pipe_resource *
dd_screen_resource_create(struct pipe_screen *_screen,
                          const struct pipe_resource *templat)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_resource *res = screen->resource_create(screen, templat);

   if (!res)
      return NULL;
   res->screen = _screen;
   return res;
}

static struct pipe_resource *
dd_screen_resource_from_handle(struct pipe_screen *_screen,
                               const struct pipe_resource *templ,
                               struct winsys_handle *handle,
                               unsigned usage)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_resource *res =
      screen->resource_from_handle(screen, templ, handle, usage);

   if (!res)
      return NULL;
   res->screen = _screen;
   return res;
}

static struct pipe_resource *
dd_screen_resource_from_user_memory(struct pipe_screen *_screen,
                                    const struct pipe_resource *templ,
                                    void *user_memory)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_resource *res =
      screen->resource_from_user_memory(screen, templ, user_memory);

   if (!res)
      return NULL;
   res->screen = _screen;
   return res;
}

static struct pipe_resource *
dd_screen_resource_from_memobj(struct pipe_screen *_screen,
                               const struct pipe_resource *templ,
                               struct pipe_memory_object *memobj,
                               uint64_t offset)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_resource *res =
      screen->resource_from_memobj(screen, templ, memobj, offset);

   if (!res)
      return NULL;
   res->screen = _screen;
   return res;
}

static void
dd_screen_resource_changed(struct pipe_screen *_screen,
                           struct pipe_resource *res)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->resource_changed(screen, res);
}

static void
dd_screen_resource_destroy(struct pipe_screen *_screen,
                           struct pipe_resource *res)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->resource_destroy(screen, res);
}

static boolean
dd_screen_resource_get_handle(struct pipe_screen *_screen,
                              struct pipe_context *_pipe,
                              struct pipe_resource *resource,
                              struct winsys_handle *handle,
                              unsigned usage)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_context *pipe = _pipe ? dd_context(_pipe)->pipe : NULL;

   return screen->resource_get_handle(screen, pipe, resource, handle, usage);
}

static bool
dd_screen_check_resource_capability(struct pipe_screen *_screen,
                                    struct pipe_resource *resource,
                                    unsigned bind)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->check_resource_capability(screen, resource, bind);
}


/********************************************************************
 * fence
 */

static void
dd_screen_fence_reference(struct pipe_screen *_screen,
                          struct pipe_fence_handle **pdst,
                          struct pipe_fence_handle *src)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->fence_reference(screen, pdst, src);
}

static boolean
dd_screen_fence_finish(struct pipe_screen *_screen,
                       struct pipe_context *_ctx,
                       struct pipe_fence_handle *fence,
                       uint64_t timeout)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;
   struct pipe_context *ctx = _ctx ? dd_context(_ctx)->pipe : NULL;

   return screen->fence_finish(screen, ctx, fence, timeout);
}

/********************************************************************
 * memobj
 */

static struct pipe_memory_object *
dd_screen_memobj_create_from_handle(struct pipe_screen *_screen,
                                    struct winsys_handle *handle,
                                    bool dedicated)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   return screen->memobj_create_from_handle(screen, handle, dedicated);
}

static void
dd_screen_memobj_destroy(struct pipe_screen *_screen,
                         struct pipe_memory_object *memobj)
{
   struct pipe_screen *screen = dd_screen(_screen)->screen;

   screen->memobj_destroy(screen, memobj);
}
/********************************************************************
 * screen
 */

static void
dd_screen_destroy(struct pipe_screen *_screen)
{
   struct dd_screen *dscreen = dd_screen(_screen);
   struct pipe_screen *screen = dscreen->screen;

   screen->destroy(screen);
   FREE(dscreen);
}

struct pipe_screen *
ddebug_screen_create(struct pipe_screen *screen)
{
   struct dd_screen *dscreen;
   const char *option;
   bool no_flush;
   unsigned timeout = 0;
   unsigned apitrace_dump_call = 0;
   enum dd_mode mode;

   option = debug_get_option("GALLIUM_DDEBUG", NULL);
   if (!option)
      return screen;

   if (!strcmp(option, "help")) {
      puts("Gallium driver debugger");
      puts("");
      puts("Usage:");
      puts("");
      puts("  GALLIUM_DDEBUG=\"always [noflush] [verbose]\"");
      puts("    Flush and dump context and driver information after every draw call into");
      puts("    $HOME/"DD_DIR"/.");
      puts("");
      puts("  GALLIUM_DDEBUG=\"[timeout in ms] [noflush] [verbose]\"");
      puts("    Flush and detect a device hang after every draw call based on the given");
      puts("    fence timeout and dump context and driver information into");
      puts("    $HOME/"DD_DIR"/ when a hang is detected.");
      puts("");
      puts("  GALLIUM_DDEBUG=\"pipelined [timeout in ms] [verbose]\"");
      puts("    Detect a device hang after every draw call based on the given fence");
      puts("    timeout without flushes and dump context and driver information into");
      puts("    $HOME/"DD_DIR"/ when a hang is detected.");
      puts("");
      puts("  GALLIUM_DDEBUG=\"apitrace [call#] [verbose]\"");
      puts("    Dump apitrace draw call information into $HOME/"DD_DIR"/. Implies 'noflush'.");
      puts("");
      puts("  If 'noflush' is specified, do not flush on every draw call. In hang");
      puts("  detection mode, this only detect hangs in pipe->flush.");
      puts("  If 'verbose' is specified, additional information is written to stderr.");
      puts("");
      puts("  GALLIUM_DDEBUG_SKIP=[count]");
      puts("    Skip flush and hang detection for the given initial number of draw calls.");
      puts("");
      exit(0);
   }

   no_flush = strstr(option, "noflush") != NULL;

   if (!strncmp(option, "always", 6)) {
      mode = DD_DUMP_ALL_CALLS;
   } else if (!strncmp(option, "apitrace", 8)) {
      mode = DD_DUMP_APITRACE_CALL;
      no_flush = true;

      if (sscanf(option+8, "%u", &apitrace_dump_call) != 1)
         return screen;
   } else if (!strncmp(option, "pipelined", 9)) {
      mode = DD_DETECT_HANGS_PIPELINED;

      if (sscanf(option+10, "%u", &timeout) != 1)
         return screen;
   } else {
      mode = DD_DETECT_HANGS;

      if (sscanf(option, "%u", &timeout) != 1)
         return screen;
   }

   dscreen = CALLOC_STRUCT(dd_screen);
   if (!dscreen)
      return NULL;

#define SCR_INIT(_member) \
   dscreen->base._member = screen->_member ? dd_screen_##_member : NULL

   dscreen->base.destroy = dd_screen_destroy;
   dscreen->base.get_name = dd_screen_get_name;
   dscreen->base.get_vendor = dd_screen_get_vendor;
   dscreen->base.get_device_vendor = dd_screen_get_device_vendor;
   SCR_INIT(get_disk_shader_cache);
   dscreen->base.get_param = dd_screen_get_param;
   dscreen->base.get_paramf = dd_screen_get_paramf;
   dscreen->base.get_compute_param = dd_screen_get_compute_param;
   dscreen->base.get_shader_param = dd_screen_get_shader_param;
   dscreen->base.query_memory_info = dd_screen_query_memory_info;
   /* get_video_param */
   /* get_compute_param */
   SCR_INIT(get_timestamp);
   dscreen->base.context_create = dd_screen_context_create;
   dscreen->base.is_format_supported = dd_screen_is_format_supported;
   /* is_video_format_supported */
   SCR_INIT(can_create_resource);
   dscreen->base.resource_create = dd_screen_resource_create;
   dscreen->base.resource_from_handle = dd_screen_resource_from_handle;
   SCR_INIT(resource_from_memobj);
   SCR_INIT(resource_from_user_memory);
   SCR_INIT(check_resource_capability);
   dscreen->base.resource_get_handle = dd_screen_resource_get_handle;
   SCR_INIT(resource_changed);
   dscreen->base.resource_destroy = dd_screen_resource_destroy;
   SCR_INIT(flush_frontbuffer);
   SCR_INIT(fence_reference);
   SCR_INIT(fence_finish);
   SCR_INIT(memobj_create_from_handle);
   SCR_INIT(memobj_destroy);
   SCR_INIT(get_driver_query_info);
   SCR_INIT(get_driver_query_group_info);
   SCR_INIT(get_compiler_options);
   SCR_INIT(get_driver_uuid);
   SCR_INIT(get_device_uuid);

#undef SCR_INIT

   dscreen->screen = screen;
   dscreen->timeout_ms = timeout;
   dscreen->mode = mode;
   dscreen->no_flush = no_flush;
   dscreen->verbose = strstr(option, "verbose") != NULL;
   dscreen->apitrace_dump_call = apitrace_dump_call;

   switch (dscreen->mode) {
   case DD_DUMP_ALL_CALLS:
      fprintf(stderr, "Gallium debugger active. Logging all calls.\n");
      break;
   case DD_DETECT_HANGS:
   case DD_DETECT_HANGS_PIPELINED:
      fprintf(stderr, "Gallium debugger active. "
              "The hang detection timeout is %i ms.\n", timeout);
      break;
   case DD_DUMP_APITRACE_CALL:
      fprintf(stderr, "Gallium debugger active. Going to dump an apitrace call.\n");
      break;
   default:
      assert(0);
   }

   dscreen->skip_count = debug_get_num_option("GALLIUM_DDEBUG_SKIP", 0);
   if (dscreen->skip_count > 0) {
      fprintf(stderr, "Gallium debugger skipping the first %u draw calls.\n",
              dscreen->skip_count);
   }

   return &dscreen->base;
}
