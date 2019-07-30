# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib

CMDS = {
    'BIOS_INFO':"dmidecode -t 0",
    'BASE_BOARD_INFO':"dmidecode -t 2",
    }


def generate_info(board_info):
    """Get bios and base board information"""
    with open(board_info, 'a+') as config:
        parser_lib.dump_excute(CMDS['BIOS_INFO'], 'BIOS_INFO', config)
        print("", file=config)
        parser_lib.dump_excute(CMDS['BASE_BOARD_INFO'], 'BASE_BOARD_INFO', config)
        print("", file=config)
