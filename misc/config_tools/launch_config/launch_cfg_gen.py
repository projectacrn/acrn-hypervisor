# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import logging
import os
import re
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
from launch_item import AvailablePthru, PthruSelected, VirtioDeviceSelect, AcrnDmArgs, SriovDeviceInput
import board_cfg_lib
import launch_cfg_lib
import com
import common

ACRN_PATH = common.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + '/misc/config_tools/data/'


def get_launch_item_values(board_info, scenario_info=None):
    """
    Get items which capable multi select for user
    :param board_info: it is a file what contains board information for script to read from
    :param sceanrio_info: it is a file what contains scenario information for script to read from
    """
    common.BOARD_INFO_FILE = board_info
    launch_item_values = {}

    # passthrough devices
    pthru = AvailablePthru(board_info)
    pthru.get_pci_dev()
    pthru.insert_nun()

    # pre passthrough device for ui
    launch_item_values["user_vm,passthrough_devices,usb_xdci"] = pthru.avl["usb_xdci"]
    launch_item_values["user_vm,passthrough_devices,gpu"] = pthru.avl["gpu"]
    launch_item_values["user_vm,passthrough_devices,ipu"] = pthru.avl["ipu"]
    launch_item_values["user_vm,passthrough_devices,ipu_i2c"] = pthru.avl["ipu_i2c"]
    launch_item_values["user_vm,passthrough_devices,cse"] = pthru.avl["cse"]
    launch_item_values["user_vm,passthrough_devices,audio"] = pthru.avl["audio"]
    launch_item_values["user_vm,passthrough_devices,audio_codec"] = pthru.avl["audio_codec"]
    launch_item_values["user_vm,passthrough_devices,sd_card"] = pthru.avl["sd_card"]
    launch_item_values["user_vm,passthrough_devices,wifi"] = pthru.avl["wifi"]
    launch_item_values["user_vm,passthrough_devices,ethernet"] = pthru.avl["ethernet"]
    launch_item_values["user_vm,passthrough_devices,sata"] = pthru.avl["sata"]
    launch_item_values["user_vm,passthrough_devices,nvme"] = pthru.avl["nvme"]
    launch_item_values["user_vm,passthrough_devices,bluetooth"] = pthru.avl["bluetooth"]

    # acrn dm available optargs
    launch_item_values['user_vm,user_vm_type'] = launch_cfg_lib.USER_VM_TYPES
    launch_item_values["user_vm,rtos_type"] = launch_cfg_lib.RTOS_TYPE

    launch_item_values["user_vm,vbootloader"] = launch_cfg_lib.BOOT_TYPE
    launch_item_values['user_vm,vuart0'] = launch_cfg_lib.DM_VUART0
    launch_item_values['user_vm,poweroff_channel'] = launch_cfg_lib.PM_CHANNEL
    launch_item_values["user_vm,cpu_affinity"] = board_cfg_lib.get_processor_info()
    launch_item_values['user_vm,enable_ptm'] = launch_cfg_lib.y_n
    launch_item_values['user_vm,allow_trigger_s5'] = launch_cfg_lib.y_n
    launch_cfg_lib.set_shm_regions(launch_item_values, scenario_info)
    launch_cfg_lib.set_pci_vuarts(launch_item_values, scenario_info)

    return launch_item_values


def validate_launch_setting(board_info, scenario_info, launch_info):
    """
    This is validate the data setting from scenario xml
    :param board_info: it is a file what contains board information for script to read from
    :param scenario_info: it is a file what user have already setting to
    :return: return a dictionary contain errors
    """
    common.SCENARIO_INFO_FILE = scenario_info
    common.get_vm_types()

    launch_cfg_lib.ERR_LIST = {}
    common.BOARD_INFO_FILE = board_info
    common.SCENARIO_INFO_FILE = scenario_info
    common.LAUNCH_INFO_FILE = launch_info

    # init available pt devices and get selected pt devices
    pt_avl = AvailablePthru(board_info)
    pt_sel = PthruSelected(launch_info, pt_avl.bdf_desc_map, pt_avl.bdf_vpid_map)
    pt_sel.get_bdf()
    pt_sel.get_vpid()
    pt_sel.get_slot()
    pt_sel.check_item()

    # virt-io devices
    virtio = VirtioDeviceSelect(launch_info)
    virtio.get_virtio()
    virtio.check_virtio()

    sriov = SriovDeviceInput(launch_info)
    sriov.get_sriov()
    sriov.check_sriov(pt_sel)

    # acrn dm arguments
    dm = AcrnDmArgs(board_info, scenario_info, launch_info)
    dm.get_args()
    dm.check_item()

    return (launch_cfg_lib.ERR_LIST, pt_sel, virtio, dm, sriov)


def ui_entry_api(board_info, scenario_info, launch_info, out=''):

    err_dic = {}
    arg_list = ['launch_cfg_gen.py', '--board', board_info, '--scenario', scenario_info, '--launch', launch_info, '--user_vmid', '0', '--out', out]

    err_dic = common.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)
    launch_cfg_lib.reset_pt_slot()
    return err_dic


