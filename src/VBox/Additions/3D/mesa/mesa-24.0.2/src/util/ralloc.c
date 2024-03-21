/*
 * Copyright © 2010 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/list.h"
#include "util/macros.h"
#include "util/u_math.h"
#include "util/u_printf.h"

#include "ralloc.h"

#define CANARY 0x5A1106

#if defined(__LP64__) || defined(_WIN64)
#define HEADER_ALIGN 16
#else
#define HEADER_ALIGN 8
#endif

/* Align the header's size so that ralloc() allocations will return with the
 * same alignment as a libc malloc would have (8 on 32-bit GLIBC, 16 on
 * 64-bit), avoiding performance penalities on x86 and alignment faults on
 * ARM.
 */
struct ralloc_header
{
   alignas(HEADER_ALIGN)

#ifndef NDEBUG
   /* A canary value used to determine whether a pointer is ralloc'd. */
   unsigned canary;
   unsigned size;
#endif

   struct ralloc_header *parent;

   /* The first child (head of a linked list) */
   struct ralloc_header *child;

   /* Linked list of siblings */
   struct ralloc_header *prev;
   struct ralloc_header *next;

   void (*destructor)(void *);
};

typedef struct ralloc_header ralloc_header;

static void unlink_block(ralloc_header *info);
static void unsafe_free(ralloc_header *info);

static ralloc_header *
get_header(const void *ptr)
{
   ralloc_header *info = (ralloc_header *) (((char *) ptr) -
					    sizeof(ralloc_header));
   assert(info->canary == CANARY);
   return info;
}

#define PTR_FROM_HEADER(info) (((char *) info) + sizeof(ralloc_header))

static void
add_child(ralloc_header *parent, ralloc_header *info)
{
   if (parent != NULL) {
      info->parent = parent;
      info->next = parent->child;
      parent->child = info;

      if (info->next != NULL)
	 info->next->prev = info;
   }
}

void *
ralloc_context(const void *ctx)
{
   return ralloc_size(ctx, 0);
}

void *
ralloc_size(const void *ctx, size_t size)
{
   /* Some malloc allocation doesn't always align to 16 bytes even on 64 bits
    * system, from Android bionic/tests/malloc_test.cpp:
    *  - Allocations of a size that rounds up to a multiple of 16 bytes
    *    must have at least 16 byte alignment.
    *  - Allocations of a size that rounds up to a multiple of 8 bytes and
    *    not 16 bytes, are only required to have at least 8 byte alignment.
    */
   void *block = malloc(align64(size + sizeof(ralloc_header),
                                alignof(ralloc_header)));
   ralloc_header *info;
   ralloc_header *parent;

   if (unlikely(block == NULL))
      return NULL;

   info = (ralloc_header *) block;
   /* measurements have shown that calloc is slower (because of
    * the multiplication overflow checking?), so clear things
    * manually
    */
   info->parent = NULL;
   info->child = NULL;
   info->prev = NULL;
   info->next = NULL;
   info->destructor = NULL;

   parent = ctx != NULL ? get_header(ctx) : NULL;

   add_child(parent, info);

#ifndef NDEBUG
   info->canary = CANARY;
   info->size = size;
#endif

   return PTR_FROM_HEADER(info);
}

void *
rzalloc_size(const void *ctx, size_t size)
{
   void *ptr = ralloc_size(ctx, size);

   if (likely(ptr))
      memset(ptr, 0, size);

   return ptr;
}

/* helper function - assumes ptr != NULL */
static void *
resize(void *ptr, size_t size)
{
   ralloc_header *child, *old, *info;

   old = get_header(ptr);
   info = realloc(old, align64(size + sizeof(ralloc_header),
                               alignof(ralloc_header)));

   if (info == NULL)
      return NULL;

   /* Update parent and sibling's links to the reallocated node. */
   if (info != old && info->parent != NULL) {
      if (info->parent->child == old)
	 info->parent->child = info;

      if (info->prev != NULL)
	 info->prev->next = info;

      if (info->next != NULL)
	 info->next->prev = info;
   }

   /* Update child->parent links for all children */
   for (child = info->child; child != NULL; child = child->next)
      child->parent = info;

   return PTR_FROM_HEADER(info);
}

void *
reralloc_size(const void *ctx, void *ptr, size_t size)
{
   if (unlikely(ptr == NULL))
      return ralloc_size(ctx, size);

   assert(ralloc_parent(ptr) == ctx);
   return resize(ptr, size);
}

void *
rerzalloc_size(const void *ctx, void *ptr, size_t old_size, size_t new_size)
{
   if (unlikely(ptr == NULL))
      return rzalloc_size(ctx, new_size);

   assert(ralloc_parent(ptr) == ctx);
   ptr = resize(ptr, new_size);

   if (new_size > old_size)
      memset((char *)ptr + old_size, 0, new_size - old_size);

   return ptr;
}

