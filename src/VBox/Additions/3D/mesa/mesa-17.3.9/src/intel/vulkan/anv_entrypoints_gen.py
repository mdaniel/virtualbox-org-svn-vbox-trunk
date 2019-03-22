# coding=utf-8
#
# Copyright © 2015, 2017 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

import argparse
import functools
import os
import xml.etree.cElementTree as et

from mako.template import Template

from anv_extensions import *

# We generate a static hash table for entry point lookup
# (vkGetProcAddress). We use a linear congruential generator for our hash
# function and a power-of-two size table. The prime numbers are determined
# experimentally.

TEMPLATE_H = Template("""\
/* This file generated from ${filename}, don't edit directly. */

struct anv_dispatch_table {
   union {
      void *entrypoints[${len(entrypoints)}];
      struct {
      % for _, name, _, _, _, guard in entrypoints:
        % if guard is not None:
#ifdef ${guard}
          PFN_vk${name} ${name};
#else
          void *${name};
# endif
        % else:
          PFN_vk${name} ${name};
        % endif
      % endfor
      };
   };
};

% for type_, name, args, num, h, guard in entrypoints:
  % if guard is not None:
#ifdef ${guard}
  % endif
  ${type_} anv_${name}(${args});
  ${type_} gen7_${name}(${args});
  ${type_} gen75_${name}(${args});
  ${type_} gen8_${name}(${args});
  ${type_} gen9_${name}(${args});
  ${type_} gen10_${name}(${args});
  % if guard is not None:
#endif // ${guard}
  % endif
% endfor
""", output_encoding='utf-8')

TEMPLATE_C = Template(u"""\
/*
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* This file generated from ${filename}, don't edit directly. */

#include "anv_private.h"

struct anv_entrypoint {
   uint32_t name;
   uint32_t hash;
};

/* We use a big string constant to avoid lots of reloctions from the entry
 * point table to lots of little strings. The entries in the entry point table
 * store the index into this big string.
 */

static const char strings[] =
% for _, name, _, _, _, _ in entrypoints:
    "vk${name}\\0"
% endfor
;

static const struct anv_entrypoint entrypoints[] = {
% for _, name, _, num, h, _ in entrypoints:
    [${num}] = { ${offsets[num]}, ${'{:0=#8x}'.format(h)} }, /* vk${name} */
% endfor
};

/* Weak aliases for all potential implementations. These will resolve to
 * NULL if they're not defined, which lets the resolve_entrypoint() function
 * either pick the correct entry point.
 */

% for layer in ['anv', 'gen7', 'gen75', 'gen8', 'gen9', 'gen10']:
  % for type_, name, args, _, _, guard in entrypoints:
    % if guard is not None:
#ifdef ${guard}
    % endif
    ${type_} ${layer}_${name}(${args}) __attribute__ ((weak));
    % if guard is not None:
#endif // ${guard}
    % endif
  % endfor

  const struct anv_dispatch_table ${layer}_layer = {
  % for _, name, args, _, _, guard in entrypoints:
    % if guard is not None:
#ifdef ${guard}
    % endif
    .${name} = ${layer}_${name},
    % if guard is not None:
#endif // ${guard}
    % endif
  % endfor
  };
% endfor

static void * __attribute__ ((noinline))
anv_resolve_entrypoint(const struct gen_device_info *devinfo, uint32_t index)
{
   if (devinfo == NULL) {
      return anv_layer.entrypoints[index];
   }

   const struct anv_dispatch_table *genX_table;
   switch (devinfo->gen) {
   case 10:
      genX_table = &gen10_layer;
      break;
   case 9:
      genX_table = &gen9_layer;
      break;
   case 8:
      genX_table = &gen8_layer;
      break;
   case 7:
      if (devinfo->is_haswell)
         genX_table = &gen75_layer;
      else
         genX_table = &gen7_layer;
      break;
   default:
      unreachable("unsupported gen\\n");
   }

   if (genX_table->entrypoints[index])
      return genX_table->entrypoints[index];
   else
      return anv_layer.entrypoints[index];
}

/* Hash table stats:
 * size ${hash_size} entries
 * collisions entries:
% for i in xrange(10):
 *     ${i}${'+' if i == 9 else ''}     ${collisions[i]}
% endfor
 */

#define none ${'{:#x}'.format(none)}
static const uint16_t map[] = {
% for i in xrange(0, hash_size, 8):
  % for j in xrange(i, i + 8):
    ## This is 6 because the 0x is counted in the length
    % if mapping[j] & 0xffff == 0xffff:
      none,
    % else:
      ${'{:0=#6x}'.format(mapping[j] & 0xffff)},
    % endif
  % endfor
% endfor
};

void *
anv_lookup_entrypoint(const struct gen_device_info *devinfo, const char *name)
{
   static const uint32_t prime_factor = ${prime_factor};
   static const uint32_t prime_step = ${prime_step};
   const struct anv_entrypoint *e;
   uint32_t hash, h, i;
   const char *p;

   hash = 0;
   for (p = name; *p; p++)
      hash = hash * prime_factor + *p;

   h = hash;
   do {
      i = map[h & ${hash_mask}];
      if (i == none)
         return NULL;
      e = &entrypoints[i];
      h += prime_step;
   } while (e->hash != hash);

   if (strcmp(name, strings + e->name) != 0)
      return NULL;

   return anv_resolve_entrypoint(devinfo, i);
}""", output_encoding='utf-8')

