# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes
import copy

import inspectorlib.cdata as cdata
import inspectorlib.unpack as unpack
from acpiparser._utils import TableHeader

class APICSubtable(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('subtype', ctypes.c_uint8),
        ('length', ctypes.c_uint8),
    ]

class local_apic_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('enabled', ctypes.c_uint32, 1),
    ]

class local_apic_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', local_apic_flags_bits),
    ]

class APICSubtableLocalApic(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('proc_id', ctypes.c_uint8),
        ('apic_id', ctypes.c_uint8),
        ('flags', local_apic_flags),
    ]

class APICSubtableIOApic(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('io_apic_id', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8),
        ('io_apic_addr', ctypes.c_uint32),
        ('global_sys_int_base', ctypes.c_uint32),
    ]

mps_inti_polarity = {
    0b00: 'Conforms to bus specifications',
    0b01: 'Active high',
    0b11: 'Active low',
}

mps_inti_trigger_mode = {
    0b00: 'Conforms to bus specifications',
    0b01: 'Edge-triggered',
    0b11: 'Level-triggered',
}

class APICSubtable_int_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('polarity', ctypes.c_uint16, 2),
        ('trigger_mode', ctypes.c_uint16, 2),
    ]
    _formats = {
        'polarity': unpack.format_table("{}", mps_inti_polarity),
        'trigger_mode': unpack.format_table("{}", mps_inti_trigger_mode),
    }

class APICSubtable_int_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint16),
        ('bits', APICSubtable_int_flags_bits),
    ]

class APICSubtableNmiIntSrc(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('flags', APICSubtable_int_flags),
        ('global_sys_interrupt', ctypes.c_uint32),
    ]

class APICSubtableLocalApicNmi(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('proc_id', ctypes.c_uint8),
        ('flags', APICSubtable_int_flags),
        ('lint_num', ctypes.c_uint8),
    ]

class APICSubtableIntSrcOverride(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('bus', ctypes.c_uint8),
        ('source', ctypes.c_uint8),
        ('global_sys_interrupt', ctypes.c_uint32),
        ('flags', APICSubtable_int_flags)
    ]

class APICSubtableLocalx2Apic(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('reserved', ctypes.c_uint16),
        ('x2apicid', ctypes.c_uint32),
        ('flags', local_apic_flags),
        ('uid', ctypes.c_uint32),
    ]

class APICSubtableLocalx2ApicNmi(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('flags', APICSubtable_int_flags),
        ('uid', ctypes.c_uint32),
        ('lint_num', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8 * 3),
    ]

_performance_interrupt_mode = {
    0: 'Level-triggered',
    1: 'Edge-triggered',
}

class APICSubtableLocalGIC_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('enabled', ctypes.c_uint32, 1),
        ('performance_interrupt_mode', ctypes.c_uint32, 1),
    ]
    _formats = {
        'performance_interrupt_mode': unpack.format_table("{}", mps_inti_polarity),
    }

class APICSubtableLocalGIC_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', APICSubtableLocalGIC_flags_bits),
    ]

class APICSubtableLocalGIC(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('reserved', ctypes.c_uint16),
        ('gic_id', ctypes.c_uint32),
        ('uid', ctypes.c_uint32),
        ('flags', APICSubtableLocalGIC_flags),
        ('parking_protocol_version', ctypes.c_uint32),
        ('performance_interrupt_gsiv', ctypes.c_uint32),
        ('parked_address', ctypes.c_uint64),
        ('physical_base_adddress', ctypes.c_uint64),
    ]

class APICSubtableLocalGICDistributor(cdata.Struct):
    _pack_ = 1
    _fields_ = copy.copy(APICSubtable._fields_) + [
        ('reserved1', ctypes.c_uint16),
        ('gic_id', ctypes.c_uint32),
        ('physical_base_adddress', ctypes.c_uint64),
        ('system_vector_base', ctypes.c_uint32),
        ('reserved2', ctypes.c_uint32),
    ]

def APICSubtableUnknown_factory(_len):
    class APICSubtableUnknown(cdata.Struct):
        _pack_ = 1
        _fields_ = APICSubtable._fields_ + [
            ('data', ctypes.c_uint8 * _len),
        ]
    return APICSubtableUnknown

