# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib

CACHE_TYPE = {
        "L2":4,
        "L3":2
    }


def dump_cpuid_reg(cmd, reg):
    """execute the cmd of cpuid, and return the register value by reg
    :param cmd: command what can be executed in shell
    :param reg: register name
    """
    cache_t = ''

    res = parser_lib.cmd_execute(cmd)
    if reg == "ebx":
        idx = 3

    if reg == "edx":
        idx = 5

    while True:
        line = parser_lib.decode_stdout(res)

        if not line:
            break

        if len(line.split()) <= 2:
            continue

        reg_value = line.split()[idx].split('=')[1]

        if reg == "ebx":
            if int(reg_value, 16) & CACHE_TYPE['L2'] != 0:
                cache_t = "L2"
                break
            elif int(reg_value, 16) & CACHE_TYPE['L3'] != 0:
                cache_t = "L3"
                break
            else:
                cache_t = False
                break
        elif reg == "edx":
            cache_t = int(reg_value, 16) + 1
            break

    return cache_t


def get_clos_info():
    """Get clos max and clos cache type"""
    clos_max = 0
    clos_cache = False
    cmd = "cpuid -r -l 0x10"
    clos_cache = dump_cpuid_reg(cmd, "ebx")

    if clos_cache == "L2":
        cmd = "cpuid -r -l 0x10 --subleaf 2"
    elif clos_cache == "L3":
        cmd = "cpuid -r -l 0x10 --subleaf 1"
    else:
        clos_max = 0
        parser_lib.print_yel("CLOS is not supported!")
        return (clos_cache, clos_max)

    clos_max = dump_cpuid_reg(cmd, "edx")

    return (clos_cache, clos_max)


def generate_info(board_info):
    """Generate clos information
    :param board_info: this is the file which stores the hardware board information
    """
    (clos_cache, clos_max) = get_clos_info()

    with open(board_info, 'a+') as config:
        print("\t<CLOS_INFO>", file=config)
        print("\tclos supported by cache:{}".format(clos_cache), file=config)
        print("\tclos max:{}".format(clos_max), file=config)
        print("\t</CLOS_INFO>\n", file=config)
