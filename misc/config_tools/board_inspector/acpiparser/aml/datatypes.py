# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import mmap
import logging
from math import floor, ceil

from .exception import *

class Object:
    def get(self):
        raise NotImplementedError(self.__class__.__name__)

    def set(self, obj):
        raise NotImplementedError(self.__class__.__name__)

    def to_buffer(self):
        raise NotImplementedError(self.__class__.__name__)

    def to_decimal_string(self):
        raise NotImplementedError(self.__class__.__name__)

    def to_hex_string(self):
        raise NotImplementedError(self.__class__.__name__)

    def to_integer(self):
        raise NotImplementedError(self.__class__.__name__)

    def to_string(self):
        raise NotImplementedError(self.__class__.__name__)

    def get_obj(self):
        return self

class UninitializedObject(Object):
    def to_string(self):
        return "Uninitialized Object"

class Buffer(Object):
    @staticmethod
    def bitmask(to, frm):
        return ((1 << (to + 1)) - 1) - ((1 << frm) - 1)

    def __init__(self, data):
        assert len(data) > 0
        self.__data = bytearray(data)
        self.__fields = {}    # name -> (offset, bitwidth)

    @property
    def data(self):
        return bytes(self.__data)

    def create_field(self, name, offset, bitwidth):
        self.__fields[name] = (offset, bitwidth)

    def read_field(self, name):
        offset, bitwidth = self.__fields[name]
        acc = 0
        acc_bit_count = 0
        bit_idx = offset
        bit_remaining = bitwidth

        assert offset + bitwidth <= len(self.__data) * 8, \
            f"Buffer overflow: attempt to access field {name} at bit {offset + bitwidth} while the buffer has only {len(self.__data) * 8} bits"

        # Bits out of byte boundary
        if bit_idx % 8 > 0:
            byte_idx = floor(bit_idx / 8)
            bit_count = (8 - bit_idx % 8)
            if bit_count > bit_remaining:
                bit_count = bit_remaining

            mask = self.bitmask(bit_idx % 8 + bit_count - 1, bit_idx % 8)
            acc = (self.__data[byte_idx] & mask) >> bit_idx % 8
            acc_bit_count += bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        while bit_remaining > 0:
            byte_idx = floor(bit_idx / 8)
            bit_count = 8 if bit_remaining >= 8 else bit_remaining

            mask = self.bitmask(bit_count - 1, 0)
            acc |= (self.__data[byte_idx] & mask) << acc_bit_count
            acc_bit_count += bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        return acc

    def write_field(self, name, value):
        offset, bitwidth = self.__fields[name]
        bit_idx = offset
        bit_remaining = bitwidth

        assert offset + bitwidth <= len(self.__data) * 8, \
            f"Buffer overflow: attempt to access field {name} at bit {offset + bitwidth} while the buffer has only {len(self.__data) * 8} bits"

        # Bits out of byte boundary
        if bit_idx % 8 > 0:
            byte_idx = floor(bit_idx / 8)
            bit_count = (8 - bit_idx % 8)
            if bit_count > bit_remaining:
                bit_count = bit_remaining

            mask = self.bitmask(bit_idx % 8 + bit_count - 1, bit_idx % 8)
            v = (value & ((1 << bit_count) - 1)) << (bit_idx % 8)
            self.__data[byte_idx] = (v & mask) | (self.__data[byte_idx] & (0xFF - mask))

            value >>= bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        while bit_remaining > 0:
            byte_idx = floor(bit_idx / 8)
            bit_count = 8 if bit_remaining >= 8 else bit_remaining

            mask = self.bitmask(bit_count - 1, 0)
            v = (value & ((1 << bit_count) - 1))
            self.__data[byte_idx] = (v & mask) | (self.__data[byte_idx] & (0xFF - mask))

            value >>= bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

    def get(self):
        return self.__data

    def to_buffer(self):
        return self

    def to_hex_string(self):
        result = ",".join(map(lambda x:hex(x)[2:], self.__data))
        return String(result)

    def to_integer(self):
        acc = 0
        i = min(len(self.__data), 8) - 1
        while i >= 0:
            acc <<= 8
            acc |= self.__data[i]
            i -= 1
        return Integer(acc)

class BufferField(Object):
    def __init__(self, buf, field):
        self.__buf = buf
        self.__field = field

    def get(self):
        return self.__buf.read_field(self.__field)

    def set(self, obj):
        self.__buf.write_field(self.__field, obj.get())

    def to_integer(self):
        return Integer(self.get())

    def to_string(self):
        return "Buffer Field"

