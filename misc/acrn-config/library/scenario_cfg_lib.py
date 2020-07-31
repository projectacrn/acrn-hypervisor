# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import board_cfg_lib

HEADER_LICENSE = common.open_license()

START_HPA_LIST = ['0', '0x100000000', '0x120000000']

KERN_TYPE_LIST = ['KERNEL_BZIMAGE', 'KERNEL_ZEPHYR']
KERN_BOOT_ADDR_LIST = ['0x100000']

VUART_TYPE = ['VUART_LEGACY_PIO', 'VUART_PCI']
VUART_BASE = ['SOS_COM1_BASE', 'SOS_COM2_BASE', 'COM1_BASE',
              'COM2_BASE', 'COM3_BASE', 'COM4_BASE', 'INVALID_COM_BASE']

AVALIBLE_COM1_BASE = ['INVALID_COM_BASE', 'COM1_BASE']
AVALIBLE_COM2_BASE = ['INVALID_COM_BASE', 'COM2_BASE']

VUART_IRQ = ['SOS_COM1_IRQ', 'SOS_COM2_IRQ', 'COM1_IRQ', 'COM2_IRQ', 'COM3_IRQ',
             'COM4_IRQ', 'CONFIG_COM_IRQ', '3', '4', '6', '7']

# Support 512M, 1G, 2G
# pre launch less then 2G, sos vm less than 24G
START_HPA_SIZE_LIST = ['0x20000000', '0x40000000', '0x80000000', 'CONFIG_SOS_RAM_SIZE']

COMMUNICATE_VM_ID = []

ERR_LIST = {}

KATA_VM_COUNT = 0
PT_SUB_PCI = {}
PT_SUB_PCI['ethernet'] = ['Ethernet controller', 'Network controller', '802.1a controller',
                        '802.1b controller', 'Wireless controller']
PT_SUB_PCI['sata'] = ['SATA controller']
PT_SUB_PCI['nvme'] = ['Non-Volatile memory controller']
PT_SUB_PCI['usb'] = ['USB controller']
UUID_DB = {
    'SOS_VM':['dbbbd434-7a57-4216-a12c-2201f1ab0240'],
    'SAFETY_VM':['fc836901-8685-4bc0-8b71-6e31dc36fa47'],
    'PRE_STD_VM':['26c5e0d8-8f8a-47d8-8109-f201ebd61a5e', 'dd87ce08-66f9-473d-bc58-7605837f935e'],
    'POST_STD_VM':['d2795438-25d6-11e8-864e-cb7a18b34643', '615db82a-e189-4b4f-8dbb-d321343e4ab3',
        '38158821-5208-4005-b72a-8a609e4190d0', 'a6750180-f87a-48d2-91d9-4e7f62b6519e', 'd1816e4a-a9bb-4cb4-a066-3f1a8a5ce73f'],
    'POST_RT_VM':['495ae2e5-2603-4d64-af76-d4bc5a8ec0e5'],
    'KATA_VM':['a7ada506-1ab0-4b6b-a0da-e513ca9b8c2f'],
    'PRE_RT_VM':['b2a92bec-ca6b-11ea-b106-3716a8ba0bb9'],
}

VM_DB = {
    'SOS_VM':{'load_type':'SOS_VM', 'severity':'SEVERITY_SOS', 'uuid':UUID_DB['SOS_VM']},
    'SAFETY_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_SAFETY_VM', 'uuid':UUID_DB['SAFETY_VM']},
    'PRE_STD_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_STANDARD_VM', 'uuid':UUID_DB['PRE_STD_VM']},
    'POST_STD_VM':{'load_type':'POST_LAUNCHED_VM', 'severity':'SEVERITY_STANDARD_VM', 'uuid':UUID_DB['POST_STD_VM']},
    'POST_RT_VM':{'load_type':'POST_LAUNCHED_VM', 'severity':'SEVERITY_RTVM', 'uuid':UUID_DB['POST_RT_VM']},
    'KATA_VM':{'load_type':'POST_LAUNCHED_VM', 'severity':'SEVERITY_STANDARD_VM', 'uuid':UUID_DB['KATA_VM']},
    'PRE_RT_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_RTVM', 'uuid':UUID_DB['PRE_RT_VM']},
}
LOAD_VM_TYPE = list(VM_DB.keys())

