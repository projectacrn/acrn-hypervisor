# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import getopt
import re

import common
import board_cfg_lib
import scenario_cfg_lib
import lxml
import lxml.etree

ERR_LIST = {}
BOOT_TYPE = ['no', 'vsbl', 'ovmf']
RTOS_TYPE = ['no', 'Soft RT', 'Hard RT']
DM_VUART0 = ['Disable', 'Enable']
y_n = ['y', 'n']
UOS_TYPES = ['CLEARLINUX', 'ANDROID', 'ALIOS', 'PREEMPT-RT LINUX', 'VXWORKS', 'WINDOWS', 'ZEPHYR', 'YOCTO', 'UBUNTU', 'GENERIC LINUX']
LINUX_LIKE_OS = ['CLEARLINUX', 'PREEMPT-RT LINUX', 'YOCTO', 'UBUNTU', 'GENERIC LINUX']

PT_SUB_PCI = {}
PT_SUB_PCI['usb_xdci'] = ['USB controller']
PT_SUB_PCI['gpu'] = ['VGA compatible controller']
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
PASSTHRU_DEVS = ['usb_xdci', 'gpu', 'ipu', 'ipu_i2c', 'cse', 'audio', 'sata',
                    'nvme', 'audio_codec', 'sd_card', 'ethernet', 'wifi', 'bluetooth']

PT_SLOT = {
        "hostbridge":0,
        "lpc":1,
        "pci-gvt":2,
        "virtio-blk":3,
        "igd-lpc":31,
    }


PM_CHANNEL = ['', 'IOC', 'PowerButton', 'vuart1(pty)', 'vuart1(tty)']
PM_CHANNEL_DIC = {
    None:'',
    'IOC':'--pm_notify_channel ioc',
    'PowerButton':'--pm_notify_channel power_button',
    'vuart1(pty)':'--pm_by_vuart pty,/run/acrn/life_mngr_$vm_name \\\n   -l com2,/run/acrn/life_mngr_$vm_name',
    'vuart1(tty)':'--pm_by_vuart tty,/dev/',
}

MOUNT_FLAG_DIC = {}


