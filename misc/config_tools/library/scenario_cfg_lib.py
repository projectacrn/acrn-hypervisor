# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import math
import acrn_config_utilities
import board_cfg_lib
import launch_cfg_lib

HEADER_LICENSE = acrn_config_utilities.open_license()
SERVICE_VM_UART1_VALID_NUM = ""
NATIVE_TTYS_DIC = {}

START_HPA_LIST = ['0', '0x100000000', '0x120000000']

KERN_TYPE_LIST = ['KERNEL_BZIMAGE', 'KERNEL_RAWIMAGE', 'KERNEL_ELF']
KERN_BOOT_ADDR_LIST = ['0x100000']

VUART_TYPE = ['VUART_LEGACY_PIO', 'VUART_PCI']
INVALID_COM_BASE = 'INVALID_COM_BASE'
VUART_BASE = ['SERVICE_VM_COM1_BASE', 'SERVICE_VM_COM2_BASE', 'SERVICE_VM_COM3_BASE', 'SERVICE_VM_COM4_BASE', 'COM1_BASE',
              'COM2_BASE', 'COM3_BASE', 'COM4_BASE', 'CONFIG_COM_BASE', INVALID_COM_BASE]
INVALID_PCI_BASE = 'INVALID_PCI_BASE'
PCI_VUART = 'PCI_VUART'
PCI_VUART_BASE = [PCI_VUART, INVALID_PCI_BASE]

AVALIBLE_COM1_BASE = [INVALID_COM_BASE, 'COM1_BASE']
AVALIBLE_COM2_BASE = [INVALID_COM_BASE, 'COM2_BASE']

VUART_IRQ = ['SERVICE_VM_COM1_IRQ', 'SERVICE_VM_COM2_IRQ', 'SERVICE_VM_COM3_IRQ', 'SERVICE_VM_COM4_IRQ',
             'COM1_IRQ', 'COM2_IRQ', 'COM3_IRQ', 'COM4_IRQ', 'CONFIG_COM_IRQ', '0']

# Support 512M, 1G, 2G
# pre launch less then 2G, sos vm less than 24G
START_HPA_SIZE_LIST = ['0x20000000', '0x40000000', '0x80000000']

COMMUNICATE_VM_ID = []

ERR_LIST = {}

PT_SUB_PCI = {}
PT_SUB_PCI['ethernet'] = ['Ethernet controller', 'Network controller', '802.1a controller',
                        '802.1b controller', 'Wireless controller']
PT_SUB_PCI['sata'] = ['SATA controller']
PT_SUB_PCI['nvme'] = ['Non-Volatile memory controller']
PT_SUB_PCI['usb'] = ['USB controller']
VM_DB = {
    'SERVICE_VM':{'load_type':'SERVICE_VM', 'severity':'SEVERITY_SERVICE_VM'},
    'SAFETY_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_SAFETY_VM'},
    'PRE_RT_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_RTVM'},
    'PRE_STD_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_STANDARD_VM'},
    'POST_STD_VM':{'load_type':'POST_LAUNCHED_VM', 'severity':'SEVERITY_STANDARD_VM'},
    'POST_RT_VM':{'load_type':'POST_LAUNCHED_VM', 'severity':'SEVERITY_RTVM'},
    'PRE_RT_VM':{'load_type':'PRE_LAUNCHED_VM', 'severity':'SEVERITY_RTVM'},
}
LOAD_VM_TYPE = list(VM_DB.keys())

# field names
F_TARGET_VM_ID = 'target_vm_id'
F_TARGET_UART_ID = 'target_uart_id'


def get_pt_pci_devs(pci_items):

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


def get_pt_pci_num(pci_devs):

    pci_devs_num = {}
    for vm_i,pci_devs_list in pci_devs.items():
        cnt_dev = 0
        for pci_dev in pci_devs_list:
            if not pci_dev:
                continue
            cnt_dev += 1

        pci_devs_num[vm_i] = cnt_dev

    return pci_devs_num

def get_shmem_regions(raw_shmem_regions):
    shmem_regions = {'err': []}
    for raw_shmem_region in raw_shmem_regions:
        if raw_shmem_region and raw_shmem_region.strip():
            shm_splited = raw_shmem_region.split(',')
            if len(shm_splited) == 3 and (shm_splited[0].strip() != '' and shm_splited[1].strip() != ''
                                          and len(shm_splited[2].split(':')) >= 2):
                name = shm_splited[0].strip()
                size = shm_splited[1].strip()
                vmid_list = shm_splited[2].split(':')
                for i in range(len(vmid_list)):
                    try:
                        int_vm_id = int(vmid_list[i])
                    except:
                        shmem_regions['err'].append(raw_shmem_region)
                        break
                    if int_vm_id not in shmem_regions.keys():
                        shmem_regions[int_vm_id] = [','.join([name, size, ':'.join(vmid_list[0:i]+vmid_list[i+1:])])]
                    else:
                        shmem_regions[int_vm_id].append(','.join([name, size, ':'.join(vmid_list[0:i]+vmid_list[i+1:])]))
            elif raw_shmem_region.strip() != '':
                shmem_regions['err'].append(raw_shmem_region)

    return shmem_regions


def get_shmem_num(shmem_regions):

    shmem_num = {}
    for shm_i, shm_list in shmem_regions.items():
        shmem_num[shm_i] = len(shm_list)

    return shmem_num


