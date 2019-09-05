# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import common

SOURCE_ROOT_DIR = common.SOURCE_PATH
BOARD_NAME = ''
BOARD_INFO_FILE = "board_info.txt"
SCENARIO_INFO_FILE = ""

BIOS_INFO = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']

BASE_BOARD = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']

TTY_CONSOLE = {
    'ttyS0':'0x3F8',
    'ttyS1':'0x2F8',
    'ttyS2':'0x3E8',
    'ttyS3':'0x2E8',
}

NATIVE_CONSOLE_DIC = {}
VALID_LEGACY_IRQ = []
VM_COUNT = 0

ERR_LIST = {}

HEADER_LICENSE = common.open_license() + "\n"


def prepare():
    """ check environment """
    return common.check_env()


def print_yel(msg, warn=False):
    """
    Print the message with color of yellow
    :param msg: the stings which will be output to STDOUT
    :param warn: the condition if needs to be output the color of yellow with 'Warning'
    """
    common.print_if_yel(msg, warn)


def print_red(msg, err=False):
    """
    Print the message with color of red
    :param msg: the stings which will be output to STDOUT
    :param err: the condition if needs to be output the color of red with 'Error'
    """
    common.print_if_red(msg, err)


def get_board_name():
    """
    Get board name from board.xml at fist line
    :param board_info: it is a file what contains board information for script to read from
    """
    (err_dic, board) = common.get_xml_attrib(BOARD_INFO_FILE, "board")
    return (err_dic, board)


def get_scenario_name():
    """
    Get scenario name from scenario.xml at fist line
    :param scenario_info: it is a file what contains board information for script to read from
    """
    (err_dic, board) = common.get_xml_attrib(SCENARIO_INFO_FILE, "scenario")
    return (err_dic, board)


def is_config_file_match():

    (err_dic, scenario_for_board) = common.get_xml_attrib(SCENARIO_INFO_FILE, "board")
    (err_dic, board_name) = common.get_xml_attrib(BOARD_INFO_FILE, "board")

    if scenario_for_board == board_name:
        return (err_dic, True)
    else:
        return (err_dic, False)


def usage(file_name):
    """ This is usage for how to use this tool """
    common.usage(file_name)


def get_param(args):
    """
    Get the script parameters from command line
    :param args: this the command line of string for the script without script name
    """
    return common.get_param(args)


