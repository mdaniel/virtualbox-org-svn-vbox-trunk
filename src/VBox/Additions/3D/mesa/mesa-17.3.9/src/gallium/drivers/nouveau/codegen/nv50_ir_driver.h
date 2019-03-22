/*
 * Copyright 2011 Christoph Bumiller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __NV50_IR_DRIVER_H__
#define __NV50_IR_DRIVER_H__

#include "pipe/p_shader_tokens.h"

#include "tgsi/tgsi_util.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_scan.h"

/*
 * This struct constitutes linkage information in TGSI terminology.
 *
 * It is created by the code generator and handed to the pipe driver
 * for input/output slot assignment.
 */
struct nv50_ir_varying
{
   uint8_t slot[4]; /* native slots for xyzw (addresses in 32-bit words) */

   unsigned mask     : 4; /* vec4 mask */
   unsigned linear   : 1; /* linearly interpolated if true (and not flat) */
   unsigned flat     : 1;
   unsigned sc       : 1; /* special colour interpolation mode (SHADE_MODEL) */
   unsigned centroid : 1;
   unsigned patch    : 1; /* patch constant value */
   unsigned regular  : 1; /* driver-specific meaning (e.g. input in sreg) */
   unsigned input    : 1; /* indicates direction of system values */
   unsigned oread    : 1; /* true if output is read from parallel TCP */

   ubyte id; /* TGSI register index */
   ubyte sn; /* TGSI semantic name */
   ubyte si; /* TGSI semantic index */
};

#ifdef DEBUG
# define NV50_IR_DEBUG_BASIC     (1 << 0)
# define NV50_IR_DEBUG_VERBOSE   (2 << 0)
# define NV50_IR_DEBUG_REG_ALLOC (1 << 2)
#else
# define NV50_IR_DEBUG_BASIC     0
# define NV50_IR_DEBUG_VERBOSE   0
# define NV50_IR_DEBUG_REG_ALLOC 0
#endif

struct nv50_ir_prog_symbol
{
   uint32_t label;
   uint32_t offset;
};

#define NVISA_GK104_CHIPSET    0xe0
#define NVISA_GK20A_CHIPSET    0xea
#define NVISA_GM107_CHIPSET    0x110

struct nv50_ir_prog_info
{
   uint16_t target; /* chipset (0x50, 0x84, 0xc0, ...) */

   uint8_t type; /* PIPE_SHADER */

   uint8_t optLevel; /* optimization level (0 to 3) */
   uint8_t dbgFlags;

   struct {
      int16_t maxGPR;     /* may be -1 if none used */
      int16_t maxOutput;
      uint32_t tlsSpace;  /* required local memory per thread */
      uint32_t *code;
      uint32_t codeSize;
      uint32_t instructions;
      uint8_t sourceRep;  /* PIPE_SHADER_IR_* */
      const void *source;
      void *relocData;
      void *fixupData;
      struct nv50_ir_prog_symbol *syms;
      uint16_t numSyms;
   } bin;

   struct nv50_ir_varying sv[PIPE_MAX_SHADER_INPUTS];
   struct nv50_ir_varying in[PIPE_MAX_SHADER_INPUTS];
   struct nv50_ir_varying out[PIPE_MAX_SHADER_OUTPUTS];
   uint8_t numInputs;
   uint8_t numOutputs;
   uint8_t numPatchConstants; /* also included in numInputs/numOutputs */
   uint8_t numSysVals;

   struct {
      uint32_t *buf;    /* for IMMEDIATE_ARRAY */
      uint16_t bufSize; /* size of immediate array */
      uint16_t count;   /* count of inline immediates */
      uint32_t *data;   /* inline immediate data */
      uint8_t *type;    /* for each vec4 (128 bit) */
   } immd;

