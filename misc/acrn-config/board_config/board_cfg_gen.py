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
import ve820_c
import new_board_kconfig

ACRN_PATH = board_cfg_lib.SOURCE_ROOT_DIR
ACRN_CONFIG = ACRN_PATH + "hypervisor/arch/x86/configs/"

ACRN_DEFAULT_PLATFORM = ACRN_PATH + "hypervisor/include/arch/x86/default_acpi_info.h"
GEN_FILE = ["pci_devices.h", "board.c", "_acpi_info.h", "misc_cfg.h", "ve820.c", ".config"]


def need_gen_new_board_config(board_name):

    # 1. if it is old board, they are already have the $(board_name).config, return and no need to generate it.

    if board_name in board_cfg_lib.BOARD_NAMES:
        return False
    else:
        return True


def main(args):
    """
    This is main function to start generate source code related with board
    :param args: it is a command line args for the script
    """
    err_dic = {}
    config_srcs = []
    config_dirs = []

    (err_dic, board_info_file, scenario_info_file, enable_commit) = board_cfg_lib.get_param(args)
    if err_dic:
        return err_dic

    # check env
    err_dic = board_cfg_lib.prepare(enable_commit)
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

    config_dirs.append(ACRN_CONFIG + board)
    if board not in board_cfg_lib.BOARD_NAMES:
        for config_dir in config_dirs:
            if not os.path.exists(config_dir):
                os.makedirs(config_dir)

    config_pci = config_dirs[0] + '/' + GEN_FILE[0]
    config_board = config_dirs[0] + '/' + GEN_FILE[1]
    config_platform = config_dirs[0] + '/' + board + GEN_FILE[2]
    config_misc_cfg = config_dirs[0] + '/' + GEN_FILE[3]
    config_ve820 = config_dirs[0] + '/' + GEN_FILE[4]
    config_board_kconfig = ACRN_CONFIG + board + GEN_FILE[5]

    config_srcs.append(config_pci)
    config_srcs.append(config_board)
    config_srcs.append(config_platform)
    config_srcs.append(config_misc_cfg)
    config_srcs.append(config_ve820)
    config_srcs.append(config_board_kconfig)

    # generate board.c
    with open(config_board, 'w+') as config:
        err_dic = board_c.generate_file(config)
        if err_dic:
            return err_dic

    # generate pci_devices.h
    with open(config_pci, 'w+') as config:
        pci_devices_h.generate_file(config)

    # generate acpi_platform.h
    with open(config_platform, 'w+') as config:
        acpi_platform_h.generate_file(config, ACRN_DEFAULT_PLATFORM)

    # generate acpi_platform.h
    with open(config_ve820, 'w+') as config:
        err_dic = ve820_c.generate_file(config)
        if err_dic:
            return err_dic

    # generate acpi_platform.h
    with open(config_misc_cfg, 'w+') as config:
        err_dic = misc_cfg_h.generate_file(config)
        if err_dic:
            return err_dic

    # generate new board_name.config
    if need_gen_new_board_config(board):
        with open(config_board_kconfig, 'w+') as config:
            err_dic = new_board_kconfig.generate_file(config)
            if err_dic:
                return err_dic

    config_str = 'Config files'
    gen_str = 'generated'
    # move changes to patch, and apply to the source code
    if enable_commit:
        err_dic = board_cfg_lib.gen_patch(config_srcs, board)
        config_str = 'Config patch'
        gen_str = 'committed'

    if board not in board_cfg_lib.BOARD_NAMES and not err_dic:
        print("{} for NEW board {} is {} successfully!".format(config_str, board, gen_str))
    elif not err_dic:
        print("{} for {} is {} successfully!".format(config_str, board, gen_str))
    else:
        print("{} for {} is failed".format(config_str, board))

    return err_dic


def ui_entry_api(board_info,scenario_info, enable_commit=False):

    git_env_check = False
    arg_list = ['board_cfg_gen.py', '--board', board_info, '--scenario', scenario_info]
    if enable_commit:
        arg_list.append('--enable_commit')
        git_env_check = True

    err_dic = board_cfg_lib.prepare(git_env_check)
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
