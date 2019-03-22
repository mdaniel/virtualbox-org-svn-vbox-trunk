/**********************************************************
 * Copyright 2008-2013 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

/**
 * @file svga_cmd_vgpu10.c
 *
 * Command construction utility for the vgpu10 SVGA3D protocol.
 *
 * \author Mingcheng Chen
 * \author Brian Paul
 */


#include "svga_winsys.h"
#include "svga_resource_buffer.h"
#include "svga_resource_texture.h"
#include "svga_surface.h"
#include "svga_cmd.h"


/**
 * Emit a surface relocation for RenderTargetViewId
 */
static void
view_relocation(struct svga_winsys_context *swc, // IN
                struct pipe_surface *surface,    // IN
                SVGA3dRenderTargetViewId *id,    // OUT
                unsigned flags)
{
   if (surface) {
      struct svga_surface *s = svga_surface(surface);
      assert(s->handle);
      swc->surface_relocation(swc, id, NULL, s->handle, flags);
   }
   else {
      swc->surface_relocation(swc, id, NULL, NULL, flags);
   }
}


/**
 * Emit a surface relocation for a ResourceId.
 */
static void
surface_to_resourceid(struct svga_winsys_context *swc, // IN
                      struct svga_winsys_surface *surface,    // IN
                      SVGA3dSurfaceId *sid,            // OUT
                      unsigned flags)                  // IN
{
   if (surface) {
      swc->surface_relocation(swc, sid, NULL, surface, flags);
   }
   else {
      swc->surface_relocation(swc, sid, NULL, NULL, flags);
   }
}


#define SVGA3D_CREATE_COMMAND(CommandName, CommandCode) \
SVGA3dCmdDX##CommandName *cmd; \
{ \
   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_##CommandCode, \
                            sizeof(SVGA3dCmdDX##CommandName), 0); \
   if (!cmd) \
      return PIPE_ERROR_OUT_OF_MEMORY; \
}

#define SVGA3D_CREATE_CMD_COUNT(CommandName, CommandCode, ElementClassName) \
SVGA3dCmdDX##CommandName *cmd; \
{ \
   assert(count > 0); \
   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_##CommandCode, \
                            sizeof(SVGA3dCmdDX##CommandName) + \
                            count * sizeof(ElementClassName), 0); \
   if (!cmd) \
      return PIPE_ERROR_OUT_OF_MEMORY; \
}

#define SVGA3D_COPY_BASIC(VariableName) \
{ \
   cmd->VariableName = VariableName; \
}

#define SVGA3D_COPY_BASIC_2(VariableName1, VariableName2) \
{ \
   SVGA3D_COPY_BASIC(VariableName1); \
   SVGA3D_COPY_BASIC(VariableName2); \
}

#define SVGA3D_COPY_BASIC_3(VariableName1, VariableName2, VariableName3) \
{ \
   SVGA3D_COPY_BASIC_2(VariableName1, VariableName2); \
   SVGA3D_COPY_BASIC(VariableName3); \
}

#define SVGA3D_COPY_BASIC_4(VariableName1, VariableName2, VariableName3, \
                            VariableName4) \
{ \
   SVGA3D_COPY_BASIC_2(VariableName1, VariableName2); \
   SVGA3D_COPY_BASIC_2(VariableName3, VariableName4); \
}

#define SVGA3D_COPY_BASIC_5(VariableName1, VariableName2, VariableName3, \
                            VariableName4, VariableName5) \
{\
   SVGA3D_COPY_BASIC_3(VariableName1, VariableName2, VariableName3); \
   SVGA3D_COPY_BASIC_2(VariableName4, VariableName5); \
}

#define SVGA3D_COPY_BASIC_6(VariableName1, VariableName2, VariableName3, \
                            VariableName4, VariableName5, VariableName6) \
{\
   SVGA3D_COPY_BASIC_3(VariableName1, VariableName2, VariableName3); \
   SVGA3D_COPY_BASIC_3(VariableName4, VariableName5, VariableName6); \
}

#define SVGA3D_COPY_BASIC_7(VariableName1, VariableName2, VariableName3, \
                            VariableName4, VariableName5, VariableName6, \
                            VariableName7) \
{\
   SVGA3D_COPY_BASIC_4(VariableName1, VariableName2, VariableName3, \
                       VariableName4); \
   SVGA3D_COPY_BASIC_3(VariableName5, VariableName6, VariableName7); \
}

#define SVGA3D_COPY_BASIC_8(VariableName1, VariableName2, VariableName3, \
                            VariableName4, VariableName5, VariableName6, \
                            VariableName7, VariableName8) \
{\
   SVGA3D_COPY_BASIC_4(VariableName1, VariableName2, VariableName3, \
                       VariableName4); \
   SVGA3D_COPY_BASIC_4(VariableName5, VariableName6, VariableName7, \
                       VariableName8); \
}

#define SVGA3D_COPY_BASIC_9(VariableName1, VariableName2, VariableName3, \
                            VariableName4, VariableName5, VariableName6, \
                            VariableName7, VariableName8, VariableName9) \
{\
   SVGA3D_COPY_BASIC_5(VariableName1, VariableName2, VariableName3, \
                       VariableName4, VariableName5); \
   SVGA3D_COPY_BASIC_4(VariableName6, VariableName7, VariableName8, \
                       VariableName9); \
}