def usage(file_name):
    """ This is usage for how to use this tool """
    print("usage= {} [h]".format(file_name), end="")
    print("--board <board_info_file> --scenario <scenario_info_file> --launch <launch_info_file> --uosid <uosid id> --out [output folder]")
    print('board_info_file :  file name of the board info')
    print('scenario_info_file :  file name of the scenario info')
    print('launch_info_file :  file name of the launch info')
    print('uosid :  this is the relative id for post launch vm in scenario info XML:[1..max post launch vm]')
    print('output folder :  path to acrn-hypervisor_folder')


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
    output_folder = False
    param_list = ['--board', '--scenario', '--launch', '--uosid']

    for arg_str in param_list:

        if arg_str not in args:
            usage(args[0])
            err_dic['common error: wrong parameter'] = "wrong usage"
            return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    args_list = args[1:]
    (optlist, args_list) = getopt.getopt(args_list, '', ['board=', 'scenario=', 'launch=', 'uosid=', 'out='])
    for arg_k, arg_v in optlist:
        if arg_k == '--board':
            board_info_file = arg_v
        if arg_k == '--scenario':
            scenario_info_file = arg_v
        if arg_k == '--launch':
            launch_info_file = arg_v
        if arg_k == '--out':
            output_folder = arg_v
        if '--uosid' in args:
            if arg_k == '--uosid':
                vm_th = arg_v
                if not vm_th.isnumeric():
                    err_dic['common error: wrong parameter'] = "--uosid should be a number"
                    return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    if not board_info_file or not scenario_info_file or not launch_info_file:
        usage(args[0])
        err_dic['common error: wrong parameter'] = "wrong usage"
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    if not os.path.exists(board_info_file):
        err_dic['common error: wrong parameter'] = "{} does not exist!".format(board_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    if not os.path.exists(scenario_info_file):
        err_dic['common error: wrong parameter'] = "{} does not exist!".format(scenario_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    if not os.path.exists(launch_info_file):
        err_dic['common error: wrong parameter'] = "{} does not exist!".format(launch_info_file)
        return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)

    return (err_dic, board_info_file, scenario_info_file, launch_info_file, int(vm_th), output_folder)


def launch_vm_cnt(config_file):
    """
    Get post vm number
    :param config_file: it is a file what contains information for script to read from
    :return: total post vm number in launch file
    """
    post_vm_count = 0

    # get post vm number
    root = common.get_config_root(config_file)
    for item in root:
        if item.tag == "uos":
            post_vm_count += 1

    return post_vm_count


def get_post_num_list():
    """
    Get post vm number list
    :return: total post dic: {launch_id:scenario_id} in launch file
    """
    post_vm_list = []

    # get post vm number
    root = common.get_config_root(common.LAUNCH_INFO_FILE)
    for item in root:
        if item.tag == "uos":
            post_vm_list.append(int(item.attrib['id']))

    return post_vm_list


def post_vm_cnt(config_file):
    """
    Calculate the pre launched vm number
    :param config_file: it is a file what contains information for script to read from
    :return: number of post launched vm
    """
    post_launch_cnt = 0

    for vm_type in common.VM_TYPES.values():
        if scenario_cfg_lib.VM_DB[vm_type]['load_type'] == "POST_LAUNCHED_VM":
            post_launch_cnt += 1

    return post_launch_cnt


def get_post_vm_cnt():
    """
    Get board name from launch.xml at fist line
    :param scenario_file: it is a file what contains scenario information for script to read from
    """
    launch_vm_count = launch_vm_cnt(common.LAUNCH_INFO_FILE)
    post_vm_count = post_vm_cnt(common.SCENARIO_INFO_FILE)
    return (launch_vm_count, post_vm_count)


def is_config_file_match():

    match = True
    # check if the board config match scenario config
    (err_dic, scenario_for_board) = common.get_xml_attrib(common.SCENARIO_INFO_FILE, "board")
    (err_dic, board_name) = common.get_xml_attrib(common.BOARD_INFO_FILE, "board")
    if scenario_for_board != board_name:
        err_dic['scenario config'] = "The board xml file does not match scenario xml file!"
        match = False

    # check if the board config match launch config
    (err_dic, launch_for_board) = common.get_xml_attrib(common.LAUNCH_INFO_FILE, "board")
    if launch_for_board != board_name:
        err_dic['launch config'] = "The board xml file does not match scenario xml file!"
        match = False

    return (err_dic, match)


def get_vm_uuid_idx(vm_type, uosid):

    i_cnt = 0
    for vm_i,vm_t in common.VM_TYPES.items():
        if vm_t == vm_type and vm_i <= uosid:
            i_cnt += 1
    if i_cnt > 0:
        i_cnt -= 1

    return i_cnt


def get_scenario_uuid(uosid, sos_vmid):
    # {id_num:uuid} (id_num:0~max)
    scenario_uuid = ''
    vm_id = uosid + sos_vmid
    i_cnt = get_vm_uuid_idx(common.VM_TYPES[vm_id], vm_id)
    scenario_uuid = scenario_cfg_lib.VM_DB[common.VM_TYPES[vm_id]]['uuid'][i_cnt]
    return scenario_uuid



def get_sos_vmid():

    sos_id = ''
    for vm_i,vm_type in common.VM_TYPES.items():
        if vm_type == "SOS_VM":
            sos_id = vm_i
            break

    return sos_id


def get_bdf_from_tag(config_file, branch_tag, tag_str):
    bdf_list = {}
    bdf_list = common.get_leaf_tag_map(config_file, branch_tag, tag_str)

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


def get_uos_type():
    """
    Get uos name from launch.xml at fist line
    """
    uos_types = common.get_leaf_tag_map(common.LAUNCH_INFO_FILE, "uos_type")

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
    skip_check_list = ['']
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


def mem_size_check(arg_list, item):
    """
     check memory size list which are set from webUI
     :param arg_list: the list of arguments from config xml
     :param item: the item of tag from config xml
     :return: None
     """
    # get total memory information
    total_mem_mb = board_cfg_lib.get_total_mem()

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
            slot_fun = virtual_dev_slot(dev)
            PT_SLOT[dev] = slot_fun
            slot_list[p_id] = slot_fun

    return slot_list


def get_pt_dev():
    """ Get passthrough device list """
    cap_pt = PASSTHRU_DEVS

    return cap_pt


def get_vuart1_from_scenario(vmid):
    """Get the vmid's  vuart1 base"""
    vuart1 = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 1)
    return vuart1[vmid]['base']


