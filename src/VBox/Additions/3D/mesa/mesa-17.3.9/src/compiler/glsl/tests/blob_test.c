/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* A collection of unit tests for blob.c */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "util/ralloc.h"
#include "blob.h"

#define bytes_test_str     "bytes_test"
#define reserve_test_str   "reserve_test"

/* This placeholder must be the same length as the next overwrite_test_str */
#define placeholder_str    "XXXXXXXXXXXXXX"
#define overwrite_test_str "overwrite_test"
#define uint32_test        0x12345678
#define uint32_placeholder 0xDEADBEEF
#define uint32_overwrite   0xA1B2C3D4
#define uint64_test        0x1234567890ABCDEF
#define string_test_str    "string_test"

bool error = false;

static void
expect_equal(uint64_t expected, uint64_t actual, const char *test)
{
   if (actual != expected) {
      fprintf(stderr,
              "Error: Test '%s' failed: "
              "Expected=%" PRIu64 ", "
              "Actual=%" PRIu64 "\n",
               test, expected, actual);
      error = true;
   }
}

static void
expect_unequal(uint64_t expected, uint64_t actual, const char *test)
{
   if (actual == expected) {
      fprintf(stderr,
              "Error: Test '%s' failed: Result=%" PRIu64 ", "
              "but expected something different.\n",
               test, actual);
      error = true;
   }
}

static void
expect_equal_str(const char *expected, const char *actual, const char *test)
{
   if (strcmp(expected, actual)) {
      fprintf (stderr, "Error: Test '%s' failed:\n\t"
               "Expected=\"%s\", Actual=\"%s\"\n",
               test, expected, actual);
      error = true;
   }
}

static void
expect_equal_bytes(uint8_t *expected, const uint8_t *actual,
                   size_t num_bytes, const char *test)
{
   size_t i;

   if (memcmp(expected, actual, num_bytes)) {
      fprintf (stderr, "Error: Test '%s' failed:\n\t", test);

      fprintf (stderr, "Expected=[");
      for (i = 0; i < num_bytes; i++) {
         if (i != 0)
            fprintf(stderr, ", ");
         fprintf(stderr, "0x%02x", expected[i]);
      }
      fprintf (stderr, "]");

      fprintf (stderr, "Actual=[");
      for (i = 0; i < num_bytes; i++) {
         if (i != 0)
            fprintf(stderr, ", ");
         fprintf(stderr, "0x%02x", actual[i]);
      }
      fprintf (stderr, "]\n");

      error = true;
   }
}

/* Test at least one call of each blob_write_foo and blob_read_foo function,
 * verifying that we read out everything we wrote, that every bytes is
 * consumed, and that the overrun bit is not set.
 */
static void
test_write_and_read_functions (void)
{
   struct blob blob;
   struct blob_reader reader;
   ssize_t reserved;
   size_t str_offset, uint_offset;
   uint8_t reserve_buf[sizeof(reserve_test_str)];

   blob_init(&blob);

   /*** Test blob by writing one of every possible kind of value. */

   blob_write_bytes(&blob, bytes_test_str, sizeof(bytes_test_str));

   reserved = blob_reserve_bytes(&blob, sizeof(reserve_test_str));
   blob_overwrite_bytes(&blob, reserved, reserve_test_str, sizeof(reserve_test_str));

   /* Write a placeholder, (to be replaced later via overwrite_bytes) */
   str_offset = blob.size;
   blob_write_bytes(&blob, placeholder_str, sizeof(placeholder_str));

   blob_write_uint32(&blob, uint32_test);

   /* Write a placeholder, (to be replaced later via overwrite_uint32) */
   uint_offset = blob.size;
   blob_write_uint32(&blob, uint32_placeholder);

   blob_write_uint64(&blob, uint64_test);

   blob_write_intptr(&blob, (intptr_t) &blob);

   blob_write_string(&blob, string_test_str);

   /* Finally, overwrite our placeholders. */
   blob_overwrite_bytes(&blob, str_offset, overwrite_test_str,
                        sizeof(overwrite_test_str));
   blob_overwrite_uint32(&blob, uint_offset, uint32_overwrite);

   /*** Now read each value and verify. */
   blob_reader_init(&reader, blob.data, blob.size);

   expect_equal_str(bytes_test_str,
                    blob_read_bytes(&reader, sizeof(bytes_test_str)),
                    "blob_write/read_bytes");

   blob_copy_bytes(&reader, reserve_buf, sizeof(reserve_buf));
   expect_equal_str(reserve_test_str, (char *) reserve_buf,
                    "blob_reserve_bytes/blob_copy_bytes");

   expect_equal_str(overwrite_test_str,
                    blob_read_bytes(&reader, sizeof(overwrite_test_str)),
                    "blob_overwrite_bytes");

   expect_equal(uint32_test, blob_read_uint32(&reader),
                "blob_write/read_uint32");
   expect_equal(uint32_overwrite, blob_read_uint32(&reader),
                "blob_overwrite_uint32");
   expect_equal(uint64_test, blob_read_uint64(&reader),
                "blob_write/read_uint64");
   expect_equal((intptr_t) &blob, blob_read_intptr(&reader),
                "blob_write/read_intptr");
   expect_equal_str(string_test_str, blob_read_string(&reader),
                    "blob_write/read_string");

   expect_equal(reader.end - reader.data, reader.current - reader.data,
                "read_consumes_all_bytes");
   expect_equal(false, reader.overrun, "read_does_not_overrun");

   blob_finish(&blob);
}

