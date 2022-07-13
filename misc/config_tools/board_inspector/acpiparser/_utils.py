# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes

import inspectorlib.cdata as cdata
import inspectorlib.unpack as unpack

class TableHeader(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('signature', ctypes.c_char * 4),
        ('length', ctypes.c_uint32),
        ('revision', ctypes.c_ubyte),
        ('checksum', ctypes.c_ubyte),
        ('oemid', ctypes.c_char * 6),
        ('oemtableid', ctypes.c_char * 8),
        ('oemrevision', ctypes.c_uint32),
        ('creatorid', ctypes.c_char * 4),
        ('creatorrevision', ctypes.c_uint32),
    ]

ASID_SYSTEM_MEMORY = 0
ASID_SYSTEM_IO = 1
ASID_PCI_CFG_SPACE = 2
ASID_EMBEDDED_CONTROLLER = 3
ASID_SMBUS = 4
ASID_PCC = 0xA
ASID_FFH = 0x7F

def _asid_str(asid):
    if asid >= 0xC0 and asid <= 0xff:
        return 'OEM Defined'
    _asid = {
        ASID_SYSTEM_MEMORY: 'System Memory',
        ASID_SYSTEM_IO: 'System IO',
        ASID_PCI_CFG_SPACE: 'PCI Configuration Space',
        ASID_EMBEDDED_CONTROLLER: 'Embedded Controller',
        ASID_SMBUS: 'SMBus',
        ASID_PCC: 'Platform Communications Channel (PCC)',
        ASID_FFH: 'Functional Fixed Hardware',
        }
    return _asid.get(asid, 'Reserved')

_access_sizes = {
    0: 'Undefined',
    1: 'Byte access',
    2: 'Word access',
    3: 'Dword access',
    4: 'Qword access',
}

class GAS(cdata.Struct):
    _pack_ = 1
    _fields_ = [
        ('address_space_id', ctypes.c_uint8),
        ('register_bit_width', ctypes.c_uint8),
        ('register_bit_offset', ctypes.c_uint8),
        ('access_size', ctypes.c_uint8),
        ('address', ctypes.c_uint64),
    ]

    _formats = {
        'address_space_id' : unpack.format_function("{:#x}", _asid_str),
        'access_size'      : unpack.format_table("{}", _access_sizes),
    }
