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

#include "api/util.hpp"
#include "core/program.hpp"
#include "util/u_debug.h"

#include <sstream>

using namespace clover;

namespace {
   void
   validate_build_common(const program &prog, cl_uint num_devs,
                         const cl_device_id *d_devs,
                         void (*pfn_notify)(cl_program, void *),
                         void *user_data) {
      if (!pfn_notify && user_data)
         throw error(CL_INVALID_VALUE);

      if (prog.kernel_ref_count())
         throw error(CL_INVALID_OPERATION);

      if (any_of([&](const device &dev) {
               return !count(dev, prog.context().devices());
            }, objs<allow_empty_tag>(d_devs, num_devs)))
         throw error(CL_INVALID_DEVICE);
   }
}

CLOVER_API cl_program
clCreateProgramWithSource(cl_context d_ctx, cl_uint count,
                          const char **strings, const size_t *lengths,
                          cl_int *r_errcode) try {
   auto &ctx = obj(d_ctx);
   std::string source;

   if (!count || !strings ||
       any_of(is_zero(), range(strings, count)))
      throw error(CL_INVALID_VALUE);

   // Concatenate all the provided fragments together
   for (unsigned i = 0; i < count; ++i)
         source += (lengths && lengths[i] ?
                    std::string(strings[i], strings[i] + lengths[i]) :
                    std::string(strings[i]));

   // ...and create a program object for them.
   ret_error(r_errcode, CL_SUCCESS);
   return new program(ctx, source);

} catch (error &e) {
   ret_error(r_errcode, e);
   return NULL;
}

CLOVER_API cl_program
clCreateProgramWithBinary(cl_context d_ctx, cl_uint n,
                          const cl_device_id *d_devs,
                          const size_t *lengths,
                          const unsigned char **binaries,
                          cl_int *r_status, cl_int *r_errcode) try {
   auto &ctx = obj(d_ctx);
   auto devs = objs(d_devs, n);

   if (!lengths || !binaries)
      throw error(CL_INVALID_VALUE);

   if (any_of([&](const device &dev) {
            return !count(dev, ctx.devices());
         }, devs))
      throw error(CL_INVALID_DEVICE);

   // Deserialize the provided binaries,
   std::vector<std::pair<cl_int, module>> result = map(
      [](const unsigned char *p, size_t l) -> std::pair<cl_int, module> {
         if (!p || !l)
            return { CL_INVALID_VALUE, {} };

         try {
            std::stringbuf bin( { (char*)p, l } );
            std::istream s(&bin);

            return { CL_SUCCESS, module::deserialize(s) };

         } catch (std::istream::failure &e) {
            return { CL_INVALID_BINARY, {} };
         }
      },
      range(binaries, n),
      range(lengths, n));

   // update the status array,
   if (r_status)
      copy(map(keys(), result), r_status);

   if (any_of(key_equals(CL_INVALID_VALUE), result))
      throw error(CL_INVALID_VALUE);

   if (any_of(key_equals(CL_INVALID_BINARY), result))
      throw error(CL_INVALID_BINARY);

   // initialize a program object with them.
   ret_error(r_errcode, CL_SUCCESS);
   return new program(ctx, devs, map(values(), result));

} catch (error &e) {
   ret_error(r_errcode, e);
   return NULL;
}

CLOVER_API cl_program
clCreateProgramWithBuiltInKernels(cl_context d_ctx, cl_uint n,
                                  const cl_device_id *d_devs,
                                  const char *kernel_names,
                                  cl_int *r_errcode) try {
   auto &ctx = obj(d_ctx);
   auto devs = objs(d_devs, n);

   if (any_of([&](const device &dev) {
            return !count(dev, ctx.devices());
         }, devs))
      throw error(CL_INVALID_DEVICE);

   // No currently supported built-in kernels.
   throw error(CL_INVALID_VALUE);

} catch (error &e) {
   ret_error(r_errcode, e);
   return NULL;
}


CLOVER_API cl_int
clRetainProgram(cl_program d_prog) try {
   obj(d_prog).retain();
   return CL_SUCCESS;

} catch (error &e) {
   return e.get();
}

