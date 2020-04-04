# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import re
import sys
import common

BOARD_NAME = ''
BIOS_INFO = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']
BASE_BOARD = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']

LEGACY_TTYS = {
    'ttyS0':'0x3F8',
    'ttyS1':'0x2F8',
    'ttyS2':'0x3E8',
    'ttyS3':'0x2E8',
}

NATIVE_CONSOLE_DIC = {}
VALID_LEGACY_IRQ = []
ERR_LIST = {}

HEADER_LICENSE = common.open_license() + "\n"

# The data base contains hide pci device
KNOWN_HIDDEN_PDEVS_BOARD_DB = {
    'apl-mrb':['00:0d:0'],
    'apl-up2':['00:0d:0'],
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


def get_order_type_by_vmid(idx):
    """
    This is get pre launched vm count
    :param idx: index of vm id
    :return: vm type of index to vmid
    """
    (err_dic, order_type) = common.get_load_order_by_vmid(common.SCENARIO_INFO_FILE, common.VM_COUNT, idx)
    if err_dic:
        ERR_LIST.update(err_dic)

    return order_type


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


def get_valid_console():
    """ Get valid console with mapping {ttyS:irq} returned """
    used_console_lines = get_info(common.BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")

    vuart0_valid_console = []
    vuart1_valid_console = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3', 'ttyS4', 'ttyS5', 'ttyS6', 'ttyS7']
    if used_console_lines:
        vuart0_valid_console.clear()
        for console in used_console_lines:
            #seri:/dev/ttySx type:mmio base:0x91526000 irq:4 bdf:"00:18.0"
            #seri:/dev/ttySy type:portio base:0x2f8 irq:5
            tty = console.split('/')[2].split()[0]
            ttys_irq = console.split()[3].split(':')[1].strip()
            NATIVE_CONSOLE_DIC[tty] = ttys_irq
            vuart0_valid_console.append(tty)
            if tty:
                vuart1_valid_console.remove(tty)

    return (vuart0_valid_console, vuart1_valid_console)


def console_to_show(board_info):
    """
     This is get available console from board info file
     :param board_info:  it is a file what contains board information for script to read from
     :return: available console
     """
    show_vuart1 = False
    (vuart0_valid_console, vuart1_valid_console) = get_valid_console()
    if not vuart1_valid_console:
        print_yel("Console are full used, sos_console/vuart1 have to chose one:", warn=True)
        vuart0_valid_console = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
        vuart1_valid_console = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
        show_vuart1 = True

    return (vuart0_valid_console, vuart1_valid_console, show_vuart1)


def parser_vuart_console():
    """
    There may be 3 types in the console item
    1. BDF:(00:18.2) seri:/dev/ttyS2
    2. /dev/ttyS2
    3. ttyS2
    """
    ttys_n = ''
    (err_dic, scenario_name) = common.get_scenario_name()

    ttys = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "DEBUG_OPTIONS", "SERIAL_CONSOLE")

    if not ttys or ttys == None:
        return (err_dic, ttys_n)

    if ttys and 'BDF' in ttys or '/dev' in ttys:
        ttys_n = ttys.split('/')[2]
    else:
        ttys_n = ttys

    return (err_dic, ttys_n)


def get_board_private_vuart(branch_tag, tag_console):
    """
    Get vuart_console from board setting
    :param tag_console: TTYS_INFO
    :return: vuart0/vuart1 console dictionary
    """
    err_dic = {}
    vuart0_console_dic = {}
    vuart1_console_dic = {}

    (err_dic, ttys_n) = parser_vuart_console()
    if err_dic:
        return err_dic

    if ttys_n:

        (vuart0_valid_console, vuart1_valid_console, show_vuart1) = console_to_show(common.BOARD_INFO_FILE)

        # VUART0
        if ttys_n not in list(NATIVE_CONSOLE_DIC.keys()):
            vuart0_console_dic[ttys_n] = alloc_irq()
        else:
            if int(NATIVE_CONSOLE_DIC[ttys_n]) >= 16:
                vuart0_console_dic[ttys_n] = alloc_irq()
            else:
                vuart0_console_dic[ttys_n] = NATIVE_CONSOLE_DIC[ttys_n]
    else:
        vuart1_valid_console = ['ttyS1']

    # VUART1
    if len(NATIVE_CONSOLE_DIC) >= 2 and 'ttyS1' in NATIVE_CONSOLE_DIC.keys():
        # There are more than 1 serial port in native, we need to use native ttyS1 info for vuart1 which include
        # base ioport and irq number.
        vuart1_console_dic['ttyS1'] = NATIVE_CONSOLE_DIC['ttyS1']
    else:
        # There is only one native serial port. We hardcode base ioport for vuart1 and allocate a irq which is
        # free in native env and use it for vuart1 irq number
        vuart1_console_dic[vuart1_valid_console[0]] = alloc_irq()

    return (err_dic, vuart0_console_dic, vuart1_console_dic)


def get_vuart_id(tmp_vuart, leaf_tag, leaf_text):
    """
    Get all vuart id member of class
    :param tmp_vuart: a dictionary to store member:value
    :param leaf_tag: key pattern of item tag
    :param leaf_text: key pattern of item tag's value
    :return: a dictionary to which stored member:value
    """
    if leaf_tag == "type":
        tmp_vuart['type'] = leaf_text
    if leaf_tag == "base":
        tmp_vuart['base'] = leaf_text
    if leaf_tag == "irq":
        tmp_vuart['irq'] = leaf_text

    if leaf_tag == "target_vm_id":
        tmp_vuart['target_vm_id'] = leaf_text
    if leaf_tag == "target_uart_id":
        tmp_vuart['target_uart_id'] = leaf_text

    return tmp_vuart


def get_vuart_info_id(config_file, idx):
    """
    Get vuart information by vuart id indexx
    :param config_file: it is a file what contains information for script to read from
    :param idx: vuart index in range: [0,1]
    :return: dictionary which stored the vuart-id
    """
    tmp_tag = {}
    vm_id = 0
    root = common.get_config_root(config_file)
    for item in root:
        for sub in item:
            tmp_vuart = {}
            for leaf in sub:
                if sub.tag == "vuart" and int(sub.attrib['id']) == idx:
                    tmp_vuart = get_vuart_id(tmp_vuart, leaf.tag, leaf.text)

            # append vuart for each vm
            if tmp_vuart and sub.tag == "vuart":
                tmp_tag[vm_id] = tmp_vuart

        if item.tag == "vm":
            vm_id += 1

    return tmp_tag


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


def get_ttys_info(board_info):
    """
    Get ttySn from board info
    :param board_info: it is a file what contains board information for script to read from
    :return: serial console list
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
