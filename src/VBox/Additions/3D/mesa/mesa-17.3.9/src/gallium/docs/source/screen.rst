.. _screen:

Screen
======

A screen is an object representing the context-independent part of a device.

Flags and enumerations
----------------------

XXX some of these don't belong in this section.


.. _pipe_cap:

PIPE_CAP_*
^^^^^^^^^^

Capability queries return information about the features and limits of the
driver/GPU.  For floating-point values, use :ref:`get_paramf`, and for boolean
or integer values, use :ref:`get_param`.

The integer capabilities:

* ``PIPE_CAP_NPOT_TEXTURES``: Whether :term:`NPOT` textures may have repeat modes,
  normalized coordinates, and mipmaps.
* ``PIPE_CAP_TWO_SIDED_STENCIL``: Whether the stencil test can also affect back-facing
  polygons.
* ``PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS``: How many dual-source blend RTs are support.
  :ref:`Blend` for more information.
* ``PIPE_CAP_ANISOTROPIC_FILTER``: Whether textures can be filtered anisotropically.
* ``PIPE_CAP_POINT_SPRITE``: Whether point sprites are available.
* ``PIPE_CAP_MAX_RENDER_TARGETS``: The maximum number of render targets that may be
  bound.
* ``PIPE_CAP_OCCLUSION_QUERY``: Whether occlusion queries are available.
* ``PIPE_CAP_QUERY_TIME_ELAPSED``: Whether PIPE_QUERY_TIME_ELAPSED queries are available.
* ``PIPE_CAP_TEXTURE_SHADOW_MAP``: indicates whether the fragment shader hardware
  can do the depth texture / Z comparison operation in TEX instructions
  for shadow testing.
* ``PIPE_CAP_TEXTURE_SWIZZLE``: Whether swizzling through sampler views is
  supported.
* ``PIPE_CAP_MAX_TEXTURE_2D_LEVELS``: The maximum number of mipmap levels available
  for a 2D texture.
* ``PIPE_CAP_MAX_TEXTURE_3D_LEVELS``: The maximum number of mipmap levels available
  for a 3D texture.
* ``PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS``: The maximum number of mipmap levels available
  for a cubemap.
* ``PIPE_CAP_TEXTURE_MIRROR_CLAMP``: Whether mirrored texture coordinates with clamp
  are supported.
* ``PIPE_CAP_BLEND_EQUATION_SEPARATE``: Whether alpha blend equations may be different
  from color blend equations, in :ref:`Blend` state.
* ``PIPE_CAP_SM3``: Whether the vertex shader and fragment shader support equivalent
  opcodes to the Shader Model 3 specification. XXX oh god this is horrible
* ``PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS``: The maximum number of stream buffers.
* ``PIPE_CAP_PRIMITIVE_RESTART``: Whether primitive restart is supported.
* ``PIPE_CAP_INDEP_BLEND_ENABLE``: Whether per-rendertarget blend enabling and channel
  masks are supported. If 0, then the first rendertarget's blend mask is
  replicated across all MRTs.
* ``PIPE_CAP_INDEP_BLEND_FUNC``: Whether per-rendertarget blend functions are
  available. If 0, then the first rendertarget's blend functions affect all
  MRTs.
* ``PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS``: The maximum number of texture array
  layers supported. If 0, the array textures are not supported at all and
  the ARRAY texture targets are invalid.
* ``PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT``: Whether the TGSI property
  FS_COORD_ORIGIN with value UPPER_LEFT is supported.
* ``PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT``: Whether the TGSI property
  FS_COORD_ORIGIN with value LOWER_LEFT is supported.
* ``PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER``: Whether the TGSI
  property FS_COORD_PIXEL_CENTER with value HALF_INTEGER is supported.
* ``PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER``: Whether the TGSI
  property FS_COORD_PIXEL_CENTER with value INTEGER is supported.
* ``PIPE_CAP_DEPTH_CLIP_DISABLE``: Whether the driver is capable of disabling
  depth clipping (through pipe_rasterizer_state)
* ``PIPE_CAP_SHADER_STENCIL_EXPORT``: Whether a stencil reference value can be
  written from a fragment shader.
* ``PIPE_CAP_TGSI_INSTANCEID``: Whether TGSI_SEMANTIC_INSTANCEID is supported
  in the vertex shader.
* ``PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR``: Whether the driver supports
  per-instance vertex attribs.
* ``PIPE_CAP_FRAGMENT_COLOR_CLAMPED``: Whether fragment color clamping is
  supported.  That is, is the pipe_rasterizer_state::clamp_fragment_color
  flag supported by the driver?  If not, the state tracker will insert
  clamping code into the fragment shaders when needed.

* ``PIPE_CAP_MIXED_COLORBUFFER_FORMATS``: Whether mixed colorbuffer formats are
  supported, e.g. RGBA8 and RGBA32F as the first and second colorbuffer, resp.
* ``PIPE_CAP_VERTEX_COLOR_UNCLAMPED``: Whether the driver is capable of
  outputting unclamped vertex colors from a vertex shader. If unsupported,
  the vertex colors are always clamped. This is the default for DX9 hardware.
* ``PIPE_CAP_VERTEX_COLOR_CLAMPED``: Whether the driver is capable of
  clamping vertex colors when they come out of a vertex shader, as specified
  by the pipe_rasterizer_state::clamp_vertex_color flag.  If unsupported,
  the vertex colors are never clamped. This is the default for DX10 hardware.
  If both clamped and unclamped CAPs are supported, the clamping can be
  controlled through pipe_rasterizer_state.  If the driver cannot do vertex
  color clamping, the state tracker may insert clamping code into the vertex
  shader.