CLOVER_API cl_int
clReleaseProgram(cl_program d_prog) try {
   if (obj(d_prog).release())
      delete pobj(d_prog);

   return CL_SUCCESS;

} catch (error &e) {
   return e.get();
}

CLOVER_API cl_int
clBuildProgram(cl_program d_prog, cl_uint num_devs,
               const cl_device_id *d_devs, const char *p_opts,
               void (*pfn_notify)(cl_program, void *),
               void *user_data) try {
   auto &prog = obj(d_prog);
   auto devs = (d_devs ? objs(d_devs, num_devs) :
                ref_vector<device>(prog.context().devices()));
   const auto opts = std::string(p_opts ? p_opts : "") + " " +
                     debug_get_option("CLOVER_EXTRA_BUILD_OPTIONS", "");

   validate_build_common(prog, num_devs, d_devs, pfn_notify, user_data);

   if (prog.has_source) {
      prog.compile(devs, opts);
      prog.link(devs, opts, { prog });
   }

   return CL_SUCCESS;

} catch (error &e) {
   return e.get();
}

CLOVER_API cl_int
clCompileProgram(cl_program d_prog, cl_uint num_devs,
                 const cl_device_id *d_devs, const char *p_opts,
                 cl_uint num_headers, const cl_program *d_header_progs,
                 const char **header_names,
                 void (*pfn_notify)(cl_program, void *),
                 void *user_data) try {
   auto &prog = obj(d_prog);
   auto devs = (d_devs ? objs(d_devs, num_devs) :
                ref_vector<device>(prog.context().devices()));
   const auto opts = std::string(p_opts ? p_opts : "") + " " +
                     debug_get_option("CLOVER_EXTRA_COMPILE_OPTIONS", "");
   header_map headers;

   validate_build_common(prog, num_devs, d_devs, pfn_notify, user_data);

   if (bool(num_headers) != bool(header_names))
      throw error(CL_INVALID_VALUE);

   if (!prog.has_source)
      throw error(CL_INVALID_OPERATION);

   for_each([&](const char *name, const program &header) {
         if (!header.has_source)
            throw error(CL_INVALID_OPERATION);

         if (!any_of(key_equals(name), headers))
            headers.push_back(std::pair<std::string, std::string>(
                                 name, header.source()));
      },
      range(header_names, num_headers),
      objs<allow_empty_tag>(d_header_progs, num_headers));

   prog.compile(devs, opts, headers);
   return CL_SUCCESS;

} catch (invalid_build_options_error &e) {
   return CL_INVALID_COMPILER_OPTIONS;

} catch (build_error &e) {
   return CL_COMPILE_PROGRAM_FAILURE;

} catch (error &e) {
   return e.get();
}

namespace {
   ref_vector<device>
   validate_link_devices(const ref_vector<program> &progs,
                         const ref_vector<device> &all_devs) {
      std::vector<device *> devs;

      for (auto &dev : all_devs) {
         const auto has_binary = [&](const program &prog) {
            const auto t = prog.build(dev).binary_type();
            return t == CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT ||
                   t == CL_PROGRAM_BINARY_TYPE_LIBRARY;
         };

         // According to the CL 1.2 spec, when "all programs specified [..]
         // contain a compiled binary or library for the device [..] a link is
         // performed",
         if (all_of(has_binary, progs))
            devs.push_back(&dev);

         // otherwise if "none of the programs contain a compiled binary or
         // library for that device [..] no link is performed.  All other
         // cases will return a CL_INVALID_OPERATION error."
         else if (any_of(has_binary, progs))
            throw error(CL_INVALID_OPERATION);
      }

      return map(derefs(), devs);
   }
}