def get_pci_vuart_num(vuarts):

    vuarts_num = {}
    # get legacy vuart information
    vuart0_setting = acrn_config_utilities.get_vuart_info_id(acrn_config_utilities.SCENARIO_INFO_FILE, 0)
    vuart1_setting = acrn_config_utilities.get_vuart_info_id(acrn_config_utilities.SCENARIO_INFO_FILE, 1)
    for vm_i,vuart_list in vuarts.items():
        vuarts_num[vm_i] = 0
        for vuart_id in vuart_list:
            if vuarts[vm_i][vuart_id]['base'] != "INVALID_PCI_BASE":
                vuarts_num[vm_i] += 1

    for vm_i in vuart0_setting:
        load_order = acrn_config_utilities.LOAD_ORDER[vm_i]
        # Skip post-launched vm's pci base vuart0
        if "POST_LAUNCHED_VM" == load_order and 0 in vuarts[vm_i].keys() \
             and vuarts[vm_i][0]['base'] != "INVALID_PCI_BASE":
            vuarts_num[vm_i] -= 1
            continue
        # Skip pci vuart 0 if the legacy vuart 0 is enabled
        if vuart0_setting[vm_i]['base'] != "INVALID_COM_BASE" and 0 in vuarts[vm_i].keys() \
             and vuarts[vm_i][0]['base'] != "INVALID_PCI_BASE":
            vuarts_num[vm_i] -= 1
    for vm_i in vuart1_setting:
        # Skip pci vuart 1 if the legacy vuart 1 is enabled
        if vuart1_setting[vm_i]['base'] != "INVALID_COM_BASE" and 1 in vuarts[vm_i].keys() \
             and vuarts[vm_i][1]['base'] != "INVALID_PCI_BASE":
            vuarts_num[vm_i] -= 1
    return vuarts_num


def get_pci_dev_num_per_vm():
    pci_dev_num_per_vm = {}

    pci_items = acrn_config_utilities.get_leaf_tag_map(acrn_config_utilities.SCENARIO_INFO_FILE, "pci_devs", "pci_dev")
    pci_devs = get_pt_pci_devs(pci_items)
    pt_pci_num = get_pt_pci_num(pci_devs)

    ivshmem_region = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE,
        "FEATURES", "IVSHMEM", "IVSHMEM_REGION")

    shmem_enabled = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE,
        "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")

    shmem_regions = get_shmem_regions(ivshmem_region)
    shmem_num = get_shmem_num(shmem_regions)

    vuarts = acrn_config_utilities.get_vuart_info(acrn_config_utilities.SCENARIO_INFO_FILE)
    vuarts_num = get_pci_vuart_num(vuarts)

    for vm_i,load_order in acrn_config_utilities.LOAD_ORDER.items():
        if "POST_LAUNCHED_VM" == load_order:
            shmem_num_i = 0
            vuart_num = vuarts_num[vm_i]
            if shmem_enabled == 'y' and vm_i in shmem_num.keys():
                shmem_num_i = shmem_num[vm_i]
            pci_dev_num_per_vm[vm_i] = shmem_num_i + vuart_num
        elif "PRE_LAUNCHED_VM" == load_order:
            shmem_num_i = 0
            if shmem_enabled == 'y' and vm_i in shmem_num.keys():
                shmem_num_i = shmem_num[vm_i]
            pci_dev_num_per_vm[vm_i] = pt_pci_num[vm_i] + shmem_num_i + vuarts_num[vm_i]
        elif "SERVICE_VM" == load_order:
            shmem_num_i = 0
            if shmem_enabled == 'y' and vm_i in shmem_num.keys():
                shmem_num_i = shmem_num[vm_i]
            pci_dev_num_per_vm[vm_i] = shmem_num_i + vuarts_num[vm_i]

    return pci_dev_num_per_vm


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
    sos_vm_ids = []
    pre_vm_ids = []
    post_vm_ids = []
    rt_vm_ids = []
    for order_i, load_str in load_vms.items():
        if not load_str:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load should not empty"
            return

        if load_str not in LOAD_VM_TYPE:
            key = "vm:id={},{}".format(order_i, item)
            ERR_LIST[key] = "VM load order unknown"

        if "SERVICE_VM" == VM_DB[load_str]['load_type']:
            sos_vm_ids.append(order_i)

        if "PRE_LAUNCHED_VM" == VM_DB[load_str]['load_type']:
            pre_vm_ids.append(order_i)

        if "POST_STD_VM" == load_str:
            post_vm_ids.append(order_i)

        if "POST_RT_VM" == load_str:
            rt_vm_ids.append(order_i)

    if len(sos_vm_ids) > 1:
        key = "vm:id={},{}".format(sos_vm_ids[0], item)
        ERR_LIST[key] = "Service VM number should not be greater than 1"
        return

    if post_vm_ids and sos_vm_ids:
        if post_vm_ids[0] < sos_vm_ids[-1]:
            key = "vm:id={},{}".format(post_vm_ids[0], item)
            ERR_LIST[key] = "Post vm should be configured after SERVICE_VM"

    if pre_vm_ids and sos_vm_ids:
        if sos_vm_ids[-1] < pre_vm_ids[-1]:
            key = "vm:id={},{}".format(sos_vm_ids[0], item)
            ERR_LIST[key] = "Pre vm should be configured before SERVICE_VM"


def get_load_vm_cnt(load_vms, type_name):
    type_cnt = 0
    for load_str in load_vms.values():
        if type_name == VM_DB[load_str]['load_type']:
            type_cnt += 1

    return type_cnt


def guest_flag_check(guest_flags, branch_tag, leaf_tag):

    for vm_i, flags in guest_flags.items():
        for guest_flag in flags:
            if guest_flag and guest_flag not in acrn_config_utilities.GUEST_FLAG:
                key = "vm:id={},{},{}".format(vm_i, branch_tag, leaf_tag)
                ERR_LIST[key] = "Unknow guest flag"