* ``PIPE_CAP_GLSL_FEATURE_LEVEL``: Whether the driver supports features
  equivalent to a specific GLSL version. E.g. for GLSL 1.3, report 130.
* ``PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION``: Whether quads adhere to
  the flatshade_first setting in ``pipe_rasterizer_state``.
* ``PIPE_CAP_USER_VERTEX_BUFFERS``: Whether the driver supports user vertex
  buffers.  If not, the state tracker must upload all data which is not in hw
  resources.  If user-space buffers are supported, the driver must also still
  accept HW resource buffers.
* ``PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY``: This CAP describes a hw
  limitation.  If true, pipe_vertex_buffer::buffer_offset must always be aligned
  to 4.  If false, there are no restrictions on the offset.
* ``PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY``: This CAP describes a hw
  limitation.  If true, pipe_vertex_buffer::stride must always be aligned to 4.
  If false, there are no restrictions on the stride.
* ``PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY``: This CAP describes
  a hw limitation.  If true, pipe_vertex_element::src_offset must always be
  aligned to 4.  If false, there are no restrictions on src_offset.
* ``PIPE_CAP_COMPUTE``: Whether the implementation supports the
  compute entry points defined in pipe_context and pipe_screen.
* ``PIPE_CAP_USER_CONSTANT_BUFFERS``: Whether user-space constant buffers
  are supported.  If not, the state tracker must put constants into HW
  resources/buffers.  If user-space constant buffers are supported, the
  driver must still accept HW constant buffers also.
* ``PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT``: Describes the required
  alignment of pipe_constant_buffer::buffer_offset.
* ``PIPE_CAP_START_INSTANCE``: Whether the driver supports
  pipe_draw_info::start_instance.
* ``PIPE_CAP_QUERY_TIMESTAMP``: Whether PIPE_QUERY_TIMESTAMP and
  the pipe_screen::get_timestamp hook are implemented.
* ``PIPE_CAP_TEXTURE_MULTISAMPLE``: Whether all MSAA resources supported
  for rendering are also supported for texturing.
* ``PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT``: The minimum alignment that should be
  expected for a pointer returned by transfer_map if the resource is
  PIPE_BUFFER. In other words, the pointer returned by transfer_map is
  always aligned to this value.
* ``PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT``: Describes the required
  alignment for pipe_sampler_view::u.buf.offset, in bytes.
  If a driver does not support offset/size, it should return 0.
* ``PIPE_CAP_BUFFER_SAMPLER_VIEW_RGBA_ONLY``: Whether the driver only
  supports R, RG, RGB and RGBA formats for PIPE_BUFFER sampler views.
  When this is the case it should be assumed that the swizzle parameters
  in the sampler view have no effect.
* ``PIPE_CAP_TGSI_TEXCOORD``: This CAP describes a hw limitation.
  If true, the hardware cannot replace arbitrary shader inputs with sprite
  coordinates and hence the inputs that are desired to be replaceable must
  be declared with TGSI_SEMANTIC_TEXCOORD instead of TGSI_SEMANTIC_GENERIC.
  The rasterizer's sprite_coord_enable state therefore also applies to the
  TEXCOORD semantic.
  Also, TGSI_SEMANTIC_PCOORD becomes available, which labels a fragment shader
  input that will always be replaced with sprite coordinates.
* ``PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER``: Whether it is preferable
  to use a blit to implement a texture transfer which needs format conversions
  and swizzling in state trackers. Generally, all hardware drivers with
  dedicated memory should return 1 and all software rasterizers should return 0.
* ``PIPE_CAP_QUERY_PIPELINE_STATISTICS``: Whether PIPE_QUERY_PIPELINE_STATISTICS
  is supported.
* ``PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK``: Bitmask indicating whether special
  considerations have to be given to the interaction between the border color
  in the sampler object and the sampler view used with it.
  If PIPE_QUIRK_TEXTURE_BORDER_COLOR_SWIZZLE_R600 is set, the border color
  may be affected in undefined ways for any kind of permutational swizzle
  (any swizzle XYZW where X/Y/Z/W are not ZERO, ONE, or R/G/B/A respectively)
  in the sampler view.
  If PIPE_QUIRK_TEXTURE_BORDER_COLOR_SWIZZLE_NV50 is set, the border color
  state should be swizzled manually according to the swizzle in the sampler
  view it is intended to be used with, or herein undefined results may occur
  for permutational swizzles.
* ``PIPE_CAP_MAX_TEXTURE_BUFFER_SIZE``: The maximum accessible size with
  a buffer sampler view, in texels.
* ``PIPE_CAP_MAX_VIEWPORTS``: The maximum number of viewports (and scissors
  since they are linked) a driver can support. Returning 0 is equivalent
  to returning 1 because every driver has to support at least a single
  viewport/scissor combination.
* ``PIPE_CAP_ENDIANNESS``:: The endianness of the device.  Either
  PIPE_ENDIAN_BIG or PIPE_ENDIAN_LITTLE.
* ``PIPE_CAP_MIXED_FRAMEBUFFER_SIZES``: Whether it is allowed to have
  different sizes for fb color/zs attachments. This controls whether
  ARB_framebuffer_object is provided.
* ``PIPE_CAP_TGSI_VS_LAYER_VIEWPORT``: Whether ``TGSI_SEMANTIC_LAYER`` and
  ``TGSI_SEMANTIC_VIEWPORT_INDEX`` are supported as vertex shader
  outputs. Note that the viewport will only be used if multiple viewports are
  exposed.
