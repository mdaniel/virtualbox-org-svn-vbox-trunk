/*
 * Copyright 2010 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pipe/p_defines.h"

#include "tgsi/tgsi_ureg.h"

#include "nvc0/nvc0_context.h"

#include "codegen/nv50_ir_driver.h"
#include "nvc0/nve4_compute.h"

/* NOTE: Using a[0x270] in FP may cause an error even if we're using less than
 * 124 scalar varying values.
 */
static uint32_t
nvc0_shader_input_address(unsigned sn, unsigned si)
{
   switch (sn) {
   case TGSI_SEMANTIC_TESSOUTER:    return 0x000 + si * 0x4;
   case TGSI_SEMANTIC_TESSINNER:    return 0x010 + si * 0x4;
   case TGSI_SEMANTIC_PATCH:        return 0x020 + si * 0x10;
   case TGSI_SEMANTIC_PRIMID:       return 0x060;
   case TGSI_SEMANTIC_LAYER:        return 0x064;
   case TGSI_SEMANTIC_VIEWPORT_INDEX:return 0x068;
   case TGSI_SEMANTIC_PSIZE:        return 0x06c;
   case TGSI_SEMANTIC_POSITION:     return 0x070;
   case TGSI_SEMANTIC_GENERIC:      return 0x080 + si * 0x10;
   case TGSI_SEMANTIC_FOG:          return 0x2e8;
   case TGSI_SEMANTIC_COLOR:        return 0x280 + si * 0x10;
   case TGSI_SEMANTIC_BCOLOR:       return 0x2a0 + si * 0x10;
   case TGSI_SEMANTIC_CLIPDIST:     return 0x2c0 + si * 0x10;
   case TGSI_SEMANTIC_CLIPVERTEX:   return 0x270;
   case TGSI_SEMANTIC_PCOORD:       return 0x2e0;
   case TGSI_SEMANTIC_TESSCOORD:    return 0x2f0;
   case TGSI_SEMANTIC_INSTANCEID:   return 0x2f8;
   case TGSI_SEMANTIC_VERTEXID:     return 0x2fc;
   case TGSI_SEMANTIC_TEXCOORD:     return 0x300 + si * 0x10;
   default:
      assert(!"invalid TGSI input semantic");
      return ~0;
   }
}

static uint32_t
nvc0_shader_output_address(unsigned sn, unsigned si)
{
   switch (sn) {
   case TGSI_SEMANTIC_TESSOUTER:     return 0x000 + si * 0x4;
   case TGSI_SEMANTIC_TESSINNER:     return 0x010 + si * 0x4;
   case TGSI_SEMANTIC_PATCH:         return 0x020 + si * 0x10;
   case TGSI_SEMANTIC_PRIMID:        return 0x060;
   case TGSI_SEMANTIC_LAYER:         return 0x064;
   case TGSI_SEMANTIC_VIEWPORT_INDEX:return 0x068;
   case TGSI_SEMANTIC_PSIZE:         return 0x06c;
   case TGSI_SEMANTIC_POSITION:      return 0x070;
   case TGSI_SEMANTIC_GENERIC:       return 0x080 + si * 0x10;
   case TGSI_SEMANTIC_FOG:           return 0x2e8;
   case TGSI_SEMANTIC_COLOR:         return 0x280 + si * 0x10;
   case TGSI_SEMANTIC_BCOLOR:        return 0x2a0 + si * 0x10;
   case TGSI_SEMANTIC_CLIPDIST:      return 0x2c0 + si * 0x10;
   case TGSI_SEMANTIC_CLIPVERTEX:    return 0x270;
   case TGSI_SEMANTIC_TEXCOORD:      return 0x300 + si * 0x10;
   /* case TGSI_SEMANTIC_VIEWPORT_MASK: return 0x3a0; */
   case TGSI_SEMANTIC_EDGEFLAG:      return ~0;
   default:
      assert(!"invalid TGSI output semantic");
      return ~0;
   }
}

static int
nvc0_vp_assign_input_slots(struct nv50_ir_prog_info *info)
{
   unsigned i, c, n;

   for (n = 0, i = 0; i < info->numInputs; ++i) {
      switch (info->in[i].sn) {
      case TGSI_SEMANTIC_INSTANCEID: /* for SM4 only, in TGSI they're SVs */
      case TGSI_SEMANTIC_VERTEXID:
         info->in[i].mask = 0x1;
         info->in[i].slot[0] =
            nvc0_shader_input_address(info->in[i].sn, 0) / 4;
         continue;
      default:
         break;
      }
      for (c = 0; c < 4; ++c)
         info->in[i].slot[c] = (0x80 + n * 0x10 + c * 0x4) / 4;
      ++n;
   }

   return 0;
}