enum pipe_error
SVGA3D_vgpu10_PredCopyRegion(struct svga_winsys_context *swc,
                             struct svga_winsys_surface *dstSurf,
                             uint32 dstSubResource,
                             struct svga_winsys_surface *srcSurf,
                             uint32 srcSubResource,
                             const SVGA3dCopyBox *box)
{
   SVGA3dCmdDXPredCopyRegion *cmd =
      SVGA3D_FIFOReserve(swc,
                         SVGA_3D_CMD_DX_PRED_COPY_REGION,
                         sizeof(SVGA3dCmdDXPredCopyRegion),
                         2);  /* two relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->dstSid, NULL, dstSurf, SVGA_RELOC_WRITE);
   swc->surface_relocation(swc, &cmd->srcSid, NULL, srcSurf, SVGA_RELOC_READ);
   cmd->dstSubResource = dstSubResource;
   cmd->srcSubResource = srcSubResource;
   cmd->box = *box;

   swc->commit(swc);

   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_PredCopy(struct svga_winsys_context *swc,
                       struct svga_winsys_surface *dstSurf,
                       struct svga_winsys_surface *srcSurf)
{
   SVGA3dCmdDXPredCopy *cmd =
      SVGA3D_FIFOReserve(swc,
                         SVGA_3D_CMD_DX_PRED_COPY,
                         sizeof(SVGA3dCmdDXPredCopy),
                         2);  /* two relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->dstSid, NULL, dstSurf, SVGA_RELOC_WRITE);
   swc->surface_relocation(swc, &cmd->srcSid, NULL, srcSurf, SVGA_RELOC_READ);

   swc->commit(swc);

   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetViewports(struct svga_winsys_context *swc,
                           unsigned count,
                           const SVGA3dViewport *viewports)
{
   SVGA3D_CREATE_CMD_COUNT(SetViewports, SET_VIEWPORTS, SVGA3dViewport);

   memcpy(cmd + 1, viewports, count * sizeof(SVGA3dViewport));

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_SetShader(struct svga_winsys_context *swc,
                        SVGA3dShaderType type,
                        struct svga_winsys_gb_shader *gbshader,
                        SVGA3dShaderId shaderId)
{
   SVGA3dCmdDXSetShader *cmd = SVGA3D_FIFOReserve(swc,
                                                  SVGA_3D_CMD_DX_SET_SHADER,
                                                  sizeof *cmd,
                                                  1);  /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->shader_relocation(swc, &cmd->shaderId, NULL, NULL, gbshader, 0);

   cmd->type = type;
   cmd->shaderId = shaderId;
   swc->commit(swc);

   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_SetShaderResources(struct svga_winsys_context *swc,
                                 SVGA3dShaderType type,
                                 uint32 startView,
                                 unsigned count,
                                 const SVGA3dShaderResourceViewId ids[],
                                 struct svga_winsys_surface **views)
{
   SVGA3dCmdDXSetShaderResources *cmd;
   SVGA3dShaderResourceViewId *cmd_ids;
   unsigned i;

   cmd = SVGA3D_FIFOReserve(swc,
                            SVGA_3D_CMD_DX_SET_SHADER_RESOURCES,
                            sizeof(SVGA3dCmdDXSetShaderResources) +
                            count * sizeof(SVGA3dShaderResourceViewId),
                            count); /* 'count' relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;


   cmd->type = type;
   cmd->startView = startView;

   cmd_ids = (SVGA3dShaderResourceViewId *) (cmd + 1);
   for (i = 0; i < count; i++) {
      swc->surface_relocation(swc, cmd_ids + i, NULL, views[i],
                              SVGA_RELOC_READ);
      cmd_ids[i] = ids[i];
   }

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_SetSamplers(struct svga_winsys_context *swc,
                          unsigned count,
                          uint32 startSampler,
                          SVGA3dShaderType type,
                          const SVGA3dSamplerId *samplerIds)
{
   SVGA3D_CREATE_CMD_COUNT(SetSamplers, SET_SAMPLERS, SVGA3dSamplerId);

   SVGA3D_COPY_BASIC_2(startSampler, type);
   memcpy(cmd + 1, samplerIds, count * sizeof(SVGA3dSamplerId));

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_ClearRenderTargetView(struct svga_winsys_context *swc,
                                    struct pipe_surface *color_surf,
                                    const float *rgba)
{
   SVGA3dCmdDXClearRenderTargetView *cmd;
   struct svga_surface *ss = svga_surface(color_surf);

   cmd = SVGA3D_FIFOReserve(swc,
                            SVGA_3D_CMD_DX_CLEAR_RENDERTARGET_VIEW,
                            sizeof(SVGA3dCmdDXClearRenderTargetView),
                            1); /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;


   /* NOTE: The following is pretty tricky.  We need to emit a view/surface
    * relocation and we have to provide a pointer to an ID which lies in
    * the bounds of the command space which we just allocated.  However,
    * we then need to overwrite it with the original RenderTargetViewId.
    */
   view_relocation(swc, color_surf, &cmd->renderTargetViewId,
                   SVGA_RELOC_WRITE);
   cmd->renderTargetViewId = ss->view_id;

   COPY_4V(cmd->rgba.value, rgba);

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_SetRenderTargets(struct svga_winsys_context *swc,
                               unsigned color_count,
                               struct pipe_surface **color_surfs,
                               struct pipe_surface *depth_stencil_surf)
{
   const unsigned surf_count = color_count + 1;
   SVGA3dCmdDXSetRenderTargets *cmd;
   SVGA3dRenderTargetViewId *ctarget;
   struct svga_surface *ss;
   unsigned i;

   assert(surf_count > 0);

   cmd = SVGA3D_FIFOReserve(swc,
                            SVGA_3D_CMD_DX_SET_RENDERTARGETS,
                            sizeof(SVGA3dCmdDXSetRenderTargets) +
                            color_count * sizeof(SVGA3dRenderTargetViewId),
                            surf_count); /* 'surf_count' relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   /* NOTE: See earlier comment about the tricky handling of the ViewIds.
    */

   /* Depth / Stencil buffer */
   if (depth_stencil_surf) {
      ss = svga_surface(depth_stencil_surf);
      view_relocation(swc, depth_stencil_surf, &cmd->depthStencilViewId,
                      SVGA_RELOC_WRITE);
      cmd->depthStencilViewId = ss->view_id;
   }
   else {
      /* no depth/stencil buffer - still need a relocation */
      view_relocation(swc, NULL, &cmd->depthStencilViewId,
                      SVGA_RELOC_WRITE);
      cmd->depthStencilViewId = SVGA3D_INVALID_ID;
   }

   /* Color buffers */
   ctarget = (SVGA3dRenderTargetViewId *) &cmd[1];
   for (i = 0; i < color_count; i++) {
      if (color_surfs[i]) {
         ss = svga_surface(color_surfs[i]);
         view_relocation(swc, color_surfs[i], ctarget + i, SVGA_RELOC_WRITE);
         ctarget[i] = ss->view_id;
      }
      else {
         view_relocation(swc, NULL, ctarget + i, SVGA_RELOC_WRITE);
         ctarget[i] = SVGA3D_INVALID_ID;
      }
   }

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_SetBlendState(struct svga_winsys_context *swc,
                            SVGA3dBlendStateId blendId,
                            const float *blendFactor,
                            uint32 sampleMask)
{
   SVGA3D_CREATE_COMMAND(SetBlendState, SET_BLEND_STATE);

   SVGA3D_COPY_BASIC_2(blendId, sampleMask);
   memcpy(cmd->blendFactor, blendFactor, sizeof(float) * 4);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetDepthStencilState(struct svga_winsys_context *swc,
                                   SVGA3dDepthStencilStateId depthStencilId,
                                   uint32 stencilRef)
{
   SVGA3D_CREATE_COMMAND(SetDepthStencilState, SET_DEPTHSTENCIL_STATE);

   SVGA3D_COPY_BASIC_2(depthStencilId, stencilRef);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetRasterizerState(struct svga_winsys_context *swc,
                                 SVGA3dRasterizerStateId rasterizerId)
{
   SVGA3D_CREATE_COMMAND(SetRasterizerState, SET_RASTERIZER_STATE);

   cmd->rasterizerId = rasterizerId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetPredication(struct svga_winsys_context *swc,
                             SVGA3dQueryId queryId,
                             uint32 predicateValue)
{
   SVGA3dCmdDXSetPredication *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_SET_PREDICATION,
                            sizeof *cmd, 0);

   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   cmd->queryId = queryId;
   cmd->predicateValue = predicateValue;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetSOTargets(struct svga_winsys_context *swc,
                           unsigned count,
                           const SVGA3dSoTarget *targets,
                           struct svga_winsys_surface **surfaces)
{
   SVGA3dCmdDXSetSOTargets *cmd;
   SVGA3dSoTarget *sot;
   unsigned i;

   cmd = SVGA3D_FIFOReserve(swc,
                            SVGA_3D_CMD_DX_SET_SOTARGETS,
                            sizeof(SVGA3dCmdDXSetSOTargets) +
                            count * sizeof(SVGA3dSoTarget),
                            count);

   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   cmd->pad0 = 0;
   sot = (SVGA3dSoTarget *)(cmd + 1);
   for (i = 0; i < count; i++, sot++) {
      if (surfaces[i]) {
         sot->offset = targets[i].offset;
         sot->sizeInBytes = targets[i].sizeInBytes;
         swc->surface_relocation(swc, &sot->sid, NULL, surfaces[i],
                                 SVGA_RELOC_WRITE);
      }
      else {
         sot->offset = 0;
         sot->sizeInBytes = ~0u;
         swc->surface_relocation(swc, &sot->sid, NULL, NULL,
                                 SVGA_RELOC_WRITE);
      }
   }
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetScissorRects(struct svga_winsys_context *swc,
                              unsigned count,
                              const SVGASignedRect *rects)
{
   SVGA3dCmdDXSetScissorRects *cmd;

   assert(count > 0);
   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_SET_SCISSORRECTS,
                            sizeof(SVGA3dCmdDXSetScissorRects) +
                            count * sizeof(SVGASignedRect),
                            0);
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   memcpy(cmd + 1, rects, count * sizeof(SVGASignedRect));

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetStreamOutput(struct svga_winsys_context *swc,
                              SVGA3dStreamOutputId soid)
{
   SVGA3D_CREATE_COMMAND(SetStreamOutput, SET_STREAMOUTPUT);

   cmd->soid = soid;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_Draw(struct svga_winsys_context *swc,
                   uint32 vertexCount,
                   uint32 startVertexLocation)
{
   SVGA3D_CREATE_COMMAND(Draw, DRAW);

   SVGA3D_COPY_BASIC_2(vertexCount, startVertexLocation);

   swc->hints |= SVGA_HINT_FLAG_CAN_PRE_FLUSH;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DrawIndexed(struct svga_winsys_context *swc,
                          uint32 indexCount,
                          uint32 startIndexLocation,
                          int32 baseVertexLocation)
{
   SVGA3D_CREATE_COMMAND(DrawIndexed, DRAW_INDEXED);

   SVGA3D_COPY_BASIC_3(indexCount, startIndexLocation,
                       baseVertexLocation);

   swc->hints |= SVGA_HINT_FLAG_CAN_PRE_FLUSH;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DrawInstanced(struct svga_winsys_context *swc,
                            uint32 vertexCountPerInstance,
                            uint32 instanceCount,
                            uint32 startVertexLocation,
                            uint32 startInstanceLocation)
{
   SVGA3D_CREATE_COMMAND(DrawInstanced, DRAW_INSTANCED);

   SVGA3D_COPY_BASIC_4(vertexCountPerInstance, instanceCount,
                       startVertexLocation, startInstanceLocation);

   swc->hints |= SVGA_HINT_FLAG_CAN_PRE_FLUSH;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DrawIndexedInstanced(struct svga_winsys_context *swc,
                                   uint32 indexCountPerInstance,
                                   uint32 instanceCount,
                                   uint32 startIndexLocation,
                                   int32  baseVertexLocation,
                                   uint32 startInstanceLocation)
{
   SVGA3D_CREATE_COMMAND(DrawIndexedInstanced, DRAW_INDEXED_INSTANCED);

   SVGA3D_COPY_BASIC_5(indexCountPerInstance, instanceCount,
                       startIndexLocation, baseVertexLocation,
                       startInstanceLocation);


   swc->hints |= SVGA_HINT_FLAG_CAN_PRE_FLUSH;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DrawAuto(struct svga_winsys_context *swc)
{
   SVGA3D_CREATE_COMMAND(DrawAuto, DRAW_AUTO);

   swc->hints |= SVGA_HINT_FLAG_CAN_PRE_FLUSH;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineQuery(struct svga_winsys_context *swc,
                          SVGA3dQueryId queryId,
                          SVGA3dQueryType type,
                          SVGA3dDXQueryFlags flags)
{
   SVGA3D_CREATE_COMMAND(DefineQuery, DEFINE_QUERY);

   SVGA3D_COPY_BASIC_3(queryId, type, flags);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyQuery(struct svga_winsys_context *swc,
                           SVGA3dQueryId queryId)
{
   SVGA3D_CREATE_COMMAND(DestroyQuery, DESTROY_QUERY);

   cmd->queryId = queryId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_BindQuery(struct svga_winsys_context *swc,
                        struct svga_winsys_gb_query *gbQuery,
                        SVGA3dQueryId queryId)
{
   SVGA3dCmdDXBindQuery *cmd = SVGA3D_FIFOReserve(swc,
                                                  SVGA_3D_CMD_DX_BIND_QUERY,
                                                  sizeof *cmd,
                                                  1);
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   cmd->queryId = queryId;
   swc->query_relocation(swc, &cmd->mobid, gbQuery);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetQueryOffset(struct svga_winsys_context *swc,
                             SVGA3dQueryId queryId,
                             uint32 mobOffset)
{
   SVGA3D_CREATE_COMMAND(SetQueryOffset, SET_QUERY_OFFSET);
   SVGA3D_COPY_BASIC_2(queryId, mobOffset);
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_BeginQuery(struct svga_winsys_context *swc,
                         SVGA3dQueryId queryId)
{
   SVGA3D_CREATE_COMMAND(BeginQuery, BEGIN_QUERY);
   cmd->queryId = queryId;
   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_EndQuery(struct svga_winsys_context *swc,
                       SVGA3dQueryId queryId)
{
   SVGA3D_CREATE_COMMAND(EndQuery, END_QUERY);
   cmd->queryId = queryId;
   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_ClearDepthStencilView(struct svga_winsys_context *swc,
                                    struct pipe_surface *ds_surf,
                                    uint16 flags,
                                    uint16 stencil,
                                    float depth)
{
   SVGA3dCmdDXClearDepthStencilView *cmd;
   struct svga_surface *ss = svga_surface(ds_surf);

   cmd = SVGA3D_FIFOReserve(swc,
                            SVGA_3D_CMD_DX_CLEAR_DEPTHSTENCIL_VIEW,
                            sizeof(SVGA3dCmdDXClearDepthStencilView),
                            1); /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   /* NOTE: The following is pretty tricky.  We need to emit a view/surface
    * relocation and we have to provide a pointer to an ID which lies in
    * the bounds of the command space which we just allocated.  However,
    * we then need to overwrite it with the original DepthStencilViewId.
    */
   view_relocation(swc, ds_surf, &cmd->depthStencilViewId,
                   SVGA_RELOC_WRITE);
   cmd->depthStencilViewId = ss->view_id;
   cmd->flags = flags;
   cmd->stencil = stencil;
   cmd->depth = depth;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineShaderResourceView(struct svga_winsys_context *swc,
                             SVGA3dShaderResourceViewId shaderResourceViewId,
                             struct svga_winsys_surface *surface,
                             SVGA3dSurfaceFormat format,
                             SVGA3dResourceType resourceDimension,
                             const SVGA3dShaderResourceViewDesc *desc)
{
   SVGA3dCmdDXDefineShaderResourceView *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_DEFINE_SHADERRESOURCE_VIEW,
                            sizeof(SVGA3dCmdDXDefineShaderResourceView),
                            1); /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   SVGA3D_COPY_BASIC_3(shaderResourceViewId, format, resourceDimension);

   swc->surface_relocation(swc, &cmd->sid, NULL, surface,
                           SVGA_RELOC_READ);

   cmd->desc = *desc;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyShaderResourceView(struct svga_winsys_context *swc,
                             SVGA3dShaderResourceViewId shaderResourceViewId)
{
   SVGA3D_CREATE_COMMAND(DestroyShaderResourceView,
                       DESTROY_SHADERRESOURCE_VIEW);

   cmd->shaderResourceViewId = shaderResourceViewId;

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_DefineRenderTargetView(struct svga_winsys_context *swc,
                                  SVGA3dRenderTargetViewId renderTargetViewId,
                                  struct svga_winsys_surface *surface,
                                  SVGA3dSurfaceFormat format,
                                  SVGA3dResourceType resourceDimension,
                                  const SVGA3dRenderTargetViewDesc *desc)
{
   SVGA3dCmdDXDefineRenderTargetView *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_DEFINE_RENDERTARGET_VIEW,
                            sizeof(SVGA3dCmdDXDefineRenderTargetView),
                            1); /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   SVGA3D_COPY_BASIC_3(renderTargetViewId, format, resourceDimension);
   cmd->desc = *desc;

   surface_to_resourceid(swc, surface,
                         &cmd->sid,
                         SVGA_RELOC_READ | SVGA_RELOC_WRITE);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyRenderTargetView(struct svga_winsys_context *swc,
                                 SVGA3dRenderTargetViewId renderTargetViewId)
{
   SVGA3D_CREATE_COMMAND(DestroyRenderTargetView, DESTROY_RENDERTARGET_VIEW);

   cmd->renderTargetViewId = renderTargetViewId;

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_DefineDepthStencilView(struct svga_winsys_context *swc,
                                  SVGA3dDepthStencilViewId depthStencilViewId,
                                  struct svga_winsys_surface *surface,
                                  SVGA3dSurfaceFormat format,
                                  SVGA3dResourceType resourceDimension,
                                  const SVGA3dRenderTargetViewDesc *desc)
{
   SVGA3dCmdDXDefineDepthStencilView *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_DEFINE_DEPTHSTENCIL_VIEW,
                            sizeof(SVGA3dCmdDXDefineDepthStencilView),
                            1); /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   SVGA3D_COPY_BASIC_3(depthStencilViewId, format, resourceDimension);
   cmd->mipSlice = desc->tex.mipSlice;
   cmd->firstArraySlice = desc->tex.firstArraySlice;
   cmd->arraySize = desc->tex.arraySize;

   surface_to_resourceid(swc, surface,
                         &cmd->sid,
                         SVGA_RELOC_READ | SVGA_RELOC_WRITE);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyDepthStencilView(struct svga_winsys_context *swc,
                                 SVGA3dDepthStencilViewId depthStencilViewId)
{
   SVGA3D_CREATE_COMMAND(DestroyDepthStencilView, DESTROY_DEPTHSTENCIL_VIEW);

   cmd->depthStencilViewId = depthStencilViewId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineElementLayout(struct svga_winsys_context *swc,
                                  unsigned count,
                                  SVGA3dElementLayoutId elementLayoutId,
                                  const SVGA3dInputElementDesc *elements)
{
   SVGA3dCmdDXDefineElementLayout *cmd;
   unsigned i;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_DEFINE_ELEMENTLAYOUT,
                            sizeof(SVGA3dCmdDXDefineElementLayout) +
                            count * sizeof(SVGA3dInputElementDesc), 0);
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   /* check that all offsets are multiples of four */
   for (i = 0; i < count; i++) {
      assert(elements[i].alignedByteOffset % 4 == 0);
   }
   (void) i; /* silence unused var in release build */

   cmd->elementLayoutId = elementLayoutId;
   memcpy(cmd + 1, elements, count * sizeof(SVGA3dInputElementDesc));

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyElementLayout(struct svga_winsys_context *swc,
                                   SVGA3dElementLayoutId elementLayoutId)
{
   SVGA3D_CREATE_COMMAND(DestroyElementLayout, DESTROY_ELEMENTLAYOUT);

   cmd->elementLayoutId = elementLayoutId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineBlendState(struct svga_winsys_context *swc,
                               SVGA3dBlendStateId blendId,
                               uint8 alphaToCoverageEnable,
                               uint8 independentBlendEnable,
                               const SVGA3dDXBlendStatePerRT *perRT)
{
   SVGA3D_CREATE_COMMAND(DefineBlendState, DEFINE_BLEND_STATE);

   cmd->blendId = blendId;
   cmd->alphaToCoverageEnable = alphaToCoverageEnable;
   cmd->independentBlendEnable = independentBlendEnable;
   memcpy(cmd->perRT, perRT, sizeof(cmd->perRT));
   cmd->pad0 = 0;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyBlendState(struct svga_winsys_context *swc,
                                SVGA3dBlendStateId blendId)
{
   SVGA3D_CREATE_COMMAND(DestroyBlendState, DESTROY_BLEND_STATE);

   cmd->blendId = blendId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineDepthStencilState(struct svga_winsys_context *swc,
                                      SVGA3dDepthStencilStateId depthStencilId,
                                      uint8 depthEnable,
                                      SVGA3dDepthWriteMask depthWriteMask,
                                      SVGA3dComparisonFunc depthFunc,
                                      uint8 stencilEnable,
                                      uint8 frontEnable,
                                      uint8 backEnable,
                                      uint8 stencilReadMask,
                                      uint8 stencilWriteMask,
                                      uint8 frontStencilFailOp,
                                      uint8 frontStencilDepthFailOp,
                                      uint8 frontStencilPassOp,
                                      SVGA3dComparisonFunc frontStencilFunc,
                                      uint8 backStencilFailOp,
                                      uint8 backStencilDepthFailOp,
                                      uint8 backStencilPassOp,
                                      SVGA3dComparisonFunc backStencilFunc)
{
   SVGA3D_CREATE_COMMAND(DefineDepthStencilState, DEFINE_DEPTHSTENCIL_STATE);

   SVGA3D_COPY_BASIC_9(depthStencilId, depthEnable,
                       depthWriteMask, depthFunc,
                       stencilEnable, frontEnable,
                       backEnable, stencilReadMask,
                       stencilWriteMask);
   SVGA3D_COPY_BASIC_8(frontStencilFailOp, frontStencilDepthFailOp,
                       frontStencilPassOp, frontStencilFunc,
                       backStencilFailOp, backStencilDepthFailOp,
                       backStencilPassOp, backStencilFunc);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyDepthStencilState(struct svga_winsys_context *swc,
                                    SVGA3dDepthStencilStateId depthStencilId)
{
   SVGA3D_CREATE_COMMAND(DestroyDepthStencilState,
                         DESTROY_DEPTHSTENCIL_STATE);

   cmd->depthStencilId = depthStencilId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineRasterizerState(struct svga_winsys_context *swc,
                                    SVGA3dRasterizerStateId rasterizerId,
                                    uint8 fillMode,
                                    SVGA3dCullMode cullMode,
                                    uint8 frontCounterClockwise,
                                    int32 depthBias,
                                    float depthBiasClamp,
                                    float slopeScaledDepthBias,
                                    uint8 depthClipEnable,
                                    uint8 scissorEnable,
                                    uint8 multisampleEnable,
                                    uint8 antialiasedLineEnable,
                                    float lineWidth,
                                    uint8 lineStippleEnable,
                                    uint8 lineStippleFactor,
                                    uint16 lineStipplePattern,
                                    uint8 provokingVertexLast)
{
   SVGA3D_CREATE_COMMAND(DefineRasterizerState, DEFINE_RASTERIZER_STATE);

   SVGA3D_COPY_BASIC_5(rasterizerId, fillMode,
                       cullMode, frontCounterClockwise,
                       depthBias);
   SVGA3D_COPY_BASIC_6(depthBiasClamp, slopeScaledDepthBias,
                       depthClipEnable, scissorEnable,
                       multisampleEnable, antialiasedLineEnable);
   cmd->lineWidth = lineWidth;
   cmd->lineStippleEnable = lineStippleEnable;
   cmd->lineStippleFactor = lineStippleFactor;
   cmd->lineStipplePattern = lineStipplePattern;
   cmd->provokingVertexLast = provokingVertexLast;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyRasterizerState(struct svga_winsys_context *swc,
                                     SVGA3dRasterizerStateId rasterizerId)
{
   SVGA3D_CREATE_COMMAND(DestroyRasterizerState, DESTROY_RASTERIZER_STATE);

   cmd->rasterizerId = rasterizerId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineSamplerState(struct svga_winsys_context *swc,
                                 SVGA3dSamplerId samplerId,
                                 SVGA3dFilter filter,
                                 uint8 addressU,
                                 uint8 addressV,
                                 uint8 addressW,
                                 float mipLODBias,
                                 uint8 maxAnisotropy,
                                 uint8 comparisonFunc,
                                 SVGA3dRGBAFloat borderColor,
                                 float minLOD,
                                 float maxLOD)
{
   SVGA3D_CREATE_COMMAND(DefineSamplerState, DEFINE_SAMPLER_STATE);

   SVGA3D_COPY_BASIC_6(samplerId, filter,
                       addressU, addressV,
                       addressW, mipLODBias);
   SVGA3D_COPY_BASIC_5(maxAnisotropy, comparisonFunc,
                       borderColor, minLOD,
                       maxLOD);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroySamplerState(struct svga_winsys_context *swc,
                                  SVGA3dSamplerId samplerId)
{
   SVGA3D_CREATE_COMMAND(DestroySamplerState, DESTROY_SAMPLER_STATE);

   cmd->samplerId = samplerId;

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_DefineAndBindShader(struct svga_winsys_context *swc,
                                  struct svga_winsys_gb_shader *gbshader,
                                  SVGA3dShaderId shaderId,
                                  SVGA3dShaderType type,
                                  uint32 sizeInBytes)
{
   SVGA3dCmdHeader *header;
   SVGA3dCmdDXDefineShader *dcmd;
   SVGA3dCmdDXBindShader *bcmd;
   unsigned totalSize = 2 * sizeof(*header) +
                        sizeof(*dcmd) + sizeof(*bcmd);

   /* Make sure there is room for both commands */
   header = swc->reserve(swc, totalSize, 2);
   if (!header)
      return PIPE_ERROR_OUT_OF_MEMORY;

   /* DXDefineShader command */
   header->id = SVGA_3D_CMD_DX_DEFINE_SHADER;
   header->size = sizeof(*dcmd);
   dcmd = (SVGA3dCmdDXDefineShader *)(header + 1);
   dcmd->shaderId = shaderId;
   dcmd->type = type;
   dcmd->sizeInBytes = sizeInBytes;

   /* DXBindShader command */
   header = (SVGA3dCmdHeader *)(dcmd + 1);

   header->id = SVGA_3D_CMD_DX_BIND_SHADER;
   header->size = sizeof(*bcmd);
   bcmd = (SVGA3dCmdDXBindShader *)(header + 1);

   bcmd->cid = swc->cid;
   swc->shader_relocation(swc, NULL, &bcmd->mobid,
                          &bcmd->offsetInBytes, gbshader, 0);

   bcmd->shid = shaderId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyShader(struct svga_winsys_context *swc,
                            SVGA3dShaderId shaderId)
{
   SVGA3D_CREATE_COMMAND(DestroyShader, DESTROY_SHADER);

   cmd->shaderId = shaderId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DefineStreamOutput(struct svga_winsys_context *swc,
       SVGA3dStreamOutputId soid,
       uint32 numOutputStreamEntries,
       uint32 streamOutputStrideInBytes[SVGA3D_DX_MAX_SOTARGETS],
       const SVGA3dStreamOutputDeclarationEntry decl[SVGA3D_MAX_STREAMOUT_DECLS])
{
   unsigned i;
   SVGA3D_CREATE_COMMAND(DefineStreamOutput, DEFINE_STREAMOUTPUT);

   cmd->soid = soid;
   cmd->numOutputStreamEntries = numOutputStreamEntries;

   for (i = 0; i < ARRAY_SIZE(cmd->streamOutputStrideInBytes); i++)
      cmd->streamOutputStrideInBytes[i] = streamOutputStrideInBytes[i];

   memcpy(cmd->decl, decl,
          sizeof(SVGA3dStreamOutputDeclarationEntry)
          * SVGA3D_MAX_STREAMOUT_DECLS);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_DestroyStreamOutput(struct svga_winsys_context *swc,
                                  SVGA3dStreamOutputId soid)
{
   SVGA3D_CREATE_COMMAND(DestroyStreamOutput, DESTROY_STREAMOUTPUT);

   cmd->soid = soid;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetInputLayout(struct svga_winsys_context *swc,
                             SVGA3dElementLayoutId elementLayoutId)
{
   SVGA3D_CREATE_COMMAND(SetInputLayout, SET_INPUT_LAYOUT);

   cmd->elementLayoutId = elementLayoutId;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetVertexBuffers(struct svga_winsys_context *swc,
                               unsigned count,
                               uint32 startBuffer,
                               const SVGA3dVertexBuffer *bufferInfo,
                               struct svga_winsys_surface **surfaces)
{
   SVGA3dCmdDXSetVertexBuffers *cmd;
   SVGA3dVertexBuffer *bufs;
   unsigned i;

   assert(count > 0);

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_SET_VERTEX_BUFFERS,
                            sizeof(SVGA3dCmdDXSetVertexBuffers) +
                            count * sizeof(SVGA3dVertexBuffer),
                            count); /* 'count' relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   cmd->startBuffer = startBuffer;

   bufs = (SVGA3dVertexBuffer *) &cmd[1];
   for (i = 0; i < count; i++) {
      bufs[i].stride = bufferInfo[i].stride;
      bufs[i].offset = bufferInfo[i].offset;
      assert(bufs[i].stride % 4 == 0);
      assert(bufs[i].offset % 4 == 0);
      swc->surface_relocation(swc, &bufs[i].sid, NULL, surfaces[i],
                              SVGA_RELOC_READ);
   }

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetTopology(struct svga_winsys_context *swc,
                          SVGA3dPrimitiveType topology)
{
   SVGA3D_CREATE_COMMAND(SetTopology, SET_TOPOLOGY);

   cmd->topology = topology;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetIndexBuffer(struct svga_winsys_context *swc,
                             struct svga_winsys_surface *indexes,
                             SVGA3dSurfaceFormat format,
                             uint32 offset)
{
   SVGA3dCmdDXSetIndexBuffer *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_SET_INDEX_BUFFER,
                            sizeof(SVGA3dCmdDXSetIndexBuffer),
                            1); /* one relocations */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->sid, NULL, indexes, SVGA_RELOC_READ);
   SVGA3D_COPY_BASIC_2(format, offset);

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_SetSingleConstantBuffer(struct svga_winsys_context *swc,
                                      unsigned slot,
                                      SVGA3dShaderType type,
                                      struct svga_winsys_surface *surface,
                                      uint32 offsetInBytes,
                                      uint32 sizeInBytes)
{
   SVGA3dCmdDXSetSingleConstantBuffer *cmd;

   assert(offsetInBytes % 256 == 0);
   if (!surface)
      assert(sizeInBytes == 0);
   else
      assert(sizeInBytes > 0);

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_SET_SINGLE_CONSTANT_BUFFER,
                            sizeof(SVGA3dCmdDXSetSingleConstantBuffer),
                            1);  /* one relocation */
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   cmd->slot = slot;
   cmd->type = type;
   swc->surface_relocation(swc, &cmd->sid, NULL, surface, SVGA_RELOC_READ);
   cmd->offsetInBytes = offsetInBytes;
   cmd->sizeInBytes = sizeInBytes;

   swc->commit(swc);

   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_ReadbackSubResource(struct svga_winsys_context *swc,
                                  struct svga_winsys_surface *surface,
                                  unsigned subResource)
{
   SVGA3dCmdDXReadbackSubResource *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_READBACK_SUBRESOURCE,
                            sizeof(SVGA3dCmdDXReadbackSubResource),
                            1);
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->sid, NULL, surface,
                           SVGA_RELOC_READ | SVGA_RELOC_INTERNAL);
   cmd->subResource = subResource;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_UpdateSubResource(struct svga_winsys_context *swc,
                                struct svga_winsys_surface *surface,
                                const SVGA3dBox *box,
                                unsigned subResource)
{
   SVGA3dCmdDXUpdateSubResource *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_UPDATE_SUBRESOURCE,
                            sizeof(SVGA3dCmdDXUpdateSubResource),
                            1);
   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->sid, NULL, surface,
                           SVGA_RELOC_WRITE | SVGA_RELOC_INTERNAL);
   cmd->subResource = subResource;
   cmd->box = *box;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_GenMips(struct svga_winsys_context *swc,
                      SVGA3dShaderResourceViewId shaderResourceViewId,
                      struct svga_winsys_surface *view)
{
   SVGA3dCmdDXGenMips *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_GENMIPS,
                            sizeof(SVGA3dCmdDXGenMips), 1);

   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->shaderResourceViewId, NULL, view,
                           SVGA_RELOC_WRITE);
   cmd->shaderResourceViewId = shaderResourceViewId;

   swc->commit(swc);
   return PIPE_OK;
}


enum pipe_error
SVGA3D_vgpu10_BufferCopy(struct svga_winsys_context *swc,
                          struct svga_winsys_surface *src,
                          struct svga_winsys_surface *dst,
                          unsigned srcx, unsigned dstx, unsigned width)
{
   SVGA3dCmdDXBufferCopy *cmd;

   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_BUFFER_COPY, sizeof *cmd, 2);

   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->dest, NULL, dst, SVGA_RELOC_WRITE);
   swc->surface_relocation(swc, &cmd->src, NULL, src, SVGA_RELOC_READ);
   cmd->destX = dstx;
   cmd->srcX = srcx;
   cmd->width = width;

   swc->commit(swc);
   return PIPE_OK;
}

enum pipe_error
SVGA3D_vgpu10_TransferFromBuffer(struct svga_winsys_context *swc,
                                 struct svga_winsys_surface *src,
                                 unsigned srcOffset, unsigned srcPitch,
                                 unsigned srcSlicePitch,
                                 struct svga_winsys_surface *dst,
                                 unsigned dstSubResource,
                                 SVGA3dBox *dstBox)
{
   SVGA3dCmdDXTransferFromBuffer *cmd;
 
   cmd = SVGA3D_FIFOReserve(swc, SVGA_3D_CMD_DX_TRANSFER_FROM_BUFFER,
                            sizeof(SVGA3dCmdDXTransferFromBuffer), 2);

   if (!cmd)
      return PIPE_ERROR_OUT_OF_MEMORY;

   swc->surface_relocation(swc, &cmd->srcSid, NULL, src, SVGA_RELOC_READ);
   swc->surface_relocation(swc, &cmd->destSid, NULL, dst, SVGA_RELOC_WRITE);
   cmd->srcOffset = srcOffset;
   cmd->srcPitch = srcPitch;
   cmd->srcSlicePitch = srcSlicePitch;
   cmd->destSubResource = dstSubResource;
   cmd->destBox = *dstBox;
 
   swc->commit(swc);
   return PIPE_OK;
}
