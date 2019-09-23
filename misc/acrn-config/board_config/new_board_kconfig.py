# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import board_cfg_lib


DESC = """
# New board kconfig generated by vm config tool
# Modified by Kconfiglib (https://github.com/ulfalizer/Kconfiglib)
"""

VM_NUM_MAP_TOTAL_HV_RAM_SIZE = {
    # 120M
    2:0x7800000,
    # 150M
    3:0x9600000,
    # 180M
    4:0xB400000,
    # 210M
    5:0xD200000,
    # 240M
    6:0xF000000,
    # 270M
    7:0x10E00000,
}


def find_avl_memory(ram_range, hpa_size):
    """
    This is get hv address from System RAM as host physical size
    :param ram_range: System RAM mapping
    :param hpa_size: fixed host physical size
    :return: start host physical address
    """
    ret_start_addr = 0
    tmp_order_key = 0

    tmp_order_key = sorted(ram_range)
    for start_addr in tmp_order_key:
        mem_range = ram_range[start_addr]
        if mem_range > int(hpa_size, 10):
            ret_start_addr = start_addr
            break

    return hex(ret_start_addr)


def get_ram_range():
    """ Get System RAM range mapping """
    # read system ram from board_info.xml
    ram_range = {}

    sys_mem_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<SYSTEM_RAM_INFO>", "</SYSTEM_RAM_INFO>")
    for line in sys_mem_lines:
        start_addr = int(line.split('-')[0], 16)
        end_addr = int(line.split('-')[1].split(':')[0], 16)
        mem_range = end_addr - start_addr
        ram_range[start_addr] = mem_range

    return ram_range


def generate_file(config):
    """Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    # this dictonary mapped with 'address start':'mem range'
    ram_range = {}

    if board_cfg_lib.VM_COUNT in list(VM_NUM_MAP_TOTAL_HV_RAM_SIZE.keys()):
        hv_ram_size = VM_NUM_MAP_TOTAL_HV_RAM_SIZE[board_cfg_lib.VM_COUNT]
    else:
        board_cfg_lib.print_red("VM num should not greater than 8", err=True)
        err_dic["baord config: total vm number error"] = "VM num should not greater than 8"
        return err_dic

    ram_range = get_ram_range()

    # reseve 16M memory for hv sbuf, ramoops, etc.
    reserved_ram = 0xf00000
    total_size = reserved_ram + hv_ram_size
    avl_start_addr = find_avl_memory(ram_range, str(total_size))
    hv_start_addr = int(avl_start_addr, 16) + int(hex(reserved_ram), 16)

    print('CONFIG_BOARD="{}"'.format(board_cfg_lib.BOARD_NAME), file=config)
    print("{}".format(DESC), file=config)

    print("CONFIG_HV_RAM_START={}".format(hex(hv_start_addr)), file=config)

    print("CONFIG_HV_RAM_SIZE={}".format(hex(hv_ram_size)), file=config)

    return err_dic