def vm_cpu_affinity_check(scenario_file, launch_file, cpu_affinity):
    """
    Check cpu number of per vm
    :param : vm cpu_affinity item in xml
    :return: error informations
    """
    err_dic = {}
    use_cpus = []
    cpu_sharing_enabled = True

    cpu_sharing = acrn_config_utilities.get_hv_item_tag(acrn_config_utilities.SCENARIO_INFO_FILE, "FEATURES", "SCHEDULER")
    if cpu_sharing == "SCHED_NOOP":
        cpu_sharing_enabled = False

    # validate cpu_affinity config with scenario file
    sos_vmid = launch_cfg_lib.get_sos_vmid()
    scenario_cpu_aff = acrn_config_utilities.get_leaf_tag_map(scenario_file, "cpu_affinity", "pcpu_id")
    scenario_vm_names = {v: k for k, v in acrn_config_utilities.get_leaf_tag_map(scenario_file, 'name').items()}
    if launch_file:
        launch_vm_names = acrn_config_utilities.get_leaf_tag_map(launch_file, 'vm_name')
        for vm_id, cpu_ids in cpu_affinity.items():
            launch_vm_name = launch_vm_names[vm_id - sos_vmid]
            if launch_vm_name not in scenario_vm_names:
                # Dynamic VM, skip scenario cpu affinity subset check
                continue
            abs_vmid = scenario_vm_names[launch_vm_name]
            for vm_cpu in cpu_ids:
                if vm_cpu is None:
                    key = "vm:id={},{}".format(abs_vmid - sos_vmid, 'pcpu_id')
                    err_dic[key] = "This vm cpu_affinity is empty. " \
                                   "Please update your launch file accordingly."
                if vm_cpu not in scenario_cpu_aff[abs_vmid]:
                    key = "vm:id={},{}".format(abs_vmid - sos_vmid, 'pcpu_id')
                    err_dic[key] = "This pCPU is not included in this VM's allowed CPU pool. " \
                                   "Please update your scenario file accordingly or remove it from this list."

    if err_dic:
        return err_dic

    for vm_i,cpu in cpu_affinity.items():
        if cpu is not None and cpu in use_cpus and not cpu_sharing_enabled:
            key = "vm:id={},{}".format(vm_i, 'pcpu_id')
            err_dic[key] = "The same pCPU was configured in <pcpu_id>/<cpu_affinity>, but CPU sharing is disabled by 'SCHED_NOOP'. Please enable CPU sharing or update your CPU affinity configuration."
            return err_dic
        else:
            use_cpus.append(cpu)

    service_vm_cpus = []
    pre_launch_cpus = []
    post_launch_cpus = []
    for vm_i, load_order in acrn_config_utilities.LOAD_ORDER.items():
        if vm_i not in cpu_affinity.keys():
            continue
        elif VM_DB[load_order]['load_type'] == "PRE_LAUNCHED_VM":
            cpus = [x for x in cpu_affinity[vm_i] if not None]
            pre_launch_cpus.extend(cpus)
        elif VM_DB[load_order]['load_type'] == "POST_LAUNCHED_VM":
            cpus = [x for x in cpu_affinity[vm_i] if not None]
            post_launch_cpus.extend(cpus)
        elif VM_DB[load_order]['load_type'] == "SERVICE_VM":
            cpus = [x for x in cpu_affinity[vm_i] if not None]
            service_vm_cpus.extend(cpus)

        # duplicate cpus assign the same VM check
        cpus_vm_i = cpu_affinity[vm_i]
        for cpu_id in cpus_vm_i:
            if cpus_vm_i.count(cpu_id) >= 2:
                key = "vm:id={},{}".format(vm_i, 'pcpu_id')
                err_dic[key] = "VM should not use the same pcpu id:{}".format(cpu_id)
                return err_dic

    if pre_launch_cpus:
        if "SERVICE_VM" in acrn_config_utilities.LOAD_ORDER and not service_vm_cpus:
            key = "Service VM cpu_affinity"
            err_dic[key] = "Should assign CPU id for Service VM"

        for pcpu in pre_launch_cpus:
            if pre_launch_cpus.count(pcpu) >= 2:
                key = "Pre launched VM cpu_affinity"
                err_dic[key] = "Pre_launched_vm vm should not have the same cpus assignment"

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


def os_kern_load_addr_check(kern_type, id_kern_load_addr_dic, prime_item, item):
    """
    Check os kernel load address
    :param prime_item: the prime item in xml file
    :param item: vm os config load address item in xml
    :return: None
    """

    for id_key, kern_load_addr in id_kern_load_addr_dic.items():
        if kern_type[id_key] != 'KERNEL_RAWIMAGE':
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
        if kern_type[id_key] != 'KERNEL_RAWIMAGE':
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
    (bdf_desc_map, bdf_vpid_map) = board_cfg_lib.get_pci_info(acrn_config_utilities.BOARD_INFO_FILE)
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
    new_vm_id_dic = {}
    for i in list(acrn_config_utilities.LOAD_ORDER.keys()):
        for key in vm_vuart1[i].keys():
            if key == "target_vm_id":
                vm_id_dic[i] = vm_vuart1[i][key]

    # remove the unavailable vmid:target_vmid from dictionary
    vmid_list = list(vm_id_dic.keys())

    for vmid in vmid_list:
        target_vmid = vm_id_dic[vmid]
        if int(target_vmid) in vmid_list and vmid == int(vm_id_dic[int(target_vmid)]):
            new_vm_id_dic[vmid] = target_vmid

    return new_vm_id_dic