def pt_devs_check_audio(audio_map, audio_codec_map):
    """
    Check the connections about audio/audio_codec pass-through devices
    If audio_codec is selected as pass-through device, the audio device
    must to be chosen as pass-through device either.
    :param audio_map: the dictionary contains vmid and bdf of audio device
    :param audio_codec_map: the dictionary contains vmid and bdf of audio_codec device
    """
    for vmid in list(audio_map.keys()):
        bdf_audio = audio_map[vmid]
        bdf_codec = audio_codec_map[vmid]
        if not bdf_audio and bdf_codec:
            key = "uos:id={},passthrough_devices,{}".format(vmid, 'audio_codec')
            ERR_LIST[key] = "Audio codec device should be pass through together with Audio devcie!"


def check_block_mount(virtio_blk_dic):
    (blk_dev_list, num) = board_cfg_lib.get_rootfs(common.BOARD_INFO_FILE)
    for vmid in list(virtio_blk_dic.keys()):
        mount_flags = []
        for blk in virtio_blk_dic[vmid]:
            rootfs_img = ''
            if not blk:
                mount_flags.append(False)
                continue

            if ':' in blk:
                blk_dev = blk.split(':')[0]
                rootfs_img = blk.split(':')[1]
            else:
                blk_dev = blk

            if blk_dev in blk_dev_list and rootfs_img:
                mount_flags.append(True)
            else:
                mount_flags.append(False)

        MOUNT_FLAG_DIC[vmid] = mount_flags


def check_sriov_param(sriov_dev, pt_sel):
    for dev_type in ['gpu', 'network']:
        for vm_id, dev_bdf in sriov_dev[dev_type].items():
            if not dev_bdf:
                continue
            pt_devname = dev_type
            if pt_devname == 'network':
                pt_devname = 'ethernet'
            if pt_sel.bdf[pt_devname][vm_id]:
                ERR_LIST[
                    'vmid:{} sriov {}'.format(vm_id, dev_type)
                ] = 'this vm has {} passthrough and sriov {} at same time!'.format(pt_devname, dev_type)
            if not re.match(r'^[\da-fA-F]{2}:[0-3][\da-fA-F]\.[0-7]$', dev_bdf):
                ERR_LIST['vmid:{} sriov {}'.format(vm_id, dev_type)] = 'sriov {} bdf error'.format(dev_type)


def bdf_duplicate_check(bdf_dic):
    """
    Check if exist duplicate slot
    :param bdf_dic: contains all selected pass-through devices
    :return: None
    """
    bdf_used = []
    for dev in bdf_dic.keys():
        dev_bdf_dic = bdf_dic[dev]
        for vm_i in dev_bdf_dic.keys():
            dev_bdf = dev_bdf_dic[vm_i]
            if not dev_bdf:
                continue

            if dev_bdf in bdf_used:
                key = "uos:id={},{},{}".format(vm_i, 'passthrough_devices', dev)
                ERR_LIST[key] = "You select the same device for {} pass-through !".format(dev)
                return
            else:
                bdf_used.append(dev_bdf)