static int
nvc0_sp_assign_input_slots(struct nv50_ir_prog_info *info)
{
   unsigned offset;
   unsigned i, c;

   for (i = 0; i < info->numInputs; ++i) {
      offset = nvc0_shader_input_address(info->in[i].sn, info->in[i].si);

      for (c = 0; c < 4; ++c)
         info->in[i].slot[c] = (offset + c * 0x4) / 4;
   }

   return 0;
}

static int
nvc0_fp_assign_output_slots(struct nv50_ir_prog_info *info)
{
   unsigned count = info->prop.fp.numColourResults * 4;
   unsigned i, c;

   for (i = 0; i < info->numOutputs; ++i)
      if (info->out[i].sn == TGSI_SEMANTIC_COLOR)
         for (c = 0; c < 4; ++c)
            info->out[i].slot[c] = info->out[i].si * 4 + c;

   if (info->io.sampleMask < PIPE_MAX_SHADER_OUTPUTS)
      info->out[info->io.sampleMask].slot[0] = count++;
   else
   if (info->target >= 0xe0)
      count++; /* on Kepler, depth is always last colour reg + 2 */

   if (info->io.fragDepth < PIPE_MAX_SHADER_OUTPUTS)
      info->out[info->io.fragDepth].slot[2] = count;

   return 0;
}

static int
nvc0_sp_assign_output_slots(struct nv50_ir_prog_info *info)
{
   unsigned offset;
   unsigned i, c;

   for (i = 0; i < info->numOutputs; ++i) {
      offset = nvc0_shader_output_address(info->out[i].sn, info->out[i].si);

      for (c = 0; c < 4; ++c)
         info->out[i].slot[c] = (offset + c * 0x4) / 4;
   }

   return 0;
}

static int
nvc0_program_assign_varying_slots(struct nv50_ir_prog_info *info)
{
   int ret;

   if (info->type == PIPE_SHADER_VERTEX)
      ret = nvc0_vp_assign_input_slots(info);
   else
      ret = nvc0_sp_assign_input_slots(info);
   if (ret)
      return ret;

   if (info->type == PIPE_SHADER_FRAGMENT)
      ret = nvc0_fp_assign_output_slots(info);
   else
      ret = nvc0_sp_assign_output_slots(info);
   return ret;
}

static inline void
nvc0_vtgp_hdr_update_oread(struct nvc0_program *vp, uint8_t slot)
{
   uint8_t min = (vp->hdr[4] >> 12) & 0xff;
   uint8_t max = (vp->hdr[4] >> 24);

   min = MIN2(min, slot);
   max = MAX2(max, slot);

   vp->hdr[4] = (max << 24) | (min << 12);
}

/* Common part of header generation for VP, TCP, TEP and GP. */
static int
nvc0_vtgp_gen_header(struct nvc0_program *vp, struct nv50_ir_prog_info *info)
{
   unsigned i, c, a;

   for (i = 0; i < info->numInputs; ++i) {
      if (info->in[i].patch)
         continue;
      for (c = 0; c < 4; ++c) {
         a = info->in[i].slot[c];
         if (info->in[i].mask & (1 << c))
            vp->hdr[5 + a / 32] |= 1 << (a % 32);
      }
   }

   for (i = 0; i < info->numOutputs; ++i) {
      if (info->out[i].patch)
         continue;
      for (c = 0; c < 4; ++c) {
         if (!(info->out[i].mask & (1 << c)))
            continue;
         assert(info->out[i].slot[c] >= 0x40 / 4);
         a = info->out[i].slot[c] - 0x40 / 4;
         vp->hdr[13 + a / 32] |= 1 << (a % 32);
         if (info->out[i].oread)
            nvc0_vtgp_hdr_update_oread(vp, info->out[i].slot[c]);
      }
   }

   for (i = 0; i < info->numSysVals; ++i) {
      switch (info->sv[i].sn) {
      case TGSI_SEMANTIC_PRIMID:
         vp->hdr[5] |= 1 << 24;
         break;
      case TGSI_SEMANTIC_INSTANCEID:
         vp->hdr[10] |= 1 << 30;
         break;
      case TGSI_SEMANTIC_VERTEXID:
         vp->hdr[10] |= 1 << 31;
         break;
      case TGSI_SEMANTIC_TESSCOORD:
         /* We don't have the mask, nor the slots populated. While this could
          * be achieved, the vast majority of the time if either of the coords
          * are read, then both will be read.
          */
         nvc0_vtgp_hdr_update_oread(vp, 0x2f0 / 4);
         nvc0_vtgp_hdr_update_oread(vp, 0x2f4 / 4);
         break;
      default:
         break;
      }
   }

   vp->vp.clip_enable = (1 << info->io.clipDistances) - 1;
   vp->vp.cull_enable =
      ((1 << info->io.cullDistances) - 1) << info->io.clipDistances;
   for (i = 0; i < info->io.cullDistances; ++i)
      vp->vp.clip_mode |= 1 << ((info->io.clipDistances + i) * 4);

   if (info->io.genUserClip < 0)
      vp->vp.num_ucps = PIPE_MAX_CLIP_PLANES + 1; /* prevent rebuilding */

   return 0;
}

