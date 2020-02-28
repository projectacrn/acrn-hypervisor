# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common

SOURCE_ROOT_DIR = common.SOURCE_PATH
HEADER_LICENSE = common.open_license()
BOARD_INFO_FILE = "board_info.txt"
SCENARIO_INFO_FILE = ""

LOAD_ORDER_TYPE = ['PRE_LAUNCHED_VM', 'SOS_VM', 'POST_LAUNCHED_VM']
START_HPA_LIST = ['0', '0x100000000', '0x120000000']

KERN_TYPE_LIST = ['KERNEL_BZIMAGE', 'KERNEL_ZEPHYR']
KERN_BOOT_ADDR_LIST = ['0x100000']

GUEST_FLAG = common.GUEST_FLAG
VM_SEVERITY = ['SEVERITY_SAFETY_VM', 'SEVERITY_RTVM', 'SEVERITY_SOS', 'SEVERITY_STANDARD_VM']
VUART_TYPE = ['VUART_LEGACY_PIO', 'VUART_PCI']
VUART_BASE = ['SOS_COM1_BASE', 'SOS_COM2_BASE', 'COM1_BASE',
              'COM2_BASE', 'COM3_BASE', 'COM4_BASE', 'INVALID_COM_BASE']

AVALIBLE_COM1_BASE = ['INVALID_COM_BASE', 'COM1_BASE']
AVALIBLE_COM2_BASE = ['INVALID_COM_BASE', 'COM2_BASE']

VUART_IRQ = ['SOS_COM1_IRQ', 'SOS_COM2_IRQ', 'COM1_IRQ', 'COM2_IRQ', 'COM3_IRQ',
             'COM4_IRQ', 'CONFIG_COM_IRQ', '3', '4', '6', '7']

PCI_DEV_NUM_LIST = ['SOS_EMULATED_PCI_DEV_NUM', 'VM0_CONFIG_PCI_DEV_NUM', 'VM1_CONFIG_PCI_DEV_NUM']
PCI_DEVS_LIST = ['sos_pci_devs', 'vm0_pci_devs', 'vm1_pci_devs']

COMMUNICATE_VM_ID = []

ERR_LIST = {}

VM_COUNT = 0
DEFAULT_VM_COUNT = {
    'sdc':2,
    'sdc2':4,
    'industry':3,
    'hybrid':3,
    'logical_partition':2,
}
KATA_VM_COUNT = 0

LEGACY_TTYS = {
    'ttyS0':'0x3F8',
    'ttyS1':'0x2F8',
    'ttyS2':'0x3E8',
    'ttyS3':'0x2E8',
}

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
    common.usage(file_name)


def get_param(args):
    """
    Get the script parameters from command line
    :param args: this the command line of string for the script without script name
    """
    return common.get_param(args)


def get_scenario_name():
    """
    Get board name from scenario.xml at fist line
    :param scenario_file: it is a file what contains scenario information for script to read from
    """
    (err_dic, scenario) = common.get_xml_attrib(SCENARIO_INFO_FILE, "scenario")

    return (err_dic, scenario)


def is_config_file_match():

    (err_dic, scenario_for_board) = common.get_xml_attrib(SCENARIO_INFO_FILE, "board")
    (err_dic, board_name) = common.get_xml_attrib(BOARD_INFO_FILE, "board")

    if scenario_for_board == board_name:
        return (err_dic, True)
    else:
        return (err_dic, False)


