# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import scenario_cfg_lib
import board_cfg_lib

IVSHMEM_HEADER_DEFINE = scenario_cfg_lib.HEADER_LICENSE + r"""
#ifndef IVSHMEM_CFG_H
#define IVSHMEM_CFG_H
"""
IVSHMEM_END_DEFINE = r"""#endif /* IVSHMEM_CFG_H */"""


def gen_common_header(config):
    """
    This is common header for ivshmem_cfg.h
    :param config: it is the pointer which file write to
    :return: None
    """
    print("{0}".format(IVSHMEM_HEADER_DEFINE), file=config)


def write_shmem_regions(config):
    raw_shmem_regions = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "IVSHMEM", "IVSHMEM_REGION")
    shmem_regions = []
    shmem_dev_num = 0
    for raw_shm in raw_shmem_regions:
        if raw_shm is None or raw_shm.strip() == '':
            continue
        raw_shm_splited = raw_shm.split(',')
        if len(raw_shm_splited) == 3 and raw_shm_splited[0].strip() != '' \
            and raw_shm_splited[1].strip() != '' and len(raw_shm_splited[2].strip().split(':')) >= 1:
            shmem_regions.append((raw_shm_splited[0].strip(), raw_shm_splited[1].strip(), raw_shm_splited[2].strip().split(':')))
            shmem_dev_num += len(raw_shm_splited[2].strip().split(':'))

    if len(shmem_regions) > 0:
        shmem_cnt = 0
        print("", file=config)
        for shmem_region in shmem_regions:
            print("#define IVSHMEM_SHM_REGION_%d\t"%shmem_cnt, end="", file=config)
            print('"{}"'.format(shmem_region[0]), file=config)
            shmem_cnt += 1
    print("", file=config)
    print("/*", file=config)
    print(" * The IVSHMEM_SHM_SIZE is the sum of all memory regions.", file=config)
    print(" * The size range of each memory region is [2MB, 512MB] and is a power of 2.", file=config)
    print(" */", file=config)
    total_shm_size = 0
    if len(shmem_regions) > 0:
        for shmem_region in shmem_regions:
            int_size = 0
            size = shmem_region[1]
            try:
                int_size = int(size) * 0x100000
            except Exception as e:
                print('the format of shm size error: ', str(e))
            total_shm_size += int_size

    print("#define IVSHMEM_SHM_SIZE\t{}UL".format(hex(total_shm_size)), file=config)
    print("#define IVSHMEM_DEV_NUM\t\t{}UL".format(shmem_dev_num), file=config)
    print("", file=config)
    print("/* All user defined memory regions */", file=config)
    if len(shmem_regions) == 0:
        print("#define IVSHMEM_SHM_REGIONS", file=config)
    else:
        print("#define IVSHMEM_SHM_REGIONS \\", file=config)
        shmem_cnt = 0
        for shmem in shmem_regions:
            print("\t{ \\", file=config)
            print('\t\t.name = IVSHMEM_SHM_REGION_{}, \\'.format(shmem_cnt), file=config)
            try:
                int_size = int(shmem[1]) * 0x100000
            except:
                int_size = 0
            print('\t\t.size = {}UL,\t\t/* {}M */ \\'.format(hex(int_size), shmem[1]), file=config)
            if shmem_cnt < len(shmem_regions) - 1:
                print("\t}, \\", file=config)
            else:
                print("\t},", file=config)
            shmem_cnt += 1
    print("", file=config)


def generate_file(scenario_items, config):
    """
    Start to generate ivshmem_cfg.h
    :param scenario_items: it is the class which contain all user setting information
    :param config: it is a file pointer of scenario information for writing to
    """
    vm_info = scenario_items['vm']
    gen_common_header(config)

    if vm_info.shmem.shmem_enabled == 'y':
        print("#include <ivshmem.h>", file=config)
        print("#include <x86/pgtable.h>", file=config)
        write_shmem_regions(config)

    print("{0}".format(IVSHMEM_END_DEFINE), file=config)
