# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import collections
import board_cfg_lib
import common

PCI_HEADER = r"""
#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_
"""
PCI_END_HEADER = r"""
#endif /* PCI_DEVICES_H_ */"""

def write_pbdf(i_cnt, bdf, bar_attr, config):
    """
    Parser and generate pbdf
    :param i_cnt: the number of pci devices have the same PCI sub class name
    :param bdf: it is a string what contains BDF
    :param bar_attr: it is a class, contains PIC bar attribute
    :param config: it is a file pointer of pci information for writing to
    """
    # if there is only one host bridge, then will discard the index of suffix
    if i_cnt == 0 and bar_attr.name.upper() == "HOST BRIDGE":
        tmp_sub_name = "_".join(bar_attr.name.split()).upper()
    else:
        if '-' in bar_attr.name:
            tmp_sub_name = common.undline_name(bar_attr.name) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(bar_attr.name.split()).upper() + "_" + str(i_cnt)

    board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic[bdf].name_w_i_cnt = tmp_sub_name

    bus = int(bdf.split(':')[0], 16)
    dev = int(bdf.split(':')[1].split('.')[0], 16)
    fun = int(bdf.split('.')[1], 16)
    print("#define %-32s" % tmp_sub_name, end="", file=config)
    print("        .pbdf.bits = {{.b = 0x{:02X}U, .d = 0x{:02X}U, .f = 0x{:02X}U}}".format(
        bus, dev, fun), end="", file=config)

    if not bar_attr.remappable:
        align = ' ' * 48
        print("\n{}/* TODO: add {} 64bit BAR support */".format(align, tmp_sub_name), file=config)
        return


def write_vbar(i_cnt, bdf, pci_bar_dic, bar_attr, config):
    """
    Parser and generate vbar
    :param i_cnt: the number of pci devices have the same PCI sub class name
    :param bdf: it is a string what contains BDF
    :param pci_bar_dic: it is a dictionary of pci vbar for those BDF
    :param bar_attr: it is a class, contains PIC bar attribute
    :param config: it is a file pointer of pci information for writing to
    """
    tail = 0
    align = ' ' * 48
    ptdev_mmio_str = ''

    tmp_sub_name = common.undline_name(bar_attr.name) + "_" + str(i_cnt)
    if bdf in pci_bar_dic.keys():
        bar_list = list(pci_bar_dic[bdf].keys())
        bar_len = len(bar_list)
        bar_num = 0
        for bar_i in bar_list:
            if not bar_attr.remappable:
                return

            if tail == 0:
                print(", \\", file=config)
                tail += 1
            bar_num += 1
            bar_val = pci_bar_dic[bdf][bar_i].addr
            if pci_bar_dic[bdf][bar_i].remapped:
                ptdev_mmio_str = 'PTDEV_HI_MMIO_START + '

            if bar_num == bar_len:
                print("{}.vbar_base[{}] = {}{}UL".format(align, bar_i, ptdev_mmio_str, bar_val), file=config)
            else:
                print("{}.vbar_base[{}] = {}{}UL, \\".format(
                    align, bar_i, ptdev_mmio_str, bar_val), file=config)

    else:
        print("", file=config)


def generate_file(config):
    """
    Get PCI device and generate pci_devices.h
    :param config: it is a file pointer of pci information for writing to
    """
    # write the license into pci
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)

    # add bios and base board info
    board_cfg_lib.handle_bios_info(config)

    # write the header into pci
    print("{0}".format(PCI_HEADER), file=config)

    sub_name_count = board_cfg_lib.parser_pci()

    print("#define %-32s" % "PTDEV_HI_MMIO_SIZE", "       {}UL".format(hex(board_cfg_lib.HI_MMIO_OFFSET)), file=config)

    compared_bdf = []
    for cnt_sub_name in sub_name_count.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            print("",file=config)
            write_pbdf(i_cnt, bdf, bar_attr, config)
            write_vbar(i_cnt, bdf, board_cfg_lib.PCI_DEV_BAR_DESC.pci_bar_dic, bar_attr, config)

            i_cnt += 1

    # write the end to the pci devices
    print("{0}".format(PCI_END_HEADER), file=config)
