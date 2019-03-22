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


#include <stdlib.h>
#include <stdio.h>
#include <float.h>

#include "util/u_memory.h"
#include "util/u_pointer.h"
#include "util/u_string.h"
#include "util/u_format.h"
#include "util/u_format_tests.h"
#include "util/u_format_s3tc.h"

#include "gallivm/lp_bld.h"
#include "gallivm/lp_bld_debug.h"
#include "gallivm/lp_bld_format.h"
#include "gallivm/lp_bld_init.h"

#include "lp_test.h"

#define USE_TEXTURE_CACHE 1

static struct lp_build_format_cache *cache_ptr;

void
write_tsv_header(FILE *fp)
{
   fprintf(fp,
           "result\t"
           "format\n");

   fflush(fp);
}


static void
write_tsv_row(FILE *fp,
              const struct util_format_description *desc,
              boolean success)
{
   fprintf(fp, "%s\t", success ? "pass" : "fail");

   fprintf(fp, "%s\n", desc->name);

   fflush(fp);
}


typedef void
(*fetch_ptr_t)(void *unpacked, const void *packed,
               unsigned i, unsigned j, struct lp_build_format_cache *cache);


static LLVMValueRef
add_fetch_rgba_test(struct gallivm_state *gallivm, unsigned verbose,
                    const struct util_format_description *desc,
                    struct lp_type type)
{
   char name[256];
   LLVMContextRef context = gallivm->context;
   LLVMModuleRef module = gallivm->module;
   LLVMBuilderRef builder = gallivm->builder;
   LLVMTypeRef args[5];
   LLVMValueRef func;
   LLVMValueRef packed_ptr;
   LLVMValueRef offset = LLVMConstNull(LLVMInt32TypeInContext(context));
   LLVMValueRef rgba_ptr;
   LLVMValueRef i;
   LLVMValueRef j;
   LLVMBasicBlockRef block;
   LLVMValueRef rgba;
   LLVMValueRef cache = NULL;

   util_snprintf(name, sizeof name, "fetch_%s_%s", desc->short_name,
                 type.floating ? "float" : "unorm8");

   args[0] = LLVMPointerType(lp_build_vec_type(gallivm, type), 0);
   args[1] = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
   args[3] = args[2] = LLVMInt32TypeInContext(context);
   args[4] = LLVMPointerType(lp_build_format_cache_type(gallivm), 0);

   func = LLVMAddFunction(module, name,
                          LLVMFunctionType(LLVMVoidTypeInContext(context),
                                           args, ARRAY_SIZE(args), 0));
   LLVMSetFunctionCallConv(func, LLVMCCallConv);
   rgba_ptr = LLVMGetParam(func, 0);
   packed_ptr = LLVMGetParam(func, 1);
   i = LLVMGetParam(func, 2);
   j = LLVMGetParam(func, 3);

   if (cache_ptr) {
      cache = LLVMGetParam(func, 4);
   }

   block = LLVMAppendBasicBlockInContext(context, func, "entry");
   LLVMPositionBuilderAtEnd(builder, block);

   rgba = lp_build_fetch_rgba_aos(gallivm, desc, type, TRUE,
                                  packed_ptr, offset, i, j, cache);

   LLVMBuildStore(builder, rgba, rgba_ptr);

   LLVMBuildRetVoid(builder);

   gallivm_verify_function(gallivm, func);

   return func;
}


