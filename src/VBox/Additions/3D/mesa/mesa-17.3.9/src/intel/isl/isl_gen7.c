/*
 * Copyright 2015 Intel Corporation
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice (including the next
 *  paragraph) shall be included in all copies or substantial portions of the
 *  Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 *  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 */

#include "isl_gen7.h"
#include "isl_priv.h"

static bool
gen7_format_needs_valign2(const struct isl_device *dev,
                          enum isl_format format)
{
   assert(ISL_DEV_GEN(dev) == 7);

   /* From the Ivybridge PRM (2012-05-31), Volume 4, Part 1, Section 2.12.1,
    * RENDER_SURFACE_STATE Surface Vertical Alignment:
    *
    *    - Value of 1 [VALIGN_4] is not supported for format YCRCB_NORMAL
    *      (0x182), YCRCB_SWAPUVY (0x183), YCRCB_SWAPUV (0x18f), YCRCB_SWAPY
    *      (0x190)
    *
    *    - VALIGN_4 is not supported for surface format R32G32B32_FLOAT.
    */
   return isl_format_is_yuv(format) ||
          format == ISL_FORMAT_R32G32B32_FLOAT;
}

bool
isl_gen7_choose_msaa_layout(const struct isl_device *dev,
                            const struct isl_surf_init_info *info,
                            enum isl_tiling tiling,
                            enum isl_msaa_layout *msaa_layout)
{
   bool require_array = false;
   bool require_interleaved = false;

   assert(ISL_DEV_GEN(dev) == 7);
   assert(info->samples >= 1);

   if (info->samples == 1) {
      *msaa_layout = ISL_MSAA_LAYOUT_NONE;
      return true;
   }

   if (!isl_format_supports_multisampling(dev->info, info->format))
      return false;

   /* From the Ivybridge PRM, Volume 4 Part 1 p73, SURFACE_STATE, Number of
    * Multisamples:
    *
    *    - If this field is any value other than MULTISAMPLECOUNT_1, the
    *      Surface Type must be SURFTYPE_2D.
    *
    *    - If this field is any value other than MULTISAMPLECOUNT_1, Surface
    *      Min LOD, Mip Count / LOD, and Resource Min LOD must be set to zero
    */
   if (info->dim != ISL_SURF_DIM_2D)
      return false;
   if (info->levels > 1)
      return false;

   /* The Ivyrbridge PRM insists twice that signed integer formats cannot be
    * multisampled.
    *
    * From the Ivybridge PRM, Volume 4 Part 1 p73, SURFACE_STATE, Number of
    * Multisamples:
    *
    *    - This field must be set to MULTISAMPLECOUNT_1 for SINT MSRTs when
    *      all RT channels are not written.
    *
    * And errata from the Ivybridge PRM, Volume 4 Part 1 p77,
    * RENDER_SURFACE_STATE, MCS Enable:
    *
    *   This field must be set to 0 [MULTISAMPLECOUNT_1] for all SINT MSRTs
    *   when all RT channels are not written.
    *
    * Note that the above SINT restrictions apply only to *MSRTs* (that is,
    * *multisampled* render targets). The restrictions seem to permit an MCS
    * if the render target is singlesampled.
    *
    * Moreover, empirically it looks that hardware can render multisampled
    * surfaces with RGBA8I, RGBA16I and RGBA32I.
    */

   /* Multisampling requires vertical alignment of four. */
   if (info->samples > 1 && gen7_format_needs_valign2(dev, info->format))
      return false;

   /* More obvious restrictions */
   if (isl_surf_usage_is_display(info->usage))
      return false;
   if (tiling == ISL_TILING_LINEAR)
      return false;

