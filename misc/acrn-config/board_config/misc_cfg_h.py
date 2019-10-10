# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import board_cfg_lib

MISC_CFG_HEADER = """
#ifndef MISC_CFG_H
#define MISC_CFG_H
"""

MISC_CFG_END = """#endif /* MISC_CFG_H */"""

class Vuart:

    t_vm_id = []
    t_vuart_id = []
    v_type = []
    v_base = []
    v_irq = []

    def style_check_1(self):
        self.v_irq = []

    def style_check_2(self):
        self.v_irq = []


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
                    print('#define SOS_BOOTARGS_DIFF\t{}'.format(sos_cmdline), file=config)
                else:
                    print('#define SOS_BOOTARGS_DIFF\t"{} " \\'.format(sos_cmdline), file=config)
            else:
                if i < sos_len:
                    print('\t\t\t\t"{} "\t\\'.format(sos_cmdline), file=config)
                else:
                    print('\t\t\t\t"{}"'.format(sos_cmdline), file=config)


def parse_boot_info():

    err_dic = {}
    vm_types = []

    (err_dic, scenario_name) = board_cfg_lib.get_scenario_name()
    if err_dic:
        return (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic, vm_types)

    if scenario_name != "logical_partition":
        sos_cmdlines = board_cfg_lib.get_sub_leaf_tag(board_cfg_lib.SCENARIO_INFO_FILE, "board_private", "bootargs")
        sos_rootfs = board_cfg_lib.get_sub_leaf_tag(board_cfg_lib.SCENARIO_INFO_FILE, "board_private", "rootfs")
        (err_dic, vuart0_dic, vuart1_dic) = board_cfg_lib.get_board_private_vuart("board_private", "console")
    else:
        sos_cmdlines = board_cfg_lib.get_sub_leaf_tag(board_cfg_lib.SCENARIO_INFO_FILE, "os_config", "bootargs")

        sos_rootfs = board_cfg_lib.get_sub_leaf_tag(board_cfg_lib.SCENARIO_INFO_FILE, "os_config", "rootfs")
        (err_dic, vuart0_dic, vuart1_dic) = board_cfg_lib.get_board_private_vuart("os_config", "console")

    if err_dic:
        return (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic, vm_types)

    for i in range(board_cfg_lib.VM_COUNT):
        vm_type = board_cfg_lib.get_order_type_by_vmid(i)
        vm_types.append(vm_type)

    return (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic, vm_types)


def generate_file(config):
    """
    Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    board_cfg_lib.get_valid_irq(board_cfg_lib.BOARD_INFO_FILE)

    # get cpu processor list
    cpu_list = board_cfg_lib.get_processor_info()
    max_cpu_num = len(cpu_list)

    # get the vuart0/vuart1 which user chosed from scenario.xml of board_private section
    (err_dic, ttys_n) = board_cfg_lib.parser_vuart_console()
    if err_dic:
        return err_dic

    # parse sos_bootargs/rootfs/console
    (err_dic, sos_cmdlines, sos_rootfs, vuart0_dic, vuart1_dic, vm_types) = parse_boot_info()
    if err_dic:
        return err_dic

    # parse to get poart/base of vuart0/vuart1
    vuart0_port_base = board_cfg_lib.TTY_CONSOLE[list(vuart0_dic.keys())[0]]
    vuart0_irq = vuart0_dic[list(vuart0_dic.keys())[0]]
    vuart1_port_base = board_cfg_lib.TTY_CONSOLE[list(vuart1_dic.keys())[0]]
    vuart1_irq = vuart1_dic[list(vuart1_dic.keys())[0]]

    # parse the setting ttys vuatx dic: {vmid:base/irq}
    vuart0_setting = Vuart()
    vuart1_setting = Vuart()
    vuart0_setting = board_cfg_lib.get_vuart_info_id(board_cfg_lib.SCENARIO_INFO_FILE, 0)
    vuart1_setting = board_cfg_lib.get_vuart_info_id(board_cfg_lib.SCENARIO_INFO_FILE, 1)

    # sos command lines information
    sos_cmdlines = [i for i in sos_cmdlines[0].split() if i != '']

    # get native rootfs list from board_info.xml
    (root_devs, root_dev_num) = board_cfg_lib.get_rootfs(board_cfg_lib.BOARD_INFO_FILE)

    # start to generate misc_cfg.h
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    print("{}".format(MISC_CFG_HEADER), file=config)

    # define CONFIG_MAX_PCPCU_NUM
    print("#define CONFIG_MAX_PCPU_NUM\t{}U".format(max_cpu_num), file=config)

    # define rootfs with macro
    for i in range(root_dev_num):
        print('#define ROOTFS_{}\t\t"root={} "'.format(i, root_devs[i]), file=config)

    # sos rootfs and console
    print("", file=config)
    if "SOS_VM" in vm_types:
        print('#define SOS_ROOTFS\t\t"root={} "'.format(sos_rootfs[0]), file=config)
        print('#define SOS_CONSOLE\t\t"console={} "'.format(ttys_n), file=config)

    # sos com base/irq
    i_type = 0
    for vm_type in vm_types:
        if vm_type == "SOS_VM":
            break
        i_type += 1

    if "SOS_VM" in vm_types:
        print("#define SOS_COM1_BASE\t\t{}U".format(vuart0_port_base), file=config)
        print("#define SOS_COM1_IRQ\t\t{}U".format(vuart0_irq), file=config)
        if vuart1_setting[i_type]['base'] != "INVALID_COM_BASE":
            print("#define SOS_COM2_BASE\t\t{}U".format(vuart1_port_base), file=config)
            print("#define SOS_COM2_IRQ\t\t{}U".format(vuart1_irq), file=config)


    # sos boot command line
    print("", file=config)
    if "SOS_VM" in vm_types:
        sos_bootarg_diff(sos_cmdlines, config)
    print("{}".format(MISC_CFG_END), file=config)

    return err_dic
