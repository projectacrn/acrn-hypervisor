# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import ctypes
from inspectorlib import external_tools

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
        ('type', ctypes.c_uint16),
        ('length', ctypes.c_uint16),
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
        self.dev_scope_id_list = []
        self.dev_scope_type_list = []

    def style_check_1(self):
        """Style check if have public method"""
        self.dev_bus_list = []

    def style_check_2(self):
        """Style check if have public method"""
        self.dev_bus_list = []


class DmarTbl:
    """DMAR TBL"""
    def __init__(self):
        self.drhd_offset = 0
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


# TODO: Get board information is independent part of acrn-config tools, it does not get the GPU_SBDF default
# config from the other part of tools, so hard code the GPU_SBDF to gernerate DRHDx_IGNORE macro
CONFIG_IGD_SBDF = 0x10

PCI_BRIDGE_HEADER = 1

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


def get_secondary_bus(dmar_tbl, tmp_dev, tmp_fun):

    cmd = "lspci -xxx"
    secondary_bus_str = ''
    found_pci_bdf = False
    pci_bridge_header_type = False
    res = external_tools.run(cmd)

    while True:
        line = res.stdout.readline().decode("ascii")

        if not line:
            break

        if ':' not in line or len(line.strip().split(":")) < 1:
            continue

        if not found_pci_bdf:
            if '.' not in line.strip():
                continue

            bus = int(line.strip().split(":")[0], 16)
            dev = int(line.strip().split()[0].split(":")[1].split(".")[0], 16)
            fun = int(line.strip().split()[0].split(":")[1].split(".")[1].strip(), 16)
            if bus == dmar_tbl.dmar_dev_scope.bus and dev == tmp_dev and fun == tmp_fun:
                found_pci_bdf = True
                continue
        else:
            if "00:" == line.strip().split()[0]:
                # PCI Header type stores in 0xE
                if len(line.split()) >= 16 and int(line.strip().split()[15], 16) & 0x7 == PCI_BRIDGE_HEADER:
                    pci_bridge_header_type = True
                    continue

            if not pci_bridge_header_type:
                continue

            # found the pci device, parse the secondary bus
            if "10:" == line.strip().split()[0]:
                # Secondary PCI BUS number stores in 0x18
                secondary_bus_str = line.split()[9]
                found_pci_bdf = False
                pci_bridge_header_type = False
                break

    # the pci device has secondary bus
    if secondary_bus_str:
        dmar_tbl.dmar_dev_scope.bus = int(secondary_bus_str, 16)


def walk_pci_bus(tmp_pdf, dmar_tbl, dmar_hw_list, drhd_cnt):
    """Walk Pci bus
    :param tmp_pdf: it is a class what contains path,device,function in dmar device scope region
    :param dmar_tbl: it is a class to describe dmar which contains Device Scope and DRHD
    :param dmar_hw_list: it is a class to describe hardware scope in DMAR table
    :param drhd_cnt: it is a counter to calculate the DRHD in DMAR table
    """
    # path offset is in end of device spcope
    dmar_tbl.path_offset = dmar_tbl.dev_scope_offset + ctypes.sizeof(DmarDevScope)
    n_cnt = (dmar_tbl.dmar_dev_scope.scope_length - ctypes.sizeof(DmarDevScope)) // 2
    while n_cnt:
        scope_path = DevScopePath.from_address(dmar_tbl.path_offset)
        tmp_pdf.device = scope_path.device
        tmp_pdf.function = scope_path.function
        tmp_pdf.path = (((tmp_pdf.device & 0x1F) << 3) | ((tmp_pdf.function & 0x7)))

        # walk the secondary pci bus
        get_secondary_bus(dmar_tbl, tmp_pdf.device, tmp_pdf.function)

        if ((dmar_tbl.dmar_drhd.segment << 16) | (
                dmar_tbl.dmar_dev_scope.bus << 8) | tmp_pdf.path) == CONFIG_IGD_SBDF:
            dmar_hw_list.hw_ignore[drhd_cnt] = 'true'

        dmar_tbl.path_offset += ctypes.sizeof(DevScopePath)
        n_cnt -= 1


