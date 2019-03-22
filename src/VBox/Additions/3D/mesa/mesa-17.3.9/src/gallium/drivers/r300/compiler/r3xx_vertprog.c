/*
 * Copyright 2009 Nicolai Hähnle <nhaehnle@gmail.com>
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
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "radeon_compiler.h"

#include <stdio.h>

#include "r300_reg.h"

#include "radeon_compiler_util.h"
#include "radeon_dataflow.h"
#include "radeon_program.h"
#include "radeon_program_alu.h"
#include "radeon_swizzle.h"
#include "radeon_emulate_branches.h"
#include "radeon_emulate_loops.h"
#include "radeon_remove_constants.h"

/*
 * Take an already-setup and valid source then swizzle it appropriately to
 * obtain a constant ZERO or ONE source.
 */
#define __CONST(x, y)	\
	(PVS_SRC_OPERAND(t_src_index(vp, &vpi->SrcReg[x]),	\
			   t_swizzle(y),	\
			   t_swizzle(y),	\
			   t_swizzle(y),	\
			   t_swizzle(y),	\
			   t_src_class(vpi->SrcReg[x].File), \
			   RC_MASK_NONE) | (vpi->SrcReg[x].RelAddr << 4))


static unsigned long t_dst_mask(unsigned int mask)
{
	/* RC_MASK_* is equivalent to VSF_FLAG_* */
	return mask & RC_MASK_XYZW;
}

static unsigned long t_dst_class(rc_register_file file)
{
	switch (file) {
	default:
		fprintf(stderr, "%s: Bad register file %i\n", __FUNCTION__, file);
		/* fall-through */
	case RC_FILE_TEMPORARY:
		return PVS_DST_REG_TEMPORARY;
	case RC_FILE_OUTPUT:
		return PVS_DST_REG_OUT;
	case RC_FILE_ADDRESS:
		return PVS_DST_REG_A0;
	}
}

static unsigned long t_dst_index(struct r300_vertex_program_code *vp,
				 struct rc_dst_register *dst)
{
	if (dst->File == RC_FILE_OUTPUT)
		return vp->outputs[dst->Index];

	return dst->Index;
}

static unsigned long t_src_class(rc_register_file file)
{
	switch (file) {
	default:
		fprintf(stderr, "%s: Bad register file %i\n", __FUNCTION__, file);
		/* fall-through */
	case RC_FILE_NONE:
	case RC_FILE_TEMPORARY:
		return PVS_SRC_REG_TEMPORARY;
	case RC_FILE_INPUT:
		return PVS_SRC_REG_INPUT;
	case RC_FILE_CONSTANT:
		return PVS_SRC_REG_CONSTANT;
	}
}

static int t_src_conflict(struct rc_src_register a, struct rc_src_register b)
{
	unsigned long aclass = t_src_class(a.File);
	unsigned long bclass = t_src_class(b.File);

	if (aclass != bclass)
		return 0;
	if (aclass == PVS_SRC_REG_TEMPORARY)
		return 0;

	if (a.RelAddr || b.RelAddr)
		return 1;
	if (a.Index != b.Index)
		return 1;

	return 0;
}

static inline unsigned long t_swizzle(unsigned int swizzle)
{
	/* this is in fact a NOP as the Mesa RC_SWIZZLE_* are all identical to VSF_IN_COMPONENT_* */
	return swizzle;
}

static unsigned long t_src_index(struct r300_vertex_program_code *vp,
				 struct rc_src_register *src)
{
	if (src->File == RC_FILE_INPUT) {
		assert(vp->inputs[src->Index] != -1);
		return vp->inputs[src->Index];
	} else {
		if (src->Index < 0) {
			fprintf(stderr,
				"negative offsets for indirect addressing do not work.\n");
			return 0;
		}
		return src->Index;
	}
}

/* these two functions should probably be merged... */

static unsigned long t_src(struct r300_vertex_program_code *vp,
			   struct rc_src_register *src)
{
	/* src->Negate uses the RC_MASK_ flags from program_instruction.h,
	 * which equal our VSF_FLAGS_ values, so it's safe to just pass it here.
	 */
	return PVS_SRC_OPERAND(t_src_index(vp, src),
			       t_swizzle(GET_SWZ(src->Swizzle, 0)),
			       t_swizzle(GET_SWZ(src->Swizzle, 1)),
			       t_swizzle(GET_SWZ(src->Swizzle, 2)),
			       t_swizzle(GET_SWZ(src->Swizzle, 3)),
			       t_src_class(src->File),
			       src->Negate) |
	       (src->RelAddr << 4) | (src->Abs << 3);
}

static unsigned long t_src_scalar(struct r300_vertex_program_code *vp,
				  struct rc_src_register *src)
{
	/* src->Negate uses the RC_MASK_ flags from program_instruction.h,
	 * which equal our VSF_FLAGS_ values, so it's safe to just pass it here.
	 */
	unsigned int swz = rc_get_scalar_src_swz(src->Swizzle);