static int
nvc0_vp_gen_header(struct nvc0_program *vp, struct nv50_ir_prog_info *info)
{
   vp->hdr[0] = 0x20061 | (1 << 10);
   vp->hdr[4] = 0xff000;

   return nvc0_vtgp_gen_header(vp, info);
}

static void
nvc0_tp_get_tess_mode(struct nvc0_program *tp, struct nv50_ir_prog_info *info)
{
   if (info->prop.tp.outputPrim == PIPE_PRIM_MAX) {
      tp->tp.tess_mode = ~0;
      return;
   }
   switch (info->prop.tp.domain) {
   case PIPE_PRIM_LINES:
      tp->tp.tess_mode = NVC0_3D_TESS_MODE_PRIM_ISOLINES;
      break;
   case PIPE_PRIM_TRIANGLES:
      tp->tp.tess_mode = NVC0_3D_TESS_MODE_PRIM_TRIANGLES;
      break;
   case PIPE_PRIM_QUADS:
      tp->tp.tess_mode = NVC0_3D_TESS_MODE_PRIM_QUADS;
      break;
   default:
      tp->tp.tess_mode = ~0;
      return;
   }

   /* It seems like lines want the "CW" bit to indicate they're connected, and
    * spit out errors in dmesg when the "CONNECTED" bit is set.
    */
   if (info->prop.tp.outputPrim != PIPE_PRIM_POINTS) {
      if (info->prop.tp.domain == PIPE_PRIM_LINES)
         tp->tp.tess_mode |= NVC0_3D_TESS_MODE_CW;
      else
         tp->tp.tess_mode |= NVC0_3D_TESS_MODE_CONNECTED;
   }

   /* Winding only matters for triangles/quads, not lines. */
   if (info->prop.tp.domain != PIPE_PRIM_LINES &&
       info->prop.tp.outputPrim != PIPE_PRIM_POINTS &&
       info->prop.tp.winding > 0)
      tp->tp.tess_mode |= NVC0_3D_TESS_MODE_CW;

   switch (info->prop.tp.partitioning) {
   case PIPE_TESS_SPACING_EQUAL:
      tp->tp.tess_mode |= NVC0_3D_TESS_MODE_SPACING_EQUAL;
      break;
   case PIPE_TESS_SPACING_FRACTIONAL_ODD:
      tp->tp.tess_mode |= NVC0_3D_TESS_MODE_SPACING_FRACTIONAL_ODD;
      break;
   case PIPE_TESS_SPACING_FRACTIONAL_EVEN:
      tp->tp.tess_mode |= NVC0_3D_TESS_MODE_SPACING_FRACTIONAL_EVEN;
      break;
   default:
      assert(!"invalid tessellator partitioning");
      break;
   }
}

static int
nvc0_tcp_gen_header(struct nvc0_program *tcp, struct nv50_ir_prog_info *info)
{
   unsigned opcs = 6; /* output patch constants (at least the TessFactors) */

   tcp->tp.input_patch_size = info->prop.tp.inputPatchSize;

   if (info->numPatchConstants)
      opcs = 8 + info->numPatchConstants * 4;

   tcp->hdr[0] = 0x20061 | (2 << 10);

   tcp->hdr[1] = opcs << 24;
   tcp->hdr[2] = info->prop.tp.outputPatchSize << 24;

   tcp->hdr[4] = 0xff000; /* initial min/max parallel output read address */

   nvc0_vtgp_gen_header(tcp, info);

   if (info->target >= NVISA_GM107_CHIPSET) {
      /* On GM107+, the number of output patch components has moved in the TCP
       * header, but it seems like blob still also uses the old position.
       * Also, the high 8-bits are located inbetween the min/max parallel
       * field and has to be set after updating the outputs. */
      tcp->hdr[3] = (opcs & 0x0f) << 28;
      tcp->hdr[4] |= (opcs & 0xf0) << 16;
   }

   nvc0_tp_get_tess_mode(tcp, info);

   return 0;
}