def get_gpu_bdf():

    pci_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<PCI_DEVICE>", "</PCI_DEVICE>")

    for line in pci_lines:
        if "VGA compatible controller" in line:
            global gpu_bdf
            gpu_bdf = line.split('\t')[1]
            gpu_bdf = gpu_bdf[0:7]
    return gpu_bdf


def get_vpid_by_bdf(bdf):
    vpid = ''
    vpid_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<PCI_VID_PID>", "</PCI_VID_PID>")

    for vpid_line in vpid_lines:
        if bdf in vpid_line:
            vpid = " ".join(vpid_line.split()[2].split(':'))
    return vpid


def get_gpu_vpid():
    gpu_bdf = get_gpu_bdf()
    return get_vpid_by_bdf(gpu_bdf)



def uos_cpu_affinity(uosid_cpu_affinity):

    cpu_affinity = {}
    sos_vm_id = get_sos_vmid()
    for uosid,cpu_affinity_list in uosid_cpu_affinity.items():
        cpu_affinity[int(uosid) + int(sos_vm_id)] = cpu_affinity_list
    return cpu_affinity


def check_slot(slot_db):

    slot_values = {}
    # init list of slot values for Post VM
    for dev in slot_db.keys():
        for uosid in slot_db[dev].keys():
            slot_values[uosid] = []
        break

    # get slot values for Passthrough devices
    for dev in PASSTHRU_DEVS:
        if dev == 'gpu':
            continue
        for uosid,slot_str in slot_db[dev].items():
            if not slot_str:
                continue
            slot_values[uosid].append(slot_str)

    # update slot values and replace the fun=0 if there is no fun 0 in bdf list
    for dev in PASSTHRU_DEVS:
        if dev == 'gpu':
            continue
        for uosid,slot_str in slot_db[dev].items():
            if not slot_str or ':' not in str(slot_str):
                continue
            bus_slot = slot_str[0:-1]
            bus_slot_fun0 = bus_slot + "0"
            if bus_slot_fun0 not in slot_values[uosid]:
                slot_db[dev][uosid] = bus_slot_fun0
                slot_values[uosid].append(bus_slot_fun0)


def is_linux_like(uos_type):

    is_linux = False
    if uos_type in LINUX_LIKE_OS:
        is_linux = True

    return is_linux