	return PVS_SRC_OPERAND(t_src_index(vp, src),
			       t_swizzle(swz),
			       t_swizzle(swz),
			       t_swizzle(swz),
			       t_swizzle(swz),
			       t_src_class(src->File),
			       src->Negate ? RC_MASK_XYZW : RC_MASK_NONE) |
	       (src->RelAddr << 4) | (src->Abs << 3);
}

static int valid_dst(struct r300_vertex_program_code *vp,
			   struct rc_dst_register *dst)
{
	if (dst->File == RC_FILE_OUTPUT && vp->outputs[dst->Index] == -1) {
		return 0;
	} else if (dst->File == RC_FILE_ADDRESS) {
		assert(dst->Index == 0);
	}

	return 1;
}

static void ei_vector1(struct r300_vertex_program_code *vp,
				unsigned int hw_opcode,
				struct rc_sub_instruction *vpi,
				unsigned int * inst)
{
	inst[0] = PVS_OP_DST_OPERAND(hw_opcode,
				     0,
				     0,
				     t_dst_index(vp, &vpi->DstReg),
				     t_dst_mask(vpi->DstReg.WriteMask),
				     t_dst_class(vpi->DstReg.File),
                                     vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	inst[1] = t_src(vp, &vpi->SrcReg[0]);
	inst[2] = __CONST(0, RC_SWIZZLE_ZERO);
	inst[3] = __CONST(0, RC_SWIZZLE_ZERO);
}

static void ei_vector2(struct r300_vertex_program_code *vp,
				unsigned int hw_opcode,
				struct rc_sub_instruction *vpi,
				unsigned int * inst)
{
	inst[0] = PVS_OP_DST_OPERAND(hw_opcode,
				     0,
				     0,
				     t_dst_index(vp, &vpi->DstReg),
				     t_dst_mask(vpi->DstReg.WriteMask),
				     t_dst_class(vpi->DstReg.File),
                                     vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	inst[1] = t_src(vp, &vpi->SrcReg[0]);
	inst[2] = t_src(vp, &vpi->SrcReg[1]);
	inst[3] = __CONST(1, RC_SWIZZLE_ZERO);
}

static void ei_math1(struct r300_vertex_program_code *vp,
				unsigned int hw_opcode,
				struct rc_sub_instruction *vpi,
				unsigned int * inst)
{
	inst[0] = PVS_OP_DST_OPERAND(hw_opcode,
				     1,
				     0,
				     t_dst_index(vp, &vpi->DstReg),
				     t_dst_mask(vpi->DstReg.WriteMask),
				     t_dst_class(vpi->DstReg.File),
                                     vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	inst[1] = t_src_scalar(vp, &vpi->SrcReg[0]);
	inst[2] = __CONST(0, RC_SWIZZLE_ZERO);
	inst[3] = __CONST(0, RC_SWIZZLE_ZERO);
}

static void ei_lit(struct r300_vertex_program_code *vp,
				      struct rc_sub_instruction *vpi,
				      unsigned int * inst)
{
	//LIT TMP 1.Y Z TMP 1{} {X W Z Y} TMP 1{} {Y W Z X} TMP 1{} {Y X Z W}

	inst[0] = PVS_OP_DST_OPERAND(ME_LIGHT_COEFF_DX,
				     1,
				     0,
				     t_dst_index(vp, &vpi->DstReg),
				     t_dst_mask(vpi->DstReg.WriteMask),
				     t_dst_class(vpi->DstReg.File),
                                     vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	/* NOTE: Users swizzling might not work. */
	inst[1] = PVS_SRC_OPERAND(t_src_index(vp, &vpi->SrcReg[0]), t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 0)),	// X
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 3)),	// W
				  PVS_SRC_SELECT_FORCE_0,	// Z
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 1)),	// Y
				  t_src_class(vpi->SrcReg[0].File),
				  vpi->SrcReg[0].Negate ? RC_MASK_XYZW : RC_MASK_NONE) |
	    (vpi->SrcReg[0].RelAddr << 4);
	inst[2] = PVS_SRC_OPERAND(t_src_index(vp, &vpi->SrcReg[0]), t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 1)),	// Y
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 3)),	// W
				  PVS_SRC_SELECT_FORCE_0,	// Z
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 0)),	// X
				  t_src_class(vpi->SrcReg[0].File),
				  vpi->SrcReg[0].Negate ? RC_MASK_XYZW : RC_MASK_NONE) |
	    (vpi->SrcReg[0].RelAddr << 4);
	inst[3] = PVS_SRC_OPERAND(t_src_index(vp, &vpi->SrcReg[0]), t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 1)),	// Y
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 0)),	// X
				  PVS_SRC_SELECT_FORCE_0,	// Z
				  t_swizzle(GET_SWZ(vpi->SrcReg[0].Swizzle, 3)),	// W
				  t_src_class(vpi->SrcReg[0].File),
				  vpi->SrcReg[0].Negate ? RC_MASK_XYZW : RC_MASK_NONE) |
	    (vpi->SrcReg[0].RelAddr << 4);
}