void *
ralloc_array_size(const void *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return ralloc_size(ctx, size * count);
}

void *
rzalloc_array_size(const void *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return rzalloc_size(ctx, size * count);
}

void *
reralloc_array_size(const void *ctx, void *ptr, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return reralloc_size(ctx, ptr, size * count);
}

void *
rerzalloc_array_size(const void *ctx, void *ptr, size_t size,
                     unsigned old_count, unsigned new_count)
{
   if (new_count > SIZE_MAX/size)
      return NULL;

   return rerzalloc_size(ctx, ptr, size * old_count, size * new_count);
}

void
ralloc_free(void *ptr)
{
   ralloc_header *info;

   if (ptr == NULL)
      return;

   info = get_header(ptr);
   unlink_block(info);
   unsafe_free(info);
}

static void
unlink_block(ralloc_header *info)
{
   /* Unlink from parent & siblings */
   if (info->parent != NULL) {
      if (info->parent->child == info)
	 info->parent->child = info->next;

      if (info->prev != NULL)
	 info->prev->next = info->next;

      if (info->next != NULL)
	 info->next->prev = info->prev;
   }
   info->parent = NULL;
   info->prev = NULL;
   info->next = NULL;
}

static void
unsafe_free(ralloc_header *info)
{
   /* Recursively free any children...don't waste time unlinking them. */
   ralloc_header *temp;
   while (info->child != NULL) {
      temp = info->child;
      info->child = temp->next;
      unsafe_free(temp);
   }

   /* Free the block itself.  Call the destructor first, if any. */
   if (info->destructor != NULL)
      info->destructor(PTR_FROM_HEADER(info));

   free(info);
}

void
ralloc_steal(const void *new_ctx, void *ptr)
{
   ralloc_header *info, *parent;

   if (unlikely(ptr == NULL))
      return;

   info = get_header(ptr);
   parent = new_ctx ? get_header(new_ctx) : NULL;

   unlink_block(info);

   add_child(parent, info);
}

void
ralloc_adopt(const void *new_ctx, void *old_ctx)
{
   ralloc_header *new_info, *old_info, *child;

   if (unlikely(old_ctx == NULL))
      return;

   old_info = get_header(old_ctx);
   new_info = get_header(new_ctx);

   /* If there are no children, bail. */
   if (unlikely(old_info->child == NULL))
      return;

   /* Set all the children's parent to new_ctx; get a pointer to the last child. */
   for (child = old_info->child; child->next != NULL; child = child->next) {
      child->parent = new_info;
   }
   child->parent = new_info;

   /* Connect the two lists together; parent them to new_ctx; make old_ctx empty. */
   child->next = new_info->child;
   if (child->next)
      child->next->prev = child;
   new_info->child = old_info->child;
   old_info->child = NULL;
}

void *
ralloc_parent(const void *ptr)
{
   ralloc_header *info;

   if (unlikely(ptr == NULL))
      return NULL;

   info = get_header(ptr);
   return info->parent ? PTR_FROM_HEADER(info->parent) : NULL;
}

void
ralloc_set_destructor(const void *ptr, void(*destructor)(void *))
{
   ralloc_header *info = get_header(ptr);
   info->destructor = destructor;
}

char *
ralloc_strdup(const void *ctx, const char *str)
{
   size_t n;
   char *ptr;

   if (unlikely(str == NULL))
      return NULL;

   n = strlen(str);
   ptr = ralloc_array(ctx, char, n + 1);
   memcpy(ptr, str, n);
   ptr[n] = '\0';
   return ptr;
}

char *
ralloc_strndup(const void *ctx, const char *str, size_t max)
{
   size_t n;
   char *ptr;

   if (unlikely(str == NULL))
      return NULL;

   n = strnlen(str, max);
   ptr = ralloc_array(ctx, char, n + 1);
   memcpy(ptr, str, n);
   ptr[n] = '\0';
   return ptr;
}

/* helper routine for strcat/strncat - n is the exact amount to copy */
static bool
cat(char **dest, const char *str, size_t n)
{
   char *both;
   size_t existing_length;
   assert(dest != NULL && *dest != NULL);

   existing_length = strlen(*dest);
   both = resize(*dest, existing_length + n + 1);
   if (unlikely(both == NULL))
      return false;

   memcpy(both + existing_length, str, n);
   both[existing_length + n] = '\0';

   *dest = both;
   return true;
}


bool
ralloc_strcat(char **dest, const char *str)
{
   return cat(dest, str, strlen(str));
}

bool
ralloc_strncat(char **dest, const char *str, size_t n)
{
   return cat(dest, str, strnlen(str, n));
}

