/**************************************************************************
 *
 * Copyright 2009 Younes Manton.
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

#ifndef PIPE_VIDEO_CONTEXT_H
#define PIPE_VIDEO_CONTEXT_H

#include "pipe/p_video_state.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pipe_screen;
struct pipe_surface;
struct pipe_macroblock;
struct pipe_picture_desc;
struct pipe_fence_handle;

/**
 * Gallium video codec for a specific format/profile
 */
struct pipe_video_codec
{
   struct pipe_context *context;

   enum pipe_video_profile profile;
   unsigned level;
   enum pipe_video_entrypoint entrypoint;
   enum pipe_video_chroma_format chroma_format;
   unsigned width;
   unsigned height;
   unsigned max_references;
   bool expect_chunked_decode;

   /**
    * destroy this video decoder
    */
   void (*destroy)(struct pipe_video_codec *codec);

   /**
    * start decoding of a new frame
    */
   void (*begin_frame)(struct pipe_video_codec *codec,
                       struct pipe_video_buffer *target,
                       struct pipe_picture_desc *picture);

   /**
    * decode a macroblock
    */
   void (*decode_macroblock)(struct pipe_video_codec *codec,
                             struct pipe_video_buffer *target,
                             struct pipe_picture_desc *picture,
                             const struct pipe_macroblock *macroblocks,
                             unsigned num_macroblocks);

   /**
    * decode a bitstream
    */
   void (*decode_bitstream)(struct pipe_video_codec *codec,
                            struct pipe_video_buffer *target,
                            struct pipe_picture_desc *picture,
                            unsigned num_buffers,
                            const void * const *buffers,
                            const unsigned *sizes);

   /**
    * encode to a bitstream
    */
   void (*encode_bitstream)(struct pipe_video_codec *codec,
                            struct pipe_video_buffer *source,
                            struct pipe_resource *destination,
                            void **feedback);

   /**
    * Perform post-process effect
    */
   void (*process_frame)(struct pipe_video_codec *codec,
                         struct pipe_video_buffer *source,
                         const struct pipe_vpp_desc *process_properties);

   /**
    * end decoding of the current frame
    */
   void (*end_frame)(struct pipe_video_codec *codec,
                     struct pipe_video_buffer *target,
                     struct pipe_picture_desc *picture);

   /**
    * flush any outstanding command buffers to the hardware
    * should be called before a video_buffer is acessed by the gallium frontend again
    */
   void (*flush)(struct pipe_video_codec *codec);

   /**
    * get encoder feedback
    */
   void (*get_feedback)(struct pipe_video_codec *codec,
                        void *feedback,
                        unsigned *size,
                        struct pipe_enc_feedback_metadata* metadata /* opt NULL */);

   /**
    * Get decoder fence.
    *
    * Can be used to query the status of the previous decode job denoted by
    * 'fence' given 'timeout'.
    *
    * A pointer to a fence pointer can be passed to the codecs before the
    * end_frame vfunc and the codec should then be responsible for allocating a
    * fence on command stream submission.
    */
   int (*get_decoder_fence)(struct pipe_video_codec *codec,
                            struct pipe_fence_handle *fence,
                            uint64_t timeout);

   /**
    * Get processor fence.
    *
    * Can be used to query the status of the previous process job denoted by
    * 'fence' given 'timeout'.
    *
    * A pointer to a fence pointer can be passed to the codecs before the
    * end_frame vfunc and the codec should then be responsible for allocating a
    * fence on command stream submission.
    */
   int (*get_processor_fence)(struct pipe_video_codec *codec,
                              struct pipe_fence_handle *fence,
                              uint64_t timeout);

   /**
    * Gets a weak reference to a feedback fence.
    *
    * Can be used to wait on the pipe_fence_handle directly instead
    * of waiting on the get_feedback blocking call.
    *
    * Returns NULL if the feedback parameter does not have
    * a valid in-flight submitted frame
    */
   struct pipe_fence_handle* (*get_feedback_fence)(struct pipe_video_codec *codec,
                                                   void *feedback);

   /**
    * Destroy fence.
    */
   void (*destroy_fence)(struct pipe_video_codec *codec,
                         struct pipe_fence_handle *fence);

   /**
    * Update target buffer address.
    *
    * Due to reallocation, target buffer address has changed, and the
    * changed buffer will need to update to decoder so that when this buffer
    * used as a reference frame, decoder can obtain its recorded information.
    * Failed updating this buffer will miss reference frames and
    * cause image corruption in the sebsequent output.
    * If no target buffer change, this call is not necessary.
    */
   void (*update_decoder_target)(struct pipe_video_codec *codec,
                                 struct pipe_video_buffer *old,
                                 struct pipe_video_buffer *updated);

   /**
    * Gets the bitstream headers for a given pipe_picture_desc
    * of an encode operation
    *
    * User passes a buffer and its allocated size and
    * driver writes the bitstream headers in the buffer,
    * updating the size parameter as well.
    *
    * Returns 0 on success or an errno error code otherwise.
    * such as ENOMEM if the buffer passed was not big enough
    */
   int (*get_encode_headers)(struct pipe_video_codec *codec,
                              struct pipe_picture_desc *picture,
                              void* bitstream_buf,
                              unsigned *size);
};

/**
 * output for decoding / input for displaying
 */
struct pipe_video_buffer
{
   struct pipe_context *context;

   enum pipe_format buffer_format;
   unsigned width;
   unsigned height;
   bool interlaced;
   unsigned bind;

   /**
    * destroy this video buffer
    */
   void (*destroy)(struct pipe_video_buffer *buffer);

   /**
    * get an individual resource for each plane,
    * only returns existing resources by reference
    */
   void (*get_resources)(struct pipe_video_buffer *buffer, struct pipe_resource **resources);

   /**
    * get an individual sampler view for each plane
    */
   struct pipe_sampler_view **(*get_sampler_view_planes)(struct pipe_video_buffer *buffer);

   /**
    * get an individual sampler view for each component
    */
   struct pipe_sampler_view **(*get_sampler_view_components)(struct pipe_video_buffer *buffer);

   /**
    * get an individual surfaces for each plane
    */
   struct pipe_surface **(*get_surfaces)(struct pipe_video_buffer *buffer);

   /*
    * auxiliary associated data
    */
   void *associated_data;

   /*
    * codec where the associated data came from
    */
   struct pipe_video_codec *codec;

   /*
    * destroy the associated data
    */
   void (*destroy_associated_data)(void *associated_data);

   /*
    * encoded frame statistics for this particular picture
    */
   void *statistics_data;
};

#ifdef __cplusplus
}
#endif

#endif /* PIPE_VIDEO_CONTEXT_H */