static int
nvc0_tep_gen_header(struct nvc0_program *tep, struct nv50_ir_prog_info *info)
{
   tep->tp.input_patch_size = ~0;

   tep->hdr[0] = 0x20061 | (3 << 10);
   tep->hdr[4] = 0xff000;

   nvc0_vtgp_gen_header(tep, info);

   nvc0_tp_get_tess_mode(tep, info);

   tep->hdr[18] |= 0x3 << 12; /* ? */

   return 0;
}

static int
nvc0_gp_gen_header(struct nvc0_program *gp, struct nv50_ir_prog_info *info)
{
   gp->hdr[0] = 0x20061 | (4 << 10);

   gp->hdr[2] = MIN2(info->prop.gp.instanceCount, 32) << 24;

   switch (info->prop.gp.outputPrim) {
   case PIPE_PRIM_POINTS:
      gp->hdr[3] = 0x01000000;
      gp->hdr[0] |= 0xf0000000;
      break;
   case PIPE_PRIM_LINE_STRIP:
      gp->hdr[3] = 0x06000000;
      gp->hdr[0] |= 0x10000000;
      break;
   case PIPE_PRIM_TRIANGLE_STRIP:
      gp->hdr[3] = 0x07000000;
      gp->hdr[0] |= 0x10000000;
      break;
   default:
      assert(0);
      break;
   }

   gp->hdr[4] = CLAMP(info->prop.gp.maxVertices, 1, 1024);

   return nvc0_vtgp_gen_header(gp, info);
}

#define NVC0_INTERP_FLAT          (1 << 0)
#define NVC0_INTERP_PERSPECTIVE   (2 << 0)
#define NVC0_INTERP_LINEAR        (3 << 0)
#define NVC0_INTERP_CENTROID      (1 << 2)

static uint8_t
nvc0_hdr_interp_mode(const struct nv50_ir_varying *var)
{
   if (var->linear)
      return NVC0_INTERP_LINEAR;
   if (var->flat)
      return NVC0_INTERP_FLAT;
   return NVC0_INTERP_PERSPECTIVE;
}

static int
nvc0_fp_gen_header(struct nvc0_program *fp, struct nv50_ir_prog_info *info)
{
   unsigned i, c, a, m;

   /* just 00062 on Kepler */
   fp->hdr[0] = 0x20062 | (5 << 10);
   fp->hdr[5] = 0x80000000; /* getting a trap if FRAG_COORD_UMASK.w = 0 */

   if (info->prop.fp.usesDiscard)
      fp->hdr[0] |= 0x8000;
   if (info->prop.fp.numColourResults > 1)
      fp->hdr[0] |= 0x4000;
   if (info->io.sampleMask < PIPE_MAX_SHADER_OUTPUTS)
      fp->hdr[19] |= 0x1;
   if (info->prop.fp.writesDepth) {
      fp->hdr[19] |= 0x2;
      fp->flags[0] = 0x11; /* deactivate ZCULL */
   }

   for (i = 0; i < info->numInputs; ++i) {
      m = nvc0_hdr_interp_mode(&info->in[i]);
      if (info->in[i].sn == TGSI_SEMANTIC_COLOR) {
         fp->fp.colors |= 1 << info->in[i].si;
         if (info->in[i].sc)
            fp->fp.color_interp[info->in[i].si] = m | (info->in[i].mask << 4);
      }
      for (c = 0; c < 4; ++c) {
         if (!(info->in[i].mask & (1 << c)))
            continue;
         a = info->in[i].slot[c];
         if (info->in[i].slot[0] >= (0x060 / 4) &&
             info->in[i].slot[0] <= (0x07c / 4)) {
            fp->hdr[5] |= 1 << (24 + (a - 0x060 / 4));
         } else
         if (info->in[i].slot[0] >= (0x2c0 / 4) &&
             info->in[i].slot[0] <= (0x2fc / 4)) {
            fp->hdr[14] |= (1 << (a - 0x280 / 4)) & 0x07ff0000;
         } else {
            if (info->in[i].slot[c] < (0x040 / 4) ||
                info->in[i].slot[c] > (0x380 / 4))
               continue;
            a *= 2;
            if (info->in[i].slot[0] >= (0x300 / 4))
               a -= 32;
            fp->hdr[4 + a / 32] |= m << (a % 32);
         }
      }
   }

   for (i = 0; i < info->numOutputs; ++i) {
      if (info->out[i].sn == TGSI_SEMANTIC_COLOR)
         fp->hdr[18] |= 0xf << info->out[i].slot[0];
   }

   /* There are no "regular" attachments, but the shader still needs to be
    * executed. It seems like it wants to think that it has some color
    * outputs in order to actually run.
    */
   if (info->prop.fp.numColourResults == 0 && !info->prop.fp.writesDepth)
      fp->hdr[18] |= 0xf;

   fp->fp.early_z = info->prop.fp.earlyFragTests;
   fp->fp.sample_mask_in = info->prop.fp.usesSampleMaskIn;
   fp->fp.reads_framebuffer = info->prop.fp.readsFramebuffer;
   fp->fp.post_depth_coverage = info->prop.fp.postDepthCoverage;

   /* Mark position xy and layer as read */
   if (fp->fp.reads_framebuffer)
      fp->hdr[5] |= 0x32000000;

   return 0;
}