static void ei_mad(struct r300_vertex_program_code *vp,
				      struct rc_sub_instruction *vpi,
				      unsigned int * inst)
{
	unsigned int i;
	/* Remarks about hardware limitations of MAD
	 * (please preserve this comment, as this information is _NOT_
	 * in the documentation provided by AMD).
	 *
	 * As described in the documentation, MAD with three unique temporary
	 * source registers requires the use of the macro version.
	 *
	 * However (and this is not mentioned in the documentation), apparently
	 * the macro version is _NOT_ a full superset of the normal version.
	 * In particular, the macro version does not always work when relative
	 * addressing is used in the source operands.
	 *
	 * This limitation caused incorrect rendering in Sauerbraten's OpenGL
	 * assembly shader path when using medium quality animations
	 * (i.e. animations with matrix blending instead of quaternion blending).
	 *
	 * Unfortunately, I (nha) have been unable to extract a Piglit regression
	 * test for this issue - for some reason, it is possible to have vertex
	 * programs whose prefix is *exactly* the same as the prefix of the
	 * offending program in Sauerbraten up to the offending instruction
	 * without causing any trouble.
	 *
	 * Bottom line: Only use the macro version only when really necessary;
	 * according to AMD docs, this should improve performance by one clock
	 * as a nice side bonus.
	 */
	if (vpi->SrcReg[0].File == RC_FILE_TEMPORARY &&
	    vpi->SrcReg[1].File == RC_FILE_TEMPORARY &&
	    vpi->SrcReg[2].File == RC_FILE_TEMPORARY &&
	    vpi->SrcReg[0].Index != vpi->SrcReg[1].Index &&
	    vpi->SrcReg[0].Index != vpi->SrcReg[2].Index &&
	    vpi->SrcReg[1].Index != vpi->SrcReg[2].Index) {
		inst[0] = PVS_OP_DST_OPERAND(PVS_MACRO_OP_2CLK_MADD,
				0,
				1,
				t_dst_index(vp, &vpi->DstReg),
				t_dst_mask(vpi->DstReg.WriteMask),
				t_dst_class(vpi->DstReg.File),
                                vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	} else {
		inst[0] = PVS_OP_DST_OPERAND(VE_MULTIPLY_ADD,
				0,
				0,
				t_dst_index(vp, &vpi->DstReg),
				t_dst_mask(vpi->DstReg.WriteMask),
				t_dst_class(vpi->DstReg.File),
                                vpi->SaturateMode == RC_SATURATE_ZERO_ONE);

		/* Arguments with constant swizzles still count as a unique
		 * temporary, so we should make sure these arguments share a
		 * register index with one of the other arguments. */
		for (i = 0; i < 3; i++) {
			unsigned int j;
			if (vpi->SrcReg[i].File != RC_FILE_NONE)
				continue;

			for (j = 0; j < 3; j++) {
				if (i != j) {
					vpi->SrcReg[i].Index =
						vpi->SrcReg[j].Index;
					break;
				}
			}
		}
	}
	inst[1] = t_src(vp, &vpi->SrcReg[0]);
	inst[2] = t_src(vp, &vpi->SrcReg[1]);
	inst[3] = t_src(vp, &vpi->SrcReg[2]);
}

static void ei_pow(struct r300_vertex_program_code *vp,
				      struct rc_sub_instruction *vpi,
				      unsigned int * inst)
{
	inst[0] = PVS_OP_DST_OPERAND(ME_POWER_FUNC_FF,
				     1,
				     0,
				     t_dst_index(vp, &vpi->DstReg),
				     t_dst_mask(vpi->DstReg.WriteMask),
				     t_dst_class(vpi->DstReg.File),
                                     vpi->SaturateMode == RC_SATURATE_ZERO_ONE);
	inst[1] = t_src_scalar(vp, &vpi->SrcReg[0]);
	inst[2] = __CONST(0, RC_SWIZZLE_ZERO);
	inst[3] = t_src_scalar(vp, &vpi->SrcReg[1]);
}

static void translate_vertex_program(struct radeon_compiler *c, void *user)
{
	struct r300_vertex_program_compiler *compiler = (struct r300_vertex_program_compiler*)c;
	struct rc_instruction *rci;

	unsigned loops[R500_PVS_MAX_LOOP_DEPTH];
	unsigned loop_depth = 0;

	compiler->code->pos_end = 0;	/* Not supported yet */
	compiler->code->length = 0;
	compiler->code->num_temporaries = 0;

	compiler->SetHwInputOutput(compiler);

	for(rci = compiler->Base.Program.Instructions.Next; rci != &compiler->Base.Program.Instructions; rci = rci->Next) {
		struct rc_sub_instruction *vpi = &rci->U.I;
		unsigned int *inst = compiler->code->body.d + compiler->code->length;
		const struct rc_opcode_info *info = rc_get_opcode_info(vpi->Opcode);

		/* Skip instructions writing to non-existing destination */
		if (!valid_dst(compiler->code, &vpi->DstReg))
			continue;

		if (info->HasDstReg) {
			/* Neither is Saturate. */
			if (vpi->SaturateMode != RC_SATURATE_NONE && !c->is_r500) {
				rc_error(&compiler->Base, "Vertex program does not support the Saturate "
					 "modifier (yet).\n");
			}
		}

		if (compiler->code->length >= c->max_alu_insts * 4) {
			rc_error(&compiler->Base, "Vertex program has too many instructions\n");
			return;
		}

		assert(compiler->Base.is_r500 ||
		       (vpi->Opcode != RC_OPCODE_SEQ &&
			vpi->Opcode != RC_OPCODE_SNE));

		switch (vpi->Opcode) {
		case RC_OPCODE_ADD: ei_vector2(compiler->code, VE_ADD, vpi, inst); break;
		case RC_OPCODE_ARL: ei_vector1(compiler->code, VE_FLT2FIX_DX, vpi, inst); break;
		case RC_OPCODE_ARR: ei_vector1(compiler->code, VE_FLT2FIX_DX_RND, vpi, inst); break;
		case RC_OPCODE_COS: ei_math1(compiler->code, ME_COS, vpi, inst); break;
		case RC_OPCODE_DP4: ei_vector2(compiler->code, VE_DOT_PRODUCT, vpi, inst); break;
		case RC_OPCODE_DST: ei_vector2(compiler->code, VE_DISTANCE_VECTOR, vpi, inst); break;
		case RC_OPCODE_EX2: ei_math1(compiler->code, ME_EXP_BASE2_FULL_DX, vpi, inst); break;
		case RC_OPCODE_EXP: ei_math1(compiler->code, ME_EXP_BASE2_DX, vpi, inst); break;
		case RC_OPCODE_FRC: ei_vector1(compiler->code, VE_FRACTION, vpi, inst); break;
		case RC_OPCODE_LG2: ei_math1(compiler->code, ME_LOG_BASE2_FULL_DX, vpi, inst); break;
		case RC_OPCODE_LIT: ei_lit(compiler->code, vpi, inst); break;
		case RC_OPCODE_LOG: ei_math1(compiler->code, ME_LOG_BASE2_DX, vpi, inst); break;
		case RC_OPCODE_MAD: ei_mad(compiler->code, vpi, inst); break;
		case RC_OPCODE_MAX: ei_vector2(compiler->code, VE_MAXIMUM, vpi, inst); break;
		case RC_OPCODE_MIN: ei_vector2(compiler->code, VE_MINIMUM, vpi, inst); break;
		case RC_OPCODE_MOV: ei_vector1(compiler->code, VE_ADD, vpi, inst); break;
		case RC_OPCODE_MUL: ei_vector2(compiler->code, VE_MULTIPLY, vpi, inst); break;
		case RC_OPCODE_POW: ei_pow(compiler->code, vpi, inst); break;
		case RC_OPCODE_RCP: ei_math1(compiler->code, ME_RECIP_DX, vpi, inst); break;
		case RC_OPCODE_RSQ: ei_math1(compiler->code, ME_RECIP_SQRT_DX, vpi, inst); break;
		case RC_OPCODE_SEQ: ei_vector2(compiler->code, VE_SET_EQUAL, vpi, inst); break;
		case RC_OPCODE_SGE: ei_vector2(compiler->code, VE_SET_GREATER_THAN_EQUAL, vpi, inst); break;
		case RC_OPCODE_SIN: ei_math1(compiler->code, ME_SIN, vpi, inst); break;
		case RC_OPCODE_SLT: ei_vector2(compiler->code, VE_SET_LESS_THAN, vpi, inst); break;
		case RC_OPCODE_SNE: ei_vector2(compiler->code, VE_SET_NOT_EQUAL, vpi, inst); break;
		case RC_OPCODE_BGNLOOP:
		{
			if ((!compiler->Base.is_r500
				&& loop_depth >= R300_VS_MAX_LOOP_DEPTH)
				|| loop_depth >= R500_PVS_MAX_LOOP_DEPTH) {
				rc_error(&compiler->Base,
						"Loops are nested too deep.");
				return;
			}
			loops[loop_depth++] = ((compiler->code->length)/ 4) + 1;
			break;
		}
		case RC_OPCODE_ENDLOOP:
		{
			unsigned int act_addr;
			unsigned int last_addr;
			unsigned int ret_addr;

			ret_addr = loops[--loop_depth];
			act_addr = ret_addr - 1;
			last_addr = (compiler->code->length / 4) - 1;

			if (loop_depth >= R300_VS_MAX_FC_OPS) {
				rc_error(&compiler->Base,
					"Too many flow control instructions.");
				return;
			}
			if (compiler->Base.is_r500) {
				compiler->code->fc_op_addrs.r500
					[compiler->code->num_fc_ops].lw =
					R500_PVS_FC_ACT_ADRS(act_addr)
					| R500_PVS_FC_LOOP_CNT_JMP_INST(0x00ff)
					;
				compiler->code->fc_op_addrs.r500
					[compiler->code->num_fc_ops].uw =
					R500_PVS_FC_LAST_INST(last_addr)
					| R500_PVS_FC_RTN_INST(ret_addr)
					;
			} else {
				compiler->code->fc_op_addrs.r300
					[compiler->code->num_fc_ops] =
					R300_PVS_FC_ACT_ADRS(act_addr)
					| R300_PVS_FC_LOOP_CNT_JMP_INST(0xff)
					| R300_PVS_FC_LAST_INST(last_addr)
					| R300_PVS_FC_RTN_INST(ret_addr)
					;
			}
			compiler->code->fc_loop_index[compiler->code->num_fc_ops] =
				R300_PVS_FC_LOOP_INIT_VAL(0x0)
				| R300_PVS_FC_LOOP_STEP_VAL(0x1)
				;
			compiler->code->fc_ops |= R300_VAP_PVS_FC_OPC_LOOP(
						compiler->code->num_fc_ops);
			compiler->code->num_fc_ops++;

			break;
		}

		case RC_ME_PRED_SET_CLR:
			ei_math1(compiler->code, ME_PRED_SET_CLR, vpi, inst);
			break;

		case RC_ME_PRED_SET_INV:
			ei_math1(compiler->code, ME_PRED_SET_INV, vpi, inst);
			break;

		case RC_ME_PRED_SET_POP:
			ei_math1(compiler->code, ME_PRED_SET_POP, vpi, inst);
			break;

		case RC_ME_PRED_SET_RESTORE:
			ei_math1(compiler->code, ME_PRED_SET_RESTORE, vpi, inst);
			break;

		case RC_ME_PRED_SEQ:
			ei_math1(compiler->code, ME_PRED_SET_EQ, vpi, inst);
			break;

		case RC_ME_PRED_SNEQ:
			ei_math1(compiler->code, ME_PRED_SET_NEQ, vpi, inst);
			break;

		case RC_VE_PRED_SNEQ_PUSH:
			ei_vector2(compiler->code, VE_PRED_SET_NEQ_PUSH,
								vpi, inst);
			break;

		default:
			rc_error(&compiler->Base, "Unknown opcode %s\n", info->Name);
			return;
		}

		if (vpi->DstReg.Pred != RC_PRED_DISABLED) {
			inst[0] |= (PVS_DST_PRED_ENABLE_MASK
						<< PVS_DST_PRED_ENABLE_SHIFT);
			if (vpi->DstReg.Pred == RC_PRED_SET) {
				inst[0] |= (PVS_DST_PRED_SENSE_MASK
						<< PVS_DST_PRED_SENSE_SHIFT);
			}
		}

		/* Update the number of temporaries. */
		if (info->HasDstReg && vpi->DstReg.File == RC_FILE_TEMPORARY &&
		    vpi->DstReg.Index >= compiler->code->num_temporaries)
			compiler->code->num_temporaries = vpi->DstReg.Index + 1;

		for (unsigned i = 0; i < info->NumSrcRegs; i++)
			if (vpi->SrcReg[i].File == RC_FILE_TEMPORARY &&
			    vpi->SrcReg[i].Index >= compiler->code->num_temporaries)
				compiler->code->num_temporaries = vpi->SrcReg[i].Index + 1;

		if (compiler->code->num_temporaries > compiler->Base.max_temp_regs) {
			rc_error(&compiler->Base, "Too many temporaries.\n");
			return;
		}

		compiler->code->length += 4;

		if (compiler->Base.Error)
			return;
	}
}

struct temporary_allocation {
	unsigned int Allocated:1;
	unsigned int HwTemp:15;
	struct rc_instruction * LastRead;
};

static void allocate_temporary_registers(struct radeon_compiler *c, void *user)
{
	struct r300_vertex_program_compiler *compiler = (struct r300_vertex_program_compiler*)c;
	struct rc_instruction *inst;
	struct rc_instruction *end_loop = NULL;
	unsigned int num_orig_temps = 0;
	char hwtemps[RC_REGISTER_MAX_INDEX];
	struct temporary_allocation * ta;
	unsigned int i, j;

	memset(hwtemps, 0, sizeof(hwtemps));

	rc_recompute_ips(c);

	/* Pass 1: Count original temporaries. */
	for(inst = compiler->Base.Program.Instructions.Next; inst != &compiler->Base.Program.Instructions; inst = inst->Next) {
		const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);

		for (i = 0; i < opcode->NumSrcRegs; ++i) {
			if (inst->U.I.SrcReg[i].File == RC_FILE_TEMPORARY) {
				if (inst->U.I.SrcReg[i].Index >= num_orig_temps)
					num_orig_temps = inst->U.I.SrcReg[i].Index + 1;
			}
		}

		if (opcode->HasDstReg) {
			if (inst->U.I.DstReg.File == RC_FILE_TEMPORARY) {
				if (inst->U.I.DstReg.Index >= num_orig_temps)
					num_orig_temps = inst->U.I.DstReg.Index + 1;
			}
		}
	}

	ta = (struct temporary_allocation*)memory_pool_malloc(&compiler->Base.Pool,
			sizeof(struct temporary_allocation) * num_orig_temps);
	memset(ta, 0, sizeof(struct temporary_allocation) * num_orig_temps);

	/* Pass 2: Determine original temporary lifetimes */
	for(inst = compiler->Base.Program.Instructions.Next; inst != &compiler->Base.Program.Instructions; inst = inst->Next) {
		const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);
		/* Instructions inside of loops need to use the ENDLOOP
		 * instruction as their LastRead. */
		if (!end_loop && inst->U.I.Opcode == RC_OPCODE_BGNLOOP) {
			int endloops = 1;
			struct rc_instruction * ptr;
			for(ptr = inst->Next;
				ptr != &compiler->Base.Program.Instructions;
							ptr = ptr->Next){
				if (ptr->U.I.Opcode == RC_OPCODE_BGNLOOP) {
					endloops++;
				} else if (ptr->U.I.Opcode == RC_OPCODE_ENDLOOP) {
					endloops--;
					if (endloops <= 0) {
						end_loop = ptr;
						break;
					}
				}
			}
		}

		if (inst == end_loop) {
			end_loop = NULL;
			continue;
		}

		for (i = 0; i < opcode->NumSrcRegs; ++i) {
			if (inst->U.I.SrcReg[i].File == RC_FILE_TEMPORARY) {
				ta[inst->U.I.SrcReg[i].Index].LastRead = end_loop ? end_loop : inst;
			}
		}
	}

	/* Pass 3: Register allocation */
	for(inst = compiler->Base.Program.Instructions.Next; inst != &compiler->Base.Program.Instructions; inst = inst->Next) {
		const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);

		for (i = 0; i < opcode->NumSrcRegs; ++i) {
			if (inst->U.I.SrcReg[i].File == RC_FILE_TEMPORARY) {
				unsigned int orig = inst->U.I.SrcReg[i].Index;
				inst->U.I.SrcReg[i].Index = ta[orig].HwTemp;

				if (ta[orig].Allocated && inst == ta[orig].LastRead)
					hwtemps[ta[orig].HwTemp] = 0;
			}
		}

		if (opcode->HasDstReg) {
			if (inst->U.I.DstReg.File == RC_FILE_TEMPORARY) {
				unsigned int orig = inst->U.I.DstReg.Index;

				if (!ta[orig].Allocated) {
					for(j = 0; j < c->max_temp_regs; ++j) {
						if (!hwtemps[j])
							break;
					}
					ta[orig].Allocated = 1;
					ta[orig].HwTemp = j;
					hwtemps[ta[orig].HwTemp] = 1;
				}

				inst->U.I.DstReg.Index = ta[orig].HwTemp;
			}
		}
	}
}