def get_pci_devs(pci_items):

    pci_devs = {}
    for vm_i,pci_descs in pci_items.items():
        bdf_list = []
        for pci_des in pci_descs:
            if not pci_des:
                continue
            bdf = pci_des.split()[0]
            bdf_list.append(bdf)

        pci_devs[vm_i] = bdf_list

    return pci_devs


def get_pci_num(pci_devs):

    pci_devs_num = {}
    for vm_i,pci_devs_list in pci_devs.items():
        # vhostbridge
        cnt_dev = 1
        for pci_dev in pci_devs_list:
            if not pci_dev:
                continue
            cnt_dev += 1

        pci_devs_num[vm_i] = cnt_dev

    return pci_devs_num


def check_board_private_info():

    if 'SOS_VM' not in common.VM_TYPES.values():
        return
    branch_tag = "board_private"
    private_info = {}
    dev_private_tags = ['rootfs']
    for tag_str in dev_private_tags:
        dev_setting = common.get_leaf_tag_map(common.SCENARIO_INFO_FILE, branch_tag, tag_str)
        private_info[tag_str] = dev_setting

    if not private_info['rootfs'] and err_dic:
        ERR_LIST['vm:id=0,boot_private,rootfs'] = "The board have to chose one rootfs partition"
        ERR_LIST.update(err_dic)


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


def load_vm_check(load_vms, item):
    """
    Check load order type
    :param load_vms: dictionary of vm vm mode
    :param item: vm name item in xml
    :return: None
    """
    global KATA_VM_COUNT
    sos_vm_ids = []
    pre_vm_ids = []
    post_vm_ids = []
    kata_vm_ids = []
    rt_vm_ids = []
    for order_i, load_str in load_vms.items():
        if not load_str:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load should not empty"
            return

        if load_str not in LOAD_VM_TYPE:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load order unknown"

        if "SOS_VM" == VM_DB[load_str]['load_type']:
            sos_vm_ids.append(order_i)

        if "PRE_LAUNCHED_VM" == VM_DB[load_str]['load_type']:
            pre_vm_ids.append(order_i)

        if "POST_STD_VM" == load_str:
            post_vm_ids.append(order_i)

        if "KATA_VM" == load_str:
            kata_vm_ids.append(order_i)

        if "POST_RT_VM" == load_str:
            rt_vm_ids.append(order_i)

    KATA_VM_COUNT = len(kata_vm_ids)
    if len(kata_vm_ids) > len(UUID_DB["KATA_VM"]):
        key = "vm:id={},{}".format(kata_vm_ids[0], item)
        ERR_LIST[key] = "KATA VM number should not be greater than {}".format(len(UUID_DB["KATA_VM"]))
        return

    if len(rt_vm_ids) > len(UUID_DB["POST_RT_VM"]):
        key = "vm:id={},{}".format(rt_vm_ids[0], item)
        ERR_LIST[key] = "POST RT VM number should not be greater than {}".format(len(UUID_DB["POST_RT_VM"]))
        return

    if len(sos_vm_ids) > 1:
        key = "vm:id={},{}".format(sos_vm_ids[0], item)
        ERR_LIST[key] = "SOS VM number should not be greater than 1"
        return

    if len(post_vm_ids) > len(UUID_DB["POST_STD_VM"]):
        key = "vm:id={},{}".format(post_vm_ids[0], item)
        ERR_LIST[key] = "POST Standard vm number should not be greater than {}".format(len(UUID_DB["POST_STD_VM"]))
        return

    max_pre_launch_vms = len(UUID_DB["PRE_STD_VM"]) + len(UUID_DB["SAFETY_VM"]) + len(UUID_DB["PRE_RT_VM"])
    if len(pre_vm_ids) > max_pre_launch_vms:
        key = "vm:id={},{}".format(pre_vm_ids[0], item)
        ERR_LIST[key] = "PRE Launched VM number should not be greater than {}".format(max_pre_launch_vms)
        return

    if post_vm_ids and sos_vm_ids:
        if post_vm_ids[0] < sos_vm_ids[-1]:
            key = "vm:id={},{}".format(post_vm_ids[0], item)
            ERR_LIST[key] = "Post vm should be configured after SOS_VM"

    if pre_vm_ids and sos_vm_ids:
        if sos_vm_ids[-1] < pre_vm_ids[-1]:
            key = "vm:id={},{}".format(sos_vm_ids[0], item)
            ERR_LIST[key] = "Pre vm should be configured before SOS_VM"