def get_info(board_info, msg_s, msg_e):
    """
    Get information which specify by argument
    :param board_info: it is a file what contains board information for script to read from
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    info_lines = common.get_board_info(board_info, msg_s, msg_e)
    return info_lines


def get_processor_info(board_info):
    """
    Get cpu core list
    :param board_info: it is a file what contains board information for script to read from
    :return: cpu processor which one cpu has
    """
    processor_list = []
    tmp_list = []
    processor_info = get_info(board_info, "<CPU_PROCESSOR_INFO>", "</CPU_PROCESSOR_INFO>")

    if not processor_info:
        key = "vm:id=0,vcpu_affinity"
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


def get_rootdev_info(board_info):
    """
    Get root devices from board info
    :param board_info: it is a file what contains board information for script to read from
    :return: root devices list
    """
    rootdev_list = []
    rootdev_info = get_info(board_info, "<BLOCK_DEVICE_INFO>", "</BLOCK_DEVICE_INFO>")

    # none 'BLOCK_DEVICE_INFO' tag
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


def get_board_private_info(config_file):

    (err_dic, scenario_name) = get_scenario_name()

    if scenario_name == "logical_partition":
        branch_tag = "os_config"
    else:
        branch_tag = "board_private"

    private_info = {}
    dev_private_tags = ['rootfs', 'console']
    for tag_str in dev_private_tags:
        dev_setting = get_sub_leaf_tag(config_file, branch_tag, tag_str)
        if not dev_setting and tag_str == "console":
            continue

        private_info[tag_str] = dev_setting

    return (err_dic, private_info)


def check_board_private_info():

    (err_dic, private_info) = get_board_private_info(SCENARIO_INFO_FILE)

    if not private_info['rootfs'] and err_dic:
        ERR_LIST['vm:id=0,boot_private,rootfs'] = "The board have to chose one rootfs partition"
        ERR_LIST.update(err_dic)


def get_vm_num(config_file):
    """
    This is get vm count
    :param config_file:  it is a file what contains vm information for script to read from
    :return: number of vm
    """
    return common.get_vm_count(config_file)


def get_sub_leaf_tag(config_file, branch_tag, tag_str=''):
    """
      This is get tag value by tag_str from config file
      :param config_file: it is a file what contains information for script to read from
      :param branch_tag: it is key of patter to config file branch tag item
      :param tag_str: it is key of pattern to config file leaf tag item
      :return: value of tag_str item
      """
    return common.get_leaf_tag_val(config_file, branch_tag, tag_str)


def get_order_type_by_vmid(idx):
    """
     Get load order by vm id

     :param idx: index of vm id
     :return: load order type of index to vmid
     """
    (err_dic, order_type) = common.get_load_order_by_vmid(SCENARIO_INFO_FILE, VM_COUNT, idx)
    if err_dic:
        ERR_LIST.update(err_dic)

    return order_type


def vm_name_check(vm_names, item):
    """
    Check vm name
    :param vm_names: dictionary of vm name
    :param item: vm name item in xml
    :return: None
    """
    for name_i, name_str in vm_names.items():
        name_len = len(name_str)
        if name_len > 32 or name_len == 0:
            key = "vm:id={},{}".format(name_i, item)
            ERR_LIST[key] = "VM name length should be in range [1,32] bytes"


def load_order_check(load_orders, item):
    """
    Check load order type
    :param load_orders: dictionary of vm load_order
    :param item: vm name item in xml
    :return: None
    """
    sos_vm_ids = []
    pre_vm_ids = []
    post_vm_ids = []
    for order_i, load_str in load_orders.items():
        if load_str == "SOS_VM":
            sos_vm_ids.append(order_i)

        if load_str == "PRE_LAUNCHED_VM":
            pre_vm_ids.append(order_i)

        if load_str == "POST_LAUNCHED_VM":
            post_vm_ids.append(order_i)

        if not load_str:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load should not empty"
            return

        if load_str not in LOAD_ORDER_TYPE:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load order unknown"

    if len(sos_vm_ids) >= 2:
        key = "vm:id={},{}".format(sos_vm_ids[0], item)
        ERR_LIST[key] = "SOS_VM number should not be greater than 1"

    if post_vm_ids and sos_vm_ids:
        if post_vm_ids[0] < sos_vm_ids[-1]:
            key = "vm:id={},{}".format(post_vm_ids[0], item)
            ERR_LIST[key] = "POST_LAUNCHED_VM should be configured after SOS_VM"

    if pre_vm_ids and sos_vm_ids:
        if sos_vm_ids[-1] < pre_vm_ids[-1]:
            key = "vm:id={},{}".format(sos_vm_ids[0], item)
            ERR_LIST[key] = "PRE_LAUNCHED_VM should be configured before SOS_VM"


def get_load_order_cnt(load_orders, type_name):
    """
    Get load order type count
    :param load_orders: dictionary of vm load_order
    :param type_name: load order type for vm
    :return: number for this load order type name
    """
    type_cnt = 0
    for load_str in load_orders.values():
        if type_name == load_str:
            type_cnt += 1

    return type_cnt


def guest_flag_check(guest_flag_idx, branch_tag, leaf_tag):

    guest_flag_len = len(common.GUEST_FLAG)
    guest_num = len(guest_flag_idx)

    for vm_i in range(guest_num):
        flag_len = len(guest_flag_idx[vm_i])
        if flag_len <= guest_flag_len:
            continue
        else:
            key = "vm:id={},{},{}".format(vm_i, branch_tag, leaf_tag)
            ERR_LIST[key] = "Unknow guest flag"
    #    for flag_i in range(flag_len):
    #        if guest_flag_idx[vm_i][flag_i] in common.GUEST_FLAG:
    #            continue
    #        else:
    #            key = "vm:id={},{},{}".format(vm_i, branch_tag, leaf_tag)
    #            ERR_LIST[key] = "Invalid guest flag"


def uuid_format_check(uuid_dic, item):
    """
    Check uuid
    :param uuid_dic: dictionary of vm uuid
    :param item: vm uuid item in xml
    :return: None
    """
    uuid_len = 36

    for uuid_i, uuid_str in uuid_dic.items():

        if not uuid_str:
            key = "vm:id={},{}".format(uuid_i, item)
            ERR_LIST[key] = "VM uuid should not empty"
            return

        uuid_str_list = list(uuid_str)
        key = "vm:id={},{}".format(uuid_i, item)

        if len(uuid_str) != uuid_len:
            ERR_LIST[key] = "VM uuid length should be 36 bytes"

        if uuid_str_list[8] != '-':
            ERR_LIST[key] = "VM uuid format unknown"


def get_leaf_tag_map(info_file, prime_item, item=''):
    """
    :param info_file: some configurations in the info file
    :param prime_item: the prime item in xml file
    :param item: the item in xml file
    :return: dictionary which item value could be indexed by vmid
    """
    vmid_item_dic = common.get_leaf_tag_map(info_file, prime_item, item)
    return vmid_item_dic


def cpus_per_vm_check(id_cpus_per_vm_dic, item):
    """
    Check cpu number of per vm
    :param item: vm pcpu_id item in xml
    :return: None
    """

    for id_key in id_cpus_per_vm_dic.keys():
        vm_type = get_order_type_by_vmid(id_key)
        cpus_vm_i = id_cpus_per_vm_dic[id_key]
        if not cpus_vm_i and vm_type == "PRE_LAUNCHED_VM":
            key = "vm:id={},{}".format(id_key, item)
            ERR_LIST[key] = "VM have no assignment cpus"


def mem_start_hpa_check(id_start_hpa_dic, prime_item, item):
    """
    Check host physical address
    :param prime_item: the prime item in xml file
    :param item: vm start_hpa item in xml
    :return: None
    """

    for id_key, hpa_str in id_start_hpa_dic.items():
        hpa_strip_ul = hpa_str.strip('UL')
        hpa_strip_u = hpa_str.strip('U')

        if not hpa_str:
            key = "vm:id={},{}".format(id_key, item)
            ERR_LIST[key] = "VM start host physical memory address should not empty"
            return

        if hpa_strip_ul not in START_HPA_LIST and hpa_strip_u not in START_HPA_LIST:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            if '0x' not in hpa_str and '0X' not in hpa_str:
                ERR_LIST[key] = "Address should be Hex format"


def mem_size_check(id_hpa_size_dic, prime_item, item):
    """
    Check host physical size
    :param prime_item: the prime item in xml file
    :param item: vm size item in xml
    :return: None
    """

    for id_key, hpa_size in id_hpa_size_dic.items():
        hpa_sz_strip_ul = hpa_size.strip('UL')
        hpa_sz_strip_u = hpa_size.strip('U')

        if not hpa_size:
            key = "vm:id={},{}".format(id_key, item)
            ERR_LIST[key] = "VM start host physical memory size should not empty"
            return

        if hpa_sz_strip_ul not in common.START_HPA_SIZE_LIST and hpa_sz_strip_u not in \
                common.START_HPA_SIZE_LIST:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            mem_max = 2 * 1024 * 1024 * 1024
            if '0x' not in hpa_size and '0X' not in hpa_size:
                ERR_LIST[key] = "Mem size should be Hex format"
            elif int(hpa_sz_strip_ul, 16) > mem_max  or int(hpa_sz_strip_u, 16) > mem_max:
                ERR_LIST[key] = "Mem size should less than 2GB"


def os_kern_name_check(id_kern_name_dic, prime_item, item):
    """
    Check os kernel name
    :param prime_item: the prime item in xml file
    :param item: vm os config name item in xml
    :return: None
    """

    for id_key, kern_name in id_kern_name_dic.items():
        if len(kern_name) > 32 or len(kern_name) == 0:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel name length should be in range [1,32] bytes"


def os_kern_type_check(id_kern_type_dic, prime_item, item):
    """
    Check os kernel type
    :param prime_item: the prime item in xml file
    :param item: vm os config type item in xml
    :return: None
    """

    for id_key, kern_type in id_kern_type_dic.items():

        if not kern_type:
            key = "vm:id={},{}".format(id_key, item)
            ERR_LIST[key] = "VM os config kernel type should not empty"
            return

        if kern_type not in KERN_TYPE_LIST:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel type unknown"


def os_kern_mod_check(id_kern_mod_dic, prime_item, item):
    """
    Check os kernel mod
    :param prime_item: the prime item in xml file
    :param item: vm os config mod item in xml
    :return: None
    """

    for id_key, kern_mod in id_kern_mod_dic.items():
        if len(kern_mod) > 32 or len(kern_mod) == 0:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel mod tag should be in range [1,32] bytes"


def os_kern_args_check(id_kern_args_dic, prime_item, item):
    """
    Check os kernel args
    :param prime_item: the prime item in xml file
    :param item: vm os config args item in xml
    :return: None
    """

    for id_key, kern_args in id_kern_args_dic.items():
        vm_type = get_order_type_by_vmid(id_key)

        if vm_type == "SOS_VM" and kern_args != "SOS_VM_BOOTARGS":
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel service os should be SOS_VM_BOOTARGS"


def os_kern_console_check(id_kern_console_dic, prime_item, item):
    """
    Check os kernel console
    :param prime_item: the prime item in xml file
    :param item: vm os config console item in xml
    :return: None
    """

    for id_key, kern_console in id_kern_console_dic.items():
        if kern_console and "ttyS" not in kern_console:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel console should be ttyS[0..3]"


def os_kern_load_addr_check(id_kern_load_addr_dic, prime_item, item):
    """
    Check os kernel load address
    :param prime_item: the prime item in xml file
    :param item: vm os config load address item in xml
    :return: None
    """

    for id_key, kern_load_addr in id_kern_load_addr_dic.items():

        if not kern_load_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel load address should not empty"
            return

        if '0x' not in kern_load_addr and '0X' not in kern_load_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel load address should Hex format"


def os_kern_entry_addr_check(id_kern_entry_addr_dic, prime_item, item):
    """
    Check os kernel entry address
    :param prime_item: the prime item in xml file
    :param item: vm os config entry address item in xml
    :return: None
    """

    for id_key, kern_entry_addr in id_kern_entry_addr_dic.items():

        if not kern_entry_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel entry address should not empty"
            return

        if '0x' not in kern_entry_addr and '0X' not in kern_entry_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel entry address should Hex format"


def os_kern_root_dev_check(id_kern_rootdev_dic, prime_item, item):
    """
    Check os kernel rootfs partitions
    :param prime_item: the prime item in xml file
    :param item: vm os config rootdev item in xml
    :return: None
    """

    for id_key, kern_rootdev in id_kern_rootdev_dic.items():
        if not kern_rootdev:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel root device should not empty"


def pci_dev_num_check(id_dev_num_dic, item):
    """
    Check vm pci device number
    :param item: vm pci_dev_num item in xml
    :return: None
    """

    for id_key, pci_dev_num in id_dev_num_dic.items():

        vm_type = get_order_type_by_vmid(id_key)
        if vm_type != "POST_LAUNCHED_VM" and pci_dev_num:
            if pci_dev_num not in PCI_DEV_NUM_LIST:
                key = "vm:id={},{}".format(id_key, item)
                ERR_LIST[key] = "VM pci device number  shoud be one of {}".format(PCI_DEV_NUM_LIST)

def pci_devs_check(id_devs_dic, item):
    """
    Check vm pci devices
    :param item: vm pci_devs item in xml
    :return: None
    """

    for id_key, pci_dev in id_devs_dic.items():

        vm_type = get_order_type_by_vmid(id_key)
        if vm_type != "POST_LAUNCHED_VM" and pci_dev:
            if pci_dev not in PCI_DEVS_LIST:
                key = "vm:id={},{}".format(id_key, item)
                ERR_LIST[key] = "VM pci device shoud be one of {}".format(PCI_DEVS_LIST)


def get_vuart1_vmid(vm_vuart1):
    """
    Get vmid:vuart1 dictionary
    :param vm_vuart1: vm_vuart1 setting from xml
    :return: dictionary of vmid:vuart1
    """
    vm_id_dic = {}
    for i in range(VM_COUNT):
        for key in vm_vuart1[i].keys():
            if key == "target_vm_id":
                vm_id_dic[i] = vm_vuart1[i][key]

    # remove the unavailable vimid:target_vmid from dictonary
    vmid_list = list(vm_id_dic.keys())

    for vmid in vmid_list:
        new_vmid = vm_id_dic[vmid]
        if int(new_vmid) in vmid_list and vmid == int(vm_id_dic[int(new_vmid)]):
            continue
        else:
            vm_id_dic.pop(vmid)

    return vm_id_dic


def cpus_assignment(cpus_per_vm, index):
    """
    Get cpu id assignment for vm by vm index
    :param cpus_per_vm: a dictionary by vmid:cpus
    :param index: vm index
    :return: cpu assignment string
    """
    vm_cpu_bmp = {}

    for i in range(len(cpus_per_vm[index])):
        if i == 0:
            if len(cpus_per_vm[index]) == 1:
                cpu_str = "{{AFFINITY_CPU({0}U)}}".format(cpus_per_vm[index][0])
            else:
                cpu_str = "{{AFFINITY_CPU({0}U)".format(cpus_per_vm[index][0])
        else:
            if i == len(cpus_per_vm[index]) - 1:
                cpu_str = cpu_str + ", AFFINITY_CPU({0}U)}}".format(cpus_per_vm[index][i])
            else:
                cpu_str = cpu_str + ", AFFINITY_CPU({0}U)".format(cpus_per_vm[index][i])

    vm_cpu_bmp['cpu_map'] = cpu_str
    vm_cpu_bmp['cpu_num'] = len(cpus_per_vm[index])
    return vm_cpu_bmp


def gen_patch(srcs_list, scenario_name):
    """
    Generate patch and apply to local source code
    :param srcs_list: it is a list what contains source files
    :param scenario_name: scenario name
    """
    err_dic = common.add_to_patch(srcs_list, scenario_name)
    return err_dic


def get_vuart_info_id(config_file, idx):
    """
    Get vuart information by vuart id indexx
    :param config_file: it is a file what contains information for script to read from
    :param idx: vuart index in range: [0,1]
    :return: dictionary which stored the vuart-id
    """
    tmp_tag = common.get_vuart_info_id(config_file, idx)
    return tmp_tag


def avl_vuart_ui_select(scenario_info):
    vm_num = get_vm_num(scenario_info)
    tmp_vuart = {}
    for vm_i in range(vm_num):
        vm_type = get_order_type_by_vmid(vm_i)

        if vm_type == "SOS_VM":
            key = "vm={},vuart=0,base".format(vm_i)
            tmp_vuart[key] = ['SOS_COM1_BASE', 'INVALID_COM_BASE']
            key = "vm={},vuart=1,base".format(vm_i)
            tmp_vuart[key] = ['SOS_COM2_BASE', 'INVALID_COM_BASE']
        else:
            key = "vm={},vuart=0,base".format(vm_i)
            tmp_vuart[key] = ['INVALID_COM_BASE', 'COM1_BASE']
            key = "vm={},vuart=1,base".format(vm_i)
            tmp_vuart[key] = ['INVALID_COM_BASE', 'COM2_BASE']

    return tmp_vuart


def get_first_post_vm():

    for i in range(VM_COUNT):
        (err_dic, vm_type) = common.get_load_order_by_vmid(SCENARIO_INFO_FILE, VM_COUNT, i)
        if vm_type == "POST_LAUNCHED_VM":
            break

    return (err_dic, i)
