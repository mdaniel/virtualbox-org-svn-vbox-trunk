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

#include <gtest/gtest.h>
#include "brw_fs.h"
#include "brw_cfg.h"
#include "program/program.h"

using namespace brw;

class copy_propagation_test : public ::testing::Test {
   virtual void SetUp();

public:
   struct brw_compiler *compiler;
   struct gen_device_info *devinfo;
   struct gl_context *ctx;
   struct brw_wm_prog_data *prog_data;
   struct gl_shader_program *shader_prog;
   fs_visitor *v;
};

class copy_propagation_fs_visitor : public fs_visitor
{
public:
   copy_propagation_fs_visitor(struct brw_compiler *compiler,
                               struct brw_wm_prog_data *prog_data,
                               nir_shader *shader)
      : fs_visitor(compiler, NULL, NULL, NULL,
                   &prog_data->base, (struct gl_program *) NULL,
                   shader, 8, -1) {}
};


void copy_propagation_test::SetUp()
{
   ctx = (struct gl_context *)calloc(1, sizeof(*ctx));
   compiler = (struct brw_compiler *)calloc(1, sizeof(*compiler));
   devinfo = (struct gen_device_info *)calloc(1, sizeof(*devinfo));
   compiler->devinfo = devinfo;

   prog_data = ralloc(NULL, struct brw_wm_prog_data);
   nir_shader *shader =
      nir_shader_create(NULL, MESA_SHADER_FRAGMENT, NULL, NULL);

   v = new copy_propagation_fs_visitor(compiler, prog_data, shader);

   devinfo->gen = 4;
}

static fs_inst *
instruction(bblock_t *block, int num)
{
   fs_inst *inst = (fs_inst *)block->start();
   for (int i = 0; i < num; i++) {
      inst = (fs_inst *)inst->next;
   }
   return inst;
}

static bool
copy_propagation(fs_visitor *v)
{
   const bool print = getenv("TEST_DEBUG");

   if (print) {
      fprintf(stderr, "= Before =\n");
      v->cfg->dump(v);
   }

   bool ret = v->opt_copy_propagation();

   if (print) {
      fprintf(stderr, "\n= After =\n");
      v->cfg->dump(v);
   }

   return ret;
}

TEST_F(copy_propagation_test, basic)
{
   const fs_builder &bld = v->bld;
   fs_reg vgrf0 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf1 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf2 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf3 = v->vgrf(glsl_type::float_type);
   bld.MOV(vgrf0, vgrf2);
   bld.ADD(vgrf1, vgrf0, vgrf3);

   /* = Before =
    *
    * 0: mov(8)        vgrf0  vgrf2
    * 1: add(8)        vgrf1  vgrf0  vgrf3
    *
    * = After =
    * 0: mov(8)        vgrf0  vgrf2
    * 1: add(8)        vgrf1  vgrf2  vgrf3
    */

   v->calculate_cfg();
   bblock_t *block0 = v->cfg->blocks[0];

   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(1, block0->end_ip);

   EXPECT_TRUE(copy_propagation(v));
   EXPECT_EQ(0, block0->start_ip);
   EXPECT_EQ(1, block0->end_ip);

   fs_inst *mov = instruction(block0, 0);
   EXPECT_EQ(BRW_OPCODE_MOV, mov->opcode);
   EXPECT_TRUE(mov->dst.equals(vgrf0));
   EXPECT_TRUE(mov->src[0].equals(vgrf2));

   fs_inst *add = instruction(block0, 1);
   EXPECT_EQ(BRW_OPCODE_ADD, add->opcode);
   EXPECT_TRUE(add->dst.equals(vgrf1));
   EXPECT_TRUE(add->src[0].equals(vgrf2));
   EXPECT_TRUE(add->src[1].equals(vgrf3));
}

TEST_F(copy_propagation_test, maxmax_sat_imm)
{
   const fs_builder &bld = v->bld;
   fs_reg vgrf0 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf1 = v->vgrf(glsl_type::float_type);
   fs_reg vgrf2 = v->vgrf(glsl_type::float_type);

   static const struct {
      enum brw_conditional_mod conditional_mod;
      float immediate;
      bool expected_result;
   } test[] = {
      /*   conditional mod,     imm, expected_result */
      { BRW_CONDITIONAL_GE  ,  0.1f, true },
      { BRW_CONDITIONAL_L   ,  0.1f, true },
      { BRW_CONDITIONAL_GE  ,  0.5f, true },
      { BRW_CONDITIONAL_L   ,  0.5f, true },
      { BRW_CONDITIONAL_GE  ,  0.9f, true },
      { BRW_CONDITIONAL_L   ,  0.9f, true },
      { BRW_CONDITIONAL_GE  , -1.5f, false },
      { BRW_CONDITIONAL_L   , -1.5f, false },
      { BRW_CONDITIONAL_GE  ,  1.5f, false },
      { BRW_CONDITIONAL_L   ,  1.5f, false },

      { BRW_CONDITIONAL_NONE, 0.5f, false },
      { BRW_CONDITIONAL_Z   , 0.5f, false },
      { BRW_CONDITIONAL_NZ  , 0.5f, false },
      { BRW_CONDITIONAL_G   , 0.5f, false },
      { BRW_CONDITIONAL_LE  , 0.5f, false },
      { BRW_CONDITIONAL_R   , 0.5f, false },
      { BRW_CONDITIONAL_O   , 0.5f, false },
      { BRW_CONDITIONAL_U   , 0.5f, false },
   };

   for (unsigned i = 0; i < sizeof(test) / sizeof(test[0]); i++) {
      fs_inst *mov = set_saturate(true, bld.MOV(vgrf0, vgrf1));
      fs_inst *sel = set_condmod(test[i].conditional_mod,
                                 bld.SEL(vgrf2, vgrf0,
                                         brw_imm_f(test[i].immediate)));

      v->calculate_cfg();

      bblock_t *block0 = v->cfg->blocks[0];

      EXPECT_EQ(0, block0->start_ip);
      EXPECT_EQ(1, block0->end_ip);

      EXPECT_EQ(test[i].expected_result, copy_propagation(v));
      EXPECT_EQ(0, block0->start_ip);
      EXPECT_EQ(1, block0->end_ip);

      EXPECT_EQ(BRW_OPCODE_MOV, mov->opcode);
      EXPECT_TRUE(mov->saturate);
      EXPECT_TRUE(mov->dst.equals(vgrf0));
      EXPECT_TRUE(mov->src[0].equals(vgrf1));

      EXPECT_EQ(BRW_OPCODE_SEL, sel->opcode);
      EXPECT_EQ(test[i].conditional_mod, sel->conditional_mod);
      EXPECT_EQ(test[i].expected_result, sel->saturate);
      EXPECT_TRUE(sel->dst.equals(vgrf2));
      if (test[i].expected_result) {
         EXPECT_TRUE(sel->src[0].equals(vgrf1));
      } else {
         EXPECT_TRUE(sel->src[0].equals(vgrf0));
      }
      EXPECT_TRUE(sel->src[1].equals(brw_imm_f(test[i].immediate)));

      delete v->cfg;
      v->cfg = NULL;
   }
}
