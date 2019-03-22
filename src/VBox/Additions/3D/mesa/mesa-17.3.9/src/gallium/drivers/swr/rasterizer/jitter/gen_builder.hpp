//============================================================================
// Copyright (C) 2014-2017 Intel Corporation.   All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//
// @file gen_builder.hpp
//
// @brief auto-generated file
//
// DO NOT EDIT
//
// Generation Command Line:
//  ./rasterizer/codegen/gen_llvm_ir_macros.py
//    --input
//    /usr/lib/llvm-3.9/include/llvm/IR/IRBuilder.h
//    --output
//    rasterizer/jitter
//    --gen_h
//
//============================================================================
#pragma once

//============================================================================
// Auto-generated Builder IR Wrappers
//============================================================================

GlobalVariable* GLOBAL_STRING(StringRef Str, const Twine &Name = "", unsigned AddressSpace = 0)
{
    return IRB()->CreateGlobalString(Str, Name, AddressSpace);
}

CallInst* MEMSET(Value *Ptr, Value *Val, uint64_t Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemSet(Ptr, Val, Size, Align, isVolatile, TBAATag, ScopeTag, NoAliasTag);
}

CallInst* MEMSET(Value *Ptr, Value *Val, Value *Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemSet(Ptr, Val, Size, Align, isVolatile, TBAATag, ScopeTag, NoAliasTag);
}

CallInst* MEMCOPY(Value *Dst, Value *Src, uint64_t Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *TBAAStructTag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemCpy(Dst, Src, Size, Align, isVolatile, TBAATag, TBAAStructTag, ScopeTag, NoAliasTag);
}

CallInst* MEMCOPY(Value *Dst, Value *Src, Value *Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *TBAAStructTag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemCpy(Dst, Src, Size, Align, isVolatile, TBAATag, TBAAStructTag, ScopeTag, NoAliasTag);
}

CallInst* MEMMOVE(Value *Dst, Value *Src, uint64_t Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemMove(Dst, Src, Size, Align, isVolatile, TBAATag, ScopeTag, NoAliasTag);
}

CallInst* MEMMOVE(Value *Dst, Value *Src, Value *Size, unsigned Align, bool isVolatile = false, MDNode *TBAATag = nullptr, MDNode *ScopeTag = nullptr, MDNode *NoAliasTag = nullptr)
{
    return IRB()->CreateMemMove(Dst, Src, Size, Align, isVolatile, TBAATag, ScopeTag, NoAliasTag);
}

CallInst* LIFETIME_START(Value *Ptr, ConstantInt *Size = nullptr)
{
    return IRB()->CreateLifetimeStart(Ptr, Size);
}

CallInst* LIFETIME_END(Value *Ptr, ConstantInt *Size = nullptr)
{
    return IRB()->CreateLifetimeEnd(Ptr, Size);
}

CallInst* MASKED_LOAD(Value *Ptr, unsigned Align, Value *Mask, Value *PassThru = nullptr, const Twine &Name = "")
{
    return IRB()->CreateMaskedLoad(Ptr, Align, Mask, PassThru, Name);
}

CallInst* MASKED_STORE(Value *Val, Value *Ptr, unsigned Align, Value *Mask)
{
    return IRB()->CreateMaskedStore(Val, Ptr, Align, Mask);
}

CallInst* MASKED_GATHER(Value *Ptrs, unsigned Align, Value *Mask = nullptr, Value *PassThru = nullptr, const Twine& Name = "")
{
    return IRB()->CreateMaskedGather(Ptrs, Align, Mask, PassThru, Name);
}

CallInst* MASKED_SCATTER(Value *Val, Value *Ptrs, unsigned Align, Value *Mask = nullptr)
{
    return IRB()->CreateMaskedScatter(Val, Ptrs, Align, Mask);
}

CallInst* ASSUMPTION(Value *Cond)
{
    return IRB()->CreateAssumption(Cond);
}

CallInst* GC_STATEPOINT_CALL(uint64_t ID, uint32_t NumPatchBytes, Value *ActualCallee, ArrayRef<Value *> CallArgs, ArrayRef<Value *> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointCall(ID, NumPatchBytes, ActualCallee, CallArgs, DeoptArgs, GCArgs, Name);
}

