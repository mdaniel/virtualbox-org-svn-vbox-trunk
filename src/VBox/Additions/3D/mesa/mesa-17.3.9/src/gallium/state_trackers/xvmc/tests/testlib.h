/**************************************************************************
 *
 * Copyright 2009 Younes Manton.
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

#ifndef testlib_h
#define testlib_h

/*
#define TEST(pred, doc)	test(pred, #pred, doc, __FILE__, __LINE__)

void test(int pred, const char *pred_string, const char *doc_string, const char *file, unsigned int line);
*/

#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/extensions/XvMClib.h>

/*
 * display: IN			A valid X display
 * width, height: IN		Surface size that the port must display
 * chroma_format: IN		Chroma format that the port must display
 * mc_types, num_mc_types: IN	List of MC types that the port must support, first port that matches the first mc_type will be returned
 * port_id: OUT			Your port's ID
 * surface_type_id: OUT		Your port's surface ID
 * is_overlay: OUT		If 1, port uses overlay surfaces, you need to set a colorkey
 * intra_unsigned: OUT		If 1, port uses unsigned values for intra-coded blocks
 */
int GetPort
(
	Display *display,
	unsigned int width,
	unsigned int height,
	unsigned int chroma_format,
	const unsigned int *mc_types,
	unsigned int num_mc_types,
	XvPortID *port_id,
	int *surface_type_id,
	unsigned int *is_overlay,
	unsigned int *intra_unsigned
);

unsigned int align(unsigned int value, unsigned int alignment);

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y);

#endif
