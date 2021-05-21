#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib

PRE_LAUNCHED_VMS_TYPE = ["SAFETY_VM", "PRE_RT_VM", "PRE_STD_VM"]
POST_LAUNCHED_VMS_TYPE = ["POST_STD_VM", "POST_RT_VM", "KATA_VM"]
SOS_VM_TYPE = ["SOS_VM"]

def parse_hv_console(scenario_etree):
    """
    There may be 3 types in the console item
    1. BDF:(00:18.2) seri:/dev/ttyS2
    2. /dev/ttyS2
    3. ttyS2
    """
    ttys_n = ''
    ttys = common.get_node("//SERIAL_CONSOLE/text()", scenario_etree)

    if not ttys or ttys == None:
        return ttys_n

    if ttys and 'BDF' in ttys or '/dev' in ttys:
        ttys_n = ttys.split('/')[2]
    else:
        ttys_n = ttys

    return ttys_n

def get_native_ttys():
    native_ttys = {}
    ttys_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")
    if ttys_lines:
        for tty_line in ttys_lines:
            tmp_dic = {}
            #seri:/dev/ttySx type:mmio base:0x91526000 irq:4 [bdf:"00:18.0"]
            #seri:/dev/ttySy type:portio base:0x2f8 irq:5
            tty = tty_line.split('/')[2].split()[0]
            ttys_type = tty_line.split()[1].split(':')[1].strip()
            ttys_base = tty_line.split()[2].split(':')[1].strip()
            ttys_irq = tty_line.split()[3].split(':')[1].strip()
            tmp_dic['type'] = ttys_type
            tmp_dic['base'] = ttys_base
            tmp_dic['irq'] = int(ttys_irq)
            native_ttys[tty] = tmp_dic
    return native_ttys

def get_shmem_regions(etree):
    ivshmem_enabled = common.get_node("//IVSHMEM_ENABLED/text()", etree)
    if ivshmem_enabled == 'n':
        return {}

    # <IVSHMEM_REGION> format is shm_name, shm_size, VM IDs
    # example: hv:/shm_region_0, 2, 0:2
    ivshmem_regions = etree.xpath("//IVSHMEM_REGION")
    shmem_regions = {}
    for idx in range(len(ivshmem_regions)):
        shm_string = ivshmem_regions[idx].text
        if shm_string is None:
            continue
        shm_string_list = shm_string.split(',')
        shm_name = shm_string_list[0].strip()
        shm_size = shm_string_list[1].strip()
        vmid_list = [vm_id.strip() for vm_id in shm_string_list[2].split(':')]
        for vm_id in vmid_list:
            if vm_id not in shmem_regions:
                shmem_regions[vm_id] = {}
            shmem_regions[vm_id][shm_name] = {'id' : str(idx), 'size' : shm_size}
    return shmem_regions

def is_pre_launched_vm(vm_type):
    if vm_type in PRE_LAUNCHED_VMS_TYPE:
        return True
    return False

def is_post_launched_vm(vm_type):
    if vm_type in POST_LAUNCHED_VMS_TYPE:
        return True
    return False

def is_sos_vm(vm_type):
    if vm_type in SOS_VM_TYPE:
        return True
    return False