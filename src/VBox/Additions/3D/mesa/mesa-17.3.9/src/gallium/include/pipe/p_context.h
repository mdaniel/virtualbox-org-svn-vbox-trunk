/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
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

#ifndef PIPE_CONTEXT_H
#define PIPE_CONTEXT_H

#include "p_compiler.h"
#include "p_format.h"
#include "p_video_enums.h"
#include "p_defines.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


struct pipe_blend_color;
struct pipe_blend_state;
struct pipe_blit_info;
struct pipe_box;
struct pipe_clip_state;
struct pipe_constant_buffer;
struct pipe_debug_callback;
struct pipe_depth_stencil_alpha_state;
struct pipe_device_reset_callback;
struct pipe_draw_info;
struct pipe_grid_info;
struct pipe_fence_handle;
struct pipe_framebuffer_state;
struct pipe_image_view;
struct pipe_query;
struct pipe_poly_stipple;
struct pipe_rasterizer_state;
struct pipe_resolve_info;
struct pipe_resource;
struct pipe_sampler_state;
struct pipe_sampler_view;
struct pipe_scissor_state;
struct pipe_shader_buffer;
struct pipe_shader_state;
struct pipe_stencil_ref;
struct pipe_stream_output_target;
struct pipe_surface;
struct pipe_transfer;
struct pipe_vertex_buffer;
struct pipe_vertex_element;
struct pipe_video_buffer;
struct pipe_video_codec;
struct pipe_viewport_state;
struct pipe_compute_state;
union pipe_color_union;
union pipe_query_result;
struct u_log_context;
struct u_upload_mgr;

/**
 * Gallium rendering context.  Basically:
 *  - state setting functions
 *  - VBO drawing functions
 *  - surface functions
 */
struct pipe_context {
   struct pipe_screen *screen;

   void *priv;  /**< context private data (for DRI for example) */
   void *draw;  /**< private, for draw module (temporary?) */

   /**
    * Stream uploaders created by the driver. All drivers, state trackers, and
    * modules should use them.
    *
    * Use u_upload_alloc or u_upload_data as many times as you want.
    * Once you are done, use u_upload_unmap.
    */
   struct u_upload_mgr *stream_uploader; /* everything but shader constants */
   struct u_upload_mgr *const_uploader;  /* shader constants only */

   void (*destroy)( struct pipe_context * );

   /**
    * VBO drawing
    */
   /*@{*/
   void (*draw_vbo)( struct pipe_context *pipe,
                     const struct pipe_draw_info *info );
   /*@}*/

   /**
    * Predicate subsequent rendering on occlusion query result
    * \param query  the query predicate, or NULL if no predicate
    * \param condition whether to skip on FALSE or TRUE query results
    * \param mode  one of PIPE_RENDER_COND_x
    */
   void (*render_condition)( struct pipe_context *pipe,
                             struct pipe_query *query,
                             boolean condition,
                             enum pipe_render_cond_flag mode );

   /**
    * Query objects
    */
   /*@{*/
   struct pipe_query *(*create_query)( struct pipe_context *pipe,
                                       unsigned query_type,
                                       unsigned index );

   /**
    * Create a query object that queries all given query types simultaneously.
    *
    * This can only be used for those query types for which
    * get_driver_query_info indicates that it must be used. Only one batch
    * query object may be active at a time.
    *
    * There may be additional constraints on which query types can be used
    * together, in particular those that are implied by
    * get_driver_query_group_info.
    *
    * \param num_queries the number of query types
    * \param query_types array of \p num_queries query types
    * \return a query object, or NULL on error.
    */
   struct pipe_query *(*create_batch_query)( struct pipe_context *pipe,
                                             unsigned num_queries,
                                             unsigned *query_types );

   void (*destroy_query)(struct pipe_context *pipe,
                         struct pipe_query *q);

