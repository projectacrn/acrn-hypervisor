# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import os
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import board_cfg_lib
import board_c
import board_info_h
import pci_devices_h
import acpi_platform_h
import acrn_config_utilities

ACRN_PATH = acrn_config_utilities.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + "misc/config_tools/data/"

ACRN_DEFAULT_ACPI = ACRN_PATH + "hypervisor/include/arch/x86/asm/default_acpi_info.h"
GEN_FILE = ["pci_devices.h", "board.c", "platform_acpi_info.h", "misc_cfg.h",
            "board_info.h", "vbar_base.h"]


def main(args):
    """
    This is main function to start generate source code related with board
    :param args: it is a command line args for the script
    """
    err_dic = {}

    (err_dic, params) = acrn_config_utilities.get_param(args)
    if err_dic:
        return err_dic

    # check env
    err_dic = acrn_config_utilities.prepare()
    if err_dic:
        return err_dic

    acrn_config_utilities.BOARD_INFO_FILE = params['--board']
    acrn_config_utilities.SCENARIO_INFO_FILE = params['--scenario']
    acrn_config_utilities.get_vm_num(params['--scenario'])
    acrn_config_utilities.get_load_order()

    if acrn_config_utilities.VM_COUNT > acrn_config_utilities.MAX_VM_NUM:
        err_dic['vm count'] = "The number of VMs in the scenario XML file should be no greater than " \
                              "hv.CAPACITIES.MAX_VM_NUM. Its current value is {}.".format(acrn_config_utilities.MAX_VM_NUM)
        return err_dic

    # check if this is the scenario config which matched board info
    # get board name
    (err_dic, board) = acrn_config_utilities.get_board_name()
    if err_dic:
        return err_dic

    (err_dic, scenario) = acrn_config_utilities.get_scenario_name()
    if err_dic:
        return err_dic
    board_cfg_lib.BOARD_NAME = board

    output = ''
    if params['--out']:
        if os.path.isabs(params['--out']):
            output = params['--out']
        else:
            output = ACRN_PATH + params['--out']
    else:
        output = ACRN_CONFIG_DEF

    board_fix_dir = os.path.join(output, "boards/")
    scen_board_dir = os.path.join(output, "scenarios/" + scenario + "/")
    acrn_config_utilities.mkdir(board_fix_dir)
    acrn_config_utilities.mkdir(scen_board_dir)

    config_pci = board_fix_dir + GEN_FILE[0]
    config_board = board_fix_dir + GEN_FILE[1]
    config_acpi =  board_fix_dir + GEN_FILE[2]
    config_board_h =  board_fix_dir + GEN_FILE[4]

    # generate pci_devices.h
    with open(config_pci, 'w+') as config:
        pci_devices_h.generate_file(config)

    # generate board_info.h
    with open(config_board_h, 'w+') as config:
        err_dic = board_info_h.generate_file(config)
        if err_dic:
            return err_dic

    # generate board.c
    with open(config_board, 'w+') as config:
        err_dic = board_c.generate_file(config)
        if err_dic:
            return err_dic

    # generate platform_acpi_info.h
    with open(config_acpi, 'w+') as config:
        acpi_platform_h.generate_file(config, ACRN_DEFAULT_ACPI)

    if not err_dic:
        print("Board configurations for {} is generated successfully.".format(board))
    else:
        print("Board configurations for {} is generated failed.".format(board))

    return err_dic


def ui_entry_api(board_info, scenario_info, out=''):

    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info, '--out', out]

    err_dic = acrn_config_utilities.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)

    return err_dic


if __name__ == '__main__':

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            acrn_config_utilities.print_red("{}: {}".format(err_k, err_v), err=True)
    sys.exit(1 if err_dic else 0)
