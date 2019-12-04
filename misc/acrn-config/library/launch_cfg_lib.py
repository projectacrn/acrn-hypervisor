# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import getopt
import common

SOURCE_ROOT_DIR = common.SOURCE_PATH
BOARD_INFO_FILE = "board_info.txt"
SCENARIO_INFO_FILE = ""
LAUNCH_INFO_FILE = ""

ERR_LIST = {}
BOOT_TYPE = ['no', 'vsbl', 'ovmf']
RTOS_TYPE = ['no', 'Soft RT', 'Hard RT']
REDIRECT_CONSOLE = ['com1(ttyS0)', 'virtio-console(hvc0)']
UOS_TYPES = ['CLEARLINUX', 'ANDROID', 'ALIOS', 'PREEMPT-RT LINUX', 'VXWORKS', 'WINDOWS', 'ZEPHYR', 'GENERIC LINUX']
GVT_ARGS = ['64 448 8']

RE_CONSOLE_MAP = {
        "com1(ttyS0)":"virtio-console,@pty:pty_port",
        "virtio-console(hvc0)":"virtio-console,@stdio:stdio_port"
}

PT_SUB_PCI = {}
PT_SUB_PCI['usb_xdci'] = ['USB controller']
PT_SUB_PCI['ipu'] = ['Multimedia controller']
PT_SUB_PCI['ipu_i2c'] = ['Signal processing controller']
PT_SUB_PCI['cse'] = ['Communication controller']
PT_SUB_PCI['audio'] = ['Audio device', 'Multimedia audio controller']
PT_SUB_PCI['audio_codec'] = ['Signal processing controller']
PT_SUB_PCI['sd_card'] = ['SD Host controller']
PT_SUB_PCI['wifi'] = ['Ethernet controller', 'Network controller', '802.1a controller',
                        '802.1b controller', 'Wireless controller']
PT_SUB_PCI['bluetooth'] = ['Signal processing controller']
PT_SUB_PCI['ethernet'] = ['Ethernet controller', 'Network controller']
PT_SUB_PCI['sata'] = ['SATA controller']
PT_SUB_PCI['nvme'] = ['Non-Volatile memory controller']

# passthrough devices for board
PASSTHRU_DEVS = ['usb_xdci', 'ipu', 'ipu_i2c', 'cse', 'audio', 'sata',
                    'nvme', 'audio_codec', 'sd_card', 'ethernet', 'wifi', 'bluetooth']

PT_SLOT = {
        "hostbridge":0,
        "lpc":1,
        "pci-gvt":2,
        "virtio-blk":3,
        "audio_codec":24
    }


POST_UUID_DIC = {}


def prepare(check_git):
    """ Check environment """
    return common.check_env(check_git)


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


def usage(file_name):
    """ This is usage for how to use this tool """
    print("usage= {} [h]".format(file_name), end="")
    print("--board <board_info_file> --scenario <scenario_info_file> --launch <launch_info_file> --uosid <uosid id> [--enable_commit]")
    print('board_info_file :  file name of the board info')
    print('scenario_info_file :  file name of the scenario info')
    print('launch_info_file :  file name of the launch info')
    print('uosid :  this is the relateive id for post launch vm in scenario info XML:[1..max post launch vm]')
    print('enable_commit:  enable the flag that git add/commit the generate files to the code base. without --enable_commit will not commit this source code')


