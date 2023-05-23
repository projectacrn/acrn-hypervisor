# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import shutil
import argparse
import pci_dev
import dmi
import acpi
import clos
import misc
import parser_lib
import logging
from inspectorlib import external_tools

OUTPUT = "./out/"
PY_CACHE = "__pycache__"

CPU_VENDOR = "GenuineIntel"

def check_permission():
    """Check if it is root permission"""
    if os.getuid():
        logging.critical("Run this tool with root privileges (sudo).")
        sys.exit(1)

def vendor_check():
    """Check the CPU vendor"""
    with open("/proc/cpuinfo", 'r') as f_node:
        while True:
            line = f_node.readline()
            if len(line.split(':')) == 2:
                if line.split(':')[0].strip() == "vendor_id":
                    vendor_name = line.split(':')[1].strip()
                    return vendor_name == CPU_VENDOR

def check_msr_nodes(cpu_dirs):
    cpu_list_of_no_msr_node = []
    for cpu_num in os.listdir(cpu_dirs):
        if cpu_num.isdigit():
            if os.path.exists(os.path.join(cpu_dirs, "{}/msr".format(cpu_num))):
                continue
            else:
                cpu_list_of_no_msr_node.append(cpu_num)
    return cpu_list_of_no_msr_node

def check_env():
    """Check if there is appropriate environment on this system"""
    if os.path.exists(PY_CACHE):
        shutil.rmtree(PY_CACHE)

    if not external_tools.locate_tools(['cpuid', 'rdmsr', 'lspci', 'dmidecode', 'blkid', 'stty', 'modprobe']):
        sys.exit(1)

    # check cpu msr file
    cpu_dirs = "/dev/cpu"
    if check_msr_nodes(cpu_dirs):
        res = external_tools.run("modprobe msr")
        err_msg = res.stderr.readline().decode('ascii')
        if err_msg:
            logging.critical("{}".format(err_msg))
            exit(-1)
    msr_node_unavailable_cpus = check_msr_nodes(cpu_dirs)
    if msr_node_unavailable_cpus:
        for cpu_num in msr_node_unavailable_cpus:
            logging.critical("Missing CPU MSR file at {}/{}/msr".format(cpu_dirs, cpu_num))
        logging.critical("Missing CPU MSR file /dev/cpu/#/msr. Check the value of CONFIG_X86_MSR in the kernel config." \
        "  Set it to 'Y' and rebuild the OS. Then rerun the Board Inspector.")
        exit(-1)

    # check cpu vendor id
    if not vendor_check():
        logging.critical(f"Unsupported processor {CPU_VENDOR} found.  ACRN requires using a {CPU_VENDOR} processor.")
        sys.exit(1)

    if os.path.exists(OUTPUT):
        shutil.rmtree(OUTPUT)


if __name__ == '__main__':
    check_permission()

    check_env()

    # arguments to parse
    PARSER = argparse.ArgumentParser(usage='%(prog)s <board_name> [--out board_info_file]')
    PARSER.add_argument('board_name', help=":  the name of the board that runs the ACRN hypervisor")
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
