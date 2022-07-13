# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import inspectorlib.cdata as cdata
from acpiparser._utils import TableHeader

class DMARSubtable(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('subtype', ctypes.c_uint16),
        ('length', ctypes.c_uint16),
    ]

class DMARDeviceScopePath(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('pci_device', ctypes.c_uint8),
        ('pci_function', ctypes.c_uint8),
    ]

def DMARDeviceScope_factory(num_dev_scope_path):
    class DMARDeviceScope(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('type', ctypes.c_uint8),
            ('length', ctypes.c_uint8),
            ('reserved', ctypes.c_uint16),
            ('enumeration_id', ctypes.c_uint8),
            ('start_bus_number', ctypes.c_uint8),
            ('paths', DMARDeviceScopePath * num_dev_scope_path),
        ]
    return DMARDeviceScope

def dmar_device_scope_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    base_len_DMARDeviceScope = ctypes.sizeof(DMARDeviceScope_factory(0))
    len_DMARDeviceScopePath = ctypes.sizeof(DMARDeviceScopePath)
    while addr < end:
        subtable_num += 1
        subtable = DMARDeviceScope_factory(0).from_address(addr)
        num_dev_scope_path = (subtable.length - base_len_DMARDeviceScope) // len_DMARDeviceScopePath
        cls = DMARDeviceScope_factory(num_dev_scope_path)
        addr += subtable.length
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

class drhd_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('include_pci_all', ctypes.c_uint8, 1),
    ]

class drhd_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', drhd_flags_bits),
    ]

def DMARSubtableDRHD_factory(field_list):

    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class DMARSubtableDRHD(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(DMARSubtable._fields_) + [
            ('flags', drhd_flags),
            ('reserved', ctypes.c_uint8),
            ('segment_number', ctypes.c_uint16),
            ('base_address', ctypes.c_uint64),
            ('device_scopes', subtables)
        ]
    return DMARSubtableDRHD

def DMARSubtableRMRR_factory(field_list):

    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class DMARSubtableRMRR(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(DMARSubtable._fields_) + [
            ('reserved', ctypes.c_uint16),
            ('segment_number', ctypes.c_uint16),
            ('base_address', ctypes.c_uint64),
            ('limit_address', ctypes.c_uint64),
            ('device_scopes', subtables),
        ]

    return DMARSubtableRMRR

class atsr_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('all_ports', ctypes.c_uint8, 1),
    ]

class atsr_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', atsr_flags_bits),
    ]

def DMARSubtableATSR_factory(field_list):

    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class DMARSubtableATSR(cdata.Struct):
        _pack = 1
        _fields_ = copy.copy(DMARSubtable._fields_) + [
            ('flags', atsr_flags),
            ('reserved', ctypes.c_uint8),
            ('segment_number', ctypes.c_uint16),
            ('device_scopes', subtables),
        ]
    return DMARSubtableATSR

class DMARSubtableRHSA(cdata.Struct):
    _pack = 1
    _fields_ = copy.copy(DMARSubtable._fields_) + [
        ('reserved', ctypes.c_uint32),
        ('base_address', ctypes.c_uint64),
        ('proximity_domain', ctypes.c_uint32),
    ]

def DMARSubTableANDD_factory(obj_name_len):
    class DMARSubTableANDD(cdata.Struct):
        _pack = 1
        _fields_ = copy.copy(DMARSubtable._fields_) + [
            ('reserved', ctypes.c_uint8 * 3),
            ('device_num', ctypes.c_uint8),
            ('object_name', ctypes.c_char * obj_name_len),
        ]
    return DMARSubTableANDD

def DMARSubtableUnknown_factory(data_len):
    class DMARSubtableUnknown(cdata.Struct):
        _pack = 1
        _fields_ = copy.copy(DMARSubtable._fields_) + [
            ('data', ctypes.c_uint8 * data_len),
        ]
    return DMARSubtableUnknown

ACPI_DMAR_TYPE_DRHD = 0
ACPI_DMAR_TYPE_RMRR = 1
ACPI_DMAR_TYPE_ATSR = 2
ACPI_DMAR_TYPE_RHSA = 3
ACPI_DMAR_TYPE_ANDD = 4

def dmar_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    base_len_DRHD = ctypes.sizeof(DMARSubtableDRHD_factory(list()))
    base_len_RMRR = ctypes.sizeof(DMARSubtableRMRR_factory(list()))
    base_len_ATSR = ctypes.sizeof(DMARSubtableATSR_factory(list()))
    base_len_ANDD = ctypes.sizeof(DMARSubTableANDD_factory(0))
    while addr < end:
        subtable_num += 1
        subtable = DMARSubtable.from_address(addr)
        if subtable.subtype == ACPI_DMAR_TYPE_DRHD:
            next_field_list = dmar_device_scope_list(addr + base_len_DRHD, subtable.length - base_len_DRHD)
            cls = DMARSubtableDRHD_factory(next_field_list)
        elif subtable.subtype == ACPI_DMAR_TYPE_RMRR:
            next_field_list = dmar_device_scope_list(addr + base_len_RMRR, subtable.length - base_len_RMRR)
            cls = DMARSubtableRMRR_factory(next_field_list)
        elif subtable.subtype == ACPI_DMAR_TYPE_ATSR:
            next_field_list = dmar_device_scope_list(addr + base_len_ATSR, subtable.length - base_len_ATSR)
            cls = DMARSubtableATSR_factory(next_field_list)
        elif subtable.subtype == ACPI_DMAR_TYPE_RHSA:
            cls = DMARSubtableRHSA
        elif subtable.subtype == ACPI_DMAR_TYPE_ANDD:
            cls = DMARSubTableANDD_factory(subtable.length - base_len_ANDD)
        else:
            cls = DMARSubtableUnknown_factory(subtable.length - ctypes.sizeof(DMARSubtable))
        addr += subtable.length
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

class dmar_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('intr_remap', ctypes.c_uint8, 1),
        ('x2apic_opt_out', ctypes.c_uint8, 1),
    ]

class dmar_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint8),
        ('bits', dmar_flags_bits),
    ]

def dmar_factory(field_list):

    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class DMAR_v1(cdata.Struct):
        _pack = 1
        _fields_ = [
            ('header', TableHeader),
            ('host_addr_width', ctypes.c_uint8),
            ('flags', ctypes.c_uint8),
            ('reserved', ctypes.c_uint8 * 10),
            ('remapping_structures', subtables),
        ]

    return DMAR_v1

def DMAR(val):
    """Create class based on decode of an DMAR table from filename."""
    base_length = ctypes.sizeof(dmar_factory(list()))
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    field_list = dmar_subtable_list(addr + base_length, hdr.length - base_length)
    return dmar_factory(field_list).from_buffer_copy(data)