   /* From the Ivybridge PRM, Volume 4 Part 1 p72, SURFACE_STATE, Multisampled
    * Suface Storage Format:
    *
    *    +---------------------+----------------------------------------------------------------+
    *    | MSFMT_MSS           | Multsampled surface was/is rendered as a render target         |
    *    | MSFMT_DEPTH_STENCIL | Multisampled surface was rendered as a depth or stencil buffer |
    *    +---------------------+----------------------------------------------------------------+
    *
    * In the table above, MSFMT_MSS refers to ISL_MSAA_LAYOUT_ARRAY, and
    * MSFMT_DEPTH_STENCIL refers to ISL_MSAA_LAYOUT_INTERLEAVED.
    */
   if (isl_surf_usage_is_depth_or_stencil(info->usage) ||
       (info->usage & ISL_SURF_USAGE_HIZ_BIT))
      require_interleaved = true;

   /* From the Ivybridge PRM, Volume 4 Part 1 p72, SURFACE_STATE, Multisampled
    * Suface Storage Format:
    *
    *    If the surface’s Number of Multisamples is MULTISAMPLECOUNT_8, Width
    *    is >= 8192 (meaning the actual surface width is >= 8193 pixels), this
    *    field must be set to MSFMT_MSS.
    */
   if (info->samples == 8 && info->width > 8192)
      require_array = true;

   /* From the Ivybridge PRM, Volume 4 Part 1 p72, SURFACE_STATE, Multisampled
    * Suface Storage Format:
    *
    *    If the surface’s Number of Multisamples is MULTISAMPLECOUNT_8,
    *    ((Depth+1) * (Height+1)) is > 4,194,304, OR if the surface’s Number
    *    of Multisamples is MULTISAMPLECOUNT_4, ((Depth+1) * (Height+1)) is
    *    > 8,388,608, this field must be set to MSFMT_DEPTH_STENCIL.
    */
   if ((info->samples == 8 && info->height > 4194304u) ||
       (info->samples == 4 && info->height > 8388608u))
      require_interleaved = true;

   /* From the Ivybridge PRM, Volume 4 Part 1 p72, SURFACE_STATE, Multisampled
    * Suface Storage Format:
    *
    *    This field must be set to MSFMT_DEPTH_STENCIL if Surface Format is
    *    one of the following: I24X8_UNORM, L24X8_UNORM, A24X8_UNORM, or
    *    R24_UNORM_X8_TYPELESS.
    */
   if (info->format == ISL_FORMAT_I24X8_UNORM ||
       info->format == ISL_FORMAT_L24X8_UNORM ||
       info->format == ISL_FORMAT_A24X8_UNORM ||
       info->format == ISL_FORMAT_R24_UNORM_X8_TYPELESS)
      require_interleaved = true;

   if (require_array && require_interleaved)
      return false;

   if (require_interleaved) {
      *msaa_layout = ISL_MSAA_LAYOUT_INTERLEAVED;
      return true;
   }

   /* Default to the array layout because it permits multisample
    * compression.
    */
   *msaa_layout = ISL_MSAA_LAYOUT_ARRAY;
   return true;
}

/**
 * @brief Filter out tiling flags that are incompatible with the surface.
 *
 * The resultant outgoing @a flags is a subset of the incoming @a flags. The
 * outgoing flags may be empty (0x0) if the incoming flags were too
 * restrictive.
 *
 * For example, if the surface will be used for a display
 * (ISL_SURF_USAGE_DISPLAY_BIT), then this function filters out all tiling
 * flags except ISL_TILING_X_BIT and ISL_TILING_LINEAR_BIT.
 */
