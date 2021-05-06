#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib, scenario_cfg_lib

HV_RAM_SIZE_MAX = 0x40000000

MEM_ALIGN = 2 * common.SIZE_M

def fn(board_etree, scenario_etree, allocation_etree):
    # this dictonary mapped with 'address start':'mem range'
    ram_range = {}

    post_launched_vm_num = 0
    for id in common.VM_TYPES:
        if common.VM_TYPES[id] in scenario_cfg_lib.VM_DB and \
                        scenario_cfg_lib.VM_DB[common.VM_TYPES[id]]["load_type"] == "POST_LAUNCHED_VM":
            post_launched_vm_num += 1
    hv_ram_size = common.HV_BASE_RAM_SIZE + common.POST_LAUNCHED_VM_RAM_SIZE * post_launched_vm_num

    ivshmem_enabled = common.get_node("//IVSHMEM_ENABLED/text()", scenario_etree)
    total_shm_size = 0
    if ivshmem_enabled == 'y':
        raw_shmem_regions = scenario_etree.xpath("//IVSHMEM_REGION/text()")
        for raw_shm in raw_shmem_regions:
            if raw_shm.strip() == '':
                continue
            raw_shm_splited = raw_shm.split(',')
            if len(raw_shm_splited) == 3 and raw_shm_splited[0].strip() != '' \
                    and raw_shm_splited[1].strip() != '' and len(raw_shm_splited[2].strip().split(':')) >= 1:
                try:
                    size = raw_shm_splited[1].strip()
                    int_size = int(size) * 0x100000
                    total_shm_size += int_size
                except Exception as e:
                    print(e)
    hv_ram_size += 2 * max(total_shm_size, 0x200000)
    assert(hv_ram_size <= HV_RAM_SIZE_MAX)

    # reseve 16M memory for hv sbuf, ramoops, etc.
    reserved_ram = 0x1000000
    # We recommend to put hv ram start address high than 0x10000000 to
    # reduce memory conflict with GRUB/SOS Kernel.
    hv_start_offset = 0x10000000
    total_size = reserved_ram + hv_ram_size
    for start_addr in list(board_cfg_lib.USED_RAM_RANGE):
        if hv_start_offset <= start_addr < 0x80000000:
            del board_cfg_lib.USED_RAM_RANGE[start_addr]
    ram_range = board_cfg_lib.get_ram_range()
    avl_start_addr = board_cfg_lib.find_avl_memory(ram_range, str(total_size), hv_start_offset)
    hv_start_addr = int(avl_start_addr, 16) + int(hex(reserved_ram), 16)
    hv_start_addr = common.round_up(hv_start_addr, MEM_ALIGN)
    board_cfg_lib.USED_RAM_RANGE[hv_start_addr] = total_size

    common.append_node("/acrn-config/hv/MEMORY/HV_RAM_START", hex(hv_start_addr), allocation_etree)
    common.append_node("/acrn-config/hv/MEMORY/HV_RAM_SIZE", hex(hv_ram_size), allocation_etree)
