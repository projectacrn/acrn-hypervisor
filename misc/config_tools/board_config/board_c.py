# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import enum
import board_cfg_lib
import acrn_config_utilities
import lxml.etree
import os
from acrn_config_utilities import get_node

class RDT(enum.Enum):
    L2 = 0
    L3 = 1
    MBA = 2

INCLUDE_HEADER = """
#include <asm/board.h>
#include <asm/vtd.h>
#include <asm/msr.h>
#include <asm/rdt.h>
#include <pci.h>
#include <misc_cfg.h>
"""

MSR_IA32_L2_MASK_BASE = 0x00000D10
MSR_IA32_L2_MASK_END = 0x00000D4F
MSR_IA32_L3_MASK_BASE = 0x00000C90
MSR_IA32_L3_MASK_END = 0x00000D0F


def gen_dmar_structure(config):
    """Generate dmar structure information"""

    dmar_info_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<DRHD_INFO>", "</DRHD_INFO>")
    drhd_cnt = 0
    drhd_dev_scope_cnt = []
    dev_scope_type = []

    if not dmar_info_lines:
        print("\n#ifndef CONFIG_ACPI_PARSE_ENABLED", file=config)
        print("#error \"DMAR info is not available, please set ACPI_PARSE_ENABLED to y. \\", file=config)
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


def populate_clos_mask_msr(rdt_res, cat_mask_list, config):
    """
    Populate the clos bitmask and msr index for a given RDT resource
    :param rdt_res: it is a string representing the RDT resource
    :param cat_mask_list: cache mask list corresponding to each CLOS
    :param config: it is a file pointer of board information for writing to
    """
    idx = 0
    for cat_mask in cat_mask_list:
        print("\t{", file=config)
        print("\t\t.clos_mask = {},".format(cat_mask), file=config)
        print("\t},", file=config)
        idx += 1

def populate_mba_delay_mask(rdt_res, mba_delay_list, config):
    """
    Populate the mba delay mask and msr index for memory resource
    :param rdt_res: it is a string representing the RDT resource
    :param mba_delay_list: mba delay value list corresponding to each CLOS
    :param config: it is a file pointer of board information for writing to
    """
    idx = 0
    for mba_delay_mask in mba_delay_list:
        print("\t{", file=config)
        print("\t\t.mba_delay = ,".format(mba_delay_mask), file=config)
        print("\t},", file=config)
        idx += 1

def get_rdt_enabled():
    scenario_etree = lxml.etree.parse(acrn_config_utilities.SCENARIO_INFO_FILE)
    enable = scenario_etree.xpath(f"//RDT_ENABLED/text()")
    if enable[0] == "y":
        return "true"
    else:
        return "false"

def get_cdp_enabled():
    scenario_etree = lxml.etree.parse(acrn_config_utilities.SCENARIO_INFO_FILE)
    enable = scenario_etree.xpath(f"//CDP_ENABLED/text()")
    if enable[0] == "y":
        return "true"
    else:
        return "false"

def get_common_clos_max(clos_number, capability_id):

    common_clos_max = 0
    if get_rdt_enabled() and not get_cdp_enabled():
        common_clos_max = clos_number
    if get_cdp_enabled() and capability_id != 'MBA':
        common_clos_max = clos_number / 2

    return common_clos_max