bool
ralloc_str_append(char **dest, const char *str,
                  size_t existing_length, size_t str_size)
{
   char *both;
   assert(dest != NULL && *dest != NULL);

   both = resize(*dest, existing_length + str_size + 1);
   if (unlikely(both == NULL))
      return false;

   memcpy(both + existing_length, str, str_size);
   both[existing_length + str_size] = '\0';

   *dest = both;

   return true;
}

char *
ralloc_asprintf(const void *ctx, const char *fmt, ...)
{
   char *ptr;
   va_list args;
   va_start(args, fmt);
   ptr = ralloc_vasprintf(ctx, fmt, args);
   va_end(args);
   return ptr;
}

char *
ralloc_vasprintf(const void *ctx, const char *fmt, va_list args)
{
   size_t size = u_printf_length(fmt, args) + 1;

   char *ptr = ralloc_size(ctx, size);
   if (ptr != NULL)
      vsnprintf(ptr, size, fmt, args);

   return ptr;
}

bool
ralloc_asprintf_append(char **str, const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = ralloc_vasprintf_append(str, fmt, args);
   va_end(args);
   return success;
}

bool
ralloc_vasprintf_append(char **str, const char *fmt, va_list args)
{
   size_t existing_length;
   assert(str != NULL);
   existing_length = *str ? strlen(*str) : 0;
   return ralloc_vasprintf_rewrite_tail(str, &existing_length, fmt, args);
}

bool
ralloc_asprintf_rewrite_tail(char **str, size_t *start, const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = ralloc_vasprintf_rewrite_tail(str, start, fmt, args);
   va_end(args);
   return success;
}

bool
ralloc_vasprintf_rewrite_tail(char **str, size_t *start, const char *fmt,
			      va_list args)
{
   size_t new_length;
   char *ptr;

   assert(str != NULL);

   if (unlikely(*str == NULL)) {
      // Assuming a NULL context is probably bad, but it's expected behavior.
      *str = ralloc_vasprintf(NULL, fmt, args);
      *start = strlen(*str);
      return true;
   }

   new_length = u_printf_length(fmt, args);

   ptr = resize(*str, *start + new_length + 1);
   if (unlikely(ptr == NULL))
      return false;

   vsnprintf(ptr + *start, new_length + 1, fmt, args);
   *str = ptr;
   *start += new_length;
   return true;
}

/***************************************************************************
 * GC context.
 ***************************************************************************
 */

/* The maximum size of an object that will be allocated specially.
 */
#define MAX_FREELIST_SIZE 512

/* Allocations small enough to be allocated from a freelist will be aligned up
 * to this size.
 */
#define FREELIST_ALIGNMENT 32

#define NUM_FREELIST_BUCKETS (MAX_FREELIST_SIZE / FREELIST_ALIGNMENT)

/* The size of a slab. */
#define SLAB_SIZE (32 * 1024)

#define GC_CONTEXT_CANARY 0xAF6B6C83
#define GC_CANARY 0xAF6B5B72

enum gc_flags {
   IS_USED = (1 << 0),
   CURRENT_GENERATION = (1 << 1),
   IS_PADDING = (1 << 7),
};

typedef struct
{
#ifndef NDEBUG
   /* A canary value used to determine whether a pointer is allocated using gc_alloc. */
   unsigned canary;
#endif

   uint16_t slab_offset;
   uint8_t bucket;
   uint8_t flags;

   /* The last padding byte must have IS_PADDING set and is used to store the amount of padding. If
    * there is no padding, the IS_PADDING bit of "flags" is unset and "flags" is checked instead.
    * Because of this, "flags" must be the last member of this struct.
    */
   uint8_t padding[];
} gc_block_header;

/* This structure is at the start of the slab. Objects inside a slab are
 * allocated using a freelist backed by a simple linear allocator.
 */
typedef struct gc_slab {
   alignas(HEADER_ALIGN)

   gc_ctx *ctx;

   /* Objects are allocated using either linear or freelist allocation. "next_available" is the
    * pointer used for linear allocation, while "freelist" is the next free object for freelist
    * allocation.
    */
   char *next_available;
   gc_block_header *freelist;

   /* Slabs that handle the same-sized objects. */
   struct list_head link;

   /* Free slabs that handle the same-sized objects. */
   struct list_head free_link;

   /* Number of allocated and free objects, recorded so that we can free the slab if it
    * becomes empty or add one to the freelist if it's no longer full.
    */
   unsigned num_allocated;
   unsigned num_free;
} gc_slab;

struct gc_ctx {
#ifndef NDEBUG
   unsigned canary;
#endif

   /* Array of slabs for fixed-size allocations. Each slab tracks allocations
    * of specific sized blocks. User allocations are rounded up to the nearest
    * fixed size. slabs[N] contains allocations of size
    * FREELIST_ALIGNMENT * (N + 1).
    */
   struct {
      /* List of slabs in this bucket. */
      struct list_head slabs;

