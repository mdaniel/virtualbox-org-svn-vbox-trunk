/*
 * Copyright © 2015 Intel Corporation
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

/* A collection of unit tests for cache.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>
#include <stdarg.h>
#include <inttypes.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#include "util/mesa-sha1.h"
#include "util/disk_cache.h"

bool error = false;

#ifdef ENABLE_SHADER_CACHE

static void
expect_true(bool result, const char *test)
{
   if (!result) {
      fprintf(stderr, "Error: Test '%s' failed: Expected=true"
              ", Actual=false\n", test);
      error = true;
   }
}

static void
expect_equal(uint64_t actual, uint64_t expected, const char *test)
{
   if (actual != expected) {
      fprintf(stderr, "Error: Test '%s' failed: Expected=%" PRIu64
              ", Actual=%" PRIu64 "\n",
              test, expected, actual);
      error = true;
   }
}

static void
expect_null(void *ptr, const char *test)
{
   if (ptr != NULL) {
      fprintf(stderr, "Error: Test '%s' failed: Result=%p, but expected NULL.\n",
              test, ptr);
      error = true;
   }
}

static void
expect_non_null(void *ptr, const char *test)
{
   if (ptr == NULL) {
      fprintf(stderr, "Error: Test '%s' failed: Result=NULL, but expected something else.\n",
              test);
      error = true;
   }
}

static void
expect_equal_str(const char *actual, const char *expected, const char *test)
{
   if (strcmp(actual, expected)) {
      fprintf(stderr, "Error: Test '%s' failed:\n\t"
              "Expected=\"%s\", Actual=\"%s\"\n",
              test, expected, actual);
      error = true;
   }
}

/* Callback for nftw used in rmrf_local below.
 */
static int
remove_entry(const char *path,
             const struct stat *sb,
             int typeflag,
             struct FTW *ftwbuf)
{
   int err = remove(path);

   if (err)
      fprintf(stderr, "Error removing %s: %s\n", path, strerror(errno));

   return err;
}

/* Recursively remove a directory.
 *
 * This is equivalent to "rm -rf <dir>" with one bit of protection
 * that the directory name must begin with "." to ensure we don't
 * wander around deleting more than intended.
 *
 * Returns 0 on success, -1 on any error.
 */
static int
rmrf_local(const char *path)
{
   if (path == NULL || *path == '\0' || *path != '.')
      return -1;

   return nftw(path, remove_entry, 64, FTW_DEPTH | FTW_PHYS);
}

static void
check_directories_created(const char *cache_dir)
{
   bool sub_dirs_created = false;

   char buf[PATH_MAX];
   if (getcwd(buf, PATH_MAX)) {
      char *full_path = NULL;
      if (asprintf(&full_path, "%s%s", buf, ++cache_dir) != -1 ) {
         struct stat sb;
         if (stat(full_path, &sb) != -1 && S_ISDIR(sb.st_mode))
            sub_dirs_created = true;

         free(full_path);
      }
   }

   expect_true(sub_dirs_created, "create sub dirs");
}

#define CACHE_TEST_TMP "./cache-test-tmp"

static void
test_disk_cache_create(void)
{
   struct disk_cache *cache;
   int err;

   /* Before doing anything else, ensure that with
    * MESA_GLSL_CACHE_DISABLE set to true, that disk_cache_create returns NULL.
    */
   setenv("MESA_GLSL_CACHE_DISABLE", "true", 1);
   cache = disk_cache_create("test", "make_check", 0);
   expect_null(cache, "disk_cache_create with MESA_GLSL_CACHE_DISABLE set");

   unsetenv("MESA_GLSL_CACHE_DISABLE");

   /* For the first real disk_cache_create() clear these environment
    * variables to test creation of cache in home directory.
    */
   unsetenv("MESA_GLSL_CACHE_DIR");
   unsetenv("XDG_CACHE_HOME");

   cache = disk_cache_create("test", "make_check", 0);
   expect_non_null(cache, "disk_cache_create with no environment variables");

   disk_cache_destroy(cache);

   /* Test with XDG_CACHE_HOME set */
   setenv("XDG_CACHE_HOME", CACHE_TEST_TMP "/xdg-cache-home", 1);
   cache = disk_cache_create("test", "make_check", 0);
   expect_null(cache, "disk_cache_create with XDG_CACHE_HOME set with"
               "a non-existing parent directory");

   mkdir(CACHE_TEST_TMP, 0755);
   cache = disk_cache_create("test", "make_check", 0);
   expect_non_null(cache, "disk_cache_create with XDG_CACHE_HOME set");

   check_directories_created(CACHE_TEST_TMP "/xdg-cache-home/"
                             CACHE_DIR_NAME);

   disk_cache_destroy(cache);

   /* Test with MESA_GLSL_CACHE_DIR set */
   err = rmrf_local(CACHE_TEST_TMP);
   expect_equal(err, 0, "Removing " CACHE_TEST_TMP);

   setenv("MESA_GLSL_CACHE_DIR", CACHE_TEST_TMP "/mesa-glsl-cache-dir", 1);
   cache = disk_cache_create("test", "make_check", 0);
   expect_null(cache, "disk_cache_create with MESA_GLSL_CACHE_DIR set with"
               "a non-existing parent directory");

   mkdir(CACHE_TEST_TMP, 0755);
   cache = disk_cache_create("test", "make_check", 0);
   expect_non_null(cache, "disk_cache_create with MESA_GLSL_CACHE_DIR set");

   check_directories_created(CACHE_TEST_TMP "/mesa-glsl-cache-dir/"
                             CACHE_DIR_NAME);

   disk_cache_destroy(cache);
}

