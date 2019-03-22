#ifndef INTEL_BATCHBUFFER_H
#define INTEL_BATCHBUFFER_H

#include "main/mtypes.h"

#include "brw_context.h"
#include "brw_bufmgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The kernel assumes batchbuffers are smaller than 256kB. */
#define MAX_BATCH_SIZE (256 * 1024)

/* 3DSTATE_BINDING_TABLE_POINTERS has a U16 offset from Surface State Base
 * Address, which means that we can't put binding tables beyond 64kB.  This
 * effectively limits the maximum statebuffer size to 64kB.
 */
#define MAX_STATE_SIZE (64 * 1024)

struct intel_batchbuffer;

void intel_batchbuffer_init(struct brw_context *brw);
void intel_batchbuffer_free(struct intel_batchbuffer *batch);
void intel_batchbuffer_save_state(struct brw_context *brw);
void intel_batchbuffer_reset_to_saved(struct brw_context *brw);
void intel_batchbuffer_require_space(struct brw_context *brw, GLuint sz,
                                     enum brw_gpu_ring ring);
int _intel_batchbuffer_flush_fence(struct brw_context *brw,
                                   int in_fence_fd, int *out_fence_fd,
                                   const char *file, int line);

#define intel_batchbuffer_flush(brw) \
   _intel_batchbuffer_flush_fence((brw), -1, NULL, __FILE__, __LINE__)

#define intel_batchbuffer_flush_fence(brw, in_fence_fd, out_fence_fd) \
   _intel_batchbuffer_flush_fence((brw), (in_fence_fd), (out_fence_fd), \
                                  __FILE__, __LINE__)

/* Unlike bmBufferData, this currently requires the buffer be mapped.
 * Consider it a convenience function wrapping multple
 * intel_buffer_dword() calls.
 */
void intel_batchbuffer_data(struct brw_context *brw,
                            const void *data, GLuint bytes,
                            enum brw_gpu_ring ring);

bool brw_batch_has_aperture_space(struct brw_context *brw,
                                  unsigned extra_space_in_bytes);

bool brw_batch_references(struct intel_batchbuffer *batch, struct brw_bo *bo);

#define RELOC_WRITE EXEC_OBJECT_WRITE
#define RELOC_NEEDS_GGTT EXEC_OBJECT_NEEDS_GTT
uint64_t brw_batch_reloc(struct intel_batchbuffer *batch,
                         uint32_t batch_offset,
                         struct brw_bo *target,
                         uint32_t target_offset,
                         unsigned flags);
uint64_t brw_state_reloc(struct intel_batchbuffer *batch,
                         uint32_t batch_offset,
                         struct brw_bo *target,
                         uint32_t target_offset,
                         unsigned flags);

#define USED_BATCH(_batch) \
   ((uintptr_t)((_batch).map_next - (_batch).batch.map))

static inline uint32_t float_as_int(float f)
{
   union {
      float f;
      uint32_t d;
   } fi;

   fi.f = f;
   return fi.d;
}

static inline void
intel_batchbuffer_begin(struct brw_context *brw, int n, enum brw_gpu_ring ring)
{
   intel_batchbuffer_require_space(brw, n * 4, ring);

#ifdef DEBUG
   brw->batch.emit = USED_BATCH(brw->batch);
   brw->batch.total = n;
#endif
}

static inline void
intel_batchbuffer_advance(struct brw_context *brw)
{
#ifdef DEBUG
   struct intel_batchbuffer *batch = &brw->batch;
   unsigned int _n = USED_BATCH(*batch) - batch->emit;
   assert(batch->total != 0);
   if (_n != batch->total) {
      fprintf(stderr, "ADVANCE_BATCH: %d of %d dwords emitted\n",
	      _n, batch->total);
      abort();
   }
   batch->total = 0;
#else
   (void) brw;
#endif
}

static inline bool
brw_ptr_in_state_buffer(struct intel_batchbuffer *batch, void *p)
{
   return (char *) p >= (char *) batch->state.map &&
          (char *) p < (char *) batch->state.map + batch->state.bo->size;
}

#define BEGIN_BATCH(n) do {                            \
   intel_batchbuffer_begin(brw, (n), RENDER_RING);     \
   uint32_t *__map = brw->batch.map_next;              \
   brw->batch.map_next += (n)

#define BEGIN_BATCH_BLT(n) do {                        \
   intel_batchbuffer_begin(brw, (n), BLT_RING);        \
   uint32_t *__map = brw->batch.map_next;              \
   brw->batch.map_next += (n)

#define OUT_BATCH(d) *__map++ = (d)
#define OUT_BATCH_F(f) OUT_BATCH(float_as_int((f)))

#define OUT_RELOC(buf, flags, delta) do {          \
   uint32_t __offset = (__map - brw->batch.batch.map) * 4;              \
   uint32_t reloc =                                                     \
      brw_batch_reloc(&brw->batch, __offset, (buf), (delta), (flags));  \
   OUT_BATCH(reloc);                                                    \
} while (0)

/* Handle 48-bit address relocations for Gen8+ */
#define OUT_RELOC64(buf, flags, delta) do {        \
   uint32_t __offset = (__map - brw->batch.batch.map) * 4;              \
   uint64_t reloc64 =                                                   \
      brw_batch_reloc(&brw->batch, __offset, (buf), (delta), (flags));  \
   OUT_BATCH(reloc64);                                                  \
   OUT_BATCH(reloc64 >> 32);                                            \
} while (0)

#define ADVANCE_BATCH()                  \
   assert(__map == brw->batch.map_next); \
   intel_batchbuffer_advance(brw);       \
} while (0)

#ifdef __cplusplus
}
#endif

#endif
