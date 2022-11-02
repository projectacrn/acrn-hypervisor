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

    bdf_list_len = 0
    known_caps_pci_devs = board_cfg_lib.get_known_caps_pci_devs()
    for dev,bdf_list in known_caps_pci_devs.items():
        if dev == "VMSIX":
            bdf_list_len = len(bdf_list)
    print("#define MAX_VMSIX_ON_MSI_PDEVS_NUM\t{}U".format(bdf_list_len), file=config)


def find_hi_mmio_window(config):

    i_cnt = 0
    mmio_min = 0
    mmio_max = 0
    is_hi_mmio = False

    iomem_lines = board_cfg_lib.get_info(acrn_config_utilities.BOARD_INFO_FILE, "<IOMEM_INFO>", "</IOMEM_INFO>")

    for line in iomem_lines:
        if "PCI Bus" not in line:
            continue

        line_start_addr = int(line.split('-')[0], 16)
        line_end_addr = int(line.split('-')[1].split()[0], 16)
        if line_start_addr < acrn_config_utilities.SIZE_4G and line_end_addr < acrn_config_utilities.SIZE_4G:
            continue
        elif line_start_addr < acrn_config_utilities.SIZE_4G and line_end_addr >= acrn_config_utilities.SIZE_4G:
            i_cnt += 1
            is_hi_mmio = True
            mmio_min = acrn_config_utilities.SIZE_4G
            mmio_max = line_end_addr
            continue

        is_hi_mmio = True
        if i_cnt == 0:
            mmio_min = line_start_addr
            mmio_max = line_end_addr

        if mmio_max < line_end_addr:
            mmio_max = line_end_addr
        i_cnt += 1

    print("", file=config)
    if is_hi_mmio:
        print("#define HI_MMIO_START\t\t\t0x%xUL" % acrn_config_utilities.round_down(mmio_min, acrn_config_utilities.SIZE_G), file=config)
        print("#define HI_MMIO_END\t\t\t0x%xUL" % acrn_config_utilities.round_up(mmio_max, acrn_config_utilities.SIZE_G), file=config)
    else:
        print("#define HI_MMIO_START\t\t\t~0UL", file=config)
        print("#define HI_MMIO_END\t\t\t0UL", file=config)
    print("#define HI_MMIO_SIZE\t\t\t{}UL".format(hex(board_cfg_lib.HI_MMIO_OFFSET)), file=config)


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

    # generate HI_MMIO_START/HI_MMIO_END
    find_hi_mmio_window(config)

    p2sb = acrn_config_utilities.get_leaf_tag_map_bool(acrn_config_utilities.SCENARIO_INFO_FILE, "mmio_resources", "p2sb")
    if (acrn_config_utilities.LOAD_ORDER.get(0) == "PRE_LAUNCHED_VM"
        and board_cfg_lib.is_p2sb_passthru_possible()
        and p2sb.get(0, False)):
        print("", file=config)
        print("#define P2SB_VGPIO_DM_ENABLED", file=config)

        hpa = board_cfg_lib.find_p2sb_bar_addr()
        print("#define P2SB_BAR_ADDR\t\t\t0x{:X}UL".format(hpa), file=config)
        gpa = acrn_config_utilities.hpa2gpa(0, hpa, 0x1000000)
        print("#define P2SB_BAR_ADDR_GPA\t\t0x{:X}UL".format(gpa), file=config)
        print("#define P2SB_BAR_SIZE\t\t\t0x1000000UL", file=config)

    if board_cfg_lib.is_matched_board(("ehl-crb-b")):
        print("", file=config)
        print("#define P2SB_BASE_GPIO_PORT_ID\t\t0x69U", file=config)
        print("#define P2SB_MAX_GPIO_COMMUNITIES\t0x6U", file=config)

    print(BOARD_INFO_ENDIF, file=config)
