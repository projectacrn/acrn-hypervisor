#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import acrn_config_utilities, board_cfg_lib, scenario_cfg_lib
from acrn_config_utilities import get_node

HV_RAM_SIZE_MAX = 0x40000000

MEM_ALIGN = 2 * acrn_config_utilities.SIZE_M

def fn(board_etree, scenario_etree, allocation_etree):
    # this dictonary mapped with 'address start':'mem range'
    ram_range = {}

    max_vm_num = int(get_node(f"//hv/CAPACITIES/MAX_VM_NUM/text()", scenario_etree))
    max_trusty_vm = len(scenario_etree.xpath(f"//vm[./secure_world_support/text() = 'y']"))
    hv_ram_size = acrn_config_utilities.HV_BASE_RAM_SIZE + acrn_config_utilities.VM_RAM_SIZE * max_vm_num + max_trusty_vm * acrn_config_utilities.TRUSTY_RAM_SIZE
    ivshmem_list = scenario_etree.xpath("//IVSHMEM_SIZE/text()")
    total_shm_size = 0
    for ram_size in ivshmem_list:
        try:
            total_shm_size += int(ram_size) * 0x100000
        except Exception as e:
            print(e)
    hv_ram_size += max(total_shm_size, 0x200000)
    assert(hv_ram_size <= HV_RAM_SIZE_MAX)

    # We recommend to put hv ram start address high than 0x400000 to
    # reduce memory conflict with hv log.
    hv_start_offset = 0x400000

    for start_addr in list(board_cfg_lib.USED_RAM_RANGE):
        if hv_start_offset <= start_addr < 0x80000000:
            del board_cfg_lib.USED_RAM_RANGE[start_addr]
    ram_range = board_cfg_lib.get_ram_range()

    avl_start_addr = board_cfg_lib.find_avl_memory(ram_range, str(hv_ram_size), hv_start_offset)
    hv_start_addr = int(avl_start_addr, 16)
    hv_start_addr = acrn_config_utilities.round_up(hv_start_addr, MEM_ALIGN)
    board_cfg_lib.USED_RAM_RANGE[hv_start_addr] = hv_ram_size
    acrn_config_utilities.append_node("/acrn-config/hv/MEMORY/HV_RAM_START", hex(hv_start_addr), allocation_etree)
    acrn_config_utilities.append_node("/acrn-config/hv/MEMORY/HV_RAM_SIZE", hex(hv_ram_size), allocation_etree)