      /* List of slabs with free space in this bucket, so we can quickly choose one when
       * allocating.
       */
      struct list_head free_slabs;
   } slabs[NUM_FREELIST_BUCKETS];

   uint8_t current_gen;
   void *rubbish;
};

static gc_block_header *
get_gc_header(const void *ptr)
{
   uint8_t *c_ptr = (uint8_t *)ptr;

   /* Adjust for padding added to ensure alignment of the allocation. There might also be padding
    * added by the compiler into gc_block_header, but that isn't counted in the IS_PADDING byte.
    */
   if (c_ptr[-1] & IS_PADDING)
      c_ptr -= c_ptr[-1] & ~IS_PADDING;

   c_ptr -= sizeof(gc_block_header);

   gc_block_header *info = (gc_block_header *)c_ptr;
   assert(info->canary == GC_CANARY);
   return info;
}

static gc_block_header *
get_gc_freelist_next(gc_block_header *ptr)
{
   gc_block_header *next;
   /* work around possible strict aliasing bug using memcpy */
   memcpy(&next, (void*)(ptr + 1), sizeof(next));
   return next;
}

static void
set_gc_freelist_next(gc_block_header *ptr, gc_block_header *next)
{
   memcpy((void*)(ptr + 1), &next, sizeof(next));
}

static gc_slab *
get_gc_slab(gc_block_header *header)
{
   return (gc_slab *)((char *)header - header->slab_offset);
}

gc_ctx *
gc_context(const void *parent)
{
   gc_ctx *ctx = rzalloc(parent, gc_ctx);
   for (unsigned i = 0; i < NUM_FREELIST_BUCKETS; i++) {
      list_inithead(&ctx->slabs[i].slabs);
      list_inithead(&ctx->slabs[i].free_slabs);
   }
#ifndef NDEBUG
   ctx->canary = GC_CONTEXT_CANARY;
#endif
   return ctx;
}

static_assert(UINT32_MAX >= MAX_FREELIST_SIZE, "Freelist sizes use uint32_t");

static uint32_t
gc_bucket_obj_size(uint32_t bucket)
{
   return (bucket + 1) * FREELIST_ALIGNMENT;
}

static uint32_t
gc_bucket_for_size(uint32_t size)
{
   return (size - 1) / FREELIST_ALIGNMENT;
}

static_assert(UINT32_MAX >= SLAB_SIZE, "SLAB_SIZE use uint32_t");

static uint32_t
gc_bucket_num_objs(uint32_t bucket)
{
   return (SLAB_SIZE - sizeof(gc_slab)) / gc_bucket_obj_size(bucket);
}

static gc_block_header *
alloc_from_slab(gc_slab *slab, uint32_t bucket)
{
   uint32_t size = gc_bucket_obj_size(bucket);
   gc_block_header *header;
   if (slab->freelist) {
      /* Prioritize already-allocated chunks, since they probably have a page
       * backing them.
       */
      header = slab->freelist;
      slab->freelist = get_gc_freelist_next(slab->freelist);
   } else if (slab->next_available + size <= ((char *) slab) + SLAB_SIZE) {
      header = (gc_block_header *) slab->next_available;
      header->slab_offset = (char *) header - (char *) slab;
      header->bucket = bucket;
      slab->next_available += size;
   } else {
      return NULL;
   }

   slab->num_allocated++;
   slab->num_free--;
   if (!slab->num_free)
      list_del(&slab->free_link);
   return header;
}

static void
free_slab(gc_slab *slab)
{
   if (list_is_linked(&slab->free_link))
      list_del(&slab->free_link);
   list_del(&slab->link);
   ralloc_free(slab);
}

static void
free_from_slab(gc_block_header *header, bool keep_empty_slabs)
{
   gc_slab *slab = get_gc_slab(header);

   if (slab->num_allocated == 1 && !(keep_empty_slabs && list_is_singular(&slab->free_link))) {
      /* Free the slab if this is the last object. */
      free_slab(slab);
      return;
   } else if (slab->num_free == 0) {
      list_add(&slab->free_link, &slab->ctx->slabs[header->bucket].free_slabs);
   } else {
      /* Keep the free list sorted by the number of free objects in ascending order. By prefering to
       * allocate from the slab with the fewest free objects, we help free the slabs with many free
       * objects.
       */
      while (slab->free_link.next != &slab->ctx->slabs[header->bucket].free_slabs &&
             slab->num_free > list_entry(slab->free_link.next, gc_slab, free_link)->num_free) {
         gc_slab *next = list_entry(slab->free_link.next, gc_slab, free_link);

         /* Move "slab" to after "next". */
         list_move_to(&slab->free_link, &next->free_link);
      }
   }

   set_gc_freelist_next(header, slab->freelist);
   slab->freelist = header;

   slab->num_allocated--;
   slab->num_free++;
}

