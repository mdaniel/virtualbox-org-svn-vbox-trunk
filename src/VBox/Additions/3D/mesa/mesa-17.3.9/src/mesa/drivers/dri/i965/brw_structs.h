/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics to
 develop this 3D driver.

 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:

 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keithw@vmware.com>
  */


#ifndef BRW_STRUCTS_H
#define BRW_STRUCTS_H

struct brw_urb_fence
{
   struct
   {
      unsigned length:8;
      unsigned vs_realloc:1;
      unsigned gs_realloc:1;
      unsigned clp_realloc:1;
      unsigned sf_realloc:1;
      unsigned vfe_realloc:1;
      unsigned cs_realloc:1;
      unsigned pad:2;
      unsigned opcode:16;
   } header;

   struct
   {
      unsigned vs_fence:10;
      unsigned gs_fence:10;
      unsigned clp_fence:10;
      unsigned pad:2;
   } bits0;

   struct
   {
      unsigned sf_fence:10;
      unsigned vf_fence:10;
      unsigned cs_fence:11;
      unsigned pad:1;
   } bits1;
};

struct gen5_sampler_default_color {
   uint8_t ub[4];
   float f[4];
   uint16_t hf[4];
   uint16_t us[4];
   int16_t s[4];
   uint8_t b[4];
};

#endif
