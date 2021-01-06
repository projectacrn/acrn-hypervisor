#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib

VM_NUM_MAP_TOTAL_HV_RAM_SIZE = {
    # 120M
    2:0x7800000,
    # 150M
    3:0x9600000,
    # 190M
    4:0xBE00000,
    # 210M
    5:0xD200000,
    # 250M
    6:0xFA00000,
    # 300M
    7:0x12C00000,
    # 328M
    8:0x14800000,
}

HV_RAM_SIZE_MAX = 0x40000000

MEM_ALIGN = 2 * common.SIZE_M

def fn(board_etree, scenario_etree, allocation_etree):
    # this dictonary mapped with 'address start':'mem range'
    ram_range = {}

    vm_count = common.count_nodes("//*[local-name() = 'vm']", scenario_etree)
    hv_ram_size = VM_NUM_MAP_TOTAL_HV_RAM_SIZE[vm_count]

    ivshmem_enabled = common.get_text("//IVSHMEM_ENABLED", scenario_etree)
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
    hv_ram_size += total_shm_size
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
