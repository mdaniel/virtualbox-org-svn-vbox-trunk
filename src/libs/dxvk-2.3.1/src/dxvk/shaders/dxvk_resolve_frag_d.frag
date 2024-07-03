#version 450

#extension GL_EXT_samplerless_texture_functions : enable

#define VK_RESOLVE_MODE_NONE            (0)
#define VK_RESOLVE_MODE_SAMPLE_ZERO_BIT (1 << 0)
#define VK_RESOLVE_MODE_AVERAGE_BIT     (1 << 1)
#define VK_RESOLVE_MODE_MIN_BIT         (1 << 2)
#define VK_RESOLVE_MODE_MAX_BIT         (1 << 3)

layout(constant_id = 0) const int c_samples = 1;
layout(constant_id = 1) const int c_mode_d  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;

layout(binding = 0) uniform texture2DMSArray s_depth;

layout(push_constant)
uniform u_info_t {
  ivec2 offset;
} u_info;

float resolve_depth(ivec3 coord) {
  float depth = 0.0f;

  switch (c_mode_d) {
    case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT:
      depth = texelFetch(s_depth, coord, 0).r;
      break;
    
    case VK_RESOLVE_MODE_AVERAGE_BIT:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth += texelFetch(s_depth, coord, i).r;
      depth /= float(c_samples);
      break;
    
    case VK_RESOLVE_MODE_MIN_BIT:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth = min(depth, texelFetch(s_depth, coord, i).r);
      break;
    
    case VK_RESOLVE_MODE_MAX_BIT:
      depth = texelFetch(s_depth, coord, 0).r;
      for (int i = 1; i < c_samples; i++)
        depth = max(depth, texelFetch(s_depth, coord, i).r);
      break;
  }

  return depth;
}

void main() {
  ivec3 coord = ivec3(gl_FragCoord.xy + u_info.offset, gl_Layer);

  gl_FragDepth = resolve_depth(coord);
}