PIPE_ALIGN_STACK
static boolean
test_format_float(unsigned verbose, FILE *fp,
                  const struct util_format_description *desc)
{
   LLVMContextRef context;
   struct gallivm_state *gallivm;
   LLVMValueRef fetch = NULL;
   fetch_ptr_t fetch_ptr;
   PIPE_ALIGN_VAR(16) uint8_t packed[UTIL_FORMAT_MAX_PACKED_BYTES];
   PIPE_ALIGN_VAR(16) float unpacked[4];
   boolean first = TRUE;
   boolean success = TRUE;
   unsigned i, j, k, l;

   context = LLVMContextCreate();
   gallivm = gallivm_create("test_module_float", context);

   fetch = add_fetch_rgba_test(gallivm, verbose, desc, lp_float32_vec4_type());

   gallivm_compile_module(gallivm);

   fetch_ptr = (fetch_ptr_t) gallivm_jit_function(gallivm, fetch);

   gallivm_free_ir(gallivm);

   for (l = 0; l < util_format_nr_test_cases; ++l) {
      const struct util_format_test_case *test = &util_format_test_cases[l];

      if (test->format == desc->format) {

         if (first) {
            printf("Testing %s (float) ...\n",
                   desc->name);
            fflush(stdout);
            first = FALSE;
         }

         /* To ensure it's 16-byte aligned */
         memcpy(packed, test->packed, sizeof packed);

         for (i = 0; i < desc->block.height; ++i) {
            for (j = 0; j < desc->block.width; ++j) {
               boolean match = TRUE;

               memset(unpacked, 0, sizeof unpacked);

               fetch_ptr(unpacked, packed, j, i, cache_ptr);

               for(k = 0; k < 4; ++k) {
                  if (util_double_inf_sign(test->unpacked[i][j][k]) != util_inf_sign(unpacked[k])) {
                     match = FALSE;
                  }

                  if (util_is_double_nan(test->unpacked[i][j][k]) != util_is_nan(unpacked[k])) {
                     match = FALSE;
                  }

                  if (!util_is_double_inf_or_nan(test->unpacked[i][j][k]) &&
                      fabs((float)test->unpacked[i][j][k] - unpacked[k]) > FLT_EPSILON) {
                     match = FALSE;
                  }
               }

               /* Ignore errors in S3TC for now */
               if (desc->layout == UTIL_FORMAT_LAYOUT_S3TC) {
                  match = TRUE;
               }

               if (!match) {
                  printf("FAILED\n");
                  printf("  Packed: %02x %02x %02x %02x\n",
                         test->packed[0], test->packed[1], test->packed[2], test->packed[3]);
                  printf("  Unpacked (%u,%u): %.9g %.9g %.9g %.9g obtained\n",
                         j, i,
                         unpacked[0], unpacked[1], unpacked[2], unpacked[3]);
                  printf("                  %.9g %.9g %.9g %.9g expected\n",
                         test->unpacked[i][j][0],
                         test->unpacked[i][j][1],
                         test->unpacked[i][j][2],
                         test->unpacked[i][j][3]);
                  fflush(stdout);
                  success = FALSE;
               }
            }
         }
      }
   }

   gallivm_destroy(gallivm);
   LLVMContextDispose(context);

   if(fp)
      write_tsv_row(fp, desc, success);

   return success;
}


