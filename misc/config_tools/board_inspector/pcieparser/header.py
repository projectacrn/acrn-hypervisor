# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy
import inspectorlib.cdata as cdata

class Common(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('vendor_id', ctypes.c_uint16),
        ('device_id', ctypes.c_uint16),
        ('command', ctypes.c_uint16),
        ('status', ctypes.c_uint16),
        ('revision_id', ctypes.c_uint32, 8),
        ('class_code', ctypes.c_uint32, 24),
        ('cacheline_size', ctypes.c_uint8),
        ('latency_timer', ctypes.c_uint8),
        ('header_type', ctypes.c_uint8, 7),
        ('multi_function', ctypes.c_uint8, 1),
        ('bist', ctypes.c_uint8),
    ]

class MemoryBar32(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('indicator', ctypes.c_uint8, 1),
        ('type', ctypes.c_uint8, 2),
        ('prefetchable', ctypes.c_uint8, 1),
        ('base_z', ctypes.c_uint32, 28),
    ]

    resource_type = "memory"

    @property
    def base(self):
        return self.base_z << 4

class MemoryBar64(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('indicator', ctypes.c_uint8, 1),
        ('type', ctypes.c_uint8, 2),
        ('prefetchable', ctypes.c_uint8, 1),
        ('base_z', ctypes.c_uint64, 60),
    ]

    resource_type = "memory"

    @property
    def base(self):
        return self.base_z << 4

class IOBar(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('indicator', ctypes.c_uint8, 1),
        ('reserved', ctypes.c_uint8, 1),
        ('base_z', ctypes.c_uint32, 30),
    ]

    resource_type = "io_port"

    @property
    def base(self):
        return self.base_z << 2

PCIE_BAR_SPACE_MASK = 0x1
PCIE_BAR_MEMORY_SPACE = 0x0
PCIE_BAR_IO_SPACE = 0x1

PCIE_BAR_TYPE_MASK = 0x6
PCIE_BAR_TYPE_32_BIT = 0x0
PCIE_BAR_TYPE_64_BIT = 0x4

def header_type_0_field_list(addr):
    bar_list = list()
    bar_addr = addr
    bar_end = addr + 0x18
    while bar_addr < bar_end:
        bar = ctypes.c_uint32.from_address(bar_addr).value
        idx = int((bar_addr - addr) / 4)
        if (bar & PCIE_BAR_SPACE_MASK) == PCIE_BAR_MEMORY_SPACE:
            if (bar & PCIE_BAR_TYPE_MASK) == PCIE_BAR_TYPE_64_BIT:
                bar_list.append((f"bar{idx}", MemoryBar64))
                bar_addr += 0x8
            else:
                bar_list.append((f"bar{idx}", MemoryBar32))
                bar_addr += 0x4
        else:
            bar_list.append((f"bar{idx}", IOBar))
            bar_addr += 0x4

    class Bars(cdata.Struct):
        _pack_ = 1
        _fields_ = bar_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    return [
        ('bars', Bars),
        ('cardbus_cis_pointer', ctypes.c_uint32),
        ('subsystem_vendor_id', ctypes.c_uint16),
        ('subsystem_device_id', ctypes.c_uint16),
        ('expansion_rom_base_address', ctypes.c_uint32),
        ('capability_pointer', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8 * 7),
        ('interrupt_line', ctypes.c_uint8),
        ('interrupt_pin', ctypes.c_uint8),
        ('min_gnt', ctypes.c_uint8),
        ('max_lat', ctypes.c_uint8),
    ]

def header_type_1_field_list(addr):
    bar_list = list()
    bar_addr = addr
    bar_end = addr + 0x08
    while bar_addr < bar_end:
        bar = ctypes.c_uint32.from_address(addr).value
        idx = int((bar_addr - addr) / 4)
        if (bar & PCIE_BAR_SPACE_MASK) == PCIE_BAR_MEMORY_SPACE:
            if (bar & PCIE_BAR_TYPE_MASK) == PCIE_BAR_TYPE_64_BIT:
                bar_list.append((f"bar{idx}", MemoryBar64))
                bar_addr += 0x8
            else:
                bar_list.append((f"bar{idx}", MemoryBar32))
                bar_addr += 0x4
        else:
            bar_list.append((f"bar{idx}", IOBar))
            bar_addr += 0x4

    class Bars(cdata.Struct):
        _pack_ = 1
        _fields_ = bar_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    return [
        ('bars', Bars),
        ('primary_bus_number', ctypes.c_uint8),
        ('secondary_bus_number', ctypes.c_uint8),
        ('subordinate_bus_number', ctypes.c_uint8),
        ('secondary_latency_timer', ctypes.c_uint8),
        ('io_base', ctypes.c_uint8),
        ('io_limit', ctypes.c_uint8),
        ('secondary_status', ctypes.c_uint16),
        ('memory_base', ctypes.c_uint16),
        ('memory_limit', ctypes.c_uint16),
        ('prefetchable_memory_base', ctypes.c_uint16),
        ('prefetchable_memory_limit', ctypes.c_uint16),
        ('prefetchable_base_upper_32_bits', ctypes.c_uint32),
        ('prefetchable_limit_upper_32_bits', ctypes.c_uint32),
        ('io_base_upper_16_bits', ctypes.c_uint16),
        ('io_limit_upper_16_bits', ctypes.c_uint16),
        ('capability_pointer', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8 * 3),
        ('expansion_rom_base_address', ctypes.c_uint32),
        ('interrupt_line', ctypes.c_uint8),
        ('interrupt_pin', ctypes.c_uint8),
        ('bridge_control', ctypes.c_uint16),
    ]

def header_field_list(addr):
    common_header = Common.from_address(addr)
    if common_header.header_type == 0x00:
        return header_type_0_field_list(addr + ctypes.sizeof(Common))
    elif common_header.header_type == 0x01:
        return header_type_1_field_list(addr + ctypes.sizeof(Common))
    else:
        return [('unparsed_data', ctypes.c_uint8 * 0x30)]

def header_factory(field_list):
    class Header(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(Common._fields_) + field_list
    return Header

def header(data):
    """Create class based on decode of a PCI configuration space header from raw data."""
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    field_list = header_field_list(addr)
    return header_factory(field_list).from_buffer_copy(data)