MADT_TYPE_LOCAL_APIC = 0
MADT_TYPE_IO_APIC = 1
MADT_TYPE_INT_SRC_OVERRIDE = 2
MADT_TYPE_NMI_INT_SRC = 3
MADT_TYPE_LOCAL_APIC_NMI = 4
MADT_TYPE_LOCAL_X2APIC = 9
MADT_TYPE_LOCAL_X2APIC_NMI = 0xA
MADT_TYPE_LOCAL_GIC = 0xB
MADT_TYPE_LOCAL_GIC_DISTRIBUTOR = 0xC

class APIC_table_flags_bits(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('pcat_compat', ctypes.c_uint32, 1),
    ]

class APIC_table_flags(cdata.Union):
    _pack_ = 1
    _anonymous_ = ("bits",)
    _fields_ = [
        ('data', ctypes.c_uint32),
        ('bits', APIC_table_flags_bits),
    ]

def apic_factory(field_list):
    class subtables(cdata.Struct):
        _pack_ = 1
        _fields_ = field_list

        def __iter__(self):
            for f in self._fields_:
                yield getattr(self, f[0])

    class APIC_v3(cdata.Struct):
        _pack_ = 1
        _fields_ = [
            ('header', TableHeader),
            ('local_apic_address', ctypes.c_uint32),
            ('flags', APIC_table_flags),
            ('interrupt_controller_structures', subtables),
        ]

        @property
        def procid_apicid(self):
            procid_apicid_dict = {}
            for subtable in self.interrupt_controller_structures:
                # accumulate the dictionary
                if subtable.subtype == MADT_TYPE_LOCAL_APIC:
                    if subtable.flags.bits.enabled == 1:
                        procid_apicid_dict[subtable.proc_id] = subtable.apic_id
            return procid_apicid_dict

        @property
        def uid_x2apicid(self):
            uid_x2apicid_dict = {}
            for subtable in self.interrupt_controller_structures:
                # accumulate the dictionary
                if subtable.subtype == MADT_TYPE_LOCAL_X2APIC:
                    if subtable.flags.bits.enabled == 1:
                        uid_x2apicid_dict[subtable.uid] = subtable.x2apicid
            return uid_x2apicid_dict

    return APIC_v3

def apic_subtable_list(addr, length):
    end = addr + length
    field_list = list()
    subtable_num = 0
    while addr < end:
        subtable_num += 1
        subtable = APICSubtable.from_address(addr)
        addr += subtable.length
        if subtable.subtype == MADT_TYPE_LOCAL_APIC:
            cls = APICSubtableLocalApic
        elif subtable.subtype == MADT_TYPE_IO_APIC:
            cls = APICSubtableIOApic
        elif subtable.subtype == MADT_TYPE_INT_SRC_OVERRIDE:
            cls = APICSubtableIntSrcOverride
        elif subtable.subtype == MADT_TYPE_NMI_INT_SRC:
            cls = APICSubtableNmiIntSrc
        elif subtable.subtype == MADT_TYPE_LOCAL_APIC_NMI:
            cls = APICSubtableLocalApicNmi
        elif subtable.subtype == MADT_TYPE_LOCAL_X2APIC:
            cls = APICSubtableLocalx2Apic
        elif subtable.subtype == MADT_TYPE_LOCAL_X2APIC_NMI:
            cls = APICSubtableLocalx2ApicNmi
        elif subtable.subtype == MADT_TYPE_LOCAL_GIC:
            cls = APICSubtableLocalGIC
        elif subtable.subtype == MADT_TYPE_LOCAL_GIC_DISTRIBUTOR:
            cls = APICSubtableLocalGICDistributor
        else:
            cls = APICSubtableUnknown_factory(subtable.length - ctypes.sizeof(APICSubtable))
        field_list.append( ('subtable{}'.format(subtable_num), cls) )
    return field_list

def APIC(val):
    """Create class based on decode of an APIC table from filename."""
    preamble_length = ctypes.sizeof(apic_factory(list()))
    data = open(val, mode='rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)
    hdr = TableHeader.from_address(addr)
    subtable_list = apic_subtable_list(addr + preamble_length, hdr.length - preamble_length)
    return apic_factory(subtable_list).from_buffer_copy(data)
