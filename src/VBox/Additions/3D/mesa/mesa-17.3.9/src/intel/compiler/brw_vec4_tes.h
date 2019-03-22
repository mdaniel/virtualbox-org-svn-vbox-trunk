/*
 * Copyright © 2013 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file brw_vec4_tes.h
 *
 * The vec4 mode tessellation evaluation shader compiler backend.
 */

#ifndef BRW_VEC4_TES_H
#define BRW_VEC4_TES_H

#include "brw_vec4.h"

#ifdef __cplusplus
namespace brw {

class vec4_tes_visitor : public vec4_visitor
{
public:
   vec4_tes_visitor(const struct brw_compiler *compiler,
                   void *log_data,
                   const struct brw_tes_prog_key *key,
                   struct brw_tes_prog_data *prog_data,
                   const nir_shader *nir,
                   void *mem_ctx,
                   int shader_time_index);

protected:
   virtual void nir_emit_intrinsic(nir_intrinsic_instr *instr);

   virtual void setup_payload();
   virtual void emit_prolog();
   virtual void emit_thread_end();

   virtual void emit_urb_write_header(int mrf);
   virtual vec4_instruction *emit_urb_write_opcode(bool complete);

private:
   src_reg input_read_header;
};

} /* namespace brw */
#endif /* __cplusplus */

#endif /* BRW_VEC4_TES_H */