static struct nvc0_transform_feedback_state *
nvc0_program_create_tfb_state(const struct nv50_ir_prog_info *info,
                              const struct pipe_stream_output_info *pso)
{
   struct nvc0_transform_feedback_state *tfb;
   unsigned b, i, c;

   tfb = MALLOC_STRUCT(nvc0_transform_feedback_state);
   if (!tfb)
      return NULL;
   for (b = 0; b < 4; ++b) {
      tfb->stride[b] = pso->stride[b] * 4;
      tfb->varying_count[b] = 0;
   }
   memset(tfb->varying_index, 0xff, sizeof(tfb->varying_index)); /* = skip */

   for (i = 0; i < pso->num_outputs; ++i) {
      unsigned s = pso->output[i].start_component;
      unsigned p = pso->output[i].dst_offset;
      const unsigned r = pso->output[i].register_index;
      b = pso->output[i].output_buffer;

      if (r >= info->numOutputs)
         continue;

      for (c = 0; c < pso->output[i].num_components; ++c)
         tfb->varying_index[b][p++] = info->out[r].slot[s + c];

      tfb->varying_count[b] = MAX2(tfb->varying_count[b], p);
      tfb->stream[b] = pso->output[i].stream;
   }
   for (b = 0; b < 4; ++b) // zero unused indices (looks nicer)
      for (c = tfb->varying_count[b]; c & 3; ++c)
         tfb->varying_index[b][c] = 0;

   return tfb;
}

#ifdef DEBUG
static void
nvc0_program_dump(struct nvc0_program *prog)
{
   unsigned pos;

   if (prog->type != PIPE_SHADER_COMPUTE) {
      for (pos = 0; pos < ARRAY_SIZE(prog->hdr); ++pos)
         debug_printf("HDR[%02"PRIxPTR"] = 0x%08x\n",
                      pos * sizeof(prog->hdr[0]), prog->hdr[pos]);
   }
   debug_printf("shader binary code (0x%x bytes):", prog->code_size);
   for (pos = 0; pos < prog->code_size / 4; ++pos) {
      if ((pos % 8) == 0)
         debug_printf("\n");
      debug_printf("%08x ", prog->code[pos]);
   }
   debug_printf("\n");
}
#endif

bool
nvc0_program_translate(struct nvc0_program *prog, uint16_t chipset,
                       struct pipe_debug_callback *debug)
{
   struct nv50_ir_prog_info *info;
   int ret;

   info = CALLOC_STRUCT(nv50_ir_prog_info);
   if (!info)
      return false;

   info->type = prog->type;
   info->target = chipset;
   info->bin.sourceRep = PIPE_SHADER_IR_TGSI;
   info->bin.source = (void *)prog->pipe.tokens;

#ifdef DEBUG
   info->target = debug_get_num_option("NV50_PROG_CHIPSET", chipset);
   info->optLevel = debug_get_num_option("NV50_PROG_OPTIMIZE", 3);
   info->dbgFlags = debug_get_num_option("NV50_PROG_DEBUG", 0);
#else
   info->optLevel = 3;
#endif

   info->io.genUserClip = prog->vp.num_ucps;
   info->io.auxCBSlot = 15;
   info->io.msInfoCBSlot = 15;
   info->io.ucpBase = NVC0_CB_AUX_UCP_INFO;
   info->io.drawInfoBase = NVC0_CB_AUX_DRAW_INFO;
   info->io.msInfoBase = NVC0_CB_AUX_MS_INFO;
   info->io.bufInfoBase = NVC0_CB_AUX_BUF_INFO(0);
   info->io.suInfoBase = NVC0_CB_AUX_SU_INFO(0);
   if (info->target >= NVISA_GK104_CHIPSET) {
      info->io.texBindBase = NVC0_CB_AUX_TEX_INFO(0);
      info->io.fbtexBindBase = NVC0_CB_AUX_FB_TEX_INFO;
   }