# DebugObject

class Device(Object):
    def __init__(self, sym):
        self.__sym = sym

    def get_sym(self):
        return self.__sym

# Event

class FieldUnit(BufferField):
    def to_string(self):
        return "Field"

class Integer(Object):
    def __init__(self, value, width=64):
        self.__value = value
        self.__width = width

    def get(self):
        return self.__value

    def set(self, obj):
        self.__value = obj.get()

    def to_buffer(self):
        assert self.__width % 8 == 0
        data = bytearray()
        i = 0
        v = self.__value
        while i < self.__width:
            data.append(v & 0xff)
            v >>= 8
            i += 8
        return Buffer(data)

    def to_decimal_string(self):
        return String(str(self.__value))

    def to_hex_string(self):
        return String(hex(self.__value)[2:])

    def to_integer(self):
        return self

class Method(Object):
    def __init__(self, tree):
        self.tree = tree
        self.name = tree.children[1].children
        self.body = tree.children[3]

class PredefinedMethod(Object):
    def __init__(self, fn):
        self.fn = fn

# Mutex

class ObjectReference(Object):
    def __init__(self, obj, index=None):
        self.__obj = obj
        self.__index = index

    def get(self):
        if self.__index is not None:
            if isinstance(self.__obj, Package):
                return self.__obj.elements[self.__index]
            else:
                raise NotImplementedError
        else:
            return self.__obj

    def set(self, obj, index=None):
        self.__obj = obj
        self.__index = index

class OperationRegion(Buffer):
    def __load_system_memory_space(self, offset, length):
        offset_page_aligned = (offset >> 12) << 12
        length_page_aligned = ceil(((offset & 0xFFF) + length) / 0x1000) * 0x1000

        with open('/dev/mem', 'rb') as f:
            mm = mmap.mmap(f.fileno(), length_page_aligned, flags=mmap.MAP_PRIVATE, prot=mmap.PROT_READ, offset=offset_page_aligned)
            mm.seek(offset & 0xFFF)
            data = mm.read(length)
            super().__init__(data)

    def __load_pci_configuration_space(self, bus, device, function, offset, length):
        sysfs_path = "/sys/devices/pci0000:%02x/0000:%02x:%02x.%d/config" % (bus, bus, device, function)
        try:
            with open(sysfs_path, "rb") as f:
                f.seek(offset)
                data = f.read(length)
                super().__init__(data)
        except FileNotFoundError:
            logging.error(f"Cannot read the configuration space of %02x:%02x.%d from {sysfs_path}. Assume the PCI device does not exist." % (bus, device, function))
            data = bytearray([0xff]) * length
            super().__init__(data)

    def __init__(self, bus_id, device_id, name, space, offset, length):
        if space == 0x00:          # SystemMemory
            logging.info(f"Loading system memory space {name}: [{hex(offset)}, {hex(offset + length - 1)}]")
            self.__load_system_memory_space(offset, length)
        elif space == 0x01:        # SystemIO
            raise FutureWork("Port I/O operation region")
        elif space == 0x02:        # PCI_Config
            assert offset <= 0xFF and (offset + length) <= 0x100
            # Assume bus is 0 for now
            bus = bus_id
            device = device_id >> 16
            function = device_id & 0xFF
            logging.info(f"Loading PCI configuration space {name}: 00:%02x:%d [{hex(offset)}, {hex(offset + length - 1)}]" % (device, function))
            self.__load_pci_configuration_space(bus, device, function, offset, length)
        else:
            raise NotImplementedError(f"Cannot load operation region in space {space}")

    def to_string(self):
        return "Operation Region"

class Package(Object):
    def __init__(self, elements):
        self.__elements = elements

    @property
    def elements(self):
        return self.__elements

    def to_string(self):
        return "Package"

# PowerResource

# Processor

class RawDataBuffer(Object):
    def __init__(self, data):
        self.__data = data

    def get(self):
        return self.__data

class String(Object):
    def __init__(self, s):
        self.__s = s

    def get(self):
        return self.__s

    def set(self, obj):
        self.__s = obj.get()

    def to_decimal_string(self):
        return self

    def to_hex_string(self):
        return self

    def to_integer(self):
        return Integer(int(self.__s, base=16))

    def to_string(self):
        return self

# ThermalZone
