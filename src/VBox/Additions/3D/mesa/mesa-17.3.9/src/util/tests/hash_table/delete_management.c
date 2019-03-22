/*
 * Copyright © 2009 Intel Corporation
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
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "hash_table.h"

static uint32_t
key_value(const void *key)
{
   return *(const uint32_t *)key;
}

static bool
uint32_t_key_equals(const void *a, const void *b)
{
   return key_value(a) == key_value(b);
}

int
main(int argc, char **argv)
{
   struct hash_table *ht;
   struct hash_entry *entry;
   unsigned size = 10000;
   uint32_t keys[size];
   uint32_t i;

   (void) argc;
   (void) argv;

   ht = _mesa_hash_table_create(NULL, key_value, uint32_t_key_equals);

   for (i = 0; i < size; i++) {
      keys[i] = i;

      _mesa_hash_table_insert(ht, keys + i, NULL);

      if (i >= 100) {
         uint32_t delete_value = i - 100;
         entry = _mesa_hash_table_search(ht, &delete_value);
         _mesa_hash_table_remove(ht, entry);
      }
   }

   /* Make sure that all our entries were present at the end. */
   for (i = size - 100; i < size; i++) {
      entry = _mesa_hash_table_search(ht, keys + i);
      assert(entry);
      assert(key_value(entry->key) == i);
   }

   /* Make sure that no extra entries got in */
   for (entry = _mesa_hash_table_next_entry(ht, NULL);
        entry != NULL;
        entry = _mesa_hash_table_next_entry(ht, entry)) {
      assert(key_value(entry->key) >= size - 100 &&
             key_value(entry->key) < size);
   }
   assert(ht->entries == 100);

   _mesa_hash_table_destroy(ht, NULL);

   return 0;
}
