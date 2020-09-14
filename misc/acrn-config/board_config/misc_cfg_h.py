# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import board_cfg_lib
import scenario_cfg_lib

MISC_CFG_HEADER = """#ifndef MISC_CFG_H
#define MISC_CFG_H"""

MISC_CFG_END = """#endif /* MISC_CFG_H */"""


class Vuart:

    t_vm_id = {}
    t_vuart_id = {}
    v_type = {}
    v_base = {}
    v_irq = {}


def sos_bootarg_diff(sos_cmdlines, config):

    if sos_cmdlines:
        sos_len = len(sos_cmdlines)
        i = 0
        for sos_cmdline in sos_cmdlines:
            if not sos_cmdline:
                continue

            i += 1
            if i == 1:
                if sos_len == 1:
                    print('#define SOS_BOOTARGS_DIFF\t"{}"'.format(sos_cmdline.strip('"')), file=config)
                else:
                    print('#define SOS_BOOTARGS_DIFF\t"{} " \\'.format(sos_cmdline), file=config)
            else:
                if i < sos_len:
                    print('\t\t\t\t"{} "\t\\'.format(sos_cmdline), file=config)
                else:
                    print('\t\t\t\t"{}"'.format(sos_cmdline), file=config)


def parse_boot_info():

    err_dic = {}

    if 'SOS_VM' in common.VM_TYPES.values():
        sos_cmdlines = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "board_private", "bootargs").values())
        sos_rootfs = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "board_private", "rootfs").values())
        (err_dic, vuart0_dic, vuart1_dic) = scenario_cfg_lib.get_sos_vuart_settings(launch_flag=False)
    else:
        sos_cmdlines = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "os_config", "bootargs").values())

        sos_rootfs = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "os_config", "rootfs").values())
        (err_dic, vuart0_dic, vuart1_dic) = scenario_cfg_lib.get_sos_vuart_settings(launch_flag=False)

    return (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic)


def clos_per_vm_gen(config):

    clos_per_vm = {}
    clos_per_vm = common.get_leaf_tag_map(
            common.SCENARIO_INFO_FILE, "clos", "vcpu_clos")
    for i,clos_list_i in clos_per_vm.items():
        clos_config = scenario_cfg_lib.clos_assignment(clos_per_vm, i)
        print("#define VM{0}_VCPU_CLOS\t\t\t{1}".format(i, clos_config['clos_map']), file=config)


