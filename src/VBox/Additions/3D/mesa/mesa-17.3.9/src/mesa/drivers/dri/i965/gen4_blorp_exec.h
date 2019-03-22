/*
 * Copyright © 2016 Intel Corporation
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

static inline struct blorp_address
dynamic_state_address(struct blorp_batch *batch, uint32_t offset)
{
   assert(batch->blorp->driver_ctx == batch->driver_batch);
   struct brw_context *brw = batch->driver_batch;

   return (struct blorp_address) {
      .buffer = brw->batch.state.bo,
      .offset = offset,
   };
}

static inline struct blorp_address
instruction_state_address(struct blorp_batch *batch, uint32_t offset)
{
   assert(batch->blorp->driver_ctx == batch->driver_batch);
   struct brw_context *brw = batch->driver_batch;

   return (struct blorp_address) {
      .buffer = brw->cache.bo,
      .offset = offset,
   };
}

static struct blorp_address
blorp_emit_vs_state(struct blorp_batch *batch,
                    const struct blorp_params *params)
{
   assert(batch->blorp->driver_ctx == batch->driver_batch);
   struct brw_context *brw = batch->driver_batch;

   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(VS_STATE), vs, 64, &offset) {
      vs.Enable = false;
      vs.URBEntryAllocationSize = brw->urb.vsize - 1;
#if GEN_GEN == 5
      vs.NumberofURBEntries = brw->urb.nr_vs_entries >> 2;
#else
      vs.NumberofURBEntries = brw->urb.nr_vs_entries;
#endif
   }

   return dynamic_state_address(batch, offset);
}

static struct blorp_address
blorp_emit_sf_state(struct blorp_batch *batch,
                    const struct blorp_params *params)
{
   assert(batch->blorp->driver_ctx == batch->driver_batch);
   struct brw_context *brw = batch->driver_batch;
   const struct brw_sf_prog_data *prog_data = params->sf_prog_data;

   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(SF_STATE), sf, 64, &offset) {
#if GEN_GEN == 4
      sf.KernelStartPointer =
         instruction_state_address(batch, params->sf_prog_kernel);
#else
      sf.KernelStartPointer = params->sf_prog_kernel;
#endif
      sf.GRFRegisterCount = DIV_ROUND_UP(prog_data->total_grf, 16) - 1;
      sf.VertexURBEntryReadLength = prog_data->urb_read_length;
      sf.VertexURBEntryReadOffset = BRW_SF_URB_ENTRY_READ_OFFSET;
      sf.DispatchGRFStartRegisterForURBData = 3;

      sf.URBEntryAllocationSize = brw->urb.sfsize - 1;
      sf.NumberofURBEntries = brw->urb.nr_sf_entries;

#if GEN_GEN == 5
      sf.MaximumNumberofThreads = MIN2(48, brw->urb.nr_sf_entries) - 1;
#else
      sf.MaximumNumberofThreads = MIN2(24, brw->urb.nr_sf_entries) - 1;
#endif

      sf.ViewportTransformEnable = false;

      sf.CullMode = CULLMODE_NONE;
   }

   return dynamic_state_address(batch, offset);
}

static struct blorp_address
blorp_emit_wm_state(struct blorp_batch *batch,
                    const struct blorp_params *params)
{
   const struct brw_wm_prog_data *prog_data = params->wm_prog_data;

   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(WM_STATE), wm, 64, &offset) {
      if (params->src.enabled) {
         /* Iron Lake can't do sampler prefetch */
         wm.SamplerCount = (GEN_GEN != 5);
         wm.BindingTableEntryCount = 2;
         uint32_t sampler = blorp_emit_sampler_state(batch, params);
         wm.SamplerStatePointer = dynamic_state_address(batch, sampler);
      }

      if (prog_data) {
         wm.DispatchGRFStartRegisterForConstantSetupData0 =
            prog_data->base.dispatch_grf_start_reg;
         wm.SetupURBEntryReadLength = prog_data->num_varying_inputs * 2;
         wm.SetupURBEntryReadOffset = 0;

         wm.DepthCoefficientURBReadOffset = 1;
         wm.PixelShaderKillsPixel = prog_data->uses_kill;
         wm.ThreadDispatchEnable = true;
         wm.EarlyDepthTestEnable = true;

         wm._8PixelDispatchEnable = prog_data->dispatch_8;
         wm._16PixelDispatchEnable = prog_data->dispatch_16;

#if GEN_GEN == 4
         wm.KernelStartPointer0 =
            instruction_state_address(batch, params->wm_prog_kernel);
         wm.GRFRegisterCount0 = prog_data->reg_blocks_0;
#else
         wm.KernelStartPointer0 = params->wm_prog_kernel;
         wm.GRFRegisterCount0 = prog_data->reg_blocks_0;
         wm.KernelStartPointer2 =
            params->wm_prog_kernel + prog_data->prog_offset_2;
         wm.GRFRegisterCount2 = prog_data->reg_blocks_2;
#endif
      }

      wm.MaximumNumberofThreads =
         batch->blorp->compiler->devinfo->max_wm_threads - 1;
   }

   return dynamic_state_address(batch, offset);
}

static struct blorp_address
blorp_emit_color_calc_state(struct blorp_batch *batch,
                            const struct blorp_params *params)
{
   uint32_t cc_viewport = blorp_emit_cc_viewport(batch, params);

   uint32_t offset;
   blorp_emit_dynamic(batch, GENX(COLOR_CALC_STATE), cc, 64, &offset) {
      cc.CCViewportStatePointer = dynamic_state_address(batch, cc_viewport);
   }

   return dynamic_state_address(batch, offset);
}

static void
blorp_emit_pipeline(struct blorp_batch *batch,
                    const struct blorp_params *params)
{
   assert(batch->blorp->driver_ctx == batch->driver_batch);
   struct brw_context *brw = batch->driver_batch;

   emit_urb_config(batch, params);

   blorp_emit(batch, GENX(3DSTATE_PIPELINED_POINTERS), pp) {
      pp.PointertoVSState = blorp_emit_vs_state(batch, params);
      pp.GSEnable = false;
      pp.ClipEnable = false;
      pp.PointertoSFState = blorp_emit_sf_state(batch, params);
      pp.PointertoWMState = blorp_emit_wm_state(batch, params);
      pp.PointertoColorCalcState = blorp_emit_color_calc_state(batch, params);
   }

   brw_upload_urb_fence(brw);

   blorp_emit(batch, GENX(CS_URB_STATE), curb);
   blorp_emit(batch, GENX(CONSTANT_BUFFER), curb);
}