def get_load_vm_cnt(load_vms, type_name):
    type_cnt = 0
    for load_str in load_vms.values():
        if type_name == VM_DB[load_str]['load_type']:
            type_cnt += 1

    return type_cnt


def guest_flag_check(guest_flags, branch_tag, leaf_tag):

    for vm_i, flags in guest_flags.items():
        for guest_flag in flags:
            if guest_flag and guest_flag not in common.GUEST_FLAG:
                key = "vm:id={},{},{}".format(vm_i, branch_tag, leaf_tag)
                ERR_LIST[key] = "Unknow guest flag"


def vm_cpu_affinity_check(config_file, id_cpus_per_vm_dic, item):
    """
    Check cpu number of per vm
    :param item: vm pcpu_id item in xml
    :return: error informations
    """
    err_dic = {}
    use_cpus = []
    cpu_sharing_enabled = True

    cpu_sharing = common.get_hv_item_tag(common.SCENARIO_INFO_FILE, "FEATURES", "SCHEDULER")
    if cpu_sharing == "SCHED_NOOP":
        cpu_sharing_enabled = False

    cpu_affinity = common.get_leaf_tag_map(config_file, "cpu_affinity", "pcpu_id")
    for vm_i in id_cpus_per_vm_dic.keys():
        for cpu in id_cpus_per_vm_dic[vm_i]:
            if cpu in use_cpus and not cpu_sharing_enabled:
                key = "vm:id={},{}".format(vm_i, item)
                err_dic[key] = "The same pcpu was configurated in <pcpu_id>/<cpu_affinity>, but CPU sharing is disabled by 'SCHED_NOOP'. Please re-configurate them!"
                return err_dic
            else:
                use_cpus.append(cpu)

    sos_vm_cpus = []
    pre_launch_cpus = []
    post_launch_cpus = []
    for vm_i, vm_type in common.VM_TYPES.items():
        if vm_i not in id_cpus_per_vm_dic.keys():
            continue
        elif VM_DB[vm_type]['load_type'] == "PRE_LAUNCHED_VM":
            cpus = [x for x in id_cpus_per_vm_dic[vm_i] if not None]
            pre_launch_cpus.extend(cpus)
        elif VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
            cpus = [x for x in id_cpus_per_vm_dic[vm_i] if not None]
            post_launch_cpus.extend(cpus)
        elif VM_DB[vm_type]['load_type'] == "SOS_VM":
            cpus = [x for x in id_cpus_per_vm_dic[vm_i] if not None]
            sos_vm_cpus.extend(cpus)

        # duplicate cpus assign the same VM check
        cpus_vm_i = id_cpus_per_vm_dic[vm_i]
        for cpu_id in cpus_vm_i:
            if cpus_vm_i.count(cpu_id) >= 2:
                key = "vm:id={},{}".format(vm_i, item)
                err_dic[key] = "VM should not use the same pcpu id:{}".format(cpu_id)
                return err_dic

    if pre_launch_cpus:
        if "SOS_VM" in common.VM_TYPES and not sos_vm_cpus:
            key = "SOS VM cpu_affinity"
            err_dic[key] = "Should assign CPU id for SOS VM"

        for pcpu in pre_launch_cpus:
            if pre_launch_cpus.count(pcpu) >= 2:
                key = "Pre launched VM cpu_affinity"
                err_dic[key] = "Pre_launched_vm vm should not have the same cpus assignment"
            if pcpu in post_launch_cpus:
                key = "Pre launched vm and Post launchded VM cpu_affinity"
                err_dic[key] = "Pre launched_vm and Post launched vm should not have the same cpus assignment"

    return err_dic


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

        if hpa_sz_strip_ul not in START_HPA_SIZE_LIST and hpa_sz_strip_u not in \
                START_HPA_SIZE_LIST:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            if '0x' not in hpa_size and '0X' not in hpa_size:
                ERR_LIST[key] = "Mem size should be Hex format"


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

    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_i not in id_kern_args_dic.keys():
            continue
        kern_args = id_kern_args_dic[vm_i]
        if "SOS_" in vm_type and kern_args != "SOS_VM_BOOTARGS":
            key = "vm:id={},{},{}".format(vm_i, prime_item, item)
            ERR_LIST[key] = "VM os config kernel service os should be SOS_VM_BOOTARGS"