static uint32_t
get_slab_size(uint32_t bucket)
{
   /* SLAB_SIZE rounded down to a multiple of the object size so that it's not larger than what can
    * be used.
    */
   uint32_t obj_size = gc_bucket_obj_size(bucket);
   uint32_t num_objs = gc_bucket_num_objs(bucket);
   return align((uint32_t)sizeof(gc_slab) + num_objs * obj_size, alignof(gc_slab));
}

static gc_slab *
create_slab(gc_ctx *ctx, unsigned bucket)
{
   gc_slab *slab = ralloc_size(ctx, get_slab_size(bucket));
   if (unlikely(!slab))
      return NULL;

   slab->ctx = ctx;
   slab->freelist = NULL;
   slab->next_available = (char*)(slab + 1);
   slab->num_allocated = 0;
   slab->num_free = gc_bucket_num_objs(bucket);

   list_addtail(&slab->link, &ctx->slabs[bucket].slabs);
   list_addtail(&slab->free_link, &ctx->slabs[bucket].free_slabs);

   return slab;
}

void *
gc_alloc_size(gc_ctx *ctx, size_t size, size_t alignment)
{
   assert(ctx);
   assert(util_is_power_of_two_nonzero(alignment));

   alignment = MAX2(alignment, alignof(gc_block_header));

   /* Alignment will add at most align-alignof(gc_block_header) bytes of padding to the header, and
    * the IS_PADDING byte can only encode up to 127.
    */
   assert((alignment - alignof(gc_block_header)) <= 127);

   /* We can only align as high as the slab is. */
   assert(alignment <= HEADER_ALIGN);

   size_t header_size = align64(sizeof(gc_block_header), alignment);
   size = align64(size, alignment);
   size += header_size;

   gc_block_header *header = NULL;
   if (size <= MAX_FREELIST_SIZE) {
      uint32_t bucket = gc_bucket_for_size((uint32_t)size);
      if (list_is_empty(&ctx->slabs[bucket].free_slabs) && !create_slab(ctx, bucket))
         return NULL;
      gc_slab *slab = list_first_entry(&ctx->slabs[bucket].free_slabs, gc_slab, free_link);
      header = alloc_from_slab(slab, bucket);
   } else {
      header = ralloc_size(ctx, size);
      if (unlikely(!header))
         return NULL;
      /* Mark the header as allocated directly, so we know to actually free it. */
      header->bucket = NUM_FREELIST_BUCKETS;
   }

   header->flags = ctx->current_gen | IS_USED;
#ifndef NDEBUG
   header->canary = GC_CANARY;
#endif

   uint8_t *ptr = (uint8_t *)header + header_size;
   if ((header_size - 1) != offsetof(gc_block_header, flags))
      ptr[-1] = IS_PADDING | (header_size - sizeof(gc_block_header));

   assert(((uintptr_t)ptr & (alignment - 1)) == 0);
   return ptr;
}

void *
gc_zalloc_size(gc_ctx *ctx, size_t size, size_t alignment)
{
   void *ptr = gc_alloc_size(ctx, size, alignment);

   if (likely(ptr))
      memset(ptr, 0, size);

   return ptr;
}

void
gc_free(void *ptr)
{
   if (!ptr)
      return;

   gc_block_header *header = get_gc_header(ptr);
   header->flags &= ~IS_USED;

   if (header->bucket < NUM_FREELIST_BUCKETS)
      free_from_slab(header, true);
   else
      ralloc_free(header);
}

gc_ctx *gc_get_context(void *ptr)
{
   gc_block_header *header = get_gc_header(ptr);

   if (header->bucket < NUM_FREELIST_BUCKETS)
      return get_gc_slab(header)->ctx;
   else
      return ralloc_parent(header);
}

void
gc_sweep_start(gc_ctx *ctx)
{
   ctx->current_gen ^= CURRENT_GENERATION;

   ctx->rubbish = ralloc_context(NULL);
   ralloc_adopt(ctx->rubbish, ctx);
}

void
gc_mark_live(gc_ctx *ctx, const void *mem)
{
   gc_block_header *header = get_gc_header(mem);
   if (header->bucket < NUM_FREELIST_BUCKETS)
      header->flags ^= CURRENT_GENERATION;
   else
      ralloc_steal(ctx, header);
}