def get_info(board_info, msg_s, msg_e):
    """
    Get information which specify by argument
    :param board_info: it is a file what contains information for script to read from
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    info_lines = common.get_board_info(board_info, msg_s, msg_e)
    return info_lines


def handle_bios_info(config):
    """
    Handle bios information
    :param config: it is a file pointer of bios information for writing to
    """
    bios_lines = get_info(BOARD_INFO_FILE, "<BIOS_INFO>", "</BIOS_INFO>")
    board_lines = get_info(BOARD_INFO_FILE, "<BASE_BOARD_INFO>", "</BASE_BOARD_INFO>")
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


def get_tree_tag(config_file, tag_str):
    """
    This is get tag value by tag_str from config file
    :param config_file: it is a file what contains information for script to read from
    :param tag_str: it is key of pattern to config file item
    :return: value of tag_str item
    """
    return common.get_tree_tag_val(config_file, tag_str)


def get_sub_tree_tag(config_file, tag_str):
    """
     This is get tag value by tag_str from config file
     :param config_file: it is a file what contains information for script to read from
     :param tag_str: it is key of pattern to config file item
     :return: value of tag_str item
     """
    return common.get_branch_tag_val(config_file, tag_str)


def get_sub_leaf_tag(config_file, branch_tag, tag_str):
    """
     This is get tag value by tag_str from config file
     :param config_file: it is a file what contains information for script to read from
     :param branch_tag: it is key of patter to config file branch tag item
     :param tag_str: it is key of pattern to config file leaf tag item
     :return: value of tag_str item
     """
    return common.get_leaf_tag_val(config_file, branch_tag, tag_str)


def gen_patch(srcs_list, board_name):
    """
    Generate patch and apply to local source code
    :param srcs_list: it is a list what contains source files
    :param board_name: board name
    """
    err_dic = common.add_to_patch(srcs_list, board_name)
    return err_dic


def is_hpa_size(hpa_size_list):
    """
    This is identify if the host physical size list is correct format
    :param hpa_size_list: host physical size list
    :return: True if good format
    """
    ret = common.check_hpa_size(hpa_size_list)
    return ret


def get_rootfs(config_file):
    """
    This will get rootfs partition from board information
    :param config_file: it is a file which contain board information
    :return: rootfs partition list
    """
    rootfs_lines = get_info(config_file, "<ROOT_DEVICE_INFO>", "</ROOT_DEVICE_INFO>")
    root_dev_list = []

    for rootfs_line in rootfs_lines:
        root_dev = rootfs_line.strip().split(':')[0]
        root_dev_list.append(root_dev)

    return (root_dev_list, len(root_dev_list))


def clos_info_parser(board_info):
    """ Parse CLOS information """
    return common.get_max_clos(board_info)


def get_pre_launch_cnt(config_file):
    """
    This is get pre launched vm count
    :param config_file:  it is a file what contains vm information for script to read from
    :return: number of per launched vm
    """
    pre_launch_cnt = common.vm_pre_launch_cnt(config_file)
    return pre_launch_cnt


def get_vm_count(config_file):
    """
    This is get vm count
    :param config_file:  it is a file what contains vm information for script to read from
    :return: number of vm
    """
    global VM_COUNT
    VM_COUNT = common.get_vm_count(config_file)


def get_order_type_by_vmid(idx):
    """
    This is get pre launched vm count
    :param idx: index of vm id
    :return: idx and vm type mapping
    """
    (err_dic, order_id_dic) = common.get_load_order_by_vmid(SCENARIO_INFO_FILE, VM_COUNT, idx)
    if err_dic:
        ERR_LIST.update(err_dic)

    return order_id_dic


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
    used_console_lines = get_info(BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")

    vuart0_valid_console = []
    vuart1_valid_console = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3', 'ttyS4', 'ttyS5', 'ttyS6', 'ttyS7']
    if used_console_lines:
        vuart0_valid_console.clear()
        for console in used_console_lines:
            tty = console.split('/')[2].split()[0]
            ttys_irq = console.split(':')[-1].strip()
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
    (err_dic, scenario_name) = get_scenario_name()

    if scenario_name != "logical_partition":
        ttys = get_sub_leaf_tag(SCENARIO_INFO_FILE, "board_private", "console")
    else:
        ttys = get_sub_leaf_tag(SCENARIO_INFO_FILE, "os_config", "console")

    if ttys and 'BDF' in ttys[0] or '/dev' in ttys[0]:
        ttys_n = ttys[0].split('/')[2]
    else:
        # sdc/sdc2 is different from logical_partition
        ttys_n = ttys[0]

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

    if not ttys_n or ttys_n not in list(TTY_CONSOLE.keys()):
        err_dic["board config: ttyS not available"] = "console should be set in scenario.xml of board_private section"
        return (err_dic, vuart0_console_dic, vuart1_console_dic)

    (vuart0_valid_console, vuart1_valid_console, show_vuart1) = console_to_show(BOARD_INFO_FILE)

    # VUART0
    if ttys_n not in list(NATIVE_CONSOLE_DIC.keys()):
        vuart0_console_dic[ttys_n] = alloc_irq()
    else:
        if int(NATIVE_CONSOLE_DIC[ttys_n]) >= 16:
            vuart0_console_dic[ttys_n] = alloc_irq()
        else:
            vuart0_console_dic[ttys_n] = NATIVE_CONSOLE_DIC[ttys_n]

    # VUART1
    if len(vuart1_valid_console) == 4:
        ttys_n = get_sub_leaf_tag(SCENARIO_INFO_FILE, branch_tag, "vuart1_console")
        vuart1_console_dic[ttys_n] = alloc_irq()
    else:
        vuart1_console_dic[vuart1_valid_console[0]] = alloc_irq()

    return (err_dic, vuart0_console_dic, vuart1_console_dic)


def get_vuart_id(tmp_vuart, leaf_tag, leaf_text):
    """
    Get all vuart id member of class
    :param leaf_tag: key pattern of item tag
    :param tmp_vuart: a dictionary to store member:value
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
    tmp_tag = []
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
                #tmp_vuart[vm_id] = tmp_vuart
                tmp_tag.append(tmp_vuart)

        if item.tag == "vm":
            vm_id += 1

    return tmp_tag
