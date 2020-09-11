# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import collections
import board_cfg_lib
import common

PCI_HEADER = r"""
#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_"""
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
    tmp_sub_name = board_cfg_lib.get_sub_pci_name(i_cnt, bar_attr)
    board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic[bdf].name_w_i_cnt = tmp_sub_name

    bus = int(bdf.split(':')[0], 16)
    dev = int(bdf.split(':')[1].split('.')[0], 16)
    fun = int(bdf.split('.')[1], 16)
    print("#define %-32s" % tmp_sub_name, end="", file=config)
    print("        .pbdf.bits = {{.b = 0x{:02X}U, .d = 0x{:02X}U, .f = 0x{:02X}U}}".format(
        bus, dev, fun), file=config)


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

    board_cfg_lib.parser_pci()

    compared_bdf = []
    for cnt_sub_name in board_cfg_lib.SUB_NAME_COUNT.keys():
        i_cnt = 0
        for bdf, bar_attr in board_cfg_lib.PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            print("",file=config)
            write_pbdf(i_cnt, bdf, bar_attr, config)

            i_cnt += 1

    # write the end to the pci devices
    print("{0}".format(PCI_END_HEADER), file=config)
