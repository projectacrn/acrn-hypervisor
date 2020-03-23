# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import copy
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
from scenario_item import HwInfo, VmInfo
import board_cfg_lib
import scenario_cfg_lib
import vm_configurations_c
import vm_configurations_h
import pci_dev_c
import common

ACRN_PATH = common.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + 'hypervisor/scenarios/'
GEN_FILE = ["vm_configurations.h", "vm_configurations.c", "pci_dev.c"]


def get_scenario_item_values(board_info, scenario_info):
    """
    Get items which capable multi select for user
    :param board_info: it is a file what contains board information for script to read from
    """
    scenario_item_values = {}
    hw_info = HwInfo(board_info)

    # get vm count
    common.BOARD_INFO_FILE = board_info
    common.SCENARIO_INFO_FILE = scenario_info
    common.get_vm_num(scenario_info)

    # pre scenario
    guest_flags = copy.deepcopy(scenario_cfg_lib.GUEST_FLAG)
    guest_flags.remove('0UL')
    scenario_item_values["vm,vcpu_affinity"] = hw_info.get_processor_val()
    scenario_item_values["vm,guest_flags"] = guest_flags
    scenario_item_values["vm,clos"] = hw_info.get_clos_val()
    scenario_item_values["vm,severity"] = scenario_cfg_lib.VM_SEVERITY
    scenario_item_values["vm,os_config,kern_type"] = scenario_cfg_lib.KERN_TYPE_LIST
    scenario_item_values.update(scenario_cfg_lib.avl_vuart_ui_select(scenario_info))

    # pre board_private
    scenario_item_values["vm,board_private,rootfs"] = board_cfg_lib.get_rootfs(board_info)
    scenario_item_values["vm,board_private,console"] = board_cfg_lib.get_ttys_info(board_info)

    # os config
    scenario_item_values["vm,os_config,rootfs"] = board_cfg_lib.get_rootfs(board_info)

    return scenario_item_values


def validate_scenario_setting(board_info, scenario_info):
    """
    This is validate the data setting from scenario xml
    :param board_info: it is a file what contains board information for script to read from
    :param scenario_info: it is a file what user have already setting to
    :return: return a dictionary contain errors
    """
    scenario_cfg_lib.ERR_LIST = {}
    common.BOARD_INFO_FILE = board_info
    common.SCENARIO_INFO_FILE = scenario_info

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

    (err_dic, board_info_file, scenario_info_file, output_folder) = common.get_param(args)
    if err_dic:
        return err_dic

    if output_folder:
        common.ACRN_CONFIG_TARGET = os.path.abspath(output_folder) + '/'

    # check env
    err_dic = common.prepare()
    if err_dic:
        return err_dic

    common.BOARD_INFO_FILE = board_info_file
    common.SCENARIO_INFO_FILE = scenario_info_file

    # get scenario name
    (err_dic, scenario) = common.get_scenario_name()
    if err_dic:
        return err_dic

    # check if this is the scenario config which matched board info
    (err_dic, status) = common.is_config_file_match()
    if not status:
        err_dic['scenario config: Not match'] = "The board xml and scenario xml should be matched!"
        return err_dic

    if common.ACRN_CONFIG_TARGET:
        scenario_dir = common.ACRN_CONFIG_TARGET + scenario + '/'
    else:
        scenario_dir = ACRN_CONFIG_DEF + scenario + '/'
    common.mkdir(scenario_dir)

    vm_config_h = scenario_dir + GEN_FILE[0]
    vm_config_c = scenario_dir + GEN_FILE[1]
    pci_config_c = scenario_dir + GEN_FILE[2]

    # parse the scenario.xml
    get_scenario_item_values(board_info_file, scenario_info_file)
    (err_dic, vm_info) = validate_scenario_setting(board_info_file, scenario_info_file)
    if err_dic:
        common.print_red("Validate the scenario item failue", err=True)
        return err_dic

    # get kata vm count
    if scenario != "logical_partition":
        scenario_cfg_lib.KATA_VM_COUNT = common.VM_COUNT - scenario_cfg_lib.DEFAULT_VM_COUNT[scenario]
        if scenario_cfg_lib.KATA_VM_COUNT > 1:
            err_dic['scenario config: kata vm count err'] = "Only one kata vm is supported!"
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

    if not err_dic:
        print("Scenario configurations for {} is generated successfully.".format(scenario))
    else:
        print("Scenario configurations for {} is generated failed.".format(scenario))

    return err_dic


def ui_entry_api(board_info, scenario_info):

    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info]

    err_dic = common.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)

    return err_dic


if __name__ == '__main__':

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            common.print_red("{}: {}".format(err_k, err_v), err=True)

