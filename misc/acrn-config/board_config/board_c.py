# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import board_cfg_lib


INCLUDE_HEADER = """
#include <board.h>
#include <vtd.h>"""


MSR_IA32_L2_MASK_BASE = 0x00000D10
MSR_IA32_L2_MASK_END = 0x00000D4F
MSR_IA32_L3_MASK_BASE = 0x00000C90
MSR_IA32_L3_MASK_END = 0x00000D0F


def gen_dmar_structure(config):
    """Generate dmar structure information"""

    dmar_info_lines = board_cfg_lib.get_info(board_cfg_lib.BOARD_INFO_FILE, "<DRHD_INFO>", "</DRHD_INFO>")
    drhd_cnt = 0
    drhd_dev_scope_cnt = []
    dev_scope_type = []

    # parse to get DRHD count and dev scope count
    for dmar_line in dmar_info_lines:
        if "DRHD_COUNT" in dmar_line and not drhd_cnt:
            drhd_cnt = int(dmar_line.split()[2].strip('U'))

    for i_cnt in range(drhd_cnt):
        for dmar_line in dmar_info_lines:
            dev_scope_cnt_str = "DRHD{}_DEV_CNT".format(i_cnt)

            if dev_scope_cnt_str in dmar_line:
                tmp_dev_scope_cnt = int(dmar_line.split()[2].strip('U'), 16)
                drhd_dev_scope_cnt.append(tmp_dev_scope_cnt)

    # gen dmar structure information
    for i_drhd_cnt in range(drhd_cnt):
        dev_cnt = drhd_dev_scope_cnt[i_drhd_cnt]
        print("static struct dmar_dev_scope drhd{}_dev_scope[DRHD{}_DEV_CNT] = {{".format(
            i_drhd_cnt, i_drhd_cnt), file=config)
        for i_dev_cnt in range(dev_cnt):
            print("\t{", file=config)
            print("\t\t.type   = DRHD{}_DEVSCOPE{}_TYPE,".format(i_drhd_cnt, i_dev_cnt), file=config)
            print("\t\t.id     = DRHD{}_DEVSCOPE{}_ID,".format(i_drhd_cnt, i_dev_cnt), file=config)
            print("\t\t.bus    = DRHD{}_DEVSCOPE{}_BUS,".format(i_drhd_cnt, i_dev_cnt), file=config)
            print("\t\t.devfun = DRHD{}_DEVSCOPE{}_PATH,".format(i_drhd_cnt, i_dev_cnt), file=config)
            print("\t},", file=config)

        print("};", file=config)
        print("", file=config)

    print("static struct dmar_drhd drhd_info_array[DRHD_COUNT] = {", file=config)
    for i_drhd_cnt in range(drhd_cnt):
        print("\t{", file=config)
        print("\t\t.dev_cnt       = DRHD{}_DEV_CNT,".format(i_drhd_cnt), file=config)
        print("\t\t.segment       = DRHD{}_SEGMENT,".format(i_drhd_cnt), file=config)
        print("\t\t.flags         = DRHD{}_FLAGS,".format(i_drhd_cnt), file=config)
        print("\t\t.reg_base_addr = DRHD{}_REG_BASE,".format(i_drhd_cnt), file=config)
        print("\t\t.ignore        = DRHD{}_IGNORE,".format(i_drhd_cnt), file=config)
        print("\t\t.devices       = drhd{}_dev_scope".format(i_drhd_cnt), file=config)
        print("\t},", file=config)

    print("};", file=config)
    print("", file=config)
    print("struct dmar_info plat_dmar_info = {", file=config)
    print("\t.drhd_count = DRHD_COUNT,", file=config)
    print("\t.drhd_units = drhd_info_array,", file=config)
    print("};", file=config)


def gen_cat(config):
    """
    Get CAT information
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    (cache_support, clos_max) = board_cfg_lib.clos_info_parser(board_cfg_lib.BOARD_INFO_FILE)

    if clos_max > MSR_IA32_L2_MASK_END - MSR_IA32_L2_MASK_BASE or\
        clos_max > MSR_IA32_L3_MASK_END - MSR_IA32_L3_MASK_BASE:
        err_dic["board config: generate board.c failed"] = "CLOS MAX should be less than reserved adress region length of L2/L3 cache"
        return err_dic

    if cache_support == "False" or clos_max == 0:
        print("\nstruct platform_clos_info platform_clos_array[MAX_PLATFORM_CLOS_NUM];", file=config)
    else:
        print("\nstruct platform_clos_info platform_clos_array[{}] = {{".format(
            "MAX_PLATFORM_CLOS_NUM"), file=config)
        for i_cnt in range(clos_max):
            print("\t{", file=config)

            print("\t\t.clos_mask = {0}U,".format(hex(0xff)), file=config)
            if cache_support == "L2":
                print("\t\t.msr_index = MSR_IA32_L2_MASK_BASE + {}U,".format(i_cnt), file=config)
            elif cache_support == "L3":
                print("\t\t.msr_index = MSR_IA32_L3_MASK_BASE + {}U,".format(i_cnt), file=config)
            else:
                err_dic['board config: generate board.c failed'] = "The input of {} was corrupted!".format(board_cfg_lib.BOARD_INFO_FILE)
                return err_dic
            print("\t},", file=config)

        print("};\n", file=config)

    print("", file=config)
    return err_dic


def gen_px_cx(config):
    """
    Get Px/Cx and store them to board.c
    :param config: it is a file pointer of board information for writing to
    """
    cpu_brand_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<CPU_BRAND>", "</CPU_BRAND>")
    cx_lines = board_cfg_lib.get_info(board_cfg_lib.BOARD_INFO_FILE, "<CX_INFO>", "</CX_INFO>")
    px_lines = board_cfg_lib.get_info(board_cfg_lib.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")

    cx_len = len(cx_lines)
    px_len = len(px_lines)
    #print("#ifdef CONFIG_CPU_POWER_STATES_SUPPORT", file=config)
    print("static const struct cpu_cx_data board_cpu_cx[%s] = {"%str(cx_len), file=config)
    for cx_l in cx_lines:
        print("\t{0}".format(cx_l.strip()), file=config)
    print("};\n", file=config)

    print("static const struct cpu_px_data board_cpu_px[%s] = {"%str(px_len), file=config)
    for px_l in px_lines:
        print("\t{0}".format(px_l.strip()), file=config)
    print("};\n", file=config)

    for brand_line in cpu_brand_lines:
        cpu_brand = brand_line

    print("const struct cpu_state_table board_cpu_state_tbl = {", file=config)
    print("\t{0},".format(cpu_brand.strip()), file=config)
    print("\t{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,", file=config)
    print("\t(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}", file=config)
    print("};", file=config)
    #print("#endif", file=config)


def generate_file(config):
    """
    Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)

    # insert bios info into board.c
    board_cfg_lib.handle_bios_info(config)
    print(INCLUDE_HEADER, file=config)

    # start to parse DMAR info
    gen_dmar_structure(config)

    # start to parse to get CAT info
    err_dic = gen_cat(config)
    if err_dic:
        return err_dic

    # start to parse PX/CX info
    gen_px_cx(config)

    return err_dic