def gen_rdt_str(cache, config):
    err_dic = {}
    cat_mask_list = {}

    board_etree = lxml.etree.parse(acrn_config_utilities.BOARD_INFO_FILE)
    mask_length = get_node(f"./capability[@id='CAT']/capacity_mask_length/text()", cache)
    clos_number = get_node(f"./capability[@id='CAT']/clos_number/text()", cache)

    bitmask = (1 << int(mask_length)) - 1
    cache_level = get_node(f"./@level", cache)
    cache_id = get_node(f"./@id", cache)
    processor_list = board_etree.xpath(f"//cache[@level = '{cache_level}' and @id = '{cache_id}']/processors/processor/text()")
    capability_list = board_etree.xpath(f"//cache[@level = '{cache_level}' and @id = '{cache_id}']/capability/@id")

    for capability_id in capability_list:

        common_clos_max = get_common_clos_max(int(clos_number), capability_id)
        if capability_id == "CAT":
            if common_clos_max > MSR_IA32_L2_MASK_END - MSR_IA32_L2_MASK_BASE or\
                common_clos_max > MSR_IA32_L3_MASK_END - MSR_IA32_L3_MASK_BASE:
                err_dic["board config: Failed to generate board.c"] = "CLOS Mask Number is more then the reserved address region length of L2/L3 cache"
                return err_dic

            cdp_enable = get_cdp_enabled()
            cat_mask_list = get_mask_list(cache_level, cache_id)
            if len(cat_mask_list) > int(clos_number):
                    err_dic['board config: Failed to generate board.c'] = "CLOS Mask Number too bigger then the supported of L2/L3 cache"
                    return err_dic;

            if cache_level == "2":
                rdt_res = "l2"
            elif cache_level == "3":
                rdt_res = "l3"

            clos_config_array = "platform_l{0}_clos_array_{1}".format(cache_level, int(cache_id, 16))

            print("\t{", file=config)
            print("\t\t.res.cache = {", file=config)
            print("\t\t\t.bitmask = {0},".format(hex(bitmask)), file=config)
            print("\t\t\t.cbm_len = {0},".format(mask_length), file=config)
            print("\t\t\t.is_cdp_enabled = {0},".format(cdp_enable), file=config)
            print("\t\t},", file=config)
        elif capability_id == "MBA":
            max_throttling_value = get_node(f"./capability/max_throttling_value/text()", cache)
            rdt_res = "mba"
            clos_config_array = "platform_mba_clos_array"
            print("\t{", file=config)
            print("\t\t.res.membw = {", file=config)
            print("\t\t\t.mba_max = {0},".format(clos_number), file=config)
            print("\t\t\t.delay_linear = {0}".format(max_throttling_value), file=config)
            print("\t\t},", file=config)

    print("\t\t.num_closids = {0},".format(clos_number), file=config)
    print("\t\t.num_clos_config = {0},".format(len(cat_mask_list)), file=config)
    print("\t\t.clos_config_array = {0},".format(clos_config_array), file=config)

    cpu_mask = 0
    for processor in processor_list:
        core_id = get_node(f"//thread[apic_id = '{processor}']/cpu_id/text()", board_etree)
        if core_id is None:
            continue
        else:
            cpu_mask = cpu_mask | (1 << int(core_id))
    print("\t\t.cpu_mask = {0},".format(hex(cpu_mask)), file=config)
    print("\t},", file=config)

    return err_dic;

def get_mask_list(cache_level, cache_id):
    allocation_dir = os.path.split(acrn_config_utilities.SCENARIO_INFO_FILE)[0] + "/configs/allocation.xml"
    allocation_etree = lxml.etree.parse(allocation_dir)
    if cache_level == "3":
        clos_list = allocation_etree.xpath(f"//clos_mask[@id = 'l3']/clos/text()")
    else:
        clos_list = allocation_etree.xpath(f"//clos_mask[@id = '{cache_id}']/clos/text()")
    return clos_list
def gen_clos_array(cache_list, config):
    err_dic = {}
    res_present = [0, 0, 0]
    if len(cache_list) == 0:
        print("union clos_config platform_{0}_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];".format("l2"), file=config)
        print("union clos_config platform_{0}_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];".format("l3"), file=config)
        print("union clos_config platform_{0}_clos_array[MAX_MBA_CLOS_NUM_ENTRIES];".format("mba"), file=config)
        print("struct rdt_info res_infos[RDT_INFO_NUMBER];", file=config)
    else:
        for idx, cache in enumerate(cache_list):
            cache_level = get_node(f"./@level", cache)
            cache_id = get_node(f"./@id", cache)
            clos_number = get_node(f"./capability/clos_number/text()", cache)
            if cache_level == "2":

                cat_mask_list = get_mask_list(cache_level, cache_id)
                array_size = len(cat_mask_list)

                print("union clos_config platform_l2_clos_array_{0}[{1}] = {{".format(int(cache_id, 16), clos_number), file=config)

                populate_clos_mask_msr("L2", cat_mask_list, config)

                print("};\n", file=config)
                res_present[RDT.L2.value] += 1
            elif cache_level == "3":
                cat_mask_list = get_mask_list(cache_level, cache_id)

                print("union clos_config platform_l3_clos_array_{0}[{1}] = {{".format(int(cache_id, 16), clos_number), file=config)

                populate_clos_mask_msr("L3", cat_mask_list, config)

                print("};\n", file=config)
                res_present[RDT.L3.value] += 1
            elif cache_level == "MBA":
                print("union clos_config platform_mba_clos_array[MAX_MBA_CLOS_NUM_ENTRIES] = {", file=config)
                err_dic = populate_mba_delay_mask("mba", mba_delay_list, config)
                print("};\n", file=config)
                res_present[RDT.MBA.value] = 1
            else:
                err_dic['board config: generate board.c failed'] = "The input of {} was corrupted!".format(acrn_config_utilities.BOARD_INFO_FILE)
                return err_dic

        if res_present[RDT.L2.value] == 0:
            print("union clos_config platform_l2_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];", file=config)
        if res_present[RDT.L3.value] == 0:
            print("union clos_config platform_l3_clos_array[MAX_CACHE_CLOS_NUM_ENTRIES];", file=config)
        if res_present[RDT.MBA.value] == 0:
            print("union clos_config platform_mba_clos_array[MAX_MBA_CLOS_NUM_ENTRIES];", file=config)
    return 0