def os_kern_load_addr_check(kern_type, id_kern_load_addr_dic, prime_item, item):
    """
    Check os kernel load address
    :param prime_item: the prime item in xml file
    :param item: vm os config load address item in xml
    :return: None
    """

    for id_key, kern_load_addr in id_kern_load_addr_dic.items():
        if kern_type[id_key] != 'KERNEL_ZEPHYR':
            continue

        if not kern_load_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel load address should not empty"
            return

        if '0x' not in kern_load_addr and '0X' not in kern_load_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel load address should Hex format"


def os_kern_entry_addr_check(kern_type, id_kern_entry_addr_dic, prime_item, item):
    """
    Check os kernel entry address
    :param prime_item: the prime item in xml file
    :param item: vm os config entry address item in xml
    :return: None
    """

    for id_key, kern_entry_addr in id_kern_entry_addr_dic.items():
        if kern_type[id_key] != 'KERNEL_ZEPHYR':
            continue

        if not kern_entry_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel entry address should not empty"
            return

        if '0x' not in kern_entry_addr and '0X' not in kern_entry_addr:
            key = "vm:id={},{},{}".format(id_key, prime_item, item)
            ERR_LIST[key] = "VM os config kernel entry address should Hex format"


def pci_devs_check(pci_bdf_devs, branch_tag, tag_str):
    """
    Check vm pci devices
    :param item: vm pci_devs item in xml
    :return: None
    """
    (bdf_desc_map, bdf_vpid_map) = board_cfg_lib.get_pci_info(common.BOARD_INFO_FILE)
    for id_key, pci_bdf_devs_list in pci_bdf_devs.items():
        for pci_bdf_dev in pci_bdf_devs_list:
            if pci_bdf_dev and pci_bdf_dev not in bdf_desc_map.keys():
                key = "vm:id={},{},{}".format(id_key, branch_tag, tag_str)
                ERR_LIST[key] = "The {} is unkonw device of PCI".format(pci_bdf_dev)


def get_vuart1_vmid(vm_vuart1):
    """
    Get vmid:vuart1 dictionary
    :param vm_vuart1: vm_vuart1 setting from xml
    :return: dictionary of vmid:vuart1
    """
    vm_id_dic = {}
    for i in list(common.VM_TYPES.keys()):
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
    if "SOS_VM" == common.VM_TYPES[index]:
        if index not in cpus_per_vm:
            sos_extend_all_cpus = board_cfg_lib.get_processor_info()
            cpus_per_vm[index] = sos_extend_all_cpus

    for i in range(len(cpus_per_vm[index])):
        if i == 0:
            if len(cpus_per_vm[index]) == 1:
                cpu_str = "(AFFINITY_CPU({0}U))".format(cpus_per_vm[index][0])
            else:
                cpu_str = "(AFFINITY_CPU({0}U)".format(cpus_per_vm[index][0])
        else:
            if i == len(cpus_per_vm[index]) - 1:
                cpu_str = cpu_str + " | AFFINITY_CPU({0}U))".format(cpus_per_vm[index][i])
            else:
                cpu_str = cpu_str + " | AFFINITY_CPU({0}U)".format(cpus_per_vm[index][i])

    vm_cpu_bmp['cpu_map'] = cpu_str
    vm_cpu_bmp['cpu_num'] = len(cpus_per_vm[index])
    return vm_cpu_bmp

def clos_assignment(clos_per_vm, index):
    """
    Get clos id assignment for vm by vm index
    :param clos_per_vm: a dictionary by vmid:cpus
    :param index: vm index
    :return: clos assignment string
    """
    vm_clos_bmp = {}

    for i in range(len(clos_per_vm[index])):
        if i == 0:
            if len(clos_per_vm[index]) == 1:
                clos_str = "{{{0}U}}".format(clos_per_vm[index][0])
            else:
                clos_str = "{{{0}U".format(clos_per_vm[index][0])
        else:
            if i == len(clos_per_vm[index]) - 1:
                clos_str = clos_str + ", {0}U}}".format(clos_per_vm[index][i])
            else:
                clos_str = clos_str + ", {0}U".format(clos_per_vm[index][i])

    vm_clos_bmp['clos_map'] = clos_str
    return vm_clos_bmp


