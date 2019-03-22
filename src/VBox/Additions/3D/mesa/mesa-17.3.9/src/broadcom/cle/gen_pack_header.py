#encoding=utf-8

# Copyright (C) 2016 Intel Corporation
# Copyright (C) 2016 Broadcom
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

from __future__ import (
    absolute_import, division, print_function, unicode_literals
)
import xml.parsers.expat
import re
import sys
import copy

license =  """/* Generated code, see packets.xml and gen_packet_header.py */
"""

pack_header = """%(license)s

/* Packets, enums and structures for %(platform)s.
 *
 * This file has been generated, do not hand edit.
 */

#ifndef %(guard)s
#define %(guard)s

#include "v3d_packet_helpers.h"

"""

def to_alphanum(name):
    substitutions = {
        ' ': '_',
        '/': '_',
        '[': '',
        ']': '',
        '(': '',
        ')': '',
        '-': '_',
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
            self.name = safe_name(attrs["name"]).lower()

        if str(attrs["start"]).endswith("b"):
            self.start = int(attrs["start"][:-1]) * 8
        else:
            self.start = int(attrs["start"])
        # packet <field> entries in XML start from the bit after the
        # opcode, so shift everything up by 8 since we'll also have a
        # Field for the opcode.
        if not parser.struct:
            self.start += 8

        self.end = self.start + int(attrs["size"]) - 1
        self.type = attrs["type"]

        if "prefix" in attrs:
            self.prefix = safe_name(attrs["prefix"]).upper()
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

        print("   %-36s %s%s;" % (type, self.name, dim))

        if len(self.values) > 0 and self.default == None:
            if self.prefix:
                prefix = self.prefix + "_"
            else:
                prefix = ""

        for value in self.values:
            print("#define %-40s %d" % ((prefix + value.name).replace("__", "_"),
                                        value.value))

    def overlaps(self, field):
        return self != field and max(self.start, field.start) <= min(self.end, field.end)


class Group(object):
    def __init__(self, parser, parent, start, count):
        self.parser = parser
        self.parent = parent
        self.start = start
        self.count = count
        self.size = 0
        self.fields = []

    def emit_template_struct(self, dim):
        if self.count == 0:
            print("   /* variable length fields follow */")
        else:
            if self.count > 1:
                dim = "%s[%d]" % (dim, self.count)

            for field in self.fields:
                field.emit_template_struct(dim)

    class Byte:
        def __init__(self):
            self.size = 8
            self.fields = []
            self.address = None

    def collect_bytes(self, bytes):
        for field in self.fields:
            first_byte = field.start // 8
            last_byte = field.end // 8

            for b in xrange(first_byte, last_byte + 1):
                if not b in bytes:
                    bytes[b] = self.Byte()

                bytes[b].fields.append(field)

                if field.type == "address":
                    # assert bytes[index].address == None
                    bytes[b].address = field

    def emit_pack_function(self, start):
        # Determine number of bytes in this group.
        self.length = max(field.end // 8 for field in self.fields) + 1

        bytes = {}
        self.collect_bytes(bytes)

        relocs_emitted = set()
        memcpy_fields = set()

        for index in range(self.length):
            # Handle MBZ bytes
            if not index in bytes:
                print("   cl[%2d] = 0;" % index)
                continue
            byte = bytes[index]

            # Call out to the driver to note our relocations.  Inside of the
            # packet we only store offsets within the BOs, and we store the
            # handle to the packet outside.  Unlike Intel genxml, we don't
            # need to have the other bits that will be stored together with
            # the address during the reloc process, so there's no need for the
            # complicated combine_address() function.
            if byte.address and byte.address not in relocs_emitted:
                print("   __gen_emit_reloc(data, &values->%s);" % byte.address.name)
                relocs_emitted.add(byte.address)

            # Special case: floats can't have any other fields packed into
            # them (since they'd change the meaning of the float), and the
            # per-byte bitshifting math below bloats the pack code for floats,
            # so just copy them directly here.  Also handle 16/32-bit
            # uints/ints with no merged fields.
            if len(byte.fields) == 1:
                field = byte.fields[0]
                if field.type in ["float", "uint", "int"] and field.start % 8 == 0 and field.end - field.start == 31:
                    if field in memcpy_fields:
                        continue

                    if not any(field.overlaps(scan_field) for scan_field in self.fields):
                        assert(field.start == index * 8)
                        print("")
                        print("   memcpy(&cl[%d], &values->%s, sizeof(values->%s));" %
                                (index, field.name, field.name))
                        memcpy_fields.add(field)
                        continue

            byte_start = index * 8

            v = None
            prefix = "   cl[%2d] =" % index

            field_index = 0
            for field in byte.fields:
                if field.type != "mbo":
                    name = field.name

                start = field.start
                end = field.end
                field_byte_start = (field.start // 8) * 8
                start -= field_byte_start
                end -= field_byte_start

                if field.type == "mbo":
                    s = "__gen_mbo(%d, %d)" % \
                        (start, end)
                elif field.type == "address":
                    s = "__gen_address_offset(&values->%s)" % byte.address.name
                elif field.type == "uint":
                    s = "__gen_uint(values->%s, %d, %d)" % \
                        (name, start, end)
                elif field.type in self.parser.enums:
                    s = "__gen_uint(values->%s, %d, %d)" % \
                        (name, start, end)
                elif field.type == "int":
                    s = "__gen_sint(values->%s, %d, %d)" % \
                        (name, start, end)
                elif field.type == "bool":
                    s = "__gen_uint(values->%s, %d, %d)" % \
                        (name, start, end)
                elif field.type == "float":
                    s = "#error %s float value mixed in with other fields" % name
                elif field.type == "offset":
                    s = "__gen_offset(values->%s, %d, %d)" % \
                        (name, start, end)
                elif field.type == 'ufixed':
                    s = "__gen_ufixed(values->%s, %d, %d, %d)" % \
                        (name, start, end, field.fractional_size)
                elif field.type == 'sfixed':
                    s = "__gen_sfixed(values->%s, %d, %d, %d)" % \
                        (name, start, end, field.fractional_size)
                elif field.type in self.parser.structs:
                    s = "__gen_uint(v%d_%d, %d, %d)" % \
                        (index, field_index, start, end)
                    field_index = field_index + 1
                else:
                    print("/* unhandled field %s, type %s */\n" % (name, field.type))
                    s = None

                if not s == None:
                    if byte_start - field_byte_start != 0:
                        s = "%s >> %d" % (s, byte_start - field_byte_start)

                    if field == byte.fields[-1]:
                        print("%s %s;" % (prefix, s))
                    else:
                        print("%s %s |" % (prefix, s))
                    prefix = "           "

            print("")
            continue

    def emit_unpack_function(self, start):
        for field in self.fields:
            if field.type != "mbo":
                convert = None

                args = []
                args.append('cl')
                args.append(str(start + field.start))
                args.append(str(start + field.end))

                if field.type == "address":
                    convert = "__gen_unpack_address"
                elif field.type == "uint":
                    convert = "__gen_unpack_uint"
                elif field.type in self.parser.enums:
                    convert = "__gen_unpack_uint"
                elif field.type == "int":
                    convert = "__gen_unpack_sint"
                elif field.type == "bool":
                    convert = "__gen_unpack_uint"
                elif field.type == "float":
                    convert = "__gen_unpack_float"
                elif field.type == "offset":
                    convert = "__gen_unpack_offset"
                elif field.type == 'ufixed':
                    args.append(str(field.fractional_size))
                    convert = "__gen_unpack_ufixed"
                elif field.type == 'sfixed':
                    args.append(str(field.fractional_size))
                    convert = "__gen_unpack_sfixed"
                else:
                    print("/* unhandled field %s, type %s */\n" % (name, field.type))
                    s = None

                print("   values->%s = %s(%s);" % \
                      (field.name, convert, ', '.join(args)))

class Value(object):
    def __init__(self, attrs):
        self.name = safe_name(attrs["name"]).upper()
        self.value = int(attrs["value"])

class Parser(object):
    def __init__(self):
        self.parser = xml.parsers.expat.ParserCreate()
        self.parser.StartElementHandler = self.start_element
        self.parser.EndElementHandler = self.end_element

        self.packet = None
        self.struct = None
        self.structs = {}
        # Set of enum names we've seen.
        self.enums = set()
        self.registers = {}

    def gen_prefix(self, name):
        if name[0] == "_":
            return 'V3D%s%s' % (self.ver, name)
        else:
            return 'V3D%s_%s' % (self.ver, name)

    def gen_guard(self):
        return self.gen_prefix("PACK_H")

    def start_element(self, name, attrs):
        if name == "vcxml":
            self.platform = "V3D {}".format(attrs["gen"])
            self.ver = attrs["gen"].replace('.', '')
            print(pack_header % {'license': license, 'platform': self.platform, 'guard': self.gen_guard()})
        elif name in ("packet", "struct", "register"):
            default_field = None

            object_name = self.gen_prefix(safe_name(attrs["name"].upper()))
            if name == "packet":
                self.packet = object_name

                # Add a fixed Field for the opcode.  We only make <field>s in
                # the XML for the fields listed in the spec, and all of those
                # start from bit 0 after of the opcode.
                default_field = {
                    "name" : "opcode",
                    "default" : attrs["code"],
                    "type" : "uint",
                    "start" : -8,
                    "size" : 8,
                }
            elif name == "struct":
                self.struct = object_name
                self.structs[attrs["name"]] = 1
            elif name == "register":
                self.register = object_name
                self.reg_num = num_from_str(attrs["num"])
                self.registers[attrs["name"]] = 1

            self.group = Group(self, None, 0, 1)
            if default_field:
                field = Field(self, default_field)
                field.values = []
                self.group.fields.append(field)

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
        if name  == "packet":
            self.emit_packet()
            self.packet = None
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
        elif name  == "field":
            self.group.fields[-1].values = self.values
        elif name  == "enum":
            self.emit_enum()
            self.enum = None
        elif name == "vcxml":
            print('#endif /* %s */' % self.gen_guard())

    def emit_template_struct(self, name, group):
        print("struct %s {" % name)
        group.emit_template_struct("")
        print("};\n")

    def emit_pack_function(self, name, group):
        print("static inline void\n%s_pack(__gen_user_data *data, uint8_t * restrict cl,\n%sconst struct %s * restrict values)\n{" %
              (name, ' ' * (len(name) + 6), name))

        group.emit_pack_function(0)

        print("}\n")

        print('#define %-33s %6d' %
              (name + "_length", self.group.length))

    def emit_unpack_function(self, name, group):
        print("#ifdef __gen_unpack_address")
        print("static inline void")
        print("%s_unpack(const uint8_t * restrict cl,\n%sstruct %s * restrict values)\n{" %
              (name, ' ' * (len(name) + 8), name))

        group.emit_unpack_function(0)

        print("}\n#endif\n")

    def emit_header(self, name):
        default_fields = []
        for field in self.group.fields:
            if not type(field) is Field:
                continue
            if field.default == None:
                continue
            default_fields.append("   .%-35s = %6d" % (field.name, field.default))

        print('#define %-40s\\' % (name + '_header'))
        print(",  \\\n".join(default_fields))
        print('')

    def emit_packet(self):
        name = self.packet

        assert(self.group.fields[0].name == "opcode")
        print('#define %-33s %6d' %
              (name + "_opcode", self.group.fields[0].default))

        self.emit_header(name)
        self.emit_template_struct(self.packet, self.group)
        self.emit_pack_function(self.packet, self.group)
        self.emit_unpack_function(self.packet, self.group)

        print('')

    def emit_register(self):
        name = self.register
        if not self.reg_num == None:
            print('#define %-33s 0x%04x' %
                  (self.gen_prefix(name + "_num"), self.reg_num))

        self.emit_template_struct(self.register, self.group)
        self.emit_pack_function(self.register, self.group)
        self.emit_unpack_function(self.register, self.group)

    def emit_struct(self):
        name = self.struct

        self.emit_header(name)
        self.emit_template_struct(self.struct, self.group)
        self.emit_pack_function(self.struct, self.group)
        self.emit_unpack_function(self.struct, self.group)

        print('')

    def emit_enum(self):
        print('enum %s {' % self.gen_prefix(self.enum))
        for value in self.values:
            if self.prefix:
                name = self.prefix + "_" + value.name
            else:
                name = value.name
            print('        % -36s = %6d,' % (name.upper(), value.value))
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
