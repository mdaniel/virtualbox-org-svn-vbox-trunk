/*
 * Copyright 2003 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _INTEL_TEX_OBJ_H
#define _INTEL_TEX_OBJ_H

#include "swrast/s_context.h"

#ifdef __cplusplus
extern "C" {
#endif

struct intel_texture_object
{
   struct gl_texture_object base;

   /* This is a mirror of base._MaxLevel, updated at validate time,
    * except that we don't bother with the non-base levels for
    * non-mipmapped textures.
    */
   unsigned int _MaxLevel;

   unsigned int validated_first_level;
   unsigned int validated_last_level;

   /* The miptree of pixel data for the texture (if !needs_validate).  After
    * validation, the images will also have references to the same mt.
    */
   struct intel_mipmap_tree *mt;

   /**
    * Set when mipmap trees in the texture images of this texture object
    * might not all be the mipmap tree above.
    */
   bool needs_validate;

   /* Mesa format for the validated texture object. For non-views this
    * will always be the same as mt->format. For views, it may differ
    * since the mt is shared across views with differing formats.
    */
   mesa_format _Format;

   const struct intel_image_format *planar_format;
};


/**
 * intel_texture_image is a subclass of swrast_texture_image because we
 * sometimes fall back to using the swrast module for software rendering.
 */
struct intel_texture_image
{
   struct swrast_texture_image base;

   /* If intelImage->mt != NULL, image data is stored here.
    * Else if intelImage->base.Buffer != NULL, image is stored there.
    * Else there is no image data.
    */
   struct intel_mipmap_tree *mt;
};

static inline struct intel_texture_object *
intel_texture_object(struct gl_texture_object *obj)
{
   return (struct intel_texture_object *) obj;
}

static inline struct intel_texture_image *
intel_texture_image(struct gl_texture_image *img)
{
   return (struct intel_texture_image *) img;
}

#ifdef __cplusplus
}
#endif

#endif /* _INTEL_TEX_OBJ_H */