def gen_rdt_res(config):
    """
    Get RDT resource (L2, L3, MBA) information
    :param config: it is a file pointer of board information for writing to
    """
    print("\n#ifdef CONFIG_RDT_ENABLED", file=config)
    err_dic = {}
    res_present = [0, 0, 0]

    scenario_etree = lxml.etree.parse(acrn_config_utilities.SCENARIO_INFO_FILE)
    allocation_etree = lxml.etree.parse(acrn_config_utilities.SCENARIO_INFO_FILE)
    board_etree = lxml.etree.parse(acrn_config_utilities.BOARD_INFO_FILE)

    cache_list = board_etree.xpath(f"//cache[capability/@id = 'CAT' or capability/@id = 'MBA']")
    gen_clos_array(cache_list, config)

    cache_list = board_etree.xpath(f"//cache[capability/@id = 'CAT' and @level = '2']")
    if len(cache_list) > 0:
        res_present[RDT.L2.value] = len(cache_list)
        rdt_ins_name = "rdt_ins_l2[" + str(len(cache_list)) + "] = {"
        print("struct rdt_ins {}".format(rdt_ins_name), file=config)
        for idx, cache in enumerate(cache_list):
            err_dic = gen_rdt_str(cache, config)
            if err_dic:
                return err_dic;
        print("};\n", file=config)

    cache_list = board_etree.xpath(f"//cache[capability/@id = 'CAT' and @level = '3']")
    if len(cache_list) > 0:
        res_present[RDT.L3.value] = len(cache_list)
        rdt_ins_name = "rdt_ins_l3[" + str(len(cache_list)) + "] = {"
        print("struct rdt_ins {}".format(rdt_ins_name), file=config)
        for idx, cache in enumerate(cache_list):
            err_dic = gen_rdt_str(cache, config)
            if err_dic:
                return err_dic;
        print("};\n", file=config)

    cache_list = board_etree.xpath(f"//cache[capability/@id = 'MBA']")
    if len(cache_list) > 0:
        res_present[RDT.L2.value] = 1
        rdt_ins_name = "rdt_ins_mba[" + str(len(cache_list)) + "] = {"
        print("struct rdt_ins {}".format(rdt_ins_name), file=config)
        for idx, cache in enumerate(cache_list):
            err_dic = gen_rdt_str(cache, config)
            if err_dic:
                return err_dic;
        print("};\n", file=config)

    print("struct rdt_type res_cap_info[RDT_NUM_RESOURCES] = {", file=config)
    if res_present[RDT.L2.value] > 0:
        print("\t{", file=config)
        print("\t\t.res_id = RDT_RESID_L2,", file=config)
        print("\t\t.msr_qos_cfg = MSR_IA32_L2_QOS_CFG,", file=config)
        print("\t\t.msr_base = MSR_IA32_L2_MASK_BASE,", file=config)
        print("\t\t.num_ins = {},".format(res_present[RDT.L2.value]), file=config)
        print("\t\t.ins_array = rdt_ins_l2,", file=config)
        print("\t},", file=config)
    if res_present[RDT.L3.value] > 0:
        print("\t{", file=config)
        print("\t\t.res_id = RDT_RESID_L3,", file=config)
        print("\t\t.msr_qos_cfg = MSR_IA32_L3_QOS_CFG,", file=config)
        print("\t\t.msr_base = MSR_IA32_L3_MASK_BASE,", file=config)
        print("\t\t.num_ins = {},".format(res_present[RDT.L3.value]), file=config)
        print("\t\t.ins_array = rdt_ins_l3,", file=config)
        print("\t},", file=config)
    if res_present[RDT.MBA.value] > 0:
        print("\t{", file=config)
        print("\t\t.res_id = RDT_RESID_MBA,", file=config)
        print("\t\t.msr_qos_cfg = MSR_IA32_MBA_QOS_CFG,", file=config)
        print("\t\t.msr_base = MSR_IA32_MBA_MASK_BASE,", file=config)
        print("\t\t.num_ins = {},".format(res_present[RDT.MBA.value]), file=config)
        print("\t\t.ins_array = rdt_ins_mba,", file=config)
        print("\t},", file=config)
    print("};\n", file=config)

    print("#endif\n", file=config)

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
                print("static const struct acrn_{}state_data board_cpu_{}x[0];".format(domain_str, domain_str), file=config)
                print("", file=config)
                data_statues = False
                break
            else:
                print("static const struct acrn_{}state_data board_cpu_{}x[{}] = {{".format(domain_str, domain_str, data_len), file=config)
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
        acrn_config_utilities.BOARD_INFO_FILE, "<CPU_BRAND>", "</CPU_BRAND>")
    cx_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<CX_INFO>", "</CX_INFO>")
    px_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")

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

    scenario_etree = lxml.etree.parse(acrn_config_utilities.SCENARIO_INFO_FILE)
    hidden_pdev_list = [x.replace('.', ':') for x in scenario_etree.xpath(f"//HIDDEN_PDEV/text()")]

    if board_cfg_lib.BOARD_NAME in list(board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB.keys()) and board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB[board_cfg_lib.BOARD_NAME] != 0:
        hidden_pdev_list += board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB[board_cfg_lib.BOARD_NAME]

    if len(hidden_pdev_list) > 0:
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