/**
 * R3xx-R4xx vertex engine does not support the Absolute source operand modifier
 * and the Saturate opcode modifier. Only Absolute is currently transformed.
 */
static int transform_nonnative_modifiers(
	struct radeon_compiler *c,
	struct rc_instruction *inst,
	void* unused)
{
	const struct rc_opcode_info *opcode = rc_get_opcode_info(inst->U.I.Opcode);
	unsigned i;

	/* Transform ABS(a) to MAX(a, -a). */
	for (i = 0; i < opcode->NumSrcRegs; i++) {
		if (inst->U.I.SrcReg[i].Abs) {
			struct rc_instruction *new_inst;
			unsigned temp;

			inst->U.I.SrcReg[i].Abs = 0;

			temp = rc_find_free_temporary(c);

			new_inst = rc_insert_new_instruction(c, inst->Prev);
			new_inst->U.I.Opcode = RC_OPCODE_MAX;
			new_inst->U.I.DstReg.File = RC_FILE_TEMPORARY;
			new_inst->U.I.DstReg.Index = temp;
			new_inst->U.I.SrcReg[0] = inst->U.I.SrcReg[i];
			new_inst->U.I.SrcReg[1] = inst->U.I.SrcReg[i];
			new_inst->U.I.SrcReg[1].Negate ^= RC_MASK_XYZW;

			memset(&inst->U.I.SrcReg[i], 0, sizeof(inst->U.I.SrcReg[i]));
			inst->U.I.SrcReg[i].File = RC_FILE_TEMPORARY;
			inst->U.I.SrcReg[i].Index = temp;
			inst->U.I.SrcReg[i].Swizzle = RC_SWIZZLE_XYZW;
		}
	}
	return 1;
}

