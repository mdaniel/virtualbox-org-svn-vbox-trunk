/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.
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


#include "pipe/p_config.h"
#include "pipe/p_compiler.h"
#include "util/u_cpu_detect.h"
#include "util/u_debug.h"
#include "util/u_memory.h"
#include "util/simple_list.h"
#include "os/os_time.h"
#include "lp_bld.h"
#include "lp_bld_debug.h"
#include "lp_bld_misc.h"
#include "lp_bld_init.h"

#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>
#include <llvm-c/BitWriter.h>


/* Only MCJIT is available as of LLVM SVN r216982 */
#if HAVE_LLVM >= 0x0306
#  define USE_MCJIT 1
#elif defined(PIPE_ARCH_PPC_64) || defined(PIPE_ARCH_S390) || defined(PIPE_ARCH_ARM) || defined(PIPE_ARCH_AARCH64)
#  define USE_MCJIT 1
#endif

#if defined(USE_MCJIT)
static const bool use_mcjit = USE_MCJIT;
#else
static bool use_mcjit = FALSE;
#endif


#ifdef DEBUG
unsigned gallivm_debug = 0;

static const struct debug_named_value lp_bld_debug_flags[] = {
   { "tgsi",   GALLIVM_DEBUG_TGSI, NULL },
   { "ir",     GALLIVM_DEBUG_IR, NULL },
   { "asm",    GALLIVM_DEBUG_ASM, NULL },
   { "nopt",   GALLIVM_DEBUG_NO_OPT, NULL },
   { "perf",   GALLIVM_DEBUG_PERF, NULL },
   { "no_brilinear", GALLIVM_DEBUG_NO_BRILINEAR, NULL },
   { "no_rho_approx", GALLIVM_DEBUG_NO_RHO_APPROX, NULL },
   { "no_quad_lod", GALLIVM_DEBUG_NO_QUAD_LOD, NULL },
   { "gc",     GALLIVM_DEBUG_GC, NULL },
   { "dumpbc", GALLIVM_DEBUG_DUMP_BC, NULL },
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(gallivm_debug, "GALLIVM_DEBUG", lp_bld_debug_flags, 0)
#endif


static boolean gallivm_initialized = FALSE;

unsigned lp_native_vector_width;


/*
 * Optimization values are:
 * - 0: None (-O0)
 * - 1: Less (-O1)
 * - 2: Default (-O2, -Os)
 * - 3: Aggressive (-O3)
 *
 * See also CodeGenOpt::Level in llvm/Target/TargetMachine.h
 */
enum LLVM_CodeGenOpt_Level {
   None,        // -O0
   Less,        // -O1
   Default,     // -O2, -Os
   Aggressive   // -O3
};


/**
 * Create the LLVM (optimization) pass manager and install
 * relevant optimization passes.
 * \return  TRUE for success, FALSE for failure
 */
static boolean
create_pass_manager(struct gallivm_state *gallivm)
{
   assert(!gallivm->passmgr);
   assert(gallivm->target);

   gallivm->passmgr = LLVMCreateFunctionPassManagerForModule(gallivm->module);
   if (!gallivm->passmgr)
      return FALSE;
   /*
    * TODO: some per module pass manager with IPO passes might be helpful -
    * the generated texture functions may benefit from inlining if they are
    * simple, or constant propagation into them, etc.
    */

#if HAVE_LLVM < 0x0309
   // Old versions of LLVM get the DataLayout from the pass manager.
   LLVMAddTargetData(gallivm->target, gallivm->passmgr);
#endif

   {
      char *td_str;
      // New ones from the Module.
      td_str = LLVMCopyStringRepOfTargetData(gallivm->target);
      LLVMSetDataLayout(gallivm->module, td_str);
      free(td_str);
   }

   if ((gallivm_debug & GALLIVM_DEBUG_NO_OPT) == 0) {
      /* These are the passes currently listed in llvm-c/Transforms/Scalar.h,
       * but there are more on SVN.
       * TODO: Add more passes.
       */
      LLVMAddScalarReplAggregatesPass(gallivm->passmgr);
      LLVMAddLICMPass(gallivm->passmgr);
      LLVMAddCFGSimplificationPass(gallivm->passmgr);
      LLVMAddReassociatePass(gallivm->passmgr);
      LLVMAddPromoteMemoryToRegisterPass(gallivm->passmgr);
      LLVMAddConstantPropagationPass(gallivm->passmgr);
      LLVMAddInstructionCombiningPass(gallivm->passmgr);
      LLVMAddGVNPass(gallivm->passmgr);
   }
   else {
      /* We need at least this pass to prevent the backends to fail in
       * unexpected ways.
       */
      LLVMAddPromoteMemoryToRegisterPass(gallivm->passmgr);
   }

   return TRUE;
}


/**
 * Free gallivm object's LLVM allocations, but not any generated code
 * nor the gallivm object itself.
 */
void
gallivm_free_ir(struct gallivm_state *gallivm)
{
   if (gallivm->passmgr) {
      LLVMDisposePassManager(gallivm->passmgr);
   }

   if (gallivm->engine) {
      /* This will already destroy any associated module */
      LLVMDisposeExecutionEngine(gallivm->engine);
   } else if (gallivm->module) {
      LLVMDisposeModule(gallivm->module);
   }

   FREE(gallivm->module_name);

   if (!use_mcjit) {
      /* Don't free the TargetData, it's owned by the exec engine */
   } else {
      if (gallivm->target) {
         LLVMDisposeTargetData(gallivm->target);
      }
   }

   if (gallivm->builder)
      LLVMDisposeBuilder(gallivm->builder);

   /* The LLVMContext should be owned by the parent of gallivm. */

   gallivm->engine = NULL;
   gallivm->target = NULL;
   gallivm->module = NULL;
   gallivm->module_name = NULL;
   gallivm->passmgr = NULL;
   gallivm->context = NULL;
   gallivm->builder = NULL;
}


/**
 * Free LLVM-generated code.  Should be done AFTER gallivm_free_ir().
 */
static void
gallivm_free_code(struct gallivm_state *gallivm)
{
   assert(!gallivm->module);
   assert(!gallivm->engine);
   lp_free_generated_code(gallivm->code);
   gallivm->code = NULL;
   lp_free_memory_manager(gallivm->memorymgr);
   gallivm->memorymgr = NULL;
}


static boolean
init_gallivm_engine(struct gallivm_state *gallivm)
{
   if (1) {
      enum LLVM_CodeGenOpt_Level optlevel;
      char *error = NULL;
      int ret;

      if (gallivm_debug & GALLIVM_DEBUG_NO_OPT) {
         optlevel = None;
      }
      else {
         optlevel = Default;
      }

      ret = lp_build_create_jit_compiler_for_module(&gallivm->engine,
                                                    &gallivm->code,
                                                    gallivm->module,
                                                    gallivm->memorymgr,
                                                    (unsigned) optlevel,
                                                    use_mcjit,
                                                    &error);
      if (ret) {
         _debug_printf("%s\n", error);
         LLVMDisposeMessage(error);
         goto fail;
      }
   }

   if (!use_mcjit) {
      gallivm->target = LLVMGetExecutionEngineTargetData(gallivm->engine);
      if (!gallivm->target)
         goto fail;
   } else {
      if (0) {
          /*
           * Dump the data layout strings.
           */

          LLVMTargetDataRef target = LLVMGetExecutionEngineTargetData(gallivm->engine);
          char *data_layout;
          char *engine_data_layout;

          data_layout = LLVMCopyStringRepOfTargetData(gallivm->target);
          engine_data_layout = LLVMCopyStringRepOfTargetData(target);

          if (1) {
             debug_printf("module target data = %s\n", data_layout);
             debug_printf("engine target data = %s\n", engine_data_layout);
          }

          free(data_layout);
          free(engine_data_layout);
      }
   }

   return TRUE;

fail:
   return FALSE;
}


/**
 * Allocate gallivm LLVM objects.
 * \return  TRUE for success, FALSE for failure
 */
static boolean
init_gallivm_state(struct gallivm_state *gallivm, const char *name,
                   LLVMContextRef context)
{
   assert(!gallivm->context);
   assert(!gallivm->module);

   if (!lp_build_init())
      return FALSE;

   gallivm->context = context;

   if (!gallivm->context)
      goto fail;

   gallivm->module_name = NULL;
   if (name) {
      size_t size = strlen(name) + 1;
      gallivm->module_name = MALLOC(size);
      if (gallivm->module_name) {
         memcpy(gallivm->module_name, name, size);
      }
   }

   gallivm->module = LLVMModuleCreateWithNameInContext(name,
                                                       gallivm->context);
   if (!gallivm->module)
      goto fail;

   gallivm->builder = LLVMCreateBuilderInContext(gallivm->context);
   if (!gallivm->builder)
      goto fail;

   gallivm->memorymgr = lp_get_default_memory_manager();
   if (!gallivm->memorymgr)
      goto fail;

   /* FIXME: MC-JIT only allows compiling one module at a time, and it must be
    * complete when MC-JIT is created. So defer the MC-JIT engine creation for
    * now.
    */
   if (!use_mcjit) {
      if (!init_gallivm_engine(gallivm)) {
         goto fail;
      }
   } else {
      /*
       * MC-JIT engine compiles the module immediately on creation, so we can't
       * obtain the target data from it.  Instead we create a target data layout
       * from a string.
       *
       * The produced layout strings are not precisely the same, but should make
       * no difference for the kind of optimization passes we run.
       *
       * For reference this is the layout string on x64:
       *
       *   e-p:64:64:64-S128-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f16:16:16-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-f128:128:128-n8:16:32:64
       *
       * See also:
       * - http://llvm.org/docs/LangRef.html#datalayout
       */

      {
         const unsigned pointer_size = 8 * sizeof(void *);
         char layout[512];
         util_snprintf(layout, sizeof layout, "%c-p:%u:%u:%u-i64:64:64-a0:0:%u-s0:%u:%u",
#ifdef PIPE_ARCH_LITTLE_ENDIAN
                       'e', // little endian
#else
                       'E', // big endian
#endif
                       pointer_size, pointer_size, pointer_size, // pointer size, abi alignment, preferred alignment
                       pointer_size, // aggregate preferred alignment
                       pointer_size, pointer_size); // stack objects abi alignment, preferred alignment

         gallivm->target = LLVMCreateTargetData(layout);
         if (!gallivm->target) {
            return FALSE;
         }
      }
   }

   if (!create_pass_manager(gallivm))
      goto fail;

   return TRUE;

fail:
   gallivm_free_ir(gallivm);
   gallivm_free_code(gallivm);
   return FALSE;
}


boolean
lp_build_init(void)
{
   if (gallivm_initialized)
      return TRUE;


   /* LLVMLinkIn* are no-ops at runtime.  They just ensure the respective
    * component is linked at buildtime, which is sufficient for its static
    * constructors to be called at load time.
    */
#if defined(USE_MCJIT)
#  if USE_MCJIT
      LLVMLinkInMCJIT();
#  else
      LLVMLinkInJIT();
#  endif
#else
   use_mcjit = debug_get_bool_option("GALLIVM_MCJIT", FALSE);
   LLVMLinkInJIT();
   LLVMLinkInMCJIT();
#endif

#ifdef DEBUG
   gallivm_debug = debug_get_option_gallivm_debug();
#endif

   lp_set_target_options();

   util_cpu_detect();

   /* For simulating less capable machines */
#ifdef DEBUG
   if (debug_get_bool_option("LP_FORCE_SSE2", FALSE)) {
      assert(util_cpu_caps.has_sse2);
      util_cpu_caps.has_sse3 = 0;
      util_cpu_caps.has_ssse3 = 0;
      util_cpu_caps.has_sse4_1 = 0;
      util_cpu_caps.has_sse4_2 = 0;
      util_cpu_caps.has_avx = 0;
      util_cpu_caps.has_avx2 = 0;
      util_cpu_caps.has_f16c = 0;
      util_cpu_caps.has_fma = 0;
   }
#endif

   /* AMD Bulldozer AVX's throughput is the same as SSE2; and because using
    * 8-wide vector needs more floating ops than 4-wide (due to padding), it is
    * actually more efficient to use 4-wide vectors on this processor.
    *
    * See also:
    * - http://www.anandtech.com/show/4955/the-bulldozer-review-amd-fx8150-tested/2
    */
   if (util_cpu_caps.has_avx &&
       util_cpu_caps.has_intel) {
      lp_native_vector_width = 256;
   } else {
      /* Leave it at 128, even when no SIMD extensions are available.
       * Really needs to be a multiple of 128 so can fit 4 floats.
       */
      lp_native_vector_width = 128;
   }
 
   lp_native_vector_width = debug_get_num_option("LP_NATIVE_VECTOR_WIDTH",
                                                 lp_native_vector_width);

   if (lp_native_vector_width <= 128) {
      /* Hide AVX support, as often LLVM AVX intrinsics are only guarded by
       * "util_cpu_caps.has_avx" predicate, and lack the
       * "lp_native_vector_width > 128" predicate. And also to ensure a more
       * consistent behavior, allowing one to test SSE2 on AVX machines.
       * XXX: should not play games with util_cpu_caps directly as it might
       * get used for other things outside llvm too.
       */
      util_cpu_caps.has_avx = 0;
      util_cpu_caps.has_avx2 = 0;
      util_cpu_caps.has_f16c = 0;
      util_cpu_caps.has_fma = 0;
   }
   if (HAVE_LLVM < 0x0304 || !use_mcjit) {
      /* AVX2 support has only been tested with LLVM 3.4, and it requires
       * MCJIT. */
      util_cpu_caps.has_avx2 = 0;
   }

#ifdef PIPE_ARCH_PPC_64
   /* Set the NJ bit in VSCR to 0 so denormalized values are handled as
    * specified by IEEE standard (PowerISA 2.06 - Section 6.3). This guarantees
    * that some rounding and half-float to float handling does not round
    * incorrectly to 0.
    * XXX: should eventually follow same logic on all platforms.
    * Right now denorms get explicitly disabled (but elsewhere) for x86,
    * whereas ppc64 explicitly enables them...
    */
   if (util_cpu_caps.has_altivec) {
      unsigned short mask[] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
                                0xFFFF, 0xFFFF, 0xFFFE, 0xFFFF };
      __asm (
        "mfvscr %%v1\n"
        "vand   %0,%%v1,%0\n"
        "mtvscr %0"
        :
        : "r" (*mask)
      );
   }
#endif

   gallivm_initialized = TRUE;

   return TRUE;
}



