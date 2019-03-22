//
// Copyright 2012-2016 Francisco Jerez
// Copyright 2012-2016 Advanced Micro Devices, Inc.
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

#ifndef CLOVER_LLVM_UTIL_HPP
#define CLOVER_LLVM_UTIL_HPP

#include "core/error.hpp"
#include "util/u_debug.h"

#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>

namespace clover {
   namespace llvm {
      template<typename E> void
      fail(std::string &r_log, E &&e, const std::string &s) {
         r_log += s;
         throw e;
      }

      inline std::vector<std::string>
      tokenize(const std::string &s) {
         std::vector<std::string> ss;
         std::ostringstream oss;

         // OpenCL programs can pass a quoted argument, most frequently the
         // include path. This is useful so that path containing spaces is
         // treated as a single argument instead of being split by the spaces.
         // Additionally, the argument should also be unquoted before being
         // passed to the compiler. We avoid using std::string::replace here to
         // remove quotes, as the single and double quote characters can be a
         // part of the file name.
         bool escape_next = false;
         bool in_quote_double = false;
         bool in_quote_single = false;

         for (auto c : s) {
            if (escape_next) {
               oss.put(c);
               escape_next = false;
            } else if (c == '\\') {
               escape_next = true;
            } else if (c == '"' && !in_quote_single) {
               in_quote_double = !in_quote_double;
            } else if (c == '\'' && !in_quote_double) {
               in_quote_single = !in_quote_single;
            } else if (c != ' ' || in_quote_single || in_quote_double) {
               oss.put(c);
            } else if (oss.tellp() > 0) {
               ss.emplace_back(oss.str());
               oss.str("");
            }
         }

         if (oss.tellp() > 0)
            ss.emplace_back(oss.str());

         if (in_quote_double || in_quote_single)
            throw invalid_build_options_error();

         return ss;
      }

      inline std::string
      as_string(const std::vector<char> &v) {
         return { v.begin(), v.end() };
      }

      struct target {
         target(const std::string &s) :
            cpu(s.begin(), s.begin() + s.find_first_of("-")),
            triple(s.begin() + s.find_first_of("-") + 1, s.end()) {}

         std::string cpu;
         std::string triple;
      };

      namespace debug {
         enum flag {
            clc = 1 << 0,
            llvm = 1 << 1,
            native = 1 << 2
         };

         inline bool
         has_flag(flag f) {
            static const struct debug_named_value debug_options[] = {
               { "clc", clc, "Dump the OpenCL C code for all kernels." },
               { "llvm", llvm, "Dump the generated LLVM IR for all kernels." },
               { "native", native, "Dump kernel assembly code for targets "
                 "specifying PIPE_SHADER_IR_NATIVE" },
               DEBUG_NAMED_VALUE_END
            };
            static const unsigned flags =
               debug_get_flags_option("CLOVER_DEBUG", debug_options, 0);

            return flags & f;
         }

         inline void
         log(const std::string &suffix, const std::string &s) {
            const std::string path = debug_get_option("CLOVER_DEBUG_FILE",
                                                      "stderr");
            if (path == "stderr")
               std::cerr << s;
            else
               std::ofstream(path + suffix, std::ios::app) << s;
         }
      }
   }
}

#endif
