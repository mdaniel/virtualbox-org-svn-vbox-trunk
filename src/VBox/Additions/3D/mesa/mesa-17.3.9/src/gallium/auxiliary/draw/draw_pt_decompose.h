#define LOCAL_VARS                           \
   char *verts = (char *) vertices;          \
   const boolean quads_flatshade_last =      \
      draw->quads_always_flatshade_last;     \
   const boolean last_vertex_last =          \
      !(draw->rasterizer->flatshade &&       \
        draw->rasterizer->flatshade_first);
/* FIXME: the draw->rasterizer->flatshade part is really wrong */

#include "draw_decompose_tmp.h"