def walk_dev_scope(dmar_tbl, dmar_dev_list, dmar_hw_list, drhd_cnt):
    """Walk device scope
    :param dmar_tbl: it is a class to describe dmar which contains Device Scope and DRHD
    :param dmar_dev_list: it is a class to describe device scope in DMAR table
    :param dmar_hw_list: it is a class to describe DRHD in DMAR table
    :param drhd_cnt: it is a counter to calculate the DRHD in DMAR table
    """
    dmar_tbl.dev_scope_offset = dmar_tbl.drhd_offset + ctypes.sizeof(DmarHwUnit)
    scope_end = dmar_tbl.dev_scope_offset + dmar_tbl.dmar_drhd.length
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

            # get type and id from device scope
            dmar_dev_list.dev_scope_type_list.append(dmar_tbl.dmar_dev_scope.entry_type)
            dmar_dev_list.dev_scope_id_list.append(dmar_tbl.dmar_dev_scope.enumeration_id)

            # walk the pci bus with path deep, and find the {Device,Function}
            tmp_pdf = PathDevFun()
            walk_pci_bus(tmp_pdf, dmar_tbl, dmar_hw_list, drhd_cnt)
            dmar_dev_list.dev_bus_list.append(dmar_tbl.dmar_dev_scope.bus)
            dmar_dev_list.dev_path_list.append(tmp_pdf.path)

        dmar_tbl.dev_scope_offset += dmar_tbl.dmar_dev_scope.scope_length


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
    dmar_tbl.drhd_offset = addr + ctypes.sizeof(DmarHeader)
    drhd_cnt = 0
    while True:
        # get one DRHD type in sub table
        dmar_tbl.dmar_drhd = DmarHwUnit.from_address(dmar_tbl.drhd_offset)
        dmar_type = dmar_tbl.dmar_drhd.type
        dmar_len = dmar_tbl.dmar_drhd.length

        if dmar_tbl.drhd_offset - addr >= dmar_tbl_header.length:
            break

        if dmar_type != ACPI_DMAR_TYPE['ACPI_DMAR_TYPE_HARDWARE_UNIT']:
            dmar_tbl.drhd_offset += dmar_len
            continue

        # initialize DRHDx_IGNORE to false
        dmar_hw_list.hw_ignore[drhd_cnt] = 'false'

        dmar_hw_list.hw_segment_list.append(dmar_tbl.dmar_drhd.segment)
        dmar_hw_list.hw_flags_list.append(dmar_tbl.dmar_drhd.flags)
        dmar_hw_list.hw_address_list.append(dmar_tbl.dmar_drhd.address)

        # in end of DRHD/sub tbl header is dev scope, then enumerate the device scope
        walk_dev_scope(dmar_tbl, dmar_dev_list, dmar_hw_list, drhd_cnt)
        dmar_dev_list.dev_scope_cnt_list.append(dmar_tbl.dev_scope_cnt)

        drhd_cnt += 1
        dmar_tbl.drhd_offset += dmar_len

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

    print("\t#define DRHD_COUNT              {0}U".format(drhd_cnt), file=config)
    print("", file=config)
    prev_dev_scope_num = 0
    for drhd_hw_i in range(drhd_cnt):
        dev_scope_num = dmar_dev_list.dev_scope_cnt_list[drhd_hw_i]
        print("\t#define DRHD"+str(drhd_hw_i)+"_DEV_CNT           {0}U".format(
            hex(dmar_dev_list.dev_scope_cnt_list[drhd_hw_i])), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_SEGMENT           {0}U".format(
            hex(dmar_hw_list.hw_segment_list[drhd_hw_i])), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_FLAGS             {0}U".format(
            hex(dmar_hw_list.hw_flags_list[drhd_hw_i])), file=config)
        print("\t#define DRHD"+str(drhd_hw_i)+"_REG_BASE          0x{:0>2X}UL".format(
            dmar_hw_list.hw_address_list[drhd_hw_i]), file=config)
        if drhd_hw_i in dmar_hw_list.hw_ignore.keys():
            print("\t#define DRHD"+str(drhd_hw_i)+"_IGNORE            {0}".format(
                dmar_hw_list.hw_ignore[drhd_hw_i]), file=config)
        for dev_scope_i in range(dev_scope_num):
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_TYPE    {0}U".format(
                hex(dmar_dev_list.dev_scope_type_list[prev_dev_scope_num + dev_scope_i])),
                  file=config)
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_ID      {0}U".format(
                hex(dmar_dev_list.dev_scope_id_list[prev_dev_scope_num + dev_scope_i])),
                  file=config)
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_BUS     {0}U".format(hex(
                dmar_dev_list.dev_bus_list[prev_dev_scope_num + dev_scope_i])),
                  file=config)
            print("\t#define DRHD"+str(drhd_hw_i)+"_DEVSCOPE"+str(dev_scope_i),
                  file=config, end="")
            print("_PATH    {0}U".format(hex(
                dmar_dev_list.dev_path_list[prev_dev_scope_num + dev_scope_i])),
                  file=config)

        print("", file=config)
        prev_dev_scope_num += dev_scope_num