* ``PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES``: The maximum number of vertices
  output by a single invocation of a geometry shader.
* ``PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS``: The maximum number of
  vertex components output by a single invocation of a geometry shader.
  This is the product of the number of attribute components per vertex and
  the number of output vertices.
* ``PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS``: Max number of components
  in format that texture gather can operate on. 1 == RED, ALPHA etc,
  4 == All formats.
* ``PIPE_CAP_TEXTURE_GATHER_SM5``: Whether the texture gather
  hardware implements the SM5 features, component selection,
  shadow comparison, and run-time offsets.
* ``PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT``: Whether
  PIPE_TRANSFER_PERSISTENT and PIPE_TRANSFER_COHERENT are supported
  for buffers.
* ``PIPE_CAP_TEXTURE_QUERY_LOD``: Whether the ``LODQ`` instruction is
  supported.
* ``PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET``: The minimum offset that can be used
  in conjunction with a texture gather opcode.
* ``PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET``: The maximum offset that can be used
  in conjunction with a texture gather opcode.
* ``PIPE_CAP_SAMPLE_SHADING``: Whether there is support for per-sample
  shading. The context->set_min_samples function will be expected to be
  implemented.
* ``PIPE_CAP_TEXTURE_GATHER_OFFSETS``: Whether the ``TG4`` instruction can
  accept 4 offsets.
* ``PIPE_CAP_TGSI_VS_WINDOW_SPACE_POSITION``: Whether
  TGSI_PROPERTY_VS_WINDOW_SPACE_POSITION is supported, which disables clipping
  and viewport transformation.
* ``PIPE_CAP_MAX_VERTEX_STREAMS``: The maximum number of vertex streams
  supported by the geometry shader. If stream-out is supported, this should be
  at least 1. If stream-out is not supported, this should be 0.
* ``PIPE_CAP_DRAW_INDIRECT``: Whether the driver supports taking draw arguments
  { count, instance_count, start, index_bias } from a PIPE_BUFFER resource.
  See pipe_draw_info.
* ``PIPE_CAP_MULTI_DRAW_INDIRECT``: Whether the driver supports
  pipe_draw_info::indirect_stride and ::indirect_count
* ``PIPE_CAP_MULTI_DRAW_INDIRECT_PARAMS``: Whether the driver supports
  taking the number of indirect draws from a separate parameter
  buffer, see pipe_draw_indirect_info::indirect_draw_count.
* ``PIPE_CAP_TGSI_FS_FINE_DERIVATIVE``: Whether the fragment shader supports
  the FINE versions of DDX/DDY.
* ``PIPE_CAP_VENDOR_ID``: The vendor ID of the underlying hardware. If it's
  not available one should return 0xFFFFFFFF.
* ``PIPE_CAP_DEVICE_ID``: The device ID (PCI ID) of the underlying hardware.
  0xFFFFFFFF if not available.
* ``PIPE_CAP_ACCELERATED``: Whether the renderer is hardware accelerated.
* ``PIPE_CAP_VIDEO_MEMORY``: The amount of video memory in megabytes.
* ``PIPE_CAP_UMA``: If the device has a unified memory architecture or on-card
  memory and GART.
* ``PIPE_CAP_CONDITIONAL_RENDER_INVERTED``: Whether the driver supports inverted
  condition for conditional rendering.
* ``PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE``: The maximum supported vertex stride.
* ``PIPE_CAP_SAMPLER_VIEW_TARGET``: Whether the sampler view's target can be
  different than the underlying resource's, as permitted by
  ARB_texture_view. For example a 2d array texture may be reinterpreted as a
  cube (array) texture and vice-versa.
* ``PIPE_CAP_CLIP_HALFZ``: Whether the driver supports the
  pipe_rasterizer_state::clip_halfz being set to true. This is required
  for enabling ARB_clip_control.
* ``PIPE_CAP_VERTEXID_NOBASE``: If true, the driver only supports
  TGSI_SEMANTIC_VERTEXID_NOBASE (and not TGSI_SEMANTIC_VERTEXID). This means
  state trackers for APIs whose vertexIDs are offset by basevertex (such as GL)
  will need to lower TGSI_SEMANTIC_VERTEXID to TGSI_SEMANTIC_VERTEXID_NOBASE
  and TGSI_SEMANTIC_BASEVERTEX, so drivers setting this must handle both these
  semantics. Only relevant if geometry shaders are supported.
  (BASEVERTEX could be exposed separately too via ``PIPE_CAP_DRAW_PARAMETERS``).
* ``PIPE_CAP_POLYGON_OFFSET_CLAMP``: If true, the driver implements support
  for ``pipe_rasterizer_state::offset_clamp``.
* ``PIPE_CAP_MULTISAMPLE_Z_RESOLVE``: Whether the driver supports blitting
  a multisampled depth buffer into a single-sampled texture (or depth buffer).
  Only the first sampled should be copied.
