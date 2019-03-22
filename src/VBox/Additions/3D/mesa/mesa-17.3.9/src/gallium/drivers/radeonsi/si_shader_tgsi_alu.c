/*
 * Copyright 2016 Advanced Micro Devices, Inc.
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

#include "si_shader_internal.h"
#include "gallivm/lp_bld_const.h"
#include "gallivm/lp_bld_intr.h"
#include "gallivm/lp_bld_gather.h"
#include "tgsi/tgsi_parse.h"
#include "amd/common/ac_llvm_build.h"

static void kill_if_fetch_args(struct lp_build_tgsi_context *bld_base,
			       struct lp_build_emit_data *emit_data)
{
	const struct tgsi_full_instruction *inst = emit_data->inst;
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMBuilderRef builder = ctx->ac.builder;
	unsigned i;
	LLVMValueRef conds[TGSI_NUM_CHANNELS];

	for (i = 0; i < TGSI_NUM_CHANNELS; i++) {
		LLVMValueRef value = lp_build_emit_fetch(bld_base, inst, 0, i);
		conds[i] = LLVMBuildFCmp(builder, LLVMRealOLT, value,
					ctx->ac.f32_0, "");
	}

	/* Or the conditions together */
	for (i = TGSI_NUM_CHANNELS - 1; i > 0; i--) {
		conds[i - 1] = LLVMBuildOr(builder, conds[i], conds[i - 1], "");
	}

	emit_data->dst_type = ctx->voidt;
	emit_data->arg_count = 1;
	emit_data->args[0] = LLVMBuildSelect(builder, conds[0],
					LLVMConstReal(ctx->f32, -1.0f),
					ctx->ac.f32_0, "");
}

static void kil_emit(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMBuilderRef builder = ctx->ac.builder;

	if (ctx->postponed_kill) {
		if (emit_data->inst->Instruction.Opcode == TGSI_OPCODE_KILL_IF) {
			LLVMValueRef val;

			/* Take the minimum kill value. This is the same as OR
			 * between 2 kill values. If the value is negative,
			 * the pixel will be killed.
			 */
			val = LLVMBuildLoad(builder, ctx->postponed_kill, "");
			val = lp_build_emit_llvm_binary(bld_base, TGSI_OPCODE_MIN,
							val, emit_data->args[0]);
			LLVMBuildStore(builder, val, ctx->postponed_kill);
		} else {
			LLVMBuildStore(builder,
				       LLVMConstReal(ctx->f32, -1),
				       ctx->postponed_kill);
		}
		return;
	}

	if (emit_data->inst->Instruction.Opcode == TGSI_OPCODE_KILL_IF)
		ac_build_kill(&ctx->ac, emit_data->args[0]);
	else
		ac_build_kill(&ctx->ac, NULL);
}

static void emit_icmp(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	unsigned pred;
	struct si_shader_context *ctx = si_shader_context(bld_base);

	switch (emit_data->inst->Instruction.Opcode) {
	case TGSI_OPCODE_USEQ:
	case TGSI_OPCODE_U64SEQ: pred = LLVMIntEQ; break;
	case TGSI_OPCODE_USNE:
	case TGSI_OPCODE_U64SNE: pred = LLVMIntNE; break;
	case TGSI_OPCODE_USGE:
	case TGSI_OPCODE_U64SGE: pred = LLVMIntUGE; break;
	case TGSI_OPCODE_USLT:
	case TGSI_OPCODE_U64SLT: pred = LLVMIntULT; break;
	case TGSI_OPCODE_ISGE:
	case TGSI_OPCODE_I64SGE: pred = LLVMIntSGE; break;
	case TGSI_OPCODE_ISLT:
	case TGSI_OPCODE_I64SLT: pred = LLVMIntSLT; break;
	default:
		assert(!"unknown instruction");
		pred = 0;
		break;
	}

	LLVMValueRef v = LLVMBuildICmp(ctx->ac.builder, pred,
			emit_data->args[0], emit_data->args[1],"");

	v = LLVMBuildSExtOrBitCast(ctx->ac.builder, v, ctx->i32, "");

	emit_data->output[emit_data->chan] = v;
}

