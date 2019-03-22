#encoding=utf-8

from __future__ import (
    absolute_import, division, print_function, unicode_literals
)
import ast
import xml.parsers.expat
import re
import sys
import copy
import textwrap

license =  """/*
 * Copyright (C) 2016 Intel Corporation
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
"""

pack_header = """%(license)s

/* Instructions, enums and structures for %(platform)s.
 *
 * This file has been generated, do not hand edit.
 */

#ifndef %(guard)s
#define %(guard)s

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#ifndef __gen_validate_value
#define __gen_validate_value(x)
#endif

#ifndef __gen_field_functions
#define __gen_field_functions

union __gen_value {
   float f;
   uint32_t dw;
};

static inline uint64_t
__gen_mbo(uint32_t start, uint32_t end)
{
   return (~0ull >> (64 - (end - start + 1))) << start;
}

static inline uint64_t
__gen_uint(uint64_t v, uint32_t start, uint32_t end)
{
   __gen_validate_value(v);

#if DEBUG
   const int width = end - start + 1;
   if (width < 64) {
      const uint64_t max = (1ull << width) - 1;
      assert(v <= max);
   }
#endif

   return v << start;
}

static inline uint64_t
__gen_sint(int64_t v, uint32_t start, uint32_t end)
{
   const int width = end - start + 1;

   __gen_validate_value(v);

#if DEBUG
   if (width < 64) {
      const int64_t max = (1ll << (width - 1)) - 1;
      const int64_t min = -(1ll << (width - 1));
      assert(min <= v && v <= max);
   }
#endif

   const uint64_t mask = ~0ull >> (64 - width);

   return (v & mask) << start;
}

static inline uint64_t
__gen_offset(uint64_t v, uint32_t start, uint32_t end)
{
   __gen_validate_value(v);
#if DEBUG
   uint64_t mask = (~0ull >> (64 - (end - start + 1))) << start;

   assert((v & ~mask) == 0);
#endif

   return v;
}

static inline uint32_t
__gen_float(float v)
{
   __gen_validate_value(v);
   return ((union __gen_value) { .f = (v) }).dw;
}

static inline uint64_t
__gen_sfixed(float v, uint32_t start, uint32_t end, uint32_t fract_bits)
{
   __gen_validate_value(v);

   const float factor = (1 << fract_bits);

#if DEBUG
   const float max = ((1 << (end - start)) - 1) / factor;
   const float min = -(1 << (end - start)) / factor;
   assert(min <= v && v <= max);
#endif

   const int64_t int_val = llroundf(v * factor);
   const uint64_t mask = ~0ull >> (64 - (end - start + 1));

   return (int_val & mask) << start;
}

static inline uint64_t
__gen_ufixed(float v, uint32_t start, uint32_t end, uint32_t fract_bits)
{
   __gen_validate_value(v);

   const float factor = (1 << fract_bits);

#if DEBUG
   const float max = ((1 << (end - start + 1)) - 1) / factor;
   const float min = 0.0f;
   assert(min <= v && v <= max);
#endif

   const uint64_t uint_val = llroundf(v * factor);

   return uint_val << start;
}

#ifndef __gen_address_type
#error #define __gen_address_type before including this file
#endif

#ifndef __gen_user_data
#error #define __gen_combine_address before including this file
#endif

#endif

"""

def to_alphanum(name):
    substitutions = {
        ' ': '',
        '/': '',
        '[': '',
        ']': '',
        '(': '',
        ')': '',
        '-': '',
        ':': '',
        '.': '',
        ',': '',
        '=': '',
        '>': '',
        '#': '',
        'α': 'alpha',
        '&': '',
        '*': '',
        '"': '',
        '+': '',
        '\'': '',
    }

    for i, j in substitutions.items():
        name = name.replace(i, j)

    return name

def safe_name(name):
    name = to_alphanum(name)
    if not name[0].isalpha():
        name = '_' + name

    return name

def num_from_str(num_str):
    if num_str.lower().startswith('0x'):
        return int(num_str, base=16)
    else:
        assert(not num_str.startswith('0') and 'octals numbers not allowed')
        return int(num_str)

