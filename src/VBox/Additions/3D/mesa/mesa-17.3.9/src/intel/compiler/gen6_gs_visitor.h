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
 *
 */

#ifndef GEN6_GS_VISITOR_H
#define GEN6_GS_VISITOR_H

#include "brw_vec4.h"
#include "brw_vec4_gs_visitor.h"

#ifdef __cplusplus

namespace brw {

class gen6_gs_visitor : public vec4_gs_visitor
{
public:
   gen6_gs_visitor(const struct brw_compiler *comp,
                   void *log_data,
                   struct brw_gs_compile *c,
                   struct brw_gs_prog_data *prog_data,
                   struct gl_program *prog,
                   const nir_shader *shader,
                   void *mem_ctx,
                   bool no_spills,
                   int shader_time_index) :
      vec4_gs_visitor(comp, log_data, c, prog_data, shader, mem_ctx, no_spills,
                      shader_time_index),
      prog(prog)
      {
      }

protected:
   virtual void emit_prolog();
   virtual void emit_thread_end();
   virtual void gs_emit_vertex(int stream_id);
   virtual void gs_end_primitive();
   virtual void emit_urb_write_header(int mrf);
   virtual void emit_urb_write_opcode(bool complete,
                                      int base_mrf,
                                      int last_mrf,
                                      int urb_offset);
   virtual void setup_payload();

private:
   void xfb_write();
   void xfb_program(unsigned vertex, unsigned num_verts);
   void xfb_setup();
   int get_vertex_output_offset_for_varying(int vertex, int varying);

   const struct gl_program *prog;

   src_reg vertex_output;
   src_reg vertex_output_offset;
   src_reg temp;
   src_reg first_vertex;
   src_reg prim_count;
   src_reg primitive_id;

   /* Transform Feedback members */
   src_reg sol_prim_written;
   src_reg svbi;
   src_reg max_svbi;
   src_reg destination_indices;
};

} /* namespace brw */

#endif /* __cplusplus */

#endif /* GEN6_GS_VISITOR_H */