   if (prog->type == PIPE_SHADER_COMPUTE) {
      if (info->target >= NVISA_GK104_CHIPSET) {
         info->io.auxCBSlot = 7;
         info->io.msInfoCBSlot = 7;
         info->io.uboInfoBase = NVC0_CB_AUX_UBO_INFO(0);
      }
      info->prop.cp.gridInfoBase = NVC0_CB_AUX_GRID_INFO(0);
   } else {
      info->io.sampleInfoBase = NVC0_CB_AUX_SAMPLE_INFO;
   }

   info->assignSlots = nvc0_program_assign_varying_slots;

   ret = nv50_ir_generate_code(info);
   if (ret) {
      NOUVEAU_ERR("shader translation failed: %i\n", ret);
      goto out;
   }
   if (prog->type != PIPE_SHADER_COMPUTE)
      FREE(info->bin.syms);

   prog->code = info->bin.code;
   prog->code_size = info->bin.codeSize;
   prog->relocs = info->bin.relocData;
   prog->fixups = info->bin.fixupData;
   prog->num_gprs = MAX2(4, (info->bin.maxGPR + 1));
   prog->num_barriers = info->numBarriers;

   prog->vp.need_vertex_id = info->io.vertexId < PIPE_MAX_SHADER_INPUTS;
   prog->vp.need_draw_parameters = info->prop.vp.usesDrawParameters;

   if (info->io.edgeFlagOut < PIPE_MAX_ATTRIBS)
      info->out[info->io.edgeFlagOut].mask = 0; /* for headergen */
   prog->vp.edgeflag = info->io.edgeFlagIn;

   switch (prog->type) {
   case PIPE_SHADER_VERTEX:
      ret = nvc0_vp_gen_header(prog, info);
      break;
   case PIPE_SHADER_TESS_CTRL:
      ret = nvc0_tcp_gen_header(prog, info);
      break;
   case PIPE_SHADER_TESS_EVAL:
      ret = nvc0_tep_gen_header(prog, info);
      break;
   case PIPE_SHADER_GEOMETRY:
      ret = nvc0_gp_gen_header(prog, info);
      break;
   case PIPE_SHADER_FRAGMENT:
      ret = nvc0_fp_gen_header(prog, info);
      break;
   case PIPE_SHADER_COMPUTE:
      prog->cp.syms = info->bin.syms;
      prog->cp.num_syms = info->bin.numSyms;
      break;
   default:
      ret = -1;
      NOUVEAU_ERR("unknown program type: %u\n", prog->type);
      break;
   }
   if (ret)
      goto out;

   if (info->bin.tlsSpace) {
      assert(info->bin.tlsSpace < (1 << 24));
      prog->hdr[0] |= 1 << 26;
      prog->hdr[1] |= align(info->bin.tlsSpace, 0x10); /* l[] size */
      prog->need_tls = true;
   }
   /* TODO: factor 2 only needed where joinat/precont is used,
    *       and we only have to count non-uniform branches
    */
   /*
   if ((info->maxCFDepth * 2) > 16) {
      prog->hdr[2] |= (((info->maxCFDepth * 2) + 47) / 48) * 0x200;
      prog->need_tls = true;
   }
   */
   if (info->io.globalAccess)
      prog->hdr[0] |= 1 << 26;
   if (info->io.globalAccess & 0x2)
      prog->hdr[0] |= 1 << 16;
   if (info->io.fp64)
      prog->hdr[0] |= 1 << 27;

   if (prog->pipe.stream_output.num_outputs)
      prog->tfb = nvc0_program_create_tfb_state(info,
                                                &prog->pipe.stream_output);

   pipe_debug_message(debug, SHADER_INFO,
                      "type: %d, local: %d, gpr: %d, inst: %d, bytes: %d",
                      prog->type, info->bin.tlsSpace, prog->num_gprs,
                      info->bin.instructions, info->bin.codeSize);

#ifdef DEBUG
   if (debug_get_option("NV50_PROG_CHIPSET", NULL) && info->dbgFlags)
      nvc0_program_dump(prog);
#endif

out:
   FREE(info);
   return !ret;
}

