# coding=utf-8
COPYRIGHT = """\
/*
 * Copyright 2020 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
"""

import argparse
import os

from mako.template import Template

# Mesa-local imports must be declared in meson variable
# '{file_without_suffix}_depend_files'.
from vk_entrypoints import get_entrypoints_from_xml

TEMPLATE_H = Template(COPYRIGHT + """\
/* This file generated from ${filename}, don't edit directly. */

#ifndef VK_DISPATCH_TRAMPOLINES_H
#define VK_DISPATCH_TRAMPOLINES_H

#include "vulkan/util/vk_dispatch_table.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct vk_physical_device_dispatch_table vk_physical_device_trampolines;
extern struct vk_device_dispatch_table vk_device_trampolines;

#ifdef __cplusplus
}
#endif

#endif /* VK_DISPATCH_TRAMPOLINES_H */
""")

TEMPLATE_C = Template(COPYRIGHT + """\
/* This file generated from ${filename}, don't edit directly. */

#include "vk_device.h"
#include "vk_dispatch_trampolines.h"
#include "vk_object.h"
#include "vk_physical_device.h"

% for e in entrypoints:
  % if not e.is_physical_device_entrypoint() or e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
static VKAPI_ATTR ${e.return_type} VKAPI_CALL
${e.prefixed_name('vk_tramp')}(${e.decl_params()})
{
    <% assert e.params[0].type == 'VkPhysicalDevice' %>
    VK_FROM_HANDLE(vk_physical_device, vk_physical_device, ${e.params[0].name});
  % if e.return_type == 'void':
    vk_physical_device->dispatch_table.${e.name}(${e.call_params()});
  % else:
    return vk_physical_device->dispatch_table.${e.name}(${e.call_params()});
  % endif
}
  % if e.guard is not None:
#endif
  % endif
% endfor

struct vk_physical_device_dispatch_table vk_physical_device_trampolines = {
% for e in entrypoints:
  % if not e.is_physical_device_entrypoint() or e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
    .${e.name} = ${e.prefixed_name('vk_tramp')},
  % if e.guard is not None:
#endif
  % endif
% endfor
};

% for e in entrypoints:
  % if not e.is_device_entrypoint() or e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
static VKAPI_ATTR ${e.return_type} VKAPI_CALL
${e.prefixed_name('vk_tramp')}(${e.decl_params()})
{
  % if e.params[0].type == 'VkDevice':
    VK_FROM_HANDLE(vk_device, vk_device, ${e.params[0].name});
    % if e.return_type == 'void':
    vk_device->dispatch_table.${e.name}(${e.call_params()});
    % else:
    return vk_device->dispatch_table.${e.name}(${e.call_params()});
    % endif
  % elif e.params[0].type in ('VkCommandBuffer', 'VkQueue'):
    struct vk_object_base *vk_object = (struct vk_object_base *)${e.params[0].name};
    % if e.return_type == 'void':
    vk_object->device->dispatch_table.${e.name}(${e.call_params()});
    % else:
    return vk_object->device->dispatch_table.${e.name}(${e.call_params()});
    % endif
  % else:
    assert(!"Unhandled device child trampoline case: ${e.params[0].type}");
  % endif
}
  % if e.guard is not None:
#endif
  % endif
% endfor

struct vk_device_dispatch_table vk_device_trampolines = {
% for e in entrypoints:
  % if not e.is_device_entrypoint() or e.alias:
    <% continue %>
  % endif
  % if e.guard is not None:
#ifdef ${e.guard}
  % endif
    .${e.name} = ${e.prefixed_name('vk_tramp')},
  % if e.guard is not None:
#endif
  % endif
% endfor
};
""")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out-c', help='Output C file.')
    parser.add_argument('--out-h', help='Output H file.')
    parser.add_argument('--beta', required=True, help='Enable beta extensions.')
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True,
                        action='append',
                        dest='xml_files')
    args = parser.parse_args()

    entrypoints = get_entrypoints_from_xml(args.xml_files, args.beta)

    # For outputting entrypoints.h we generate a anv_EntryPoint() prototype
    # per entry point.
    try:
        if args.out_h:
            with open(args.out_h, 'w', encoding='utf-8') as f:
                f.write(TEMPLATE_H.render(entrypoints=entrypoints,
                                          filename=os.path.basename(__file__)))
        if args.out_c:
            with open(args.out_c, 'w', encoding='utf-8') as f:
                f.write(TEMPLATE_C.render(entrypoints=entrypoints,
                                          filename=os.path.basename(__file__)))
    except Exception:
        # In the event there's an error, this imports some helpers from mako
        # to print a useful stack trace and prints it, then exits with
        # status 1, if python is run with debug; otherwise it just raises
        # the exception
        if __debug__:
            import sys
            from mako import exceptions
            sys.stderr.write(exceptions.text_error_template().render() + '\n')
            sys.exit(1)
        raise


if __name__ == '__main__':
    main()
