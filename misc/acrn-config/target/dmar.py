# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import ctypes

ACPI_DMAR_TYPE = {
    'ACPI_DMAR_TYPE_HARDWARE_UNIT':0,
    'ACPI_DMAR_TYPE_RESERVED_MEMORY':1,
    'ACPI_DMAR_TYPE_ROOT_ATS':2,
    'ACPI_DMAR_TYPE_HARDWARE_AFFINITY':3,
    'ACPI_DMAR_TYPE_NAMESPACE':4,
    'ACPI_DMAR_TYPE_RESERVED':5,
    }

ACPI_DEV_SCOPE_TYPE = {
    'ACPI_DMAR_SCOPE_TYPE_NOT_USED':0,
    'ACPI_DMAR_SCOPE_TYPE_ENDPOINT':1,
    'ACPI_DMAR_SCOPE_TYPE_BRIDGE':2,
    'ACPI_DMAR_SCOPE_TYPE_IOAPIC':3,
    'ACPI_DMAR_SCOPE_TYPE_HPET':4,
    'ACPI_DMAR_SCOPE_TYPE_NAMESPACE':5,
    'ACPI_DMAR_SCOPE_TYPE_RESERVED':6,
    }

class DmarHeader(ctypes.Structure):
    """DMAR Header"""
    _pack_ = 1
    _fields_ = [
        ('signature', ctypes.c_char*4),
        ('length', ctypes.c_uint32),
        ('revision', ctypes.c_ubyte),
        ('checksum', ctypes.c_ubyte),
        ('oem_id', ctypes.c_char*6),
        ('oem_table_id', ctypes.c_char*8),
        ('oem_revision', ctypes.c_uint32),
        ('asl_compiler_id', ctypes.c_char*4),
        ('asl_compiler_revision', ctypes.c_uint32),
        ('host_addr_width', ctypes.c_ubyte),
        ('flags', ctypes.c_ubyte),
        ('reserved', ctypes.c_ubyte*10),
    ]

    def style_check_1(self):
        """Style check if have public method"""
        self._pack_ = 0

    def style_check_2(self):
        """Style check if have public method"""
        self._pack_ = 0


class DmarSubtblHeader(ctypes.Structure):
    """DMAR Sub Table Header"""
    _pack_ = 1
    _fields_ = [
        ('type', ctypes.c_uint16),
        ('length', ctypes.c_uint16),
    ]

    def style_check_1(self):
        """Style check if have public method"""
        self._pack_ = 0

    def style_check_2(self):
        """Style check if have public method"""
        self._pack_ = 0


class DmarDevScope(ctypes.Structure):
    """DMAR Device Scope"""
    _pack_ = 1
    _fields_ = [
        ('entry_type', ctypes.c_uint8),
        ('scope_length', ctypes.c_uint8),
        ('reserved', ctypes.c_uint16),
        ('enumeration_id', ctypes.c_uint8),
        ('bus', ctypes.c_uint8),
    ]

    def style_check_1(self):
        """Style check if have public method"""
        self._pack_ = 0

    def style_check_2(self):
        """Style check if have public method"""
        self._pack_ = 0


class DmarHwUnit(ctypes.Structure):
    """DMAR Hardware Unit"""
    _pack_ = 1
    _fields_ = [
        ('sub_header', DmarSubtblHeader),
        ('flags', ctypes.c_uint8),
        ('reserved', ctypes.c_uint8),
        ('segment', ctypes.c_uint16),
        ('address', ctypes.c_uint64),
    ]

    def style_check_1(self):
        """Style check if have public method"""
        self._pack_ = 0

    def style_check_2(self):
        """Style check if have public method"""
        self._pack_ = 0


class DevScopePath(ctypes.Structure):
    """DEVICE Scope Path"""
    _pack_ = 1
    _fields_ = [
        ("device", ctypes.c_uint8),
        ("function", ctypes.c_uint8),
    ]

    def style_check_1(self):
        """Style check if have public method"""
        self._pack_ = 0

    def style_check_2(self):
        """Style check if have public method"""
        self._pack_ = 0


