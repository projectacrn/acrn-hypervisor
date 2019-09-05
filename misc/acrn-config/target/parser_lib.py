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
    """Output the message with the color of yellow
    :param msg: the stings which will be output to STDOUT
    :param warn: the condition if needs to be output the color of yellow with 'Warning'
    """
    if warn:
        print("\033[1;33mWarning\033[0m:"+msg)
    else:
        print("\033[1;33m{0}\033[0m".format(msg))


def print_red(msg, err=False):
    """Output the messag with the color of red
    :param msg: the stings which will be output to STDOUT
    :param err: the condition if needs to be output the color of red with 'Error'
    """
    if err:
        print("\033[1;31mError\033[0m:"+msg)
    else:
        print("\033[1;31m{0}\033[0m".format(msg))


def decode_stdout(resource):
    """Decode the information and return one line of the decoded information
    :param resource: it contains information produced by subprocess.Popen method
    """
    line = resource.stdout.readline().decode('ascii')
    return line


def handle_hw_info(line, hw_info):
    """Handle the hardware information
    :param line: one line of information which had decoded to 'ASCII'
    :param hw_info: the list which contains key strings what can describe bios/board
    """
    for board_line in hw_info:
        if board_line == " ".join(line.split()[0:1]) or \
                board_line == " ".join(line.split()[0:2]) or \
                board_line == " ".join(line.split()[0:3]):
            return True
    return False


def handle_pci_dev(line):
    """Handle if it match PCI device information pattern
    :param line: one line of information which had decoded to 'ASCII'
    """
    if "Region" in line and "Memory at" in line:
        return True

    if line != '\n':
        if line.split()[0][2:3] == ':' and line.split()[0][5:6] == '.':
            return True

    return False


def cmd_execute(cmd):
    """Excute cmd and retrun raw information
    :param cmd: command what can be executed in shell
    """
    res = subprocess.Popen(cmd, shell=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)

    return res


def handle_root_dev(line):
    """Handle if it match root device information pattern
    :param line: one line of information which had decoded to 'ASCII'
    """
    for root_type in line.split():
        if "ext4" in root_type or "ext3" in root_type:
            return True

    return False


def dump_execute(cmd, desc, config):
    """Execute cmd and get information
    :param cmd: command what can be executed in shell
    :param desc: the string indicated what class information store to board.xml
    :param config: file pointer that opened for writing board information
    """
    val_dmi = check_dmi()
    print("\t<{0}>".format(desc), file=config)

    if not val_dmi and "dmidecode" in cmd:
        print("\t\t</{0}>".format(desc), file=config)
        return

    res = cmd_execute(cmd)
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

        if desc == "ROOT_DEVICE_INFO":
            ret = handle_root_dev(line)
            if not ret:
                continue

        print("\t{}".format(line.strip()), file=config)

    print("\t</{0}>".format(desc), file=config)