def cpus_assignment(cpus_per_vm, index):
    """
    Get cpu id assignment for vm by vm index
    :param cpus_per_vm: a dictionary by vmid:cpus
    :param index: vm index
    :return: cpu assignment string
    """
    vm_cpu_bmp = {}
    if "SERVICE_VM" == acrn_config_utilities.LOAD_ORDER[index]:
        if index not in cpus_per_vm or cpus_per_vm[index] == [None]:
            sos_extend_all_cpus = board_cfg_lib.get_processor_info()
            pre_all_cpus = []
            for vmid, cpu_list in cpus_per_vm.items():
                if vmid in acrn_config_utilities.LOAD_ORDER:
                    load_order = acrn_config_utilities.LOAD_ORDER[vmid]
                    load_type = ''
                    if load_order in VM_DB:
                        load_type = VM_DB[load_order]['load_type']
                    if load_type == "PRE_LAUNCHED_VM":
                        pre_all_cpus += cpu_list
            cpus_per_vm[index] = list(set(sos_extend_all_cpus) - set(pre_all_cpus))
            cpus_per_vm[index].sort()

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
    for vm_i,load_order in acrn_config_utilities.LOAD_ORDER.items():

        if "SERVICE_VM" == VM_DB[load_order]['load_type']:
            key = "vm={},legacy_vuart=0,base".format(vm_i)
            tmp_vuart[key] = ['SERVICE_VM_COM1_BASE', 'INVALID_COM_BASE']
            key = "vm={},legacy_vuart=1,base".format(vm_i)
            tmp_vuart[key] = ['SERVICE_VM_COM2_BASE', 'INVALID_COM_BASE']
        else:
            key = "vm={},legacy_vuart=0,base".format(vm_i)
            tmp_vuart[key] = ['INVALID_COM_BASE', 'COM1_BASE']
            key = "vm={},legacy_vuart=1,base".format(vm_i)
            tmp_vuart[key] = ['INVALID_COM_BASE', 'COM2_BASE']

    return tmp_vuart


def get_first_post_vm():

    i = 0
    for vm_i,load_order in acrn_config_utilities.LOAD_ORDER.items():
        if "POST_LAUNCHED_VM" == VM_DB[load_order]['load_type']:
            i = vm_i
            break

    return (err_dic, i)


def avl_pci_devs():

    pci_dev_list = []
    (bdf_desc_map, bdf_vpid_map) = board_cfg_lib.get_pci_info(acrn_config_utilities.BOARD_INFO_FILE)
    pci_dev_list = acrn_config_utilities.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['ethernet'])
    tmp_pci_list = acrn_config_utilities.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['sata'])
    pci_dev_list.extend(tmp_pci_list)
    tmp_pci_list = acrn_config_utilities.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['nvme'])
    pci_dev_list.extend(tmp_pci_list)
    tmp_pci_list = acrn_config_utilities.get_avl_dev_info(bdf_desc_map, PT_SUB_PCI['usb'])
    pci_dev_list.extend(tmp_pci_list)
    pci_dev_list.insert(0, '')

    return pci_dev_list


def check_vuart(v0_vuart, v1_vuart):

    vm_target_id_dic = {}
    for vm_i,vuart_dic in v1_vuart.items():
        # check target vm id
        if 'base' not in vuart_dic.keys():
            key = "vm:id={},legacy_vuart:id=1,base".format(vm_i)
            ERR_LIST[key] = "base should be in xml"
            return

        if not vuart_dic['base'] or vuart_dic['base'] not in VUART_BASE:
            key = "vm:id={},legacy_vuart:id=1,base".format(vm_i)
            ERR_LIST[key] = "base should be Service VM/COM BASE"

        if vuart_dic['base'] == "INVALID_COM_BASE":
            continue

        if 'target_vm_id' not in vuart_dic.keys():
            key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id should be in xml"

        if not vuart_dic['target_vm_id'] or not vuart_dic['target_vm_id'].isnumeric():
            key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id should be numeric of vm id"
        vm_target_id_dic[vm_i] = vuart_dic['target_vm_id']

    connect_set = False
    target_id_keys = list(vm_target_id_dic.keys())
    i = 0
    for vm_i,t_vm_id in vm_target_id_dic.items():
        if t_vm_id.isnumeric() and int(t_vm_id) not in acrn_config_utilities.LOAD_ORDER.keys():
            key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(vm_i)
            ERR_LIST[key] = "target_vm_id which specified does not exist"

        idx = target_id_keys.index(vm_i)
        i = idx
        for j in range(idx + 1, len(target_id_keys)):
            vm_j = target_id_keys[j]
            if int(vm_target_id_dic[vm_i]) == vm_j and int(vm_target_id_dic[vm_j]) == vm_i:
                connect_set = True

    if not connect_set and len(target_id_keys) >= 2:
        key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(i)
        ERR_LIST[key] = "Creating the wrong configuration for target_vm_id."


def get_legacy_vuart1_target_dict(legacy_vuart1):
    vuart1_target_dict = {}
    vuart1_visited = {}
    for vm_i, vuart_dict in legacy_vuart1.items():
        vuart_base = vuart_dict.get('base')
        if vuart_base not in VUART_BASE:
            continue
        if vuart_base == INVALID_COM_BASE:
            continue

        try:
            key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(vm_i)
            err_key = "vm:id={},legacy_vuart:id=1,target_uart_id".format(vm_i)
            target_vm_id = get_target_vm_id(vuart_dict, vm_i)
            target_uart_id = get_target_uart_id(vuart_dict)
        except XmlError as exc:
            ERR_LIST[err_key] = str(exc)
            return vuart1_target_dict, vuart1_visited

        if vm_i not in vuart1_target_dict:
            vuart1_target_dict[vm_i] = (target_vm_id, target_uart_id)
        else:
            raise ValueError('vm id {} has more than one legacy vuart 1'.format(vm_i))

        if vm_i not in vuart1_visited:
            vuart1_visited[vm_i] = -1
        else:
            raise ValueError('vm id {} has more than one legacy vuart 1'.format(vm_i))

    return vuart1_target_dict, vuart1_visited


class InvalidError(Exception):
    pass

class LegacyVuartError(Exception):
    pass

class PciVuartError(Exception):
    pass

class TargetError(Exception):
    pass
class XmlError(Exception):
    pass

def check_vuart_id(vuart_id):
    if not isinstance(vuart_id, int):
        raise ValueError('vuart_id must be int: {}, {!r}'.format(type(vuart_id), vuart_id))