/**
 * Create a new gallivm_state object.
 */
struct gallivm_state *
gallivm_create(const char *name, LLVMContextRef context)
{
   struct gallivm_state *gallivm;

   gallivm = CALLOC_STRUCT(gallivm_state);
   if (gallivm) {
      if (!init_gallivm_state(gallivm, name, context)) {
         FREE(gallivm);
         gallivm = NULL;
      }
   }

   return gallivm;
}


/**
 * Destroy a gallivm_state object.
 */
void
gallivm_destroy(struct gallivm_state *gallivm)
{
   gallivm_free_ir(gallivm);
   gallivm_free_code(gallivm);
   FREE(gallivm);
}


/**
 * Validate a function.
 * Verification is only done with debug builds.
 */
void
gallivm_verify_function(struct gallivm_state *gallivm,
                        LLVMValueRef func)
{
   /* Verify the LLVM IR.  If invalid, dump and abort */
#ifdef DEBUG
   if (LLVMVerifyFunction(func, LLVMPrintMessageAction)) {
      lp_debug_dump_value(func);
      assert(0);
      return;
   }
#endif

   if (gallivm_debug & GALLIVM_DEBUG_IR) {
      /* Print the LLVM IR to stderr */
      lp_debug_dump_value(func);
      debug_printf("\n");
   }
}