CallInst* GC_STATEPOINT_CALL(uint64_t ID, uint32_t NumPatchBytes, Value *ActualCallee, uint32_t Flags, ArrayRef<Use> CallArgs, ArrayRef<Use> TransitionArgs, ArrayRef<Use> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointCall(ID, NumPatchBytes, ActualCallee, Flags, CallArgs, TransitionArgs, DeoptArgs, GCArgs, Name);
}

CallInst* GC_STATEPOINT_CALL(uint64_t ID, uint32_t NumPatchBytes, Value *ActualCallee, ArrayRef<Use> CallArgs, ArrayRef<Value *> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointCall(ID, NumPatchBytes, ActualCallee, CallArgs, DeoptArgs, GCArgs, Name);
}

InvokeInst* GC_STATEPOINT_INVOKE(uint64_t ID, uint32_t NumPatchBytes, Value *ActualInvokee, BasicBlock *NormalDest, BasicBlock *UnwindDest, ArrayRef<Value *> InvokeArgs, ArrayRef<Value *> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointInvoke(ID, NumPatchBytes, ActualInvokee, NormalDest, UnwindDest, InvokeArgs, DeoptArgs, GCArgs, Name);
}

InvokeInst* GC_STATEPOINT_INVOKE(uint64_t ID, uint32_t NumPatchBytes, Value *ActualInvokee, BasicBlock *NormalDest, BasicBlock *UnwindDest, uint32_t Flags, ArrayRef<Use> InvokeArgs, ArrayRef<Use> TransitionArgs, ArrayRef<Use> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointInvoke(ID, NumPatchBytes, ActualInvokee, NormalDest, UnwindDest, Flags, InvokeArgs, TransitionArgs, DeoptArgs, GCArgs, Name);
}

InvokeInst* GC_STATEPOINT_INVOKE(uint64_t ID, uint32_t NumPatchBytes, Value *ActualInvokee, BasicBlock *NormalDest, BasicBlock *UnwindDest, ArrayRef<Use> InvokeArgs, ArrayRef<Value *> DeoptArgs, ArrayRef<Value *> GCArgs, const Twine &Name = "")
{
    return IRB()->CreateGCStatepointInvoke(ID, NumPatchBytes, ActualInvokee, NormalDest, UnwindDest, InvokeArgs, DeoptArgs, GCArgs, Name);
}

CallInst* GC_RESULT(Instruction *Statepoint, Type *ResultType, const Twine &Name = "")
{
    return IRB()->CreateGCResult(Statepoint, ResultType, Name);
}

CallInst* GC_RELOCATE(Instruction *Statepoint, int BaseOffset, int DerivedOffset, Type *ResultType, const Twine &Name = "")
{
    return IRB()->CreateGCRelocate(Statepoint, BaseOffset, DerivedOffset, ResultType, Name);
}

ReturnInst* RET_VOID()
{
    return IRB()->CreateRetVoid();
}

ReturnInst* RET(Value *V)
{
    return IRB()->CreateRet(V);
}

ReturnInst* AGGREGATE_RET(Value *const *retVals, unsigned N)
{
    return IRB()->CreateAggregateRet(retVals, N);
}

BranchInst* BR(BasicBlock *Dest)
{
    return IRB()->CreateBr(Dest);
}

BranchInst* COND_BR(Value *Cond, BasicBlock *True, BasicBlock *False, MDNode *BranchWeights = nullptr, MDNode *Unpredictable = nullptr)
{
    return IRB()->CreateCondBr(Cond, True, False, BranchWeights, Unpredictable);
}

SwitchInst* SWITCH(Value *V, BasicBlock *Dest, unsigned NumCases = 10, MDNode *BranchWeights = nullptr, MDNode *Unpredictable = nullptr)
{
    return IRB()->CreateSwitch(V, Dest, NumCases, BranchWeights, Unpredictable);
}

IndirectBrInst* INDIRECT_BR(Value *Addr, unsigned NumDests = 10)
{
    return IRB()->CreateIndirectBr(Addr, NumDests);
}