static bool
does_cache_contain(struct disk_cache *cache, const cache_key key)
{
   void *result;

   result = disk_cache_get(cache, key, NULL);

   if (result) {
      free(result);
      return true;
   }

   return false;
}

static void
wait_until_file_written(struct disk_cache *cache, const cache_key key)
{
   struct timespec req;
   struct timespec rem;

   /* Set 100ms delay */
   req.tv_sec = 0;
   req.tv_nsec = 100000000;

   unsigned retries = 0;
   while (retries++ < 20) {
      if (does_cache_contain(cache, key)) {
         break;
      }

      nanosleep(&req, &rem);
   }
}

static void
test_put_and_get(void)
{
   struct disk_cache *cache;
   char blob[] = "This is a blob of thirty-seven bytes";
   uint8_t blob_key[20];
   char string[] = "While this string has thirty-four";
   uint8_t string_key[20];
   char *result;
   size_t size;
   uint8_t *one_KB, *one_MB;
   uint8_t one_KB_key[20], one_MB_key[20];
   int count;

   cache = disk_cache_create("test", "make_check", 0);

   disk_cache_compute_key(cache, blob, sizeof(blob), blob_key);

   /* Ensure that disk_cache_get returns nothing before anything is added. */
   result = disk_cache_get(cache, blob_key, &size);
   expect_null(result, "disk_cache_get with non-existent item (pointer)");
   expect_equal(size, 0, "disk_cache_get with non-existent item (size)");

   /* Simple test of put and get. */
   disk_cache_put(cache, blob_key, blob, sizeof(blob), NULL);

   /* disk_cache_put() hands things off to a thread give it some time to
    * finish.
    */
   wait_until_file_written(cache, blob_key);

   result = disk_cache_get(cache, blob_key, &size);
   expect_equal_str(blob, result, "disk_cache_get of existing item (pointer)");
   expect_equal(size, sizeof(blob), "disk_cache_get of existing item (size)");

   free(result);

   /* Test put and get of a second item. */
   disk_cache_compute_key(cache, string, sizeof(string), string_key);
   disk_cache_put(cache, string_key, string, sizeof(string), NULL);

   /* disk_cache_put() hands things off to a thread give it some time to
    * finish.
    */
   wait_until_file_written(cache, string_key);

   result = disk_cache_get(cache, string_key, &size);
   expect_equal_str(result, string, "2nd disk_cache_get of existing item (pointer)");
   expect_equal(size, sizeof(string), "2nd disk_cache_get of existing item (size)");

   free(result);

   /* Set the cache size to 1KB and add a 1KB item to force an eviction. */
   disk_cache_destroy(cache);

   setenv("MESA_GLSL_CACHE_MAX_SIZE", "1K", 1);
   cache = disk_cache_create("test", "make_check", 0);

   one_KB = calloc(1, 1024);

   /* Obviously the SHA-1 hash of 1024 zero bytes isn't particularly
    * interesting. But we do have want to take some special care with
    * the hash we use here. The issue is that in this artificial case,
    * (with only three files in the cache), the probability is good
    * that each of the three files will end up in their own
    * directory. Then, if the directory containing the .tmp file for
    * the new item being added for disk_cache_put() is the chosen victim
    * directory for eviction, then no suitable file will be found and
    * nothing will be evicted.
    *
    * That's actually expected given how the eviction code is
    * implemented, (which expects to only evict once things are more
    * interestingly full than that).
    *
    * For this test, we force this signature to land in the same
    * directory as the original blob first written to the cache.
    */
   disk_cache_compute_key(cache, one_KB, 1024, one_KB_key);
   one_KB_key[0] = blob_key[0];

   disk_cache_put(cache, one_KB_key, one_KB, 1024, NULL);

   free(one_KB);

   /* disk_cache_put() hands things off to a thread give it some time to
    * finish.
    */
   wait_until_file_written(cache, one_KB_key);

   result = disk_cache_get(cache, one_KB_key, &size);
   expect_non_null(result, "3rd disk_cache_get of existing item (pointer)");
   expect_equal(size, 1024, "3rd disk_cache_get of existing item (size)");

   free(result);

   /* Ensure eviction happened by checking that both of the previous
    * cache itesm were evicted.
    */
   bool contains_1KB_file = false;
   count = 0;
   if (does_cache_contain(cache, blob_key))
       count++;

   if (does_cache_contain(cache, string_key))
       count++;

   if (does_cache_contain(cache, one_KB_key)) {
      count++;
      contains_1KB_file = true;
   }

   expect_true(contains_1KB_file,
               "disk_cache_put eviction last file == MAX_SIZE (1KB)");
   expect_equal(count, 1, "disk_cache_put eviction with MAX_SIZE=1K");

   /* Now increase the size to 1M, add back both items, and ensure all
    * three that have been added are available via disk_cache_get.
    */
   disk_cache_destroy(cache);

   setenv("MESA_GLSL_CACHE_MAX_SIZE", "1M", 1);
   cache = disk_cache_create("test", "make_check", 0);

   disk_cache_put(cache, blob_key, blob, sizeof(blob), NULL);
   disk_cache_put(cache, string_key, string, sizeof(string), NULL);

   /* disk_cache_put() hands things off to a thread give it some time to
    * finish.
    */
   wait_until_file_written(cache, blob_key);
   wait_until_file_written(cache, string_key);

   count = 0;
   if (does_cache_contain(cache, blob_key))
       count++;

   if (does_cache_contain(cache, string_key))
       count++;

   if (does_cache_contain(cache, one_KB_key))
       count++;

   expect_equal(count, 3, "no eviction before overflow with MAX_SIZE=1M");

   /* Finally, check eviction again after adding an object of size 1M. */
   one_MB = calloc(1024, 1024);

   disk_cache_compute_key(cache, one_MB, 1024 * 1024, one_MB_key);
   one_MB_key[0] = blob_key[0];

   disk_cache_put(cache, one_MB_key, one_MB, 1024 * 1024, NULL);

   free(one_MB);

   /* disk_cache_put() hands things off to a thread give it some time to
    * finish.
    */
   wait_until_file_written(cache, one_MB_key);

   bool contains_1MB_file = false;
   count = 0;
   if (does_cache_contain(cache, blob_key))
       count++;

   if (does_cache_contain(cache, string_key))
       count++;

   if (does_cache_contain(cache, one_KB_key))
       count++;

   if (does_cache_contain(cache, one_MB_key)) {
      count++;
      contains_1MB_file = true;
   }

   expect_true(contains_1MB_file,
               "disk_cache_put eviction last file == MAX_SIZE (1MB)");
   expect_equal(count, 1, "eviction after overflow with MAX_SIZE=1M");

   disk_cache_destroy(cache);
}

