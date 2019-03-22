/* -*- c++ -*- */
/*
 * Copyright © 2010-2015 Intel Corporation
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

#ifndef BRW_FS_BUILDER_H
#define BRW_FS_BUILDER_H

#include "brw_ir_fs.h"
#include "brw_shader.h"

namespace brw {
   /**
    * Toolbox to assemble an FS IR program out of individual instructions.
    *
    * This object is meant to have an interface consistent with
    * brw::vec4_builder.  They cannot be fully interchangeable because
    * brw::fs_builder generates scalar code while brw::vec4_builder generates
    * vector code.
    */
   class fs_builder {
   public:
      /** Type used in this IR to represent a source of an instruction. */
      typedef fs_reg src_reg;

      /** Type used in this IR to represent the destination of an instruction. */
      typedef fs_reg dst_reg;

      /** Type used in this IR to represent an instruction. */
      typedef fs_inst instruction;

      /**
       * Construct an fs_builder that inserts instructions into \p shader.
       * \p dispatch_width gives the native execution width of the program.
       */
      fs_builder(backend_shader *shader,
                 unsigned dispatch_width) :
         shader(shader), block(NULL), cursor(NULL),
         _dispatch_width(dispatch_width),
         _group(0),
         force_writemask_all(false),
         annotation()
      {
      }

      /**
       * Construct an fs_builder that inserts instructions into \p shader
       * before instruction \p inst in basic block \p block.  The default
       * execution controls and debug annotation are initialized from the
       * instruction passed as argument.
       */
      fs_builder(backend_shader *shader, bblock_t *block, fs_inst *inst) :
         shader(shader), block(block), cursor(inst),
         _dispatch_width(inst->exec_size),
         _group(inst->group),
         force_writemask_all(inst->force_writemask_all)
      {
         annotation.str = inst->annotation;
         annotation.ir = inst->ir;
      }

      /**
       * Construct an fs_builder that inserts instructions before \p cursor in
       * basic block \p block, inheriting other code generation parameters
       * from this.
       */
      fs_builder
      at(bblock_t *block, exec_node *cursor) const
      {
         fs_builder bld = *this;
         bld.block = block;
         bld.cursor = cursor;
         return bld;
      }

      /**
       * Construct an fs_builder appending instructions at the end of the
       * instruction list of the shader, inheriting other code generation
       * parameters from this.
       */
      fs_builder
      at_end() const
      {
         return at(NULL, (exec_node *)&shader->instructions.tail_sentinel);
      }

      /**
       * Construct a builder specifying the default SIMD width and group of
       * channel enable signals, inheriting other code generation parameters
       * from this.
       *
       * \p n gives the default SIMD width, \p i gives the slot group used for
       * predication and control flow masking in multiples of \p n channels.
       */
      fs_builder
      group(unsigned n, unsigned i) const
      {
         assert(force_writemask_all ||
                (n <= dispatch_width() && i < dispatch_width() / n));
         fs_builder bld = *this;
         bld._dispatch_width = n;
         bld._group += i * n;
         return bld;
      }

      /**
       * Alias for group() with width equal to eight.
       */
      fs_builder
      half(unsigned i) const
      {
         return group(8, i);
      }

      /**
       * Construct a builder with per-channel control flow execution masking
       * disabled if \p b is true.  If control flow execution masking is
       * already disabled this has no effect.
       */
      fs_builder
      exec_all(bool b = true) const
      {
         fs_builder bld = *this;
         if (b)
            bld.force_writemask_all = true;
         return bld;
      }

      /**
       * Construct a builder with the given debug annotation info.
       */
      fs_builder
      annotate(const char *str, const void *ir = NULL) const
      {
         fs_builder bld = *this;
         bld.annotation.str = str;
         bld.annotation.ir = ir;
         return bld;
      }

      /**
       * Get the SIMD width in use.
       */
      unsigned
      dispatch_width() const
      {
         return _dispatch_width;
      }

      /**
       * Get the channel group in use.
       */
      unsigned
      group() const
      {
         return _group;
      }

      /**
       * Allocate a virtual register of natural vector size (one for this IR)
       * and SIMD width.  \p n gives the amount of space to allocate in
       * dispatch_width units (which is just enough space for one logical
       * component in this IR).
       */
      dst_reg
      vgrf(enum brw_reg_type type, unsigned n = 1) const
      {
         assert(dispatch_width() <= 32);

         if (n > 0)
            return dst_reg(VGRF, shader->alloc.allocate(
                              DIV_ROUND_UP(n * type_sz(type) * dispatch_width(),
                                           REG_SIZE)),
                           type);
         else
            return retype(null_reg_ud(), type);
      }

      /**
       * Create a null register of floating type.
       */
      dst_reg
      null_reg_f() const
      {
         return dst_reg(retype(brw_null_reg(), BRW_REGISTER_TYPE_F));
      }

      dst_reg
      null_reg_df() const
      {
         return dst_reg(retype(brw_null_reg(), BRW_REGISTER_TYPE_DF));
      }

      /**
       * Create a null register of signed integer type.
       */
      dst_reg
      null_reg_d() const
      {
         return dst_reg(retype(brw_null_reg(), BRW_REGISTER_TYPE_D));
      }

      /**
       * Create a null register of unsigned integer type.
       */
      dst_reg
      null_reg_ud() const
      {
         return dst_reg(retype(brw_null_reg(), BRW_REGISTER_TYPE_UD));
      }

      /**
       * Get the mask of SIMD channels enabled by dispatch and not yet
       * disabled by discard.
       */
      src_reg
      sample_mask_reg() const
      {
         assert(shader->stage != MESA_SHADER_FRAGMENT ||
                group() + dispatch_width() <= 16);
         if (shader->stage != MESA_SHADER_FRAGMENT) {
            return brw_imm_d(0xffffffff);
         } else if (brw_wm_prog_data(shader->stage_prog_data)->uses_kill) {
            return brw_flag_reg(0, 1);
         } else {
            return retype(brw_vec1_grf(1, 7), BRW_REGISTER_TYPE_UD);
         }
      }

      /**
       * Insert an instruction into the program.
       */
      instruction *
      emit(const instruction &inst) const
      {
         return emit(new(shader->mem_ctx) instruction(inst));
      }

      /**
       * Create and insert a nullary control instruction into the program.
       */
      instruction *
      emit(enum opcode opcode) const
      {
         return emit(instruction(opcode, dispatch_width()));
      }

      /**
       * Create and insert a nullary instruction into the program.
       */
      instruction *
      emit(enum opcode opcode, const dst_reg &dst) const
      {
         return emit(instruction(opcode, dispatch_width(), dst));
      }

      /**
       * Create and insert a unary instruction into the program.
       */
      instruction *
      emit(enum opcode opcode, const dst_reg &dst, const src_reg &src0) const
      {
         switch (opcode) {
         case SHADER_OPCODE_RCP:
         case SHADER_OPCODE_RSQ:
         case SHADER_OPCODE_SQRT:
         case SHADER_OPCODE_EXP2:
         case SHADER_OPCODE_LOG2:
         case SHADER_OPCODE_SIN:
         case SHADER_OPCODE_COS:
            return emit(instruction(opcode, dispatch_width(), dst,
                                    fix_math_operand(src0)));

         default:
            return emit(instruction(opcode, dispatch_width(), dst, src0));
         }
      }

      /**
       * Create and insert a binary instruction into the program.
       */
      instruction *
      emit(enum opcode opcode, const dst_reg &dst, const src_reg &src0,
           const src_reg &src1) const
      {
         switch (opcode) {
         case SHADER_OPCODE_POW:
         case SHADER_OPCODE_INT_QUOTIENT:
         case SHADER_OPCODE_INT_REMAINDER:
            return emit(instruction(opcode, dispatch_width(), dst,
                                    fix_math_operand(src0),
                                    fix_math_operand(src1)));

         default:
            return emit(instruction(opcode, dispatch_width(), dst, src0, src1));

         }
      }

      /**
       * Create and insert a ternary instruction into the program.
       */
      instruction *
      emit(enum opcode opcode, const dst_reg &dst, const src_reg &src0,
           const src_reg &src1, const src_reg &src2) const
      {
         switch (opcode) {
         case BRW_OPCODE_BFE:
         case BRW_OPCODE_BFI2:
         case BRW_OPCODE_MAD:
         case BRW_OPCODE_LRP:
            return emit(instruction(opcode, dispatch_width(), dst,
                                    fix_3src_operand(src0),
                                    fix_3src_operand(src1),
                                    fix_3src_operand(src2)));

         default:
            return emit(instruction(opcode, dispatch_width(), dst,
                                    src0, src1, src2));
         }
      }

      /**
       * Create and insert an instruction with a variable number of sources
       * into the program.
       */
      instruction *
      emit(enum opcode opcode, const dst_reg &dst, const src_reg srcs[],
           unsigned n) const
      {
         return emit(instruction(opcode, dispatch_width(), dst, srcs, n));
      }

      /**
       * Insert a preallocated instruction into the program.
       */
      instruction *
      emit(instruction *inst) const
      {
         assert(inst->exec_size <= 32);
         assert(inst->exec_size == dispatch_width() ||
                force_writemask_all);

         inst->group = _group;
         inst->force_writemask_all = force_writemask_all;
         inst->annotation = annotation.str;
         inst->ir = annotation.ir;

         if (block)
            static_cast<instruction *>(cursor)->insert_before(block, inst);
         else
            cursor->insert_before(inst);

         return inst;
      }

      /**
       * Select \p src0 if the comparison of both sources with the given
       * conditional mod evaluates to true, otherwise select \p src1.
       *
       * Generally useful to get the minimum or maximum of two values.
       */
      instruction *
      emit_minmax(const dst_reg &dst, const src_reg &src0,
                  const src_reg &src1, brw_conditional_mod mod) const
      {
         assert(mod == BRW_CONDITIONAL_GE || mod == BRW_CONDITIONAL_L);

         return set_condmod(mod, SEL(dst, fix_unsigned_negate(src0),
                                     fix_unsigned_negate(src1)));
      }

      /**
       * Copy any live channel from \p src to the first channel of the result.
       */
      src_reg
      emit_uniformize(const src_reg &src) const
      {
         /* FIXME: We use a vector chan_index and dst to allow constant and
          * copy propagration to move result all the way into the consuming
          * instruction (typically a surface index or sampler index for a
          * send). This uses 1 or 3 extra hw registers in 16 or 32 wide
          * dispatch. Once we teach const/copy propagation about scalars we
          * should go back to scalar destinations here.
          */
         const fs_builder ubld = exec_all();
         const dst_reg chan_index = vgrf(BRW_REGISTER_TYPE_UD);
         const dst_reg dst = vgrf(src.type);

         ubld.emit(SHADER_OPCODE_FIND_LIVE_CHANNEL, chan_index);
         ubld.emit(SHADER_OPCODE_BROADCAST, dst, src, component(chan_index, 0));

         return src_reg(component(dst, 0));
      }

      /**
       * Assorted arithmetic ops.
       * @{
       */
#define ALU1(op)                                        \
      instruction *                                     \
      op(const dst_reg &dst, const src_reg &src0) const \
      {                                                 \
         return emit(BRW_OPCODE_##op, dst, src0);       \
      }

#define ALU2(op)                                                        \
      instruction *                                                     \
      op(const dst_reg &dst, const src_reg &src0, const src_reg &src1) const \
      {                                                                 \
         return emit(BRW_OPCODE_##op, dst, src0, src1);                 \
      }

#define ALU2_ACC(op)                                                    \
      instruction *                                                     \
      op(const dst_reg &dst, const src_reg &src0, const src_reg &src1) const \
      {                                                                 \
         instruction *inst = emit(BRW_OPCODE_##op, dst, src0, src1);    \
         inst->writes_accumulator = true;                               \
         return inst;                                                   \
      }

#define ALU3(op)                                                        \
      instruction *                                                     \
      op(const dst_reg &dst, const src_reg &src0, const src_reg &src1,  \
         const src_reg &src2) const                                     \
      {                                                                 \
         return emit(BRW_OPCODE_##op, dst, src0, src1, src2);           \
      }

      ALU2(ADD)
      ALU2_ACC(ADDC)
      ALU2(AND)
      ALU2(ASR)
      ALU2(AVG)
      ALU3(BFE)
      ALU2(BFI1)
      ALU3(BFI2)
      ALU1(BFREV)
      ALU1(CBIT)
      ALU2(CMPN)
      ALU3(CSEL)
      ALU1(DIM)
      ALU2(DP2)
      ALU2(DP3)
      ALU2(DP4)
      ALU2(DPH)
      ALU1(F16TO32)
      ALU1(F32TO16)
      ALU1(FBH)
      ALU1(FBL)
      ALU1(FRC)
      ALU2(LINE)
      ALU1(LZD)
      ALU2(MAC)
      ALU2_ACC(MACH)
      ALU3(MAD)
      ALU1(MOV)
      ALU2(MUL)
      ALU1(NOT)
      ALU2(OR)
      ALU2(PLN)
      ALU1(RNDD)
      ALU1(RNDE)
      ALU1(RNDU)
      ALU1(RNDZ)
      ALU2(SAD2)
      ALU2_ACC(SADA2)
      ALU2(SEL)
      ALU2(SHL)
      ALU2(SHR)
      ALU2_ACC(SUBB)
      ALU2(XOR)

#undef ALU3
#undef ALU2_ACC
#undef ALU2
#undef ALU1
      /** @} */

      /**
       * CMP: Sets the low bit of the destination channels with the result
       * of the comparison, while the upper bits are undefined, and updates
       * the flag register with the packed 16 bits of the result.
       */
      instruction *
      CMP(const dst_reg &dst, const src_reg &src0, const src_reg &src1,
          brw_conditional_mod condition) const
      {
         /* Take the instruction:
          *
          * CMP null<d> src0<f> src1<f>
          *
          * Original gen4 does type conversion to the destination type
          * before comparison, producing garbage results for floating
          * point comparisons.
          *
          * The destination type doesn't matter on newer generations,
          * so we set the type to match src0 so we can compact the
          * instruction.
          */
         return set_condmod(condition,
                            emit(BRW_OPCODE_CMP, retype(dst, src0.type),
                                 fix_unsigned_negate(src0),
                                 fix_unsigned_negate(src1)));
      }

      /**
       * Gen4 predicated IF.
       */
      instruction *
      IF(brw_predicate predicate) const
      {
         return set_predicate(predicate, emit(BRW_OPCODE_IF));
      }

      /**
       * Emit a linear interpolation instruction.
       */
      instruction *
      LRP(const dst_reg &dst, const src_reg &x, const src_reg &y,
          const src_reg &a) const
      {
         if (shader->devinfo->gen >= 6) {
            /* The LRP instruction actually does op1 * op0 + op2 * (1 - op0), so
             * we need to reorder the operands.
             */
            return emit(BRW_OPCODE_LRP, dst, a, y, x);

         } else {
            /* We can't use the LRP instruction.  Emit x*(1-a) + y*a. */
            const dst_reg y_times_a = vgrf(dst.type);
            const dst_reg one_minus_a = vgrf(dst.type);
            const dst_reg x_times_one_minus_a = vgrf(dst.type);

            MUL(y_times_a, y, a);
            ADD(one_minus_a, negate(a), brw_imm_f(1.0f));
            MUL(x_times_one_minus_a, x, src_reg(one_minus_a));
            return ADD(dst, src_reg(x_times_one_minus_a), src_reg(y_times_a));
         }
      }

      /**
       * Collect a number of registers in a contiguous range of registers.
       */
      instruction *
      LOAD_PAYLOAD(const dst_reg &dst, const src_reg *src,
                   unsigned sources, unsigned header_size) const
      {
         instruction *inst = emit(SHADER_OPCODE_LOAD_PAYLOAD, dst, src, sources);
         inst->header_size = header_size;
         inst->size_written = header_size * REG_SIZE;
         for (unsigned i = header_size; i < sources; i++) {
            inst->size_written +=
               ALIGN(dispatch_width() * type_sz(src[i].type) * dst.stride,
                     REG_SIZE);
         }

         return inst;
      }

      backend_shader *shader;

   private:
      /**
       * Workaround for negation of UD registers.  See comment in
       * fs_generator::generate_code() for more details.
       */
      src_reg
      fix_unsigned_negate(const src_reg &src) const
      {
         if (src.type == BRW_REGISTER_TYPE_UD &&
             src.negate) {
            dst_reg temp = vgrf(BRW_REGISTER_TYPE_UD);
            MOV(temp, src);
            return src_reg(temp);
         } else {
            return src;
         }
      }

      /**
       * Workaround for source register modes not supported by the ternary
       * instruction encoding.
       */
      src_reg
      fix_3src_operand(const src_reg &src) const
      {
         if (src.file == VGRF || src.file == UNIFORM || src.stride > 1) {
            return src;
         } else {
            dst_reg expanded = vgrf(src.type);
            MOV(expanded, src);
            return expanded;
         }
      }

      /**
       * Workaround for source register modes not supported by the math
       * instruction.
       */
      src_reg
      fix_math_operand(const src_reg &src) const
      {
         /* Can't do hstride == 0 args on gen6 math, so expand it out. We
          * might be able to do better by doing execsize = 1 math and then
          * expanding that result out, but we would need to be careful with
          * masking.
          *
          * Gen6 hardware ignores source modifiers (negate and abs) on math
          * instructions, so we also move to a temp to set those up.
          *
          * Gen7 relaxes most of the above restrictions, but still can't use IMM
          * operands to math
          */
         if ((shader->devinfo->gen == 6 &&
              (src.file == IMM || src.file == UNIFORM ||
               src.abs || src.negate)) ||
             (shader->devinfo->gen == 7 && src.file == IMM)) {
            const dst_reg tmp = vgrf(src.type);
            MOV(tmp, src);
            return tmp;
         } else {
            return src;
         }
      }

      bblock_t *block;
      exec_node *cursor;

      unsigned _dispatch_width;
      unsigned _group;
      bool force_writemask_all;

      /** Debug annotation info. */
      struct {
         const char *str;
         const void *ir;
      } annotation;
   };
}

#endif