   boolean (*begin_query)(struct pipe_context *pipe, struct pipe_query *q);
   bool (*end_query)(struct pipe_context *pipe, struct pipe_query *q);

   /**
    * Get results of a query.
    * \param wait  if true, this query will block until the result is ready
    * \return TRUE if results are ready, FALSE otherwise
    */
   boolean (*get_query_result)(struct pipe_context *pipe,
                               struct pipe_query *q,
                               boolean wait,
                               union pipe_query_result *result);

   /**
    * Get results of a query, storing into resource. Note that this may not
    * be used with batch queries.
    *
    * \param wait  if true, this query will block until the result is ready
    * \param result_type  the type of the value being stored:
    * \param index  for queries that return multiple pieces of data, which
    *               item of that data to store (e.g. for
    *               PIPE_QUERY_PIPELINE_STATISTICS).
    *               When the index is -1, instead of the value of the query
    *               the driver should instead write a 1 or 0 to the appropriate
    *               location with 1 meaning that the query result is available.
    */
   void (*get_query_result_resource)(struct pipe_context *pipe,
                                     struct pipe_query *q,
                                     boolean wait,
                                     enum pipe_query_value_type result_type,
                                     int index,
                                     struct pipe_resource *resource,
                                     unsigned offset);

   /**
    * Set whether all current non-driver queries except TIME_ELAPSED are
    * active or paused.
    */
   void (*set_active_query_state)(struct pipe_context *pipe, boolean enable);

   /*@}*/

   /**
    * State functions (create/bind/destroy state objects)
    */
   /*@{*/
   void * (*create_blend_state)(struct pipe_context *,
                                const struct pipe_blend_state *);
   void   (*bind_blend_state)(struct pipe_context *, void *);
   void   (*delete_blend_state)(struct pipe_context *, void  *);

   void * (*create_sampler_state)(struct pipe_context *,
                                  const struct pipe_sampler_state *);
   void   (*bind_sampler_states)(struct pipe_context *,
                                 enum pipe_shader_type shader,
                                 unsigned start_slot, unsigned num_samplers,
                                 void **samplers);
   void   (*delete_sampler_state)(struct pipe_context *, void *);

   void * (*create_rasterizer_state)(struct pipe_context *,
                                     const struct pipe_rasterizer_state *);
   void   (*bind_rasterizer_state)(struct pipe_context *, void *);
   void   (*delete_rasterizer_state)(struct pipe_context *, void *);

   void * (*create_depth_stencil_alpha_state)(struct pipe_context *,
                                        const struct pipe_depth_stencil_alpha_state *);
   void   (*bind_depth_stencil_alpha_state)(struct pipe_context *, void *);
   void   (*delete_depth_stencil_alpha_state)(struct pipe_context *, void *);

   void * (*create_fs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_fs_state)(struct pipe_context *, void *);
   void   (*delete_fs_state)(struct pipe_context *, void *);

   void * (*create_vs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_vs_state)(struct pipe_context *, void *);
   void   (*delete_vs_state)(struct pipe_context *, void *);

   void * (*create_gs_state)(struct pipe_context *,
                             const struct pipe_shader_state *);
   void   (*bind_gs_state)(struct pipe_context *, void *);
   void   (*delete_gs_state)(struct pipe_context *, void *);

   void * (*create_tcs_state)(struct pipe_context *,
                              const struct pipe_shader_state *);
   void   (*bind_tcs_state)(struct pipe_context *, void *);
   void   (*delete_tcs_state)(struct pipe_context *, void *);

   void * (*create_tes_state)(struct pipe_context *,
                              const struct pipe_shader_state *);
   void   (*bind_tes_state)(struct pipe_context *, void *);
   void   (*delete_tes_state)(struct pipe_context *, void *);

   void * (*create_vertex_elements_state)(struct pipe_context *,
                                          unsigned num_elements,
                                          const struct pipe_vertex_element *);
   void   (*bind_vertex_elements_state)(struct pipe_context *, void *);
   void   (*delete_vertex_elements_state)(struct pipe_context *, void *);

