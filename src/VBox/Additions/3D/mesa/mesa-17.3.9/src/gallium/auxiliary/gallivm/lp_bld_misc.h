/**************************************************************************
 *
 * Copyright 2012 VMware, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/


#ifndef LP_BLD_MISC_H
#define LP_BLD_MISC_H


#include "lp_bld.h"
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>


#ifdef __cplusplus
extern "C" {
#endif


struct lp_generated_code;

extern LLVMTargetLibraryInfoRef
gallivm_create_target_library_info(const char *triple);

extern void
gallivm_dispose_target_library_info(LLVMTargetLibraryInfoRef library_info);

extern void
lp_set_target_options(void);


extern int
lp_build_create_jit_compiler_for_module(LLVMExecutionEngineRef *OutJIT,
                                        struct lp_generated_code **OutCode,
                                        LLVMModuleRef M,
                                        LLVMMCJITMemoryManagerRef MM,
                                        unsigned OptLevel,
                                        int useMCJIT,
                                        char **OutError);

extern void
lp_free_generated_code(struct lp_generated_code *code);

extern LLVMMCJITMemoryManagerRef
lp_get_default_memory_manager();

extern void
lp_free_memory_manager(LLVMMCJITMemoryManagerRef memorymgr);

extern LLVMValueRef
lp_get_called_value(LLVMValueRef call);

extern bool
lp_is_function(LLVMValueRef v);

enum lp_float_mode {
   LP_FLOAT_MODE_DEFAULT,
   LP_FLOAT_MODE_NO_SIGNED_ZEROS_FP_MATH,
   LP_FLOAT_MODE_UNSAFE_FP_MATH,
};

extern LLVMBuilderRef
lp_create_builder(LLVMContextRef ctx, enum lp_float_mode float_mode);

#ifdef __cplusplus
}
#endif


#endif /* !LP_BLD_MISC_H */
