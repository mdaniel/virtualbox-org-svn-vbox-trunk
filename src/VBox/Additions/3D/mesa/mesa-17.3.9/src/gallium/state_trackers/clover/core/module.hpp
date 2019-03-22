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

#ifndef CLOVER_CORE_MODULE_HPP
#define CLOVER_CORE_MODULE_HPP

#include <vector>
#include <string>

namespace clover {
   struct module {
      typedef uint32_t resource_id;
      typedef uint32_t size_t;

      struct section {
         enum type {
            text_intermediate,
            text_library,
            text_executable,
            data_constant,
            data_global,
            data_local,
            data_private
         };

         section(resource_id id, enum type type, size_t size,
                 const std::vector<char> &data) :
                 id(id), type(type), size(size), data(data) { }
         section() : id(0), type(text_intermediate), size(0), data() { }

         resource_id id;
         type type;
         size_t size;
         std::vector<char> data;
      };

      struct argument {
         enum type {
            scalar,
            constant,
            global,
            local,
            image2d_rd,
            image2d_wr,
            image3d_rd,
            image3d_wr,
            sampler
         };

         enum ext_type {
            zero_ext,
            sign_ext
         };

         enum semantic {
            general,
            grid_dimension,
            grid_offset,
            image_size,
            image_format
         };

         argument(enum type type, size_t size,
                  size_t target_size, size_t target_align,
                  enum ext_type ext_type,
                  enum semantic semantic = general) :
            type(type), size(size),
            target_size(target_size), target_align(target_align),
            ext_type(ext_type), semantic(semantic) { }

         argument(enum type type, size_t size) :
            type(type), size(size),
            target_size(size), target_align(1),
            ext_type(zero_ext), semantic(general) { }

         argument() : type(scalar), size(0),
                      target_size(0), target_align(1),
                      ext_type(zero_ext), semantic(general) { }

         type type;
         size_t size;
         size_t target_size;
         size_t target_align;
         ext_type ext_type;
         semantic semantic;
      };

      struct symbol {
         symbol(const std::string &name, resource_id section,
                size_t offset, const std::vector<argument> &args) :
                name(name), section(section), offset(offset), args(args) { }
         symbol() : name(), section(0), offset(0), args() { }

         std::string name;
         resource_id section;
         size_t offset;
         std::vector<argument> args;
      };

      void serialize(std::ostream &os) const;
      static module deserialize(std::istream &is);
      size_t size() const;

      std::vector<symbol> syms;
      std::vector<section> secs;
   };
}

#endif