/**
 * Compile a module.
 * This does IR optimization on all functions in the module.
 */
void
gallivm_compile_module(struct gallivm_state *gallivm)
{
   LLVMValueRef func;
   int64_t time_begin = 0;

   assert(!gallivm->compiled);

   if (gallivm->builder) {
      LLVMDisposeBuilder(gallivm->builder);
      gallivm->builder = NULL;
   }

   if (gallivm_debug & GALLIVM_DEBUG_PERF)
      time_begin = os_time_get();

   /* Run optimization passes */
   LLVMInitializeFunctionPassManager(gallivm->passmgr);
   func = LLVMGetFirstFunction(gallivm->module);
   while (func) {
      if (0) {
         debug_printf("optimizing func %s...\n", LLVMGetValueName(func));
      }

   /* Disable frame pointer omission on debug/profile builds */
   /* XXX: And workaround http://llvm.org/PR21435 */
#if HAVE_LLVM >= 0x0307 && \
    (defined(DEBUG) || defined(PROFILE) || \
     defined(PIPE_ARCH_X86) || defined(PIPE_ARCH_X86_64))
      LLVMAddTargetDependentFunctionAttr(func, "no-frame-pointer-elim", "true");
      LLVMAddTargetDependentFunctionAttr(func, "no-frame-pointer-elim-non-leaf", "true");
#endif

      LLVMRunFunctionPassManager(gallivm->passmgr, func);
      func = LLVMGetNextFunction(func);
   }
   LLVMFinalizeFunctionPassManager(gallivm->passmgr);

   if (gallivm_debug & GALLIVM_DEBUG_PERF) {
      int64_t time_end = os_time_get();
      int time_msec = (int)(time_end - time_begin) / 1000;
      assert(gallivm->module_name);
      debug_printf("optimizing module %s took %d msec\n",
                   gallivm->module_name, time_msec);
   }

   /* Dump byte code to a file */
   if (gallivm_debug & GALLIVM_DEBUG_DUMP_BC) {
      char filename[256];
      assert(gallivm->module_name);
      util_snprintf(filename, sizeof(filename), "ir_%s.bc", gallivm->module_name);
      LLVMWriteBitcodeToFile(gallivm->module, filename);
      debug_printf("%s written\n", filename);
      debug_printf("Invoke as \"llc %s%s -o - %s\"\n",
                   (HAVE_LLVM >= 0x0305) ? "[-mcpu=<-mcpu option>] " : "",
                   "[-mattr=<-mattr option(s)>]",
                   filename);
   }

   if (use_mcjit) {
      /* Setting the module's DataLayout to an empty string will cause the
       * ExecutionEngine to copy to the DataLayout string from its target
       * machine to the module.  As of LLVM 3.8 the module and the execution
       * engine are required to have the same DataLayout.
       *
       * We must make sure we do this after running the optimization passes,
       * because those passes need a correct datalayout string.  For example,
       * if those optimization passes see an empty datalayout, they will assume
       * this is a little endian target and will do optimizations that break big
       * endian machines.
       *
       * TODO: This is just a temporary work-around.  The correct solution is
       * for gallivm_init_state() to create a TargetMachine and pull the
       * DataLayout from there.  Currently, the TargetMachine used by llvmpipe
       * is being implicitly created by the EngineBuilder in
       * lp_build_create_jit_compiler_for_module()
       */
      LLVMSetDataLayout(gallivm->module, "");
      assert(!gallivm->engine);
      if (!init_gallivm_engine(gallivm)) {
         assert(0);
      }
   }
   assert(gallivm->engine);

   ++gallivm->compiled;

   if (gallivm_debug & GALLIVM_DEBUG_ASM) {
      LLVMValueRef llvm_func = LLVMGetFirstFunction(gallivm->module);

      while (llvm_func) {
         /*
          * Need to filter out functions which don't have an implementation,
          * such as the intrinsics. May not be sufficient in case of IPO?
          * LLVMGetPointerToGlobal() will abort otherwise.
          */
         if (!LLVMIsDeclaration(llvm_func)) {
            void *func_code = LLVMGetPointerToGlobal(gallivm->engine, llvm_func);
            lp_disassemble(llvm_func, func_code);
         }
         llvm_func = LLVMGetNextFunction(llvm_func);
      }
   }

#if defined(PROFILE)
   {
      LLVMValueRef llvm_func = LLVMGetFirstFunction(gallivm->module);

      while (llvm_func) {
         if (!LLVMIsDeclaration(llvm_func)) {
            void *func_code = LLVMGetPointerToGlobal(gallivm->engine, llvm_func);
            lp_profile(llvm_func, func_code);
         }
         llvm_func = LLVMGetNextFunction(llvm_func);
      }
   }
#endif
}



func_pointer
gallivm_jit_function(struct gallivm_state *gallivm,
                     LLVMValueRef func)
{
   void *code;
   func_pointer jit_func;
   int64_t time_begin = 0;

   assert(gallivm->compiled);
   assert(gallivm->engine);

   if (gallivm_debug & GALLIVM_DEBUG_PERF)
      time_begin = os_time_get();

   code = LLVMGetPointerToGlobal(gallivm->engine, func);
   assert(code);
   jit_func = pointer_to_func(code);

   if (gallivm_debug & GALLIVM_DEBUG_PERF) {
      int64_t time_end = os_time_get();
      int time_msec = (int)(time_end - time_begin) / 1000;
      debug_printf("   jitting func %s took %d msec\n",
                   LLVMGetValueName(func), time_msec);
   }

   return jit_func;
}