void
isl_gen6_filter_tiling(const struct isl_device *dev,
                       const struct isl_surf_init_info *restrict info,
                       isl_tiling_flags_t *flags)
{
   /* IVB+ requires separate stencil */
   assert(ISL_DEV_USE_SEPARATE_STENCIL(dev));

   /* Clear flags unsupported on this hardware */
   if (ISL_DEV_GEN(dev) < 9) {
      *flags &= ~ISL_TILING_Yf_BIT;
      *flags &= ~ISL_TILING_Ys_BIT;
   }

   /* And... clear the Yf and Ys bits anyway because Anvil doesn't support
    * them yet.
    */
   *flags &= ~ISL_TILING_Yf_BIT; /* FINISHME[SKL]: Support Yf */
   *flags &= ~ISL_TILING_Ys_BIT; /* FINISHME[SKL]: Support Ys */

   if (isl_surf_usage_is_depth(info->usage)) {
      /* Depth requires Y. */
      *flags &= ISL_TILING_ANY_Y_MASK;
   }

   /* Separate stencil requires W tiling, and W tiling requires separate
    * stencil.
    */
   if (isl_surf_usage_is_stencil(info->usage)) {
      *flags &= ISL_TILING_W_BIT;
   } else {
      *flags &= ~ISL_TILING_W_BIT;
   }

   /* From the SKL+ PRMs, RENDER_SURFACE_STATE:TileMode,
    *    If Surface Format is ASTC*, this field must be TILEMODE_YMAJOR.
    */
   if (isl_format_get_layout(info->format)->txc == ISL_TXC_ASTC)
      *flags &= ISL_TILING_Y0_BIT;

   /* MCS buffers are always Y-tiled */
   if (isl_format_get_layout(info->format)->txc == ISL_TXC_MCS)
      *flags &= ISL_TILING_Y0_BIT;

   if (info->usage & (ISL_SURF_USAGE_DISPLAY_ROTATE_90_BIT |
                      ISL_SURF_USAGE_DISPLAY_ROTATE_180_BIT |
                      ISL_SURF_USAGE_DISPLAY_ROTATE_270_BIT)) {
      assert(*flags & ISL_SURF_USAGE_DISPLAY_BIT);
      isl_finishme("%s:%s: handle rotated display surfaces",
                   __FILE__, __func__);
   }

   if (info->usage & (ISL_SURF_USAGE_DISPLAY_FLIP_X_BIT |
                      ISL_SURF_USAGE_DISPLAY_FLIP_Y_BIT)) {
      assert(*flags & ISL_SURF_USAGE_DISPLAY_BIT);
      isl_finishme("%s:%s: handle flipped display surfaces",
                   __FILE__, __func__);
   }

   if (info->usage & ISL_SURF_USAGE_DISPLAY_BIT) {
      /* Before Skylake, the display engine does not accept Y */
      /* FINISHME[SKL]: Y tiling for display surfaces */
      *flags &= (ISL_TILING_LINEAR_BIT | ISL_TILING_X_BIT);
   }

   if (info->samples > 1) {
      /* From the Sandybridge PRM, Volume 4 Part 1, SURFACE_STATE Tiled
       * Surface:
       *
       *   For multisample render targets, this field must be 1 (true). MSRTs
       *   can only be tiled.
       *
       * From the Broadwell PRM >> Volume2d: Command Structures >>
       * RENDER_SURFACE_STATE Tile Mode:
       *
       *   If Number of Multisamples is not MULTISAMPLECOUNT_1, this field
       *   must be YMAJOR.
       *
       * As usual, though, stencil is special and requires W-tiling.
       */
      *flags &= (ISL_TILING_ANY_Y_MASK | ISL_TILING_W_BIT);
   }

   /* workaround */
   if (ISL_DEV_GEN(dev) == 7 &&
       gen7_format_needs_valign2(dev, info->format) &&
       (info->usage & ISL_SURF_USAGE_RENDER_TARGET_BIT) &&
       info->samples == 1) {
      /* Y tiling is illegal. From the Ivybridge PRM, Vol4 Part1 2.12.2.1,
       * SURFACE_STATE Surface Vertical Alignment:
       *
       *     This field must be set to VALIGN_4 for all tiled Y Render Target
       *     surfaces.
       */
      *flags &= ~ISL_TILING_Y0_BIT;
   }

   /* From the Sandybridge PRM, Volume 1, Part 2, page 32:
    *
    *    "NOTE: 128BPE Format Color Buffer ( render target ) MUST be either
    *     TileX or Linear."
    *
    * This is necessary all the way back to 965, but is permitted on Gen7+.
    */
   if (ISL_DEV_GEN(dev) < 7 && isl_format_get_layout(info->format)->bpb >= 128)
      *flags &= ~ISL_TILING_Y0_BIT;
}