def set_shm_regions(launch_item_values, scenario_info):

    try:
        raw_shmem_regions = common.get_hv_item_tag(scenario_info, "FEATURES", "IVSHMEM", "IVSHMEM_REGION")
        vm_types = common.get_leaf_tag_map(scenario_info, "vm_type")
        shm_enabled = common.get_hv_item_tag(scenario_info, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")
    except:
        return

    sos_vm_id = 0
    for vm_id, vm_type in vm_types.items():
        if vm_type in ['SOS_VM']:
            sos_vm_id = vm_id
        elif vm_type in ['POST_STD_VM', 'POST_RT_VM', 'KATA_VM']:
            uos_id = vm_id - sos_vm_id
            shm_region_key = 'uos:id={},shm_regions,shm_region'.format(uos_id)
            launch_item_values[shm_region_key] = ['']
            if shm_enabled == 'y':
                for shmem_region in raw_shmem_regions:
                    if shmem_region is None or shmem_region.strip() == '':
                        continue
                    try:
                        shm_splited = shmem_region.split(',')
                        name = shm_splited[0].strip()
                        size = shm_splited[1].strip()
                        vm_id_list = [x.strip() for x in shm_splited[2].split(':')]
                        if str(vm_id) in vm_id_list:
                            launch_item_values[shm_region_key].append(','.join([name, size]))
                    except Exception as e:
                        print(e)


def set_pci_vuarts(launch_item_values, scenario_info):
    try:
        launch_item_values['uos,console_vuart'] = DM_VUART0
        vm_types = common.get_leaf_tag_map(scenario_info, 'vm_type')
        sos_vm_id = 0
        for vm_id, vm_type in vm_types.items():
            if vm_type in ['SOS_VM']:
                sos_vm_id = vm_id
        for vm in list(common.get_config_root(scenario_info)):
            if vm.tag == 'vm' and scenario_cfg_lib.VM_DB[vm_types[int(vm.attrib['id'])]]['load_type'] == 'POST_LAUNCHED_VM':
                uos_id = int(vm.attrib['id']) - sos_vm_id
                pci_vuart_key = 'uos:id={},communication_vuarts,communication_vuart'.format(uos_id)
                for elem in list(vm):
                    if elem.tag == 'communication_vuart':
                        for sub_elem in list(elem):
                            if sub_elem.tag == 'base' and sub_elem.text == 'PCI_VUART':
                                if pci_vuart_key not in launch_item_values.keys():
                                    launch_item_values[pci_vuart_key] = ['', elem.attrib['id']]
                                else:
                                    launch_item_values[pci_vuart_key].append(elem.attrib['id'])
    except:
        return


def check_shm_regions(launch_shm_regions, scenario_info):
    launch_item_values = {}
    set_shm_regions(launch_item_values, scenario_info)

    for uos_id, shm_regions in launch_shm_regions.items():
        shm_region_key = 'uos:id={},shm_regions,shm_region'.format(uos_id)
        for shm_region in shm_regions:
            if shm_region_key not in launch_item_values.keys() or shm_region not in launch_item_values[shm_region_key]:
                ERR_LIST[shm_region_key] = "shm {} should be configured in scenario setting and the size should be decimal" \
                                           "in MB and spaces should not exist.".format(shm_region)
                return


def check_console_vuart(launch_console_vuart, vuart0, scenario_info):
    vuarts = common.get_vuart_info(scenario_info)

    for uos_id, console_vuart_enable in launch_console_vuart.items():
        key = 'uos:id={},console_vuart'.format(uos_id)
        if console_vuart_enable == "Enable" and vuart0[uos_id] == "Enable":
            ERR_LIST[key] = "vuart0 and console_vuart of uos {} should not be enabled " \
                 "at the same time".format(uos_id)
            return
        if console_vuart_enable == "Enable" and int(uos_id) in vuarts.keys() \
             and 0 in vuarts[uos_id] and vuarts[uos_id][0]['base'] == "INVALID_PCI_BASE":
            ERR_LIST[key] = "console_vuart of uos {} should be enabled in scenario setting".format(uos_id)
            return


def check_communication_vuart(launch_communication_vuarts, scenario_info):
    vuarts = common.get_vuart_info(scenario_info)
    vuart1_setting = common.get_vuart_info_id(common.SCENARIO_INFO_FILE, 1)

    for uos_id, vuart_list in launch_communication_vuarts.items():
        vuart_key = 'uos:id={},communication_vuarts,communication_vuart'.format(uos_id)
        for vuart_id in vuart_list:
            if not vuart_id:
                return
            if int(vuart_id) not in vuarts[uos_id].keys():
                ERR_LIST[vuart_key] = "communication_vuart {} of uos {} should be configured" \
                     "in scenario setting.".format(vuart_id, uos_id)
                return
            if int(vuart_id) == 1 and vuarts[uos_id][1]['base'] != "INVALID_PCI_BASE":
                if uos_id in vuart1_setting.keys() and vuart1_setting[uos_id]['base'] != "INVALID_COM_BASE":
                    ERR_LIST[vuart_key] = "uos {}'s communication_vuart 1 and legacy_vuart 1 should " \
                        "not be configured at the same time.".format(uos_id)
                return

def check_enable_ptm(launch_enable_ptm, scenario_info):
    scenario_etree = lxml.etree.parse(scenario_info)
    enable_ptm_vm_list = scenario_etree.xpath("//vm[PTM = 'y']/@id")
    for uos_id, enable_ptm in launch_enable_ptm.items():
        key = 'uos:id={},enable_ptm'.format(uos_id)
        if enable_ptm == 'y' and str(uos_id) not in enable_ptm_vm_list:
            ERR_LIST[key] = "PTM of uos:{} set to 'n' in scenario xml".format(uos_id)