InvokeInst* INVOKE(Value *Callee, BasicBlock *NormalDest, BasicBlock *UnwindDest, ArrayRef<Value *> Args = None, const Twine &Name = "")
{
    return IRB()->CreateInvoke(Callee, NormalDest, UnwindDest, Args, Name);
}

InvokeInst* INVOKE(Value *Callee, BasicBlock *NormalDest, BasicBlock *UnwindDest, ArrayRef<Value *> Args, ArrayRef<OperandBundleDef> OpBundles, const Twine &Name = "")
{
    return IRB()->CreateInvoke(Callee, NormalDest, UnwindDest, Args, OpBundles, Name);
}

ResumeInst* RESUME(Value *Exn)
{
    return IRB()->CreateResume(Exn);
}

CleanupReturnInst* CLEANUP_RET(CleanupPadInst *CleanupPad, BasicBlock *UnwindBB = nullptr)
{
    return IRB()->CreateCleanupRet(CleanupPad, UnwindBB);
}

CatchSwitchInst* CATCH_SWITCH(Value *ParentPad, BasicBlock *UnwindBB, unsigned NumHandlers, const Twine &Name = "")
{
    return IRB()->CreateCatchSwitch(ParentPad, UnwindBB, NumHandlers, Name);
}

CatchPadInst* CATCH_PAD(Value *ParentPad, ArrayRef<Value *> Args, const Twine &Name = "")
{
    return IRB()->CreateCatchPad(ParentPad, Args, Name);
}

CleanupPadInst* CLEANUP_PAD(Value *ParentPad, ArrayRef<Value *> Args = None, const Twine &Name = "")
{
    return IRB()->CreateCleanupPad(ParentPad, Args, Name);
}

CatchReturnInst* CATCH_RET(CatchPadInst *CatchPad, BasicBlock *BB)
{
    return IRB()->CreateCatchRet(CatchPad, BB);
}

UnreachableInst* UNREACHABLE()
{
    return IRB()->CreateUnreachable();
}