def generate_file(config):
    """
    Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    board_cfg_lib.get_valid_irq(common.BOARD_INFO_FILE)

    # get the vuart0/vuart1 which user chosed from scenario.xml of board_private section
    (err_dic, ttys_n) = board_cfg_lib.parser_hv_console()
    if err_dic:
        return err_dic

    # parse sos_bootargs/rootfs/console
    (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic) = parse_boot_info()
    if err_dic:
        return err_dic

    if vuart0_dic:
        # parse to get poart/base of vuart0/vuart1
        vuart0_port_base = board_cfg_lib.LEGACY_TTYS[list(vuart0_dic.keys())[0]]
        vuart0_irq = vuart0_dic[list(vuart0_dic.keys())[0]]

    vuart1_port_base = board_cfg_lib.LEGACY_TTYS[list(vuart1_dic.keys())[0]]
    vuart1_irq = vuart1_dic[list(vuart1_dic.keys())[0]]

    # parse the setting ttys vuatx dic: {vmid:base/irq}
    vuart0_setting = Vuart()
    vuart1_setting = Vuart()
    vuart0_setting = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 0)
    vuart1_setting = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 1)

    # sos command lines information
    sos_cmdlines = [i for i in sos_cmdlines[0].split() if i != '']

    # add maxcpus parameter into sos cmdlines if there are pre-launched VMs
    pcpu_list = board_cfg_lib.get_processor_info()
    cpu_affinity = common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "cpu_affinity", "pcpu_id")
    pre_cpu_list = []
    sos_cpu_num = 0
    for vmid, cpu_list in cpu_affinity.items():
        if vmid in common.VM_TYPES and cpu_list != [None]:
            vm_type = common.VM_TYPES[vmid]
            load_type = ''
            if vm_type in scenario_cfg_lib.VM_DB:
                load_type = scenario_cfg_lib.VM_DB[vm_type]['load_type']
            if load_type == "PRE_LAUNCHED_VM":
                pre_cpu_list += cpu_list
            elif load_type == "SOS_VM":
                sos_cpu_num += len(cpu_list)
    if sos_cpu_num == 0:
        sos_cpu_num_max = len(list(set(pcpu_list) - set(pre_cpu_list)))
    else:
        sos_cpu_num_max = sos_cpu_num
    if sos_cpu_num_max > 0:
        sos_cmdlines.append('maxcpus='+str(sos_cpu_num_max))

    # get native rootfs list from board_info.xml
    (root_devs, root_dev_num) = board_cfg_lib.get_rootfs(common.BOARD_INFO_FILE)

    # start to generate misc_cfg.h
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    print("{}".format(MISC_CFG_HEADER), file=config)
    print("", file=config)

    # define rootfs with macro
    #for i in range(root_dev_num):
    #    print('#define ROOTFS_{}\t\t"root={} "'.format(i, root_devs[i]), file=config)

    # sos rootfs and console
    if "SOS_VM" in common.VM_TYPES.values():
        print('#define SOS_ROOTFS\t\t"root={} "'.format(sos_rootfs[0]), file=config)
        if ttys_n:
            print('#define SOS_CONSOLE\t\t"console={} "'.format(ttys_n), file=config)
        else:
            print('#define SOS_CONSOLE\t\t" "', file=config)

    # sos com base/irq
    i_type = 0
    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_type == "SOS_VM":
            i_type = vm_i
            break

    if "SOS_VM" in common.VM_TYPES.values():
        if vuart0_dic:
            print("#define SOS_COM1_BASE\t\t{}U".format(vuart0_port_base), file=config)
            print("#define SOS_COM1_IRQ\t\t{}U".format(vuart0_irq), file=config)
        else:
            print("#define SOS_COM1_BASE\t\t0U", file=config)
            print("#define SOS_COM1_IRQ\t\t0U", file=config)

        if vuart1_setting[i_type]['base'] != "INVALID_COM_BASE":
            print("#define SOS_COM2_BASE\t\t{}U".format(vuart1_port_base), file=config)
            print("#define SOS_COM2_IRQ\t\t{}U".format(vuart1_irq), file=config)

        # sos boot command line
        print("", file=config)

    if "SOS_VM" in common.VM_TYPES.values():
        sos_bootarg_diff(sos_cmdlines, config)
        print("", file=config)

    common_clos_max = board_cfg_lib.get_common_clos_max()
    max_mba_clos_entries = common_clos_max
    max_cache_clos_entries = common_clos_max

    if board_cfg_lib.is_cdp_enabled():
        max_cache_clos_entries_cdp_enable = 2 * common_clos_max
        (res_info, rdt_res_clos_max, clos_max_mask_list) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
        common_clos_max_cdp_disable = min(rdt_res_clos_max)

        print("#ifdef CONFIG_RDT_ENABLED", file=config)
        print("#ifdef CONFIG_CDP_ENABLED", file=config)
        print("#define HV_SUPPORTED_MAX_CLOS\t{}U".format(common_clos_max), file=config)
        print("#define MAX_CACHE_CLOS_NUM_ENTRIES\t{}U".format(max_cache_clos_entries_cdp_enable), file=config)
        print("#else", file=config)
        print("#define HV_SUPPORTED_MAX_CLOS\t{}U".format(common_clos_max_cdp_disable), file=config)
        print("#define MAX_CACHE_CLOS_NUM_ENTRIES\t{}U".format(max_cache_clos_entries), file=config)
        print("#endif", file=config)
        print("#define MAX_MBA_CLOS_NUM_ENTRIES\t{}U".format(max_mba_clos_entries), file=config)
    else:
        print("#ifdef CONFIG_RDT_ENABLED", file=config)
        print("#define HV_SUPPORTED_MAX_CLOS\t{}U".format(common_clos_max), file=config)
        print("#define MAX_MBA_CLOS_NUM_ENTRIES\t{}U".format(max_mba_clos_entries), file=config)
        print("#define MAX_CACHE_CLOS_NUM_ENTRIES\t{}U".format(max_cache_clos_entries), file=config)
        if not board_cfg_lib.is_rdt_supported():
            print("#endif", file=config)

    print("", file=config)

    if board_cfg_lib.is_rdt_supported():
        (rdt_resources, rdt_res_clos_max, _) = board_cfg_lib.clos_info_parser(common.BOARD_INFO_FILE)
        cat_mask_list = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "RDT", "CLOS_MASK")
        mba_delay_list = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "RDT", "MBA_DELAY")
        idx = 0
        for mba_delay_mask in mba_delay_list:
            print("#define MBA_MASK_{}\t\t\t{}U".format(idx, mba_delay_mask), file=config)
            idx += 1

        idx = 0
        for cat_mask in cat_mask_list:
            print("#define CLOS_MASK_{}\t\t\t{}U".format(idx, cat_mask), file=config)
            idx += 1
        print("", file=config)

        clos_per_vm_gen(config)
        print("#endif", file=config)
        print("", file=config)

    vm0_pre_launch = False
    common.get_vm_types()
    for vm_idx,vm_type in common.VM_TYPES.items():
        if vm_idx == 0 and scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
            vm0_pre_launch = True

    if vm0_pre_launch and board_cfg_lib.is_tpm_passthru():
        print("#define VM0_PASSTHROUGH_TPM", file=config)
        print("#define VM0_TPM_BUFFER_BASE_ADDR   0xFED40000UL", file=config)
        print("#define VM0_TPM_BUFFER_SIZE        0x5000UL", file=config)
        print("", file=config)

    print("{}".format(MISC_CFG_END), file=config)

    return err_dic