NONE = 0xffff
HASH_SIZE = 256
U32_MASK = 2**32 - 1
HASH_MASK = HASH_SIZE - 1

PRIME_FACTOR = 5024183
PRIME_STEP = 19


def cal_hash(name):
    """Calculate the same hash value that Mesa will calculate in C."""
    return functools.reduce(
        lambda h, c: (h * PRIME_FACTOR + ord(c)) & U32_MASK, name, 0)


def get_entrypoints(doc, entrypoints_to_defines, start_index):
    """Extract the entry points from the registry."""
    entrypoints = []

    enabled_commands = set()
    for feature in doc.findall('./feature'):
        assert feature.attrib['api'] == 'vulkan'
        if VkVersion(feature.attrib['number']) > MAX_API_VERSION:
            continue

        for command in feature.findall('./require/command'):
            enabled_commands.add(command.attrib['name'])

    supported = set(ext.name for ext in EXTENSIONS)
    for extension in doc.findall('.extensions/extension'):
        if extension.attrib['name'] not in supported:
            continue

        if extension.attrib['supported'] != 'vulkan':
            continue

        for command in extension.findall('./require/command'):
            enabled_commands.add(command.attrib['name'])

    index = start_index
    for command in doc.findall('./commands/command'):
        type = command.find('./proto/type').text
        fullname = command.find('./proto/name').text

        if fullname not in enabled_commands:
            continue

        shortname = fullname[2:]
        params = (''.join(p.itertext()) for p in command.findall('./param'))
        params = ', '.join(params)
        guard = entrypoints_to_defines.get(fullname)
        entrypoints.append((type, shortname, params, index, cal_hash(fullname), guard))
        index += 1

    return entrypoints


def get_entrypoints_defines(doc):
    """Maps entry points to extension defines."""
    entrypoints_to_defines = {}

    for extension in doc.findall('./extensions/extension[@protect]'):
        define = extension.attrib['protect']

        for entrypoint in extension.findall('./require/command'):
            fullname = entrypoint.attrib['name']
            entrypoints_to_defines[fullname] = define

    return entrypoints_to_defines


def gen_code(entrypoints):
    """Generate the C code."""
    i = 0
    offsets = []
    for _, name, _, _, _, _ in entrypoints:
        offsets.append(i)
        i += 2 + len(name) + 1

    mapping = [NONE] * HASH_SIZE
    collisions = [0] * 10
    for _, name, _, num, h, _ in entrypoints:
        level = 0
        while mapping[h & HASH_MASK] != NONE:
            h = h + PRIME_STEP
            level = level + 1
        if level > 9:
            collisions[9] += 1
        else:
            collisions[level] += 1
        mapping[h & HASH_MASK] = num

    return TEMPLATE_C.render(entrypoints=entrypoints,
                             offsets=offsets,
                             collisions=collisions,
                             mapping=mapping,
                             hash_mask=HASH_MASK,
                             prime_step=PRIME_STEP,
                             prime_factor=PRIME_FACTOR,
                             none=NONE,
                             hash_size=HASH_SIZE,
                             filename=os.path.basename(__file__))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--outdir', help='Where to write the files.',
                        required=True)
    parser.add_argument('--xml',
                        help='Vulkan API XML file.',
                        required=True,
                        action='append',
                        dest='xml_files')
    args = parser.parse_args()

    entrypoints = []

    for filename in args.xml_files:
        doc = et.parse(filename)
        entrypoints += get_entrypoints(doc, get_entrypoints_defines(doc),
                                       start_index=len(entrypoints))

    # Manually add CreateDmaBufImageINTEL for which we don't have an extension
    # defined.
    entrypoints.append(('VkResult', 'CreateDmaBufImageINTEL',
                        'VkDevice device, ' +
                        'const VkDmaBufImageCreateInfo* pCreateInfo, ' +
                        'const VkAllocationCallbacks* pAllocator,' +
                        'VkDeviceMemory* pMem,' +
                        'VkImage* pImage', len(entrypoints),
                        cal_hash('vkCreateDmaBufImageINTEL'), None))

    # For outputting entrypoints.h we generate a anv_EntryPoint() prototype
    # per entry point.
    with open(os.path.join(args.outdir, 'anv_entrypoints.h'), 'wb') as f:
        f.write(TEMPLATE_H.render(entrypoints=entrypoints,
                                  filename=os.path.basename(__file__)))
    with open(os.path.join(args.outdir, 'anv_entrypoints.c'), 'wb') as f:
        f.write(gen_code(entrypoints))


if __name__ == '__main__':
    main()