/* Test that data values are written and read with proper alignment. */
static void
test_alignment(void)
{
   struct blob blob;
   struct blob_reader reader;
   uint8_t bytes[] = "ABCDEFGHIJKLMNOP";
   size_t delta, last, num_bytes;

   blob_init(&blob);

   /* First, write an intptr value to the blob and capture that size. This is
    * the expected offset between any pair of intptr values (if written with
    * alignment).
    */
   blob_write_intptr(&blob, (intptr_t) &blob);

   delta = blob.size;
   last = blob.size;

   /* Then loop doing the following:
    *
    *   1. Write an unaligned number of bytes
    *   2. Verify that write results in an unaligned size
    *   3. Write an intptr_t value
    *   2. Verify that that write results in an aligned size
    */
   for (num_bytes = 1; num_bytes < sizeof(intptr_t); num_bytes++) {
      blob_write_bytes(&blob, bytes, num_bytes);

      expect_unequal(delta, blob.size - last, "unaligned write of bytes");

      blob_write_intptr(&blob, (intptr_t) &blob);

      expect_equal(2 * delta, blob.size - last, "aligned write of intptr");

      last = blob.size;
   }

   /* Finally, test that reading also does proper alignment. Since we know
    * that values were written with all the right alignment, all we have to do
    * here is verify that correct values are read.
    */
   blob_reader_init(&reader, blob.data, blob.size);

   expect_equal((intptr_t) &blob, blob_read_intptr(&reader),
                "read of initial, aligned intptr_t");

   for (num_bytes = 1; num_bytes < sizeof(intptr_t); num_bytes++) {
      expect_equal_bytes(bytes, blob_read_bytes(&reader, num_bytes),
                         num_bytes, "unaligned read of bytes");
      expect_equal((intptr_t) &blob, blob_read_intptr(&reader),
                   "aligned read of intptr_t");
   }

   blob_finish(&blob);
}

/* Test that we detect overrun. */
static void
test_overrun(void)
{
   struct blob blob;
   struct blob_reader reader;
   uint32_t value = 0xdeadbeef;

   blob_init(&blob);

   blob_write_uint32(&blob, value);

   blob_reader_init(&reader, blob.data, blob.size);

   expect_equal(value, blob_read_uint32(&reader), "read before overrun");
   expect_equal(false, reader.overrun, "overrun flag not set");
   expect_equal(0, blob_read_uint32(&reader), "read at overrun");
   expect_equal(true, reader.overrun, "overrun flag set");

   blob_finish(&blob);
}

/* Test that we can read and write some large objects, (exercising the code in
 * the blob_write functions to realloc blob->data.
 */
static void
test_big_objects(void)
{
   void *ctx = ralloc_context(NULL);
   struct blob blob;
   struct blob_reader reader;
   int size = 1000;
   int count = 1000;
   size_t i;
   char *buf;

   blob_init(&blob);

   /* Initialize our buffer. */
   buf = ralloc_size(ctx, size);
   for (i = 0; i < size; i++) {
      buf[i] = i % 256;
   }

   /* Write it many times. */
   for (i = 0; i < count; i++) {
      blob_write_bytes(&blob, buf, size);
   }

   blob_reader_init(&reader, blob.data, blob.size);

   /* Read and verify it many times. */
   for (i = 0; i < count; i++) {
      expect_equal_bytes((uint8_t *) buf, blob_read_bytes(&reader, size), size,
                         "read of large objects");
   }

   expect_equal(reader.end - reader.data, reader.current - reader.data,
                "number of bytes read reading large objects");

   expect_equal(false, reader.overrun,
                "overrun flag not set reading large objects");

   blob_finish(&blob);
   ralloc_free(ctx);
}

int
main (void)
{
   test_write_and_read_functions ();
   test_alignment ();
   test_overrun ();
   test_big_objects ();

   return error ? 1 : 0;
}