/**
 * Vertex engine cannot read two inputs or two constants at the same time.
 * Introduce intermediate MOVs to temporary registers to account for this.
 */
static int transform_source_conflicts(
	struct radeon_compiler *c,
	struct rc_instruction* inst,
	void* unused)
{
	const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);

	if (opcode->NumSrcRegs == 3) {
		if (t_src_conflict(inst->U.I.SrcReg[1], inst->U.I.SrcReg[2])
		    || t_src_conflict(inst->U.I.SrcReg[0], inst->U.I.SrcReg[2])) {
			int tmpreg = rc_find_free_temporary(c);
			struct rc_instruction * inst_mov = rc_insert_new_instruction(c, inst->Prev);
			inst_mov->U.I.Opcode = RC_OPCODE_MOV;
			inst_mov->U.I.DstReg.File = RC_FILE_TEMPORARY;
			inst_mov->U.I.DstReg.Index = tmpreg;
			inst_mov->U.I.SrcReg[0] = inst->U.I.SrcReg[2];

			reset_srcreg(&inst->U.I.SrcReg[2]);
			inst->U.I.SrcReg[2].File = RC_FILE_TEMPORARY;
			inst->U.I.SrcReg[2].Index = tmpreg;
		}
	}

	if (opcode->NumSrcRegs >= 2) {
		if (t_src_conflict(inst->U.I.SrcReg[1], inst->U.I.SrcReg[0])) {
			int tmpreg = rc_find_free_temporary(c);
			struct rc_instruction * inst_mov = rc_insert_new_instruction(c, inst->Prev);
			inst_mov->U.I.Opcode = RC_OPCODE_MOV;
			inst_mov->U.I.DstReg.File = RC_FILE_TEMPORARY;
			inst_mov->U.I.DstReg.Index = tmpreg;
			inst_mov->U.I.SrcReg[0] = inst->U.I.SrcReg[1];

			reset_srcreg(&inst->U.I.SrcReg[1]);
			inst->U.I.SrcReg[1].File = RC_FILE_TEMPORARY;
			inst->U.I.SrcReg[1].Index = tmpreg;
		}
	}

	return 1;
}