static void emit_ucmp(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef arg0 = ac_to_integer(&ctx->ac, emit_data->args[0]);

	LLVMValueRef v = LLVMBuildICmp(ctx->ac.builder, LLVMIntNE, arg0,
				       ctx->i32_0, "");

	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(ctx->ac.builder, v, emit_data->args[1], emit_data->args[2], "");
}

static void emit_cmp(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef cond, *args = emit_data->args;

	cond = LLVMBuildFCmp(ctx->ac.builder, LLVMRealOLT, args[0],
			     ctx->ac.f32_0, "");

	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(ctx->ac.builder, cond, args[1], args[2], "");
}

static void emit_set_cond(const struct lp_build_tgsi_action *action,
			  struct lp_build_tgsi_context *bld_base,
			  struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMRealPredicate pred;
	LLVMValueRef cond;

	/* Use ordered for everything but NE (which is usual for
	 * float comparisons)
	 */
	switch (emit_data->inst->Instruction.Opcode) {
	case TGSI_OPCODE_SGE: pred = LLVMRealOGE; break;
	case TGSI_OPCODE_SEQ: pred = LLVMRealOEQ; break;
	case TGSI_OPCODE_SLE: pred = LLVMRealOLE; break;
	case TGSI_OPCODE_SLT: pred = LLVMRealOLT; break;
	case TGSI_OPCODE_SNE: pred = LLVMRealUNE; break;
	case TGSI_OPCODE_SGT: pred = LLVMRealOGT; break;
	default: assert(!"unknown instruction"); pred = 0; break;
	}

	cond = LLVMBuildFCmp(ctx->ac.builder,
		pred, emit_data->args[0], emit_data->args[1], "");

	emit_data->output[emit_data->chan] = LLVMBuildSelect(ctx->ac.builder,
		cond, ctx->ac.f32_1, ctx->ac.f32_0, "");
}

static void emit_fcmp(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMRealPredicate pred;

	/* Use ordered for everything but NE (which is usual for
	 * float comparisons)
	 */
	switch (emit_data->inst->Instruction.Opcode) {
	case TGSI_OPCODE_FSEQ: pred = LLVMRealOEQ; break;
	case TGSI_OPCODE_FSGE: pred = LLVMRealOGE; break;
	case TGSI_OPCODE_FSLT: pred = LLVMRealOLT; break;
	case TGSI_OPCODE_FSNE: pred = LLVMRealUNE; break;
	default: assert(!"unknown instruction"); pred = 0; break;
	}

	LLVMValueRef v = LLVMBuildFCmp(ctx->ac.builder, pred,
			emit_data->args[0], emit_data->args[1],"");

	v = LLVMBuildSExtOrBitCast(ctx->ac.builder, v, ctx->i32, "");

	emit_data->output[emit_data->chan] = v;
}

static void emit_dcmp(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMRealPredicate pred;

	/* Use ordered for everything but NE (which is usual for
	 * float comparisons)
	 */
	switch (emit_data->inst->Instruction.Opcode) {
	case TGSI_OPCODE_DSEQ: pred = LLVMRealOEQ; break;
	case TGSI_OPCODE_DSGE: pred = LLVMRealOGE; break;
	case TGSI_OPCODE_DSLT: pred = LLVMRealOLT; break;
	case TGSI_OPCODE_DSNE: pred = LLVMRealUNE; break;
	default: assert(!"unknown instruction"); pred = 0; break;
	}

	LLVMValueRef v = LLVMBuildFCmp(ctx->ac.builder, pred,
			emit_data->args[0], emit_data->args[1],"");

	v = LLVMBuildSExtOrBitCast(ctx->ac.builder, v, ctx->i32, "");

	emit_data->output[emit_data->chan] = v;
}

static void emit_not(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef v = ac_to_integer(&ctx->ac, emit_data->args[0]);
	emit_data->output[emit_data->chan] = LLVMBuildNot(ctx->ac.builder, v, "");
}

static void emit_arl(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef floor_index =  lp_build_emit_llvm_unary(bld_base, TGSI_OPCODE_FLR, emit_data->args[0]);
	emit_data->output[emit_data->chan] = LLVMBuildFPToSI(ctx->ac.builder,
			floor_index, ctx->i32, "");
}

