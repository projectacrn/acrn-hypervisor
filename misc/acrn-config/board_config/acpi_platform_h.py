# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import board_cfg_lib
import common

PLATFORM_HEADER = r"""/* DO NOT MODIFY THIS FILE UNLESS YOU KNOW WHAT YOU ARE DOING!
 */

#ifndef PLATFORM_ACPI_INFO_H
#define PLATFORM_ACPI_INFO_H
"""

PLATFORM_END_HEADER = "#endif /* PLATFORM_ACPI_INFO_H */"


class OverridAccessSize():
    """ The Pm access size which are needed to redefine """
    def __init__(self):
        self.pm1a_cnt_ac_sz = True
        self.pm1b_cnt_ac_sz = True
        self.pm1b_evt_ac_sz = True

    def style_check_1(self):
        """ Style check if have public method """
        self.pm1a_cnt_ac_sz = True

    def style_check_2(self):
        """ Style check if have public method """
        self.pm1a_cnt_ac_sz = True


def multi_parser(line, s_line, pm_ac_sz, config):
    """
    Multi parse the line
    :param line: it is a line read from default_acpi_info.h
    :param s_line: it is a line read from board information file
    :param pm_ac_sz: it is a class for access size which would be override
    :param config: it is a file pointer to write acpi information
    """
    addr = ['PM1A_EVT_ADDRESS', 'PM1B_EVT_ADDRESS', 'PM1A_CNT_ADDRESS', 'PM1B_CNT_ADDRESS']
    space_id = ['PM1A_EVT_SPACE_ID', 'PM1B_EVT_SPACE_ID', 'PM1A_CNT_SPACE_ID', 'PM1B_CNT_SPACE_ID']

    if line.split()[1] in space_id and line.split()[1] == s_line.split()[1]:
        if line.split()[2] != s_line.split()[2]:
            print("#undef {}".format(s_line.split()[1]), file=config)
            print("{}".format(s_line.strip()), file=config)
        return

    if line.split()[1] in addr and line.split()[1] == s_line.split()[1]:
        if int(line.split()[2].strip('UL'), 16) != \
                int(s_line.split()[2].strip('UL'), 16) and \
                int(s_line.split()[2].strip('UL'), 16) != 0:
            print("#undef {}".format(s_line.split()[1]), file=config)
            print("{}".format(s_line.strip()), file=config)
        else:
            if "PM1B_EVT" in line.split()[1]:
                pm_ac_sz.pm1b_evt_ac_sz = False
            if "PM1B_CNT" in line.split()[1]:
                pm_ac_sz.pm1b_cnt_ac_sz = False

        return

    if line.split()[1] == s_line.split()[1]:
        if "PM1B_EVT" in line.split()[1] and not pm_ac_sz.pm1b_evt_ac_sz:
            return

        if "PM1B_CNT" in line.split()[1] and not pm_ac_sz.pm1b_cnt_ac_sz:
            return

        if "PM1A_CNT" in line.split()[1] and not pm_ac_sz.pm1a_cnt_ac_sz:
            return

        if int(line.split()[2].strip('U'), 16) != int(s_line.split()[2].strip('U'), 16):
            print("#undef {}".format(s_line.split()[1]), file=config)
            print("{}".format(s_line.strip()), file=config)


def multi_info_parser(config, default_platform, msg_s, msg_e):
    """
    Parse multi information
    :param config: it is a file pointer to write acpi information
    :param default_platform: it is the default_acpi_info.h in acrn-hypervisor
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    write_direct = ['PM1A_EVT_ACCESS_SIZE', 'PM1A_EVT_ADDRESS', 'PM1A_CNT_ADDRESS']

    pm_ac_sz = OverridAccessSize()
    multi_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, msg_s, msg_e)

    # S3/S5 not supported by BIOS
    sx_name = msg_s.split('_')[0].strip('<')
    if not multi_lines and sx_name in ("S3", "S5"):
        print("/* {} is not supported by BIOS */".format(sx_name), file=config)
        return

    for s_line in multi_lines:
        # parse the commend line
        if '/*' in s_line:
            print("{}".format(s_line), file=config)
            continue

        if s_line.split()[1] in write_direct:
            if "PM1A_CNT" in s_line.split()[1] and int(s_line.split()[2].strip('UL'), 16) == 0:
                pm_ac_sz.pm1a_cnt_ac_sz = False

            print("{}".format(s_line.strip()), file=config)
            continue

        with open(default_platform, 'r') as default:
            while True:
                line = default.readline()

                if not line:
                    break

                if len(line.split()) < 2:
                    continue

                multi_parser(line, s_line, pm_ac_sz, config)


def write_direct_info_parser(config, msg_s, msg_e):
    """
    Direct to write
    :param config: it is a file pointer to write acpi information
    :param msg_s: it is a pattern of key stings what start to match from board information
    :param msg_e: it is a pattern of key stings what end to match from board information
    """
    vector_lines = board_cfg_lib.get_info(common.BOARD_INFO_FILE, msg_s, msg_e)

    for vector in vector_lines:
        print("{}".format(vector.strip()), file=config)

    print("", file=config)


def drhd_info_parser(config):
    """
    Parse DRHD information
    :param config: it is a file pointer to write acpi information
    """
    prev_num = 0

    drhd_lines = board_cfg_lib.get_info(
        common.BOARD_INFO_FILE, "<DRHD_INFO>", "</DRHD_INFO>")

    # write DRHD
    print("/* DRHD of DMAR */", file=config)
    for drhd in drhd_lines:
        print(drhd.strip(), file=config)


def platform_info_parser(config, default_platform):
    """
    Parse ACPI information
    :param config: it is a file pointer to write acpi information
    :param default_platform: it is the default_acpi_info.h in acrn-hypervisor
    """
    print("\n/* pm sstate data */", file=config)
    multi_info_parser(config, default_platform, "<PM_INFO>", "</PM_INFO>")
    multi_info_parser(config, default_platform, "<S3_INFO>", "</S3_INFO>")
    multi_info_parser(config, default_platform, "<S5_INFO>", "</S5_INFO>")
    print("", file=config)

    write_direct_info_parser(config, "<WAKE_VECTOR_INFO>", "</WAKE_VECTOR_INFO>")
    write_direct_info_parser(config, "<RESET_REGISTER_INFO>", "</RESET_REGISTER_INFO>")
    drhd_info_parser(config)
    write_direct_info_parser(config, "<MMCFG_BASE_INFO>", "</MMCFG_BASE_INFO>")


def generate_file(config, default_platform):
    """
    write board_name_acpi_info.h
    :param config: it is a file pointer to write acpi information
    :param default_platform: it is the default_acpi_info.h in acrn-hypervisor
    """
    print("{}".format(board_cfg_lib.HEADER_LICENSE), file=config)

    print("{}".format(PLATFORM_HEADER), file=config)

    board_cfg_lib.handle_bios_info(config)
    # parse for the platform info
    platform_info_parser(config, default_platform)

    print("{}".format(PLATFORM_END_HEADER), file=config)
