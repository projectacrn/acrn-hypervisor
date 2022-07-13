# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
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

class BufferBase(Object):
    @staticmethod
    def bitmask(to, frm):
        return ((1 << (to + 1)) - 1) - ((1 << frm) - 1)

    def __init__(self, length):
        self.__length = length
        self.__fields = {}    # name -> (offset, bitwidth, access_width)

    def read(self, byte_idx, bit_width):
        return NotImplementedError(self.__class__.__name__)

    def write(self, byte_idx, value, bit_width):
        return NotImplementedError(self.__class__.__name__)

    def create_field(self, name, offset, bitwidth, access_width):
        self.__fields[name] = (offset, bitwidth, access_width)

    def field_bitwidth(self, name):
        return self.__fields[name][1]

    def read_field(self, name):
        offset, bitwidth, access_width = self.__fields[name]
        acc = 0
        acc_bit_count = 0
        bit_idx = offset
        bit_remaining = bitwidth

        assert offset + bitwidth <= self.__length * 8, \
            f"Buffer overflow: attempt to access field {name} at bit {offset + bitwidth} while the buffer has only {len(self.__data) * 8} bits"

        # Bits out of byte boundary
        if bit_idx % access_width > 0:
            # byte_idx shall be (access_width // 8)-byte aligned
            byte_idx = (bit_idx // access_width) * (access_width // 8)
            bit_count = (access_width - bit_idx % access_width)
            if bit_count > bit_remaining:
                bit_count = bit_remaining

            mask = self.bitmask(bit_idx % access_width + bit_count - 1, bit_idx % access_width)
            acc = (self.read(byte_idx, access_width) & mask) >> (bit_idx % access_width)
            acc_bit_count += bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        while bit_remaining > 0:
            byte_idx = bit_idx // 8
            bit_count = min(access_width, bit_remaining)

            mask = self.bitmask(bit_count - 1, 0)
            acc |= (self.read(byte_idx, access_width) & mask) << acc_bit_count
            acc_bit_count += bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        return acc

    def write_field(self, name, value):
        offset, bitwidth, access_width = self.__fields[name]
        bit_idx = offset
        bit_remaining = bitwidth

        assert offset + bitwidth <= self.__length * 8, \
            f"Buffer overflow: attempt to access field {name} at bit {offset + bitwidth} while the buffer has only {len(self.__data) * 8} bits"

        # Bits out of access_width boundary
        if bit_idx % access_width > 0:
            byte_idx = (bit_idx // access_width) * (access_width // 8)
            bit_count = (access_width - bit_idx % access_width)
            if bit_count > bit_remaining:
                bit_count = bit_remaining

            mask_of_write = self.bitmask(bit_idx % access_width + bit_count - 1, bit_idx % access_width)
            mask_of_keep = ((1 << access_width) - 1) - mask_of_write
            v = (value & ((1 << bit_count) - 1)) << (bit_idx % access_width)
            self.write(byte_idx, (v & mask_of_write) | (self.read(byte_idx, access_width) & mask_of_keep), access_width)

            value >>= bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

        while bit_remaining > 0:
            byte_idx = bit_idx // 8
            bit_count = min(access_width, bit_remaining)

            mask_of_write = self.bitmask(bit_count - 1, 0)
            mask_of_keep = ((1 << access_width) - 1) - mask_of_write
            v = (value & ((1 << bit_count) - 1))
            self.write(byte_idx, (v & mask_of_write) | (self.read(byte_idx, access_width) & mask_of_keep), access_width)

            value >>= bit_count
            bit_idx += bit_count
            bit_remaining -= bit_count

    def to_buffer(self):
        return self

class Buffer(BufferBase):
    def __init__(self, data):
        assert len(data) > 0
        super().__init__(len(data))
        self.__data = bytearray(data)

    @property
    def data(self):
        return bytes(self.__data)

    def read(self, byte_idx, bit_width):
        acc = 0
        byte_width = min(bit_width // 8, len(self.__data) - byte_idx)
        return int.from_bytes(self.__data[byte_idx : (byte_idx + byte_width)], sys.byteorder)

    def write(self, byte_idx, value, bit_width):
        byte_width = min(bit_width // 8, len(self.__data) - byte_idx)
        self.__data[byte_idx : (byte_idx + byte_width)] = value.to_bytes(byte_width, sys.byteorder)

    def get(self):
        return self.__data

    def set(self, value):
        data = value.to_buffer().get()
        copy_length = min(len(data), len(self.__data))
        self.__data[:copy_length] = data[:copy_length]

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

class StreamIOBuffer(BufferBase):
    def __init__(self, stream, base, length):
        super().__init__(length)
        self.__stream = stream
        self.__base = base

    def read(self, byte_idx, bit_width):
        byte_width = bit_width // 8
        self.__stream.seek(self.__base + byte_idx)
        data = self.__stream.read(byte_width)
        return int.from_bytes(data, sys.byteorder)

    def write(self, byte_idx, value, bit_width):
        byte_width = bit_width // 8
        self.__stream.seek(self.__base + byte_idx)
        self.__stream.write(value.to_bytes(byte_width, sys.byteorder))

class IndexedIOBuffer(BufferBase):
    def __init__(self, index_register, data_register):
        # FIXME: Get the real size of an indexed I/O region
        super().__init__(256)
        self.__index_register = index_register
        self.__data_register = data_register

    def read(self, byte_idx, bit_width):
        assert bit_width == 8, f"Indexed I/O buffers can only be read one byte at a time"
        self.__index_register.set(Integer(byte_idx, 8))
        return self.__data_register.get()

    def write(self, byte_idx, value, bit_width):
        # Do not allow writes to indexed I/O buffer
        assert False, "Cannot write to indexed I/O buffers"

class BufferField(Object):
    def __init__(self, buf, field):
        self.__buf = buf
        self.__field = field

    def get(self):
        return self.__buf.read_field(self.__field)

    def set(self, obj):
        self.__buf.write_field(self.__field, obj.get())

    def set_writable(self):
        self.__buf.set_field_writable(self.__field)

    def to_integer(self):
        return Integer(self.get())

    def to_buffer(self):
        bitwidth = self.__buf.field_bitwidth(self.__field)
        return Buffer(self.get().to_bytes((bitwidth + 7) // 8, sys.byteorder))

    def to_string(self):
        return f"BufferField({self.__field})"

    def to_hex_string(self):
        return String(hex(self.get())[2:])

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
        self.name = tree.children[1].value
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
            elif isinstance(self.__obj, Buffer):
                name = f"byte_{hex(self.__index)[2:]}"
                self.__obj.create_field(name, self.__index * 8, 8, 8)
                return BufferField(self.__obj, name)
            else:
                raise NotImplementedError(self.__obj.__class__.__name__)
        else:
            return self.__obj

    def set(self, obj, index=None):
        self.__obj = obj
        self.__index = index

class OperationRegion(Object):
    devmem = None
    devport = None
    opened_indexed_regions = {}

    @classmethod
    def open_system_memory(cls, name, offset, length):
        if not cls.devmem:
            cls.devmem = open("/dev/mem", "rb", buffering=0)

        logging.debug(f"Open system memory space {name}: [{hex(offset)}, {hex(offset + length - 1)}]")
        offset_page_aligned = (offset >> 12) << 12
        length_page_aligned = ceil(((offset & 0xFFF) + length) / 0x1000) * 0x1000
        try:
            mm = mmap.mmap(cls.devmem.fileno(), length_page_aligned, flags=mmap.MAP_PRIVATE, prot=mmap.PROT_READ, offset=offset_page_aligned)
        except PermissionError as e:
            logging.debug(f"Do not have permission to access [{hex(offset_page_aligned)}, {hex(offset_page_aligned + length_page_aligned)}] by /dev/mem.")
            logging.debug(f"You may need to add `iomem=relaxed` to the Linux kernel command line in your bootloader configuration file.")
            raise
        iobuf = StreamIOBuffer(mm, offset & 0xFFF, length)
        return OperationRegion(iobuf)

    @classmethod
    def open_system_io(cls, name, offset, length):
        if not cls.devport:
            cls.devport = open("/dev/port", "w+b", buffering=0)

        logging.debug(f"Open system I/O space {name}: [{hex(offset)}, {hex(offset + length - 1)}]")
        iobuf = StreamIOBuffer(cls.devport, offset, length)
        return OperationRegion(iobuf)

    @classmethod
    def open_pci_configuration_space(cls, bus_number, device_adr, offset, length):
        assert offset <= 0xFF and (offset + length) <= 0x100
        # Assume bus is 0 for now
        bus = bus_number
        device = device_adr >> 16
        function = device_adr & 0xFF
        sysfs_path = "/sys/devices/pci0000:%02x/0000:%02x:%02x.%d/config" % (bus, bus, device, function)
        try:
            f = open(sysfs_path, "rb", buffering=0)
            iobuf = StreamIOBuffer(f, offset, length)
            return OperationRegion(iobuf)
        except FileNotFoundError:
            logging.debug(f"Cannot read the configuration space of %02x:%02x.%d from {sysfs_path}. Assume the PCI device does not exist." % (bus, device, function))
            data = bytearray([0xff]) * length
            buf = Buffer(data)
            return OperationRegion(buf)

    @classmethod
    def open_indexed_region(cls, index_register, data_register):
        logging.debug(f"Open I/O region indexed by index register {index_register.to_string()} and data register {data_register.to_string()}.")
        k = (str(index_register), str(data_register))
        if k not in cls.opened_indexed_regions.keys():
            iobuf = IndexedIOBuffer(index_register, data_register)
            region = OperationRegion(iobuf)
            cls.opened_indexed_regions[k] = region

            # Mark the index register as writable
            index_register.set_writable()

            return region
        else:
            return cls.opened_indexed_regions[k]

    def __init__(self, iobuf):
        self.__iobuf = iobuf
        self.__writable_fields = set()

    def create_field(self, name, offset, bitwidth, access_width):
        self.__iobuf.create_field(name, offset, bitwidth, access_width)

    def read_field(self, name):
        return self.__iobuf.read_field(name)

    def field_bitwidth(self, name):
        return self.__iobuf.field_bitwidth(name)

    def write_field(self, name, value):
        # Do not allow writes to stream I/O buffer unless the base is explicitly marked as writable
        if name in self.__writable_fields:
            self.__iobuf.write_field(name, value)
        else:
            if isinstance(value, int):
                logging.debug(f"Skip writing 0x{value:0X} to I/O field {name}")
            else:
                logging.debug(f"Skip writing {value} to I/O field {name}")

    def set_field_writable(self, name):
        self.__writable_fields.add(name)

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

class PowerResource(Object):
    def __init__(self, name):
        self.name = name

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