void
gc_sweep_end(gc_ctx *ctx)
{
   assert(ctx->rubbish);

   for (unsigned i = 0; i < NUM_FREELIST_BUCKETS; i++) {
      unsigned obj_size = gc_bucket_obj_size(i);
      list_for_each_entry_safe(gc_slab, slab, &ctx->slabs[i].slabs, link) {
         if (!slab->num_allocated) {
            free_slab(slab);
            continue;
         }

         for (char *ptr = (char*)(slab + 1); ptr != slab->next_available; ptr += obj_size) {
            gc_block_header *header = (gc_block_header *)ptr;
            if (!(header->flags & IS_USED))
               continue;
            if ((header->flags & CURRENT_GENERATION) == ctx->current_gen)
               continue;

            bool last = slab->num_allocated == 1;

            header->flags &= ~IS_USED;
            free_from_slab(header, false);

            if (last)
               break;
         }
      }
   }

   for (unsigned i = 0; i < NUM_FREELIST_BUCKETS; i++) {
      list_for_each_entry(gc_slab, slab, &ctx->slabs[i].slabs, link) {
         assert(slab->num_allocated > 0); /* free_from_slab() should free it otherwise */
         ralloc_steal(ctx, slab);
      }
   }

   ralloc_free(ctx->rubbish);
   ctx->rubbish = NULL;
}

/***************************************************************************
 * Linear allocator for short-lived allocations.
 ***************************************************************************
 *
 * The allocator consists of a parent node (2K buffer), which requires
 * a ralloc parent, and child nodes (allocations). Child nodes can't be freed
 * directly, because the parent doesn't track them. You have to release
 * the parent node in order to release all its children.
 *
 * The allocator uses a fixed-sized buffer with a monotonically increasing
 * offset after each allocation. If the buffer is all used, another buffer
 * is allocated, using the linear parent node as ralloc parent.
 *
 * The linear parent node is always the first buffer and keeps track of all
 * other buffers.
 */

#define SUBALLOC_ALIGNMENT 8
#define LMAGIC_CONTEXT 0x87b9c7d3
#define LMAGIC_NODE    0x87b910d3

struct linear_ctx {

   alignas(HEADER_ALIGN)

#ifndef NDEBUG
   unsigned magic;   /* for debugging */
#endif
   unsigned min_buffer_size;

   unsigned offset;  /* points to the first unused byte in the latest buffer */
   unsigned size;    /* size of the latest buffer */
   void *latest;     /* the only buffer that has free space */
};

typedef struct linear_ctx linear_ctx;

#ifndef NDEBUG
struct linear_node_canary {
   alignas(HEADER_ALIGN)
   unsigned magic;
   unsigned offset;  /* points to the first unused byte in *this* buffer */
};

typedef struct linear_node_canary linear_node_canary;

static linear_node_canary *
get_node_canary(void *ptr)
{
   return (void *)((char *)ptr - sizeof(linear_node_canary));
}
#endif

static unsigned
get_node_canary_size()
{
#ifndef NDEBUG
   return sizeof(linear_node_canary);
#else
   return 0;
#endif
}

void *
linear_alloc_child(linear_ctx *ctx, unsigned size)
{
   assert(ctx->magic == LMAGIC_CONTEXT);
   assert(get_node_canary(ctx->latest)->magic == LMAGIC_NODE);
   assert(get_node_canary(ctx->latest)->offset == ctx->offset);

   size = ALIGN_POT(size, SUBALLOC_ALIGNMENT);

   if (unlikely(ctx->offset + size > ctx->size)) {
      /* allocate a new node */
      unsigned node_size = size;
      if (likely(node_size < ctx->min_buffer_size))
         node_size = ctx->min_buffer_size;

      const unsigned canary_size = get_node_canary_size();
      const unsigned full_size = canary_size + node_size;

      /* linear context is also a ralloc context */
      char *ptr = ralloc_size(ctx, full_size);
      if (unlikely(!ptr))
         return NULL;

#ifndef NDEBUG
      linear_node_canary *canary = (void *) ptr;
      canary->magic = LMAGIC_NODE;
      canary->offset = 0;
#endif

      /* If the new buffer is going to be full, don't update `latest`
       * pointer.  Either the current one is also full, so doesn't
       * matter, or the current one is not full, so there's still chance
       * to use that space.
       */
      if (unlikely(size == node_size)) {
#ifndef NDEBUG
         canary->offset = size;
#endif
         assert((uintptr_t)(ptr + canary_size) % SUBALLOC_ALIGNMENT == 0);
         return ptr + canary_size;
      }

      ctx->offset = 0;
      ctx->size = node_size;
      ctx->latest = ptr + canary_size;
   }

   void *ptr = (char *)ctx->latest + ctx->offset;
   ctx->offset += size;

#ifndef NDEBUG
   linear_node_canary *canary = get_node_canary(ctx->latest);
   canary->offset += size;
#endif

   assert((uintptr_t)ptr % SUBALLOC_ALIGNMENT == 0);
   return ptr;
}

linear_ctx *
linear_context(void *ralloc_ctx)
{
   const linear_opts opts = {0};
   return linear_context_with_opts(ralloc_ctx, &opts);
}

