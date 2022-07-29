# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import inspectorlib.cdata as cdata
from acpiparser._utils import TableHeader

# Common structures

class RTCTSubtable(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('subtable_size', ctypes.c_uint16),
        ('format_or_version', ctypes.c_uint16),
        ('type', ctypes.c_uint32),
    ]

ACPI_RTCT_TYPE_COMPATIBILITY = 0

class RTCTSubtableCompatibility(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('rtct_version_major', ctypes.c_uint32),
        ('rtct_version_minor', ctypes.c_uint32),
        ('rtcd_version_major', ctypes.c_uint32),
        ('rtcd_version_minor', ctypes.c_uint32),
    ]

def RTCTSubtableUnknown_factory(data_len):
    class RTCTSubtableUnknown(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('data', ctypes.c_uint8 * data_len),
        ]
    return RTCTSubtableUnknown

# RTCT v1

ACPI_RTCT_V1_TYPE_RTCM_BINARY = 2
ACPI_RTCT_V1_TYPE_WRC_L3Waymasks = 3
ACPI_RTCT_V1_TYPE_GT_L3Waymasks = 4
ACPI_RTCT_V1_TYPE_SoftwareSRAM = 5
ACPI_RTCT_V1_TYPE_Memory_Hierarchy_Latency = 9

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
            ('waymask', ctypes.c_uint32 * (data_len // 4)),
        ]
    return RTCTSubtableWRCL3Waymasks

def RTCTSubtableGTL3Waymasks_factory(data_len):
    class RTCTSubtableGTL3Waymasks(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('waymask', ctypes.c_uint32 * (data_len // 4)),
        ]
    return RTCTSubtableGTL3Waymasks

def RTCTSubtableSoftwareSRAM_v1_factory(data_len):
    class RTCTSubtableSoftwareSRAM_v1(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('cache_level', ctypes.c_uint32),
            ('base', ctypes.c_uint64),
            ('ways', ctypes.c_uint32),
            ('size', ctypes.c_uint32),
            ('apic_id_tbl', ctypes.c_uint32 * ((data_len - 20) // 4)),
        ]
    return RTCTSubtableSoftwareSRAM_v1

def RTCTSubtableMemoryHierarchyLatency_v1_factory(data_len):
    class RTCTSubtableMemoryHierarchyLatency_v1(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('hierarchy', ctypes.c_uint32),
            ('clock_cycles', ctypes.c_uint32),
            ('apic_id_tbl', ctypes.c_uint32 * ((data_len - 8) // 4)),
        ]
    return RTCTSubtableMemoryHierarchyLatency_v1

# RTCT v2

ACPI_RTCT_V2_TYPE_RTCD_Limits = 1
ACPI_RTCT_V2_TYPE_CRL_Binary = 2
ACPI_RTCT_V2_TYPE_IA_WayMasks = 3
ACPI_RTCT_V2_TYPE_WRC_WayMasks = 4
ACPI_RTCT_V2_TYPE_GT_WayMasks = 5
ACPI_RTCT_V2_TYPE_SSRAM_WayMask = 6
ACPI_RTCT_V2_TYPE_SoftwareSRAM = 7
ACPI_RTCT_V2_TYPE_MemoryHierarchyLatency = 8
ACPI_RTCT_V2_TYPE_ErrorLogAddress = 9

class RTCTSubtableRTCDLimits(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('total_ia_l2_clos', ctypes.c_uint32),
        ('total_ia_l3_clos', ctypes.c_uint32),
        ('total_l2_instances', ctypes.c_uint32),
        ('total_l3_instances', ctypes.c_uint32),
        ('total_gt_clos', ctypes.c_uint32),
        ('total_wrc_clos', ctypes.c_uint32),
        ('max_tcc_streams', ctypes.c_uint32),
        ('max_tcc_registers', ctypes.c_uint32),
    ]

def RTCTSubtableIAWayMasks_factory(data_len):
    class RTCTSubtableIAWayMasks(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('level', ctypes.c_uint32),
            ('cache_id', ctypes.c_uint32),
            ('waymask', ctypes.c_uint32 * ((data_len - 8) // 4)),
        ]
    return RTCTSubtableIAWayMasks

class RTCTSubtableWRCWayMasks(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('level', ctypes.c_uint32),
        ('cache_id', ctypes.c_uint32),
        ('waymask', ctypes.c_uint32),
    ]

def RTCTSubtableGTWayMasks_factory(data_len):
    class RTCTSubtableGTWayMasks(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('level', ctypes.c_uint32),
            ('cache_id', ctypes.c_uint32),
            ('waymask', ctypes.c_uint32 * ((data_len - 8) // 4)),
        ]
    return RTCTSubtableGTWayMasks

class RTCTSubtableSSRAMWayMask(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('level', ctypes.c_uint32),
        ('cache_id', ctypes.c_uint32),
        ('waymask', ctypes.c_uint32),
    ]

class RTCTSubtableSoftwareSRAM_v2(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('level', ctypes.c_uint32),
        ('cache_id', ctypes.c_uint32),
        ('base', ctypes.c_uint64),
        ('size', ctypes.c_uint32),
        ('shared', ctypes.c_uint32),
    ]

def RTCTSubtableMemoryHierarchyLatency_v2_factory(data_len):
    class RTCTSubtableMemoryHierarchyLatency_v2(cdata.Struct):
        _pack_ = 1
        _fields_ = copy.copy(RTCTSubtable._fields_) + [
            ('hierarchy', ctypes.c_uint32),
            ('clock_cycles', ctypes.c_uint32),
            ('cache_id', ctypes.c_uint32 * ((data_len - 8) // 4)),
        ]
    return RTCTSubtableMemoryHierarchyLatency_v2

class RTCTSubtableErrorLogAddress(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(RTCTSubtable._fields_) + [
        ('address', ctypes.c_uint64),
        ('size', ctypes.c_uint32),
    ]

# Parsers

def rtct_version(addr, length):
    end = addr + length
    while addr < end:
        subtable = RTCTSubtable.from_address(addr)
        if subtable.type == ACPI_RTCT_TYPE_COMPATIBILITY:
            subtable = RTCTSubtableCompatibility.from_address(addr)
            return subtable.rtct_version_major
        addr += subtable.subtable_size
    # RTCT v1 does not have a compatibility entry
    return 1

def rtct_v1_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    while addr < end:
        subtable_num += 1
        subtable = RTCTSubtable.from_address(addr)
        data_len = subtable.subtable_size - ctypes.sizeof(RTCTSubtable)
        if subtable.type == ACPI_RTCT_V1_TYPE_RTCM_BINARY:
            cls = RTCTSubtableRTCMBinary
        elif subtable.type == ACPI_RTCT_V1_TYPE_WRC_L3Waymasks:
            cls = RTCTSubtableWRCL3Waymasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_V1_TYPE_GT_L3Waymasks:
            cls = RTCTSubtableGTL3Waymasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_V1_TYPE_SoftwareSRAM:
            cls = RTCTSubtableSoftwareSRAM_v1_factory(data_len)
        elif subtable.type == ACPI_RTCT_V1_TYPE_Memory_Hierarchy_Latency:
            cls = RTCTSubtableMemoryHierarchyLatency_v1_factory(data_len)
        else:
            cls = RTCTSubtableUnknown_factory(data_len)
        addr += subtable.subtable_size
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

def rtct_v2_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    while addr < end:
        subtable_num += 1
        subtable = RTCTSubtable.from_address(addr)
        data_len = subtable.subtable_size - ctypes.sizeof(RTCTSubtable)
        if subtable.type == ACPI_RTCT_TYPE_COMPATIBILITY:
            cls = RTCTSubtableCompatibility
        elif subtable.type == ACPI_RTCT_V2_TYPE_RTCD_Limits:
            cls = RTCTSubtableRTCDLimits
        elif subtable.type == ACPI_RTCT_V2_TYPE_CRL_Binary:
            cls = RTCTSubtableRTCMBinary
        elif subtable.type == ACPI_RTCT_V2_TYPE_IA_WayMasks:
            cls = RTCTSubtableIAWayMasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_V2_TYPE_WRC_WayMasks:
            cls = RTCTSubtableWRCWayMasks
        elif subtable.type == ACPI_RTCT_V2_TYPE_GT_WayMasks:
            cls = RTCTSubtableGTWayMasks_factory(data_len)
        elif subtable.type == ACPI_RTCT_V2_TYPE_SSRAM_WayMask:
            cls = RTCTSubtableSSRAMWayMask
        elif subtable.type == ACPI_RTCT_V2_TYPE_SoftwareSRAM:
            cls = RTCTSubtableSoftwareSRAM_v2
        elif subtable.type == ACPI_RTCT_V2_TYPE_MemoryHierarchyLatency:
            cls = RTCTSubtableMemoryHierarchyLatency_v2_factory(data_len)
        elif subtable.type == ACPI_RTCT_V2_TYPE_ErrorLogAddress:
            cls = RTCTSubtableErrorLogAddress
        else:
            cls = RTCTSubtableUnknown_factory(data_len)
        addr += subtable.subtable_size
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

def rtct_version_and_subtable_list(addr, length):
    version = rtct_version(addr, length)

    if version == 1:
        return (version, rtct_v1_subtable_list(addr, length))
    else:
        return (version, rtct_v2_subtable_list(addr, length))

def rtct_factory(field_list, version):

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

        @property
        def version(self):
            return version

    return RTCT

def RTCT(val):
    """Create class based on decode of an RTCT table from filename."""
    base_length = ctypes.sizeof(rtct_factory(list(), 0))
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    version, field_list = rtct_version_and_subtable_list(addr + base_length, hdr.length - base_length)
    return rtct_factory(field_list, version).from_buffer_copy(data)
