/*
 * Copyright © 2014 Intel Corporation
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

#include "brw_cfg.h"
#include "brw_eu.h"
#include "common/gen_debug.h"
#include "intel_asm_annotation.h"
#include "compiler/nir/nir.h"

__attribute__((weak)) void nir_print_instr(const nir_instr *instr, FILE *fp) {}

void
dump_assembly(void *assembly, int num_annotations, struct annotation *annotation,
              const struct gen_device_info *devinfo)
{
   const char *last_annotation_string = NULL;
   const void *last_annotation_ir = NULL;

   for (int i = 0; i < num_annotations; i++) {
      int start_offset = annotation[i].offset;
      int end_offset = annotation[i + 1].offset;

      if (annotation[i].block_start) {
         fprintf(stderr, "   START B%d", annotation[i].block_start->num);
         foreach_list_typed(struct bblock_link, predecessor_link, link,
                            &annotation[i].block_start->parents) {
            struct bblock_t *predecessor_block = predecessor_link->block;
            fprintf(stderr, " <-B%d", predecessor_block->num);
         }
         fprintf(stderr, " (%u cycles)\n", annotation[i].block_start->cycle_count);
      }

      if (last_annotation_ir != annotation[i].ir) {
         last_annotation_ir = annotation[i].ir;
         if (last_annotation_ir) {
            fprintf(stderr, "   ");
            nir_print_instr(annotation[i].ir, stderr);
            fprintf(stderr, "\n");
         }
      }

      if (last_annotation_string != annotation[i].annotation) {
         last_annotation_string = annotation[i].annotation;
         if (last_annotation_string)
            fprintf(stderr, "   %s\n", last_annotation_string);
      }

      brw_disassemble(devinfo, assembly, start_offset, end_offset, stderr);

      if (annotation[i].error) {
         fputs(annotation[i].error, stderr);
      }

      if (annotation[i].block_end) {
         fprintf(stderr, "   END B%d", annotation[i].block_end->num);
         foreach_list_typed(struct bblock_link, successor_link, link,
                            &annotation[i].block_end->children) {
            struct bblock_t *successor_block = successor_link->block;
            fprintf(stderr, " ->B%d", successor_block->num);
         }
         fprintf(stderr, "\n");
      }
   }
   fprintf(stderr, "\n");
}

static bool
annotation_array_ensure_space(struct annotation_info *annotation)
{
   if (annotation->ann_size <= annotation->ann_count) {
      int old_size = annotation->ann_size;
      annotation->ann_size = MAX2(1024, annotation->ann_size * 2);
      annotation->ann = reralloc(annotation->mem_ctx, annotation->ann,
                                 struct annotation, annotation->ann_size);
      if (!annotation->ann)
         return false;

      memset(annotation->ann + old_size, 0,
             (annotation->ann_size - old_size) * sizeof(struct annotation));
   }

   return true;
}

void annotate(const struct gen_device_info *devinfo,
              struct annotation_info *annotation, const struct cfg_t *cfg,
              struct backend_instruction *inst, unsigned offset)
{
   if (annotation->mem_ctx == NULL)
      annotation->mem_ctx = ralloc_context(NULL);

   if (!annotation_array_ensure_space(annotation))
      return;

   struct annotation *ann = &annotation->ann[annotation->ann_count++];
   ann->offset = offset;
   if ((INTEL_DEBUG & DEBUG_ANNOTATION) != 0) {
      ann->ir = inst->ir;
      ann->annotation = inst->annotation;
   }

   if (bblock_start(cfg->blocks[annotation->cur_block]) == inst) {
      ann->block_start = cfg->blocks[annotation->cur_block];
   }

   /* There is no hardware DO instruction on Gen6+, so since DO always
    * starts a basic block, we need to set the .block_start of the next
    * instruction's annotation with a pointer to the bblock started by
    * the DO.
    *
    * There's also only complication from emitting an annotation without
    * a corresponding hardware instruction to disassemble.
    */
   if (devinfo->gen >= 6 && inst->opcode == BRW_OPCODE_DO) {
      annotation->ann_count--;
   }

   if (bblock_end(cfg->blocks[annotation->cur_block]) == inst) {
      ann->block_end = cfg->blocks[annotation->cur_block];
      annotation->cur_block++;
   }
}

void
annotation_finalize(struct annotation_info *annotation,
                    unsigned next_inst_offset)
{
   if (!annotation->ann_count)
      return;

   if (annotation->ann_count == annotation->ann_size) {
      annotation->ann = reralloc(annotation->mem_ctx, annotation->ann,
                                 struct annotation, annotation->ann_size + 1);
   }
   annotation->ann[annotation->ann_count].offset = next_inst_offset;
}

void
annotation_insert_error(struct annotation_info *annotation, unsigned offset,
                        const char *error)
{
   struct annotation *ann;

   if (!annotation->ann_count)
      return;

   /* We may have to split an annotation, so ensure we have enough space
    * allocated for that case up front.
    */
   if (!annotation_array_ensure_space(annotation))
      return;

   assume(annotation->ann_count > 0);

   for (int i = 0; i < annotation->ann_count; i++) {
      struct annotation *cur = &annotation->ann[i];
      struct annotation *next = &annotation->ann[i + 1];
      ann = cur;

      if (next->offset <= offset)
         continue;

      if (offset + sizeof(brw_inst) != next->offset) {
         memmove(next, cur,
                 (annotation->ann_count - i + 2) * sizeof(struct annotation));
         cur->error = NULL;
         cur->error_length = 0;
         cur->block_end = NULL;
         next->offset = offset + sizeof(brw_inst);
         next->block_start = NULL;
         annotation->ann_count++;
      }
      break;
   }

   if (ann->error)
      ralloc_strcat(&ann->error, error);
   else
      ann->error = ralloc_strdup(annotation->mem_ctx, error);
}