def map_file(sysnode):
    """Map sys node to memory address"""
    data = open(sysnode, 'rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)

    return addr

class DmarHwList:
    """DMAR HW List"""
    def __init__(self):
        self.hw_segment_list = []
        self.hw_flags_list = []
        self.hw_address_list = []
        self.hw_ignore = {}

    def style_check_1(self):
        """Style check if have public method"""
        self.hw_ignore = {}

    def style_check_2(self):
        """Style check if have public method"""
        self.hw_ignore = {}


class DmarDevList:
    """DMAR DEV List"""
    def __init__(self):
        self.dev_scope_cnt_list = []
        self.dev_bus_list = []
        self.dev_path_list = []
        self.dev_ioapic = {}

    def style_check_1(self):
        """Style check if have public method"""
        self.dev_bus_list = []

    def style_check_2(self):
        """Style check if have public method"""
        self.dev_bus_list = []


class DmarTbl:
    """DMAR TBL"""
    def __init__(self):
        self.sub_tbl_offset = 0
        self.dmar_drhd = 0
        self.dmar_dev_scope = 0
        self.dev_scope_offset = 0
        self.dev_scope_cnt = 0
        self.path_offset = 0

    def style_check_1(self):
        """Style check if have public method"""
        self.path_offset = 0

    def style_check_2(self):
        """Style check if have public method"""
        self.path_offset = 0


class PathDevFun:
    """Path Device Function meta data"""
    def __init__(self):
        self.path = 0
        self.device = 0
        self.function = 0

    def style_check_1(self):
        """Style check if have public method"""
        self.path = 0

    def style_check_2(self):
        """Style check if have public method"""
        self.path = 0


def walk_pci_bus(tmp_pdf, dmar_tbl, dmar_hw_list, n_cnt, drhd_cnt):
    """Walk Pci bus
    :param tmp_pdf: it is a class what contains path,device,function in dmar device scope region
    :param dmar_tbl: it is a class to describe dmar which contains Device Scope and DRHD
    :param dmar_hw_list: it is a class to describe hardware scope in DMAR table
    :param n_cnt: the number of device in device scope
    :param drhd_cnt: it is a counter to calculate the DRHD in DMAR table
    """
    while n_cnt:
        scope_path = DevScopePath.from_address(dmar_tbl.path_offset)
        tmp_pdf.device = scope_path.device
        tmp_pdf.function = scope_path.function
        tmp_pdf.path = (((tmp_pdf.device & 0x1F) << 3) | ((tmp_pdf.function & 0x7)))
        dmar_tbl.path_offset += ctypes.sizeof(DevScopePath)
        n_cnt -= 1

        # this not support to warning if no dedicated iommu for gpu
        if ((dmar_tbl.dmar_drhd.segment << 16) | (
                dmar_tbl.dmar_dev_scope.bus << 8) | scope_path.function) == 0x10:
            dmar_hw_list.hw_ignore[drhd_cnt] = 'true'
        else:
            dmar_hw_list.hw_ignore[drhd_cnt] = 'false'

    return (tmp_pdf, dmar_tbl, dmar_hw_list)


def walk_dev_scope(dmar_tbl, dmar_dev_list, dmar_hw_list, drhd_cnt):
    """Walk device scope
    :param dmar_tbl: it is a class to describe dmar which contains Device Scope and DRHD
    :param dmar_dev_list: it is a class to describe device scope in DMAR table
    :param dmar_hw_list: it is a class to describe DRHD in DMAR table
    :param drhd_cnt: it is a counter to calculate the DRHD in DMAR table
    """
    dmar_tbl.dev_scope_offset = dmar_tbl.sub_tbl_offset + ctypes.sizeof(DmarHwUnit)
    scope_end = dmar_tbl.dev_scope_offset + dmar_tbl.dmar_drhd.sub_header.length
    dmar_tbl.dev_scope_cnt = 0

    while dmar_tbl.dev_scope_offset < scope_end:
        dmar_tbl.dmar_dev_scope = DmarDevScope.from_address(dmar_tbl.dev_scope_offset)
        if dmar_tbl.dmar_dev_scope.scope_length <= 0:
            break
        if dmar_tbl.dmar_dev_scope.entry_type != \
                ACPI_DEV_SCOPE_TYPE['ACPI_DMAR_SCOPE_TYPE_NOT_USED'] and \
                dmar_tbl.dmar_dev_scope.entry_type < \
                ACPI_DEV_SCOPE_TYPE['ACPI_DMAR_SCOPE_TYPE_RESERVED']:
            dmar_tbl.dev_scope_cnt += 1

            # path offset is in end of device spcope
            dmar_tbl.path_offset = dmar_tbl.dev_scope_offset + ctypes.sizeof(DmarDevScope)
            # walk the pci bus with path deep, and find the {Device,Function}
            tmp_pdf = PathDevFun()
            n_cnt = (dmar_tbl.dmar_dev_scope.scope_length - ctypes.sizeof(DmarDevScope)) // 2
            (tmp_pdf, dmar_tbl, dmar_hw_list) = walk_pci_bus(tmp_pdf, dmar_tbl, dmar_hw_list,
                                                             n_cnt, drhd_cnt)
            dmar_dev_list.dev_bus_list.append(dmar_tbl.dmar_dev_scope.bus)
            dmar_dev_list.dev_path_list.append(tmp_pdf.path)

            # if the scope entry type is ioapic, should address enumeration id
            if dmar_tbl.dmar_dev_scope.entry_type ==\
                    ACPI_DEV_SCOPE_TYPE['ACPI_DMAR_SCOPE_TYPE_IOAPIC']:
                dmar_dev_list.dev_ioapic[drhd_cnt] = dmar_tbl.dmar_dev_scope.enumeration_id

        dmar_tbl.dev_scope_offset += dmar_tbl.dmar_dev_scope.scope_length

    return (dmar_tbl, dmar_dev_list, dmar_hw_list)


def walk_dmar_table(dmar_tbl, dmar_hw_list, dmar_dev_list, sysnode):
    """Walk dmar table and get information
    :param dmar_tbl: it is a class to describe dmar which contains Device Scope and DRHD
    :param dmar_hw_list: it is a class to describe hardware scope in DMAR table
    :param dmar_dev_list: it is a class to describe device scope in DMAR table
    :param sysnode: the system device node of acpi table, such as: /sys/firmware/acpi/tables/DMAR
    """
    data = open(sysnode, 'rb').read()
    buf = ctypes.create_string_buffer(data, len(data))
    addr = ctypes.addressof(buf)

    # contain the dmar tbl_header
    dmar_tbl_header = DmarHeader.from_address(addr)

    # cytpes.c_int16.from_address(addr) reade int16 from ad1
    # in end of tbl header is remapping structure(DRHD/sub tbl)
    dmar_tbl.sub_tbl_offset = addr + ctypes.sizeof(DmarHeader)
    drhd_cnt = 0
    while True:
        sub_dmar = DmarSubtblHeader.from_address(dmar_tbl.sub_tbl_offset)
        sub_dmar_type = sub_dmar.type
        sub_dmar_len = sub_dmar.length

        if dmar_tbl.sub_tbl_offset - addr >= dmar_tbl_header.length:
            break

        if sub_dmar_type != ACPI_DMAR_TYPE['ACPI_DMAR_TYPE_HARDWARE_UNIT']:
            dmar_tbl.sub_tbl_offset += sub_dmar.length
            continue

        # get one DRHD type in sub table
        dmar_tbl.dmar_drhd = DmarHwUnit.from_address(dmar_tbl.sub_tbl_offset)
        dmar_hw_list.hw_segment_list.append(dmar_tbl.dmar_drhd.segment)
        dmar_hw_list.hw_flags_list.append(dmar_tbl.dmar_drhd.flags)
        dmar_hw_list.hw_address_list.append(dmar_tbl.dmar_drhd.address)

        # in end of DRHD/sub tbl header is dev scope, then enumerate the device scope
        (dmar_tbl, dmar_dev_list, dmar_hw_list) = walk_dev_scope(
            dmar_tbl, dmar_dev_list, dmar_hw_list, drhd_cnt)

        dmar_dev_list.dev_scope_cnt_list.append(dmar_tbl.dev_scope_cnt)
        drhd_cnt += 1
        dmar_tbl.sub_tbl_offset += sub_dmar_len

    return (dmar_tbl, dmar_hw_list, dmar_dev_list, drhd_cnt)


def write_dmar_data(sysnode, config):
    """Write the DMAR data to board info
    :param sysnode: the system device node of acpi table, such as: /sys/firmware/acpi/tables/DMAR
    :param config: file pointer that opened for writing board information
    """

    dmar_hw_list = DmarHwList()
    dmar_dev_list = DmarDevList()
    dmar_tbl = DmarTbl()

    (dmar_tbl, dmar_hw_list, dmar_dev_list, drhd_cnt) = walk_dmar_table(
        dmar_tbl, dmar_hw_list, dmar_dev_list, sysnode)

    # num drhd and scope are hard coded
    drhd_num = 4
    scope_num = 4
    # padding dev_scope_cnt_list
    j = 0
    if len(dmar_dev_list.dev_scope_cnt_list) < scope_num:
        for j in range(scope_num - dmar_dev_list.dev_scope_cnt_list[j]):
            dmar_dev_list.dev_scope_cnt_list.append(0)

    # padding dev_bus_list/dev_path_list
    for i in range(drhd_num):
        for scope_level in range(scope_num - dmar_dev_list.dev_scope_cnt_list[i]):
            dmar_dev_list.dev_path_list.insert(i * 4 + dmar_dev_list.dev_scope_cnt_list[i], 0)
            dmar_dev_list.dev_bus_list.insert(i * 4 + dmar_dev_list.dev_scope_cnt_list[i], 0)
            scope_level = scope_level

    # padding the drhd count from hard code drhd_num
    for i in range(drhd_num - drhd_cnt):
        dmar_dev_list.dev_scope_cnt_list.append(0)
        dmar_hw_list.hw_segment_list.append(0)
        dmar_hw_list.hw_flags_list.append(0)
        dmar_hw_list.hw_address_list.append(0)
        dmar_hw_list.hw_ignore[drhd_cnt + i] = 'false'

    # output the DRHD macro
    print("\t#define DRHD_COUNT              {0}U".format(drhd_cnt), file=config)
    for drhd_hw_i in range(drhd_num):
        print("\t#define DRHD"+str(drhd_hw_i)+"_DEV_CNT           {0}U".format(
            dmar_dev_list.dev_scope_cnt_list[drhd_hw_i]), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_SEGMENT           {0}U".format(
            dmar_hw_list.hw_segment_list[drhd_hw_i]), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_FLAGS             {0}U".format(
            dmar_hw_list.hw_flags_list[drhd_hw_i]), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_REG_BASE          0x{:0>2X}UL".format(
            dmar_hw_list.hw_address_list[drhd_hw_i]), file=config)
        if drhd_hw_i in dmar_hw_list.hw_ignore.keys():
            print("\t#define DRHD"+str(drhd_hw_i)+"_IGNORE            {0}".format(
                dmar_hw_list.hw_ignore[drhd_hw_i]), file=config)
        for dev_scope_i in range(scope_num):
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_BUS     {0}U".format(hex(
                dmar_dev_list.dev_bus_list[drhd_hw_i * scope_num + dev_scope_i])),
                  file=config)
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_PATH    {0}U".format(hex(
                dmar_dev_list.dev_path_list[drhd_hw_i * scope_num + dev_scope_i])),
                  file=config)
        if drhd_hw_i in dmar_dev_list.dev_ioapic.keys():
            print("\t#define DRHD"+str(drhd_hw_i)+"_IOAPIC_ID         {0}U".format(
                dmar_dev_list.dev_ioapic[drhd_hw_i]), file=config)
