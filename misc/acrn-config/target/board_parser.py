# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import shutil
import argparse
import subprocess
import pci_dev
import dmi
import acpi
import clos
import misc
import parser_lib

OUTPUT = "./out/"
PY_CACHE = "__pycache__"

# This file store information which query from hw board
BIN_LIST = ['cpuid', 'rdmsr', 'lspci', ' dmidecode', 'blkid', 'stty']
PCI_IDS = ["/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"]

CPU_VENDOR = "GenuineIntel"


def check_permission():
    """Check if it is root permission"""
    if os.getuid():
        parser_lib.print_red("You need run with sudo!")
        sys.exit(1)


def native_check():
    """Check if this is natvie os"""
    cmd = "cpuid -r -l 0x01"
    res = parser_lib.cmd_execute(cmd)
    while True:
        line = parser_lib.decode_stdout(res)

        if line:

            if len(line.split()) <= 2:
                continue

            reg_value = line.split()[4].split('=')[1]
            break

    return int(reg_value, 16) & 0x80000000 == 0


def vendor_check():
    """Check the CPU vendor"""
    with open("/proc/cpuinfo", 'r') as f_node:
        while True:
            line = f_node.readline()
            if len(line.split(':')) == 2:
                if line.split(':')[0].strip() == "vendor_id":
                    vendor_name = line.split(':')[1].strip()
                    return vendor_name == CPU_VENDOR


def check_env():
    """Check if there is appropriate environment on this system"""
    if os.path.exists(PY_CACHE):
        shutil.rmtree(PY_CACHE)

    # check cpu vendor id
    if not vendor_check():
        parser_lib.print_red("Please run this tools on {}!".format(CPU_VENDOR))
        sys.exit(1)

    # check if required tools are exists
    for excute in BIN_LIST:
        res = subprocess.Popen("which {}".format(excute),
                               shell=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, close_fds=True)

        line = res.stdout.readline().decode('ascii')
        if not line:
            parser_lib.print_yel("'{}' not found, please install it!".format(excute))
            sys.exit(1)

        if excute == 'cpuid':
            res = subprocess.Popen("cpuid -v",
                                   shell=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, close_fds=True)
            line = res.stdout.readline().decode('ascii')
            version = line.split()[2]
            if int(version) < 20170122:
                parser_lib.print_yel("Need CPUID version >= 20170122")
                sys.exit(1)

    if not native_check():
        parser_lib.print_red("Please run this tools on natvie OS!")
        sys.exit(1)

    if not os.path.exists(PCI_IDS[0]) and not os.path.exists(PCI_IDS[1]):
        parser_lib.print_yel("pci.ids not found, please make sure lspci is installed correctly!")
        sys.exit(1)

    if os.path.exists(OUTPUT):
        shutil.rmtree(OUTPUT)


if __name__ == '__main__':
    check_permission()

    check_env()

    # arguments to parse
    PARSER = argparse.ArgumentParser(usage='%(prog)s <board_name> [--out board_info_file]')
    PARSER.add_argument('board_name', help=":  the name of board that run ACRN hypervisor")
    PARSER.add_argument('--out', help=":  the name of board info file.")
    ARGS = PARSER.parse_args()

    if not ARGS.out:
        os.makedirs(OUTPUT)
        BOARD_INFO = OUTPUT + ARGS.board_name + ".xml"
    else:
        BOARD_INFO = ARGS.out

    with open(BOARD_INFO, 'w+') as f:
        print('<acrn-config board="{}">'.format(ARGS.board_name), file=f)

    # Get bios and base board info and store to board info
    dmi.generate_info(BOARD_INFO)

    # Get pci devicse table and store pci info to board info
    pci_dev.generate_info(BOARD_INFO)

    # Generate board info
    acpi.generate_info(BOARD_INFO)

    # Generate clos info
    clos.generate_info(BOARD_INFO)

    # Generate misc info
    misc.generate_info(BOARD_INFO)

    with open(BOARD_INFO, 'a+') as f:
        print("</acrn-config>", file=f)

    print("{} is generaged successfully!".format(BOARD_INFO))
