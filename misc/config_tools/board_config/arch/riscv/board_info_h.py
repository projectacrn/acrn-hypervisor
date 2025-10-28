# Copyright (C) 2020-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import acrn_config_utilities
import board_cfg_lib
import scenario_cfg_lib

BOARD_INFO_DEFINE="""#ifndef BOARD_INFO_H
#define BOARD_INFO_H
"""

BOARD_INFO_ENDIF="""
#endif /* BOARD_INFO_H */"""

def gen_known_caps_pci_head(config):
    # TO BE IMPLEMENTED
    pass

def find_hi_mmio_window(config):
    # TO BE IMPLEMENTED
    pass

def generate_file(config):
    # get cpu processor list
    cpu_list = board_cfg_lib.get_processor_info()
    max_cpu_num = len(cpu_list)

    # start to generate board_info.h
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)
    print(BOARD_INFO_DEFINE, file=config)

    # define CONFIG_MAX_PCPCU_NUM
    print("#define MAX_PCPU_NUM\t\t\t{}U".format(max_cpu_num), file=config)

    # define MAX_VMSIX_ON_MSI_PDEVS_NUM
    gen_known_caps_pci_head(config)

    # define MAX_HIDDEN_PDEVS_NUM
    if board_cfg_lib.BOARD_NAME in list(board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB):
        print("#define MAX_HIDDEN_PDEVS_NUM\t\t{}U".format(len(board_cfg_lib.KNOWN_HIDDEN_PDEVS_BOARD_DB[board_cfg_lib.BOARD_NAME])), file=config)
    else:
        print("#define MAX_HIDDEN_PDEVS_NUM\t\t0U", file=config)

    print(BOARD_INFO_ENDIF, file=config)
