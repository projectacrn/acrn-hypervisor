# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import enum
import board_cfg_lib
import acrn_config_utilities
from defusedxml.lxml import parse
import os
from acrn_config_utilities import get_node
import bareboot_struct

INCLUDE_HEADER = """
#include <pci.h>
#include <misc_cfg.h>
"""

def generate_file(config):
    """
    Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    err_dic = {}
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)

    # insert bios info into board.c
    board_cfg_lib.handle_bios_info(config)
    print(INCLUDE_HEADER, file=config)

    bareboot_struct.gen_bare_boot(config)