   /*@}*/

   /**
    * Parameter-like state (or properties)
    */
   /*@{*/
   void (*set_blend_color)( struct pipe_context *,
                            const struct pipe_blend_color * );

   void (*set_stencil_ref)( struct pipe_context *,
                            const struct pipe_stencil_ref * );

   void (*set_sample_mask)( struct pipe_context *,
                            unsigned sample_mask );

   void (*set_min_samples)( struct pipe_context *,
                            unsigned min_samples );

   void (*set_clip_state)( struct pipe_context *,
                            const struct pipe_clip_state * );

   void (*set_constant_buffer)( struct pipe_context *,
                                enum pipe_shader_type shader, uint index,
                                const struct pipe_constant_buffer *buf );

   void (*set_framebuffer_state)( struct pipe_context *,
                                  const struct pipe_framebuffer_state * );

   void (*set_polygon_stipple)( struct pipe_context *,
				const struct pipe_poly_stipple * );

   void (*set_scissor_states)( struct pipe_context *,
                               unsigned start_slot,
                               unsigned num_scissors,
                               const struct pipe_scissor_state * );

   void (*set_window_rectangles)( struct pipe_context *,
                                  boolean include,
                                  unsigned num_rectangles,
                                  const struct pipe_scissor_state * );

   void (*set_viewport_states)( struct pipe_context *,
                                unsigned start_slot,
                                unsigned num_viewports,
                                const struct pipe_viewport_state *);

   void (*set_sampler_views)(struct pipe_context *,
                             enum pipe_shader_type shader,
                             unsigned start_slot, unsigned num_views,
                             struct pipe_sampler_view **);

   void (*set_tess_state)(struct pipe_context *,
                          const float default_outer_level[4],
                          const float default_inner_level[2]);

   /**
    * Sets the debug callback. If the pointer is null, then no callback is
    * set, otherwise a copy of the data should be made.
    */
   void (*set_debug_callback)(struct pipe_context *,
                              const struct pipe_debug_callback *);

   /**
    * Bind an array of shader buffers that will be used by a shader.
    * Any buffers that were previously bound to the specified range
    * will be unbound.
    *
    * \param shader     selects shader stage
    * \param start_slot first buffer slot to bind.
    * \param count      number of consecutive buffers to bind.
    * \param buffers    array of pointers to the buffers to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no buffers will
    *                   be bound.
    */
   void (*set_shader_buffers)(struct pipe_context *,
                              enum pipe_shader_type shader,
                              unsigned start_slot, unsigned count,
                              const struct pipe_shader_buffer *buffers);

   /**
    * Bind an array of images that will be used by a shader.
    * Any images that were previously bound to the specified range
    * will be unbound.
    *
    * \param shader     selects shader stage
    * \param start_slot first image slot to bind.
    * \param count      number of consecutive images to bind.
    * \param buffers    array of the images to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no images will
    *                   be bound.
    */
   void (*set_shader_images)(struct pipe_context *,
                             enum pipe_shader_type shader,
                             unsigned start_slot, unsigned count,
                             const struct pipe_image_view *images);

   void (*set_vertex_buffers)( struct pipe_context *,
                               unsigned start_slot,
                               unsigned num_buffers,
                               const struct pipe_vertex_buffer * );

   /*@}*/

   /**
    * Stream output functions.
    */
   /*@{*/

   struct pipe_stream_output_target *(*create_stream_output_target)(
                        struct pipe_context *,
                        struct pipe_resource *,
                        unsigned buffer_offset,
                        unsigned buffer_size);

   void (*stream_output_target_destroy)(struct pipe_context *,
                                        struct pipe_stream_output_target *);

   void (*set_stream_output_targets)(struct pipe_context *,
                              unsigned num_targets,
                              struct pipe_stream_output_target **targets,
                              const unsigned *offsets);

