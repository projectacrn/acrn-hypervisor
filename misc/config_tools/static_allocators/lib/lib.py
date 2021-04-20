#!/usr/bin/env python3
#
# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#


import sys, os
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
import common, board_cfg_lib, scenario_cfg_lib

def parse_hv_console(scenario_etree):
    """
    There may be 3 types in the console item
    1. BDF:(00:18.2) seri:/dev/ttyS2
    2. /dev/ttyS2
    3. ttyS2
    """
    ttys_n = ''
    ttys = common.get_node("//SERIAL_CONSOLE/text()", scenario_etree)

    if not ttys or ttys == None:
        return ttys_n

    if ttys and 'BDF' in ttys or '/dev' in ttys:
        ttys_n = ttys.split('/')[2]
    else:
        ttys_n = ttys

    return ttys_n

def get_native_ttys():
    native_ttys = {}
    ttys_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, "<TTYS_INFO>", "</TTYS_INFO>")
    if ttys_lines:
        for tty_line in ttys_lines:
            tmp_dic = {}
            #seri:/dev/ttySx type:mmio base:0x91526000 irq:4 [bdf:"00:18.0"]
            #seri:/dev/ttySy type:portio base:0x2f8 irq:5
            tty = tty_line.split('/')[2].split()[0]
            ttys_type = tty_line.split()[1].split(':')[1].strip()
            ttys_base = tty_line.split()[2].split(':')[1].strip()
            ttys_irq = tty_line.split()[3].split(':')[1].strip()
            tmp_dic['type'] = ttys_type
            tmp_dic['base'] = ttys_base
            tmp_dic['irq'] = int(ttys_irq)
            native_ttys[tty] = tmp_dic
    return native_ttys