def check_vuart_id_count(vm_pci_vuarts, legacy_vuart0, legacy_vuart1):
    vuart_cnt = 0
    for vuart_id in vm_pci_vuarts:
        pci_vuart_base = vm_pci_vuarts.get(vuart_id, {}).get('base')
        if pci_vuart_base == PCI_VUART:
            vuart_cnt += 1

    legacy_vuart_base0 = legacy_vuart0.get('base')
    if legacy_vuart_base0 != INVALID_COM_BASE:
        vuart_cnt += 1

    legacy_vuart_base1 = legacy_vuart1.get('base')
    if legacy_vuart_base1 != INVALID_COM_BASE:
        vuart_cnt += 1

    if vuart_cnt > acrn_config_utilities.MAX_VUART_NUM:
        raise XmlError("enables more than {} vuarts, total number: {}".format(acrn_config_utilities.MAX_VUART_NUM, vuart_cnt))


def check_against_coexistence(vm_pci_vuarts, vm_legacy_vuart, legacy_vuart_idx):
    pci_vuart_base = vm_pci_vuarts.get(legacy_vuart_idx, {}).get('base')
    legacy_base = vm_legacy_vuart.get('base')
    if legacy_base not in VUART_BASE:
        raise LegacyVuartError('legacy vuart base should be one of {}'.format(", ".join(VUART_BASE)))
    if legacy_base == INVALID_COM_BASE:
        return
    if pci_vuart_base not in PCI_VUART_BASE:
        raise PciVuartError('pci vuart base should be one of {}, last call: {!r}'.format(", ".join(PCI_VUART_BASE), pci_vuart_base))
    if pci_vuart_base == INVALID_PCI_BASE:
        return
    raise PciVuartError('cannot enable legacy vuart {} and this vuart {} at the same time' \
                    .format(legacy_vuart_idx, legacy_vuart_idx))


def check_pci_vuart_base(pci_vuart):
    if not isinstance(pci_vuart, dict):
        raise TypeError('pci_vuart should be a dict: {}, {!r}'.format(type(pci_vuart), pci_vuart))
    if 'base' not in pci_vuart:
        raise ValueError('base should be in vuart: keys_found={}'.format(pci_vuart.keys()))

    pci_vuart_base_str = pci_vuart['base']
    if pci_vuart_base_str not in PCI_VUART_BASE:
        raise ValueError("base should be one of {}, last called:  {!r}".format(", ".join(PCI_VUART_BASE), pci_vuart_base_str))
    if pci_vuart_base_str == INVALID_PCI_BASE:
        raise InvalidError


def get_target_vm_id(vuart, vm_id):
    if not isinstance(vuart, dict):
        raise TypeError('vuart should be a dict: {}, {!r}'.format(type(vuart), vuart))
    if not isinstance(vm_id, int):
        raise TypeError('vm_id should be an int: {}, {!r}'.format(type(vm_id), vm_id))
    if F_TARGET_VM_ID not in vuart:
        raise ValueError('target_vm_id should be in vuart: keys_found={}'.format(vuart.keys()))

    try:
        target_vm_id_str = vuart.get(F_TARGET_VM_ID)
        target_vm_id = int(target_vm_id_str)
    except (TypeError, ValueError):
        raise XmlError(
            "target_vm_id should be present and numeric: {!r}".format(
                target_vm_id_str))

    if target_vm_id not in acrn_config_utilities.LOAD_ORDER:
        raise XmlError(
            'invalid target_vm_id: target_vm_id={!r}, vm_ids={}'.format(
                target_vm_id, acrn_config_utilities.LOAD_ORDER.keys()))

    if target_vm_id == vm_id:
        raise XmlError(
            "cannot connect to itself, target_vm_id: {}".format(vm_id))
    return target_vm_id


def get_target_uart_id(vuart):
    if not isinstance(vuart, dict):
        raise TypeError('vuart should be a dict: {}, {!r}'.format(type(vuart), vuart))
    if F_TARGET_UART_ID not in vuart:
        raise ValueError('target_uart_id should be in vuart: keys_found={}'.format(vuart.keys()))

    try:
        target_uart_id_str = vuart.get(F_TARGET_UART_ID)
        target_uart_id = int(target_uart_id_str)
    except (TypeError, ValueError):
        raise XmlError(
            "target_uart_id_str should be present and numeric: {!r}".format(
                target_uart_id_str))
    if target_uart_id == 0:
        raise XmlError("cannot connect to any type of vuart 0")
    return target_uart_id