def avl_vuart_ui_select(scenario_info):
    tmp_vuart = {}
    for vm_i,vm_type in common.VM_TYPES.items():

        if "SOS_VM" == VM_DB[vm_type]['load_type']:
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

    i = 0
    for vm_i,vm_type in common.VM_TYPES.items():
        if "POST_LAUNCHED_VM" == VM_DB[vm_type]['load_type']:
            i = vm_i
            break

    return (err_dic, i)


def avl_pci_devs():

    pci_dev_list = []
    (bdf_desc_map, bdf_vpid_map) = board_cfg_lib.get_pci_info(common.BOARD_INFO_FILE)
    pci_dev_list = common.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['ethernet'])
    tmp_pci_list = common.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['sata'])
    pci_dev_list.extend(tmp_pci_list)
    tmp_pci_list = common.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['nvme'])
    pci_dev_list.extend(tmp_pci_list)
    tmp_pci_list = common.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['usb'])
    pci_dev_list.extend(tmp_pci_list)
    pci_dev_list.insert(0, '')

    return pci_dev_list


def check_vuart(v0_vuart, v1_vuart):

    vm_target_id_dic = {}
    for vm_i,vuart_dic in v1_vuart.items():
        # check target vm id
        if 'base' not in vuart_dic.keys():
            key = "vm:id={},vuart:id=1,base".format(vm_i)
            ERR_LIST[key] = "base should be in xml"
            return

        if not vuart_dic['base'] or vuart_dic['base'] not in VUART_BASE:
            key = "vm:id={},vuart:id=1,base".format(vm_i)
            ERR_LIST[key] = "base should be SOS/COM BASE"

        if vuart_dic['base'] == "INVALID_COM_BASE":
            continue

        if 'target_vm_id' not in vuart_dic.keys():
            key = "vm:id={},vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id should be in xml"

        if not vuart_dic['target_vm_id'] or not vuart_dic['target_vm_id'].isnumeric():
            key = "vm:id={},vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id should be numeric of vm id"
        vm_target_id_dic[vm_i] = vuart_dic['target_vm_id']

    connect_set = False
    target_id_keys = list(vm_target_id_dic.keys())
    i = 0
    for vm_i,t_vm_id in vm_target_id_dic.items():
        if t_vm_id.isnumeric() and int(t_vm_id) not in common.VM_TYPES.keys():
            key = "vm:id={},vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id which specified does not exist"

        idx = target_id_keys.index(vm_i)
        i = idx
        for j in range(idx + 1, len(target_id_keys)):
            vm_j = target_id_keys[j]
            if int(vm_target_id_dic[vm_i]) == vm_j and int(vm_target_id_dic[vm_j]) == vm_i:
                connect_set = True

    if not connect_set and len(target_id_keys) >= 2:
        key = "vm:id={},vuart:id=1,target_vm_id".format(i)
        ERR_LIST[key] = "Creating the wrong configuration for target_vm_id."


def vcpu_clos_check(cpus_per_vm, clos_per_vm, prime_item, item):

    if not board_cfg_lib.is_rdt_enabled():
        return

    common_clos_max = board_cfg_lib.get_common_clos_max()

    for vm_i,vcpus in cpus_per_vm.items():
        clos_per_vm_len = 0
        if vm_i in clos_per_vm:
            clos_per_vm_len = len(clos_per_vm[vm_i])

        if clos_per_vm_len != len(vcpus):
            key = "vm:id={},{},{}".format(vm_i, prime_item, item)
            ERR_LIST[key] = "'vcpu_clos' number should be equal 'pcpu_id' number for VM{}".format(vm_i)
            return

        if board_cfg_lib.is_cdp_enabled() and common_clos_max != 0:
            for clos_val in clos_per_vm[vm_i]:
                if not clos_val or clos_val == None:
                    key = "vm:id={},{},{}".format(vm_i, prime_item, item)
                    ERR_LIST[key] = "'vcpu_clos' should be not None"
                    return

                if int(clos_val) >= common_clos_max:
                    key = "vm:id={},{},{}".format(vm_i, prime_item, item)
                    ERR_LIST[key] = "CDP_ENABLED=y, the clos value should not be greater than {} for VM{}".format(common_clos_max - 1, vm_i)
                    return