   /*@}*/


   /**
    * Resource functions for blit-like functionality
    *
    * If a driver supports multisampling, blit must implement color resolve.
    */
   /*@{*/

   /**
    * Copy a block of pixels from one resource to another.
    * The resource must be of the same format.
    * Resources with nr_samples > 1 are not allowed.
    */
   void (*resource_copy_region)(struct pipe_context *pipe,
                                struct pipe_resource *dst,
                                unsigned dst_level,
                                unsigned dstx, unsigned dsty, unsigned dstz,
                                struct pipe_resource *src,
                                unsigned src_level,
                                const struct pipe_box *src_box);

   /* Optimal hardware path for blitting pixels.
    * Scaling, format conversion, up- and downsampling (resolve) are allowed.
    */
   void (*blit)(struct pipe_context *pipe,
                const struct pipe_blit_info *info);

   /*@}*/

   /**
    * Clear the specified set of currently bound buffers to specified values.
    * The entire buffers are cleared (no scissor, no colormask, etc).
    *
    * \param buffers  bitfield of PIPE_CLEAR_* values.
    * \param color  pointer to a union of fiu array for each of r, g, b, a.
    * \param depth  depth clear value in [0,1].
    * \param stencil  stencil clear value
    */
   void (*clear)(struct pipe_context *pipe,
                 unsigned buffers,
                 const union pipe_color_union *color,
                 double depth,
                 unsigned stencil);

   /**
    * Clear a color rendertarget surface.
    * \param color  pointer to an union of fiu array for each of r, g, b, a.
    */
   void (*clear_render_target)(struct pipe_context *pipe,
                               struct pipe_surface *dst,
                               const union pipe_color_union *color,
                               unsigned dstx, unsigned dsty,
                               unsigned width, unsigned height,
                               bool render_condition_enabled);

   /**
    * Clear a depth-stencil surface.
    * \param clear_flags  bitfield of PIPE_CLEAR_DEPTH/STENCIL values.
    * \param depth  depth clear value in [0,1].
    * \param stencil  stencil clear value
    */
   void (*clear_depth_stencil)(struct pipe_context *pipe,
                               struct pipe_surface *dst,
                               unsigned clear_flags,
                               double depth,
                               unsigned stencil,
                               unsigned dstx, unsigned dsty,
                               unsigned width, unsigned height,
                               bool render_condition_enabled);

   /**
    * Clear the texture with the specified texel. Not guaranteed to be a
    * renderable format. Data provided in the resource's format.
    */
   void (*clear_texture)(struct pipe_context *pipe,
                         struct pipe_resource *res,
                         unsigned level,
                         const struct pipe_box *box,
                         const void *data);

   /**
    * Clear a buffer. Runs a memset over the specified region with the element
    * value passed in through clear_value of size clear_value_size.
    */
   void (*clear_buffer)(struct pipe_context *pipe,
                        struct pipe_resource *res,
                        unsigned offset,
                        unsigned size,
                        const void *clear_value,
                        int clear_value_size);

   /**
    * Flush draw commands
    *
    * NOTE: use screen->fence_reference() (or equivalent) to transfer
    * new fence ref to **fence, to ensure that previous fence is unref'd
    *
    * \param fence  if not NULL, an old fence to unref and transfer a
    *    new fence reference to
    * \param flags  bitfield of enum pipe_flush_flags values.
    */
   void (*flush)(struct pipe_context *pipe,
                 struct pipe_fence_handle **fence,
                 unsigned flags);

   /**
    * Create a fence from a native sync fd.
    *
    * This is used for importing a foreign/external fence fd.
    *
    * \param fence  if not NULL, an old fence to unref and transfer a
    *    new fence reference to
    * \param fd     native fence fd
    */
   void (*create_fence_fd)(struct pipe_context *pipe,
                           struct pipe_fence_handle **fence,
                           int fd);

