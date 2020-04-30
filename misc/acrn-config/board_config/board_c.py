# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import enum
import subprocess
import board_cfg_lib
import common

class RDT(enum.Enum):
    L2 = 0
    L3 = 1
    MBA = 2

INCLUDE_HEADER = """
#include <board.h>
#include <vtd.h>
#include <msr.h>
#include <pci.h>"""

MSR_IA32_L2_MASK_BASE = 0x00000D10
MSR_IA32_L2_MASK_END = 0x00000D4F
MSR_IA32_L3_MASK_BASE = 0x00000C90
MSR_IA32_L3_MASK_END = 0x00000D0F


def gen_dmar_structure(config):
    """Generate dmar structure information"""

    dmar_info_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<DRHD_INFO>", "</DRHD_INFO>")
    drhd_cnt = 0
    drhd_dev_scope_cnt = []
    dev_scope_type = []

    if not dmar_info_lines:
        print("\n#ifndef CONFIG_ACPI_PARSE_ENABLED", file=config)
        print("#error \"DMAR info is not available, please set ACPI_PARSE_ENABLED to y in Kconfig. \\", file=config)
        print("\tOr use acrn-config tool to generate platform DMAR info.\"", file=config)
        print("#endif\n", file=config)

        print("struct dmar_info plat_dmar_info;\n", file=config)
        return

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


def populate_clos_mask_msr(rdt_res, common_clos_max, mask, config):
    """
    Populate the clos bitmask and msr index for a given RDT resource
    :param rdt_res: it is a string representing the RDT resource
    :param common_clos_max: Least common clos supported by all RDT resource
    :param mask: Max CLOS mask supported by the RDT resource
    :param config: it is a file pointer of board information for writing to
    """
    for idx in range(common_clos_max):
        print("\t{", file=config)
        print("\t\t.clos_mask = {0}U,".format(hex(mask)), file=config)
        print("\t\t.msr_index = MSR_IA32_{0}_MASK_BASE + {1},".format(
              rdt_res, idx), file=config)
        print("\t},", file=config)

def populate_mba_delay_mask(rdt_res, common_clos_max, config):
    """
    Populate the mba delay mask and msr index for memory resource
    :param rdt_res: it is a string representing the RDT resource
    :param common_clos_max: Least common clos supported by all RDT resource
    :param config: it is a file pointer of board information for writing to
    """
    for idx in range(common_clos_max):
        print("\t{", file=config)
        print("\t\t.mba_delay = 0U,", file=config)
        print("\t\t.msr_index = MSR_IA32_{0}_MASK_BASE + {1},".format(
              rdt_res, idx), file=config)
        print("\t},", file=config)