linear_ctx *
linear_context_with_opts(void *ralloc_ctx, const linear_opts *opts)
{
   linear_ctx *ctx;

   if (unlikely(!ralloc_ctx))
      return NULL;

   const unsigned default_min_buffer_size = 2048;
   const unsigned min_buffer_size =
      MAX2(ALIGN_POT(opts->min_buffer_size, default_min_buffer_size),
           default_min_buffer_size);

   const unsigned size = min_buffer_size;
   const unsigned canary_size = get_node_canary_size();
   const unsigned full_size =
      sizeof(linear_ctx) + canary_size + size;

   ctx = ralloc_size(ralloc_ctx, full_size);
   if (unlikely(!ctx))
      return NULL;

   ctx->min_buffer_size = min_buffer_size;

   ctx->offset = 0;
   ctx->size = size;
   ctx->latest = (char *)&ctx[1] + canary_size;
#ifndef NDEBUG
   ctx->magic = LMAGIC_CONTEXT;
   linear_node_canary *canary = get_node_canary(ctx->latest);
   canary->magic = LMAGIC_NODE;
   canary->offset = 0;
#endif

   return ctx;
}

void *
linear_zalloc_child(linear_ctx *ctx, unsigned size)
{
   void *ptr = linear_alloc_child(ctx, size);

   if (likely(ptr))
      memset(ptr, 0, size);
   return ptr;
}

void
linear_free_context(linear_ctx *ctx)
{
   if (unlikely(!ctx))
      return;

   assert(ctx->magic == LMAGIC_CONTEXT);

   /* Linear context is also the ralloc parent of extra nodes. */
   ralloc_free(ctx);
}

void
ralloc_steal_linear_context(void *new_ralloc_ctx, linear_ctx *ctx)
{
   if (unlikely(!ctx))
      return;

   assert(ctx->magic == LMAGIC_CONTEXT);

   /* Linear context is also the ralloc parent of extra nodes. */
   ralloc_steal(new_ralloc_ctx, ctx);
}

void *
ralloc_parent_of_linear_context(linear_ctx *ctx)
{
   assert(ctx->magic == LMAGIC_CONTEXT);
   return PTR_FROM_HEADER(get_header(ctx)->parent);
}

/* All code below is pretty much copied from ralloc and only the alloc
 * calls are different.
 */

char *
linear_strdup(linear_ctx *ctx, const char *str)
{
   unsigned n;
   char *ptr;

   if (unlikely(!str))
      return NULL;

   n = strlen(str);
   ptr = linear_alloc_child(ctx, n + 1);
   if (unlikely(!ptr))
      return NULL;

   memcpy(ptr, str, n);
   ptr[n] = '\0';
   return ptr;
}

char *
linear_asprintf(linear_ctx *ctx, const char *fmt, ...)
{
   char *ptr;
   va_list args;
   va_start(args, fmt);
   ptr = linear_vasprintf(ctx, fmt, args);
   va_end(args);
   return ptr;
}

char *
linear_vasprintf(linear_ctx *ctx, const char *fmt, va_list args)
{
   unsigned size = u_printf_length(fmt, args) + 1;

   char *ptr = linear_alloc_child(ctx, size);
   if (ptr != NULL)
      vsnprintf(ptr, size, fmt, args);

   return ptr;
}

bool
linear_asprintf_append(linear_ctx *ctx, char **str, const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = linear_vasprintf_append(ctx, str, fmt, args);
   va_end(args);
   return success;
}

bool
linear_vasprintf_append(linear_ctx *ctx, char **str, const char *fmt, va_list args)
{
   size_t existing_length;
   assert(str != NULL);
   existing_length = *str ? strlen(*str) : 0;
   return linear_vasprintf_rewrite_tail(ctx, str, &existing_length, fmt, args);
}

bool
linear_asprintf_rewrite_tail(linear_ctx *ctx, char **str, size_t *start,
                             const char *fmt, ...)
{
   bool success;
   va_list args;
   va_start(args, fmt);
   success = linear_vasprintf_rewrite_tail(ctx, str, start, fmt, args);
   va_end(args);
   return success;
}

bool
linear_vasprintf_rewrite_tail(linear_ctx *ctx, char **str, size_t *start,
                              const char *fmt, va_list args)
{
   size_t new_length;
   char *ptr;

   assert(str != NULL);

   if (unlikely(*str == NULL)) {
      *str = linear_vasprintf(ctx, fmt, args);
      *start = strlen(*str);
      return true;
   }

   new_length = u_printf_length(fmt, args);

   ptr = linear_alloc_child(ctx, *start + new_length + 1);
   if (unlikely(ptr == NULL))
      return false;

   memcpy(ptr, *str, *start);

   vsnprintf(ptr + *start, new_length + 1, fmt, args);
   *str = ptr;
   *start += new_length;
   return true;
}