def gen_known_caps_pci_devs(config):
    """Generate information for known capabilities of pci devices"""
    known_caps_pci_devs = board_cfg_lib.get_known_caps_pci_devs()
    for dev,bdf_list in known_caps_pci_devs.items():
        if dev == "VMSIX":
            print("", file=config)
            bdf_list_len = len(bdf_list)
            if bdf_list_len == 0:
                print("const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM];", file=config)
                break
            for i in range(bdf_list_len):
                b = bdf_list[i].split(":")[0]
                d = bdf_list[i].split(":")[1].split(".")[0]
                f = bdf_list[i].split(".")[1]
                print("#define VMSIX_ON_MSI_DEV{}\t.bdf.bits = {{.b = 0x{}U, .d = 0x{}U, .f =0x{}U}},".format(i, b, d, f), file=config)

            for i in range(bdf_list_len):
                if i == 0:
                    print("const struct vmsix_on_msi_info vmsix_on_msi_devs[MAX_VMSIX_ON_MSI_PDEVS_NUM] = {", file=config)
                print("\t{{VMSIX_ON_MSI_DEV{}}},".format(i), file=config)
                if i == (bdf_list_len - 1):
                    print("};", file=config)

def gen_cpufreq_limits(config):
    allocation_dir = os.path.split(acrn_config_utilities.SCENARIO_INFO_FILE)[0] + "/configs/allocation.xml"
    allocation_etree = lxml.etree.parse(allocation_dir)
    cpu_list = board_cfg_lib.get_processor_info()
    max_cpu_num = len(cpu_list)

    print("\nstruct acrn_cpufreq_limits cpufreq_limits[MAX_PCPU_NUM] = {", file=config)
    for cpu_id in range(max_cpu_num):
        limit_node = get_node(f"//cpufreq/CPU[@id='{cpu_id}']/limits", allocation_etree)
        if limit_node != None:
            limit_guaranteed_lvl = get_node("./limit_guaranteed_lvl/text()", limit_node)
            limit_highest_lvl = get_node("./limit_highest_lvl/text()", limit_node)
            limit_lowest_lvl = get_node("./limit_lowest_lvl/text()", limit_node)
            limit_nominal_pstate = get_node("./limit_nominal_pstate/text()", limit_node)
            limit_highest_pstate = get_node("./limit_highest_pstate/text()", limit_node)
            limit_lowest_pstate = get_node("./limit_lowest_pstate/text()", limit_node)

            print("\t{", file=config)
            print(f"\t\t.guaranteed_hwp_lvl = {limit_guaranteed_lvl},", file=config)
            print(f"\t\t.highest_hwp_lvl = {limit_highest_lvl},", file=config)
            print(f"\t\t.lowest_hwp_lvl = {limit_lowest_lvl},", file=config)
            print(f"\t\t.nominal_pstate = {limit_nominal_pstate},", file=config)
            print(f"\t\t.performance_pstate = {limit_highest_pstate},", file=config)
            print("\t},", file=config)
    print("};", file=config)

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

    # gen known caps of pci dev info for platform
    gen_known_caps_pci_devs(config)

    gen_cpufreq_limits(config)

    return err_dic