   union {
      struct {
         uint32_t inputMask[4]; /* mask of attributes read (1 bit per scalar) */
         bool usesDrawParameters;
      } vp;
      struct {
         uint8_t inputPatchSize;
         uint8_t outputPatchSize;
         uint8_t partitioning;    /* PIPE_TESS_PART */
         int8_t winding;          /* +1 (clockwise) / -1 (counter-clockwise) */
         uint8_t domain;          /* PIPE_PRIM_{QUADS,TRIANGLES,LINES} */
         uint8_t outputPrim;      /* PIPE_PRIM_{TRIANGLES,LINES,POINTS} */
      } tp;
      struct {
         uint8_t inputPrim;
         uint8_t outputPrim;
         unsigned instanceCount;
         unsigned maxVertices;
      } gp;
      struct {
         unsigned numColourResults;
         bool writesDepth;
         bool earlyFragTests;
         bool postDepthCoverage;
         bool separateFragData;
         bool usesDiscard;
         bool persampleInvocation;
         bool usesSampleMaskIn;
         bool readsFramebuffer;
      } fp;
      struct {
         uint32_t inputOffset; /* base address for user args */
         uint32_t sharedOffset; /* reserved space in s[] */
         uint32_t gridInfoBase;  /* base address for NTID,NCTAID */
         uint16_t numThreads[3]; /* max number of threads */
      } cp;
   } prop;

   uint8_t numBarriers;

   struct {
      uint8_t clipDistances;     /* number of clip distance outputs */
      uint8_t cullDistances;     /* number of cull distance outputs */
      int8_t genUserClip;        /* request user clip planes for ClipVertex */
      uint8_t auxCBSlot;         /* driver constant buffer slot */
      uint16_t ucpBase;          /* base address for UCPs */
      uint16_t drawInfoBase;     /* base address for draw parameters */
      uint16_t alphaRefBase;     /* base address for alpha test values */
      uint8_t pointSize;         /* output index for PointSize */
      uint8_t instanceId;        /* system value index of InstanceID */
      uint8_t vertexId;          /* system value index of VertexID */
      uint8_t edgeFlagIn;
      uint8_t edgeFlagOut;
      int8_t viewportId;         /* output index of ViewportIndex */
      uint8_t fragDepth;         /* output index of FragDepth */
      uint8_t sampleMask;        /* output index of SampleMask */
      uint8_t backFaceColor[2];  /* input/output indices of back face colour */
      uint8_t globalAccess;      /* 1 for read, 2 for wr, 3 for rw */
      bool fp64;                 /* program uses fp64 math */
      bool mul_zero_wins;        /* program wants for x*0 = 0 */
      bool nv50styleSurfaces;    /* generate gX[] access for raw buffers */
      uint16_t texBindBase;      /* base address for tex handles (nve4) */
      uint16_t fbtexBindBase;    /* base address for fbtex handle (nve4) */
      uint16_t suInfoBase;       /* base address for surface info (nve4) */
      uint16_t bufInfoBase;      /* base address for buffer info */
      uint16_t sampleInfoBase;   /* base address for sample positions */
      uint8_t msInfoCBSlot;      /* cX[] used for multisample info */
      uint16_t msInfoBase;       /* base address for multisample info */
      uint16_t uboInfoBase;      /* base address for compute UBOs (gk104+) */
   } io;

   /* driver callback to assign input/output locations */
   int (*assignSlots)(struct nv50_ir_prog_info *);

   void *driverPriv;
};

#ifdef __cplusplus
extern "C" {
#endif

extern int nv50_ir_generate_code(struct nv50_ir_prog_info *);

extern void nv50_ir_relocate_code(void *relocData, uint32_t *code,
                                  uint32_t codePos,
                                  uint32_t libPos,
                                  uint32_t dataPos);

extern void
nv50_ir_apply_fixups(void *fixupData, uint32_t *code,
                     bool force_per_sample, bool flatshade,
                     uint8_t alphatest);

/* obtain code that will be shared among programs */
extern void nv50_ir_get_target_library(uint32_t chipset,
                                       const uint32_t **code, uint32_t *size);

#ifdef __cplusplus
}
#endif

#endif // __NV50_IR_DRIVER_H__