void
isl_gen7_choose_image_alignment_el(const struct isl_device *dev,
                                   const struct isl_surf_init_info *restrict info,
                                   enum isl_tiling tiling,
                                   enum isl_dim_layout dim_layout,
                                   enum isl_msaa_layout msaa_layout,
                                   struct isl_extent3d *image_align_el)
{
   assert(ISL_DEV_GEN(dev) == 7);

   /* Handled by isl_choose_image_alignment_el */
   assert(info->format != ISL_FORMAT_HIZ);

   /* IVB+ does not support combined depthstencil. */
   assert(!isl_surf_usage_is_depth_and_stencil(info->usage));

   /* From the Ivy Bridge PRM, Vol. 2, Part 2, Section 6.18.4.4,
    * "Alignment unit size", the alignment parameters are summarized in the
    * following table:
    *
    *     Surface Defined By | Surface Format  | Align Width | Align Height
    *    --------------------+-----------------+-------------+--------------
    *       DEPTH_BUFFER     |   D16_UNORM     |      8      |      4
    *                        |     other       |      4      |      4
    *    --------------------+-----------------+-------------+--------------
    *       STENCIL_BUFFER   |      N/A        |      8      |      8
    *    --------------------+-----------------+-------------+--------------
    *       SURFACE_STATE    | BC*, ETC*, EAC* |      4      |      4
    *                        |      FXT1       |      8      |      4
    *                        |   all others    |   HALIGN    |   VALIGN
    *    -------------------------------------------------------------------
    */
   if (isl_surf_usage_is_depth(info->usage)) {
      *image_align_el = info->format == ISL_FORMAT_R16_UNORM ?
                        isl_extent3d(8, 4, 1) : isl_extent3d(4, 4, 1);
      return;
   } else if (isl_surf_usage_is_stencil(info->usage)) {
      *image_align_el = isl_extent3d(8, 8, 1);
      return;
   } else if (isl_format_is_compressed(info->format)) {
      /* Compressed formats all have alignment equal to block size. */
      *image_align_el = isl_extent3d(1, 1, 1);
      return;
   }

   /* Everything after this point is in the "set by Surface Horizontal or
    * Vertical Alignment" case.  Now it's just a matter of applying
    * restrictions.
    */

   /* There are no restrictions on halign beyond what's given in the table
    * above.  We set it to the minimum value of 4 because that uses the least
    * memory.
    */
   const uint32_t halign = 4;

   bool require_valign4 = false;

   /* From the Ivybridge PRM, Volume 4, Part 1, Section 2.12.1:
    * RENDER_SURFACE_STATE Surface Vertical Alignment:
    *
    *    * This field is intended to be set to VALIGN_4 if the surface was
    *      rendered as a depth buffer,
    *
    *    * for a multisampled (4x) render target, or for a multisampled (8x)
    *      render target, since these surfaces support only alignment of 4.
    *
    *    * This field must be set to VALIGN_4 for all tiled Y Render Target
    *      surfaces
    *
    *    * Value of 1 is not supported for format YCRCB_NORMAL (0x182),
    *      YCRCB_SWAPUVY (0x183), YCRCB_SWAPUV (0x18f), YCRCB_SWAPY (0x190)
    *
    *    * If Number of Multisamples is not MULTISAMPLECOUNT_1, this field
    *      must be set to VALIGN_4."
    *
    * The first restriction is already handled by the table above and the
    * second restriction is redundant with the fifth.
    */
   if (info->samples > 1)
      require_valign4 = true;

   if (tiling == ISL_TILING_Y0 &&
       (info->usage & ISL_SURF_USAGE_RENDER_TARGET_BIT))
      require_valign4 = true;

   assert(!(require_valign4 && gen7_format_needs_valign2(dev, info->format)));

   /* We default to VALIGN_2 because it uses the least memory. */
   const uint32_t valign = require_valign4 ? 4 : 2;

   *image_align_el = isl_extent3d(halign, valign, 1);
}
