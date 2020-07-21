# Copyright (C) 2020 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import board_cfg_lib
import scenario_cfg_lib

VBAR_INFO_DEFINE="""#ifndef VBAR_BASE_H_
#define VBAR_BASE_H_
"""

VBAR_INFO_ENDIF="""#endif /* VBAR_BASE_H_ */"""

def write_vbar(i_cnt, bdf, pci_bar_dic, bar_attr, config):
    """
    Parser and generate vbar
    :param i_cnt: the number of pci devices have the same PCI sub class name
    :param bdf: it is a string what contains BDF
    :param pci_bar_dic: it is a dictionary of pci vbar for those BDF
    :param bar_attr: it is a class, contains PIC bar attribute
    :param config: it is a file pointer of pci information for writing to
    """
    align = ' ' * 54
    ptdev_mmio_str = ''

    tmp_sub_name = board_cfg_lib.get_sub_pci_name(i_cnt, bar_attr)
    if bdf in pci_bar_dic.keys():
        bar_list = list(pci_bar_dic[bdf].keys())
        bar_len = len(bar_list)
        bar_num = 0
        for bar_i in bar_list:
            if not bar_attr.remappable:
                print("/* TODO: add {} 64bit BAR support */".format(tmp_sub_name), file=config)

            bar_num += 1
            bar_val = pci_bar_dic[bdf][bar_i].addr
            if pci_bar_dic[bdf][bar_i].remapped:
                ptdev_mmio_str = 'PTDEV_HI_MMIO_START + '

            if bar_num == bar_len:
                if bar_len == 1:
                    print("#define %-38s" % (tmp_sub_name+"_VBAR"), "       .vbar_base[{}] = {}{}UL".format(bar_i, ptdev_mmio_str, bar_val), file=config)
                else:
                    print("{}.vbar_base[{}] = {}{}UL".format(align, bar_i, ptdev_mmio_str, bar_val), file=config)
            elif bar_num == 1:
                print("#define %-38s" % (tmp_sub_name+"_VBAR"), "       .vbar_base[{}] = {}{}UL, \\".format(bar_i, ptdev_mmio_str, bar_val), file=config)
            else:
                print("{}.vbar_base[{}] = {}{}UL, \\".format(align, bar_i, ptdev_mmio_str, bar_val), file=config)
        print("", file=config)


def generate_file(config):
    # start to generate board_info.h
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    print(VBAR_INFO_DEFINE, file=config)
    common.get_vm_types()
    pre_vm = False
    for vm_type in common.VM_TYPES.values():
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
            pre_vm = True

    if not pre_vm:
        print(VBAR_INFO_ENDIF, file=config)
        return

    compared_bdf = []
    for cnt_sub_name in board_cfg_lib.SUB_NAME_COUNT.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            write_vbar(i_cnt, bdf, board_cfg_lib.PCI_DEV_BAR_DESC.pci_bar_dic, bar_attr, config)

            i_cnt += 1

    print(VBAR_INFO_ENDIF, file=config)
