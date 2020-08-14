# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import board_cfg_lib
import scenario_cfg_lib

MISC_CFG_HEADER = """#ifndef MISC_CFG_H
#define MISC_CFG_H"""

NATIVE_TTYS_DIC = {}
MISC_CFG_END = """#endif /* MISC_CFG_H */"""


class Vuart:

    t_vm_id = {}
    t_vuart_id = {}
    v_type = {}
    v_base = {}
    v_irq = {}


def get_valid_ttys_for_vuart(ttys_n):
    """
    Get available ttysn list for vuart0/vuart1
    :param ttys_n: the serial port was chosen as hv console
     """
    vuart0_valid = []
    vuart1_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
    ttys_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")
    if ttys_lines:
        vuart0_valid.clear()
        for tty_line in ttys_lines:
            tmp_dic = {}
            #seri:/dev/ttySx type:mmio base:0x91526000 irq:4 [bdf:"00:18.0"]
            #seri:/dev/ttySy type:portio base:0x2f8 irq:5
            tty = tty_line.split('/')[2].split()[0]
            ttys_irq = tty_line.split()[3].split(':')[1].strip()
            ttys_type = tty_line.split()[1].split(':')[1].strip()
            tmp_dic['irq'] = int(ttys_irq)
            tmp_dic['type'] = ttys_type
            NATIVE_TTYS_DIC[tty] = tmp_dic
            vuart0_valid.append(tty)
            if tty and tty in vuart1_valid:
                vuart1_valid.remove(tty)

    if not vuart1_valid:
        common.print_yel("ttyS are fully used. ttyS0 is used for hv_console, ttyS1 is used for vuart1!", warn=True)
        vuart1_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
        if ttys_n in vuart1_valid:
            vuart1_valid.remove(ttys_n)

    return (vuart0_valid, vuart1_valid)


def get_vuart_settings():
    """
    Get vuart setting from scenario setting
    :return: vuart0/vuart1 setting dictionary
    """
    err_dic = {}
    vuart0_setting = {}
    vuart1_setting = {}

    (err_dic, ttys_n) = board_cfg_lib.parser_hv_console()
    if err_dic:
        return err_dic

    if ttys_n:
        (vuart0_valid, vuart1_valid) = get_valid_ttys_for_vuart(ttys_n)

        # VUART0 setting
        if ttys_n not in list(NATIVE_TTYS_DIC.keys()):
            vuart0_setting['ttyS0'] = board_cfg_lib.alloc_irq()
        else:
            if int(NATIVE_TTYS_DIC[ttys_n]['irq']) >= 16:
                vuart0_setting[ttys_n] = board_cfg_lib.alloc_irq()
            else:
                vuart0_setting[ttys_n] = NATIVE_TTYS_DIC[ttys_n]['irq']
    else:
        vuart1_valid = ['ttyS1']

    # VUART1 setting
    # The IRQ of vUART1(COM2) might be hard-coded by SOS ACPI table(i.e. host ACPI),
    # so we had better follow native COM2 IRQ assignment for vUART1 if COM2 is a legacy ttyS,
    # otherwise function of vUART1 would be failed. If host COM2 does not exist or it is a PCI ttyS,
    # then we could allocate a free IRQ for vUART1.

    if 'ttyS1' in NATIVE_TTYS_DIC.keys() \
        and NATIVE_TTYS_DIC['ttyS1']['type'] == "portio" \
        and 'irq' in list(NATIVE_TTYS_DIC['ttyS1'].keys()) \
        and NATIVE_TTYS_DIC['ttyS1']['irq'] < 16:
        vuart1_setting['ttyS1'] = NATIVE_TTYS_DIC['ttyS1']['irq']
    else:
        vuart1_setting[vuart1_valid[0]] = board_cfg_lib.alloc_irq()

    return (err_dic, vuart0_setting, vuart1_setting)


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
        (err_dic, vuart0_dic, vuart1_dic) = get_vuart_settings()
    else:
        sos_cmdlines = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "os_config", "bootargs").values())

        sos_rootfs = list(common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, "os_config", "rootfs").values())
        (err_dic, vuart0_dic, vuart1_dic) = get_vuart_settings()

    return (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic)


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