def gen_rdt_res(config):
    """
    Get RDT resource (L2, L3, MBA) information
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    rdt_res_str =""
    res_present = [0, 0, 0]
    (rdt_resources, rdt_res_clos_max, rdt_res_mask_max) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
    if len(rdt_res_clos_max) != 0:
        common_clos_max = min(rdt_res_clos_max)
    else:
        common_clos_max = 0

    if common_clos_max > MSR_IA32_L2_MASK_END - MSR_IA32_L2_MASK_BASE or\
        common_clos_max > MSR_IA32_L3_MASK_END - MSR_IA32_L3_MASK_BASE:
        err_dic["board config: generate board.c failed"] = "CLOS MAX should be less than reserved adress region length of L2/L3 cache"
        return err_dic

    print("\n#ifdef CONFIG_RDT_ENABLED", file=config)
    if len(rdt_resources) == 0 or common_clos_max == 0:
        print("struct platform_clos_info platform_{0}_clos_array[MAX_PLATFORM_CLOS_NUM];".format("l2"), file=config)
        print("struct platform_clos_info platform_{0}_clos_array[MAX_PLATFORM_CLOS_NUM];".format("l3"), file=config)
        print("struct platform_clos_info platform_{0}_clos_array[MAX_PLATFORM_CLOS_NUM];".format("mba"), file=config)
    else:
        for idx, rdt_res in enumerate(rdt_resources):
            if rdt_res == "L2":
                rdt_res_str = "l2"
                print("struct platform_clos_info platform_{0}_clos_array[{1}] = {{".format(rdt_res_str,
                      "MAX_PLATFORM_CLOS_NUM"), file=config)
                populate_clos_mask_msr(rdt_res, common_clos_max, int(rdt_res_mask_max[idx].strip('\''), 16), config)
                print("};\n", file=config)
                res_present[RDT.L2.value] = 1
            elif rdt_res == "L3":
                rdt_res_str = "l3"
                print("struct platform_clos_info platform_{0}_clos_array[{1}] = {{".format(rdt_res_str,
                      "MAX_PLATFORM_CLOS_NUM"), file=config)
                populate_clos_mask_msr(rdt_res, common_clos_max, int(rdt_res_mask_max[idx].strip('\''), 16), config)
                print("};\n", file=config)
                res_present[RDT.L3.value] = 1
            elif rdt_res == "MBA":
                rdt_res_str = "mba"
                print("struct platform_clos_info platform_{0}_clos_array[{1}] = {{".format(rdt_res_str,
                      "MAX_PLATFORM_CLOS_NUM"), file=config)
                populate_mba_delay_mask(rdt_res, common_clos_max, config)
                print("};\n", file=config)
                res_present[RDT.MBA.value] = 1
            else:
                err_dic['board config: generate board.c failed'] = "The input of {} was corrupted!".format(common.BOARD_INFO_FILE)
                return err_dic

        if res_present[RDT.L2.value] == 0:
            print("struct platform_clos_info platform_{0}_clos_array[{1}];".format("l2", "MAX_PLATFORM_CLOS_NUM"), file=config)
        if res_present[RDT.L3.value] == 0:
            print("struct platform_clos_info platform_{0}_clos_array[{1}];".format("l3", "MAX_PLATFORM_CLOS_NUM"), file=config)
        if res_present[RDT.MBA.value] == 0:
            print("struct platform_clos_info platform_{0}_clos_array[{1}];".format("mba", "MAX_PLATFORM_CLOS_NUM"), file=config)

    print("#endif", file=config)

    print("", file=config)
    return err_dic


def gen_single_data(data_lines, domain_str, config):
    line_i = 0
    data_statues = True
    data_len = len(data_lines)

    if data_len == 0:
        return

    for data_l in data_lines:
        if line_i == 0:
            if "not available" in data_l:
                print(data_l.strip(), file=config)
                print("static const struct cpu_{}x_data board_cpu_{}x[0];".format(domain_str, domain_str), file=config)
                print("", file=config)
                data_statues = False
                break
            else:
                print("static const struct cpu_{}x_data board_cpu_{}x[{}] = {{".format(domain_str, domain_str, data_len), file=config)
        print("\t{0}".format(data_l.strip()), file=config)
        line_i += 1
    if data_statues:
        print("};\n", file=config)


def gen_px_cx(config):
    """
    Get Px/Cx and store them to board.c
    :param config: it is a file pointer of board information for writing to
    """
    cpu_brand_lines = board_cfg_lib.get_info(
        common.BOARD_INFO_FILE, "<CPU_BRAND>", "</CPU_BRAND>")
    cx_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<CX_INFO>", "</CX_INFO>")
    px_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")

    gen_single_data(cx_lines, 'c', config)
    gen_single_data(px_lines, 'p', config)

    if not cpu_brand_lines:
        print("\nconst struct cpu_state_table board_cpu_state_tbl;\n", file=config)
        return

    for brand_line in cpu_brand_lines:
        cpu_brand = brand_line

    print("const struct cpu_state_table board_cpu_state_tbl = {", file=config)
    print("\t{0},".format(cpu_brand.strip()), file=config)
    print("\t{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,", file=config)
    print("\t(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}", file=config)
    print("};", file=config)


def gen_pci_hide(config):
    """Generate hide pci information for this platform"""
    if board_cfg_lib.BOARD_NAME in list(board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB.keys()) and board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB[board_cfg_lib.BOARD_NAME] != 0:
        hidden_pdev_list = board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB[board_cfg_lib.BOARD_NAME]
        hidden_pdev_num = len(hidden_pdev_list)
        print("const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM] = {", file=config)
        for hidden_pdev_i in range(hidden_pdev_num):
            bus = hex(int(hidden_pdev_list[hidden_pdev_i].split(':')[0], 16))
            dev = hex(int(hidden_pdev_list[hidden_pdev_i].split(':')[1], 16))
            fun = hex(int(hidden_pdev_list[hidden_pdev_i].split(':')[2], 16))
            print("\t{", file=config)
            print("\t\t.bits.b = {}U,".format(bus), file=config)
            print("\t\t.bits.d = {}U,".format(dev), file=config)
            print("\t\t.bits.f = {}U,".format(fun), file=config)
            print("\t},", file=config)
        print("};", file=config)
    else:
        print("const union pci_bdf plat_hidden_pdevs[MAX_HIDDEN_PDEVS_NUM];", file=config)


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

    # start to parse RDT resource info
    err_dic = gen_rdt_res(config)
    if err_dic:
        return err_dic

    # start to parse PX/CX info
    gen_px_cx(config)

    # gen hide pci info for platform
    gen_pci_hide(config)

    return err_dic
