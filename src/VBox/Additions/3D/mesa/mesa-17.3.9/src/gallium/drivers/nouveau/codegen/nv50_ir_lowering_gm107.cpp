/*
 * Copyright 2011 Christoph Bumiller
 *           2014 Red Hat Inc.
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

#include "codegen/nv50_ir.h"
#include "codegen/nv50_ir_build_util.h"

#include "codegen/nv50_ir_target_nvc0.h"
#include "codegen/nv50_ir_lowering_gm107.h"

#include <limits>

namespace nv50_ir {

#define QOP_ADD  0
#define QOP_SUBR 1
#define QOP_SUB  2
#define QOP_MOV2 3

//             UL UR LL LR
#define QUADOP(q, r, s, t)                      \
   ((QOP_##q << 6) | (QOP_##r << 4) |           \
    (QOP_##s << 2) | (QOP_##t << 0))

#define SHFL_BOUND_QUAD 0x1c03

void
GM107LegalizeSSA::handlePFETCH(Instruction *i)
{
   Value *src0;

   if (i->src(0).getFile() == FILE_GPR && !i->srcExists(1))
      return;

   bld.setPosition(i, false);
   src0 = bld.getSSA();

   if (i->srcExists(1))
      bld.mkOp2(OP_ADD , TYPE_U32, src0, i->getSrc(0), i->getSrc(1));
   else
      bld.mkOp1(OP_MOV , TYPE_U32, src0, i->getSrc(0));

   i->setSrc(0, src0);
   i->setSrc(1, NULL);
}

void
GM107LegalizeSSA::handleLOAD(Instruction *i)
{
   if (i->src(0).getFile() != FILE_MEMORY_CONST)
      return;
   if (i->src(0).isIndirect(0))
      return;
   if (typeSizeof(i->dType) != 4)
      return;

   i->op = OP_MOV;
}

bool
GM107LegalizeSSA::visit(Instruction *i)
{
   switch (i->op) {
   case OP_PFETCH:
      handlePFETCH(i);
      break;
   case OP_LOAD:
      handleLOAD(i);
      break;
   default:
      break;
   }
   return true;
}

bool
GM107LoweringPass::handleManualTXD(TexInstruction *i)
{
   static const uint8_t qOps[4][2] =
   {
      { QUADOP(MOV2, ADD,  MOV2, ADD),  QUADOP(MOV2, MOV2, ADD,  ADD) }, // l0
      { QUADOP(SUBR, MOV2, SUBR, MOV2), QUADOP(MOV2, MOV2, ADD,  ADD) }, // l1
      { QUADOP(MOV2, ADD,  MOV2, ADD),  QUADOP(SUBR, SUBR, MOV2, MOV2) }, // l2
      { QUADOP(SUBR, MOV2, SUBR, MOV2), QUADOP(SUBR, SUBR, MOV2, MOV2) }, // l3
   };
   Value *def[4][4];
   Value *crd[3];
   Value *tmp;
   Instruction *tex, *add;
   Value *zero = bld.loadImm(bld.getSSA(), 0);
   int l, c;
   const int dim = i->tex.target.getDim() + i->tex.target.isCube();
   const int array = i->tex.target.isArray();

   i->op = OP_TEX; // no need to clone dPdx/dPdy later

   for (c = 0; c < dim; ++c)
      crd[c] = bld.getScratch();
   tmp = bld.getScratch();

   for (l = 0; l < 4; ++l) {
      Value *src[3], *val;
      // mov coordinates from lane l to all lanes
      bld.mkOp(OP_QUADON, TYPE_NONE, NULL);
      for (c = 0; c < dim; ++c) {
         bld.mkOp3(OP_SHFL, TYPE_F32, crd[c], i->getSrc(c + array),
                   bld.mkImm(l), bld.mkImm(SHFL_BOUND_QUAD));
         add = bld.mkOp2(OP_QUADOP, TYPE_F32, crd[c], crd[c], zero);
         add->subOp = 0x00;
         add->lanes = 1; /* abused for .ndv */
      }

      // add dPdx from lane l to lanes dx
      for (c = 0; c < dim; ++c) {
         bld.mkOp3(OP_SHFL, TYPE_F32, tmp, i->dPdx[c].get(), bld.mkImm(l),
                   bld.mkImm(SHFL_BOUND_QUAD));
         add = bld.mkOp2(OP_QUADOP, TYPE_F32, crd[c], tmp, crd[c]);
         add->subOp = qOps[l][0];
         add->lanes = 1; /* abused for .ndv */
      }

      // add dPdy from lane l to lanes dy
      for (c = 0; c < dim; ++c) {
         bld.mkOp3(OP_SHFL, TYPE_F32, tmp, i->dPdy[c].get(), bld.mkImm(l),
                   bld.mkImm(SHFL_BOUND_QUAD));
         add = bld.mkOp2(OP_QUADOP, TYPE_F32, crd[c], tmp, crd[c]);
         add->subOp = qOps[l][1];
         add->lanes = 1; /* abused for .ndv */
      }

      // normalize cube coordinates if necessary
      if (i->tex.target.isCube()) {
         for (c = 0; c < 3; ++c)
            src[c] = bld.mkOp1v(OP_ABS, TYPE_F32, bld.getSSA(), crd[c]);
         val = bld.getScratch();
         bld.mkOp2(OP_MAX, TYPE_F32, val, src[0], src[1]);
         bld.mkOp2(OP_MAX, TYPE_F32, val, src[2], val);
         bld.mkOp1(OP_RCP, TYPE_F32, val, val);
         for (c = 0; c < 3; ++c)
            src[c] = bld.mkOp2v(OP_MUL, TYPE_F32, bld.getSSA(), crd[c], val);
      } else {
         for (c = 0; c < dim; ++c)
            src[c] = crd[c];
      }

      // texture
      bld.insert(tex = cloneForward(func, i));
      for (c = 0; c < dim; ++c)
         tex->setSrc(c + array, src[c]);
      bld.mkOp(OP_QUADPOP, TYPE_NONE, NULL);

      // save results
      for (c = 0; i->defExists(c); ++c) {
         Instruction *mov;
         def[c][l] = bld.getSSA();
         mov = bld.mkMov(def[c][l], tex->getDef(c));
         mov->fixed = 1;
         mov->lanes = 1 << l;
      }
   }

   for (c = 0; i->defExists(c); ++c) {
      Instruction *u = bld.mkOp(OP_UNION, TYPE_U32, i->getDef(c));
      for (l = 0; l < 4; ++l)
         u->setSrc(l, def[c][l]);
   }

   i->bb->remove(i);
   return true;
}

