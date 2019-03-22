//
// Copyright 2012 Francisco Jerez
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef CLOVER_CORE_DEVICE_HPP
#define CLOVER_CORE_DEVICE_HPP

#include <set>
#include <vector>

#include "core/object.hpp"
#include "core/format.hpp"
#include "pipe-loader/pipe_loader.h"

namespace clover {
   class platform;
   class root_resource;
   class hard_event;

   class device : public ref_counter, public _cl_device_id {
   public:
      device(clover::platform &platform, pipe_loader_device *ldev);
      ~device();

      device(const device &dev) = delete;
      device &
      operator=(const device &dev) = delete;

      bool
      operator==(const device &dev) const;

      cl_device_type type() const;
      cl_uint vendor_id() const;
      size_t max_images_read() const;
      size_t max_images_write() const;
      size_t max_image_buffer_size() const;
      cl_uint max_image_levels_2d() const;
      cl_uint max_image_levels_3d() const;
      size_t max_image_array_number() const;
      cl_uint max_samplers() const;
      cl_ulong max_mem_global() const;
      cl_ulong max_mem_local() const;
      cl_ulong max_mem_input() const;
      cl_ulong max_const_buffer_size() const;
      cl_uint max_const_buffers() const;
      size_t max_threads_per_block() const;
      cl_ulong max_mem_alloc_size() const;
      cl_uint max_clock_frequency() const;
      cl_uint max_compute_units() const;
      bool image_support() const;
      bool has_doubles() const;
      bool has_halves() const;
      bool has_int64_atomics() const;
      bool has_unified_memory() const;
      cl_uint mem_base_addr_align() const;

      std::vector<size_t> max_block_size() const;
      cl_uint subgroup_size() const;
      cl_uint address_bits() const;
      std::string device_name() const;
      std::string vendor_name() const;
      std::string device_version() const;
      std::string device_clc_version() const;
      enum pipe_shader_ir ir_format() const;
      std::string ir_target() const;
      enum pipe_endian endianness() const;

      friend class command_queue;
      friend class root_resource;
      friend class hard_event;
      friend std::set<cl_image_format>
      supported_formats(const context &, cl_mem_object_type);

      clover::platform &platform;

   private:
      pipe_screen *pipe;
      pipe_loader_device *ldev;
   };
}

#endif