/* helper routine for strcat/strncat - n is the exact amount to copy */
static bool
linear_cat(linear_ctx *ctx, char **dest, const char *str, unsigned n)
{
   char *both;
   unsigned existing_length;
   assert(dest != NULL && *dest != NULL);

   existing_length = strlen(*dest);
   both = linear_alloc_child(ctx, existing_length + n + 1);
   if (unlikely(both == NULL))
      return false;

   memcpy(both, *dest, existing_length);
   memcpy(both + existing_length, str, n);
   both[existing_length + n] = '\0';

   *dest = both;
   return true;
}

bool
linear_strcat(linear_ctx *ctx, char **dest, const char *str)
{
   return linear_cat(ctx, dest, str, strlen(str));
}

void *
linear_alloc_child_array(linear_ctx *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return linear_alloc_child(ctx, size * count);
}

void *
linear_zalloc_child_array(linear_ctx *ctx, size_t size, unsigned count)
{
   if (count > SIZE_MAX/size)
      return NULL;

   return linear_zalloc_child(ctx, size * count);
}

typedef struct {
   FILE *f;
   unsigned indent;

   unsigned ralloc_count;
   unsigned linear_count;
   unsigned gc_count;

   /* These don't include padding or metadata from suballocators. */
   unsigned content_bytes;
   unsigned ralloc_metadata_bytes;
   unsigned linear_metadata_bytes;
   unsigned gc_metadata_bytes;

   bool inside_linear;
   bool inside_gc;
} ralloc_print_info_state;

static void
ralloc_print_info_helper(ralloc_print_info_state *state, const ralloc_header *info)
{
   FILE *f = state->f;

   if (f) {
      for (unsigned i = 0; i < state->indent; i++) fputc(' ', f);
      fprintf(f, "%p", info);
   }

   /* TODO: Account for padding used in various places. */

#ifndef NDEBUG
   assert(info->canary == CANARY);
   if (f) fprintf(f, " (%d bytes)", info->size);
   state->content_bytes += info->size;
   state->ralloc_metadata_bytes += sizeof(ralloc_header);

   const void *ptr = PTR_FROM_HEADER(info);
   const linear_ctx *lin_ctx = ptr;
   const gc_ctx *gc_ctx = ptr;

   if (lin_ctx->magic == LMAGIC_CONTEXT) {
      if (f) fprintf(f, " (linear context)");
      assert(!state->inside_gc && !state->inside_linear);
      state->inside_linear = true;
      state->linear_metadata_bytes += sizeof(linear_ctx);
      state->content_bytes -= sizeof(linear_ctx);
      state->linear_count++;
   } else if (gc_ctx->canary == GC_CONTEXT_CANARY) {
      if (f) fprintf(f, " (gc context)");
      assert(!state->inside_gc && !state->inside_linear);
      state->inside_gc = true;
      state->gc_metadata_bytes += sizeof(gc_block_header);
   } else if (state->inside_linear) {
      const linear_node_canary *lin_node = ptr;
      if (lin_node->magic == LMAGIC_NODE) {
         if (f) fprintf(f, " (linear node buffer)");
         state->content_bytes -= sizeof(linear_node_canary);
         state->linear_metadata_bytes += sizeof(linear_node_canary);
         state->linear_count++;
      }
   } else if (state->inside_gc) {
      if (f) fprintf(f, " (gc slab or large block)");
      state->gc_count++;
   }
#endif

   state->ralloc_count++;
   if (f) fprintf(f, "\n");

   const ralloc_header *c = info->child;
   state->indent += 2;
   while (c != NULL) {
      ralloc_print_info_helper(state, c);
      c = c->next;
   }
   state->indent -= 2;

#ifndef NDEBUG
   if (lin_ctx->magic == LMAGIC_CONTEXT) state->inside_linear = false;
   else if (gc_ctx->canary == GC_CONTEXT_CANARY) state->inside_gc = false;
#endif
}

void
ralloc_print_info(FILE *f, const void *p, unsigned flags)
{
   ralloc_print_info_state state = {
      .f =  ((flags & RALLOC_PRINT_INFO_SUMMARY_ONLY) == 1) ? NULL : f,
   };

   const ralloc_header *info = get_header(p);
   ralloc_print_info_helper(&state, info);

   fprintf(f, "==== RALLOC INFO ptr=%p info=%p\n"
              "ralloc allocations    = %d\n"
              "  - linear            = %d\n"
              "  - gc                = %d\n"
              "  - other             = %d\n",
              p, info,
              state.ralloc_count,
              state.linear_count,
              state.gc_count,
              state.ralloc_count - state.linear_count - state.gc_count);

   if (state.content_bytes) {
      fprintf(f,
              "content bytes         = %d\n"
              "ralloc metadata bytes = %d\n"
              "linear metadata bytes = %d\n",
              state.content_bytes,
              state.ralloc_metadata_bytes,
              state.linear_metadata_bytes);
   }

   fprintf(f, "====\n");
}