static inline int
nvc0_program_alloc_code(struct nvc0_context *nvc0, struct nvc0_program *prog)
{
   struct nvc0_screen *screen = nvc0->screen;
   const bool is_cp = prog->type == PIPE_SHADER_COMPUTE;
   int ret;
   uint32_t size = prog->code_size + (is_cp ? 0 : NVC0_SHADER_HEADER_SIZE);

   /* On Fermi, SP_START_ID must be aligned to 0x40.
    * On Kepler, the first instruction must be aligned to 0x80 because
    * latency information is expected only at certain positions.
    */
   if (screen->base.class_3d >= NVE4_3D_CLASS)
      size = size + (is_cp ? 0x40 : 0x70);
   size = align(size, 0x40);

   ret = nouveau_heap_alloc(screen->text_heap, size, prog, &prog->mem);
   if (ret)
      return ret;
   prog->code_base = prog->mem->start;

   if (!is_cp) {
      if (screen->base.class_3d >= NVE4_3D_CLASS) {
         switch (prog->mem->start & 0xff) {
         case 0x40: prog->code_base += 0x70; break;
         case 0x80: prog->code_base += 0x30; break;
         case 0xc0: prog->code_base += 0x70; break;
         default:
            prog->code_base += 0x30;
            assert((prog->mem->start & 0xff) == 0x00);
            break;
         }
      }
   } else {
      if (screen->base.class_3d >= NVE4_3D_CLASS) {
         if (prog->mem->start & 0x40)
            prog->code_base += 0x40;
         assert((prog->code_base & 0x7f) == 0x00);
      }
   }

   return 0;
}

static inline void
nvc0_program_upload_code(struct nvc0_context *nvc0, struct nvc0_program *prog)
{
   struct nvc0_screen *screen = nvc0->screen;
   const bool is_cp = prog->type == PIPE_SHADER_COMPUTE;
   uint32_t code_pos = prog->code_base + (is_cp ? 0 : NVC0_SHADER_HEADER_SIZE);

   if (prog->relocs)
      nv50_ir_relocate_code(prog->relocs, prog->code, code_pos,
                            screen->lib_code->start, 0);
   if (prog->fixups) {
      nv50_ir_apply_fixups(prog->fixups, prog->code,
                           prog->fp.force_persample_interp,
                           prog->fp.flatshade,
                           0 /* alphatest */);
      for (int i = 0; i < 2; i++) {
         unsigned mask = prog->fp.color_interp[i] >> 4;
         unsigned interp = prog->fp.color_interp[i] & 3;
         if (!mask)
            continue;
         prog->hdr[14] &= ~(0xff << (8 * i));
         if (prog->fp.flatshade)
            interp = NVC0_INTERP_FLAT;
         for (int c = 0; c < 4; c++)
            if (mask & (1 << c))
               prog->hdr[14] |= interp << (2 * (4 * i + c));
      }
   }

   if (!is_cp)
      nvc0->base.push_data(&nvc0->base, screen->text, prog->code_base,
                           NV_VRAM_DOMAIN(&screen->base),
                           NVC0_SHADER_HEADER_SIZE, prog->hdr);

   nvc0->base.push_data(&nvc0->base, screen->text, code_pos,
                        NV_VRAM_DOMAIN(&screen->base), prog->code_size,
                        prog->code);
}

bool
nvc0_program_upload(struct nvc0_context *nvc0, struct nvc0_program *prog)
{
   struct nvc0_screen *screen = nvc0->screen;
   const bool is_cp = prog->type == PIPE_SHADER_COMPUTE;
   int ret;
   uint32_t size = prog->code_size + (is_cp ? 0 : NVC0_SHADER_HEADER_SIZE);

   ret = nvc0_program_alloc_code(nvc0, prog);
   if (ret) {
      struct nouveau_heap *heap = screen->text_heap;
      struct nvc0_program *progs[] = { /* Sorted accordingly to SP_START_ID */
         nvc0->compprog, nvc0->vertprog, nvc0->tctlprog,
         nvc0->tevlprog, nvc0->gmtyprog, nvc0->fragprog
      };

      /* Note that the code library, which is allocated before anything else,
       * does not have a priv pointer. We can stop once we hit it.
       */
      while (heap->next && heap->next->priv) {
         struct nvc0_program *evict = heap->next->priv;
         nouveau_heap_free(&evict->mem);
      }
      debug_printf("WARNING: out of code space, evicting all shaders.\n");

      /* Make sure to synchronize before deleting the code segment. */
      IMMED_NVC0(nvc0->base.pushbuf, NVC0_3D(SERIALIZE), 0);

      if ((screen->text->size << 1) <= (1 << 23)) {
         ret = nvc0_screen_resize_text_area(screen, screen->text->size << 1);
         if (ret) {
            NOUVEAU_ERR("Error allocating TEXT area: %d\n", ret);
            return false;
         }
         nouveau_bufctx_reset(nvc0->bufctx_3d, NVC0_BIND_3D_TEXT);
         BCTX_REFN_bo(nvc0->bufctx_3d, 3D_TEXT,
                      NV_VRAM_DOMAIN(&screen->base) | NOUVEAU_BO_RD,
                      screen->text);
         if (screen->compute) {
            nouveau_bufctx_reset(nvc0->bufctx_cp, NVC0_BIND_CP_TEXT);
            BCTX_REFN_bo(nvc0->bufctx_cp, CP_TEXT,
                         NV_VRAM_DOMAIN(&screen->base) | NOUVEAU_BO_RD,
                         screen->text);
         }

         /* Re-upload the builtin function into the new code segment. */
         nvc0_program_library_upload(nvc0);
      }

      ret = nvc0_program_alloc_code(nvc0, prog);
      if (ret) {
         NOUVEAU_ERR("shader too large (0x%x) to fit in code space ?\n", size);
         return false;
      }

      /* All currently bound shaders have to be reuploaded. */
      for (int i = 0; i < ARRAY_SIZE(progs); i++) {
         if (!progs[i] || progs[i] == prog)
            continue;

         ret = nvc0_program_alloc_code(nvc0, progs[i]);
         if (ret) {
            NOUVEAU_ERR("failed to re-upload a shader after code eviction.\n");
            return false;
         }
         nvc0_program_upload_code(nvc0, progs[i]);

         if (progs[i]->type == PIPE_SHADER_COMPUTE) {
            /* Caches have to be invalidated but the CP_START_ID will be
             * updated in the launch_grid functions. */
            BEGIN_NVC0(nvc0->base.pushbuf, NVC0_CP(FLUSH), 1);
            PUSH_DATA (nvc0->base.pushbuf, NVC0_COMPUTE_FLUSH_CODE);
         } else {
            BEGIN_NVC0(nvc0->base.pushbuf, NVC0_3D(SP_START_ID(i)), 1);
            PUSH_DATA (nvc0->base.pushbuf, progs[i]->code_base);
         }
      }
   }

   nvc0_program_upload_code(nvc0, prog);

#ifdef DEBUG
   if (debug_get_bool_option("NV50_PROG_DEBUG", false))
      nvc0_program_dump(prog);
#endif

   BEGIN_NVC0(nvc0->base.pushbuf, NVC0_3D(MEM_BARRIER), 1);
   PUSH_DATA (nvc0->base.pushbuf, 0x1011);

   return true;
}