static void
test_put_key_and_get_key(void)
{
   struct disk_cache *cache;
   bool result;

   uint8_t key_a[20] = {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                         10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
   uint8_t key_b[20] = { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
                         30, 33, 32, 33, 34, 35, 36, 37, 38, 39};
   uint8_t key_a_collide[20] =
                        { 0,  1, 42, 43, 44, 45, 46, 47, 48, 49,
                         50, 55, 52, 53, 54, 55, 56, 57, 58, 59};

   cache = disk_cache_create("test", "make_check", 0);

   /* First test that disk_cache_has_key returns false before disk_cache_put_key */
   result = disk_cache_has_key(cache, key_a);
   expect_equal(result, 0, "disk_cache_has_key before key added");

   /* Then a couple of tests of disk_cache_put_key followed by disk_cache_has_key */
   disk_cache_put_key(cache, key_a);
   result = disk_cache_has_key(cache, key_a);
   expect_equal(result, 1, "disk_cache_has_key after key added");

   disk_cache_put_key(cache, key_b);
   result = disk_cache_has_key(cache, key_b);
   expect_equal(result, 1, "2nd disk_cache_has_key after key added");

   /* Test that a key with the same two bytes as an existing key
    * forces an eviction.
    */
   disk_cache_put_key(cache, key_a_collide);
   result = disk_cache_has_key(cache, key_a_collide);
   expect_equal(result, 1, "put_key of a colliding key lands in the cache");

   result = disk_cache_has_key(cache, key_a);
   expect_equal(result, 0, "put_key of a colliding key evicts from the cache");

   /* And finally test that we can re-add the original key to re-evict
    * the colliding key.
    */
   disk_cache_put_key(cache, key_a);
   result = disk_cache_has_key(cache, key_a);
   expect_equal(result, 1, "put_key of original key lands again");

   result = disk_cache_has_key(cache, key_a_collide);
   expect_equal(result, 0, "put_key of orginal key evicts the colliding key");

   disk_cache_destroy(cache);
}
#endif /* ENABLE_SHADER_CACHE */

int
main(void)
{
#ifdef ENABLE_SHADER_CACHE
   int err;

   test_disk_cache_create();

   test_put_and_get();

   test_put_key_and_get_key();

   err = rmrf_local(CACHE_TEST_TMP);
   expect_equal(err, 0, "Removing " CACHE_TEST_TMP " again");
#endif /* ENABLE_SHADER_CACHE */

   return error ? 1 : 0;
}