* ``PIPE_CAP_RESOURCE_FROM_USER_MEMORY``: Whether the driver can create
  a pipe_resource where an already-existing piece of (malloc'd) user memory
  is used as its backing storage. In other words, whether the driver can map
  existing user memory into the device address space for direct device access.
  The create function is pipe_screen::resource_from_user_memory. The address
  and size must be page-aligned.
* ``PIPE_CAP_DEVICE_RESET_STATUS_QUERY``:
  Whether pipe_context::get_device_reset_status is implemented.
* ``PIPE_CAP_MAX_SHADER_PATCH_VARYINGS``:
  How many per-patch outputs and inputs are supported between tessellation
  control and tessellation evaluation shaders, not counting in TESSINNER and
  TESSOUTER. The minimum allowed value for OpenGL is 30.
* ``PIPE_CAP_TEXTURE_FLOAT_LINEAR``: Whether the linear minification and
  magnification filters are supported with single-precision floating-point
  textures.
* ``PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR``: Whether the linear minification and
  magnification filters are supported with half-precision floating-point
  textures.
* ``PIPE_CAP_DEPTH_BOUNDS_TEST``: Whether bounds_test, bounds_min, and
  bounds_max states of pipe_depth_stencil_alpha_state behave according
  to the GL_EXT_depth_bounds_test specification.
* ``PIPE_CAP_TGSI_TXQS``: Whether the `TXQS` opcode is supported
* ``PIPE_CAP_FORCE_PERSAMPLE_INTERP``: If the driver can force per-sample
  interpolation for all fragment shader inputs if
  pipe_rasterizer_state::force_persample_interp is set. This is only used
  by GL3-level sample shading (ARB_sample_shading). GL4-level sample shading
  (ARB_gpu_shader5) doesn't use this. While GL3 hardware has a state for it,
  GL4 hardware will likely need to emulate it with a shader variant, or by
  selecting the interpolation weights with a conditional assignment
  in the shader.
* ``PIPE_CAP_SHAREABLE_SHADERS``: Whether shader CSOs can be used by any
  pipe_context.
* ``PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS``:
  Whether copying between compressed and plain formats is supported where
  a compressed block is copied to/from a plain pixel of the same size.
* ``PIPE_CAP_CLEAR_TEXTURE``: Whether `clear_texture` will be
  available in contexts.
* ``PIPE_CAP_DRAW_PARAMETERS``: Whether ``TGSI_SEMANTIC_BASEVERTEX``,
  ``TGSI_SEMANTIC_BASEINSTANCE``, and ``TGSI_SEMANTIC_DRAWID`` are
  supported in vertex shaders.
* ``PIPE_CAP_TGSI_PACK_HALF_FLOAT``: Whether the ``UP2H`` and ``PK2H``
  TGSI opcodes are supported.
* ``PIPE_CAP_TGSI_FS_POSITION_IS_SYSVAL``: If state trackers should use
  a system value for the POSITION fragment shader input.
* ``PIPE_CAP_TGSI_FS_FACE_IS_INTEGER_SYSVAL``: If state trackers should use
  a system value for the FACE fragment shader input.
  Also, the FACE system value is integer, not float.
* ``PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT``: Describes the required
  alignment for pipe_shader_buffer::buffer_offset, in bytes. Maximum
  value allowed is 256 (for GL conformance). 0 is only allowed if
  shader buffers are not supported.
* ``PIPE_CAP_INVALIDATE_BUFFER``: Whether the use of ``invalidate_resource``
  for buffers is supported.
* ``PIPE_CAP_GENERATE_MIPMAP``: Indicates whether pipe_context::generate_mipmap
  is supported.
* ``PIPE_CAP_STRING_MARKER``: Whether pipe->emit_string_marker() is supported.
* ``PIPE_CAP_SURFACE_REINTERPRET_BLOCKS``: Indicates whether
  pipe_context::create_surface supports reinterpreting a texture as a surface
  of a format with different block width/height (but same block size in bits).
  For example, a compressed texture image can be interpreted as a
  non-compressed surface whose texels are the same number of bits as the
  compressed blocks, and vice versa. The width and height of the surface is
  adjusted appropriately.
* ``PIPE_CAP_QUERY_BUFFER_OBJECT``: Driver supports
  context::get_query_result_resource callback.
* ``PIPE_CAP_PCI_GROUP``: Return the PCI segment group number.
* ``PIPE_CAP_PCI_BUS``: Return the PCI bus number.
* ``PIPE_CAP_PCI_DEVICE``: Return the PCI device number.
* ``PIPE_CAP_PCI_FUNCTION``: Return the PCI function number.
* ``PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT``:
  If non-zero, rendering to framebuffers with no surface attachments
  is supported. The context->is_format_supported function will be expected
  to be implemented with PIPE_FORMAT_NONE yeilding the MSAA modes the hardware
  supports. N.B., The maximum number of layers supported for rasterizing a
  primitive on a layer is obtained from ``PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS``
  even though it can be larger than the number of layers supported by either
  rendering or textures.
* ``PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR``: Implementation uses bounds
  checking on resource accesses by shader if the context is created with
  PIPE_CONTEXT_ROBUST_BUFFER_ACCESS. See the ARB_robust_buffer_access_behavior
  extension for information on the required behavior for out of bounds accesses
  and accesses to unbound resources.
* ``PIPE_CAP_CULL_DISTANCE``: Whether the driver supports the arb_cull_distance
  extension and thus implements proper support for culling planes.
* ``PIPE_CAP_PRIMITIVE_RESTART_FOR_PATCHES``: Whether primitive restart is
  supported for patch primitives.
* ``PIPE_CAP_TGSI_VOTE``: Whether the ``VOTE_*`` ops can be used in shaders.
* ``PIPE_CAP_MAX_WINDOW_RECTANGLES``: The maxium number of window rectangles
  supported in ``set_window_rectangles``.
* ``PIPE_CAP_POLYGON_OFFSET_UNITS_UNSCALED``: If true, the driver implements support
  for ``pipe_rasterizer_state::offset_units_unscaled``.
* ``PIPE_CAP_VIEWPORT_SUBPIXEL_BITS``: Number of bits of subpixel precision for
  floating point viewport bounds.
* ``PIPE_CAP_MIXED_COLOR_DEPTH_BITS``: Whether there is non-fallback
  support for color/depth format combinations that use a different
  number of bits. For the purpose of this cap, Z24 is treated as
  32-bit. If set to off, that means that a B5G6R5 + Z24 or RGBA8 + Z16
  combination will require a driver fallback, and should not be
  advertised in the GLX/EGL config list.
* ``PIPE_CAP_TGSI_ARRAY_COMPONENTS``: If true, the driver interprets the
  UsageMask of input and output declarations and allows declaring arrays
  in overlapping ranges. The components must be a contiguous range, e.g. a
  UsageMask of  xy or yzw is allowed, but xz or yw isn't. Declarations with
  overlapping locations must have matching semantic names and indices, and
  equal interpolation qualifiers.
  Components may overlap, notably when the gaps in an array of dvec3 are
  filled in.
* ``PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS``: Whether interleaved stream
  output mode is able to interleave across buffers. This is required for
  ARB_transform_feedback3.
* ``PIPE_CAP_TGSI_CAN_READ_OUTPUTS``: Whether every TGSI shader stage can read
  from the output file.
* ``PIPE_CAP_GLSL_OPTIMIZE_CONSERVATIVELY``: Tell the GLSL compiler to use
  the minimum amount of optimizations just to be able to do all the linking
  and lowering.
* ``PIPE_CAP_TGSI_FS_FBFETCH``: Whether a fragment shader can use the FBFETCH
  opcode to retrieve the current value in the framebuffer.
* ``PIPE_CAP_TGSI_MUL_ZERO_WINS``: Whether TGSI shaders support the
  ``TGSI_PROPERTY_MUL_ZERO_WINS`` shader property.
* ``PIPE_CAP_DOUBLES``: Whether double precision floating-point operations
  are supported.
* ``PIPE_CAP_INT64``: Whether 64-bit integer operations are supported.
* ``PIPE_CAP_INT64_DIVMOD``: Whether 64-bit integer division/modulo
  operations are supported.
* ``PIPE_CAP_TGSI_TEX_TXF_LZ``: Whether TEX_LZ and TXF_LZ opcodes are
  supported.
* ``PIPE_CAP_TGSI_CLOCK``: Whether the CLOCK opcode is supported.
* ``PIPE_CAP_POLYGON_MODE_FILL_RECTANGLE``: Whether the
  PIPE_POLYGON_MODE_FILL_RECTANGLE mode is supported for
  ``pipe_rasterizer_state::fill_front`` and
  ``pipe_rasterizer_state::fill_back``.
* ``PIPE_CAP_SPARSE_BUFFER_PAGE_SIZE``: The page size of sparse buffers in
  bytes, or 0 if sparse buffers are not supported. The page size must be at
  most 64KB.
* ``PIPE_CAP_TGSI_BALLOT``: Whether the BALLOT and READ_* opcodes as well as
  the SUBGROUP_* semantics are supported.
* ``PIPE_CAP_TGSI_TES_LAYER_VIEWPORT``: Whether ``TGSI_SEMANTIC_LAYER`` and
  ``TGSI_SEMANTIC_VIEWPORT_INDEX`` are supported as tessellation evaluation
  shader outputs.
* ``PIPE_CAP_CAN_BIND_CONST_BUFFER_AS_VERTEX``: Whether a buffer with just
  PIPE_BIND_CONSTANT_BUFFER can be legally passed to set_vertex_buffers.
* ``PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION``: As the name says.
* ``PIPE_CAP_POST_DEPTH_COVERAGE``: whether
  ``TGSI_PROPERTY_FS_POST_DEPTH_COVERAGE`` is supported.
* ``PIPE_CAP_BINDLESS_TEXTURE``: Whether bindless texture operations are
  supported.
* ``PIPE_CAP_NIR_SAMPLERS_AS_DEREF``: Whether NIR tex instructions should
  reference texture and sampler as NIR derefs instead of by indices.
* ``PIPE_CAP_QUERY_SO_OVERFLOW``: Whether the
  ``PIPE_QUERY_SO_OVERFLOW_PREDICATE`` and
  ``PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE`` query types are supported. Note that
  for a driver that does not support multiple output streams (i.e.,
  ``PIPE_CAP_MAX_VERTEX_STREAMS`` is 1), both query types are identical.
* ``PIPE_CAP_MEMOBJ``: Whether operations on memory objects are supported.
* ``PIPE_CAP_LOAD_CONSTBUF``: True if the driver supports TGSI_OPCODE_LOAD use
  with constant buffers.
* ``PIPE_CAP_TGSI_ANY_REG_AS_ADDRESS``: Any TGSI register can be used as
  an address for indirect register indexing.
* ``PIPE_CAP_TILE_RASTER_ORDER``: Whether the driver supports
  GL_MESA_tile_raster_order, using the tile_raster_order_* fields in
  pipe_rasterizer_state.


.. _pipe_capf:

PIPE_CAPF_*
^^^^^^^^^^^^^^^^

The floating-point capabilities are:

* ``PIPE_CAPF_MAX_LINE_WIDTH``: The maximum width of a regular line.
* ``PIPE_CAPF_MAX_LINE_WIDTH_AA``: The maximum width of a smoothed line.
* ``PIPE_CAPF_MAX_POINT_WIDTH``: The maximum width and height of a point.
* ``PIPE_CAPF_MAX_POINT_WIDTH_AA``: The maximum width and height of a smoothed point.
* ``PIPE_CAPF_MAX_TEXTURE_ANISOTROPY``: The maximum level of anisotropy that can be
  applied to anisotropically filtered textures.
* ``PIPE_CAPF_MAX_TEXTURE_LOD_BIAS``: The maximum :term:`LOD` bias that may be applied
  to filtered textures.
* ``PIPE_CAPF_GUARD_BAND_LEFT``,
  ``PIPE_CAPF_GUARD_BAND_TOP``,
  ``PIPE_CAPF_GUARD_BAND_RIGHT``,
  ``PIPE_CAPF_GUARD_BAND_BOTTOM``: TODO


.. _pipe_shader_cap:

PIPE_SHADER_CAP_*
^^^^^^^^^^^^^^^^^

These are per-shader-stage capabitity queries. Different shader stages may
support different features.

* ``PIPE_SHADER_CAP_MAX_INSTRUCTIONS``: The maximum number of instructions.
* ``PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS``: The maximum number of arithmetic instructions.
* ``PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS``: The maximum number of texture instructions.
* ``PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS``: The maximum number of texture indirections.
* ``PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH``: The maximum nested control flow depth.
* ``PIPE_SHADER_CAP_MAX_INPUTS``: The maximum number of input registers.
* ``PIPE_SHADER_CAP_MAX_OUTPUTS``: The maximum number of output registers.
  This is valid for all shaders except the fragment shader.
* ``PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE``: The maximum size per constant buffer in bytes.
* ``PIPE_SHADER_CAP_MAX_CONST_BUFFERS``: Maximum number of constant buffers that can be bound
  to any shader stage using ``set_constant_buffer``. If 0 or 1, the pipe will
  only permit binding one constant buffer per shader.

If a value greater than 0 is returned, the driver can have multiple
constant buffers bound to shader stages. The CONST register file is
accessed with two-dimensional indices, like in the example below.

DCL CONST[0][0..7]       # declare first 8 vectors of constbuf 0
DCL CONST[3][0]          # declare first vector of constbuf 3
MOV OUT[0], CONST[0][3]  # copy vector 3 of constbuf 0

* ``PIPE_SHADER_CAP_MAX_TEMPS``: The maximum number of temporary registers.
* ``PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED``: Whether the continue opcode is supported.
* ``PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR``: Whether indirect addressing
  of the input file is supported.
* ``PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR``: Whether indirect addressing
  of the output file is supported.
* ``PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR``: Whether indirect addressing
  of the temporary file is supported.
* ``PIPE_SHADER_CAP_INDIRECT_CONST_ADDR``: Whether indirect addressing
  of the constant file is supported.
* ``PIPE_SHADER_CAP_SUBROUTINES``: Whether subroutines are supported, i.e.
  BGNSUB, ENDSUB, CAL, and RET, including RET in the main block.
* ``PIPE_SHADER_CAP_INTEGERS``: Whether integer opcodes are supported.
  If unsupported, only float opcodes are supported.
* ``PIPE_SHADER_CAP_INT64_ATOMICS``: Whether int64 atomic opcodes are supported. The device needs to support add, sub, swap, cmpswap, and, or, xor, min, and max.
* ``PIPE_SHADER_CAP_FP16``: Whether half precision floating-point opcodes are supported.
   If unsupported, half precision ops need to be lowered to full precision.
* ``PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS``: The maximum number of texture
  samplers.
* ``PIPE_SHADER_CAP_PREFERRED_IR``: Preferred representation of the
  program.  It should be one of the ``pipe_shader_ir`` enum values.
* ``PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS``: The maximum number of texture
  sampler views. Must not be lower than PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS.
* ``PIPE_SHADER_CAP_TGSI_DROUND_SUPPORTED``: Whether double precision rounding
  is supported. If it is, DTRUNC/DCEIL/DFLR/DROUND opcodes may be used.
* ``PIPE_SHADER_CAP_TGSI_DFRACEXP_DLDEXP_SUPPORTED``: Whether DFRACEXP and
  DLDEXP are supported.
* ``PIPE_SHADER_CAP_TGSI_LDEXP_SUPPORTED``: Whether LDEXP is supported.
* ``PIPE_SHADER_CAP_TGSI_FMA_SUPPORTED``: Whether FMA and DFMA (doubles only)
  are supported.
* ``PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE``: Whether the driver doesn't
  ignore tgsi_declaration_range::Last for shader inputs and outputs.
* ``PIPE_SHADER_CAP_MAX_UNROLL_ITERATIONS_HINT``: This is the maximum number
  of iterations that loops are allowed to have to be unrolled. It is only
  a hint to state trackers. Whether any loops will be unrolled is not
  guaranteed.
* ``PIPE_SHADER_CAP_MAX_SHADER_BUFFERS``: Maximum number of memory buffers
  (also used to implement atomic counters). Having this be non-0 also
  implies support for the ``LOAD``, ``STORE``, and ``ATOM*`` TGSI
  opcodes.
* ``PIPE_SHADER_CAP_SUPPORTED_IRS``: Supported representations of the
  program.  It should be a mask of ``pipe_shader_ir`` bits.
* ``PIPE_SHADER_CAP_MAX_SHADER_IMAGES``: Maximum number of image units.
* ``PIPE_SHADER_CAP_LOWER_IF_THRESHOLD``: IF and ELSE branches with a lower
  cost than this value should be lowered by the state tracker for better
  performance. This is a tunable for the GLSL compiler and the behavior is
  specific to the compiler.
* ``PIPE_SHADER_CAP_TGSI_SKIP_MERGE_REGISTERS``: Whether the merge registers
  TGSI pass is skipped. This might reduce code size and register pressure if
  the underlying driver has a real backend compiler.


.. _pipe_compute_cap:

PIPE_COMPUTE_CAP_*
^^^^^^^^^^^^^^^^^^

Compute-specific capabilities. They can be queried using
pipe_screen::get_compute_param.

* ``PIPE_COMPUTE_CAP_IR_TARGET``: A description of the target of the form
  ``processor-arch-manufacturer-os`` that will be passed on to the compiler.
  This CAP is only relevant for drivers that specify PIPE_SHADER_IR_LLVM
  or PIPE_SHADER_IR_NATIVE for their preferred IR.
  Value type: null-terminated string. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_GRID_DIMENSION``: Number of supported dimensions
  for grid and block coordinates.  Value type: ``uint64_t``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_GRID_SIZE``: Maximum grid size in block
  units.  Value type: ``uint64_t []``.  Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE``: Maximum block size in thread
  units.  Value type: ``uint64_t []``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK``: Maximum number of threads that
  a single block can contain.  Value type: ``uint64_t``. Shader IR type dependent.
  This may be less than the product of the components of MAX_BLOCK_SIZE and is
  usually limited by the number of threads that can be resident simultaneously
  on a compute unit.
* ``PIPE_COMPUTE_CAP_MAX_GLOBAL_SIZE``: Maximum size of the GLOBAL
  resource.  Value type: ``uint64_t``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE``: Maximum size of the LOCAL
  resource.  Value type: ``uint64_t``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_PRIVATE_SIZE``: Maximum size of the PRIVATE
  resource.  Value type: ``uint64_t``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_INPUT_SIZE``: Maximum size of the INPUT
  resource.  Value type: ``uint64_t``. Shader IR type dependent.
* ``PIPE_COMPUTE_CAP_MAX_MEM_ALLOC_SIZE``: Maximum size of a memory object
  allocation in bytes.  Value type: ``uint64_t``.
* ``PIPE_COMPUTE_CAP_MAX_CLOCK_FREQUENCY``: Maximum frequency of the GPU
  clock in MHz. Value type: ``uint32_t``
* ``PIPE_COMPUTE_CAP_MAX_COMPUTE_UNITS``: Maximum number of compute units
  Value type: ``uint32_t``
* ``PIPE_COMPUTE_CAP_IMAGES_SUPPORTED``: Whether images are supported
  non-zero means yes, zero means no. Value type: ``uint32_t``
* ``PIPE_COMPUTE_CAP_SUBGROUP_SIZE``: The size of a basic execution unit in
  threads. Also known as wavefront size, warp size or SIMD width.
* ``PIPE_COMPUTE_CAP_ADDRESS_BITS``: The default compute device address space
  size specified as an unsigned integer value in bits.
* ``PIPE_COMPUTE_CAP_MAX_VARIABLE_THREADS_PER_BLOCK``: Maximum variable number
  of threads that a single block can contain. This is similar to
  PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK, except that the variable size is not
  known a compile-time but at dispatch-time.

.. _pipe_bind:

PIPE_BIND_*
^^^^^^^^^^^

These flags indicate how a resource will be used and are specified at resource
creation time. Resources may be used in different roles
during their lifecycle. Bind flags are cumulative and may be combined to create
a resource which can be used for multiple things.
Depending on the pipe driver's memory management and these bind flags,
resources might be created and handled quite differently.

* ``PIPE_BIND_RENDER_TARGET``: A color buffer or pixel buffer which will be
  rendered to.  Any surface/resource attached to pipe_framebuffer_state::cbufs
  must have this flag set.
* ``PIPE_BIND_DEPTH_STENCIL``: A depth (Z) buffer and/or stencil buffer. Any
  depth/stencil surface/resource attached to pipe_framebuffer_state::zsbuf must
  have this flag set.
* ``PIPE_BIND_BLENDABLE``: Used in conjunction with PIPE_BIND_RENDER_TARGET to
  query whether a device supports blending for a given format.
  If this flag is set, surface creation may fail if blending is not supported
  for the specified format. If it is not set, a driver may choose to ignore
  blending on surfaces with formats that would require emulation.
* ``PIPE_BIND_DISPLAY_TARGET``: A surface that can be presented to screen. Arguments to
  pipe_screen::flush_front_buffer must have this flag set.
* ``PIPE_BIND_SAMPLER_VIEW``: A texture that may be sampled from in a fragment
  or vertex shader.
* ``PIPE_BIND_VERTEX_BUFFER``: A vertex buffer.
* ``PIPE_BIND_INDEX_BUFFER``: An vertex index/element buffer.
* ``PIPE_BIND_CONSTANT_BUFFER``: A buffer of shader constants.
* ``PIPE_BIND_STREAM_OUTPUT``: A stream output buffer.
* ``PIPE_BIND_CUSTOM``:
* ``PIPE_BIND_SCANOUT``: A front color buffer or scanout buffer.
* ``PIPE_BIND_SHARED``: A sharable buffer that can be given to another
  process.
* ``PIPE_BIND_GLOBAL``: A buffer that can be mapped into the global
  address space of a compute program.
* ``PIPE_BIND_SHADER_BUFFER``: A buffer without a format that can be bound
  to a shader and can be used with load, store, and atomic instructions.
* ``PIPE_BIND_SHADER_IMAGE``: A buffer or texture with a format that can be
  bound to a shader and can be used with load, store, and atomic instructions.
* ``PIPE_BIND_COMPUTE_RESOURCE``: A buffer or texture that can be
  bound to the compute program as a shader resource.
* ``PIPE_BIND_COMMAND_ARGS_BUFFER``: A buffer that may be sourced by the
  GPU command processor. It can contain, for example, the arguments to
  indirect draw calls.

.. _pipe_usage:

PIPE_USAGE_*
^^^^^^^^^^^^

The PIPE_USAGE enums are hints about the expected usage pattern of a resource.
Note that drivers must always support read and write CPU access at any time
no matter which hint they got.

* ``PIPE_USAGE_DEFAULT``: Optimized for fast GPU access.
* ``PIPE_USAGE_IMMUTABLE``: Optimized for fast GPU access and the resource is
  not expected to be mapped or changed (even by the GPU) after the first upload.
* ``PIPE_USAGE_DYNAMIC``: Expect frequent write-only CPU access. What is
  uploaded is expected to be used at least several times by the GPU.
* ``PIPE_USAGE_STREAM``: Expect frequent write-only CPU access. What is
  uploaded is expected to be used only once by the GPU.
* ``PIPE_USAGE_STAGING``: Optimized for fast CPU access.


Methods
-------

XXX to-do

get_name
^^^^^^^^

Returns an identifying name for the screen.

The returned string should remain valid and immutable for the lifetime of
pipe_screen.

get_vendor
^^^^^^^^^^

Returns the screen vendor.

The returned string should remain valid and immutable for the lifetime of
pipe_screen.

get_device_vendor
^^^^^^^^^^^^^^^^^

Returns the actual vendor of the device driving the screen
(as opposed to the driver vendor).

The returned string should remain valid and immutable for the lifetime of
pipe_screen.

.. _get_param:

get_param
^^^^^^^^^

Get an integer/boolean screen parameter.

**param** is one of the :ref:`PIPE_CAP` names.

.. _get_paramf:

get_paramf
^^^^^^^^^^

Get a floating-point screen parameter.

**param** is one of the :ref:`PIPE_CAPF` names.

context_create
^^^^^^^^^^^^^^

Create a pipe_context.

**priv** is private data of the caller, which may be put to various
unspecified uses, typically to do with implementing swapbuffers
and/or front-buffer rendering.

is_format_supported
^^^^^^^^^^^^^^^^^^^

Determine if a resource in the given format can be used in a specific manner.

**format** the resource format

**target** one of the PIPE_TEXTURE_x flags

**sample_count** the number of samples. 0 and 1 mean no multisampling,
the maximum allowed legal value is 32.

**bindings** is a bitmask of :ref:`PIPE_BIND` flags.

Returns TRUE if all usages can be satisfied.


can_create_resource
^^^^^^^^^^^^^^^^^^^

Check if a resource can actually be created (but don't actually allocate any
memory).  This is used to implement OpenGL's proxy textures.  Typically, a
driver will simply check if the total size of the given resource is less than
some limit.

For PIPE_TEXTURE_CUBE, the pipe_resource::array_size field should be 6.


.. _resource_create:

resource_create
^^^^^^^^^^^^^^^

Create a new resource from a template.
The following fields of the pipe_resource must be specified in the template:

**target** one of the pipe_texture_target enums.
Note that PIPE_BUFFER and PIPE_TEXTURE_X are not really fundamentally different.
Modern APIs allow using buffers as shader resources.

**format** one of the pipe_format enums.

**width0** the width of the base mip level of the texture or size of the buffer.

**height0** the height of the base mip level of the texture
(1 for 1D or 1D array textures).

**depth0** the depth of the base mip level of the texture
(1 for everything else).

**array_size** the array size for 1D and 2D array textures.
For cube maps this must be 6, for other textures 1.

**last_level** the last mip map level present.

**nr_samples** the nr of msaa samples. 0 (or 1) specifies a resource
which isn't multisampled.

**usage** one of the :ref:`PIPE_USAGE` flags.

**bind** bitmask of the :ref:`PIPE_BIND` flags.

**flags** bitmask of PIPE_RESOURCE_FLAG flags.



resource_changed
^^^^^^^^^^^^^^^^

Mark a resource as changed so derived internal resources will be recreated
on next use.

When importing external images that can't be directly used as texture sampler
source, internal copies may have to be created that the hardware can sample
from. When those resources are reimported, the image data may have changed, and
the previously derived internal resources must be invalidated to avoid sampling
from old copies.



resource_destroy
^^^^^^^^^^^^^^^^

Destroy a resource. A resource is destroyed if it has no more references.



get_timestamp
^^^^^^^^^^^^^

Query a timestamp in nanoseconds. The returned value should match
PIPE_QUERY_TIMESTAMP. This function returns immediately and doesn't
wait for rendering to complete (which cannot be achieved with queries).



get_driver_query_info
^^^^^^^^^^^^^^^^^^^^^

Return a driver-specific query. If the **info** parameter is NULL,
the number of available queries is returned.  Otherwise, the driver
query at the specified **index** is returned in **info**.
The function returns non-zero on success.
The driver-specific query is described with the pipe_driver_query_info
structure.

get_driver_query_group_info
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Return a driver-specific query group. If the **info** parameter is NULL,
the number of available groups is returned.  Otherwise, the driver
query group at the specified **index** is returned in **info**.
The function returns non-zero on success.
The driver-specific query group is described with the
pipe_driver_query_group_info structure.



get_disk_shader_cache
^^^^^^^^^^^^^^^^^^^^^

Returns a pointer to a driver-specific on-disk shader cache. If the driver
failed to create the cache or does not support an on-disk shader cache NULL is
returned. The callback itself may also be NULL if the driver doesn't support
an on-disk shader cache.


Thread safety
-------------

Screen methods are required to be thread safe. While gallium rendering
contexts are not required to be thread safe, it is required to be safe to use
different contexts created with the same screen in different threads without
locks. It is also required to be safe using screen methods in a thread, while
using one of its contexts in another (without locks).