/* Upload code for builtin functions like integer division emulation. */
void
nvc0_program_library_upload(struct nvc0_context *nvc0)
{
   struct nvc0_screen *screen = nvc0->screen;
   int ret;
   uint32_t size;
   const uint32_t *code;

   if (screen->lib_code)
      return;

   nv50_ir_get_target_library(screen->base.device->chipset, &code, &size);
   if (!size)
      return;

   ret = nouveau_heap_alloc(screen->text_heap, align(size, 0x100), NULL,
                            &screen->lib_code);
   if (ret)
      return;

   nvc0->base.push_data(&nvc0->base,
                        screen->text, screen->lib_code->start, NV_VRAM_DOMAIN(&screen->base),
                        size, code);
   /* no need for a memory barrier, will be emitted with first program */
}

void
nvc0_program_destroy(struct nvc0_context *nvc0, struct nvc0_program *prog)
{
   const struct pipe_shader_state pipe = prog->pipe;
   const ubyte type = prog->type;

   if (prog->mem)
      nouveau_heap_free(&prog->mem);
   FREE(prog->code); /* may be 0 for hardcoded shaders */
   FREE(prog->relocs);
   FREE(prog->fixups);
   if (prog->type == PIPE_SHADER_COMPUTE && prog->cp.syms)
      FREE(prog->cp.syms);
   if (prog->tfb) {
      if (nvc0->state.tfb == prog->tfb)
         nvc0->state.tfb = NULL;
      FREE(prog->tfb);
   }

   memset(prog, 0, sizeof(*prog));

   prog->pipe = pipe;
   prog->type = type;
}

uint32_t
nvc0_program_symbol_offset(const struct nvc0_program *prog, uint32_t label)
{
   const struct nv50_ir_prog_symbol *syms =
      (const struct nv50_ir_prog_symbol *)prog->cp.syms;
   unsigned base = 0;
   unsigned i;
   if (prog->type != PIPE_SHADER_COMPUTE)
      base = NVC0_SHADER_HEADER_SIZE;
   for (i = 0; i < prog->cp.num_syms; ++i)
      if (syms[i].label == label)
         return prog->code_base + base + syms[i].offset;
   return prog->code_base; /* no symbols or symbol not found */
}

void
nvc0_program_init_tcp_empty(struct nvc0_context *nvc0)
{
   struct ureg_program *ureg;

   ureg = ureg_create(PIPE_SHADER_TESS_CTRL);
   if (!ureg)
      return;

   ureg_property(ureg, TGSI_PROPERTY_TCS_VERTICES_OUT, 1);
   ureg_END(ureg);

   nvc0->tcp_empty = ureg_create_shader_and_destroy(ureg, &nvc0->base.pipe);
}
