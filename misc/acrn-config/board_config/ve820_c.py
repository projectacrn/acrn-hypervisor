# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import board_cfg_lib


TOTAL_MEM_SIZE = 4 * 1024 * 1024 * 1024
LOW_MEM_TO_PCI_HOLE = 0x20000000


def ve820_per_launch(config, hpa_size):
    """
    Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    (err_dic, board_name) = board_cfg_lib.get_board_name()
    if err_dic:
        return err_dic

    board_name = board_cfg_lib.undline_name(board_name)

    low_mem_to_pci_hole_len = '0xA0000000'
    low_mem_to_pci_hole = '0x20000000'
    pci_hole_addr = '0xe0000000'
    pci_hole_len = '0x20000000'
    start_low_hpa = 0x100000
    hpa_len = int(hpa_size, 16) - 1 * 1024 * 1024

    # pre_launch memroy: mem_size is the ve820 length
    print("#include <e820.h>", file=config)
    print("#include <vm.h>", file=config)
    print("", file=config)
    print("#define VE820_ENTRIES_{}\t{}U".format(board_name, 5), file=config)
    print("static const struct e820_entry ve820_entry[{}] = {{".format(
        "VE820_ENTRIES_{}".format(board_name)), file=config)
    print("\t{\t/* usable RAM under 1MB */", file=config)
    print("\t\t.baseaddr = 0x0UL,", file=config)
    print("\t\t.length   = 0xF0000UL,\t\t/* 960KB */", file=config)
    print("\t\t.type     = E820_TYPE_RAM", file=config)
    print("\t},", file=config)
    print("", file=config)
    print("\t{\t/* mptable */", file=config)
    print("\t\t.baseaddr = 0xF0000UL,\t\t/* 960KB */", file=config)
    print("\t\t.length   = 0x10000UL,\t\t/* 16KB */", file=config)
    print("\t\t.type     = E820_TYPE_RESERVED", file=config)
    print("\t},", file=config)
    print("", file=config)

    print("\t{\t/* lowmem */", file=config)

    print("\t\t.baseaddr = {}UL,\t\t/* 1MB */".format(
        hex(start_low_hpa)), file=config)
    print("\t\t.length   = {}UL,\t/* {}MB */".format(
        hex(hpa_len), hpa_len / 1024 / 1024), file=config)

    print("\t\t.type     = E820_TYPE_RAM", file=config)
    print("\t},", file=config)
    print("", file=config)

    print("\t{\t/* between lowmem and PCI hole */", file=config)
    print("\t\t.baseaddr = {}UL,\t/* {}MB */".format(
        low_mem_to_pci_hole, int(low_mem_to_pci_hole, 16) / 1024 / 1024), file=config)
    print("\t\t.length   = {}UL,\t/* {}MB */".format(
        low_mem_to_pci_hole_len, int(low_mem_to_pci_hole_len, 16) / 1024 / 1024), file=config)
    print("\t\t.type     = E820_TYPE_RESERVED", file=config)
    print("\t},", file=config)
    print("", file=config)
    print("\t{{\t/* between PCI hole and {}GB */".format(
        TOTAL_MEM_SIZE / 1024 / 1024 / 1024), file=config)
    print("\t\t.baseaddr = {}UL,\t/* {}GB */".format(
        hex(int(pci_hole_addr, 16)), int(pci_hole_addr, 16) / 1024 / 1024 / 1024), file=config)
    print("\t\t.length   = {}UL,\t/* {}MB */".format(
        hex(int(pci_hole_len, 16)), int(pci_hole_len, 16) / 1024 / 1024), file=config)
    print("\t\t.type     = E820_TYPE_RESERVED", file=config)
    print("\t},", file=config)
    print("};", file=config)
    print("", file=config)
    print("/**", file=config)
    print(" * @pre vm != NULL", file=config)
    print("*/", file=config)
    print("void create_prelaunched_vm_e820(struct acrn_vm *vm)", file=config)
    print("{", file=config)
    print("\tvm->e820_entry_num = VE820_ENTRIES_{};".format(board_name), file=config)
    print("\tvm->e820_entries = ve820_entry;", file=config)
    print("}", file=config)

    return err_dic


def non_ve820_pre_launch(config):
    """
    This is none pre launch vm setting
    :param config:
    :return:
    """
    print("#include <e820.h>", file=config)
    print("#include <vm.h>", file=config)
    print("/**", file=config)
    print(" * @pre vm != NULL", file=config)
    print("*/", file=config)
    print("void create_prelaunched_vm_e820(struct acrn_vm *vm)", file=config)
    print("{", file=config)
    print("\tvm->e820_entry_num = 0;", file=config)
    print("\tvm->e820_entries = NULL;", file=config)
    print("}", file=config)


def generate_file(config):
    """Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    pre_vm_cnt = board_cfg_lib.get_pre_launch_cnt(board_cfg_lib.SCENARIO_INFO_FILE)

    # read mem size from scenario.xml
    hpa_size_list = board_cfg_lib.get_sub_leaf_tag(
        board_cfg_lib.SCENARIO_INFO_FILE, "memory", "size")
    ret = board_cfg_lib.is_hpa_size(hpa_size_list)
    if not ret:
        board_cfg_lib.print_red("Unknow type of host physical address size", err=True)
        err_dic['board config: generate ve820.c failed'] = "Unknow type of host physical address size"
        return err_dic

    hpa_size = hpa_size_list[0]
    if pre_vm_cnt != 0 and ('0x' in hpa_size or '0X' in hpa_size):
        err_dic = ve820_per_launch(config, hpa_size)
    else:
        non_ve820_pre_launch(config)

    return err_dic