def check_pci_vuart(pci_vuarts, legacy_vuart0, legacy_vuart1):

    vm_target_dict = {}
    vm_visited = {}

    for vm_id, vm_pci_vuarts in pci_vuarts.items():

        try:
            vuart_id = 0
            check_against_coexistence(vm_pci_vuarts, legacy_vuart0.get(vm_id), vuart_id)
            vuart_id = 1
            check_against_coexistence(vm_pci_vuarts, legacy_vuart1.get(vm_id), vuart_id)
            key = "vm:id={}".format(vm_id)
            check_vuart_id_count(vm_pci_vuarts, legacy_vuart0.get(vm_id), legacy_vuart1.get(vm_id))
        except XmlError as exc:
            ERR_LIST[key] = str(exc)
            return
        except LegacyVuartError as exc:
            key = "vm:id={},legacy_vuart:id={},base".format(vm_id, vuart_id)
            ERR_LIST[key] = str(exc)
            return
        except PciVuartError as exc:
            err_key = (
                "vm:id={},console_vuart:id={},base".format(vm_id, vuart_id)
                if vuart_id == 0
                else "vm:id={},communication_vuart:id={},base".format(vm_id, vuart_id)
                )
            ERR_LIST[err_key] = str(exc)
            return

        for vuart_id, pci_vuart in vm_pci_vuarts.items():

            check_vuart_id(vuart_id)

            err_key = (
                "vm:id={},console_vuart:id={},base".format(vm_id, vuart_id)
                if vuart_id == 0
                else "vm:id={},communication_vuart:id={},base".format(vm_id, vuart_id)
                )
            key = err_key

            try:
                check_pci_vuart_base(pci_vuart)
            except ValueError as exc:
                ERR_LIST[err_key] = str(exc)
                return
            except InvalidError:
                continue

            if vuart_id == 0:
                continue

            try:
                err_key = "vm:id={},communication_vuart:id={},target_vm_id".format(vm_id, vuart_id)
                target_vm_id = get_target_vm_id(pci_vuart, vm_id)
                err_key = "vm:id={},communication_vuart:id={},target_uart_id".format(vm_id, vuart_id)
                target_uart_id = get_target_uart_id(pci_vuart)
            except XmlError as exc:
                ERR_LIST[err_key] = str(exc)
                return

            if vm_id not in vm_target_dict:
                vm_target_dict[vm_id] = {}
            if vuart_id in vm_target_dict[vm_id]:
                raise ValueError('Duplicated vm id {} and vuart id {}'.format(vm_id, vuart_id))
            else:
                vm_target_dict[vm_id][vuart_id] = (target_vm_id, target_uart_id)

            if vm_id not in vm_visited:
                vm_visited[vm_id] = {}
            if vuart_id in vm_visited[vm_id]:
                raise ValueError('vuart {} of vm {} is duplicated'.format(vuart_id, vm_id))
            else:
                vm_visited[vm_id][vuart_id] = -1


    legacy_vuart1_target_dict, legacy_vuart1_visited = get_legacy_vuart1_target_dict(legacy_vuart1)

    for vm_id, target in legacy_vuart1_target_dict.items():
        try:
            err_key = "vm:id={},legacy_vuart:id=1,target_vm_id".format(vm_id)
            target_vm_id = int(target[0])
            is_target_vm_available(target_vm_id, vm_visited, legacy_vuart1_visited)
            err_key = "vm:id={},legacy_vuart:id=1,target_uart_id".format(vm_id)
            target_uart_id = int(target[1])
            check_target_connection(vm_id, target_vm_id, target_uart_id, vm_visited, legacy_vuart1_visited)
        except TargetError as exc:
            ERR_LIST[err_key] = str(exc)
            continue

    for vm_id, target_list in vm_target_dict.items():
        for vuart_id, target in target_list.items():
            try:
                err_key = "vm:id={},communication_vuart:id={},target_vm_id".format(vm_id, vuart_id)
                target_vm_id = int(target[0])
                is_target_vm_available(target_vm_id, vm_visited, legacy_vuart1_visited)
                err_key = "vm:id={},communication_vuart:id={},target_uart_id".format(vm_id, vuart_id)
                target_uart_id = int(target[1])
                check_target_connection(vm_id, target_vm_id, target_uart_id, vm_visited, legacy_vuart1_visited)
            except TargetError as exc:
                ERR_LIST[err_key] = str(exc)
                continue


def is_target_vm_available(target_vm_id, vm_visited, legacy_vuart1_visited):
    if not isinstance(target_vm_id, int):
        raise TypeError('target_vm_id should be an int: {}, {!r}'.format(type(target_vm_id), target_vm_id))
    if not isinstance(vm_visited, dict):
        raise TypeError('vm_visited should be a dict: {}, {!r}'.format(type(vm_visited), vm_visited))
    if not isinstance(legacy_vuart1_visited, dict):
        raise TypeError('legacy_vuart1_visited should be a dict: {}, {!r}' \
                 .format(type(legacy_vuart1_visited), legacy_vuart1_visited))

    if target_vm_id not in acrn_config_utilities.LOAD_ORDER:
        raise TargetError("target vm {} is not present".format(target_vm_id))
    if target_vm_id in vm_visited:
        pass
    elif target_vm_id in legacy_vuart1_visited:
        pass
    else:
        raise TargetError("target vm {} disables legacy vuart 1 and all communication vuarts"\
                         .format(target_vm_id))


def check_target_connection(vm_id, target_vm_id, target_uart_id, vm_visited, legacy_vuart1_visited):
    if not isinstance(vm_visited, dict):
        raise TypeError('vm_visited should be a dict: {}, {!r}'.format(type(vm_visited), vm_visited))
    if not isinstance(legacy_vuart1_visited, dict):
        raise TypeError('legacy_vuart1_visited should be a dict: {}, {!r}' \
                 .format(type(legacy_vuart1_visited), legacy_vuart1_visited))
    if not isinstance(target_vm_id, int):
        raise TypeError('vm_id should be an int: {}, {!r}'.format(type(vm_id), vm_id))
    if not isinstance(target_vm_id, int):
        raise TypeError('target_vm_id should be an int: {}, {!r}'.format(type(target_vm_id), target_vm_id))
    if not isinstance(target_uart_id, int):
        raise TypeError('target_uart_id should be an int: {}, {!r}'.format(type(target_uart_id), target_uart_id))

    if target_uart_id == 0:
        raise TargetError("cannot connect to any type of vuart 0")

    if vm_visited.get(target_vm_id) and target_uart_id in vm_visited[target_vm_id]:
        connected_vm = vm_visited[target_vm_id][target_uart_id]
        if  connected_vm > -1:
            raise TargetError("target vm{} : vuart{} is connected to vm {}" \
                             .format(target_vm_id, target_uart_id, connected_vm))
        else:
            vm_visited[target_vm_id][target_uart_id] = vm_id
    elif target_uart_id == 1 and target_vm_id in legacy_vuart1_visited:
        connected_vm = legacy_vuart1_visited[target_vm_id]
        if  connected_vm > -1:
            raise TargetError("target vm{} : vuart{} is connected to vm {}" \
                             .format(target_vm_id, target_uart_id, connected_vm))
        else:
            legacy_vuart1_visited[target_vm_id] = vm_id
    else:
        raise TargetError("target vm{}'s vuart{} is not present".format(target_vm_id ,target_uart_id))