def get_names():

    names = {}

    # get User Vm name
    user_vm_types = launch_cfg_lib.get_user_vm_type()
    names['user_vm_types'] = user_vm_types

    # get user_vm name
    user_vm_names = launch_cfg_lib.get_user_vm_names()
    names['user_vm_names'] = user_vm_names

    # get board name
    (err_dic, board_name) = common.get_board_name()
    if err_dic:
        return (err_dic, names)
    names['board_name'] = board_name

    # get scenario name
    (err_dic, scenario_name) = common.get_scenario_name()
    if err_dic:
        return (err_dic, names)
    names['scenario_name'] = scenario_name

    return (err_dic, names)


def generate_script_file(names, pt_sel, virt_io, dm, sriov, vmid, config):

    user_vm_type = names['user_vm_types'][vmid]
    board_name = names['board_name']
    scenario_name = names['scenario_name']

    header_info = "#!/bin/bash\n" +\
        "# board: {}, scenario: {}, user_vm: {}".format(
            board_name.upper(), scenario_name.upper(), user_vm_type.upper())

    print("{}".format(header_info), file=config)
    com.gen(names, pt_sel, virt_io, dm, sriov, vmid, config)
    if launch_cfg_lib.ERR_LIST:
        return launch_cfg_lib.ERR_LIST


def main(args):
    """
    This is main function to start generate launch script
    :param args: it is a command line args for the script
    """
    # get parameters
    (err_dic, board_info_file, scenario_info_file, launch_info_file, vm_th, output_folder) = launch_cfg_lib.get_param(args)
    if err_dic:
        return err_dic

    # check env
    err_dic = common.prepare()
    if err_dic:
        return err_dic

    # vm_th =[0..post_vm_max]
    # 0: generate all launch script for all post vm launch script
    # 1: generate launch script for 1st post vm launch script
    # 2: generate launch script for 2nd post vm launch script

    common.BOARD_INFO_FILE = board_info_file
    common.SCENARIO_INFO_FILE = scenario_info_file
    common.LAUNCH_INFO_FILE = launch_info_file
    common.get_vm_types()

    # get post vm dic
    post_num_list = launch_cfg_lib.get_post_num_list()

    # get toatl post vm number and total vm in launch config file
    (launch_vm_count, post_vm_count) = launch_cfg_lib.get_post_vm_cnt()
    if vm_th < 0 or vm_th > post_vm_count:
        err_dic['user_vmid err:'] = "--user_vmid shoudl be positive and less than total post vm count in scenario"
    if vm_th and vm_th not in post_num_list:
        err_dic['user_vmid err:'] = "--user_vmid generate the {} post vm, but this vm's config not in launch xml".format(vm_th)

    # validate vm_names
    scenario_names = common.get_leaf_tag_map(scenario_info_file, "name").values()
    for user_vm_id, vm_name in launch_cfg_lib.get_user_vm_names().items():
        if not re.match(r"^\S{1,15}$", vm_name):
            err_name = 'user_vm id="{}" name error:'.format(user_vm_id)
            err_dic[err_name] = 'vm_name only allowed 1-15 characters with letters, numbers & symbols ' \
                                '(not include space)'
        if vm_name not in scenario_names:
            logging.warning(
                'user_vm id="{}"\'s vm_name ({}) not found in scenario file, set it to dynamic vm.'.format(
                    user_vm_id, vm_name
                )
            )

    if err_dic:
        return err_dic

    # validate launch config file
    (err_dic, pt_sel, virt_io, dm, sriov) = validate_launch_setting(board_info_file, scenario_info_file, launch_info_file)
    if err_dic:
        return err_dic

    # check if this is the scenario config which matched board info
    (err_dic, status) = launch_cfg_lib.is_config_file_match()
    if not status:
        return err_dic

    (err_dic, names) = get_names()
    if err_dic:
        return err_dic

    # create output directory
    board_name = names['board_name']
    if output_folder:
        if os.path.isabs(output_folder):
            output = os.path.join(output_folder, board_name, 'output')
        else:
            output = os.path.join(ACRN_PATH, output_folder, board_name, 'output')
    else:
        output = os.path.join(ACRN_CONFIG_DEF, board_name, 'output')
    output = os.path.abspath(output)
    common.mkdir(output)

    # generate launch script
    if vm_th:
        script_name = "launch_user_vm_id{}.sh".format(vm_th)
        launch_script_file = os.path.join(output, script_name)
        with open(launch_script_file, mode = 'w', newline=None, encoding='utf-8') as config:
            err_dic = generate_script_file(names, pt_sel, virt_io.dev, dm.args, sriov.dev, vm_th, config)
            if err_dic:
                return err_dic
    else:
        for post_vm_i in post_num_list:
            script_name = "launch_user_vm_id{}.sh".format(post_vm_i)
            launch_script_file = os.path.join(output, script_name)
            with open(launch_script_file, mode = 'w', newline='\n', encoding='utf-8') as config:
                err_dic = generate_script_file(names, pt_sel, virt_io.dev, dm.args, sriov.dev, post_vm_i, config)
                if err_dic:
                    return err_dic

    if not err_dic:
        print("Launch files in {} is generated successfully!".format(output))
    else:
        print("Launch files generate failed".format(output))

    return err_dic


if __name__ == '__main__':

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            common.print_red("{}: {}".format(err_k, err_v), err=True)