static void rc_vs_add_artificial_outputs(struct radeon_compiler *c, void *user)
{
	struct r300_vertex_program_compiler * compiler = (struct r300_vertex_program_compiler*)c;
	int i;

	for(i = 0; i < 32; ++i) {
		if ((compiler->RequiredOutputs & (1 << i)) &&
		    !(compiler->Base.Program.OutputsWritten & (1 << i))) {
			struct rc_instruction * inst = rc_insert_new_instruction(&compiler->Base, compiler->Base.Program.Instructions.Prev);
			inst->U.I.Opcode = RC_OPCODE_MOV;

			inst->U.I.DstReg.File = RC_FILE_OUTPUT;
			inst->U.I.DstReg.Index = i;
			inst->U.I.DstReg.WriteMask = RC_MASK_XYZW;

			inst->U.I.SrcReg[0].File = RC_FILE_CONSTANT;
			inst->U.I.SrcReg[0].Index = 0;
			inst->U.I.SrcReg[0].Swizzle = RC_SWIZZLE_XYZW;

			compiler->Base.Program.OutputsWritten |= 1 << i;
		}
	}
}

static void dataflow_outputs_mark_used(void * userdata, void * data,
		void (*callback)(void *, unsigned int, unsigned int))
{
	struct r300_vertex_program_compiler * c = userdata;
	int i;

	for(i = 0; i < 32; ++i) {
		if (c->RequiredOutputs & (1 << i))
			callback(data, i, RC_MASK_XYZW);
	}
}