def vcpu_clos_check(cpus_per_vm, clos_per_vm, guest_flags, prime_item, item):

    if not board_cfg_lib.is_rdt_enabled():
        return

    common_clos_max = board_cfg_lib.get_common_clos_max()

    for vm_i,vcpus in cpus_per_vm.items():
        if vm_i in guest_flags and "GUEST_FLAG_VCAT_ENABLED" in guest_flags[vm_i]:
            continue

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


def share_mem_check(shmem_regions, raw_shmem_regions, vm_type_info, prime_item, item, sub_item):

    shmem_names = {}

    MAX_SHMEM_REGION_NUM = 8
    shmem_region_num = 0
    for raw_shmem_region in raw_shmem_regions:
        if raw_shmem_region is not None and  raw_shmem_region.strip() != '':
            shmem_region_num += 1
    if shmem_region_num > MAX_SHMEM_REGION_NUM:
        key = "hv,{},{},{},{}".format(prime_item, item, sub_item, MAX_SHMEM_REGION_NUM)
        ERR_LIST[key] = "The number of hv-land shmem regions should not be greater than {}.".format(MAX_SHMEM_REGION_NUM)
        return

    for shm_i, shm_list in shmem_regions.items():
        for shm_str in shm_list:
            index = -1
            if shm_i == 'err':
                for i in range(len(raw_shmem_regions)):
                    if raw_shmem_regions[i] == shm_str:
                        index = i
                        break
            if index == -1:
                try:
                    for i in range(len(raw_shmem_regions)):
                        if raw_shmem_regions[i].split(',')[0].strip() == shm_str.split(',')[0].strip():
                            index = i
                            break
                except:
                    index = 0
            key = "hv,{},{},{},{}".format(prime_item, item, sub_item, index)

            shm_str_splited = shm_str.split(',')
            if len(shm_str_splited) < 3:
                ERR_LIST[key] = "The name, size, communication VM IDs of the share memory should be separated " \
                                "by comma and not be empty."
                return
            try:
                curr_vm_id = int(shm_i)
            except:
                ERR_LIST[key] = "The shared memory region should be configured with format like this: hv:/shm_region_0, 2, 0:2"
                return
            name = shm_str_splited[0].strip()
            size = shm_str_splited[1].strip()
            vmid_list = shm_str_splited[2].split(':')
            int_vmid_list = []
            for vmid in vmid_list:
                try:
                    int_vmid = int(vmid)
                    int_vmid_list.append(int_vmid)
                except:
                    ERR_LIST[key] = "The communication VM IDs of the share memory should be decimal and separated by comma."
                    return
            if not int_vmid_list:
                ERR_LIST[key] = "The communication VM IDs of the share memory should be decimal and separated by comma."
                return
            if curr_vm_id in int_vmid_list or len(set(int_vmid_list)) != len(int_vmid_list):
                ERR_LIST[key] = "The communication VM IDs of the share memory should not be duplicated."
                return
            for target_vm_id in int_vmid_list:
                if curr_vm_id not in vm_type_info.keys() or target_vm_id not in vm_type_info.keys():
                    ERR_LIST[key] = "Shared Memory can be only configured for existed VMs."
                    return

            if name =='' or size == '':
                ERR_LIST[key] = "The name, size of the share memory should not be empty."
                return
            name_len = len(name)
            if name_len > 32 or name_len == 0:
                ERR_LIST[key] = "The size of share Memory name should be in range [1,32] bytes."
                return

            int_size = 0
            try:
                int_size = int(size) * 0x100000
            except:
                ERR_LIST[key] = "The size of the shared memory region should be a decimal number."
                return
            if int_size < 0x200000 or int_size > 0x20000000:
                ERR_LIST[key] = "The size of the shared memory region should be in the range of [2MB, 512MB]."
                return
            if not ((int_size & (int_size-1) == 0) and int_size != 0):
                ERR_LIST[key] = "The size of share Memory region should be a power of 2."
                return

            if name in shmem_names.keys():
                shmem_names[name] += 1
            else:
                shmem_names[name] = 1
            if shmem_names[name] > len(vmid_list)+1:
                ERR_LIST[key] = "The names of share memory regions should not be duplicated: {}".format(name)
                return

    board_cfg_lib.parse_mem()
    for shm_i, shm_list in shmem_regions.items():
        for shm_str in shm_list:
            shm_str_splited = shm_str.split(',')
            name = shm_str_splited[0].strip()
            index = 0
            try:
                for i in range(len(raw_shmem_regions)):
                    if raw_shmem_regions[i].split(',')[0].strip() == shm_str.split(',')[0].strip():
                        index = i
                        break
            except:
                index = 0
            key = "hv,{},{},{},{}".format(prime_item, item, sub_item, index)
            if 'IVSHMEM_'+name in board_cfg_lib.PCI_DEV_BAR_DESC.shm_bar_dic.keys():
                bar_attr_dic = board_cfg_lib.PCI_DEV_BAR_DESC.shm_bar_dic['IVSHMEM_'+name]
                if (0 in bar_attr_dic.keys() and int(bar_attr_dic[0].addr, 16) < 0x80000000) \
                    or (2 in bar_attr_dic.keys() and int(bar_attr_dic[2].addr, 16) < 0x100000000):
                    ERR_LIST[key] = "Failed to get the start address of the shared memory, please check the size of it."
                    return


def check_p2sb(enable_p2sb):

    for vm_i,p2sb in enable_p2sb.items():
        if vm_i != 0:
            key = "vm:id={},p2sb".format(vm_i)
            ERR_LIST[key] = "Can only specify p2sb passthru for VM0"
            return

        if p2sb and not VM_DB[acrn_config_utilities.LOAD_ORDER[0]]['load_type'] == "PRE_LAUNCHED_VM":
            ERR_LIST["vm:id=0,p2sb"] = "p2sb passthru can only be enabled for Pre-launched VM"
            return

        if p2sb and not board_cfg_lib.is_p2sb_passthru_possible():
            ERR_LIST["vm:id=0,p2sb"] = "p2sb passthru is not allowed for this board"
            return

        if p2sb and board_cfg_lib.is_tpm_passthru():
            ERR_LIST["vm:id=0,p2sb"] = "Cannot enable p2sb and tpm passthru at the same time"
            return