class Field(object):
    ufixed_pattern = re.compile(r"u(\d+)\.(\d+)")
    sfixed_pattern = re.compile(r"s(\d+)\.(\d+)")

    def __init__(self, parser, attrs):
        self.parser = parser
        if "name" in attrs:
            self.name = safe_name(attrs["name"])
        self.start = int(attrs["start"])
        self.end = int(attrs["end"])
        self.type = attrs["type"]

        if "prefix" in attrs:
            self.prefix = attrs["prefix"]
        else:
            self.prefix = None

        if "default" in attrs:
            self.default = int(attrs["default"])
        else:
            self.default = None

        ufixed_match = Field.ufixed_pattern.match(self.type)
        if ufixed_match:
            self.type = 'ufixed'
            self.fractional_size = int(ufixed_match.group(2))

        sfixed_match = Field.sfixed_pattern.match(self.type)
        if sfixed_match:
            self.type = 'sfixed'
            self.fractional_size = int(sfixed_match.group(2))

    def emit_template_struct(self, dim):
        if self.type == 'address':
            type = '__gen_address_type'
        elif self.type == 'bool':
            type = 'bool'
        elif self.type == 'float':
            type = 'float'
        elif self.type == 'ufixed':
            type = 'float'
        elif self.type == 'sfixed':
            type = 'float'
        elif self.type == 'uint' and self.end - self.start > 32:
            type = 'uint64_t'
        elif self.type == 'offset':
            type = 'uint64_t'
        elif self.type == 'int':
            type = 'int32_t'
        elif self.type == 'uint':
            type = 'uint32_t'
        elif self.type in self.parser.structs:
            type = 'struct ' + self.parser.gen_prefix(safe_name(self.type))
        elif self.type in self.parser.enums:
            type = 'enum ' + self.parser.gen_prefix(safe_name(self.type))
        elif self.type == 'mbo':
            return
        else:
            print("#error unhandled type: %s" % self.type)
            return

        print("   %-36s %s%s;" % (type, self.name, dim))

        prefix = ""
        if len(self.values) > 0 and self.default == None:
            if self.prefix:
                prefix = self.prefix + "_"

        for value in self.values:
            print("#define %-40s %d" % (prefix + value.name, value.value))

