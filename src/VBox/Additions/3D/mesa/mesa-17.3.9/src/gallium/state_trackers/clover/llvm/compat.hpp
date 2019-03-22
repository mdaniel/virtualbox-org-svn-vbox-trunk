//
// Copyright 2016 Francisco Jerez
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

///
/// \file
/// Some thin wrappers around the Clang/LLVM API used to preserve
/// compatibility with older API versions while keeping the ifdef clutter low
/// in the rest of the clover::llvm subtree.  In case of an API break please
/// consider whether it's possible to preserve backwards compatibility by
/// introducing a new one-liner inline function or typedef here under the
/// compat namespace in order to keep the running code free from preprocessor
/// conditionals.
///

#ifndef CLOVER_LLVM_COMPAT_HPP
#define CLOVER_LLVM_COMPAT_HPP

#include "util/algorithm.hpp"

#include <llvm/IR/LLVMContext.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Target/TargetMachine.h>
#if HAVE_LLVM >= 0x0400
#include <llvm/Support/Error.h>
#else
#include <llvm/Support/ErrorOr.h>
#endif

#if HAVE_LLVM >= 0x0307
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#else
#include <llvm/PassManager.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/Support/FormattedStream.h>
#endif

#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CodeGenOptions.h>
#include <clang/Frontend/CompilerInstance.h>

namespace clover {
   namespace llvm {
      namespace compat {
#if HAVE_LLVM >= 0x0307
         typedef ::llvm::TargetLibraryInfoImpl target_library_info;
#else
         typedef ::llvm::TargetLibraryInfo target_library_info;
#endif

         template<typename T, typename AS>
         unsigned target_address_space(const T &target, const AS lang_as) {
            const auto &map = target.getAddressSpaceMap();
#if HAVE_LLVM >= 0x0500
            return map[static_cast<unsigned>(lang_as)];
#else
            return map[lang_as - clang::LangAS::Offset];
#endif
         }

#if HAVE_LLVM >= 0x0500
         const clang::InputKind ik_opencl = clang::InputKind::OpenCL;
#else
         const clang::InputKind ik_opencl = clang::IK_OpenCL;
#endif

         inline void
         set_lang_defaults(clang::CompilerInvocation &inv,
                           clang::LangOptions &lopts, clang::InputKind ik,
                           const ::llvm::Triple &t,
                           clang::PreprocessorOptions &ppopts,
                           clang::LangStandard::Kind std) {
#if HAVE_LLVM >= 0x0309
            inv.setLangDefaults(lopts, ik, t, ppopts, std);
#else
            inv.setLangDefaults(lopts, ik, std);
#endif
         }

         inline void
         add_link_bitcode_file(clang::CodeGenOptions &opts,
                               const std::string &path) {
#if HAVE_LLVM >= 0x0500
            clang::CodeGenOptions::BitcodeFileToLink F;

            F.Filename = path;
            F.PropagateAttrs = true;
            F.LinkFlags = ::llvm::Linker::Flags::None;
            opts.LinkBitcodeFiles.emplace_back(F);
#elif HAVE_LLVM >= 0x0308
            opts.LinkBitcodeFiles.emplace_back(::llvm::Linker::Flags::None, path);
#else
            opts.LinkBitcodeFile = path;
#endif
         }

#if HAVE_LLVM >= 0x0307
         typedef ::llvm::legacy::PassManager pass_manager;
#else
         typedef ::llvm::PassManager pass_manager;
#endif

         inline void
         add_data_layout_pass(pass_manager &pm) {
#if HAVE_LLVM < 0x0307
            pm.add(new ::llvm::DataLayoutPass());
#endif
         }

         inline void
         add_internalize_pass(pass_manager &pm,
                              const std::vector<std::string> &names) {
#if HAVE_LLVM >= 0x0309
            pm.add(::llvm::createInternalizePass(
                      [=](const ::llvm::GlobalValue &gv) {
                         return std::find(names.begin(), names.end(),
                                          gv.getName()) != names.end();
                      }));
#else
            pm.add(::llvm::createInternalizePass(std::vector<const char *>(
                      map(std::mem_fn(&std::string::data), names))));
#endif
         }

         inline std::unique_ptr< ::llvm::Linker>
         create_linker(::llvm::Module &mod) {
#if HAVE_LLVM >= 0x0308
            return std::unique_ptr< ::llvm::Linker>(new ::llvm::Linker(mod));
#else
            return std::unique_ptr< ::llvm::Linker>(new ::llvm::Linker(&mod));
#endif
         }

         inline bool
         link_in_module(::llvm::Linker &linker,
                        std::unique_ptr< ::llvm::Module> mod) {
#if HAVE_LLVM >= 0x0308
            return linker.linkInModule(std::move(mod));
#else
            return linker.linkInModule(mod.get());
#endif
         }

#if HAVE_LLVM >= 0x0307
         typedef ::llvm::raw_svector_ostream &raw_ostream_to_emit_file;
#else
         typedef ::llvm::formatted_raw_ostream raw_ostream_to_emit_file;
#endif

#if HAVE_LLVM >= 0x0307
         typedef ::llvm::DataLayout data_layout;
#else
         typedef const ::llvm::DataLayout *data_layout;
#endif

         inline data_layout
         get_data_layout(::llvm::TargetMachine &tm) {
#if HAVE_LLVM >= 0x0307
            return tm.createDataLayout();
#else
            return tm.getSubtargetImpl()->getDataLayout();
#endif
         }

#if HAVE_LLVM >= 0x0600
         const auto default_code_model = ::llvm::None;
#else
         const auto default_code_model = ::llvm::CodeModel::Default;
#endif

#if HAVE_LLVM >= 0x0309
         const auto default_reloc_model = ::llvm::None;
#else
         const auto default_reloc_model = ::llvm::Reloc::Default;
#endif

         template<typename M, typename F> void
         handle_module_error(M &mod, const F &f) {
#if HAVE_LLVM >= 0x0400
            if (::llvm::Error err = mod.takeError())
               ::llvm::handleAllErrors(std::move(err), [&](::llvm::ErrorInfoBase &eib) {
                     f(eib.message());
                  });
#else
            if (!mod)
               f(mod.getError().message());
#endif
         }

        template<typename T> void
        set_diagnostic_handler(::llvm::LLVMContext &ctx,
                               T *diagnostic_handler, void *data) {
#if HAVE_LLVM >= 0x0600
           ctx.setDiagnosticHandlerCallBack(diagnostic_handler, data);
#else
           ctx.setDiagnosticHandler(diagnostic_handler, data);
#endif
        }
      }
   }
}

#endif
