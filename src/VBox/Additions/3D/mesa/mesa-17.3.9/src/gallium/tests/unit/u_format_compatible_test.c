/**************************************************************************
 *
 * Copyright 2009-2010 VMware, Inc.
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

#include "util/u_format.h"


static boolean
test_all(void)
{
   enum pipe_format src_format;
   enum pipe_format dst_format;

   for (src_format = 1; src_format < PIPE_FORMAT_COUNT; ++src_format) {
      const struct util_format_description *src_format_desc;
      src_format_desc = util_format_description(src_format);
      if (!src_format_desc) {
         continue;
      }

      for (dst_format = 1; dst_format < PIPE_FORMAT_COUNT; ++dst_format) {
	 const struct util_format_description *dst_format_desc;
	 dst_format_desc = util_format_description(dst_format);
	 if (!dst_format_desc) {
	    continue;
	 }

         if (dst_format == src_format) {
            continue;
         }

	 if (util_is_format_compatible(src_format_desc, dst_format_desc)) {
	    printf("%s -> %s\n", src_format_desc->short_name, dst_format_desc->short_name);
	 }
      }
   }

   return TRUE;
}


int main(int argc, char **argv)
{
   boolean success;

   success = test_all();

   return success ? 0 : 1;
}
