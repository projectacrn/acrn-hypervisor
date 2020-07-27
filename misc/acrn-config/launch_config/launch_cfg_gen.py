# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
from launch_item import AvailablePthru, PthruSelected, VirtioDeviceSelect, AcrnDmArgs
import board_cfg_lib
import launch_cfg_lib
import com
import common

ACRN_PATH = common.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + '/misc/vm_configs/xmls/config-xmls/'


def get_launch_item_values(board_info):
    """
    Get items which capable multi select for user
    :param board_info: it is a file what contains board information for script to read from
    """
    common.BOARD_INFO_FILE = board_info
    launch_item_values = {}

    # passthrough devices
    pthru = AvailablePthru(board_info)
    pthru.get_pci_dev()
    pthru.insert_nun()

    # pre passthrough device for ui
    launch_item_values["uos,passthrough_devices,usb_xdci"] = pthru.avl["usb_xdci"]
    launch_item_values["uos,passthrough_devices,ipu"] = pthru.avl["ipu"]
    launch_item_values["uos,passthrough_devices,ipu_i2c"] = pthru.avl["ipu_i2c"]
    launch_item_values["uos,passthrough_devices,cse"] = pthru.avl["cse"]
    launch_item_values["uos,passthrough_devices,audio"] = pthru.avl["audio"]
    launch_item_values["uos,passthrough_devices,audio_codec"] = pthru.avl["audio_codec"]
    launch_item_values["uos,passthrough_devices,sd_card"] = pthru.avl["sd_card"]
    launch_item_values["uos,passthrough_devices,wifi"] = pthru.avl["wifi"]
    launch_item_values["uos,passthrough_devices,ethernet"] = pthru.avl["ethernet"]
    launch_item_values["uos,passthrough_devices,sata"] = pthru.avl["sata"]
    launch_item_values["uos,passthrough_devices,nvme"] = pthru.avl["nvme"]
    launch_item_values["uos,passthrough_devices,bluetooth"] = pthru.avl["bluetooth"]

    # acrn dm available optargs
    launch_item_values['uos,uos_type'] = launch_cfg_lib.UOS_TYPES
    launch_item_values["uos,rtos_type"] = launch_cfg_lib.RTOS_TYPE

    launch_item_values["uos,vbootloader"] = launch_cfg_lib.BOOT_TYPE
    launch_item_values['uos,vuart0'] = launch_cfg_lib.DM_VUART0
    launch_item_values['uos,poweroff_channel'] = launch_cfg_lib.PM_CHANNEL
    launch_item_values["uos,cpu_affinity"] = board_cfg_lib.get_processor_info()

    return launch_item_values


def validate_launch_setting(board_info, scenario_info, launch_info):
    """
    This is validate the data setting from scenario xml
    :param board_info: it is a file what contains board information for script to read from
    :param scenario_info: it is a file what user have already setting to
    :return: return a dictionary contain errors
    """
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

    # acrn dm arguments
    dm = AcrnDmArgs(board_info, scenario_info, launch_info)
    dm.get_args()
    dm.check_item()

    return (launch_cfg_lib.ERR_LIST, pt_sel, virtio, dm)


def ui_entry_api(board_info, scenario_info, launch_info, out=''):

    err_dic = {}
    arg_list = ['launch_cfg_gen.py', '--board', board_info, '--scenario', scenario_info, '--launch', launch_info, '--uosid', '0', '--out', out]

    err_dic = common.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)
    return err_dic


def get_names():

    names = {}

    # get uos name
    uos_types = launch_cfg_lib.get_uos_type()
    names['uos_types'] = uos_types

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


def generate_script_file(names, pt_sel, virt_io, dm, vmid, config):

    uos_type = names['uos_types'][vmid]
    board_name = names['board_name']
    scenario_name = names['scenario_name']

    header_info = "#!/bin/bash\n" +\
        "# board: {}, scenario: {}, uos: {}".format(
            board_name.upper(), scenario_name.upper(), uos_type.upper())

    print("{}".format(header_info), file=config)
    com.gen(names, pt_sel, virt_io, dm, vmid, config)
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
        err_dic['uosid err:'] = "--uosid shoudl be positive and less than total post vm count in scenario"
    if vm_th and vm_th not in post_num_list:
        err_dic['uosid err:'] = "--uosid generate the {} post vm, but this vm's config not in launch xml".format(vm_th)
    if launch_vm_count > post_vm_count:
        err_dic['xm config err:'] = "too many vms config than scenario"

    for post_num in post_num_list:
        if post_num > post_vm_count:
            err_dic['xm config err:'] = "launch xml uos id config is bigger than scenario post vm count"

    if err_dic:
        return err_dic

    # validate launch config file
    (err_dic, pt_sel, virt_io, dm) = validate_launch_setting(board_info_file, scenario_info_file, launch_info_file)
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
            output = os.path.join(output_folder + '/' + board_name, 'output/')
        else:
            output = os.path.join(ACRN_PATH + output_folder + '/' + board_name, 'output/')
    else:
        output = os.path.join(ACRN_CONFIG_DEF + board_name, 'output/')
    common.mkdir(output)

    # generate launch script
    if vm_th:
        script_name = "launch_uos_id{}.sh".format(vm_th)
        launch_script_file = output + script_name
        with open(launch_script_file, mode = 'w', newline=None, encoding='utf-8') as config:
            err_dic = generate_script_file(names, pt_sel, virt_io.dev, dm.args, vm_th, config)
            if err_dic:
                return err_dic
    else:
        for post_vm_i in post_num_list:
            script_name = "launch_uos_id{}.sh".format(post_vm_i)
            launch_script_file = output + script_name
            with open(launch_script_file, mode = 'w', newline='\n', encoding='utf-8') as config:
                err_dic = generate_script_file(names, pt_sel, virt_io.dev, dm.args, post_vm_i, config)
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
