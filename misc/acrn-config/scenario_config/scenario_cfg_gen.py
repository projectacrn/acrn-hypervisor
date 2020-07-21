# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import copy
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'hv_config'))
from scenario_item import HwInfo, VmInfo
import board_cfg_lib
import scenario_cfg_lib
import vm_configurations_c
import vm_configurations_h
import pci_dev_c
import common
import hv_cfg_lib
import board_defconfig
from hv_item import HvInfo

ACRN_PATH = common.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + 'misc/vm_configs/scenarios/'
GEN_FILE = ["vm_configurations.h", "vm_configurations.c", "pci_dev.c", ".config"]


def get_scenario_item_values(board_info, scenario_info):
    """
    Get items which capable multi select for user
    :param board_info: it is a file what contains board information for script to read from
    """
    hv_cfg_lib.ERR_LIST = {}
    scenario_item_values = {}
    hw_info = HwInfo(board_info)
    hv_info = HvInfo(scenario_info)

    # get vm count
    common.BOARD_INFO_FILE = board_info
    common.SCENARIO_INFO_FILE = scenario_info
    common.get_vm_num(scenario_info)
    common.get_vm_types()

    # pre scenario
    guest_flags = copy.deepcopy(common.GUEST_FLAG)
    guest_flags.remove('0UL')
    scenario_item_values['vm,vm_type'] = scenario_cfg_lib.LOAD_VM_TYPE
    scenario_item_values["vm,cpu_affinity"] = hw_info.get_processor_val()
    scenario_item_values["vm,guest_flags"] = guest_flags
    scenario_item_values["vm,clos,vcpu_clos"] = hw_info.get_clos_val()
    scenario_item_values["vm,pci_devs"] = scenario_cfg_lib.avl_pci_devs()
    scenario_item_values["vm,os_config,kern_type"] = scenario_cfg_lib.KERN_TYPE_LIST
    scenario_item_values.update(scenario_cfg_lib.avl_vuart_ui_select(scenario_info))

    # pre board_private
    (scenario_item_values["vm,board_private,rootfs"], num) = board_cfg_lib.get_rootfs(board_info)

    scenario_item_values["hv,DEBUG_OPTIONS,RELEASE"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,DEBUG_OPTIONS,NPK_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,MEM_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,CONSOLE_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,SERIAL_CONSOLE"] = board_cfg_lib.get_native_ttys_info(board_info)
    scenario_item_values["hv,DEBUG_OPTIONS,LOG_DESTINATION"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_DESTINATION_BITMAP")

    scenario_item_values["hv,CAPACITIES,MAX_IOAPIC_NUM"] = hv_cfg_lib.get_select_range("CAPACITIES", "IOAPIC_NUM")

    scenario_item_values["hv,FEATURES,MULTIBOOT2"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,RDT,RDT_ENABLED"] = board_cfg_lib.get_rdt_select_opt()
    scenario_item_values["hv,FEATURES,RDT,CDP_ENABLED"] = board_cfg_lib.get_rdt_select_opt()
    scenario_item_values["hv,FEATURES,SCHEDULER"] = hv_cfg_lib.SCHEDULER_TYPE
    scenario_item_values["hv,FEATURES,RELOC"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,HYPERV_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,ACPI_PARSE_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,L1D_VMENTRY_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,MCE_ON_PSC_DISABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,IOMMU_ENFORCE_SNP"] = hv_cfg_lib.N_Y

    scenario_cfg_lib.ERR_LIST.update(hv_cfg_lib.ERR_LIST)
    return scenario_item_values


def validate_scenario_setting(board_info, scenario_info):
    """
    This is validate the data setting from scenario xml
    :param board_info: it is a file what contains board information for script to read from
    :param scenario_info: it is a file what user have already setting to
    :return: return a dictionary contain errors
    """
    hv_cfg_lib.ERR_LIST = {}
    scenario_cfg_lib.ERR_LIST = {}
    common.BOARD_INFO_FILE = board_info
    common.SCENARIO_INFO_FILE = scenario_info

    scenario_info_items = {}
    vm_info = VmInfo(board_info, scenario_info)
    vm_info.get_info()
    vm_info.check_item()

    hv_info = HvInfo(scenario_info)
    hv_info.get_info()
    hv_info.check_item()

    scenario_info_items['vm'] = vm_info
    scenario_info_items['hv'] = hv_info

    scenario_cfg_lib.ERR_LIST.update(hv_cfg_lib.ERR_LIST)
    return (scenario_cfg_lib.ERR_LIST, scenario_info_items)


def main(args):
    """
    This is main function to start generate source code related with board
    :param args: it is a command line args for the script
    """
    err_dic = {}

    (err_dic, params) = common.get_param(args)
    if err_dic:
        return err_dic

    # check env
    err_dic = common.prepare()
    if err_dic:
        return err_dic

    common.BOARD_INFO_FILE = params['--board']
    common.SCENARIO_INFO_FILE = params['--scenario']
    common.get_vm_num(params['--scenario'])
    common.get_vm_types()

    # get board name
    (err_dic, board_name) = common.get_board_name()

    # get scenario name
    (err_dic, scenario) = common.get_scenario_name()
    if err_dic:
        return err_dic

    if common.VM_COUNT > common.MAX_VM_NUM:
        err_dic['vm count'] = "The vm count in config xml should be less or equal {}!".format(common.MAX_VM_NUM)
        return err_dic

    # check if this is the scenario config which matched board info
    (err_dic, status) = common.is_config_file_match()
    if not status:
        err_dic['scenario config'] = "The board xml and scenario xml should be matched!"
        return err_dic

    if params['--out']:
        if os.path.isabs(params['--out']):
            scen_output = params['--out'] + "/scenarios/" + scenario + "/"
        else:
            scen_output = ACRN_PATH + params['--out'] + "/scenarios/" + scenario + "/"
    else:
        scen_output = ACRN_CONFIG_DEF + "/scenarios/" + scenario + "/"

    scen_board = scen_output + board_name + "/"
    common.mkdir(scen_board)
    common.mkdir(scen_output)

    vm_config_h  = scen_output + GEN_FILE[0]
    vm_config_c  = scen_output + GEN_FILE[1]
    pci_config_c = scen_board + GEN_FILE[2]
    config_hv = scen_board + board_name + GEN_FILE[3]

    # parse the scenario.xml
    get_scenario_item_values(params['--board'], params['--scenario'])
    (err_dic, scenario_items) = validate_scenario_setting(params['--board'], params['--scenario'])
    if err_dic:
        common.print_red("Validate the scenario item failure", err=True)
        return err_dic

    # generate board defconfig
    with open(config_hv, 'w+') as config:
        err_dic = board_defconfig.generate_file(scenario_items['hv'], config)
        if err_dic:
            return err_dic

    # generate vm_configuration.h
    with open(vm_config_h, 'w') as config:
        vm_configurations_h.generate_file(scenario_items, config)

    # generate vm_configuration.c
    with open(vm_config_c, 'w') as config:
        err_dic = vm_configurations_c.generate_file(scenario_items, config)
        if err_dic:
            return err_dic

    # generate pci_dev.c
    with open(pci_config_c, 'w') as config:
        pci_dev_c.generate_file(scenario_items['vm'], config)

    if not err_dic:
        print("Scenario configurations for {} is generated successfully.".format(scenario))
    else:
        print("Scenario configurations for {} is generated failed.".format(scenario))

    return err_dic


def ui_entry_api(board_info, scenario_info, out=''):

    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info, '--out', out]

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

