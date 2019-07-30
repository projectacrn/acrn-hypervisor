# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys

HV_LICENSE_FILE = '../library/hypervisor_license'
BOARD_INFO_FILE = "board_info.txt"

BIOS_INFO = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']

BASE_BOARD = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']


def open_license():
    """Get the license"""
    with open(HV_LICENSE_FILE, 'r') as f_licence:
        license_s = f_licence.read().strip()
        return license_s


HEADER_LICENSE = open_license() + "\n"

#LOGICAL_PT_PROFILE = {}
#LOGICAL_PCI_DEV = {}
#LOGICAL_PCI_LINE = {}


def print_yel(msg, warn=False):
    """Print the message with color of yellow"""
    if warn:
        print("\033[1;33mWarning\033[0m:"+msg)
    else:
        print("\033[1;33m{0}\033[0m".format(msg))


def print_red(msg, err=False):
    """Print the message with color of red"""
    if err:
        print("\033[1;31mError\033[0m:"+msg)
    else:
        print("\033[1;31m{0}\033[0m".format(msg))


def get_board_name(board_info):
    """Get board name"""
    with open(board_info, 'rt') as f_board:
        line = f_board.readline()
        if not "board=" in line:
            print_red("acrn-config board info xml was corrupted!")
            sys.exit(1)

        board = line.split('"')[1].strip('"')
        return board


def get_info(board_info, msg_s, msg_e):
    """Get information which specify by argument"""
    info_start = False
    info_end = False
    info_lines = []
    num = len(msg_s.split())

    try:
        with open(board_info, 'rt') as f_board:
            while True:

                line = f_board.readline()
                if not line:
                    break

                if " ".join(line.split()[0:num]) == msg_s:
                    info_start = True
                    info_end = False
                    continue

                if " ".join(line.split()[0:num]) == msg_e:
                    info_start = False
                    info_end = True
                    continue

                if info_start and not info_end:
                    info_lines.append(line)
                    continue

                if not info_start and info_end:
                    return info_lines

    except IOError as err:
        print_red(str(err), err=True)
        sys.exit(1)


def handle_bios_info(config):
    """Handle bios information"""
    bios_lines = get_info(BOARD_INFO_FILE, "<BIOS_INFO>", "</BIOS_INFO>")
    board_lines = get_info(BOARD_INFO_FILE, "<BASE_BOARD_INFO>", "</BASE_BOARD_INFO>")
    print("/*", file=config)

    if not bios_lines or not board_lines:
        print(" * DMI info is not found", file=config)
    else:
        i_cnt = 0
        bios_board = BIOS_INFO + BASE_BOARD

        # remove the same value and keep origin sort
        bios_board_info = list(set(bios_board))
        bios_board_info.sort(key=bios_board.index)

        bios_board_lines = bios_lines + board_lines
        bios_info_len = len(bios_lines)
        for line in bios_board_lines:
            if i_cnt == bios_info_len:
                print(" *", file=config)

            i_cnt += 1

            for mics_info in bios_board_info:
                if mics_info == " ".join(line.split()[0:1]) or mics_info == \
                        " ".join(line.split()[0:2]) or mics_info == " ".join(line.split()[0:3]):
                    print(" * {0}".format(line.strip()), file=config)

    print(" */", file=config)
