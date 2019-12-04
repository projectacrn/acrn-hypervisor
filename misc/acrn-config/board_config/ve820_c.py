# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import board_cfg_lib

FOUR_GBYTE = 4 * 1024 * 1024 * 1024
LOW_MEM_TO_PCI_HOLE = 0x20000000


def ve820_per_launch(config, hpa_size, hpa2_size):
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
    low_mem_hpa_len = []
    high_mem_hpa_len = []
    high_mem_hpa2_len = []
    high_mem_hpa2_addr = []

    # pre_launch memroy: mem_size is the ve820 length
    print("#include <e820.h>", file=config)
    print("#include <vm.h>", file=config)
    print("", file=config)

    for i in range(board_cfg_lib.VM_COUNT):
        if (int(hpa_size[i], 16) <= 512 * 1024 * 1024):
               low_mem_hpa_len.append(int(hpa_size[i], 16) - 1 * 1024 * 1024)
               high_mem_hpa_len.append(0)
        else:
            low_mem_hpa_len.append(511 * 1024 * 1024)
            high_mem_hpa_len.append(int(hpa_size[i], 16) - 512 * 1024 * 1024)

         #HPA2 is always allocated in >4G space.
        high_mem_hpa2_len.append(int(hpa2_size[i], 16))
        if (high_mem_hpa_len[i] != 0) and (high_mem_hpa2_len[i] != 0):
            high_mem_hpa2_addr.append(hex(FOUR_GBYTE) + high_mem_hpa_len[i])
        else:
            high_mem_hpa2_addr.append(hex(FOUR_GBYTE))

        if (high_mem_hpa_len[i] != 0) and (high_mem_hpa2_len[i] != 0):
            print("#define VM{}_VE820_ENTRIES_{}\t{}U".format(i, board_name, 7), file=config)
        elif (high_mem_hpa_len[i] != 0) or (high_mem_hpa2_len[i] != 0):
            print("#define VM{}_VE820_ENTRIES_{}\t{}U".format(i, board_name, 6), file=config)
        else:
            print("#define VM{}_VE820_ENTRIES_{}\t{}U".format(i, board_name, 5), file=config)

    for i in range(board_cfg_lib.VM_COUNT):
        print("static const struct e820_entry vm{}_ve820_entry[{}] = {{".format(
            i, "VM{}_VE820_ENTRIES_{}".format(i, board_name)), file=config)
        print("\t{\t/* usable RAM under 1MB */", file=config)
        print("\t\t.baseaddr = 0x0UL,", file=config)
        print("\t\t.length   = 0xF0000UL,\t\t/* 960KB */", file=config)
        print("\t\t.type     = E820_TYPE_RAM", file=config)
        print("\t},", file=config)
        print("", file=config)
        print("\t{\t/* mptable */", file=config)
        print("\t\t.baseaddr = 0xF0000UL,\t\t/* 960KB */", file=config)
        print("\t\t.length   = 0x10000UL,\t\t/* 64KB */", file=config)
        print("\t\t.type     = E820_TYPE_RESERVED", file=config)
        print("\t},", file=config)
        print("", file=config)

        print("\t{\t/* lowmem */", file=config)

        print("\t\t.baseaddr = {}UL,\t\t/* 1MB */".format(
            hex(start_low_hpa)), file=config)
        print("\t\t.length   = {}UL,\t/* {}MB */".format(
            hex(low_mem_hpa_len[i]), low_mem_hpa_len[i] / 1024 / 1024), file=config)

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
        print("\t{\t/* between PCI hole and 4 GB */", file=config)
        print("\t\t.baseaddr = {}UL,\t/* {}GB */".format(
            hex(int(pci_hole_addr, 16)), int(pci_hole_addr, 16) / 1024 / 1024 / 1024), file=config)
        print("\t\t.length   = {}UL,\t/* {}MB */".format(
            hex(int(pci_hole_len, 16)), int(pci_hole_len, 16) / 1024 / 1024), file=config)
        print("\t\t.type     = E820_TYPE_RESERVED", file=config)
        print("\t},", file=config)
        print("", file=config)
        if (high_mem_hpa_len[i] != 0) and (high_mem_hpa2_len[i] != 0):
            print("\t{\t/* high mem after 4GB*/", file=config)
            print("\t\t.baseaddr = {}UL,\t/* 4 GB */".format(
                hex(FOUR_GBYTE)), file=config)
            print("\t\t.length   = {}UL,\t/* {}MB */".format(
                hex(high_mem_hpa_len[i]), high_mem_hpa_len[i] / 1024 / 1024), file=config)
            print("\t\t.type     = E820_TYPE_RAM", file=config)
            print("\t},", file=config)
            print("", file=config)
            print("\t{\t/* HPA2 after high mem*/", file=config)
            print("\t\t.baseaddr = {}UL,\t/* {}GB */".format(
                hex(high_mem_hpa2_addr[i]), int(high_mem_hpa2_addr[i], 16) / 1024 / 1024 / 1024), file=config)
            print("\t\t.length   = {}UL,\t/* {}MB */".format(
                hex(high_mem_hpa_len[i]), high_mem_hpa_len[i] / 1024 / 1024), file=config)
            print("\t\t.type     = E820_TYPE_RAM", file=config)
            print("\t},", file=config)
            print("", file=config)
        elif (high_mem_hpa_len[i] != 0):
            print("\t{\t/* high mem after 4GB*/", file=config)
            print("\t\t.baseaddr = {}UL,\t/* 4 GB */".format(
                hex(FOUR_GBYTE)), file=config)
            print("\t\t.length   = {}UL,\t/* {}MB */".format(
                hex(high_mem_hpa_len[i]), high_mem_hpa_len[i] / 1024 / 1024), file=config)
            print("\t\t.type     = E820_TYPE_RAM", file=config)
            print("\t},", file=config)
            print("", file=config)
        elif(high_mem_hpa2_len[i] != 0):
            print("\t{\t/* HPA2 after 4GB*/", file=config)
            print("\t\t.baseaddr = {}UL,\t/* 4 GB */".format(
                hex(FOUR_GBYTE)), file=config)
            print("\t\t.length   = {}UL,\t/* {}MB */".format(
                hex(high_mem_hpa2_len[i]), high_mem_hpa2_len[i] / 1024 / 1024), file=config)
            print("\t\t.type     = E820_TYPE_RAM", file=config)
            print("\t},", file=config)
            print("", file=config)
        print("};", file=config)
        print("", file=config)

    print("/**", file=config)
    print(" * @pre vm != NULL", file=config)
    print("*/", file=config)
    print("void create_prelaunched_vm_e820(struct acrn_vm *vm)", file=config)
    print("{", file=config)
    for i in range(board_cfg_lib.VM_COUNT):
        print("\tif (vm->vm_id == {}U)".format(hex(i)), file=config)
        print("\t{", file=config)
        print("\t\tvm->e820_entry_num = VM{}_VE820_ENTRIES_{};".format(i, board_name), file=config)
        print("\t\tvm->e820_entries = vm{}_ve820_entry;".format(i), file=config)
        print("\t}", file=config)
        print("", file=config)

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

    # read HPA2 mem size from scenario.xml
    hpa2_size_list = board_cfg_lib.get_sub_leaf_tag(
        board_cfg_lib.SCENARIO_INFO_FILE, "memory", "size_hpa2")
    ret = board_cfg_lib.is_hpa_size(hpa2_size_list)
    if not ret:
        board_cfg_lib.print_red("Unknow type of second host physical address size", err=True)
        err_dic['board config: generate ve820.c failed'] = "Unknow type of second host physical address size"
        return err_dic

    # HPA size for both VMs should have valid length.
    for i in range(board_cfg_lib.VM_COUNT):
        if hpa_size_list[i] == '0x0' or hpa_size_list[i] == '0X0':
            board_cfg_lib.print_red("HPA size should not be zero", err=True)
            err_dic['board config: generate ve820.c failed'] = "HPA size should not be zero"
            return err_dic

    if pre_vm_cnt != 0:
        err_dic = ve820_per_launch(config, hpa_size_list, hpa2_size_list)
    else:
        non_ve820_pre_launch(config)

    return err_dic
