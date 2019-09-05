# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import copy
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
from scenario_item import HwInfo, VmInfo
import scenario_cfg_lib
import vm_configurations_c
import vm_configurations_h
import pci_dev_c

ACRN_PATH = scenario_cfg_lib.SOURCE_ROOT_DIR
SCENARIO_PATH = ACRN_PATH + 'hypervisor/scenarios'
GEN_FILE = ["vm_configurations.h", "vm_configurations.c", "pci_dev.c"]


def get_scenario_item_values(board_info, scenario_info):
    """
    Get items which capable multi select for user
    :param board_info: it is a file what contains board information for script to read from
    """
    scenario_item_values = {}
    hw_info = HwInfo(board_info)

    # get vm count
    scenario_cfg_lib.SCENARIO_INFO_FILE = scenario_info
    scenario_cfg_lib.BOARD_INFO_FILE = board_info
    scenario_cfg_lib.VM_COUNT = scenario_cfg_lib.get_vm_num(scenario_info)

    # pre scenario
    guest_flags = copy.deepcopy(scenario_cfg_lib.GUEST_FLAG)
    guest_flags.remove('0UL')
    scenario_item_values["vm,pcpu_ids"] = hw_info.get_processor_val()
    scenario_item_values["vm,guest_flags"] = guest_flags
    scenario_item_values["vm,clos"] = hw_info.get_clos_val()
    scenario_item_values["vm,os_config,kern_type"] = scenario_cfg_lib.KERN_TYPE_LIST
    scenario_item_values.update(scenario_cfg_lib.avl_vuart_ui_select(scenario_info))

    # pre board_private
    scenario_item_values["vm,board_private,rootfs"] = scenario_cfg_lib.get_rootdev_info(board_info)
    scenario_item_values["vm,board_private,console"] = scenario_cfg_lib.get_ttys_info(board_info)

    # os config
    scenario_item_values["vm,os_config,rootfs"] = scenario_cfg_lib.get_rootdev_info(board_info)

    return scenario_item_values


def validate_scenario_setting(board_info, scenario_info):
    """
    This is validate the data setting from scenario xml
    :param board_info: it is a file what contains board information for script to read from
    :param scenario_info: it is a file what user have already setting to
    :return: return a dictionary contain errors
    """
    scenario_cfg_lib.ERR_LIST = {}
    scenario_cfg_lib.BOARD_INFO_FILE = board_info
    scenario_cfg_lib.SCENARIO_INFO_FILE = scenario_info

    vm_info = VmInfo(board_info, scenario_info)

    vm_info.get_info()

    vm_info.check_item()

    return (scenario_cfg_lib.ERR_LIST, vm_info)


def main(args):
    """
    This is main function to start generate source code related with board
    :param args: it is a command line args for the script
    """
    err_dic = {}
    config_srcs = []

    (err_dic, board_info_file, scenario_info_file) = scenario_cfg_lib.get_param(args)
    if err_dic:
        return err_dic

    scenario_cfg_lib.BOARD_INFO_FILE = board_info_file
    scenario_cfg_lib.SCENARIO_INFO_FILE = scenario_info_file

    # get scenario name
    (err_dic, scenario) = scenario_cfg_lib.get_scenario_name()
    if err_dic:
        return err_dic

    # check if this is the scenario config which matched board info
    (err_dic, status) = scenario_cfg_lib.is_config_file_match()
    if not status:
        err_dic['scenario config: Not match'] = "The board xml and scenario xml should be matched!"
        return err_dic

    vm_config_h = SCENARIO_PATH + '/' + scenario + '/' + GEN_FILE[0]
    vm_config_c = SCENARIO_PATH + '/' + scenario + '/' + GEN_FILE[1]
    pci_config_c = SCENARIO_PATH + '/' + scenario + '/' + GEN_FILE[2]

    config_srcs.append(vm_config_h)
    config_srcs.append(vm_config_c)
    if scenario == "logical_partition":
        config_srcs.append(pci_config_c)

    # parse the scenario.xml
    get_scenario_item_values(board_info_file, scenario_info_file)
    (err_dic, vm_info) = validate_scenario_setting(board_info_file, scenario_info_file)
    if err_dic:
        scenario_cfg_lib.print_red("Validate the scenario item failue", err=True)
        return err_dic

    # generate vm_configuration.h
    with open(vm_config_h, 'w') as config:
        vm_configurations_h.generate_file(scenario, vm_info, config)

    # generate vm_configuration.c
    with open(vm_config_c, 'w') as config:
        err_dic = vm_configurations_c.generate_file(scenario, vm_info, config)
        if err_dic:
            return err_dic

    # generate pci_dev.c if scenario is logical_partition
    if scenario == 'logical_partition':
        with open(pci_config_c, 'w') as config:
            pci_dev_c.generate_file(config)

    # move changes to patch, and apply to the source code
    err_dic = scenario_cfg_lib.gen_patch(config_srcs, scenario)
    if not err_dic:
        print("Config patch for {} is committed successfully!".format(scenario))
    else:
        print("Config patch for {} is failed".format(scenario))

    return err_dic


def ui_entry_api(board_info, scenario_info):
    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info]
    err_dic = scenario_cfg_lib.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)

    return err_dic


if __name__ == '__main__':

    err_dic = scenario_cfg_lib.prepare()
    if err_dic:
        for err_k, err_v in err_dic.items():
            scenario_cfg_lib.print_red("{}: {}".format(err_k, err_v), err=True)
        sys.exit(1)

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            scenario_cfg_lib.print_red("{}: {}".format(err_k, err_v), err=True)

