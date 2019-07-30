# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib

CMDS = {
    'PCI_DEVICE':"lspci -vv",
    'PCI_VID_PID':"lspci -n",
    }


def generate_info(board_info):
    """Get the pci info"""
    with open(board_info, 'a+') as config:
        parser_lib.dump_excute(CMDS['PCI_DEVICE'], 'PCI_DEVICE', config)
        print("", file=config)
        parser_lib.dump_excute(CMDS['PCI_VID_PID'], 'PCI_VID_PID', config)
        print("", file=config)