def get_param(args):
    """
    Get the script parameters from command line
    :param args: this the command line of string for the script without script name
    """
    vm_th = '0'
    err_dic = {}
    board_info_file = False
    scenario_info_file = False
    launch_info_file = False
    enable_commit = False
    param_list = ['--board', '--scenario', '--launch', '--uosid', '--enable_commit']

    for arg_str in param_list:

        if arg_str == '--enable_commit':
            continue

        if arg_str not in args:
            usage(args[0])
            err_dic['common error: get wrong parameter'] = "wrong usage"
            return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    args_list = args[1:]
    (optlist, args_list) = getopt.getopt(args_list, '', ['board=', 'scenario=', 'launch=', 'uosid=', 'enable_commit'])
    for arg_k, arg_v in optlist:
        if arg_k == '--enable_commit':
            enable_commit = True
        if arg_k == '--board':
            board_info_file = arg_v
        if arg_k == '--scenario':
            scenario_info_file = arg_v
        if arg_k == '--launch':
            launch_info_file = arg_v
        if '--uosid' in args:
            if arg_k == '--uosid':
                vm_th = arg_v
                if not vm_th.isnumeric():
                    err_dic['common error: get wrong parameter'] = "--uosid should be a number"
                    return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    if not board_info_file or not scenario_info_file or not launch_info_file:
        usage(args[0])
        err_dic['common error: get wrong parameter'] = "wrong usage"
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    if not os.path.exists(board_info_file):
        err_dic['common error: get wrong parameter'] = "{} is not exist!".format(board_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    if not os.path.exists(scenario_info_file):
        err_dic['common error: get wrong parameter'] = "{} is not exist!".format(scenario_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    if not os.path.exists(launch_info_file):
        err_dic['common error: get wrong parameter'] = "{} is not exist!".format(launch_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)

    return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), enable_commit)


def get_post_num_list():
    """
    Get board name from launch.xml at fist line
    :param scenario_file: it is a file what contains scenario information for script to read from
    """
    post_num_list = common.get_post_num_list(LAUNCH_INFO_FILE)
    # {launch_id:scenario_id}
    return post_num_list


def get_post_vm_cnt():
    """
    Get board name from launch.xml at fist line
    :param scenario_file: it is a file what contains scenario information for script to read from
    """
    launch_vm_count = common.launch_vm_cnt(LAUNCH_INFO_FILE)
    post_vm_count = common.post_vm_cnt(SCENARIO_INFO_FILE)
    return (launch_vm_count, post_vm_count)


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


def get_avl_dev_info(bdf_desc_map, pci_sub_class):

    tmp_pci_desc = []
    for sub_class in pci_sub_class:
        for pci_desc_value in bdf_desc_map.values():
            pci_desc_sub_class = ' '.join(pci_desc_value.strip().split(':')[1].split()[1:])
            if sub_class == pci_desc_sub_class:
                tmp_pci_desc.append(pci_desc_value.strip())

    return tmp_pci_desc