   /**
    * Insert commands to have GPU wait for fence to be signaled.
    */
   void (*fence_server_sync)(struct pipe_context *pipe,
                             struct pipe_fence_handle *fence);

   /**
    * Create a view on a texture to be used by a shader stage.
    */
   struct pipe_sampler_view * (*create_sampler_view)(struct pipe_context *ctx,
                                                     struct pipe_resource *texture,
                                                     const struct pipe_sampler_view *templat);

   void (*sampler_view_destroy)(struct pipe_context *ctx,
                                struct pipe_sampler_view *view);


   /**
    * Get a surface which is a "view" into a resource, used by
    * render target / depth stencil stages.
    */
   struct pipe_surface *(*create_surface)(struct pipe_context *ctx,
                                          struct pipe_resource *resource,
                                          const struct pipe_surface *templat);

   void (*surface_destroy)(struct pipe_context *ctx,
                           struct pipe_surface *);


   /**
    * Map a resource.
    *
    * Transfers are (by default) context-private and allow uploads to be
    * interleaved with rendering.
    *
    * out_transfer will contain the transfer object that must be passed
    * to all the other transfer functions. It also contains useful
    * information (like texture strides).
    */
   void *(*transfer_map)(struct pipe_context *,
                         struct pipe_resource *resource,
                         unsigned level,
                         unsigned usage,  /* a combination of PIPE_TRANSFER_x */
                         const struct pipe_box *,
                         struct pipe_transfer **out_transfer);

   /* If transfer was created with WRITE|FLUSH_EXPLICIT, only the
    * regions specified with this call are guaranteed to be written to
    * the resource.
    */
   void (*transfer_flush_region)( struct pipe_context *,
				  struct pipe_transfer *transfer,
				  const struct pipe_box *);

   void (*transfer_unmap)(struct pipe_context *,
                          struct pipe_transfer *transfer);

   /* One-shot transfer operation with data supplied in a user
    * pointer.
    */
   void (*buffer_subdata)(struct pipe_context *,
                          struct pipe_resource *,
                          unsigned usage, /* a combination of PIPE_TRANSFER_x */
                          unsigned offset,
                          unsigned size,
                          const void *data);

   void (*texture_subdata)(struct pipe_context *,
                           struct pipe_resource *,
                           unsigned level,
                           unsigned usage, /* a combination of PIPE_TRANSFER_x */
                           const struct pipe_box *,
                           const void *data,
                           unsigned stride,
                           unsigned layer_stride);

   /**
    * Flush any pending framebuffer writes and invalidate texture caches.
    */
   void (*texture_barrier)(struct pipe_context *, unsigned flags);

   /**
    * Flush caches according to flags.
    */
   void (*memory_barrier)(struct pipe_context *, unsigned flags);

   /**
    * Change the commitment status of a part of the given resource, which must
    * have been created with the PIPE_RESOURCE_FLAG_SPARSE bit.
    *
    * \param level The texture level whose commitment should be changed.
    * \param box The region of the resource whose commitment should be changed.
    * \param commit Whether memory should be committed or un-committed.
    *
    * \return false if out of memory, true on success.
    */
   bool (*resource_commit)(struct pipe_context *, struct pipe_resource *,
                           unsigned level, struct pipe_box *box, bool commit);

   /**
    * Creates a video codec for a specific video format/profile
    */
   struct pipe_video_codec *(*create_video_codec)( struct pipe_context *context,
                                                   const struct pipe_video_codec *templat );

   /**
    * Creates a video buffer as decoding target
    */
   struct pipe_video_buffer *(*create_video_buffer)( struct pipe_context *context,
                                                     const struct pipe_video_buffer *templat );