class Group(object):
    def __init__(self, parser, parent, start, count, size):
        self.parser = parser
        self.parent = parent
        self.start = start
        self.count = count
        self.size = size
        self.fields = []

    def emit_template_struct(self, dim):
        if self.count == 0:
            print("   /* variable length fields follow */")
        else:
            if self.count > 1:
                dim = "%s[%d]" % (dim, self.count)

            for field in self.fields:
                field.emit_template_struct(dim)

    class DWord:
        def __init__(self):
            self.size = 32
            self.fields = []
            self.address = None

    def collect_dwords(self, dwords, start, dim):
        for field in self.fields:
            if type(field) is Group:
                if field.count == 1:
                    field.collect_dwords(dwords, start + field.start, dim)
                else:
                    for i in range(field.count):
                        field.collect_dwords(dwords,
                                             start + field.start + i * field.size,
                                             "%s[%d]" % (dim, i))
                continue

            index = (start + field.start) // 32
            if not index in dwords:
                dwords[index] = self.DWord()

            clone = copy.copy(field)
            clone.start = clone.start + start
            clone.end = clone.end + start
            clone.dim = dim
            dwords[index].fields.append(clone)

            if field.type == "address":
                # assert dwords[index].address == None
                dwords[index].address = field

            # Coalesce all the dwords covered by this field. The two cases we
            # handle are where multiple fields are in a 64 bit word (typically
            # and address and a few bits) or where a single struct field
            # completely covers multiple dwords.
            while index < (start + field.end) // 32:
                if index + 1 in dwords and not dwords[index] == dwords[index + 1]:
                    dwords[index].fields.extend(dwords[index + 1].fields)
                dwords[index].size = 64
                dwords[index + 1] = dwords[index]
                index = index + 1

    def collect_dwords_and_length(self):
        dwords = {}
        self.collect_dwords(dwords, 0, "")

        # Determine number of dwords in this group. If we have a size, use
        # that, since that'll account for MBZ dwords at the end of a group
        # (like dword 8 on BDW+ 3DSTATE_HS). Otherwise, use the largest dword
        # index we've seen plus one.
        if self.size > 0:
            length = self.size // 32
        elif dwords:
            length = max(dwords.keys()) + 1
        else:
            length = 0

        return (dwords, length)

    def emit_pack_function(self, dwords, length):
        for index in range(length):
            # Handle MBZ dwords
            if not index in dwords:
                print("")
                print("   dw[%d] = 0;" % index)
                continue

            # For 64 bit dwords, we aliased the two dword entries in the dword
            # dict it occupies. Now that we're emitting the pack function,
            # skip the duplicate entries.
            dw = dwords[index]
            if index > 0 and index - 1 in dwords and dw == dwords[index - 1]:
                continue

            # Special case: only one field and it's a struct at the beginning
            # of the dword. In this case we pack directly into the
            # destination. This is the only way we handle embedded structs
            # larger than 32 bits.
            if len(dw.fields) == 1:
                field = dw.fields[0]
                name = field.name + field.dim
                if field.type in self.parser.structs and field.start % 32 == 0:
                    print("")
                    print("   %s_pack(data, &dw[%d], &values->%s);" %
                          (self.parser.gen_prefix(safe_name(field.type)), index, name))
                    continue

            # Pack any fields of struct type first so we have integer values
            # to the dword for those fields.
            field_index = 0
            for field in dw.fields:
                if type(field) is Field and field.type in self.parser.structs:
                    name = field.name + field.dim
                    print("")
                    print("   uint32_t v%d_%d;" % (index, field_index))
                    print("   %s_pack(data, &v%d_%d, &values->%s);" %
                          (self.parser.gen_prefix(safe_name(field.type)), index, field_index, name))
                    field_index = field_index + 1

            print("")
            dword_start = index * 32
            if dw.address == None:
                address_count = 0
            else:
                address_count = 1

            if dw.size == 32 and dw.address == None:
                v = None
                print("   dw[%d] =" % index)
            elif len(dw.fields) > address_count:
                v = "v%d" % index
                print("   const uint%d_t %s =" % (dw.size, v))
            else:
                v = "0"

            field_index = 0
            non_address_fields = []
            for field in dw.fields:
                if field.type != "mbo":
                    name = field.name + field.dim

                if field.type == "mbo":
                    non_address_fields.append("__gen_mbo(%d, %d)" % \
                        (field.start - dword_start, field.end - dword_start))
                elif field.type == "address":
                    pass
                elif field.type == "uint":
                    non_address_fields.append("__gen_uint(values->%s, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start))
                elif field.type in self.parser.enums:
                    non_address_fields.append("__gen_uint(values->%s, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start))
                elif field.type == "int":
                    non_address_fields.append("__gen_sint(values->%s, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start))
                elif field.type == "bool":
                    non_address_fields.append("__gen_uint(values->%s, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start))
                elif field.type == "float":
                    non_address_fields.append("__gen_float(values->%s)" % name)
                elif field.type == "offset":
                    non_address_fields.append("__gen_offset(values->%s, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start))
                elif field.type == 'ufixed':
                    non_address_fields.append("__gen_ufixed(values->%s, %d, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start, field.fractional_size))
                elif field.type == 'sfixed':
                    non_address_fields.append("__gen_sfixed(values->%s, %d, %d, %d)" % \
                        (name, field.start - dword_start, field.end - dword_start, field.fractional_size))
                elif field.type in self.parser.structs:
                    non_address_fields.append("__gen_uint(v%d_%d, %d, %d)" % \
                        (index, field_index, field.start - dword_start, field.end - dword_start))
                    field_index = field_index + 1
                else:
                    non_address_fields.append("/* unhandled field %s, type %s */\n" % \
                                              (name, field.type))

            if len(non_address_fields) > 0:
                print(" |\n".join("      " + f for f in non_address_fields) + ";")

            if dw.size == 32:
                if dw.address:
                    print("   dw[%d] = __gen_combine_address(data, &dw[%d], values->%s, %s);" % (index, index, dw.address.name + field.dim, v))
                continue

            if dw.address:
                v_address = "v%d_address" % index
                print("   const uint64_t %s =\n      __gen_combine_address(data, &dw[%d], values->%s, %s);" %
                      (v_address, index, dw.address.name + field.dim, v))
                v = v_address

            print("   dw[%d] = %s;" % (index, v))
            print("   dw[%d] = %s >> 32;" % (index + 1, v))

class Value(object):
    def __init__(self, attrs):
        self.name = safe_name(attrs["name"])
        self.value = ast.literal_eval(attrs["value"])

class Parser(object):
    def __init__(self):
        self.parser = xml.parsers.expat.ParserCreate()
        self.parser.StartElementHandler = self.start_element
        self.parser.EndElementHandler = self.end_element

        self.instruction = None
        self.structs = {}
        # Set of enum names we've seen.
        self.enums = set()
        self.registers = {}

    def gen_prefix(self, name):
        if name[0] == "_":
            return 'GEN%s%s' % (self.gen, name)
        else:
            return 'GEN%s_%s' % (self.gen, name)

    def gen_guard(self):
        return self.gen_prefix("PACK_H")

    def start_element(self, name, attrs):
        if name == "genxml":
            self.platform = attrs["name"]
            self.gen = attrs["gen"].replace('.', '')
            print(pack_header % {'license': license, 'platform': self.platform, 'guard': self.gen_guard()})
        elif name in ("instruction", "struct", "register"):
            if name == "instruction":
                self.instruction = safe_name(attrs["name"])
                self.length_bias = int(attrs["bias"])
            elif name == "struct":
                self.struct = safe_name(attrs["name"])
                self.structs[attrs["name"]] = 1
            elif name == "register":
                self.register = safe_name(attrs["name"])
                self.reg_num = num_from_str(attrs["num"])
                self.registers[attrs["name"]] = 1
            if "length" in attrs:
                self.length = int(attrs["length"])
                size = self.length * 32
            else:
                self.length = None
                size = 0
            self.group = Group(self, None, 0, 1, size)

        elif name == "group":
            group = Group(self, self.group,
                          int(attrs["start"]), int(attrs["count"]), int(attrs["size"]))
            self.group.fields.append(group)
            self.group = group
        elif name == "field":
            self.group.fields.append(Field(self, attrs))
            self.values = []
        elif name == "enum":
            self.values = []
            self.enum = safe_name(attrs["name"])
            self.enums.add(attrs["name"])
            if "prefix" in attrs:
                self.prefix = safe_name(attrs["prefix"])
            else:
                self.prefix= None
        elif name == "value":
            self.values.append(Value(attrs))

    def end_element(self, name):
        if name  == "instruction":
            self.emit_instruction()
            self.instruction = None
            self.group = None
        elif name == "struct":
            self.emit_struct()
            self.struct = None
            self.group = None
        elif name == "register":
            self.emit_register()
            self.register = None
            self.reg_num = None
            self.group = None
        elif name == "group":
            self.group = self.group.parent
        elif name  == "field":
            self.group.fields[-1].values = self.values
        elif name  == "enum":
            self.emit_enum()
            self.enum = None
        elif name == "genxml":
            print('#endif /* %s */' % self.gen_guard())

    def emit_template_struct(self, name, group):
        print("struct %s {" % self.gen_prefix(name))
        group.emit_template_struct("")
        print("};\n")

    def emit_pack_function(self, name, group):
        name = self.gen_prefix(name)
        print(textwrap.dedent("""\
            static inline void
            %s_pack(__attribute__((unused)) __gen_user_data *data,
                  %s__attribute__((unused)) void * restrict dst,
                  %s__attribute__((unused)) const struct %s * restrict values)
            {""") % (name, ' ' * len(name), ' ' * len(name), name))

        (dwords, length) = group.collect_dwords_and_length()
        if length:
            # Cast dst to make header C++ friendly
            print("   uint32_t * restrict dw = (uint32_t * restrict) dst;")

            group.emit_pack_function(dwords, length)

        print("}\n")

    def emit_instruction(self):
        name = self.instruction
        if not self.length == None:
            print('#define %-33s %6d' %
                  (self.gen_prefix(name + "_length"), self.length))
        print('#define %-33s %6d' %
              (self.gen_prefix(name + "_length_bias"), self.length_bias))

        default_fields = []
        for field in self.group.fields:
            if not type(field) is Field:
                continue
            if field.default == None:
                continue
            default_fields.append("   .%-35s = %6d" % (field.name, field.default))

        if default_fields:
            print('#define %-40s\\' % (self.gen_prefix(name + '_header')))
            print(",  \\\n".join(default_fields))
            print('')

        self.emit_template_struct(self.instruction, self.group)

        self.emit_pack_function(self.instruction, self.group)

    def emit_register(self):
        name = self.register
        if not self.reg_num == None:
            print('#define %-33s 0x%04x' %
                  (self.gen_prefix(name + "_num"), self.reg_num))

        if not self.length == None:
            print('#define %-33s %6d' %
                  (self.gen_prefix(name + "_length"), self.length))

        self.emit_template_struct(self.register, self.group)
        self.emit_pack_function(self.register, self.group)

    def emit_struct(self):
        name = self.struct
        if not self.length == None:
            print('#define %-33s %6d' %
                  (self.gen_prefix(name + "_length"), self.length))

        self.emit_template_struct(self.struct, self.group)
        self.emit_pack_function(self.struct, self.group)

    def emit_enum(self):
        print('enum %s {' % self.gen_prefix(self.enum))
        for value in self.values:
            if self.prefix:
                name = self.prefix + "_" + value.name
            else:
                name = value.name
            print('   %-36s = %6d,' % (name.upper(), value.value))
        print('};\n')

    def parse(self, filename):
        file = open(filename, "rb")
        self.parser.ParseFile(file)
        file.close()

if len(sys.argv) < 2:
    print("No input xml file specified")
    sys.exit(1)

input_file = sys.argv[1]

p = Parser()
p.parse(input_file)
