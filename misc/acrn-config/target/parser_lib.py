# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import subprocess

BIOS_INFO_KEY = ['BIOS Information', 'Vendor:', 'Version:', 'Release Date:', 'BIOS Revision:']

BASE_BOARD_KEY = ['Base Board Information', 'Manufacturer:', 'Product Name:', 'Version:']


def check_dmi():
    """Check if this tool run on native os"""
    return os.path.exists("/sys/firmware/dmi")


def print_yel(msg, warn=False):
    """Print the msg wiht color of yellow"""
    if warn:
        print("\033[1;33mWarning\033[0m:"+msg)
    else:
        print("\033[1;33m{0}\033[0m".format(msg))


def print_red(msg, err=False):
    """Print the msg wiht color of red"""
    if err:
        print("\033[1;31mError\033[0m:"+msg)
    else:
        print("\033[1;31m{0}\033[0m".format(msg))


def decode_stdout(resource):
    """Decode stdout"""
    line = resource.stdout.readline().decode('ascii')
    return line


def handle_hw_info(line, hw_info):
    """handle the hardware information"""
    for board_line in hw_info:
        if board_line == " ".join(line.split()[0:1]) or \
                board_line == " ".join(line.split()[0:2]) or \
                board_line == " ".join(line.split()[0:3]):
            return True
    return False


def handle_pci_dev(line):
    """Handle if it is pci line"""
    if "Region" in line and "Memory at" in line:
        return True

    if line != '\n':
        if line.split()[0][2:3] == ':' and line.split()[0][5:6] == '.':
            return True

    return False


def cmd_excute(cmd):
    """Excute cmd and retrun raw"""
    res = subprocess.Popen(cmd, shell=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)

    return res


def dump_excute(cmd, desc, config):
    """Execute cmd and get information"""
    val_dmi = check_dmi()
    print("\t<{0}>".format(desc), file=config)

    if not val_dmi and "dmidecode" in cmd:
        print("\t\t</{0}>".format(desc), file=config)
        return

    res = cmd_excute(cmd)
    while True:
        line = res.stdout.readline().decode('ascii')

        if not line:
            break

        if desc == "PCI_DEVICE":
            if "prog-if" in line:
                line = line.rsplit('(', 1)[0] + '\n'
            ret = handle_pci_dev(line)
            if not ret:
                continue

        if desc == "BIOS_INFO":
            ret = handle_hw_info(line, BIOS_INFO_KEY)
            if not ret:
                continue

        if desc == "BASE_BOARD_INFO":
            ret = handle_hw_info(line, BASE_BOARD_KEY)
            if not ret:
                continue

        print("\t{}".format(line.strip()), file=config)

    print("\t</{0}>".format(desc), file=config)
