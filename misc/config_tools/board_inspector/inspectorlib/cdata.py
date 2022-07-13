# Copyright (c) 2015-2022 Intel Corporation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright notice,
#       this list of conditions and the following disclaimer in the documentation
#       and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""bits.cdata module."""

from __future__ import print_function
import binascii
import ctypes
import textwrap
import uuid

def print_fields(cls):
    with ttypager.page():
        print("{}".format(cls.__name__))
        print("{:20s} {:6} {:6}".format('field', 'length', 'offset'))
        for f in cls._fields_:
            a = getattr(cls, f[0])
            print("{:20s} {:6} {:6}".format(f[0], a.size, a.offset))

def to_bytes(var):
    return (ctypes.c_char * ctypes.sizeof(var)).from_buffer(var).raw

_CTYPES_HEX_TYPES = (
    ctypes.c_void_p,
    ctypes.c_uint8, ctypes.c_uint16, ctypes.c_uint32, ctypes.c_uint64,
    ctypes.c_ubyte, ctypes.c_ushort, ctypes.c_uint, ctypes.c_ulong, ctypes.c_ulonglong,
)

class c_base(object):
    """Base class for ctypes structures and unions."""
    @staticmethod
    def _formatval(t, val):
        if val is not None and t in _CTYPES_HEX_TYPES:
            return "{:#x}".format(val)
        if issubclass(t, ctypes.Array):
            if issubclass(t._type_, (ctypes.c_char, ctypes.c_wchar)):
                return "'{}'".format(val)
            else:
                return "[{}]".format(", ".join(Struct._formatval(t._type_, item) for item in val))
        return "{}".format(val)


    def _formatter(self, field):
        name = field[0]
        t = field[1]
        val = getattr(self, name)
        if hasattr(self, '_formats'):
            f = self._formats.get(name, None)
            if f:
                return f(val)
        if issubclass(t, (Struct, Union)):
            val._indent = self._indent
            return str(val)
        if issubclass(t, ctypes.Array):
            if issubclass(t._type_, (Struct, Union)):
                s = "["
                for item in val:
                    item._indent = self._indent + "  "
                    s += "\n" + str(item)
                s += "]"
                return s
        return self._formatval(t, val)

    _indent = ""

    def _wrap(self, str, indent=True):
        line_len = 77 - len(self._indent + '  ')
        _wrapper = textwrap.TextWrapper(width=line_len, initial_indent=self._indent, subsequent_indent=self._indent + '  ')
        _wrapper_indentall = textwrap.TextWrapper(width=line_len, initial_indent=self._indent + '  ', subsequent_indent=self._indent + '  ')
        def __wrap():
            wrapper = _wrapper
            for line in str.split("\n"):
                # Preserve blank lines, for which wrapper emits an empty list
                if not line:
                    yield ""
                for wrapped_line in wrapper.wrap(line):
                    yield wrapped_line
                if indent:
                    wrapper = _wrapper_indentall
        return '\n'.join(__wrap())

    def preface_field(self, field):
        a = getattr(self.__class__, field[0])
        return "ofs={} ".format(a.offset)

    def bitfield_info(self, field):
        a = getattr(self.__class__, field[0])
        bit_count = a.size >> 16
        lo_bit = a.size & 0xFFFF
        hi_bit = lo_bit + bit_count - 1
        return bit_count, hi_bit, lo_bit

    def preface_bitfield(self, field):
        bit_count, hi_bit, lo_bit = self.bitfield_info(field)
        if bit_count > 1:
            return "bits[{}:{}]=".format(hi_bit, lo_bit)
        if bit_count == 1:
            return "bit[{}]=".format(lo_bit)
        return ""

    def __str__(self):
        self._indent += "  "
        s = "{}({})".format(self.__class__.__name__, "".join("\n{}{}={}{}".format(self.preface_field(field), field[0], self.preface_bitfield(field), self._formatter(field)) for field in self._fields_))
        self._indent = ""
        return self._wrap(s)

class Struct(ctypes.Structure, c_base):
    """Base class for ctypes structures."""
    def __hash__(self):
        buf = (ctypes.c_uint8 * ctypes.sizeof(self)).from_buffer(self)
        return binascii.crc32(buf)

    def __cmp__(self, other):
        return cmp(hash(self), hash(other))

class Union(ctypes.Union, c_base):
    """Base class for ctypes unions."""
    def __hash__(self):
        buf = (ctypes.c_uint8 * ctypes.sizeof(self)).from_buffer(self)
        return binascii.crc32(buf)

    def __cmp__(self, other):
        return cmp(hash(self), hash(other))

class GUID(Struct):
    _fields_ = [
        ('Data', ctypes.c_ubyte * 16),
    ]

    def __init__(self, *args, **kwargs):
        """Create a GUID.  Accepts any arguments the uuid.UUID constructor
        would accept.  Also accepts an instance of uuid.UUID, either as the
        first argument or as a keyword argument "uuid".  As with other
        ctypes structures, passing no parameters yields a zero-initialized
        structure."""
        u = kwargs.get("uuid")
        if u is not None:
            self.uuid = u
        elif not(args) and not(kwargs):
            self.uuid = uuid.UUID(int=0)
        elif args and isinstance(args[0], uuid.UUID):
            self.uuid = args[0]
        else:
            self.uuid = uuid.UUID(*args, **kwargs)

    def _get_uuid(self):
        return uuid.UUID(bytes_le=to_bytes(self))

    def _set_uuid(self, u):
        ctypes.memmove(ctypes.addressof(self), ctypes.c_char_p(u.bytes_le), ctypes.sizeof(self))

    uuid = property(_get_uuid, _set_uuid)

    def __cmp__(self, other):
        if isinstance(other, GUID):
            return cmp(self.uuid, other.uuid)
        if isinstance(other, uuid.UUID):
            return cmp(self.uuid, other)
        return NotImplemented

    def __hash__(self):
        return hash(self.uuid)

    def __repr__(self):
        return "GUID({})".format(self.uuid)

    def __str__(self):
        return "{}".format(self.uuid)

def _format_guid(val):
    try:
        import efi
        guid_str = efi.known_uuids.get(val.uuid, None)
    except:
        guid_str = None
    if guid_str:
        return '{} ({})'.format(val, guid_str)
    return '{}'.format(val)
