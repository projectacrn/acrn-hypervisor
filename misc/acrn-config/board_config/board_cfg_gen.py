# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import os
import sys
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import board_cfg_lib
import board_c
import pci_devices_h
import acpi_platform_h
import misc_cfg_h
import new_board_kconfig

ACRN_PATH = board_cfg_lib.SOURCE_ROOT_DIR
ACRN_CONFIG_TARGET = ACRN_PATH + "hypervisor/arch/x86/configs/"

ACRN_DEFAULT_ACPI = ACRN_PATH + "hypervisor/include/arch/x86/default_acpi_info.h"
GEN_FILE = ["pci_devices.h", "board.c", "_acpi_info.h", "misc_cfg.h", ".config"]


def main(args):
    """
    This is main function to start generate source code related with board
    :param args: it is a command line args for the script
    """
    global ACRN_CONFIG_TARGET
    err_dic = {}

    (err_dic, board_info_file, scenario_info_file, output_folder) = board_cfg_lib.get_param(args)
    if err_dic:
        return err_dic

    if output_folder:
        ACRN_CONFIG_TARGET = os.path.abspath(output_folder) + '/'

    # check env
    err_dic = board_cfg_lib.prepare()
    if err_dic:
        return err_dic

    board_cfg_lib.BOARD_INFO_FILE = board_info_file
    board_cfg_lib.SCENARIO_INFO_FILE = scenario_info_file
    board_cfg_lib.get_vm_count(scenario_info_file)

    # get board name
    (err_dic, board) = board_cfg_lib.get_board_name()
    if err_dic:
        return err_dic
    board_cfg_lib.BOARD_NAME = board

    # check if this is the scenario config which matched board info
    (err_dic, status) = board_cfg_lib.is_config_file_match()
    if not status:
        err_dic['board config: Not match'] = "The board xml and scenario xml should be matched"
        return err_dic

    board_dir = ACRN_CONFIG_TARGET + board + '/'
    board_cfg_lib.mkdir(board_dir)

    config_pci = board_dir + GEN_FILE[0]
    config_board = board_dir + GEN_FILE[1]
    config_acpi = board_dir + board + GEN_FILE[2]
    config_misc_cfg = board_dir + GEN_FILE[3]
    config_board_kconfig = ACRN_CONFIG_TARGET + board + GEN_FILE[4]

    # generate board.c
    with open(config_board, 'w+') as config:
        err_dic = board_c.generate_file(config)
        if err_dic:
            return err_dic

    # generate pci_devices.h
    with open(config_pci, 'w+') as config:
        pci_devices_h.generate_file(config)

    # generate ($board)_acpi_info.h
    with open(config_acpi, 'w+') as config:
        acpi_platform_h.generate_file(config, ACRN_DEFAULT_ACPI)

    # generate misc_cfg.h
    with open(config_misc_cfg, 'w+') as config:
        err_dic = misc_cfg_h.generate_file(config)
        if err_dic:
            return err_dic

    # generate ($board).config
    with open(config_board_kconfig, 'w+') as config:
        err_dic = new_board_kconfig.generate_file(config)
        if err_dic:
            return err_dic

    if not err_dic:
        print("Board configurations for {} is generated successfully.".format(board))
    else:
        print("Board configurations for {} is generated failed.".format(board))

    return err_dic


def ui_entry_api(board_info, scenario_info):

    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info]

    err_dic = board_cfg_lib.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)

    return err_dic


if __name__ == '__main__':

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            board_cfg_lib.print_red("{}: {}".format(err_k, err_v), err=True)