bool
GM107LoweringPass::handleDFDX(Instruction *insn)
{
   Instruction *shfl;
   int qop = 0, xid = 0;

   switch (insn->op) {
   case OP_DFDX:
      qop = QUADOP(SUB, SUBR, SUB, SUBR);
      xid = 1;
      break;
   case OP_DFDY:
      qop = QUADOP(SUB, SUB, SUBR, SUBR);
      xid = 2;
      break;
   default:
      assert(!"invalid dfdx opcode");
      break;
   }

   shfl = bld.mkOp3(OP_SHFL, TYPE_F32, bld.getScratch(), insn->getSrc(0),
                    bld.mkImm(xid), bld.mkImm(SHFL_BOUND_QUAD));
   shfl->subOp = NV50_IR_SUBOP_SHFL_BFLY;
   insn->op = OP_QUADOP;
   insn->subOp = qop;
   insn->lanes = 0; /* abused for !.ndv */
   insn->setSrc(1, insn->getSrc(0));
   insn->setSrc(0, shfl->getDef(0));
   return true;
}

bool
GM107LoweringPass::handlePFETCH(Instruction *i)
{
   Value *tmp0 = bld.getScratch();
   Value *tmp1 = bld.getScratch();
   Value *tmp2 = bld.getScratch();
   bld.mkOp1(OP_RDSV, TYPE_U32, tmp0, bld.mkSysVal(SV_INVOCATION_INFO, 0));
   bld.mkOp2(OP_SHR , TYPE_U32, tmp1, tmp0, bld.mkImm(16));
   bld.mkOp2(OP_AND , TYPE_U32, tmp0, tmp0, bld.mkImm(0xff));
   bld.mkOp2(OP_AND , TYPE_U32, tmp1, tmp1, bld.mkImm(0xff));
   if (i->getSrc(1))
      bld.mkOp2(OP_ADD , TYPE_U32, tmp2, i->getSrc(0), i->getSrc(1));
   else
      bld.mkOp1(OP_MOV , TYPE_U32, tmp2, i->getSrc(0));
   bld.mkOp3(OP_MAD , TYPE_U32, tmp0, tmp0, tmp1, tmp2);
   i->setSrc(0, tmp0);
   i->setSrc(1, NULL);
   return true;
}

bool
GM107LoweringPass::handlePOPCNT(Instruction *i)
{
   Value *tmp = bld.mkOp2v(OP_AND, i->sType, bld.getScratch(),
                           i->getSrc(0), i->getSrc(1));
   i->setSrc(0, tmp);
   i->setSrc(1, NULL);
   return true;
}

//
// - add quadop dance for texturing
// - put FP outputs in GPRs
// - convert instruction sequences
//
bool
GM107LoweringPass::visit(Instruction *i)
{
   bld.setPosition(i, false);

   if (i->cc != CC_ALWAYS)
      checkPredicate(i);

   switch (i->op) {
   case OP_PFETCH:
      return handlePFETCH(i);
   case OP_DFDX:
   case OP_DFDY:
      return handleDFDX(i);
   case OP_POPCNT:
      return handlePOPCNT(i);
   default:
      return NVC0LoweringPass::visit(i);
   }
}

} // namespace nv50_ir
