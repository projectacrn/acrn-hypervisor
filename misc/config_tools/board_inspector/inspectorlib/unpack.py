# Copyright (c) 2013-2022 Intel Corporation.
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

"""unpack module."""

from collections import OrderedDict
import struct

class UnpackError(Exception):
    pass

class Unpackable(object):
    def __init__(self, data, offset=0, size=None):
        self.data = data
        data_size = len(data)
        if offset > data_size:
            raise UnpackError("Unpackable.__init__: offset={} but len(data)={}".format(offset, data_size))
        self.offset = offset
        if size is None:
            self.size = data_size
        else:
            self.size = offset + size
            if self.size > data_size:
                raise UnpackError("Unpackable.__init__: offset+size={} but len(data)={}".format(self.size, data_size))

    def _check_unpack(self, size):
        if self.offset + size > self.size:
            raise UnpackError("Unpackable: Attempted to unpack {} bytes, but only {} bytes remaining".format(size, self.size - self.offset))

    def skip(self, size):
        self._check_unpack(size)
        self.offset += size

    def unpack(self, fmt):
        try:
            l = struct.calcsize(fmt)
            self._check_unpack(l)
            value = struct.unpack_from(fmt, self.data, self.offset)
            self.offset += l
            return value
        except struct.error as e:
            raise UnpackError("Unpackable.unpack: " + str(e))

    def unpack_one(self, fmt):
        return self.unpack(fmt)[0]

    def unpack_peek(self, fmt):
        try:
            l = struct.calcsize(fmt)
            self._check_unpack(l)
            return struct.unpack_from(fmt, self.data, self.offset)
        except struct.error as e:
            raise UnpackError("Unpackable.unpack_peek: " + str(e))

    def unpack_peek_one(self, fmt):
        return self.unpack_peek(fmt)[0]

    def unpack_peek_raw(self, size):
        """Peek at the specified number of bytes as a str"""
        self._check_unpack(size)
        return self.data[self.offset:self.offset+size]

    def unpack_peek_rest(self):
        """Peek at the remainder of the unpackable as a str"""
        return self.data[self.offset:self.size]

    def unpack_raw(self, size):
        """Unpack the specified number of bytes as a str"""
        self._check_unpack(size)
        old_offset = self.offset
        self.offset += size
        return self.data[old_offset:self.offset]

    def unpack_rest(self):
        """Return the remainder of the unpackable as a str"""
        offset = self.offset
        self.offset = self.size
        return self.data[offset:self.size]

    def unpack_unpackable(self, size):
        """Unpack the specified number of bytes as an Unpackable"""
        u = Unpackable(self.data, self.offset, size)
        self.offset += size
        return u

    def at_end(self):
        return self.offset == self.size

class StructError(Exception):
    pass

class Struct(object):
    def __init__(self):
        self.fields = OrderedDict()

    @classmethod
    def unpack(cls, u):
        s = cls()
        for field in cls._unpack(u):
            s.add_field(*field)
        return s

    def add_field(self, name, value, fmt=None):
        if hasattr(self, name):
            raise StructError("Internal error: Duplicate Struct field name {}".format(name))
        if fmt is None:
            if isinstance(value, int) and not isinstance(value, bool):
                fmt = "{:#x}".format
            else:
                fmt = "{!r}".format
        elif isinstance(fmt, str):
            fmt = fmt.format
        elif not callable(fmt):
            raise StructError("Internal error: Expected a format string or callable, but got: {}".format(fmt))
        setattr(self, name, value)
        self.fields[name] = fmt

    def format_field(self, name):
        return self.fields[name](getattr(self, name))

    def __repr__(self):
        return "{}({})".format(self.__class__.__name__, ", ".join("{}={}".format(k, self.format_field(k)) for k in self.fields.keys()))

    def __iter__(self):
        return (getattr(self, k) for k in self.fields.keys())

    def __eq__(self, other):
        if type(self) is not type(other):
            return NotImplemented
        return self.fields.keys() == other.fields.keys() and all(getattr(self, name) == getattr(other, name) for name in self.fields.keys())

    def __ne__(self, other):
        return not self == other

    def __hash__(self):
        return hash(tuple((name, getattr(self, name)) for name in self.fields.keys()))

def format_each(fmt_one):
    def f(it):
        return "({})".format(", ".join(fmt_one.format(i) for i in it))
    return f

format_each_hex = format_each("{:#x}")

def format_table(fmt, table, default='Reserved'):
    def f(value):
        return "{} ({})".format(fmt.format(value), table.get(value, default))
    return f

def format_function(fmt, function):
    def f(value):
        return "{} ({})".format(fmt.format(value), function(value))
    return f

def reserved_None(fmt="{!r}"):
    def f(value):
        if value is None:
            return "Reserved"
        return fmt.format(value)
    return f

def unpack_all(u, structs, *args):
    """Keep constructing structs from the unpackable u until it runs out of data.

    structs should consist of a list of Struct subclasses to be tried in order.
    Each of them should return None from their constructor if they're not the
    correct type to unpack the next chunk of data.  Any catch-all generic
    structure should apepar last in the list.  Raises a StructError if no
    struct matches."""
    def _substructs():
        while not u.at_end():
            for s in structs:
                temp = s(u, *args)
                if temp is not None:
                    yield temp
                    break
            else:
                raise StructError("Internal error: unable to unpack any structure at byte {} of unpackable".format(u.offset))
    return tuple(_substructs())
