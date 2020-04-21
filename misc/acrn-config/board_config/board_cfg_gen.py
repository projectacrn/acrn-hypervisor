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
import common

ACRN_PATH = common.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + "hypervisor/arch/x86/configs/"

ACRN_DEFAULT_ACPI = ACRN_PATH + "hypervisor/include/arch/x86/default_acpi_info.h"
GEN_FILE = ["pci_devices.h", "board.c", "_acpi_info.h", "misc_cfg.h", ".config"]


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

    if common.VM_COUNT > common.MAX_VM_NUM:
        err_dic['vm count'] = "The vm count in config xml should be less or equal {}!".format(common.MAX_VM_NUM)
        return err_dic

    # check if this is the scenario config which matched board info
    # get board name
    (err_dic, board) = common.get_board_name()
    if err_dic:
        return err_dic
    board_cfg_lib.BOARD_NAME = board

    # check if this is the scenario config which matched board info
    (err_dic, status) = common.is_config_file_match()
    if not status:
        err_dic['board config: Not match'] = "The board xml and scenario xml should be matched"
        return err_dic

    if params['--out']:
        if os.path.isabs(params['--out']):
            board_dir = os.path.join(params['--out'], board + '/')
            config_board_kconfig = os.path.join(board_dir,  GEN_FILE[4])
        else:
            board_dir = os.path.join(ACRN_PATH + params['--out'], board + '/')
            config_board_kconfig = os.path.join(board_dir, GEN_FILE[4])
    else:
        board_dir = os.path.join(ACRN_CONFIG_DEF, board + '/')
        config_board_kconfig = os.path.join(board_dir, GEN_FILE[4])
    common.mkdir(board_dir)

    config_pci = board_dir + GEN_FILE[0]
    config_board = board_dir + GEN_FILE[1]
    config_acpi = board_dir + board + GEN_FILE[2]
    config_misc_cfg = board_dir + GEN_FILE[3]

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

    if not err_dic:
        print("Board configurations for {} is generated successfully.".format(board))
    else:
        print("Board configurations for {} is generated failed.".format(board))

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