   /**
    * Compute kernel execution
    */
   /*@{*/
   /**
    * Define the compute program and parameters to be used by
    * pipe_context::launch_grid.
    */
   void *(*create_compute_state)(struct pipe_context *context,
				 const struct pipe_compute_state *);
   void (*bind_compute_state)(struct pipe_context *, void *);
   void (*delete_compute_state)(struct pipe_context *, void *);

   /**
    * Bind an array of shader resources that will be used by the
    * compute program.  Any resources that were previously bound to
    * the specified range will be unbound after this call.
    *
    * \param start      first resource to bind.
    * \param count      number of consecutive resources to bind.
    * \param resources  array of pointers to the resources to bind, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no new
    *                   resources will be bound.
    */
   void (*set_compute_resources)(struct pipe_context *,
                                 unsigned start, unsigned count,
                                 struct pipe_surface **resources);

   /**
    * Bind an array of buffers to be mapped into the address space of
    * the GLOBAL resource.  Any buffers that were previously bound
    * between [first, first + count - 1] are unbound after this call.
    *
    * \param first      first buffer to map.
    * \param count      number of consecutive buffers to map.
    * \param resources  array of pointers to the buffers to map, it
    *                   should contain at least \a count elements
    *                   unless it's NULL, in which case no new
    *                   resources will be bound.
    * \param handles    array of pointers to the memory locations that
    *                   will be updated with the address each buffer
    *                   will be mapped to.  The base memory address of
    *                   each of the buffers will be added to the value
    *                   pointed to by its corresponding handle to form
    *                   the final address argument.  It should contain
    *                   at least \a count elements, unless \a
    *                   resources is NULL in which case \a handles
    *                   should be NULL as well.
    *
    * Note that the driver isn't required to make any guarantees about
    * the contents of the \a handles array being valid anytime except
    * during the subsequent calls to pipe_context::launch_grid.  This
    * means that the only sensible location handles[i] may point to is
    * somewhere within the INPUT buffer itself.  This is so to
    * accommodate implementations that lack virtual memory but
    * nevertheless migrate buffers on the fly, leading to resource
    * base addresses that change on each kernel invocation or are
    * unknown to the pipe driver.
    */
   void (*set_global_binding)(struct pipe_context *context,
                              unsigned first, unsigned count,
                              struct pipe_resource **resources,
                              uint32_t **handles);

   /**
    * Launch the compute kernel starting from instruction \a pc of the
    * currently bound compute program.
    */
   void (*launch_grid)(struct pipe_context *context,
                       const struct pipe_grid_info *info);
   /*@}*/

   /**
    * Get sample position for an individual sample point.
    *
    * \param sample_count - total number of samples
    * \param sample_index - sample to get the position values for
    * \param out_value - return value of 2 floats for x and y position for
    *                    requested sample.
    */
   void (*get_sample_position)(struct pipe_context *context,
                               unsigned sample_count,
                               unsigned sample_index,
                               float *out_value);

   /**
    * Query a timestamp in nanoseconds.  This is completely equivalent to
    * pipe_screen::get_timestamp() but takes a context handle for drivers
    * that require a context.
    */
   uint64_t (*get_timestamp)(struct pipe_context *);

   /**
    * Flush the resource cache, so that the resource can be used
    * by an external client. Possible usage:
    * - flushing a resource before presenting it on the screen
    * - flushing a resource if some other process or device wants to use it
    * This shouldn't be used to flush caches if the resource is only managed
    * by a single pipe_screen and is not shared with another process.
    * (i.e. you shouldn't use it to flush caches explicitly if you want to e.g.
    * use the resource for texturing)
    */
   void (*flush_resource)(struct pipe_context *ctx,
                          struct pipe_resource *resource);

   /**
    * Invalidate the contents of the resource. This is used to
    *
    * (1) implement EGL's semantic of undefined depth/stencil
    * contenst after a swapbuffers.  This allows a tiled renderer (for
    * example) to not store the depth buffer.
    *
    * (2) implement GL's InvalidateBufferData. For backwards compatibility,
    * you must only rely on the usability for this purpose when
    * PIPE_CAP_INVALIDATE_BUFFER is enabled.
    */
   void (*invalidate_resource)(struct pipe_context *ctx,
                               struct pipe_resource *resource);

