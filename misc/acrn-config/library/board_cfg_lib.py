# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re
import sys
import common
import collections

BOARD_NAME = ''
BIOS_INFO = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']
BASE_BOARD = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']

LEGACY_TTYS = {
    'ttyS0':'0x3F8',
    'ttyS1':'0x2F8',
    'ttyS2':'0x3E8',
    'ttyS3':'0x2E8',
}

VALID_LEGACY_IRQ = []
ERR_LIST = {}

HEADER_LICENSE = common.open_license() + "\n"

# The data base contains hide pci device
KNOWN_HIDDEN_PDEVS_BOARD_DB = {
    'apl-mrb':['00:0d:0'],
    'apl-up2':['00:0d:0'],
}

TSN_DEVS = ["8086:4b30", "8086:4b31", "8086:4b32", "8086:4ba0", "8086:4ba1", "8086:4ba2",
            "8086:4bb0", "8086:4bb1", "8086:4bb2", "8086:a0ac", "8086:43ac", "8086:43a2"]
TPM_PASSTHRU_BOARD = ['whl-ipc-i5', 'whl-ipc-i7']

KNOWN_CAPS_PCI_DEVS_DB = {
    "TSN":TSN_DEVS,
}

def get_info(board_info, msg_s, msg_e):
    """
    Get information which specify by argument
    :param board_info: it is a file what contains information for script to read from
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    info_start = False
    info_end = False
    info_lines = []
    num = len(msg_s.split())

    with open(board_info, 'rt') as f_board:
        while True:

            line = f_board.readline()
            if not line:
                break

            if " ".join(line.split()[0:num]) == msg_s:
                info_start = True
                info_end = False
                continue

            if " ".join(line.split()[0:num]) == msg_e:
                info_start = False
                info_end = True
                continue

            if info_start and not info_end:
                info_lines.append(line)
                continue

            if not info_start and info_end:
                return info_lines


def handle_bios_info(config):
    """
    Handle bios information
    :param config: it is a file pointer of bios information for writing to
    """
    bios_lines = get_info(common.BOARD_INFO_FILE, "<BIOS_INFO>", "</BIOS_INFO>")
    board_lines = get_info(common.BOARD_INFO_FILE, "<BASE_BOARD_INFO>", "</BASE_BOARD_INFO>")
    print("/*", file=config)

    if not bios_lines or not board_lines:
        print(" * DMI info is not found", file=config)
    else:
        i_cnt = 0
        bios_board = BIOS_INFO + BASE_BOARD

        # remove the same value and keep origin sort
        bios_board_info = list(set(bios_board))
        bios_board_info.sort(key=bios_board.index)

        bios_board_lines = bios_lines + board_lines
        bios_info_len = len(bios_lines)
        for line in bios_board_lines:
            if i_cnt == bios_info_len:
                print(" *", file=config)

            i_cnt += 1

            for misc_info in bios_board_info:
                if misc_info == " ".join(line.split()[0:1]) or misc_info == \
                        " ".join(line.split()[0:2]) or misc_info == " ".join(line.split()[0:3]):
                    print(" * {0}".format(line.strip()), file=config)

    print(" */", file=config)


def handle_root_dev(line):
    """Handle if it match root device information pattern
    :param line: one line of information which had decoded to 'ASCII'
    """
    for root_type in line.split():
        # only support ext4 rootfs
        if "ext4" in root_type:
            return True

    return False


def get_max_clos_mask(board_file):
    """
    Parse CLOS information
    :param board_file: it is a file what contains board information for script to read from
    :return: type of rdt resource supported and their corresponding clos max.
    """
    rdt_res=[]
    rdt_res_clos_max=[]
    rdt_res_mask_max=[]

    clos_lines = get_info(board_file, "<CLOS_INFO>", "</CLOS_INFO>")
    for line in clos_lines:
        if line.split(':')[0].strip() == "rdt resources supported":
            rdt_res = line.split(':')[1].strip()
        elif line.split(':')[0].strip() == "rdt resource clos max":
            rdt_res_clos_max = line.split(':')[1].strip()
        elif line.split(':')[0].strip() == "rdt resource mask max":
            rdt_res_mask_max = line.split(':')[1].strip()

    if (len(rdt_res) == 0) or (len(rdt_res_clos_max) == 0):
        return rdt_res, rdt_res_clos_max, rdt_res_mask_max
    else:
        return list(re.split(', |\s |,', rdt_res)), list(map(int, rdt_res_clos_max.split(','))), list(re.split(', |\s |,', rdt_res_mask_max))


def get_rootfs(config_file):
    """
    This will get rootfs partition from board information
    :param config_file: it is a file which contain board information
    :return: rootfs partition list
    """
    root_dev_list = []
    rootfs_lines = get_info(config_file, "<BLOCK_DEVICE_INFO>", "</BLOCK_DEVICE_INFO>")

    # none 'BLOCK_DEVICE_INFO' tag
    if rootfs_lines == None:
        return root_dev_list

    for rootfs_line in rootfs_lines:
        if not rootfs_line:
            break

        if not handle_root_dev(rootfs_line):
            continue

        root_dev = rootfs_line.strip().split(':')[0]
        root_dev_list.append(root_dev)

    return (root_dev_list, len(root_dev_list))


def clos_info_parser(board_info):
    """ Parse CLOS information """
    return get_max_clos_mask(board_info)


def get_valid_irq(board_info):
    """
     This is get available irq from board info file
     :param board_info:  it is a file what contains board information for script to read from
     :return: None
     """
    global VALID_LEGACY_IRQ
    val_irq = []
    irq_info_lines = get_info(board_info, "<AVAILABLE_IRQ_INFO>", "</AVAILABLE_IRQ_INFO>")
    for irq_string in irq_info_lines:
        val_irq = [x.strip() for x in irq_string.split(',')]

    VALID_LEGACY_IRQ = val_irq


def alloc_irq():
    """
      This is allocated an available irq
      :return: free irq
      """
    irq_val = VALID_LEGACY_IRQ.pop(0)

    return irq_val


def parser_hv_console():
    """
    There may be 3 types in the console item
    1. BDF:(00:18.2) seri:/dev/ttyS2
    2. /dev/ttyS2
    3. ttyS2
    """
    ttys_n = ''
    err_dic = {}
    ttys = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "DEBUG_OPTIONS", "SERIAL_CONSOLE")

    if not ttys or ttys == None:
        return (err_dic, ttys_n)

    if ttys and 'BDF' in ttys or '/dev' in ttys:
        ttys_n = ttys.split('/')[2]
    else:
        ttys_n = ttys

    return (err_dic, ttys_n)


def get_processor_info():
    """
    Get cpu processor list
    :param board_info: it is a file what contains board information
    :return: cpu processor list
    """
    processor_list = []
    tmp_list = []
    processor_info = get_info(common.BOARD_INFO_FILE, "<CPU_PROCESSOR_INFO>", "</CPU_PROCESSOR_INFO>")

    if not processor_info:
        key = "CPU PROCESSOR_INFO error:"
        ERR_LIST[key] = "CPU core is not exists"
        return processor_list

    for processor_line in processor_info:
        if not processor_line:
            break

        processor_list = processor_line.strip().split(',')
        for processor in processor_list:
            tmp_list.append(processor.strip())
        break

    return tmp_list


def get_native_ttys_info(board_info):
    """
    Get ttySn from board info
    :param board_info: it is a file what contains board information for script to read from
    :return: serial port list
    """
    ttys_list = []
    ttys_info = get_info(board_info, "<TTYS_INFO>", "</TTYS_INFO>")

    for ttys_line in ttys_info:
        if not ttys_line:
            break

        ttys_dev = ttys_line.split()[0].split(':')[1]
        ttysn = ttys_dev.split('/')[-1]
        # currently SOS console can only support legacy serial port
        if ttysn not in list(LEGACY_TTYS.keys()):
            continue
        ttys_list.append(ttys_dev)

    return ttys_list

def get_total_mem():
    """
    get total memory size from config file which is dumped from native board
    :return: integer number of total memory size, Unit: MByte
    """
    scale_to_mb = 1
    total_mem_mb = scale_to_mb
    mem_lines = get_info(common.BOARD_INFO_FILE, "<TOTAL_MEM_INFO>", "</TOTAL_MEM_INFO>")
    for mem_line in mem_lines:
        mem_info_list = mem_line.split()

    if len(mem_info_list) <= 1:
        return total_mem_mb

    if mem_info_list[1] == "kB":
        scale_to_mb = 1024

    total_mem_mb = int(mem_info_list[0]) / scale_to_mb
    return total_mem_mb


def get_pci_info(board_info):
    pci_bdf_vpid = {}
    pci_vid_start = False
    pci_vid_end = False
    pci_desc = {}
    pci_start = False
    pci_end = False

    with open(board_info, 'r') as f:
        while True:
            line = f.readline()
            if not line:
                break

            s = " "
            if s.join(line.split()[0:2]) == "<PCI_DEVICE>":
                pci_start = True
                pci_end = False
                continue

            if s.join(line.split()[0:2]) == "</PCI_DEVICE>":
                pci_start = False
                pci_end = True
                continue

            # all pci device wiht description
            if pci_start and not pci_end:
                if "Region" in line and "Memory at" in line:
                    continue
                bdf = line.split()[0]
                pci_desc[bdf] = line

            if s.join(line.split()[0:2]) == "<PCI_VID_PID>":
                pci_vid_start = True
                pci_vid_end = False
                continue

            if s.join(line.split()[0:2]) == "</PCI_VID_PID>":
                pci_vid_start = False
                pci_vid_end = True
                continue

            # all pci device with vid/pid and bdf
            if pci_vid_start and not pci_vid_end:
                bdf_str = line.split()[0]
                vid_pid = line.split()[2]
                pci_bdf_vpid[bdf_str] = vid_pid

    return (pci_desc, pci_bdf_vpid)

HI_MMIO_OFFSET = 0

class Bar_Mem:
    def __init__(self):
        self.addr = 0
        self.remapped = False


class Bar_Attr:
    def __init__(self):
        self.name = 0
        self.remappable = True
        self.name_w_i_cnt = 0

class Pci_Dev_Bar_Desc:
    def __init__(self):
        self.pci_dev_dic = {}
        self.pci_bar_dic = {}

PCI_DEV_BAR_DESC = Pci_Dev_Bar_Desc()
SUB_NAME_COUNT = {}


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
        size = int(size_str.strip('G')) * common.SIZE_G
    elif 'M' in size_str:
        size = int(size_str.strip('M')) * common.SIZE_M
    elif 'K' in size_str:
        size = int(size_str.strip('K')) * common.SIZE_K
    else:
        size = int(size_str)

    return size

# round up the running bar_addr to the size of the incoming bar "line"
def remap_bar_addr_to_high(bar_addr, line):
    """Generate vbar address"""
    global HI_MMIO_OFFSET
    size = get_size(line)
    cur_addr = common.round_up(bar_addr, size)
    HI_MMIO_OFFSET = cur_addr + size
    return cur_addr


def parser_pci():
    """ Parse PCI lines """
    global SUB_NAME_COUNT, HI_MMIO_OFFSET
    cur_bdf = 0
    prev_bdf = 0
    tmp_bar_dic = {}
    bar_addr = bar_num = '0'
    cal_sub_pci_name = []

    pci_lines = get_info(common.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")

    for line in pci_lines:
        tmp_bar_mem = Bar_Mem()
        # get pci bar information into board_cfg_lib.PCI_DEV_BAR_DESC
        if "Region" in line and "Memory at" in line:
            #ignore memory region from SR-IOV capabilities
            if "size=" not in line:
                 continue

            bar_addr = int(get_value_after_str(line, "at"), 16)
            bar_num = line.split()[1].strip(':')
            if bar_addr >= common.SIZE_4G or bar_addr < common.SIZE_2G:
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
    SUB_NAME_COUNT = collections.Counter(cal_sub_pci_name)

    if tmp_bar_dic:
        PCI_DEV_BAR_DESC.pci_bar_dic[cur_bdf] = tmp_bar_dic


def is_rdt_supported():
    """
    Returns True if platform supports RDT else False
    """
    (rdt_resources, rdt_res_clos_max, _) = clos_info_parser(common.BOARD_INFO_FILE)
    if len(rdt_resources) == 0 or len(rdt_res_clos_max) == 0:
        return False
    else:
        return True


def is_rdt_enabled():
    """
    Returns True if RDT enabled else False
    """
    rdt_enabled = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "RDT", "RDT_ENABLED")
    if is_rdt_supported() and rdt_enabled == 'y':
        return True
    return False


def is_cdp_enabled():
    """
    Returns True if platform supports RDT/CDP else False
    """
    rdt_enabled = is_rdt_enabled()
    cdp_enabled = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "RDT", "CDP_ENABLED")
    if rdt_enabled and cdp_enabled == 'y':
        return True

    return False


def get_rdt_select_opt():

    support_sel = ['n']
    if is_rdt_supported():
        support_sel.append('y')
    return support_sel


def get_common_clos_max():

    common_clos_max = 0
    (res_info, rdt_res_clos_max, clos_max_mask_list) = clos_info_parser(common.BOARD_INFO_FILE)
    if is_rdt_enabled() and not is_cdp_enabled():
        common_clos_max = min(rdt_res_clos_max)

    if is_cdp_enabled():
        tmp_clos_max_list = []
        for res, clos_max in zip(res_info, rdt_res_clos_max):
            if res == 'MBA':
                tmp_clos_max_list.append(clos_max)
            else:
                tmp_clos_max_list.append(clos_max//2)
        common_clos_max = min(tmp_clos_max_list)

    return common_clos_max


def get_sub_pci_name(i_cnt, bar_attr):
    tmp_sub_name = ''
    # if there is only one host bridge, then will discard the index of suffix
    if i_cnt == 0 and bar_attr.name.upper() == "HOST BRIDGE":
        tmp_sub_name = "_".join(bar_attr.name.split()).upper()
    else:
        if '-' in bar_attr.name:
            tmp_sub_name = common.undline_name(bar_attr.name) + "_" + str(i_cnt)
        else:
            tmp_sub_name = "_".join(bar_attr.name.split()).upper() + "_" + str(i_cnt)

    return tmp_sub_name

def get_known_caps_pci_devs():
    known_caps_pci_devs = {}
    vpid_lines = get_info(common.BOARD_INFO_FILE, "<PCI_VID_PID>", "</PCI_VID_PID>")
    for dev,known_dev in KNOWN_CAPS_PCI_DEVS_DB.items():
        if dev not in known_caps_pci_devs:
            known_caps_pci_devs[dev] = []
        for k_dev in known_dev:
            for vpid_line in vpid_lines:
                if k_dev in vpid_line:
                    bdf = vpid_line.split()[0]
                    known_caps_pci_devs[dev].append(bdf)
                    break

    return known_caps_pci_devs


def is_tpm_passthru():

    tpm_passthru = False
    (_, board) = common.get_board_name()
    if board in TPM_PASSTHRU_BOARD:
        tpm_passthru = True

    return tpm_passthru
