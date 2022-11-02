# Copyright (C) 2019-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""the tool to generate ASL code of ACPI tables for Pre-launched VMs.

"""

import sys, os, re, argparse, shutil, ctypes
from acpi_const import *
import board_cfg_lib, acrn_config_utilities
import collections
import lxml.etree
from acrn_config_utilities import get_node

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'board_inspector'))
from acpiparser._utils import TableHeader
from acpiparser import rtct
from acpiparser import rdt
from acpiparser.dsdt import parse_tree
from acpiparser.aml import builder
from acpiparser.aml.context import Context
from acpiparser.aml.visitors import GenerateBinaryVisitor, PrintLayoutVisitor

def calculate_checksum8():
    '''
    this function is implemented in iasl.
    :return:
    '''
    pass


def gen_rsdp(dest_vm_acpi_path):
    '''
    generate rsdp.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    rsdp_asl = 'rsdp.asl'
    p_xsdt_addr = r'XSDT Address : ([0-9a-fA-F]{16})'

    with open(os.path.join(dest_vm_acpi_path, rsdp_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, rsdp_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_xsdt_addr, line):
                    lines.append(re.sub(p_xsdt_addr, 'XSDT Address : {0:016X}'.format(ACPI_XSDT_ADDR), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def gen_xsdt(dest_vm_acpi_path, passthru_devices):
    '''
    generate xsdt.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param passthru_devices: dict to store passthru device list
    :return:
    '''
    xsdt_asl = 'xsdt.asl'
    p_fadt_addr = r'ACPI Table Address   0 : ([0-9a-fA-F]{16})'
    p_mcfg_addr = r'ACPI Table Address   1 : ([0-9a-fA-F]{16})'
    p_madt_addr = r'ACPI Table Address   2 : ([0-9a-fA-F]{16})'
    p_tpm2_addr = r'ACPI Table Address   3 : ([0-9a-fA-F]{16})'
    p_rtct_addr = r'ACPI Table Address   4 : ([0-9a-fA-F]{16})'

    with open(os.path.join(dest_vm_acpi_path, xsdt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, xsdt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_fadt_addr, line):
                    lines.append(re.sub(p_fadt_addr, 'ACPI Table Address   0 : {0:016X}'.format(ACPI_FADT_ADDR), line))
                elif re.search(p_mcfg_addr, line):
                    lines.append(re.sub(p_mcfg_addr, 'ACPI Table Address   1 : {0:016X}'.format(ACPI_MCFG_ADDR), line))
                elif re.search(p_madt_addr, line):
                    lines.append(re.sub(p_madt_addr, 'ACPI Table Address   2 : {0:016X}'.format(ACPI_MADT_ADDR), line))
                elif re.search(p_tpm2_addr, line):
                    if 'TPM2' in passthru_devices:
                        lines.append(re.sub(p_tpm2_addr, 'ACPI Table Address   3 : {0:016X}'.format(ACPI_TPM2_ADDR), line))
                elif re.search(p_rtct_addr, line):
                    if 'PTCT' in passthru_devices or 'RTCT' in passthru_devices:
                        lines.append(re.sub(p_rtct_addr, 'ACPI Table Address   4 : {0:016X}'.format(ACPI_RTCT_ADDR), line))
                else:
                    lines.append(line)

        dest.writelines(lines)


def gen_fadt(dest_vm_acpi_path, board_root):
    '''
    generate facp.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param board_root: the root element of board xml
    :return:
    '''
    fadt_asl = 'facp.asl'
    p_facs_addr = r'FACS Address : ([0-9a-fA-F]{8})'
    p_dsdt_addr = r'DSDT Address : ([0-9a-fA-F]{8})$'

    with open(os.path.join(dest_vm_acpi_path, fadt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, fadt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_facs_addr, line):
                    lines.append(re.sub(p_facs_addr, 'FACS Address : {0:08X}'.format(ACPI_FACS_ADDR), line))
                elif re.search(p_dsdt_addr, line):
                    lines.append(re.sub(p_dsdt_addr, 'DSDT Address : {0:08X}'.format(ACPI_DSDT_ADDR), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def gen_mcfg(dest_vm_acpi_path):
    '''
    generate mcfg.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    mcfg_asl = 'mcfg.asl'
    p_base_addr = r'Base Address : ([0-9a-fA-F]{16})'
    p_segment_group_num = r'Segment Group Number : (\d+)'
    p_start_bus_num = r'Start Bus Number : (\d+)'
    p_end_bus_num = r'End Bus Number : ([0-9a-fA-F]{2})'

    with open(os.path.join(dest_vm_acpi_path, mcfg_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, mcfg_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_base_addr, line):
                    lines.append(re.sub(p_base_addr, 'Base Address : {0:016X}'.format(VIRT_PCI_MMCFG_BASE), line))
                elif re.search(p_segment_group_num, line):
                    lines.append(re.sub(p_segment_group_num, 'Segment Group Number : {0:04X}'.format(0), line))
                elif re.search(p_start_bus_num, line):
                    lines.append(re.sub(p_start_bus_num, 'Start Bus Number : {0:02X}'.format(0), line))
                elif re.search(p_end_bus_num, line):
                    lines.append(re.sub(p_end_bus_num, 'End Bus Number : {0:02X}'.format(0xff), line))
                else:
                    lines.append(line)
        dest.writelines(lines)

def gen_madt(dest_vm_acpi_path, max_cpu_num, apic_ids):
    '''
    generate apic.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :return:
    '''
    madt_asl = 'apic.asl'

    lapic_index = 0
    p_lapic_addr = r'Local Apic Address : ([0-9a-fA-F]{8})'
    p_flags = r'\[0004\]        Flags (decoded below) : (\d{8})'  # dup flags
    flags_index = 0

    p_lapic_index = 0
    p_lapic_type = r'Subtable Type : (\d+) \[Processor Local APIC\]'
    p_lapic_len = r'\[0001\]                       Length : ([0-9a-fA-F]{2})'  # dup len
    p_lapic_len_index = 0
    p_lapic_flags_index = 0
    p_lapic_process_id = r'\[0001\]                 Processor ID : (\d+)'   # dup processor
    p_lapic_process_id_index = 0
    p_lapic_id = r'Local Apic ID : ([0-9a-fA-F]{2})'
    p_lapic_line_index = 0
    lapic_lines = []

    ioapic_index = 0
    p_ioapic_type = r'Subtable Type : (\d+) \[I/O APIC\]'
    p_ioapic_len_index = 0
    p_ioapic_id = r'I/O Apic ID : (\d+)'
    p_ioapic_addr = r'\[0004\]                      Address : ([0-9a-fA-F]{8})'

    lapic_nmi_index = 0
    p_lapic_nmi_type = r'Subtable Type : (\d+) \[Local APIC NMI\]'
    p_lapic_nmi_len_index = 0
    p_lapic_nmi_processor_id_index = 0
    p_lapic_nmi_flags = r'\[0002\]        Flags (decoded below) : ([0-9a-fA-F]{4})'
    p_lapic_nmi_flags_index = 0
    p_lapic_nmi_lint = r'Interrupt Input LINT : (\d+)'

    with open(os.path.join(dest_vm_acpi_path, madt_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, madt_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_lapic_addr, line):
                    lapic_index += 1
                    lines.append(re.sub(p_lapic_addr, 'Local Apic Address : {0:08X}'.format(0xFEE00000), line))
                elif re.search(p_flags, line):
                    if lapic_index == 1 and flags_index == 0:
                        lines.append(
                            re.sub(p_flags, '[0004]        Flags (decoded below) : {0:08X}'.format(0x1), line))
                        flags_index += 1
                    elif p_lapic_index == 1 and p_lapic_flags_index == 0:
                        lines.append(
                            re.sub(p_flags, '[0004]        Flags (decoded below) : {0:08X}'.format(0x1),
                                   line))
                        p_lapic_flags_index += 1
                    else:
                        lines.append(line)

                elif re.search(p_lapic_type, line):
                    p_lapic_index += 1
                    if lapic_index == 1:
                        lines.append(re.sub(p_lapic_type, 'Subtable Type : {0:02X} [Processor Local APIC]'.format(
                            ACPI_MADT_TYPE_LOCAL_APIC), line))
                    else:
                        lines.append(line)
                elif re.search(p_lapic_len, line):
                    if p_lapic_index == 1 and p_lapic_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x8),
                                   line))
                        p_lapic_len_index += 1
                    elif ioapic_index == 1 and p_ioapic_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x0C),
                                   line))
                        p_ioapic_len_index += 1
                    elif lapic_nmi_index == 1 and p_lapic_nmi_len_index == 0:
                        lines.append(
                            re.sub(p_lapic_len, '[0001]                       Length : {0:02X}'.format(0x06),
                                   line))
                        p_lapic_nmi_len_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_process_id, line):
                    if p_lapic_index == 1 and p_lapic_process_id_index == 0:
                        lines.append(re.sub(p_lapic_process_id,
                                            '[0001]                 Processor ID : {0:02X}'.format(0x0),
                                            line))
                        p_lapic_process_id_index += 1
                    elif lapic_nmi_index == 1 and p_lapic_nmi_processor_id_index == 0:
                        lines.append(
                            re.sub(p_lapic_process_id,
                                   '[0001]                 Processor ID : {0:02X}'.format(0xFF),
                                   line))
                        p_lapic_nmi_processor_id_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_id, line):
                    lines.append(re.sub(p_lapic_id, 'Local Apic ID : {0:02X}'.format(apic_ids[0]), line))

                elif re.search(p_ioapic_type, line):
                    ioapic_index += 1
                    lines.append(
                        re.sub(p_ioapic_type, 'Subtable Type : {0:02X} [I/O APIC]'.format(ACPI_MADT_TYPE_IOAPIC), line))
                elif re.search(p_ioapic_id, line):
                    lines.append(re.sub(p_ioapic_id, 'I/O Apic ID : {0:02X}'.format(0x01), line))
                elif re.search(p_ioapic_addr, line):
                    lines.append(re.sub(p_ioapic_addr,
                                        '[0004]                      Address : {0:02X}'.format(VIOAPIC_BASE),
                                        line))

                elif re.search(p_lapic_nmi_type, line):
                    lapic_nmi_index += 1
                    if lapic_nmi_index == 1:
                        lines.append(re.sub(p_lapic_nmi_type, 'Subtable Type : {0:02X} [Local APIC NMI]'.format(
                            ACPI_MADT_TYPE_LOCAL_APIC_NMI), line))
                    else:
                        lines.append(line)
                elif re.search(p_lapic_nmi_flags, line):
                    if lapic_nmi_index == 1 and p_lapic_nmi_flags_index == 0:
                        lines.append(
                            re.sub(p_lapic_nmi_flags, '[0002]        Flags (decoded below) : {0:04X}'.format(0x5),
                                   line))
                        p_lapic_nmi_flags_index += 1
                    else:
                        lines.append(line)
                elif re.search(p_lapic_nmi_lint, line):
                    if lapic_nmi_index == 1:
                        lines.append(re.sub(p_lapic_nmi_lint, 'Interrupt Input LINT : {0:02X}'.format(0x1), line))
                    else:
                        lines.append(line)
                else:
                    lines.append(line)

                if p_lapic_index == 1 and p_lapic_line_index < 7:
                    lapic_lines.append(line)
                    p_lapic_line_index += 1
                if p_lapic_index == 1 and p_lapic_line_index == 7:
                    p_lapic_line_index = 0
                    for process_id in range(1, max_cpu_num):
                        p_lapic_index = process_id + 1
                        lines.append('\n')
                        for lapic_line in lapic_lines:
                            if re.search(p_lapic_type, lapic_line):
                                lines.append(re.sub(p_lapic_type,
                                                    'Subtable Type : {0:02X} [Processor Local APIC]'.format(
                                                        ACPI_MADT_TYPE_LOCAL_APIC), lapic_line))
                            elif re.search(p_lapic_len, lapic_line):
                                lines.append(
                                    re.sub(p_lapic_len,
                                           '[0001]                       Length : {0:02X}'.format(0x8),
                                           lapic_line))
                            elif re.search(p_flags, lapic_line):
                                lines.append(
                                    re.sub(p_flags,
                                           '[0004]              Flags (decoded below) : {0:08X}'.format(0x1),
                                           lapic_line))
                            elif re.search(p_lapic_process_id, lapic_line):
                                lines.append(re.sub(p_lapic_process_id,
                                                    '[0001]                 Processor ID : {0:02X}'.format(
                                                        process_id), lapic_line))
                            elif re.search(p_lapic_id, lapic_line):
                                lines.append(
                                    re.sub(p_lapic_id, 'Local Apic ID : {0:02X}'.format(apic_ids[process_id]), lapic_line))
                            else:
                                lines.append(lapic_line)

        dest.writelines(lines)


def gen_tpm2(dest_vm_acpi_path, passthru_devices):
    '''
    generate tpm2.asl
    :param dest_vm_acpi_path: the path to store generated ACPI asl code
    :param passthru_devices: dict to store passthru device list
    :return:
    '''
    tpm2_asl = 'tpm2.asl'
    p_control_addr = r'Control Address : ([0-9a-fA-F]{16})'
    p_start_method = r'Start Method : (.*)'

    if 'TPM2' not in passthru_devices:
        if os.path.isfile(os.path.join(dest_vm_acpi_path, tpm2_asl)):
            os.remove(os.path.join(dest_vm_acpi_path, tpm2_asl))
        return

    with open(os.path.join(dest_vm_acpi_path, tpm2_asl), 'w') as dest:
        lines = []
        with open(os.path.join(TEMPLATE_ACPI_PATH, tpm2_asl), 'r') as src:
            for line in src.readlines():
                if re.search(p_control_addr, line):
                    lines.append(re.sub(p_control_addr, 'Control Address : {0:016X}'.format(0xFED40040), line))
                elif re.search(p_start_method, line):
                    lines.append(re.sub(p_start_method, 'Start Method : {0:02X}'.format(0x7), line))
                else:
                    lines.append(line)
        dest.writelines(lines)


def encode_eisa_id(s):
    chars = list(map(lambda x: (ord(x) - 0x40) & 0x1F, s[0:3]))
    digits = list(map(lambda x: int(x, 16), s[3:7]))
    encoded = [
        (chars[0] << 2) | (chars[1] >> 3),     # Bit 6:2 is char[0]; Bit 1:0 is the higher 2 bits of char[1].
        ((chars[1] & 0x7) << 5) | (chars[2]),  # Bit 7:5 is the lower 3 bits of char[1]; Bit 4:0 is char[2].
        (digits[0] << 4) | (digits[1]),        # Bit 7:4 is digits[0]; Bit 3:0 is digits[1]
        (digits[2] << 4) | (digits[3]),        # Bit 7:4 is digits[2]; Bit 3:0 is digits[2]
    ]
    return int.from_bytes(bytes(encoded), sys.byteorder)

def create_object(cls, **kwargs):
    length = ctypes.sizeof(cls)
    data = bytearray(length)
    obj = cls.from_buffer(data)
    for key, value in kwargs.items():
        setattr(obj, key, value)
    return obj

def gen_root_pci_bus(path, prt_packages):
    resources = []

    # Bus number
    word_address_space_cls = rdt.LargeResourceItemWordAddressSpace_factory()
    res = create_object(
        word_address_space_cls,
        type   = 1,    # Large type
        name   = rdt.LARGE_RESOURCE_ITEM_WORD_ADDRESS_SPACE,
        length = ctypes.sizeof(word_address_space_cls) - 3,
        _TYP   = 2,    # Bus number range
        _DEC   = 0,    # Positive decoding
        _MIF   = 1,    # Minimum address fixed
        _MAF   = 1,    # Maximum address fixed
        flags  = 0,
        _MAX   = 0xff,
        _LEN   = 0x100
    )
    resources.append(res)

    # The PCI hole below 4G
    dword_address_space_cls = rdt.LargeResourceItemDWordAddressSpace_factory()
    res = create_object(
        dword_address_space_cls,
        type   = 1,    # Large type
        name   = rdt.LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE,
        length = ctypes.sizeof(dword_address_space_cls) - 3,
        _TYP   = 0,    # Memory range
        _DEC   = 0,    # Positive decoding
        _MIF   = 1,    # Minimum address fixed
        _MAF   = 1,    # Maximum address fixed
        flags  = 1,    # read-write, non-cachable, TypeStatic
        _MIN   = 0x80000000,
        _MAX   = 0xdfffffff,
        _LEN   = 0x60000000
    )
    resources.append(res)

    # The PCI hole above 4G
    res = create_object(
        dword_address_space_cls,
        type   = 1,    # Large type
        name   = rdt.LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE,
        length = ctypes.sizeof(dword_address_space_cls) - 3,
        _TYP   = 0,    # Memory range
        _DEC   = 0,    # Positive decoding
        _MIF   = 1,    # Minimum address fixed
        _MAF   = 1,    # Maximum address fixed
        flags  = 1,    # read-write, non-cachable, TypeStatic
        _MIN = 0x4000000000,
        _MAX = 0x7fffffffff,
        _LEN = 0x4000000000
    )
    resources.append(res)

    # The PCI hole for I/O ports
    res = create_object(
        dword_address_space_cls,
        type   = 1,    # Large type
        name   = rdt.LARGE_RESOURCE_ITEM_ADDRESS_SPACE_RESOURCE,
        length = ctypes.sizeof(dword_address_space_cls) - 3,
        _TYP   = 1,    # I/O range
        _DEC   = 0,    # Positive decoding
        _MIF   = 1,    # Minimum address fixed
        _MAF   = 1,    # Maximum address fixed
        flags  = 3,    # Entire range, TypeStatic
        _MIN = 0x0,
        _MAX = 0xffff,
        _LEN = 0x10000
    )
    resources.append(res)

    # End tag
    resources.append(bytes([0x79, 0]))
    resource_buf = bytearray().join(map(bytearray, resources))
    checksum = (256 - (sum(resource_buf) % 256)) % 256
    resource_buf[-1] = checksum

    # Device object for the root PCI bus
    tree = builder.DefDevice(
        builder.PkgLength(),
        path,
        builder.TermList(
            builder.DefName(
                "_HID",
                builder.DWordConst(encode_eisa_id("PNP0A08"))),
            builder.DefName(
                "_CID",
                builder.DWordConst(encode_eisa_id("PNP0A03"))),
            builder.DefName(
                "_BBN",
                builder.ZeroOp()),
            builder.DefName(
                "_UID",
                builder.ZeroOp()),
            builder.DefName(
                "_CRS",
                builder.DefBuffer(
                    builder.PkgLength(),
                    builder.WordConst(len(resource_buf)),
                    builder.ByteList(resource_buf))),
            builder.DefName(
                "_PRT",
                builder.DefPackage(
                    builder.PkgLength(),
                    builder.ByteData(len(prt_packages)),
                    builder.PackageElementList(*prt_packages)))))

    return tree

def pnp_uart(path, uid, ddn, port, irq):
    resources = []

    res = create_object(
        rdt.SmallResourceItemIOPort,
        type   = 0,
        name   = rdt.SMALL_RESOURCE_ITEM_IO_PORT,
        length = ctypes.sizeof(rdt.SmallResourceItemIOPort) - 1,
        _DEC   = 1,
        _MIN   = port,
        _MAX   = port,
        _ALN   = 1,
        _LEN   = 8
    )
    resources.append(res)

    cls = rdt.SmallResourceItemIRQ_factory()
    res = create_object(
        cls,
        type   = 0,
        name   = rdt.SMALL_RESOURCE_ITEM_IRQ_FORMAT,
        length = ctypes.sizeof(cls) - 1,
        _INT   = 1 << irq
    )
    resources.append(res)

    resources.append(bytes([0x79, 0]))

    resource_buf = bytearray().join(map(bytearray, resources))
    checksum = (256 - (sum(resource_buf) % 256)) % 256
    resource_buf[-1] = checksum
    uart = builder.DefDevice(
        builder.PkgLength(),
        path,
        builder.TermList(
            builder.DefName(
                "_HID",
                builder.DWordConst(encode_eisa_id("PNP0501"))),
            builder.DefName(
                "_UID",
                builder.build_value(uid)),
            builder.DefName(
                "_DDN",
                builder.String(ddn)),
            builder.DefName(
                "_CRS",
                builder.DefBuffer(
                    builder.PkgLength(),
                    builder.WordConst(len(resource_buf)),
                    builder.ByteList(resource_buf)))))

    return uart

def pnp_rtc(path):
    resources = []

    res = create_object(
        rdt.SmallResourceItemIOPort,
        type   = 0,
        name   = rdt.SMALL_RESOURCE_ITEM_IO_PORT,
        length = ctypes.sizeof(rdt.SmallResourceItemIOPort) - 1,
        _DEC   = 1,
        _MIN   = 0x70,
        _MAX   = 0x70,
        _ALN   = 1,
        _LEN   = 8
    )
    resources.append(res)

    cls = rdt.SmallResourceItemIRQ_factory()
    res = create_object(
        cls,
        type   = 0,
        name   = rdt.SMALL_RESOURCE_ITEM_IRQ_FORMAT,
        length = ctypes.sizeof(cls) - 1,
        _INT   = 1 << 8
    )
    resources.append(res)

    resources.append(bytes([0x79, 0]))

    resource_buf = bytearray().join(map(bytearray, resources))
    checksum = (256 - (sum(resource_buf) % 256)) % 256
    resource_buf[-1] = checksum
    rtc = builder.DefDevice(
        builder.PkgLength(),
        path,
        builder.TermList(
            builder.DefName(
                "_HID",
                builder.DWordConst(encode_eisa_id("PNP0B00"))),
            builder.DefName(
                "_CRS",
                builder.DefBuffer(
                    builder.PkgLength(),
                    builder.WordConst(len(resource_buf)),
                    builder.ByteList(resource_buf)))))

    return rtc

def collect_dependent_devices(board_etree, device_node):
    types_in_scope = ["uses", "is used by", "consumes resources from"]
    result = set()
    queue = [device_node]

    while queue:
        device = queue.pop()
        if device not in result:
            result.add(device)
            for node in device.findall("dependency"):
                if node.get("type") in types_in_scope:
                    peer_device = get_node(f"//device[acpi_object='{node.text}']", board_etree)
                    if peer_device is not None:
                        queue.append(peer_device)

    result.discard(device_node)
    return result

def add_or_replace_object(device_object, new_tree):
    name = new_tree.NameString.value
    objects_tree = device_object.TermList
    for idx, object_tree in enumerate(objects_tree.children):
        if object_tree.NameString.value == name:
            objects_tree.children[idx] = new_tree
            return
    objects_tree.append_child(new_tree)

class ObjectCollector:
    def __init__(self):
        self.__objects = collections.defaultdict(lambda: [])

    @staticmethod
    def __discard_external_objects(tree):
        objects_tree = tree.TermList
        objects_tree.children = list(filter(lambda x: x.label != "DefExternal", objects_tree.children))
        return tree

    def add_device_object(self, device_object):
        device_path = device_object.NameString.value
        scope = Context.parent(device_path)
        device_name = device_path[-4:]
        device_object.NameString.value = device_name
        self.__objects[scope].append(self.__discard_external_objects(device_object))

    def add_object(self, scope, obj):
        self.__objects[scope].append(obj)

    def __get_scope_contents(self, termlist, path):
        scopes = [i for i in path.lstrip("\\").split(".") if i]
        ret = termlist
        for scope in scopes:
            for term in ret:
                if term.label in ["DefScope", "DefDevice"] and term.NameString.value == scope:
                    ret = term.TermList.children
                    break
            else:
                tree = builder.DefScope(builder.PkgLength(), scope, builder.TermList())
                ret.append(tree)
                ret = tree.TermList.children
        return ret

    def get_term_list(self):
        acc = []

        for scope, objects in sorted(self.__objects.items(), key=lambda p:p[0]):
            if scope.startswith("\\"):
                self.__get_scope_contents(acc, scope).extend(objects)
            else:
                print(f"Relative scope is unexpected: {scope}. The objects will be added to the root scope.")
                acc.extend(objects)

        return acc

def gen_dsdt(board_etree, scenario_etree, allocation_etree, vm_id, dest_path):
    interrupt_pin_ids = {
        "INTA#": 0,
        "INTB#": 1,
        "INTC#": 2,
        "INTD#": 3,
    }

    header = builder.DefBlockHeader(
        int.from_bytes(bytearray("DSDT", "ascii"), sys.byteorder),     # Signature
        0x0badbeef,                                                    # Length, will calculate later
        3,                                                             # Revision
        0,                                                             # Checksum, will calculate later
        int.from_bytes(bytearray("ACRN  ", "ascii"), sys.byteorder),   # OEM ID
        int.from_bytes(bytearray("ACRNDSDT", "ascii"), sys.byteorder), # OEM Table ID
        1,                                                             # OEM Revision
        int.from_bytes(bytearray("INTL", "ascii"), sys.byteorder),     # Compiler ID
        0x20190703,                                                    # Compiler Version
    )

    objects = ObjectCollector()
    prt_packages = []

    pci_dev_regex = re.compile(r"([0-9a-f]{2}):([0-1][0-9a-f]{1}).([0-7]) .*")
    for pci_dev in scenario_etree.xpath(f"//vm[@id='{vm_id}']/pci_devs/pci_dev/text()"):
        m = pci_dev_regex.match(pci_dev)
        if m:
            device_number = int(m.group(2), 16)
            function_number = int(m.group(3))
            bus_number = int(m.group(1), 16)
            bdf = f"{bus_number:02x}:{device_number:02x}.{function_number}"
            address = hex((device_number << 16) | (function_number))
            device_node = get_node(f"//bus[@address='{hex(bus_number)}']/device[@address='{address}']", board_etree)
            alloc_node = get_node(f"/acrn-config/vm[@id='{vm_id}']/device[@name='PTDEV_{bdf}']", allocation_etree)
            if device_node is not None and alloc_node is not None:
                assert int(alloc_node.find("bus").text, 16) == 0, "Virtual PCI devices must be on bus 0."
                vdev = int(alloc_node.find("dev").text, 16)
                vfunc = int(alloc_node.find("func").text, 16)

                # The AML object template, with _ADR replaced to vBDF
                template = device_node.find("aml_template")
                if template is not None:
                    tree = parse_tree("DefDevice", bytes.fromhex(template.text))
                    vaddr = (vdev << 16) | vfunc
                    add_or_replace_object(tree,
                        builder.DefName("_ADR", builder.build_value(vaddr)))
                    objects.add_device_object(tree)

                # The _PRT remapping package, if necessary
                intr_pin_node = get_node("resource[@type='interrupt_pin']", device_node)
                virq_node = get_node("pt_intx", alloc_node)
                if intr_pin_node is not None and virq_node is not None:
                    pin_id = interrupt_pin_ids[intr_pin_node.get("pin")]
                    vaddr = (vdev << 16) | 0xffff
                    pirq = int(intr_pin_node.get("source", -1))
                    virq_mapping = dict(eval(f"[{virq_node.text.replace(' ','').replace(')(', '), (')}]"))
                    if pirq in virq_mapping.keys():
                        virq = virq_mapping[pirq]
                        prt_packages.append(
                            builder.DefPackage(
                                builder.PkgLength(),
                                builder.ByteData(4),
                                builder.PackageElementList(
                                    builder.build_value(vaddr),
                                    builder.build_value(pin_id),
                                    builder.build_value(0),
                                    builder.build_value(virq))))

                for peer_device_node in collect_dependent_devices(board_etree, device_node):
                    template = peer_device_node.find("aml_template")
                    if template is not None:
                        tree = parse_tree("DefDevice", bytes.fromhex(template.text))
                        objects.add_device_object(tree)

    root_pci_bus = board_etree.xpath("//bus[@type='pci' and @address='0x0']")
    if root_pci_bus:
        acpi_object = root_pci_bus[0].find("acpi_object")
        if acpi_object is not None:
            path = acpi_object.text
            objects.add_device_object(gen_root_pci_bus(path, prt_packages))

    # If TPM is assigned to the VM, copy the TPM2 device object to vACPI as well.
    #
    # FIXME: Today the TPM2 MMIO registers are always located at 0xFED40000 with length 0x5000. The same address is used
    # as the guest physical address of the passed through TPM2. Thus, it works for now to reuse the host TPM2 device
    # object without updating the addresses of operation regions or resource descriptors. It is, however, necessary to
    # introduce a pass to translate such address to arbitrary guest physical ones in the future.
    has_tpm2 = get_node(f"//vm[@id='{vm_id}']//TPM2/text()", scenario_etree)
    if has_tpm2 == "y":
        # TPM2 devices should have "MSFT0101" as hardware id or compatible ID
        template = get_node("//device[@id='MSFT0101']/aml_template", board_etree)
        if template is None:
            template = get_node("//device[compatible_id='MSFT0101']/aml_template", board_etree)
        if template is not None:
            tree = parse_tree("DefDevice", bytes.fromhex(template.text))
            objects.add_device_object(tree)

    s5_object = builder.DefName(
        "_S5_",
        builder.DefPackage(
            builder.PkgLength(),
            2,
            builder.PackageElementList(
                builder.build_value(5),
                builder.build_value(0))))
    objects.add_object("\\", s5_object)

    rtvm = bool(scenario_etree.xpath(f"//vm[@id='{vm_id}']//lapic_passthrough[text()='y']"))
    # RTVM cannot set IRQ because no INTR is sent with LAPIC PT
    if rtvm is False:
        objects.add_object("\\_SB_", pnp_uart("UAR0", 0, "COM1", 0x3f8, 4))
        objects.add_object("\\_SB_", pnp_uart("UAR1", 1, "COM2", 0x2f8, 3))

    objects.add_object("\\_SB_", pnp_rtc("RTC0"))

    amlcode = builder.AMLCode(header, *objects.get_term_list())
    with open(dest_path, "wb") as dest:
        visitor = GenerateBinaryVisitor()
        binary = bytearray(visitor.generate(amlcode))

        # Overwrite length
        binary[4:8] = len(binary).to_bytes(4, sys.byteorder)

        # Overwrite checksum
        checksum = (256 - (sum(binary) % 256)) % 256
        binary[9] = checksum

        dest.write(binary)

def gen_rtct(board_etree, scenario_etree, allocation_etree, vm_id, dest_path):
    def cpu_id_to_lapic_id(cpu_id):
        return get_node(f"//thread[cpu_id = '{cpu_id}']/apic_id/text()", board_etree)

    vm_node = get_node(f"//vm[@id='{vm_id}']", scenario_etree)
    if vm_node is None:
        return False

    vcpus = ",".join(map(cpu_id_to_lapic_id, vm_node.xpath("cpu_affinity//pcpu_id/text()")))
    rtct_entries = []

    # ACPI table header

    common_header = create_object(
        TableHeader,
        signature       = b'RTCT',
        revision        = 1,
        oemid           = b'ACRN  ',
        oemtableid      = b'ACRNRTCT',
        oemrevision     = 5,
        creatorid       = b'INTL',
        creatorrevision = 0x100000d
    )
    rtct_entries.append(common_header)

    # Compatibility entry

    compat_entry = create_object(
        rtct.RTCTSubtableCompatibility,
        subtable_size      = ctypes.sizeof(rtct.RTCTSubtableCompatibility),
        format_or_version  = 1,
        type               = rtct.ACPI_RTCT_TYPE_COMPATIBILITY,
        rtct_version_major = 2,
        rtct_version_minor = 0,
        rtcd_version_major = 0,
        rtcd_version_minor = 0
    )
    rtct_entries.append(compat_entry)

    # SSRAM entries

    # Look for the cache blocks that are visible to this VM and have software SRAM in it. Those software SRAM will be
    # exposed to the VM in RTCT.
    for cache in board_etree.xpath(f"//caches/cache[count(processors/processor[contains('{vcpus}', .)]) and capability[@id = 'Software SRAM']]"):
        ssram_cap = get_node("capability[@id = 'Software SRAM']", cache)

        ssram_entry = create_object(
            rtct.RTCTSubtableSoftwareSRAM_v2,
            subtable_size     = ctypes.sizeof(rtct.RTCTSubtableSoftwareSRAM_v2),
            format_or_version = 2,
            type              = rtct.ACPI_RTCT_V2_TYPE_SoftwareSRAM,
            level             = int(cache.get("level")),
            cache_id          = int(cache.get("id"), base=16),
            base              = int(ssram_cap.find("start").text, base=16),
            size              = int(ssram_cap.find("size").text),
            shared            = 0
        )
        rtct_entries.append(ssram_entry)

        if ssram_cap.find("waymask") is not None:
            ssram_waymask_entry = create_object(
                rtct.RTCTSubtableSSRAMWayMask,
                subtable_size     = ctypes.sizeof(rtct.RTCTSubtableSSRAMWayMask),
                format_or_version = 1,
                type              = rtct.ACPI_RTCT_V2_TYPE_SSRAM_WayMask,
                level             = int(cache.get("level")),
                cache_id          = int(cache.get("id"), base=16),
                waymask           = int(ssram_cap.find("waymask").text, base=16)
            )
            rtct_entries.append(ssram_waymask_entry)

    with open(dest_path, "wb") as dest:
        length = sum(map(ctypes.sizeof, rtct_entries))
        common_header.length = length
        binary = bytearray().join(map(bytearray, rtct_entries))

        checksum = (256 - (sum(binary) % 256)) % 256
        binary[9] = checksum

        dest.write(binary)

def main(args):

    err_dic = {}

    (err_dic, params) = acrn_config_utilities.get_param(args)
    if err_dic:
        return err_dic

    board = params['--board']
    scenario= params['--scenario']
    out = params['--out']

    board_etree = lxml.etree.parse(board)
    board_root = board_etree.getroot()
    scenario_etree = lxml.etree.parse(scenario)
    scenario_root = scenario_etree.getroot()
    allocation_etree = lxml.etree.parse(os.path.join(os.path.dirname(board), "configs", "allocation.xml"))
    board_type = board_root.attrib['board']
    scenario_name = scenario_root.attrib['scenario']
    pcpu_list = board_root.find('CPU_PROCESSOR_INFO').text.strip().split(',')
    if isinstance(pcpu_list, list):
        pcpu_list = [x.strip() for x in pcpu_list]
    if out is None or out == '':
        DEST_ACPI_PATH = os.path.join(VM_CONFIGS_PATH, 'scenarios', scenario_name)
    else:
        DEST_ACPI_PATH = os.path.join(acrn_config_utilities.SOURCE_ROOT_DIR, out, 'scenarios', scenario_name)

    if os.path.isdir(DEST_ACPI_PATH):
        for config in os.listdir(DEST_ACPI_PATH):
            if config.startswith('ACPI_VM') and os.path.isdir(os.path.join(DEST_ACPI_PATH, config)):
                shutil.rmtree(os.path.join(DEST_ACPI_PATH, config))

    dict_passthru_devices = collections.OrderedDict()
    dict_pcpu_list = collections.OrderedDict()
    for vm in scenario_root.findall('vm'):
        vm_id = vm.attrib['id']
        load_order_node = vm.find('load_order')
        if (load_order_node is not None) and (load_order_node.text == 'PRE_LAUNCHED_VM'):
            dict_passthru_devices[vm_id] = []
            for pci_dev_node in vm.findall('pci_devs/pci_dev'):
                if pci_dev_node is not None and pci_dev_node.text is not None and pci_dev_node.text.strip():
                    dict_passthru_devices[vm_id].append(pci_dev_node.text)
            mmio_dev_nodes = vm.find('mmio_resources')
            if mmio_dev_nodes is not None:
                for mmio_dev_node in list(mmio_dev_nodes):
                    if mmio_dev_node is not None and mmio_dev_node.text.strip() == 'y':
                        dict_passthru_devices[vm_id].append(mmio_dev_node.tag)
            dict_pcpu_list[vm_id] = []
            for pcpu_id in vm.findall('cpu_affinity//pcpu_id'):
                if pcpu_id is not None and pcpu_id.text.strip() in pcpu_list:
                    dict_pcpu_list[vm_id].append(int(pcpu_id.text))

    PASSTHROUGH_RTCT = False
    PRELAUNCHED_RTVM_ID = None
    try:
        if scenario_root.find('hv/FEATURES/SSRAM/SSRAM_ENABLED').text.strip() == 'y':
            PASSTHROUGH_RTCT = True
        for vm in scenario_root.findall('vm'):
            vm_id = vm.attrib['id']
            vm_type_node = vm.find('vm_type')
            load_order_node = vm.find('load_order')
            if (load_order_node is not None) and (load_order_node.text == 'PRE_LAUNCHED_VM') and (vm_type_node.text == 'RTVM'):
                PRELAUNCHED_RTVM_ID = vm_id
                break
    except:
        PASSTHROUGH_RTCT = False

    kern_args = acrn_config_utilities.get_leaf_tag_map(scenario, "os_config", "bootargs")
    kern_type = acrn_config_utilities.get_leaf_tag_map(scenario, "os_config", "kern_type")
    for vm_id, passthru_devices in dict_passthru_devices.items():
        bootargs_node= get_node(f"//vm[@id='{vm_id}']/os_config/bootargs", scenario_etree)
        if bootargs_node is not None and kern_args[int(vm_id)].find('reboot=acpi') == -1 and kern_type[int(vm_id)] in ['KERNEL_BZIMAGE']:
            emsg = "you need to specify 'reboot=acpi' in scenario file's bootargs for VM{}".format(vm_id)
            print(emsg)
            err_dic['vm,bootargs'] = emsg
            break

        print('start to generate ACPI ASL code for VM{}'.format(vm_id))
        dest_vm_acpi_path = os.path.join(DEST_ACPI_PATH, 'ACPI_VM'+vm_id)
        if not os.path.isdir(dest_vm_acpi_path):
            os.makedirs(dest_vm_acpi_path)
        if PASSTHROUGH_RTCT is True and vm_id == PRELAUNCHED_RTVM_ID:
            passthru_devices.append("RTCT")
            gen_rtct(board_etree, scenario_etree, allocation_etree, vm_id, os.path.join(dest_vm_acpi_path, "rtct.aml"))
        gen_rsdp(dest_vm_acpi_path)
        gen_xsdt(dest_vm_acpi_path, passthru_devices)
        gen_fadt(dest_vm_acpi_path, board_root)
        gen_mcfg(dest_vm_acpi_path)
        if vm_id in dict_pcpu_list:
            dict_pcpu_list[vm_id].sort()

            apic_ids = []
            for id in dict_pcpu_list[vm_id]:
                apic_id = get_node(f"//processors//thread[cpu_id='{id}']/apic_id/text()", board_etree)
                if apic_id is None:
                    emsg = 'some or all of the processors//thread/cpu_id tags are missing in board xml file for cpu {}, please run board_inspector.py to regenerate the board xml file!'.format(id)
                    print(emsg)
                    err_dic['board config: processors'] = emsg
                    return err_dic
                else:
                    apic_ids.append(int(apic_id, 16))

            gen_madt(dest_vm_acpi_path, len(dict_pcpu_list[vm_id]), apic_ids)
            gen_tpm2(dest_vm_acpi_path, passthru_devices)
            gen_dsdt(board_etree, scenario_etree, allocation_etree, vm_id, os.path.join(dest_vm_acpi_path, "dsdt.aml"))
            print('generate ASL code of ACPI tables for VM {} into {}'.format(vm_id, dest_vm_acpi_path))
        else:
            emsg = 'no cpu affinity config for VM {}'.format(vm_id)
            print(emsg)
            err_dic['vm,cpu_affinity,pcpu_id'] = emsg

    return err_dic


if __name__ == '__main__':

    main(sys.argv)