Value* ADD(Value *LHS, Value *RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateAdd(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* NSW_ADD(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNSWAdd(LHS, RHS, Name);
}

Value* NUW_ADD(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNUWAdd(LHS, RHS, Name);
}

Value* FADD(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFAdd(LHS, RHS, Name, FPMathTag);
}

Value* SUB(Value *LHS, Value *RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateSub(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* NSW_SUB(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNSWSub(LHS, RHS, Name);
}

Value* NUW_SUB(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNUWSub(LHS, RHS, Name);
}

Value* FSUB(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFSub(LHS, RHS, Name, FPMathTag);
}

Value* MUL(Value *LHS, Value *RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateMul(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* NSW_MUL(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNSWMul(LHS, RHS, Name);
}

Value* NUW_MUL(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateNUWMul(LHS, RHS, Name);
}

Value* FMUL(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFMul(LHS, RHS, Name, FPMathTag);
}

Value* UDIV(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateUDiv(LHS, RHS, Name, isExact);
}

Value* EXACT_U_DIV(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateExactUDiv(LHS, RHS, Name);
}

Value* SDIV(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateSDiv(LHS, RHS, Name, isExact);
}

Value* EXACT_S_DIV(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateExactSDiv(LHS, RHS, Name);
}

Value* FDIV(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFDiv(LHS, RHS, Name, FPMathTag);
}

Value* UREM(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateURem(LHS, RHS, Name);
}

Value* SREM(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateSRem(LHS, RHS, Name);
}

Value* FREM(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFRem(LHS, RHS, Name, FPMathTag);
}

Value* SHL(Value *LHS, Value *RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateShl(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* SHL(Value *LHS, const APInt &RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateShl(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* SHL(Value *LHS, uint64_t RHS, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateShl(LHS, RHS, Name, HasNUW, HasNSW);
}

Value* LSHR(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateLShr(LHS, RHS, Name, isExact);
}

Value* LSHR(Value *LHS, const APInt &RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateLShr(LHS, RHS, Name, isExact);
}

Value* LSHR(Value *LHS, uint64_t RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateLShr(LHS, RHS, Name, isExact);
}

Value* ASHR(Value *LHS, Value *RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateAShr(LHS, RHS, Name, isExact);
}

Value* ASHR(Value *LHS, const APInt &RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateAShr(LHS, RHS, Name, isExact);
}

Value* ASHR(Value *LHS, uint64_t RHS, const Twine &Name = "", bool isExact = false)
{
    return IRB()->CreateAShr(LHS, RHS, Name, isExact);
}

Value* AND(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateAnd(LHS, RHS, Name);
}

Value* AND(Value *LHS, const APInt &RHS, const Twine &Name = "")
{
    return IRB()->CreateAnd(LHS, RHS, Name);
}

Value* AND(Value *LHS, uint64_t RHS, const Twine &Name = "")
{
    return IRB()->CreateAnd(LHS, RHS, Name);
}

Value* OR(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateOr(LHS, RHS, Name);
}

Value* OR(Value *LHS, const APInt &RHS, const Twine &Name = "")
{
    return IRB()->CreateOr(LHS, RHS, Name);
}

Value* OR(Value *LHS, uint64_t RHS, const Twine &Name = "")
{
    return IRB()->CreateOr(LHS, RHS, Name);
}

Value* XOR(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateXor(LHS, RHS, Name);
}

Value* XOR(Value *LHS, const APInt &RHS, const Twine &Name = "")
{
    return IRB()->CreateXor(LHS, RHS, Name);
}

Value* XOR(Value *LHS, uint64_t RHS, const Twine &Name = "")
{
    return IRB()->CreateXor(LHS, RHS, Name);
}

Value* BINOP(Instruction::BinaryOps Opc, Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateBinOp(Opc, LHS, RHS, Name, FPMathTag);
}

Value* NEG(Value *V, const Twine &Name = "", bool HasNUW = false, bool HasNSW = false)
{
    return IRB()->CreateNeg(V, Name, HasNUW, HasNSW);
}

Value* NSW_NEG(Value *V, const Twine &Name = "")
{
    return IRB()->CreateNSWNeg(V, Name);
}

Value* NUW_NEG(Value *V, const Twine &Name = "")
{
    return IRB()->CreateNUWNeg(V, Name);
}

Value* FNEG(Value *V, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFNeg(V, Name, FPMathTag);
}

Value* NOT(Value *V, const Twine &Name = "")
{
    return IRB()->CreateNot(V, Name);
}

AllocaInst* ALLOCA(Type *Ty, Value *ArraySize = nullptr, const Twine &Name = "")
{
    return IRB()->CreateAlloca(Ty, ArraySize, Name);
}

LoadInst* LOAD(Value *Ptr, const char *Name)
{
    return IRB()->CreateLoad(Ptr, Name);
}

LoadInst* LOAD(Value *Ptr, const Twine &Name = "")
{
    return IRB()->CreateLoad(Ptr, Name);
}

LoadInst* LOAD(Type *Ty, Value *Ptr, const Twine &Name = "")
{
    return IRB()->CreateLoad(Ty, Ptr, Name);
}

LoadInst* LOAD(Value *Ptr, bool isVolatile, const Twine &Name = "")
{
    return IRB()->CreateLoad(Ptr, isVolatile, Name);
}

StoreInst* STORE(Value *Val, Value *Ptr, bool isVolatile = false)
{
    return IRB()->CreateStore(Val, Ptr, isVolatile);
}

LoadInst* ALIGNED_LOAD(Value *Ptr, unsigned Align, const char *Name)
{
    return IRB()->CreateAlignedLoad(Ptr, Align, Name);
}

LoadInst* ALIGNED_LOAD(Value *Ptr, unsigned Align, const Twine &Name = "")
{
    return IRB()->CreateAlignedLoad(Ptr, Align, Name);
}

LoadInst* ALIGNED_LOAD(Value *Ptr, unsigned Align, bool isVolatile, const Twine &Name = "")
{
    return IRB()->CreateAlignedLoad(Ptr, Align, isVolatile, Name);
}

StoreInst* ALIGNED_STORE(Value *Val, Value *Ptr, unsigned Align, bool isVolatile = false)
{
    return IRB()->CreateAlignedStore(Val, Ptr, Align, isVolatile);
}

Value* GEPA(Value *Ptr, ArrayRef<Value *> IdxList, const Twine &Name = "")
{
    return IRB()->CreateGEP(Ptr, IdxList, Name);
}

Value* GEPA(Type *Ty, Value *Ptr, ArrayRef<Value *> IdxList, const Twine &Name = "")
{
    return IRB()->CreateGEP(Ty, Ptr, IdxList, Name);
}

Value* IN_BOUNDS_GEP(Value *Ptr, ArrayRef<Value *> IdxList, const Twine &Name = "")
{
    return IRB()->CreateInBoundsGEP(Ptr, IdxList, Name);
}

Value* IN_BOUNDS_GEP(Type *Ty, Value *Ptr, ArrayRef<Value *> IdxList, const Twine &Name = "")
{
    return IRB()->CreateInBoundsGEP(Ty, Ptr, IdxList, Name);
}

Value* GEP(Value *Ptr, Value *Idx, const Twine &Name = "")
{
    return IRB()->CreateGEP(Ptr, Idx, Name);
}

Value* GEP(Type *Ty, Value *Ptr, Value *Idx, const Twine &Name = "")
{
    return IRB()->CreateGEP(Ty, Ptr, Idx, Name);
}

Value* IN_BOUNDS_GEP(Type *Ty, Value *Ptr, Value *Idx, const Twine &Name = "")
{
    return IRB()->CreateInBoundsGEP(Ty, Ptr, Idx, Name);
}

Value* CONST_GEP1_32(Value *Ptr, unsigned Idx0, const Twine &Name = "")
{
    return IRB()->CreateConstGEP1_32(Ptr, Idx0, Name);
}

Value* CONST_GEP1_32(Type *Ty, Value *Ptr, unsigned Idx0, const Twine &Name = "")
{
    return IRB()->CreateConstGEP1_32(Ty, Ptr, Idx0, Name);
}

Value* CONST_IN_BOUNDS_GEP1_32(Type *Ty, Value *Ptr, unsigned Idx0, const Twine &Name = "")
{
    return IRB()->CreateConstInBoundsGEP1_32(Ty, Ptr, Idx0, Name);
}

Value* CONST_GEP2_32(Type *Ty, Value *Ptr, unsigned Idx0, unsigned Idx1, const Twine &Name = "")
{
    return IRB()->CreateConstGEP2_32(Ty, Ptr, Idx0, Idx1, Name);
}

Value* CONST_IN_BOUNDS_GEP2_32(Type *Ty, Value *Ptr, unsigned Idx0, unsigned Idx1, const Twine &Name = "")
{
    return IRB()->CreateConstInBoundsGEP2_32(Ty, Ptr, Idx0, Idx1, Name);
}

Value* CONST_GEP1_64(Value *Ptr, uint64_t Idx0, const Twine &Name = "")
{
    return IRB()->CreateConstGEP1_64(Ptr, Idx0, Name);
}

Value* CONST_IN_BOUNDS_GEP1_64(Value *Ptr, uint64_t Idx0, const Twine &Name = "")
{
    return IRB()->CreateConstInBoundsGEP1_64(Ptr, Idx0, Name);
}

Value* CONST_GEP2_64(Value *Ptr, uint64_t Idx0, uint64_t Idx1, const Twine &Name = "")
{
    return IRB()->CreateConstGEP2_64(Ptr, Idx0, Idx1, Name);
}

Value* CONST_IN_BOUNDS_GEP2_64(Value *Ptr, uint64_t Idx0, uint64_t Idx1, const Twine &Name = "")
{
    return IRB()->CreateConstInBoundsGEP2_64(Ptr, Idx0, Idx1, Name);
}

Value* STRUCT_GEP(Type *Ty, Value *Ptr, unsigned Idx, const Twine &Name = "")
{
    return IRB()->CreateStructGEP(Ty, Ptr, Idx, Name);
}

Value* GLOBAL_STRING_PTR(StringRef Str, const Twine &Name = "", unsigned AddressSpace = 0)
{
    return IRB()->CreateGlobalStringPtr(Str, Name, AddressSpace);
}

Value* TRUNC(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateTrunc(V, DestTy, Name);
}

Value* Z_EXT(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateZExt(V, DestTy, Name);
}

Value* S_EXT(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateSExt(V, DestTy, Name);
}

Value* Z_EXT_OR_TRUNC(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateZExtOrTrunc(V, DestTy, Name);
}

Value* S_EXT_OR_TRUNC(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateSExtOrTrunc(V, DestTy, Name);
}

Value* FP_TO_UI(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateFPToUI(V, DestTy, Name);
}

Value* FP_TO_SI(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateFPToSI(V, DestTy, Name);
}

Value* UI_TO_FP(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateUIToFP(V, DestTy, Name);
}

Value* SI_TO_FP(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateSIToFP(V, DestTy, Name);
}

Value* FP_TRUNC(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateFPTrunc(V, DestTy, Name);
}

Value* FP_EXT(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateFPExt(V, DestTy, Name);
}

Value* PTR_TO_INT(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreatePtrToInt(V, DestTy, Name);
}

Value* INT_TO_PTR(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateIntToPtr(V, DestTy, Name);
}

Value* BITCAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateBitCast(V, DestTy, Name);
}

Value* ADDR_SPACE_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateAddrSpaceCast(V, DestTy, Name);
}

Value* Z_EXT_OR_BIT_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateZExtOrBitCast(V, DestTy, Name);
}

Value* S_EXT_OR_BIT_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateSExtOrBitCast(V, DestTy, Name);
}

Value* TRUNC_OR_BIT_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateTruncOrBitCast(V, DestTy, Name);
}

Value* CAST(Instruction::CastOps Op, Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateCast(Op, V, DestTy, Name);
}

Value* POINTER_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreatePointerCast(V, DestTy, Name);
}

Value* POINTER_BIT_CAST_OR_ADDR_SPACE_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreatePointerBitCastOrAddrSpaceCast(V, DestTy, Name);
}

Value* INT_CAST(Value *V, Type *DestTy, bool isSigned, const Twine &Name = "")
{
    return IRB()->CreateIntCast(V, DestTy, isSigned, Name);
}

Value* BIT_OR_POINTER_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateBitOrPointerCast(V, DestTy, Name);
}

Value* FP_CAST(Value *V, Type *DestTy, const Twine &Name = "")
{
    return IRB()->CreateFPCast(V, DestTy, Name);
}

Value* ICMP_EQ(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpEQ(LHS, RHS, Name);
}

Value* ICMP_NE(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpNE(LHS, RHS, Name);
}

Value* ICMP_UGT(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpUGT(LHS, RHS, Name);
}

Value* ICMP_UGE(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpUGE(LHS, RHS, Name);
}

Value* ICMP_ULT(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpULT(LHS, RHS, Name);
}

Value* ICMP_ULE(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpULE(LHS, RHS, Name);
}

Value* ICMP_SGT(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpSGT(LHS, RHS, Name);
}

Value* ICMP_SGE(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpSGE(LHS, RHS, Name);
}

Value* ICMP_SLT(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpSLT(LHS, RHS, Name);
}

Value* ICMP_SLE(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmpSLE(LHS, RHS, Name);
}

Value* FCMP_OEQ(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpOEQ(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_OGT(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpOGT(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_OGE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpOGE(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_OLT(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpOLT(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_OLE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpOLE(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_ONE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpONE(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_ORD(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpORD(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_UNO(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpUNO(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_UEQ(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpUEQ(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_UGT(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpUGT(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_UGE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpUGE(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_ULT(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpULT(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_ULE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpULE(LHS, RHS, Name, FPMathTag);
}

Value* FCMP_UNE(Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmpUNE(LHS, RHS, Name, FPMathTag);
}

Value* ICMP(CmpInst::Predicate P, Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreateICmp(P, LHS, RHS, Name);
}

Value* FCMP(CmpInst::Predicate P, Value *LHS, Value *RHS, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateFCmp(P, LHS, RHS, Name, FPMathTag);
}

PHINode* PHI(Type *Ty, unsigned NumReservedValues, const Twine &Name = "")
{
    return IRB()->CreatePHI(Ty, NumReservedValues, Name);
}

CallInst* CALLA(Value *Callee, ArrayRef<Value *> Args = None, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateCall(Callee, Args, Name, FPMathTag);
}

CallInst* CALLA(llvm::FunctionType *FTy, Value *Callee, ArrayRef<Value *> Args, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateCall(FTy, Callee, Args, Name, FPMathTag);
}

CallInst* CALLA(Value *Callee, ArrayRef<Value *> Args, ArrayRef<OperandBundleDef> OpBundles, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateCall(Callee, Args, OpBundles, Name, FPMathTag);
}

CallInst* CALLA(Function *Callee, ArrayRef<Value *> Args, const Twine &Name = "", MDNode *FPMathTag = nullptr)
{
    return IRB()->CreateCall(Callee, Args, Name, FPMathTag);
}

Value* SELECT(Value *C, Value *True, Value *False, const Twine &Name = "", Instruction *MDFrom = nullptr)
{
    return IRB()->CreateSelect(C, True, False, Name, MDFrom);
}

VAArgInst* VA_ARG(Value *List, Type *Ty, const Twine &Name = "")
{
    return IRB()->CreateVAArg(List, Ty, Name);
}

Value* VEXTRACT(Value *Vec, Value *Idx, const Twine &Name = "")
{
    return IRB()->CreateExtractElement(Vec, Idx, Name);
}

Value* VEXTRACT(Value *Vec, uint64_t Idx, const Twine &Name = "")
{
    return IRB()->CreateExtractElement(Vec, Idx, Name);
}

Value* VINSERT(Value *Vec, Value *NewElt, Value *Idx, const Twine &Name = "")
{
    return IRB()->CreateInsertElement(Vec, NewElt, Idx, Name);
}

Value* VINSERT(Value *Vec, Value *NewElt, uint64_t Idx, const Twine &Name = "")
{
    return IRB()->CreateInsertElement(Vec, NewElt, Idx, Name);
}

Value* VSHUFFLE(Value *V1, Value *V2, Value *Mask, const Twine &Name = "")
{
    return IRB()->CreateShuffleVector(V1, V2, Mask, Name);
}

Value* VSHUFFLE(Value *V1, Value *V2, ArrayRef<uint32_t> IntMask, const Twine &Name = "")
{
    return IRB()->CreateShuffleVector(V1, V2, IntMask, Name);
}

Value* EXTRACT_VALUE(Value *Agg, ArrayRef<unsigned> Idxs, const Twine &Name = "")
{
    return IRB()->CreateExtractValue(Agg, Idxs, Name);
}

Value* INSERT_VALUE(Value *Agg, Value *Val, ArrayRef<unsigned> Idxs, const Twine &Name = "")
{
    return IRB()->CreateInsertValue(Agg, Val, Idxs, Name);
}

LandingPadInst* LANDING_PAD(Type *Ty, unsigned NumClauses, const Twine &Name = "")
{
    return IRB()->CreateLandingPad(Ty, NumClauses, Name);
}

Value* IS_NULL(Value *Arg, const Twine &Name = "")
{
    return IRB()->CreateIsNull(Arg, Name);
}

Value* IS_NOT_NULL(Value *Arg, const Twine &Name = "")
{
    return IRB()->CreateIsNotNull(Arg, Name);
}

Value* PTR_DIFF(Value *LHS, Value *RHS, const Twine &Name = "")
{
    return IRB()->CreatePtrDiff(LHS, RHS, Name);
}

Value* INVARIANT_GROUP_BARRIER(Value *Ptr)
{
    return IRB()->CreateInvariantGroupBarrier(Ptr);
}

Value* VECTOR_SPLAT(unsigned NumElts, Value *V, const Twine &Name = "")
{
    return IRB()->CreateVectorSplat(NumElts, V, Name);
}

Value* EXTRACT_INTEGER(const DataLayout &DL, Value *From, IntegerType *ExtractedTy, uint64_t Offset, const Twine &Name)
{
    return IRB()->CreateExtractInteger(DL, From, ExtractedTy, Offset, Name);
}

CallInst* ALIGNMENT_ASSUMPTION(const DataLayout &DL, Value *PtrValue, unsigned Alignment, Value *OffsetValue = nullptr)
{
    return IRB()->CreateAlignmentAssumption(DL, PtrValue, Alignment, OffsetValue);
}

