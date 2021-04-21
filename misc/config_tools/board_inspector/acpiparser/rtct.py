# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import lib.cdata as cdata
from acpiparser._utils import TableHeader

class RTCTSubtable(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('size', ctypes.c_uint16),
        ('format', ctypes.c_uint16),
        ('type', ctypes.c_uint32),
    ]

class RTCTSubtableRTCMBinary(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('address', ctypes.c_uint64),
        ('size', ctypes.c_uint32),
    ]

def RTCTSubtableWRCL3Waymasks_factory(data_len):
    class RTCTSubtableWRCL3Waymasks(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('waskmask', ctypes.c_uint32 * (data_len // 4)),
        ]
    return RTCTSubtableWRCL3Waymasks

def RTCTSubtableGTL3Waymasks_factory(data_len):
    class RTCTSubtableGTL3Waymasks(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('waskmask', ctypes.c_uint32 * (data_len // 4)),
        ]
    return RTCTSubtableGTL3Waymasks

def RTCTSubtableSoftwareSRAM_factory(data_len):
    class RTCTSubtableSoftwareSRAM(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('cache_level', ctypes.c_uint32),
            ('base', ctypes.c_uint64),
            ('ways', ctypes.c_uint32),
            ('size', ctypes.c_uint32),
            ('apic_id_tbl', ctypes.c_uint32 * ((data_len - 20) // 4)),
        ]
    return RTCTSubtableSoftwareSRAM

def RTCTSubtableMemoryHierarchyLatency_factory(data_len):
    class RTCTSubtableMemoryHierarchyLatency(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('hierarchy', ctypes.c_uint32),
            ('clock_cycles', ctypes.c_uint32),
            ('apic_id_tbl', ctypes.c_uint32 * ((data_len - 8) // 4)),
        ]
    return RTCTSubtableMemoryHierarchyLatency

def RTCTSubtableUnknown_factory(data_len):
    class RTCTSubtableUnknown(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('data', ctypes.c_uint8 * data_len),
        ]
    return RTCTSubtableUnknown

ACPI_RTCT_TYPE_RTCM_BINARY = 2
ACPI_RTCT_TYPE_WRC_L3Waymasks = 3
ACPI_RTCT_TYPE_GT_L3Waymasks = 4
ACPI_RTCT_TYPE_SoftwareSRAM = 5
ACPI_RTCT_TYPE_Memory_Hierarchy_Latency = 9

def rtct_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    while addr < end:
        subtable_num += 1
        subtable = RTCTSubtable.from_address(addr)
        data_len = subtable.size - ctypes.sizeof(RTCTSubtable)
        if subtable.type == ACPI_RTCT_TYPE_RTCM_BINARY:
            cls = RTCTSubtableRTCMBinary
        elif subtable.type == ACPI_RTCT_TYPE_WRC_L3Waymasks:
            cls = RTCTSubtableWRCL3Waymasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_TYPE_GT_L3Waymasks:
            cls = RTCTSubtableGTL3Waymasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_TYPE_SoftwareSRAM:
            cls = RTCTSubtableSoftwareSRAM_factory(data_len)
        elif subtable.type == ACPI_RTCT_TYPE_Memory_Hierarchy_Latency:
            cls = RTCTSubtableMemoryHierarchyLatency_factory(data_len)
        else:
            cls = RTCTSubtableUnknown_factory(data_len)
        addr += subtable.size
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

def rtct_factory(field_list):

    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class RTCT(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('header', TableHeader),
            ('entries', subtables),
        ]

    return RTCT

def RTCT(val):
    """Create class based on decode of an RTCT table from filename."""
    base_length = ctypes.sizeof(rtct_factory(list()))
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    field_list = rtct_subtable_list(addr + base_length, hdr.length - base_length)
    return rtct_factory(field_list).from_buffer_copy(data)