def get_info(board_info, msg_s, msg_e):
    """
    Get information which specify by argument
    :param board_info: it is a file what contains board information for script to read from
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    info_lines = common.get_board_info(board_info, msg_s, msg_e)
    return info_lines


def get_rootdev_info(board_info):
    """
    Get root devices from board info
    :param board_info: it is a file what contains board information for script to read from
    :return: root devices list
    """
    rootdev_list = []
    rootdev_info = get_info(board_info, "<BLOCK_DEVICE_INFO>", "</BLOCK_DEVICE_INFO>")

    if rootdev_info == None:
        return rootdev_list

    for rootdev_line in rootdev_info:
        if not rootdev_line:
            break

        if not common.handle_root_dev(rootdev_line):
            continue

        root_dev = rootdev_line.strip().split(':')[0]
        rootdev_list.append(root_dev)

    return rootdev_list


def get_scenario_name():
    """
    Get board name from scenario.xml at fist line
    :param scenario_file: it is a file what contains scenario information for script to read from
    """
    (err_dic, scenario) = common.get_xml_attrib(SCENARIO_INFO_FILE, "scenario")

    return (err_dic, scenario)


def get_board_name():
    """
    Get board name from board.xml at fist line
    :param board_info: it is a file what contains board information for script to read from
    """
    (err_dic, board) = common.get_xml_attrib(BOARD_INFO_FILE, "board")
    return (err_dic, board)


def is_config_file_match():

    match = True
    # check if the board config match scenario config
    (err_dic, scenario_for_board) = common.get_xml_attrib(SCENARIO_INFO_FILE, "board")
    (err_dic, board_name) = common.get_xml_attrib(BOARD_INFO_FILE, "board")
    if scenario_for_board != board_name:
        err_dic['scenario config: Not match'] = "The board xml and scenario xml should be matched!"
        match = False

    # check if the board config match launch config
    (err_dic, launch_for_board) = common.get_xml_attrib(LAUNCH_INFO_FILE, "board")
    if launch_for_board != board_name:
        err_dic['launch config: Not match'] = "The board xml and launch xml should be matched!"
        match = False

    return (err_dic, match)


def get_leaf_tag_map(info_file, prime_item, item=''):
    """
    :param info_file: some configurations in the info file
    :param prime_item: the prime item in xml file
    :param item: the item in xml file
    :return: dictionary which item value could be indexed by vmid
    """
    vmid_item_dic = common.get_leaf_tag_map(info_file, prime_item, item)
    return vmid_item_dic


def get_scenario_uuid():
    # {id_num:uuid} (id_num:0~max)
    scenario_uuid_dic = {}
    scenario_uuid_dic = get_leaf_tag_map(SCENARIO_INFO_FILE, 'uuid')
    return scenario_uuid_dic


def get_sos_vmid():

    load_dic = get_leaf_tag_map(SCENARIO_INFO_FILE, "load_order")

    sos_id = ''
    for idx,load_order in load_dic.items():
        if load_order == "SOS_VM":
            sos_id = idx
            break

    return sos_id


def get_bdf_from_tag(config_file, branch_tag, tag_str):
    bdf_list = {}
    bdf_list = get_leaf_tag_map(config_file, branch_tag, tag_str)

    # split b:d:f from pci description
    for idx, bdf_v in bdf_list.items():
        if bdf_v:
            bdf_list[idx] = bdf_v.split()[0]

    return bdf_list


def get_vpid_from_bdf(bdf_vpid_map, bdf_list):
    vpid_list = {}
    post_vm_list = get_post_num_list()
    for p_id in post_vm_list:
        for bdf_k, vpid_v in bdf_vpid_map.items():
            if bdf_k == bdf_list[p_id]:
                #    print("k:{}, v{}".format(bdf_k, bdf_list[p_id]))
                # convert "808x:0xxx" to "808x 0xxx"
                tmp_vpid = " ".join(vpid_v.split(':'))
                vpid_list[p_id] = tmp_vpid
            elif not bdf_list[p_id]:
                vpid_list[p_id] = ''

    return vpid_list


def gen_patch(srcs_list, launch_name):
    """
    Generate patch and apply to local source code
    :param srcs_list: it is a list what contains source files
    :param scenario_name: scenario name
    """
    err_dic = common.add_to_patch(srcs_list, launch_name)
    return err_dic


def get_uos_type():
    """
    Get uos name from launch.xml at fist line
    """
    uos_types = get_leaf_tag_map(LAUNCH_INFO_FILE, "uos_type")

    return uos_types


def is_bdf_format(bdf_str):
    bdf_len = 7
    status = True
    if not bdf_str:
        return status

    bdf_str_len = len(bdf_str)
    if ':' in bdf_str and '.' in bdf_str and bdf_len == bdf_str_len:
        status = True
    else:
        status = False

    return status


def is_vpid_format(vpid_str):
    status = True
    if not vpid_str:
        return status

    vpid_len = 9
    vpid_str_len = len(vpid_str)

    if ' ' in vpid_str and vpid_len == vpid_str_len:
        status = True
    else:
        status = False

    return status


def pt_devs_check(bdf_list, vpid_list, item):
    i_cnt = 1

    # check bdf
    for bdf_str in bdf_list.values():
        if is_bdf_format(bdf_str):
            continue
        else:
            key = "uos:id={},passthrough_devices,{}".format(i_cnt, item)
            ERR_LIST[key] = "Unkonw the BDF format of {} device".format(item)
        i_cnt += 1

    # check vpid
    i_cnt = 1
    for vpid_str in vpid_list.values():
        if is_vpid_format(vpid_str):
            continue
        else:
            key = "uos:id={},passthrough_devices,{}".format(i_cnt, item)
            ERR_LIST[key] = "Unkonw the Vendor:Product ID format of {} device".format(item)

        i_cnt += 1


def empty_err(i_cnt, item):
    """
    add empty error message into ERR_LIST
    :param i_cnt: the launch vm index from config xml
    :param item: the item of tag from config xml
    :return: None
    """
    key = "uos:id={},{}".format(i_cnt, item)
    ERR_LIST[key] = "The parameter should not be empty"


def args_aval_check(arg_list, item, avl_list):
    """
    check arguments from config xml are available and validate
    :param arg_list: the list of arguments from config xml
    :param item: the item of tag from config xml
    :param avl_list: available argument which are allowed to chose
    :return: None
    """
    # args should be set into launch xml from webUI
    i_cnt = 1
    skip_check_list = ['gvt_args']
    if item in skip_check_list:
        return

    for arg_str in arg_list.values():
        if arg_str == None or not arg_str.strip():
            empty_err(i_cnt, item)
            i_cnt += 1
            continue

        if arg_str not in avl_list:
            key = "uos:id={},{}".format(i_cnt, item)
            ERR_LIST[key] = "The {} is invalidate".format(item)
        i_cnt += 1


def get_cpu_processor_num():
    """
    get cpu processor number from config file which is dumped from native board
    :return: integer number of cpu processor
    """
    cpu_lines = get_info(BOARD_INFO_FILE, "<CPU_PROCESSOR_INFO>", "</CPU_PROCESSOR_INFO>")

    for cpu_line in cpu_lines:
        cpu_processor_num = len(cpu_line.split(','))

    return cpu_processor_num


def get_total_mem():
    """
    get total memory size from config file which is dumped from native board
    :return: integer number of total memory size, Unit: MByte
    """
    scale_to_mb = 1
    total_mem_mb = scale_to_mb
    mem_lines = get_info(BOARD_INFO_FILE, "<TOTAL_MEM_INFO>", "</TOTAL_MEM_INFO>")
    for mem_line in mem_lines:
        mem_info_list = mem_line.split()

    if len(mem_info_list) <= 1:
        return total_mem_mb

    if mem_info_list[1] == "kB":
        scale_to_mb = 1024

    total_mem_mb = int(mem_info_list[0]) / scale_to_mb
    return total_mem_mb


def mem_size_check(arg_list, item):
    """
     check memory size list which are set from webUI
     :param arg_list: the list of arguments from config xml
     :param item: the item of tag from config xml
     :return: None
     """
    # get total memory information
    total_mem_mb = get_total_mem()

    # available check
    i_cnt = 1
    for arg_str in arg_list.values():
        if arg_str == None or not arg_str.strip():
            empty_err(i_cnt, item)
            i_cnt += 1
            continue

        mem_size_set = int(arg_str.strip())
        if mem_size_set > total_mem_mb:
            key = "uos:id={},{}".format(i_cnt, item)
            ERR_LIST[key] = "{}MB should be less than total memory {}MB".format(item)
        i_cnt += 1


def virtual_dev_slot(dev):
    max_slot = 31
    base_slot = 3

    #slot_used_len = len(list(PT_SLOT.values()))

    # get devices slot which already stored
    if dev in list(PT_SLOT.keys()):
        return PT_SLOT[dev]

    # alloc a new slot for device
    for slot_num in range(base_slot, max_slot):
        if slot_num not in list(PT_SLOT.values()):

            if (slot_num == 6 and 14 in list(PT_SLOT.values())) or (slot_num == 14 and 6 in list(PT_SLOT.values())):
                continue
            if (slot_num == 7 and 15 in list(PT_SLOT.values())) or (slot_num == 15 and 7 in list(PT_SLOT.values())):
                continue

            PT_SLOT[dev] = slot_num
            break

    return slot_num


def get_slot(bdf_list, dev):

    slot_list = {}
    post_vm_list = get_post_num_list()
    for p_id in post_vm_list:
        if not bdf_list[p_id]:
            slot_list[p_id] = ''
        else:
            slot = int(bdf_list[p_id][3:5], 16)
            # re-allocate virtual slot while slot is 0
            if slot == 0:
                slot = virtual_dev_slot(dev)
            slot_list[p_id] = slot
            PT_SLOT[dev] = slot

    return slot_list


def get_pt_dev():
    """ Get passthrough device list """
    cap_pt = PASSTHRU_DEVS

    return cap_pt


def undline_name(name):
    """
    This convert name which has contain '-' to '_'
    :param name: name which contain '-' and ' '
    :return: name_str which contain'_'
    """
    return common.undline_name(name)