static int swizzle_is_native(rc_opcode opcode, struct rc_src_register reg)
{
	(void) opcode;
	(void) reg;

	return 1;
}

static void transform_negative_addressing(struct r300_vertex_program_compiler *c,
					  struct rc_instruction *arl,
					  struct rc_instruction *end,
					  int min_offset)
{
	struct rc_instruction *inst, *add;
	unsigned const_swizzle;

	/* Transform ARL/ARR */
	add = rc_insert_new_instruction(&c->Base, arl->Prev);
	add->U.I.Opcode = RC_OPCODE_ADD;
	add->U.I.DstReg.File = RC_FILE_TEMPORARY;
	add->U.I.DstReg.Index = rc_find_free_temporary(&c->Base);
	add->U.I.DstReg.WriteMask = RC_MASK_X;
	add->U.I.SrcReg[0] = arl->U.I.SrcReg[0];
	add->U.I.SrcReg[1].File = RC_FILE_CONSTANT;
	add->U.I.SrcReg[1].Index = rc_constants_add_immediate_scalar(&c->Base.Program.Constants,
								     min_offset, &const_swizzle);
	add->U.I.SrcReg[1].Swizzle = const_swizzle;

	arl->U.I.SrcReg[0].File = RC_FILE_TEMPORARY;
	arl->U.I.SrcReg[0].Index = add->U.I.DstReg.Index;
	arl->U.I.SrcReg[0].Swizzle = RC_SWIZZLE_XXXX;

	/* Rewrite offsets up to and excluding inst. */
	for (inst = arl->Next; inst != end; inst = inst->Next) {
		const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);

		for (unsigned i = 0; i < opcode->NumSrcRegs; i++)
			if (inst->U.I.SrcReg[i].RelAddr)
				inst->U.I.SrcReg[i].Index -= min_offset;
	}
}