def check_pt_intx(phys_gsi, virt_gsi):

    if not phys_gsi and not virt_gsi:
        return

    if len(phys_gsi) == 0 and len(virt_gsi) == 0:
        return

    if not board_cfg_lib.is_matched_board(('ehl-crb-b','generic_board')):
        ERR_LIST["pt_intx"] = "only board ehl-crb-b/generic_board is supported"
        return

    if not VM_DB[acrn_config_utilities.LOAD_ORDER[0]]['load_type'] == "PRE_LAUNCHED_VM":
       ERR_LIST["pt_intx"] = "pt_intx can only be specified for pre-launched VM"
       return

    for (id1,p), (id2,v) in zip(phys_gsi.items(), virt_gsi.items()):
        if id1 != 0 or id2 != 0:
            ERR_LIST["pt_intx"] = "virt_gsi and phys_gsi can only be specified for VM0"
            return

        if len(p) != len(v):
            ERR_LIST["vm:id=0,pt_intx"] = "virt_gsi and phys_gsi must have same length"
            return

        if len(p) != len(set(p)):
            ERR_LIST["vm:id=0,pt_intx"] = "phys_gsi contains duplicates"
            return

        if len(v) != len(set(v)):
            ERR_LIST["vm:id=0,pt_intx"] = "virt_gsi contains duplicates"
            return

        if len(p) > 120:
            ERR_LIST["vm:id=0,pt_intx"] = "# of phys_gsi and virt_gsi pairs must not be greater than 120"
            return

        if not all(pin < 120 for pin in v):
            ERR_LIST["vm:id=0,pt_intx"] = "virt_gsi must be less than 120"
            return


def get_valid_ttys_for_sos_vuart(ttys_n):
    """
    Get available ttysn list for vuart0/vuart1
    :param ttys_n: the serial port was chosen as hv console
     """
    vuart0_valid = []
    vuart1_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
    ttys_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")
    if ttys_lines:
        vuart0_valid.clear()
        for tty_line in ttys_lines:
            tmp_dic = {}
            #seri:/dev/ttySx type:mmio base:0x91526000 irq:4 [bdf:"00:18.0"]
            #seri:/dev/ttySy type:portio base:0x2f8 irq:5
            tty = tty_line.split('/')[2].split()[0]
            ttys_irq = tty_line.split()[3].split(':')[1].strip()
            ttys_type = tty_line.split()[1].split(':')[1].strip()
            tmp_dic['irq'] = int(ttys_irq)
            tmp_dic['type'] = ttys_type
            NATIVE_TTYS_DIC[tty] = tmp_dic
            vuart0_valid.append(tty)
            if tty and tty in vuart1_valid:
                vuart1_valid.remove(tty)

    if not vuart1_valid:
        acrn_config_utilities.print_yel("ttyS are fully used. ttyS0 is used for hv_console, ttyS1 is used for vuart1!", warn=True)
        vuart1_valid = ['ttyS0', 'ttyS1', 'ttyS2', 'ttyS3']
        if ttys_n in vuart1_valid:
            vuart1_valid.remove(ttys_n)

    return (vuart0_valid, vuart1_valid)


def get_sos_vuart_settings(launch_flag=True):
    """
    Get vuart setting from scenario setting
    :return: vuart0/vuart1 setting dictionary
    """
    global SERVICE_VM_UART1_VALID_NUM
    err_dic = {}
    vuart0_setting = {}
    vuart1_setting = {}

    (err_dic, ttys_n) = board_cfg_lib.parser_hv_console()
    if err_dic:
        if launch_flag:
            SERVICE_VM_UART1_VALID_NUM += "ttyS1"
            return
        return err_dic

    if ttys_n:
        (vuart0_valid, vuart1_valid) = get_valid_ttys_for_sos_vuart(ttys_n)

        # VUART0 setting
        if not launch_flag:
            if ttys_n not in list(NATIVE_TTYS_DIC.keys()):
                vuart0_setting['ttyS0'] = board_cfg_lib.alloc_irq()
            else:
                if int(NATIVE_TTYS_DIC[ttys_n]['irq']) >= 16:
                    vuart0_setting[ttys_n] = board_cfg_lib.alloc_irq()
                else:
                    vuart0_setting[ttys_n] = NATIVE_TTYS_DIC[ttys_n]['irq']
    else:
        vuart1_valid = ['ttyS1']

    if launch_flag:
        SERVICE_VM_UART1_VALID_NUM += vuart1_valid[0]
        return

    # VUART1 setting
    # The IRQ of vUART1(COM2) might be hard-coded by Service VM ACPI table(i.e. host ACPI),
    # so we had better follow native COM2 IRQ assignment for vUART1 if COM2 is a legacy ttyS,
    # otherwise function of vUART1 would be failed. If host COM2 does not exist or it is a PCI ttyS,
    # then we could allocate a free IRQ for vUART1.

    if 'ttyS1' in NATIVE_TTYS_DIC.keys() \
        and NATIVE_TTYS_DIC['ttyS1']['type'] == "portio" \
        and 'irq' in list(NATIVE_TTYS_DIC['ttyS1'].keys()) \
        and NATIVE_TTYS_DIC['ttyS1']['irq'] < 16:
        vuart1_setting['ttyS1'] = NATIVE_TTYS_DIC['ttyS1']['irq']
    else:
        vuart1_setting[vuart1_valid[0]] = board_cfg_lib.alloc_irq()

    return (err_dic, vuart0_setting, vuart1_setting)
