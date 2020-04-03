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


HI_MMIO_OFFSET = 0

class Bar_Mem:
    def __init__(self):
        self.addr = 0
        self.remapped = False


class Bar_Attr:
    def __init__(self):
        self.name = 0
        self.remappable = True


class Pci_Dev_Bar_Desc:
    def __init__(self):
        self.pci_dev_dic = {}
        self.pci_bar_dic = {}

PCI_DEV_BAR_DESC = Pci_Dev_Bar_Desc()


def get_value_after_str(line, key):
    """ Get the value after cstate string """
    idx = 0
    line_in_list = line.split()
    for idx_key, val in enumerate(line_in_list):
        if val == key:
            idx = idx_key
            break

    return line_in_list[idx + 1]


def check_bar_remappable(line):
    #TODO: check device BAR remappable per ACPI table

    return True


def get_size(line):

    # get size string from format, Region n: Memory at x ... [size=NK]
    size_str = line.split()[-1].strip(']').split('=')[1]
    if 'G' in size_str:
        size = int(size_str.strip('G')) * board_cfg_lib.SIZE_G
    elif 'M' in size_str:
        size = int(size_str.strip('M')) * board_cfg_lib.SIZE_M
    elif 'K' in size_str:
        size = int(size_str.strip('K')) * board_cfg_lib.SIZE_K
    else:
        size = int(size_str)

    return size

# round up the running bar_addr to the size of the incoming bar "line"
def remap_bar_addr_to_high(bar_addr, line):
    """Generate vbar address"""
    global HI_MMIO_OFFSET
    size = get_size(line)
    cur_addr = board_cfg_lib.round_up(bar_addr, size)
    HI_MMIO_OFFSET = cur_addr + size
    return cur_addr


def parser_pci():
    """ Parse PCI lines """
    cur_bdf = 0
    prev_bdf = 0
    tmp_bar_dic = {}
    bar_addr = bar_num = '0'
    cal_sub_pci_name = []

    pci_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")

    for line in pci_lines:
        tmp_bar_mem = Bar_Mem()
        # get pci bar information into PCI_DEV_BAR_DESC
        if "Region" in line and "Memory at" in line:
            #ignore memory region from SR-IOV capabilities
            if "size=" not in line:
                 continue

            bar_addr = int(get_value_after_str(line, "at"), 16)
            bar_num = line.split()[1].strip(':')
            if bar_addr >= board_cfg_lib.SIZE_4G or bar_addr < board_cfg_lib.SIZE_2G:
                if not tmp_bar_attr.remappable:
                    continue

                bar_addr = remap_bar_addr_to_high(HI_MMIO_OFFSET, line)
                tmp_bar_mem.remapped = True

            tmp_bar_mem.addr = hex(bar_addr)
            tmp_bar_dic[int(bar_num)] = tmp_bar_mem
        else:
            tmp_bar_attr = Bar_Attr()
            prev_bdf = cur_bdf
            pci_bdf = line.split()[0]
            tmp_bar_attr.name = " ".join(line.split(':')[1].split()[1:])

            # remove '[*]' in pci subname
            if '[' in tmp_bar_attr.name:
                tmp_bar_attr.name = tmp_bar_attr.name.rsplit('[', 1)[0]

            cal_sub_pci_name.append(tmp_bar_attr.name)
            tmp_bar_attr.remappable = check_bar_remappable(line)
            PCI_DEV_BAR_DESC.pci_dev_dic[pci_bdf] = tmp_bar_attr
            cur_bdf = pci_bdf

            if not prev_bdf:
                prev_bdf = cur_bdf

            if tmp_bar_dic and cur_bdf != prev_bdf:
                PCI_DEV_BAR_DESC.pci_bar_dic[prev_bdf] = tmp_bar_dic

            # clear the tmp_bar_dic before store the next dic
            tmp_bar_dic = {}

    # output all the pci device list to pci_device.h
    sub_name_count = collections.Counter(cal_sub_pci_name)

    if tmp_bar_dic:
        PCI_DEV_BAR_DESC.pci_bar_dic[cur_bdf] = tmp_bar_dic

    return sub_name_count


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
            tmp_sub_name = board_cfg_lib.undline_name(bar_attr.name) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(bar_attr.name.split()).upper() + "_" + str(i_cnt)

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

    tmp_sub_name = board_cfg_lib.undline_name(bar_attr.name) + "_" + str(i_cnt)
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

    sub_name_count = parser_pci()

    print("#define %-32s" % "PTDEV_HI_MMIO_SIZE", "       {}UL".format(hex(HI_MMIO_OFFSET)), file=config)

    compared_bdf = []
    for cnt_sub_name in sub_name_count.keys():
        i_cnt = 0
        for bdf, bar_attr in PCI_DEV_BAR_DESC.pci_dev_dic.items():
            if cnt_sub_name == bar_attr.name and bdf not in compared_bdf:
                compared_bdf.append(bdf)
            else:
                continue

            print("",file=config)
            write_pbdf(i_cnt, bdf, bar_attr, config)
            write_vbar(i_cnt, bdf, PCI_DEV_BAR_DESC.pci_bar_dic, bar_attr, config)

            i_cnt += 1

    # write the end to the pci devices
    print("{0}".format(PCI_END_HEADER), file=config)