CLOVER_API cl_program
clLinkProgram(cl_context d_ctx, cl_uint num_devs, const cl_device_id *d_devs,
              const char *p_opts, cl_uint num_progs, const cl_program *d_progs,
              void (*pfn_notify) (cl_program, void *), void *user_data,
              cl_int *r_errcode) try {
   auto &ctx = obj(d_ctx);
   const auto opts = std::string(p_opts ? p_opts : "") + " " +
                     debug_get_option("CLOVER_EXTRA_LINK_OPTIONS", "");
   auto progs = objs(d_progs, num_progs);
   auto prog = create<program>(ctx);
   auto devs = validate_link_devices(progs,
                                     (d_devs ? objs(d_devs, num_devs) :
                                      ref_vector<device>(ctx.devices())));

   validate_build_common(prog, num_devs, d_devs, pfn_notify, user_data);

   try {
      prog().link(devs, opts, progs);
      ret_error(r_errcode, CL_SUCCESS);

   } catch (build_error &e) {
      ret_error(r_errcode, CL_LINK_PROGRAM_FAILURE);
   }

   return ret_object(prog);

} catch (invalid_build_options_error &e) {
   ret_error(r_errcode, CL_INVALID_LINKER_OPTIONS);
   return NULL;

} catch (error &e) {
   ret_error(r_errcode, e);
   return NULL;
}

CLOVER_API cl_int
clUnloadCompiler() {
   return CL_SUCCESS;
}

CLOVER_API cl_int
clUnloadPlatformCompiler(cl_platform_id d_platform) {
   return CL_SUCCESS;
}

CLOVER_API cl_int
clGetProgramInfo(cl_program d_prog, cl_program_info param,
                 size_t size, void *r_buf, size_t *r_size) try {
   property_buffer buf { r_buf, size, r_size };
   auto &prog = obj(d_prog);

   switch (param) {
   case CL_PROGRAM_REFERENCE_COUNT:
      buf.as_scalar<cl_uint>() = prog.ref_count();
      break;

   case CL_PROGRAM_CONTEXT:
      buf.as_scalar<cl_context>() = desc(prog.context());
      break;

   case CL_PROGRAM_NUM_DEVICES:
      buf.as_scalar<cl_uint>() = (prog.devices().size() ?
                                  prog.devices().size() :
                                  prog.context().devices().size());
      break;

   case CL_PROGRAM_DEVICES:
      buf.as_vector<cl_device_id>() = (prog.devices().size() ?
                                       descs(prog.devices()) :
                                       descs(prog.context().devices()));
      break;

   case CL_PROGRAM_SOURCE:
      buf.as_string() = prog.source();
      break;

   case CL_PROGRAM_BINARY_SIZES:
      buf.as_vector<size_t>() = map([&](const device &dev) {
            return prog.build(dev).binary.size();
         },
         prog.devices());
      break;

   case CL_PROGRAM_BINARIES:
      buf.as_matrix<unsigned char>() = map([&](const device &dev) {
            std::stringbuf bin;
            std::ostream s(&bin);
            prog.build(dev).binary.serialize(s);
            return bin.str();
         },
         prog.devices());
      break;

   case CL_PROGRAM_NUM_KERNELS:
      buf.as_scalar<cl_uint>() = prog.symbols().size();
      break;

   case CL_PROGRAM_KERNEL_NAMES:
      buf.as_string() = fold([](const std::string &a, const module::symbol &s) {
            return ((a.empty() ? "" : a + ";") + s.name);
         }, std::string(), prog.symbols());
      break;

   default:
      throw error(CL_INVALID_VALUE);
   }

   return CL_SUCCESS;

} catch (error &e) {
   return e.get();
}

CLOVER_API cl_int
clGetProgramBuildInfo(cl_program d_prog, cl_device_id d_dev,
                      cl_program_build_info param,
                      size_t size, void *r_buf, size_t *r_size) try {
   property_buffer buf { r_buf, size, r_size };
   auto &prog = obj(d_prog);
   auto &dev = obj(d_dev);

   if (!count(dev, prog.context().devices()))
      return CL_INVALID_DEVICE;

   switch (param) {
   case CL_PROGRAM_BUILD_STATUS:
      buf.as_scalar<cl_build_status>() = prog.build(dev).status();
      break;

   case CL_PROGRAM_BUILD_OPTIONS:
      buf.as_string() = prog.build(dev).opts;
      break;

   case CL_PROGRAM_BUILD_LOG:
      buf.as_string() = prog.build(dev).log;
      break;

   case CL_PROGRAM_BINARY_TYPE:
      buf.as_scalar<cl_program_binary_type>() = prog.build(dev).binary_type();
      break;

   default:
      throw error(CL_INVALID_VALUE);
   }

   return CL_SUCCESS;

} catch (error &e) {
   return e.get();
}
