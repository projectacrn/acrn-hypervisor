# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy
import inspectorlib.cdata as cdata

class Capability:
    # Capability names from PCI Local Bus Specification and PCI Express Base Specification
    _cap_names_ = {
        0x01: "Power Management",
        0x02: "AGP",
        0x03: "VPD",
        0x04: "Slot Identification",
        0x05: "MSI",
        0x06: "CompactPCI Hot Swap",
        0x07: "PCI-X",
        0x08: "Hyper Transport",
        0x09: "Vendor-Specific",
        0x0a: "Debug port",
        0x0b: "CompactPCI Central Resource Control",
        0x0c: "Hot Plug",
        0x0d: "Subsystem ID and Subsystem Vendor ID",
        0x0e: "AGP 8x",
        0x0f: "Secure Device",
        0x10: "PCI Express",
        0x11: "MSI-X",
        0x13: "Conventional PCI Advanced Features",
        0x14: "Enhanced Allocation",
        0x15: "FPB",
    }

    @property
    def name(self):
        if self.id in self._cap_names_.keys():
            return self._cap_names_[self.id]
        else:
            return f"Reserved ({hex(self.id)})"

class CapabilityListRegister(cdata.Struct, Capability):
    _pack_ = 1
    _fields_ = [
        ('id', ctypes.c_uint8),
        ('next_cap_ptr', ctypes.c_uint8),
    ]

# Power Management (0x01)

class PowerManagement(cdata.Struct, Capability):
    _pack_ = 1
    _fields_ = copy.copy(CapabilityListRegister._fields_) + [
        ('version', ctypes.c_uint16, 3),
        ('pme_clock', ctypes.c_uint16, 1),
        ('immediate_readiness_on_return_to_d0', ctypes.c_uint16, 1),
        ('device_specific_initialization', ctypes.c_uint16, 1),
        ('aux_current', ctypes.c_uint16, 3),
        ('d1_support', ctypes.c_uint16, 1),
        ('d2_support', ctypes.c_uint16, 1),
        ('pme_support', ctypes.c_uint16, 5),
        ('power_state', ctypes.c_uint16, 2),
        ('reserved1', ctypes.c_uint16, 1),
        ('no_soft_reset', ctypes.c_uint16, 1),
        ('reserved2', ctypes.c_uint16, 4),
        ('pme_en', ctypes.c_uint16, 1),
        ('data_select', ctypes.c_uint16, 4),
        ('data_scale', ctypes.c_uint16, 2),
        ('pme_status', ctypes.c_uint16, 1),
        ('reserved3', ctypes.c_uint8, 6),
        ('undefined', ctypes.c_uint8, 2),
        ('data', ctypes.c_uint8),
    ]

def parse_power_management(buf, cap_ptr):
    return PowerManagement.from_buffer_copy(buf, cap_ptr)

# MSI (0x05)

def MSI_factory(field_list):
    class MSI(cdata.Struct, Capability):
        _pack_ = 1
        _fields_ = copy.copy(CapabilityListRegister._fields_) + [
            ('msi_enable', ctypes.c_uint16, 1),
            ('multiple_message_capable', ctypes.c_uint16, 3),
            ('multiple_message_enable', ctypes.c_uint16, 3),
            ('address_64bit', ctypes.c_uint16, 1),
            ('per_vector_masking_capable', ctypes.c_uint16, 1),
            ('reserved', ctypes.c_uint16, 7),
        ] + field_list

    return MSI

def msi_field_list(addr):
    field_list = list()
    msgctrl = MSI_factory([]).from_address(addr)

    if msgctrl.address_64bit == 1:
        field_list.append(('message_address', ctypes.c_uint64))
    else:
        field_list.append(('message_address', ctypes.c_uint32))

    field_list.append(('message_data', ctypes.c_uint16))

    if msgctrl.per_vector_masking_capable:
        field_list.append(('reserved', ctypes.c_uint16))
        field_list.append(('mask_bits', ctypes.c_uint32))
        field_list.append(('pending_bits', ctypes.c_uint32))

    return field_list

def parse_msi(buf, cap_ptr):
    addr = ctypes.addressof(buf) + cap_ptr
    field_list = msi_field_list(addr)
    return MSI_factory(field_list).from_buffer_copy(buf, cap_ptr)

# MSI-X (0x11)

class MSIX(cdata.Struct, Capability):
    _pack_ = 1
    _fields_ = copy.copy(CapabilityListRegister._fields_) + [
        ('table_size_z', ctypes.c_uint16, 10),
        ('reserved', ctypes.c_uint16, 3),
        ('function_mask', ctypes.c_uint16, 1),
        ('msix_enable', ctypes.c_uint16, 1),
        ('table_bir', ctypes.c_uint32, 3),
        ('table_offset_z', ctypes.c_uint32, 29),
        ('pba_bir', ctypes.c_uint32, 3),
        ('pba_offset_z', ctypes.c_uint32, 29),
    ]

    @property
    def table_size(self):
        return self.table_size_z + 1

    @property
    def table_offset(self):
        return self.table_offset_z << 3

    @property
    def pba_offset(self):
        return self.pba_offset_z << 3

def parse_msix(buf, cap_ptr):
    return MSIX.from_buffer_copy(buf, cap_ptr)

# Module API

capability_parsers = {
    0x1: parse_power_management,
    0x5: parse_msi,
    0x11: parse_msix,
}

def capabilities(data, cap_ptr):
    buf = ctypes.create_string_buffer(data, len(data))

    acc = list()
    while cap_ptr != 0:
        caplist = CapabilityListRegister.from_buffer_copy(buf, cap_ptr)
        if caplist.id in capability_parsers.keys():
            acc.append(capability_parsers[caplist.id](buf, cap_ptr))
        else:
            acc.append(caplist)
        cap_ptr = caplist.next_cap_ptr

    return acc
