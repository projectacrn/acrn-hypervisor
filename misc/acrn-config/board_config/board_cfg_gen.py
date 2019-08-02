# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import os
import sys
import shutil
import subprocess
import board_cfg_lib
import board_c
import pci_devices_h
import acpi_platform_h

ACRN_PATH = "../../../"
ACRN_CONFIG = ACRN_PATH + "hypervisor/arch/x86/configs/"

BOARD_NAMES = ['apl-mrb', 'apl-nuc', 'apl-up2', 'dnv-cb2', 'nuc6cayh',
               'nuc7i7dnb', 'kbl-nuc-i7', 'icl-rvp']

ACRN_DEFAULT_PLATFORM = ACRN_PATH + "hypervisor/include/arch/x86/default_acpi_info.h"
GEN_FILE = ["vm_configurations.h", "vm_configurations.c", "pt_dev.c", "pci_devices.h",
            "board.c", "_acpi_info.h"]

PY_CACHES = ["__pycache__", "board_config/__pycache__"]
BIN_LIST = ['git']


def prepare():
    """Prepare to check the environment"""
    for excute in BIN_LIST:
        res = subprocess.Popen("which {}".format(excute), shell=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, close_fds=True)

        line = res.stdout.readline().decode('ascii')

        if not line:
            board_cfg_lib.print_yel("'{}' not found, please install it!".format(excute))
            sys.exit(1)

        if excute == "git":
            res = subprocess.Popen("git tag -l", shell=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, close_fds=True)
            line = res.stdout.readline().decode("ascii")

            if "acrn" not in line:
                board_cfg_lib.print_red("Run this tool in acrn-hypervisor mainline source code!")
                sys.exit(1)

    for py_cache in PY_CACHES:
        if os.path.exists(py_cache):
            shutil.rmtree(py_cache)


def gen_patch(srcs_list, board_name):
    """Generate patch and apply to local source code
    :param srcs_list: it is a list what contains source files
    :param board_name: board name
    """
    changes = ' '.join(srcs_list)
    git_add = "git add {}".format(changes)
    subprocess.call(git_add, shell=True, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, close_fds=True)

    # commit this changes
    git_commit = 'git commit -sm "acrn-config: config board patch for {}"'.format(board_name)
    subprocess.call(git_commit, shell=True, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, close_fds=True)


def main(board_info_file):
    """This is main function to start generate source code related with board
    :param board_info_file: it is a file what contains board information for script to read from
    """
    board = ''
    config_srcs = []
    config_dirs = []

    # get board name
    board = board_cfg_lib.get_board_name(board_info_file)

    config_dirs.append(ACRN_CONFIG + board)
    if board not in BOARD_NAMES:
        for config_dir in config_dirs:
            if not os.path.exists(config_dir):
                os.makedirs(config_dir)

    config_pci = config_dirs[0] + '/' + GEN_FILE[3]
    config_board = config_dirs[0] + '/' + GEN_FILE[4]
    config_platform = config_dirs[0] + '/' + board + GEN_FILE[5]

    config_srcs.append(config_pci)
    config_srcs.append(config_board)
    config_srcs.append(config_platform)

    # generate board.c
    with open(config_board, 'w+') as config:
        board_c.generate_file(config)

    # generate pci_devices.h
    with open(config_pci, 'w+') as config:
        pci_devices_h.generate_file(config)

    # generate acpi_platform.h
    with open(config_platform, 'w+') as config:
        acpi_platform_h.generate_file(config, ACRN_DEFAULT_PLATFORM)

    # move changes to patch, and apply to the source code
    gen_patch(config_srcs, board)

    if board not in BOARD_NAMES:
        print("Config patch for NEW board {} is committed successfully!".format(board))
    else:
        print("Config patch for {} is committed successfully!".format(board))


def usage():
    """This is usage for how to use this tool"""
    print("usage= [h] --board <board_info_file>'")
    print('board_info_file, :  file name of the board info"')
    sys.exit(1)


if __name__ == '__main__':
    prepare()

    ARGS = sys.argv[1:]

    if ARGS[0] != '--board':
        usage()
        sys.exit(1)

    BOARD_INFO_FILE = ARGS[1]
    if not os.path.exists(BOARD_INFO_FILE):
        board_cfg_lib.print_red("{} is not exist!".format(BOARD_INFO_FILE))
        sys.exit(1)

    board_cfg_lib.BOARD_INFO_FILE = BOARD_INFO_FILE

    main(BOARD_INFO_FILE)