PIPE_ALIGN_STACK
static boolean
test_format_unorm8(unsigned verbose, FILE *fp,
                   const struct util_format_description *desc)
{
   LLVMContextRef context;
   struct gallivm_state *gallivm;
   LLVMValueRef fetch = NULL;
   fetch_ptr_t fetch_ptr;
   PIPE_ALIGN_VAR(16) uint8_t packed[UTIL_FORMAT_MAX_PACKED_BYTES];
   uint8_t unpacked[4];
   boolean first = TRUE;
   boolean success = TRUE;
   unsigned i, j, k, l;

   context = LLVMContextCreate();
   gallivm = gallivm_create("test_module_unorm8", context);

   fetch = add_fetch_rgba_test(gallivm, verbose, desc, lp_unorm8_vec4_type());

   gallivm_compile_module(gallivm);

   fetch_ptr = (fetch_ptr_t) gallivm_jit_function(gallivm, fetch);

   gallivm_free_ir(gallivm);

   for (l = 0; l < util_format_nr_test_cases; ++l) {
      const struct util_format_test_case *test = &util_format_test_cases[l];

      if (test->format == desc->format) {

         if (first) {
            printf("Testing %s (unorm8) ...\n",
                   desc->name);
            first = FALSE;
         }

         /* To ensure it's 16-byte aligned */
         /* Could skip this and use unaligned lp_build_fetch_rgba_aos */
         memcpy(packed, test->packed, sizeof packed);

         for (i = 0; i < desc->block.height; ++i) {
            for (j = 0; j < desc->block.width; ++j) {
               boolean match;

               memset(unpacked, 0, sizeof unpacked);

               fetch_ptr(unpacked, packed, j, i, cache_ptr);

               match = TRUE;
               for(k = 0; k < 4; ++k) {
                  int error = float_to_ubyte(test->unpacked[i][j][k]) - unpacked[k];

                  if (util_is_double_nan(test->unpacked[i][j][k]))
                     continue;

                  if (error < 0)
                     error = -error;

                  if (error > 1)
                     match = FALSE;
               }

               /* Ignore errors in S3TC as we only implement a poor man approach */
               if (desc->layout == UTIL_FORMAT_LAYOUT_S3TC) {
                  match = TRUE;
               }

               if (!match) {
                  printf("FAILED\n");
                  printf("  Packed: %02x %02x %02x %02x\n",
                         test->packed[0], test->packed[1], test->packed[2], test->packed[3]);
                  printf("  Unpacked (%u,%u): %02x %02x %02x %02x obtained\n",
                         j, i,
                         unpacked[0], unpacked[1], unpacked[2], unpacked[3]);
                  printf("                  %02x %02x %02x %02x expected\n",
                         float_to_ubyte(test->unpacked[i][j][0]),
                         float_to_ubyte(test->unpacked[i][j][1]),
                         float_to_ubyte(test->unpacked[i][j][2]),
                         float_to_ubyte(test->unpacked[i][j][3]));

                  success = FALSE;
               }
            }
         }
      }
   }

   gallivm_destroy(gallivm);
   LLVMContextDispose(context);

   if(fp)
      write_tsv_row(fp, desc, success);

   return success;
}




static boolean
test_one(unsigned verbose, FILE *fp,
         const struct util_format_description *format_desc)
{
   boolean success = TRUE;

   if (!test_format_float(verbose, fp, format_desc)) {
     success = FALSE;
   }

   if (!test_format_unorm8(verbose, fp, format_desc)) {
     success = FALSE;
   }

   return success;
}


boolean
test_all(unsigned verbose, FILE *fp)
{
   enum pipe_format format;
   boolean success = TRUE;

#if USE_TEXTURE_CACHE
   cache_ptr = align_malloc(sizeof(struct lp_build_format_cache), 16);
#endif

   for (format = 1; format < PIPE_FORMAT_COUNT; ++format) {
      const struct util_format_description *format_desc;

      format_desc = util_format_description(format);
      if (!format_desc) {
         continue;
      }


      /*
       * TODO: test more
       */

      if (format_desc->colorspace == UTIL_FORMAT_COLORSPACE_ZS) {
         continue;
      }

      if (util_format_is_pure_integer(format))
	 continue;

      /* only have util fetch func for etc1 */
      if (format_desc->layout == UTIL_FORMAT_LAYOUT_ETC &&
          format != PIPE_FORMAT_ETC1_RGB8) {
         continue;
      }

      /* missing fetch funcs */
      if (format_desc->layout == UTIL_FORMAT_LAYOUT_BPTC ||
          format_desc->layout == UTIL_FORMAT_LAYOUT_ASTC) {
         continue;
      }

      if (!test_one(verbose, fp, format_desc)) {
           success = FALSE;
      }
   }
#if USE_TEXTURE_CACHE
   align_free(cache_ptr);
#endif

   return success;
}


boolean
test_some(unsigned verbose, FILE *fp,
          unsigned long n)
{
   return test_all(verbose, fp);
}


boolean
test_single(unsigned verbose, FILE *fp)
{
   printf("no test_single()");
   return TRUE;
}
