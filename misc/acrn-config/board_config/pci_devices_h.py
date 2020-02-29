# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import collections
import board_cfg_lib

PCI_HEADER = r"""
#ifndef PCI_DEVICES_H_
#define PCI_DEVICES_H_
"""
PCI_END_HEADER = r"""
#endif /* PCI_DEVICES_H_ */"""


def get_value_after_str(line, key):
    """ Get the value after cstate string """
    idx = 0
    line_in_list = line.split()
    for idx_key, val in enumerate(line_in_list):
        if val == key:
            idx = idx_key
            break

    return line_in_list[idx + 1]


def parser_pci():
    """ Parse PCI lines """
    cur_bdf = 0
    prev_bdf = 0
    tmp_bar_dic = {}
    pci_dev_dic = {}
    pci_bar_dic = {}
    above_4G_mmio = False
    bar_addr = bar_num = '0'

    pci_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")
    for line in pci_lines:
        # get pci bar information into pci_bar_dic
        if "Region" in line and "Memory at" in line:
            #ignore memory region from SR-IOV capabilities
            if "size=" not in line:
                 continue
            bar_num = line.split()[1].strip(':')
            bar_addr = get_value_after_str(line, "at")
            if int(bar_addr, 16) > 0xffffffff:
                above_4G_mmio = True
            tmp_bar_dic[int(bar_num)] = hex(int(bar_addr, 16))
        else:
            prev_bdf = cur_bdf
            pci_bdf = line.split()[0]
            pci_sub_name = " ".join(line.split(':')[1].split()[1:])

            # remove '[*]' in pci subname
            if '[' in pci_sub_name:
                pci_sub_name = pci_sub_name.rsplit('[', 1)[0]

            pci_dev_dic[pci_bdf] = pci_sub_name
            cur_bdf = pci_bdf

            if not prev_bdf:
                prev_bdf = cur_bdf

            if tmp_bar_dic and cur_bdf != prev_bdf:
                pci_bar_dic[prev_bdf] = tmp_bar_dic

            # clear the tmp_bar_dic before store the next dic
            tmp_bar_dic = {}

    if above_4G_mmio:
        board_cfg_lib.print_yel("Currently ACRN does not support BAR size above 4G, please double check below possible items in BIOS:\n\
        1. GPU Aperture size is less than 1GB;\n\
        2. the device MMIO mapping region is below 4GB", warn=True)

    # output all the pci device list to pci_device.h
    sub_name_count = collections.Counter(pci_dev_dic.values())

    if tmp_bar_dic:
        pci_bar_dic[cur_bdf] = tmp_bar_dic

    return (pci_dev_dic, pci_bar_dic, sub_name_count)


def write_pbdf(i_cnt, bdf, subname, config):
    """
    Parser and generate pbdf
    :param i_cnt: the number of pci devices have the same PCI subname
    :param bdf: it is a string what contains BDF
    :param subname: it is a string belong to PIC subname
    :param config: it is a file pointer of pci information for writing to
    """
    # if there is only one host bridge, then will discard the index of suffix
    if i_cnt == 0 and subname.upper() == "HOST BRIDGE":
        tmp_sub_name = "_".join(subname.split()).upper()
    else:
        if '-' in subname:
            tmp_sub_name = board_cfg_lib.undline_name(subname) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(subname.split()).upper() + "_" + str(i_cnt)

    bus = int(bdf.split(':')[0], 16)
    dev = int(bdf.split(':')[1].split('.')[0], 16)
    fun = int(bdf.split('.')[1], 16)
    print("#define %-32s" % tmp_sub_name, end="", file=config)
    print("        .pbdf.bits = {{.b = 0x{:02X}U, .d = 0x{:02X}U, .f = 0x{:02X}U}}".format(
        bus, dev, fun), end="", file=config)


def write_vbar(bdf, pci_bar_dic, config):
    """
    Parser and generate vbar
    :param bdf: it is a string what contains BDF
    :param pci_bar_dic: it is a dictionary of pci vbar for those BDF
    :param config: it is a file pointer of pci information for writing to
    """
    tail = 0
    align = ' ' * 48
    if bdf in pci_bar_dic.keys():
        bar_list = list(pci_bar_dic[bdf].keys())
        bar_len = len(bar_list)
        bar_num = 0
        for bar_i in bar_list:
            if tail == 0:
                print(", \\", file=config)
                tail += 1
            bar_num += 1
            bar_val = pci_bar_dic[bdf][bar_i]

            if bar_num == bar_len:
                print("{}.vbar_base[{}] = {}UL".format(align, bar_i, bar_val), file=config)
            else:
                print("{}.vbar_base[{}] = {}UL, \\".format(
                    align, bar_i, bar_val), file=config)

        # print("", file=config)
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

    (pci_dev_dic, pci_bar_dic, sub_name_count) = parser_pci()


    compared_bdf = []
    for cnt_sub_name in sub_name_count.keys():
        i_cnt = 0
        for bdf, subname in pci_dev_dic.items():
            if cnt_sub_name == subname and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            write_pbdf(i_cnt, bdf, subname, config)
            write_vbar(bdf, pci_bar_dic, config)

            i_cnt += 1

    # write the end to the pci devices
    print("{0}".format(PCI_END_HEADER), file=config)