static void rc_emulate_negative_addressing(struct radeon_compiler *compiler, void *user)
{
	struct r300_vertex_program_compiler * c = (struct r300_vertex_program_compiler*)compiler;
	struct rc_instruction *inst, *lastARL = NULL;
	int min_offset = 0;

	for (inst = c->Base.Program.Instructions.Next; inst != &c->Base.Program.Instructions; inst = inst->Next) {
		const struct rc_opcode_info * opcode = rc_get_opcode_info(inst->U.I.Opcode);

		if (inst->U.I.Opcode == RC_OPCODE_ARL || inst->U.I.Opcode == RC_OPCODE_ARR) {
			if (lastARL != NULL && min_offset < 0)
				transform_negative_addressing(c, lastARL, inst, min_offset);

			lastARL = inst;
			min_offset = 0;
			continue;
		}

		for (unsigned i = 0; i < opcode->NumSrcRegs; i++) {
			if (inst->U.I.SrcReg[i].RelAddr &&
			    inst->U.I.SrcReg[i].Index < 0) {
				/* ARL must precede any indirect addressing. */
				if (!lastARL) {
					rc_error(&c->Base, "Vertex shader: Found relative addressing without ARL/ARR.");
					return;
				}

				if (inst->U.I.SrcReg[i].Index < min_offset)
					min_offset = inst->U.I.SrcReg[i].Index;
			}
		}
	}

	if (lastARL != NULL && min_offset < 0)
		transform_negative_addressing(c, lastARL, inst, min_offset);
}

struct rc_swizzle_caps r300_vertprog_swizzle_caps = {
	.IsNative = &swizzle_is_native,
	.Split = 0 /* should never be called */
};

void r3xx_compile_vertex_program(struct r300_vertex_program_compiler *c)
{
	int is_r500 = c->Base.is_r500;
	int opt = !c->Base.disable_optimizations;

	/* Lists of instruction transformations. */
	struct radeon_program_transformation alu_rewrite_r500[] = {
		{ &r300_transform_vertex_alu, 0 },
		{ &r300_transform_trig_scale_vertex, 0 },
		{ 0, 0 }
	};

	struct radeon_program_transformation alu_rewrite_r300[] = {
		{ &r300_transform_vertex_alu, 0 },
		{ &r300_transform_trig_simple, 0 },
		{ 0, 0 }
	};

	/* Note: These passes have to be done seperately from ALU rewrite,
	 * otherwise non-native ALU instructions with source conflits
	 * or non-native modifiers will not be treated properly.
	 */
	struct radeon_program_transformation emulate_modifiers[] = {
		{ &transform_nonnative_modifiers, 0 },
		{ 0, 0 }
	};

	struct radeon_program_transformation resolve_src_conflicts[] = {
		{ &transform_source_conflicts, 0 },
		{ 0, 0 }
	};

	/* List of compiler passes. */
	struct radeon_compiler_pass vs_list[] = {
		/* NAME				DUMP PREDICATE	FUNCTION			PARAM */
		{"add artificial outputs",	0, 1,		rc_vs_add_artificial_outputs,	NULL},
		{"emulate branches",		1, !is_r500,	rc_emulate_branches,		NULL},
		{"emulate negative addressing", 1, 1,		rc_emulate_negative_addressing,	NULL},
		{"native rewrite",		1, is_r500,	rc_local_transform,		alu_rewrite_r500},
		{"native rewrite",		1, !is_r500,	rc_local_transform,		alu_rewrite_r300},
		{"emulate modifiers",		1, !is_r500,	rc_local_transform,		emulate_modifiers},
		{"deadcode",			1, opt,		rc_dataflow_deadcode,		dataflow_outputs_mark_used},
		{"dataflow optimize",		1, opt,		rc_optimize,			NULL},
		/* This pass must be done after optimizations. */
		{"source conflict resolve",	1, 1,		rc_local_transform,		resolve_src_conflicts},
		{"register allocation",		1, opt,		allocate_temporary_registers,	NULL},
		{"dead constants",		1, 1,		rc_remove_unused_constants,	&c->code->constants_remap_table},
		{"lower control flow opcodes",	1, is_r500,	rc_vert_fc,			NULL},
		{"final code validation",	0, 1,		rc_validate_final_shader,	NULL},
		{"machine code generation",	0, 1,		translate_vertex_program,	NULL},
		{"dump machine code",		0, c->Base.Debug & RC_DBG_LOG, r300_vertex_program_dump,	NULL},
		{NULL, 0, 0, NULL, NULL}
	};

	c->Base.type = RC_VERTEX_PROGRAM;
	c->Base.SwizzleCaps = &r300_vertprog_swizzle_caps;

	rc_run_compiler(&c->Base, vs_list);

	c->code->InputsRead = c->Base.Program.InputsRead;
	c->code->OutputsWritten = c->Base.Program.OutputsWritten;
	rc_constants_copy(&c->code->constants, &c->Base.Program.Constants);
}