static void emit_and(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildAnd(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_or(const struct lp_build_tgsi_action *action,
		    struct lp_build_tgsi_context *bld_base,
		    struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildOr(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_uadd(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildAdd(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_udiv(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildUDiv(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_idiv(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildSDiv(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_mod(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildSRem(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_umod(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildURem(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_shl(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildShl(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_ushr(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildLShr(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}
static void emit_ishr(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildAShr(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_xor(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildXor(ctx->ac.builder,
			emit_data->args[0], emit_data->args[1], "");
}

static void emit_ssg(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMBuilderRef builder = ctx->ac.builder;

	LLVMValueRef cmp, val;

	if (emit_data->inst->Instruction.Opcode == TGSI_OPCODE_I64SSG) {
		cmp = LLVMBuildICmp(builder, LLVMIntSGT, emit_data->args[0], bld_base->int64_bld.zero, "");
		val = LLVMBuildSelect(builder, cmp, bld_base->int64_bld.one, emit_data->args[0], "");
		cmp = LLVMBuildICmp(builder, LLVMIntSGE, val, bld_base->int64_bld.zero, "");
		val = LLVMBuildSelect(builder, cmp, val, LLVMConstInt(ctx->i64, -1, true), "");
	} else if (emit_data->inst->Instruction.Opcode == TGSI_OPCODE_ISSG) {
		cmp = LLVMBuildICmp(builder, LLVMIntSGT, emit_data->args[0], ctx->i32_0, "");
		val = LLVMBuildSelect(builder, cmp, ctx->i32_1, emit_data->args[0], "");
		cmp = LLVMBuildICmp(builder, LLVMIntSGE, val, ctx->i32_0, "");
		val = LLVMBuildSelect(builder, cmp, val, LLVMConstInt(ctx->i32, -1, true), "");
	} else if (emit_data->inst->Instruction.Opcode == TGSI_OPCODE_DSSG) {
		cmp = LLVMBuildFCmp(builder, LLVMRealOGT, emit_data->args[0], bld_base->dbl_bld.zero, "");
		val = LLVMBuildSelect(builder, cmp, bld_base->dbl_bld.one, emit_data->args[0], "");
		cmp = LLVMBuildFCmp(builder, LLVMRealOGE, val, bld_base->dbl_bld.zero, "");
		val = LLVMBuildSelect(builder, cmp, val, LLVMConstReal(bld_base->dbl_bld.elem_type, -1), "");
	} else { // float SSG
		cmp = LLVMBuildFCmp(builder, LLVMRealOGT, emit_data->args[0], ctx->ac.f32_0, "");
		val = LLVMBuildSelect(builder, cmp, ctx->ac.f32_1, emit_data->args[0], "");
		cmp = LLVMBuildFCmp(builder, LLVMRealOGE, val, ctx->ac.f32_0, "");
		val = LLVMBuildSelect(builder, cmp, val, LLVMConstReal(ctx->f32, -1), "");
	}

	emit_data->output[emit_data->chan] = val;
}

static void emit_ineg(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildNeg(ctx->ac.builder,
			emit_data->args[0], "");
}

static void emit_dneg(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildFNeg(ctx->ac.builder,
			emit_data->args[0], "");
}

static void emit_frac(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	char *intr;

	if (emit_data->info->opcode == TGSI_OPCODE_FRC)
		intr = "llvm.floor.f32";
	else if (emit_data->info->opcode == TGSI_OPCODE_DFRAC)
		intr = "llvm.floor.f64";
	else {
		assert(0);
		return;
	}

	LLVMValueRef floor = lp_build_intrinsic(ctx->ac.builder, intr, emit_data->dst_type,
						&emit_data->args[0], 1,
						LP_FUNC_ATTR_READNONE);
	emit_data->output[emit_data->chan] = LLVMBuildFSub(ctx->ac.builder,
			emit_data->args[0], floor, "");
}

static void emit_f2i(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildFPToSI(ctx->ac.builder,
			emit_data->args[0], ctx->i32, "");
}

static void emit_f2u(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildFPToUI(ctx->ac.builder,
			emit_data->args[0], ctx->i32, "");
}

static void emit_i2f(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildSIToFP(ctx->ac.builder,
			emit_data->args[0], ctx->f32, "");
}

static void emit_u2f(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] = LLVMBuildUIToFP(ctx->ac.builder,
			emit_data->args[0], ctx->f32, "");
}

static void
build_tgsi_intrinsic_nomem(const struct lp_build_tgsi_action *action,
			   struct lp_build_tgsi_context *bld_base,
			   struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] =
		lp_build_intrinsic(ctx->ac.builder, action->intr_name,
				   emit_data->dst_type, emit_data->args,
				   emit_data->arg_count, LP_FUNC_ATTR_READNONE);
}

static void emit_bfi(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMBuilderRef builder = ctx->ac.builder;
	LLVMValueRef bfi_args[3];
	LLVMValueRef bfi_sm5;
	LLVMValueRef cond;

	// Calculate the bitmask: (((1 << src3) - 1) << src2
	bfi_args[0] = LLVMBuildShl(builder,
				   LLVMBuildSub(builder,
						LLVMBuildShl(builder,
							     ctx->i32_1,
							     emit_data->args[3], ""),
						ctx->i32_1, ""),
				   emit_data->args[2], "");

	bfi_args[1] = LLVMBuildShl(builder, emit_data->args[1],
				   emit_data->args[2], "");

	bfi_args[2] = emit_data->args[0];

	/* Calculate:
	 *   (arg0 & arg1) | (~arg0 & arg2) = arg2 ^ (arg0 & (arg1 ^ arg2)
	 * Use the right-hand side, which the LLVM backend can convert to V_BFI.
	 */
	bfi_sm5 =
		LLVMBuildXor(builder, bfi_args[2],
			LLVMBuildAnd(builder, bfi_args[0],
				LLVMBuildXor(builder, bfi_args[1], bfi_args[2],
					     ""), ""), "");

	/* Since shifts of >= 32 bits are undefined in LLVM IR, the backend
	 * uses the convenient V_BFI lowering for the above, which follows SM5
	 * and disagrees with GLSL semantics when bits (src3) is 32.
	 */
	cond = LLVMBuildICmp(builder, LLVMIntUGE, emit_data->args[3],
			     LLVMConstInt(ctx->i32, 32, 0), "");
	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(builder, cond, emit_data->args[1], bfi_sm5, "");
}

static void emit_bfe(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef bfe_sm5;
	LLVMValueRef cond;

	bfe_sm5 = ac_build_bfe(&ctx->ac, emit_data->args[0],
			       emit_data->args[1], emit_data->args[2],
			       emit_data->info->opcode == TGSI_OPCODE_IBFE);

	/* Correct for GLSL semantics. */
	cond = LLVMBuildICmp(ctx->ac.builder, LLVMIntUGE, emit_data->args[2],
			     LLVMConstInt(ctx->i32, 32, 0), "");
	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(ctx->ac.builder, cond, emit_data->args[0], bfe_sm5, "");
}

/* this is ffs in C */
static void emit_lsb(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMValueRef args[2] = {
		emit_data->args[0],

		/* The value of 1 means that ffs(x=0) = undef, so LLVM won't
		 * add special code to check for x=0. The reason is that
		 * the LLVM behavior for x=0 is different from what we
		 * need here. However, LLVM also assumes that ffs(x) is
		 * in [0, 31], but GLSL expects that ffs(0) = -1, so
		 * a conditional assignment to handle 0 is still required.
		 */
		LLVMConstInt(ctx->i1, 1, 0)
	};

	LLVMValueRef lsb =
		lp_build_intrinsic(ctx->ac.builder, "llvm.cttz.i32",
				emit_data->dst_type, args, ARRAY_SIZE(args),
				LP_FUNC_ATTR_READNONE);

	/* TODO: We need an intrinsic to skip this conditional. */
	/* Check for zero: */
	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(ctx->ac.builder,
				LLVMBuildICmp(ctx->ac.builder, LLVMIntEQ, args[0],
					      ctx->i32_0, ""),
				LLVMConstInt(ctx->i32, -1, 0), lsb, "");
}

/* Find the last bit set. */
static void emit_umsb(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);

	emit_data->output[emit_data->chan] =
		ac_build_umsb(&ctx->ac, emit_data->args[0], emit_data->dst_type);
}

/* Find the last bit opposite of the sign bit. */
static void emit_imsb(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	emit_data->output[emit_data->chan] =
		ac_build_imsb(&ctx->ac, emit_data->args[0],
			      emit_data->dst_type);
}

static void emit_iabs(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);

	emit_data->output[emit_data->chan] =
		lp_build_emit_llvm_binary(bld_base, TGSI_OPCODE_IMAX,
					  emit_data->args[0],
					  LLVMBuildNeg(ctx->ac.builder,
						       emit_data->args[0], ""));
}

static void emit_minmax_int(const struct lp_build_tgsi_action *action,
			    struct lp_build_tgsi_context *bld_base,
			    struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMIntPredicate op;

	switch (emit_data->info->opcode) {
	default:
		assert(0);
	case TGSI_OPCODE_IMAX:
	case TGSI_OPCODE_I64MAX:
		op = LLVMIntSGT;
		break;
	case TGSI_OPCODE_IMIN:
	case TGSI_OPCODE_I64MIN:
		op = LLVMIntSLT;
		break;
	case TGSI_OPCODE_UMAX:
	case TGSI_OPCODE_U64MAX:
		op = LLVMIntUGT;
		break;
	case TGSI_OPCODE_UMIN:
	case TGSI_OPCODE_U64MIN:
		op = LLVMIntULT;
		break;
	}

	emit_data->output[emit_data->chan] =
		LLVMBuildSelect(ctx->ac.builder,
				LLVMBuildICmp(ctx->ac.builder, op, emit_data->args[0],
					      emit_data->args[1], ""),
				emit_data->args[0],
				emit_data->args[1], "");
}

static void pk2h_fetch_args(struct lp_build_tgsi_context *bld_base,
			    struct lp_build_emit_data *emit_data)
{
	emit_data->args[0] = lp_build_emit_fetch(bld_base, emit_data->inst,
						 0, TGSI_CHAN_X);
	emit_data->args[1] = lp_build_emit_fetch(bld_base, emit_data->inst,
						 0, TGSI_CHAN_Y);
}

static void emit_pk2h(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	/* From the GLSL 4.50 spec:
	 *   "The rounding mode cannot be set and is undefined."
	 *
	 * v_cvt_pkrtz_f16 rounds to zero, but it's fastest.
	 */
	emit_data->output[emit_data->chan] =
		ac_build_cvt_pkrtz_f16(&si_shader_context(bld_base)->ac,
				       emit_data->args);
}

static void up2h_fetch_args(struct lp_build_tgsi_context *bld_base,
			    struct lp_build_emit_data *emit_data)
{
	emit_data->args[0] = lp_build_emit_fetch(bld_base, emit_data->inst,
						 0, TGSI_CHAN_X);
}

static void emit_up2h(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);
	LLVMTypeRef i16;
	LLVMValueRef const16, input, val;
	unsigned i;

	i16 = LLVMInt16TypeInContext(ctx->ac.context);
	const16 = LLVMConstInt(ctx->i32, 16, 0);
	input = emit_data->args[0];

	for (i = 0; i < 2; i++) {
		val = i == 1 ? LLVMBuildLShr(ctx->ac.builder, input, const16, "") : input;
		val = LLVMBuildTrunc(ctx->ac.builder, val, i16, "");
		val = ac_to_float(&ctx->ac, val);
		emit_data->output[i] = LLVMBuildFPExt(ctx->ac.builder, val, ctx->f32, "");
	}
}

static void emit_fdiv(const struct lp_build_tgsi_action *action,
		      struct lp_build_tgsi_context *bld_base,
		      struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);

	emit_data->output[emit_data->chan] =
		LLVMBuildFDiv(ctx->ac.builder,
			      emit_data->args[0], emit_data->args[1], "");

	/* Use v_rcp_f32 instead of precise division. */
	if (!LLVMIsConstant(emit_data->output[emit_data->chan]))
		LLVMSetMetadata(emit_data->output[emit_data->chan],
				ctx->fpmath_md_kind, ctx->fpmath_md_2p5_ulp);
}

/* 1/sqrt is translated to rsq for f32 if fp32 denormals are not enabled in
 * the target machine. f64 needs global unsafe math flags to get rsq. */
static void emit_rsq(const struct lp_build_tgsi_action *action,
		     struct lp_build_tgsi_context *bld_base,
		     struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);

	LLVMValueRef sqrt =
		lp_build_emit_llvm_unary(bld_base, TGSI_OPCODE_SQRT,
					 emit_data->args[0]);

	emit_data->output[emit_data->chan] =
		lp_build_emit_llvm_binary(bld_base, TGSI_OPCODE_DIV,
					  ctx->ac.f32_1, sqrt);
}

static void dfracexp_fetch_args(struct lp_build_tgsi_context *bld_base,
				struct lp_build_emit_data *emit_data)
{
	emit_data->args[0] = lp_build_emit_fetch(bld_base, emit_data->inst, 0, TGSI_CHAN_X);
	emit_data->arg_count = 1;
}

static void dfracexp_emit(const struct lp_build_tgsi_action *action,
			  struct lp_build_tgsi_context *bld_base,
			  struct lp_build_emit_data *emit_data)
{
	struct si_shader_context *ctx = si_shader_context(bld_base);

	emit_data->output[emit_data->chan] =
		lp_build_intrinsic(ctx->ac.builder, "llvm.amdgcn.frexp.mant.f64",
				   ctx->ac.f64, &emit_data->args[0], 1, 0);
	emit_data->output1[emit_data->chan] =
		lp_build_intrinsic(ctx->ac.builder, "llvm.amdgcn.frexp.exp.i32.f64",
				   ctx->ac.i32, &emit_data->args[0], 1, 0);
}

void si_shader_context_init_alu(struct lp_build_tgsi_context *bld_base)
{
	lp_set_default_actions(bld_base);

	bld_base->op_actions[TGSI_OPCODE_AND].emit = emit_and;
	bld_base->op_actions[TGSI_OPCODE_ARL].emit = emit_arl;
	bld_base->op_actions[TGSI_OPCODE_BFI].emit = emit_bfi;
	bld_base->op_actions[TGSI_OPCODE_BREV].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_BREV].intr_name = "llvm.bitreverse.i32";
	bld_base->op_actions[TGSI_OPCODE_CEIL].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_CEIL].intr_name = "llvm.ceil.f32";
	bld_base->op_actions[TGSI_OPCODE_CMP].emit = emit_cmp;
	bld_base->op_actions[TGSI_OPCODE_COS].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_COS].intr_name = "llvm.cos.f32";
	bld_base->op_actions[TGSI_OPCODE_DABS].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DABS].intr_name = "llvm.fabs.f64";
	bld_base->op_actions[TGSI_OPCODE_DCEIL].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DCEIL].intr_name = "llvm.ceil.f64";
	bld_base->op_actions[TGSI_OPCODE_DFLR].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DFLR].intr_name = "llvm.floor.f64";
	bld_base->op_actions[TGSI_OPCODE_DFMA].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DFMA].intr_name = "llvm.fma.f64";
	bld_base->op_actions[TGSI_OPCODE_DFRAC].emit = emit_frac;
	bld_base->op_actions[TGSI_OPCODE_DIV].emit = emit_fdiv;
	bld_base->op_actions[TGSI_OPCODE_DNEG].emit = emit_dneg;
	bld_base->op_actions[TGSI_OPCODE_DROUND].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DROUND].intr_name = "llvm.rint.f64";
	bld_base->op_actions[TGSI_OPCODE_DSEQ].emit = emit_dcmp;
	bld_base->op_actions[TGSI_OPCODE_DSGE].emit = emit_dcmp;
	bld_base->op_actions[TGSI_OPCODE_DSLT].emit = emit_dcmp;
	bld_base->op_actions[TGSI_OPCODE_DSNE].emit = emit_dcmp;
	bld_base->op_actions[TGSI_OPCODE_DSSG].emit = emit_ssg;
	bld_base->op_actions[TGSI_OPCODE_DRSQ].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DRSQ].intr_name = "llvm.amdgcn.rsq.f64";
	bld_base->op_actions[TGSI_OPCODE_DSQRT].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DSQRT].intr_name = "llvm.sqrt.f64";
	bld_base->op_actions[TGSI_OPCODE_DTRUNC].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DTRUNC].intr_name = "llvm.trunc.f64";
	bld_base->op_actions[TGSI_OPCODE_DFRACEXP].fetch_args = dfracexp_fetch_args;
	bld_base->op_actions[TGSI_OPCODE_DFRACEXP].emit = dfracexp_emit;
	bld_base->op_actions[TGSI_OPCODE_DLDEXP].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_DLDEXP].intr_name = "llvm.amdgcn.ldexp.f64";
	bld_base->op_actions[TGSI_OPCODE_EX2].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_EX2].intr_name = "llvm.exp2.f32";
	bld_base->op_actions[TGSI_OPCODE_FLR].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_FLR].intr_name = "llvm.floor.f32";
	bld_base->op_actions[TGSI_OPCODE_FMA].emit =
		bld_base->op_actions[TGSI_OPCODE_MAD].emit;
	bld_base->op_actions[TGSI_OPCODE_FRC].emit = emit_frac;
	bld_base->op_actions[TGSI_OPCODE_F2I].emit = emit_f2i;
	bld_base->op_actions[TGSI_OPCODE_F2U].emit = emit_f2u;
	bld_base->op_actions[TGSI_OPCODE_FSEQ].emit = emit_fcmp;
	bld_base->op_actions[TGSI_OPCODE_FSGE].emit = emit_fcmp;
	bld_base->op_actions[TGSI_OPCODE_FSLT].emit = emit_fcmp;
	bld_base->op_actions[TGSI_OPCODE_FSNE].emit = emit_fcmp;
	bld_base->op_actions[TGSI_OPCODE_IABS].emit = emit_iabs;
	bld_base->op_actions[TGSI_OPCODE_IBFE].emit = emit_bfe;
	bld_base->op_actions[TGSI_OPCODE_IDIV].emit = emit_idiv;
	bld_base->op_actions[TGSI_OPCODE_IMAX].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_IMIN].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_IMSB].emit = emit_imsb;
	bld_base->op_actions[TGSI_OPCODE_INEG].emit = emit_ineg;
	bld_base->op_actions[TGSI_OPCODE_ISHR].emit = emit_ishr;
	bld_base->op_actions[TGSI_OPCODE_ISGE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_ISLT].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_ISSG].emit = emit_ssg;
	bld_base->op_actions[TGSI_OPCODE_I2F].emit = emit_i2f;
	bld_base->op_actions[TGSI_OPCODE_KILL_IF].fetch_args = kill_if_fetch_args;
	bld_base->op_actions[TGSI_OPCODE_KILL_IF].emit = kil_emit;
	bld_base->op_actions[TGSI_OPCODE_KILL].emit = kil_emit;
	bld_base->op_actions[TGSI_OPCODE_LDEXP].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_LDEXP].intr_name = "llvm.amdgcn.ldexp.f32";
	bld_base->op_actions[TGSI_OPCODE_LSB].emit = emit_lsb;
	bld_base->op_actions[TGSI_OPCODE_LG2].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_LG2].intr_name = "llvm.log2.f32";
	bld_base->op_actions[TGSI_OPCODE_MAX].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_MAX].intr_name = "llvm.maxnum.f32";
	bld_base->op_actions[TGSI_OPCODE_MIN].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_MIN].intr_name = "llvm.minnum.f32";
	bld_base->op_actions[TGSI_OPCODE_MOD].emit = emit_mod;
	bld_base->op_actions[TGSI_OPCODE_UMSB].emit = emit_umsb;
	bld_base->op_actions[TGSI_OPCODE_NOT].emit = emit_not;
	bld_base->op_actions[TGSI_OPCODE_OR].emit = emit_or;
	bld_base->op_actions[TGSI_OPCODE_PK2H].fetch_args = pk2h_fetch_args;
	bld_base->op_actions[TGSI_OPCODE_PK2H].emit = emit_pk2h;
	bld_base->op_actions[TGSI_OPCODE_POPC].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_POPC].intr_name = "llvm.ctpop.i32";
	bld_base->op_actions[TGSI_OPCODE_POW].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_POW].intr_name = "llvm.pow.f32";
	bld_base->op_actions[TGSI_OPCODE_ROUND].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_ROUND].intr_name = "llvm.rint.f32";
	bld_base->op_actions[TGSI_OPCODE_RSQ].emit = emit_rsq;
	bld_base->op_actions[TGSI_OPCODE_SGE].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SEQ].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SHL].emit = emit_shl;
	bld_base->op_actions[TGSI_OPCODE_SLE].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SLT].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SNE].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SGT].emit = emit_set_cond;
	bld_base->op_actions[TGSI_OPCODE_SIN].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_SIN].intr_name = "llvm.sin.f32";
	bld_base->op_actions[TGSI_OPCODE_SQRT].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_SQRT].intr_name = "llvm.sqrt.f32";
	bld_base->op_actions[TGSI_OPCODE_SSG].emit = emit_ssg;
	bld_base->op_actions[TGSI_OPCODE_TRUNC].emit = build_tgsi_intrinsic_nomem;
	bld_base->op_actions[TGSI_OPCODE_TRUNC].intr_name = "llvm.trunc.f32";
	bld_base->op_actions[TGSI_OPCODE_UADD].emit = emit_uadd;
	bld_base->op_actions[TGSI_OPCODE_UBFE].emit = emit_bfe;
	bld_base->op_actions[TGSI_OPCODE_UDIV].emit = emit_udiv;
	bld_base->op_actions[TGSI_OPCODE_UMAX].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_UMIN].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_UMOD].emit = emit_umod;
	bld_base->op_actions[TGSI_OPCODE_USEQ].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_USGE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_USHR].emit = emit_ushr;
	bld_base->op_actions[TGSI_OPCODE_USLT].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_USNE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_U2F].emit = emit_u2f;
	bld_base->op_actions[TGSI_OPCODE_XOR].emit = emit_xor;
	bld_base->op_actions[TGSI_OPCODE_UCMP].emit = emit_ucmp;
	bld_base->op_actions[TGSI_OPCODE_UP2H].fetch_args = up2h_fetch_args;
	bld_base->op_actions[TGSI_OPCODE_UP2H].emit = emit_up2h;

	bld_base->op_actions[TGSI_OPCODE_I64MAX].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_I64MIN].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_U64MAX].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_U64MIN].emit = emit_minmax_int;
	bld_base->op_actions[TGSI_OPCODE_I64ABS].emit = emit_iabs;
	bld_base->op_actions[TGSI_OPCODE_I64SSG].emit = emit_ssg;
	bld_base->op_actions[TGSI_OPCODE_I64NEG].emit = emit_ineg;

	bld_base->op_actions[TGSI_OPCODE_U64SEQ].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_U64SNE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_U64SGE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_U64SLT].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_I64SGE].emit = emit_icmp;
	bld_base->op_actions[TGSI_OPCODE_I64SLT].emit = emit_icmp;

	bld_base->op_actions[TGSI_OPCODE_U64ADD].emit = emit_uadd;
	bld_base->op_actions[TGSI_OPCODE_U64SHL].emit = emit_shl;
	bld_base->op_actions[TGSI_OPCODE_U64SHR].emit = emit_ushr;
	bld_base->op_actions[TGSI_OPCODE_I64SHR].emit = emit_ishr;

	bld_base->op_actions[TGSI_OPCODE_U64MOD].emit = emit_umod;
	bld_base->op_actions[TGSI_OPCODE_I64MOD].emit = emit_mod;
	bld_base->op_actions[TGSI_OPCODE_U64DIV].emit = emit_udiv;
	bld_base->op_actions[TGSI_OPCODE_I64DIV].emit = emit_idiv;
}