   /**
    * Return information about unexpected device resets.
    */
   enum pipe_reset_status (*get_device_reset_status)(struct pipe_context *ctx);

   /**
    * Sets the reset status callback. If the pointer is null, then no callback
    * is set, otherwise a copy of the data should be made.
    */
   void (*set_device_reset_callback)(struct pipe_context *ctx,
                                     const struct pipe_device_reset_callback *cb);

   /**
    * Dump driver-specific debug information into a stream. This is
    * used by debugging tools.
    *
    * \param ctx        pipe context
    * \param stream     where the output should be written to
    * \param flags      a mask of PIPE_DUMP_* flags
    */
   void (*dump_debug_state)(struct pipe_context *ctx, FILE *stream,
                            unsigned flags);

   /**
    * Set the log context to which the driver should write internal debug logs
    * (internal states, command streams).
    *
    * The caller must ensure that the log context is destroyed and reset to
    * NULL before the pipe context is destroyed, and that log context functions
    * are only called from the driver thread.
    *
    * \param ctx pipe context
    * \param log logging context
    */
   void (*set_log_context)(struct pipe_context *ctx, struct u_log_context *log);

   /**
    * Emit string marker in cmdstream
    */
   void (*emit_string_marker)(struct pipe_context *ctx,
                              const char *string,
                              int len);

   /**
    * Generate mipmap.
    * \return TRUE if mipmap generation succeeds, FALSE otherwise
    */
   boolean (*generate_mipmap)(struct pipe_context *ctx,
                              struct pipe_resource *resource,
                              enum pipe_format format,
                              unsigned base_level,
                              unsigned last_level,
                              unsigned first_layer,
                              unsigned last_layer);

   /**
    * Create a 64-bit texture handle.
    *
    * \param ctx        pipe context
    * \param view       pipe sampler view object
    * \param state      pipe sampler state template
    * \return           a 64-bit texture handle if success, 0 otherwise
    */
   uint64_t (*create_texture_handle)(struct pipe_context *ctx,
                                     struct pipe_sampler_view *view,
                                     const struct pipe_sampler_state *state);

   /**
    * Delete a texture handle.
    *
    * \param ctx        pipe context
    * \param handle     64-bit texture handle
    */
   void (*delete_texture_handle)(struct pipe_context *ctx, uint64_t handle);

   /**
    * Make a texture handle resident.
    *
    * \param ctx        pipe context
    * \param handle     64-bit texture handle
    * \param resident   TRUE for resident, FALSE otherwise
    */
   void (*make_texture_handle_resident)(struct pipe_context *ctx,
                                        uint64_t handle, bool resident);

   /**
    * Create a 64-bit image handle.
    *
    * \param ctx        pipe context
    * \param image      pipe image view template
    * \return           a 64-bit image handle if success, 0 otherwise
    */
   uint64_t (*create_image_handle)(struct pipe_context *ctx,
                                   const struct pipe_image_view *image);

   /**
    * Delete an image handle.
    *
    * \param ctx        pipe context
    * \param handle     64-bit image handle
    */
   void (*delete_image_handle)(struct pipe_context *ctx, uint64_t handle);

   /**
    * Make an image handle resident.
    *
    * \param ctx        pipe context
    * \param handle     64-bit image handle
    * \param access     GL_READ_ONLY, GL_WRITE_ONLY or GL_READ_WRITE
    * \param resident   TRUE for resident, FALSE otherwise
    */
   void (*make_image_handle_resident)(struct pipe_context *ctx, uint64_t handle,
                                      unsigned access, bool resident);
};


#ifdef __cplusplus
}
#endif

#endif /* PIPE_CONTEXT_H */
