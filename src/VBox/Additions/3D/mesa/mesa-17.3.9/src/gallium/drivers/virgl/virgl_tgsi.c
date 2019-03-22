/*
 * Copyright 2014, 2015 Red Hat.
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
 */

/* the virgl hw tgsi vs what the current gallium want will diverge over time.
   so add a transform stage to remove things we don't want to send unless
   the receiver supports it.
*/
#include "tgsi/tgsi_transform.h"
#include "virgl_context.h"
struct virgl_transform_context {
   struct tgsi_transform_context base;
};

static void
virgl_tgsi_transform_declaration(struct tgsi_transform_context *ctx,
                                 struct tgsi_full_declaration *decl)
{
   switch (decl->Declaration.File) {
   case TGSI_FILE_CONSTANT:
      if (decl->Declaration.Dimension) {
         if (decl->Dim.Index2D == 0)
            decl->Declaration.Dimension = 0;
      }
      break;
   default:
      break;
   }
   ctx->emit_declaration(ctx, decl);

}

/* for now just strip out the new properties the remote doesn't understand
   yet */
static void
virgl_tgsi_transform_property(struct tgsi_transform_context *ctx,
                              struct tgsi_full_property *prop)
{
   switch (prop->Property.PropertyName) {
   case TGSI_PROPERTY_NUM_CLIPDIST_ENABLED:
   case TGSI_PROPERTY_NUM_CULLDIST_ENABLED:
   case TGSI_PROPERTY_NEXT_SHADER:
      break;
   default:
      ctx->emit_property(ctx, prop);
      break;
   }
}

static void
virgl_tgsi_transform_instruction(struct tgsi_transform_context *ctx,
				 struct tgsi_full_instruction *inst)
{
   if (inst->Instruction.Precise)
      inst->Instruction.Precise = 0;

   for (unsigned i = 0; i < inst->Instruction.NumSrcRegs; i++) {
      if (inst->Src[i].Register.File == TGSI_FILE_CONSTANT &&
          inst->Src[i].Register.Dimension &&
          inst->Src[i].Dimension.Index == 0)
         inst->Src[i].Register.Dimension = 0;
   }
   ctx->emit_instruction(ctx, inst);
}

struct tgsi_token *virgl_tgsi_transform(const struct tgsi_token *tokens_in)
{

   struct virgl_transform_context transform;
   const uint newLen = tgsi_num_tokens(tokens_in);
   struct tgsi_token *new_tokens;

   new_tokens = tgsi_alloc_tokens(newLen);
   if (!new_tokens)
      return NULL;

   memset(&transform, 0, sizeof(transform));
   transform.base.transform_declaration = virgl_tgsi_transform_declaration;
   transform.base.transform_property = virgl_tgsi_transform_property;
   transform.base.transform_instruction = virgl_tgsi_transform_instruction;
   tgsi_transform_shader(tokens_in, new_tokens, newLen, &transform.base);

   return new_tokens;
}
