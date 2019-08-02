# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys
import board_cfg_lib


def clos_info_parser():
    """Parser CLOS information"""

    cache_support = False
    clos_max = 0

    cat_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<CLOS_INFO>", "</CLOS_INFO>")

    for line in cat_lines:
        if line.split(':')[0] == "clos supported by cache":
            cache_support = line.split(':')[1].strip()
        elif line.split(':')[0] == "clos max":
            clos_max = int(line.split(':')[1])

    return (cache_support, clos_max)


def gen_cat(config):
    """Get CAT information
    :param config: it is a file pointer of board information for writing to
    """
    (cache_support, clos_max) = clos_info_parser()

    print("\n#include <board.h>", file=config)
    print("#include <acrn_common.h>", file=config)

    if cache_support == "False" or clos_max == 0:
        print("\nstruct platform_clos_info platform_clos_array[0];", file=config)
        print("uint16_t platform_clos_num = 0;", file=config)
    else:
        print("\nstruct platform_clos_info platform_clos_array[{0}] = {{".format(
            clos_max), file=config)
        for i_cnt in range(clos_max):
            print("\t{", file=config)

            print("\t\t.clos_mask = {0},".format(hex(0xff)), file=config)
            if cache_support == "L2":
                print("\t\t.msr_index = MSR_IA32_{0}_MASK_{1},".format(
                    cache_support, i_cnt), file=config)
            elif cache_support == "L3":
                print("\t\t.msr_index = {0}U,".format(hex(0x00000C90+i_cnt)), file=config)
            else:
                board_cfg_lib.print_red("The input of {} was corrupted!".format(
                    board_cfg_lib.BOARD_INFO_FILE))
                sys.exit(1)
            print("\t},", file=config)

        print("};\n", file=config)
        print("uint16_t platform_clos_num = ", file=config, end="")
        print("(uint16_t)(sizeof(platform_clos_array)/sizeof(struct platform_clos_info));",
              file=config)

    print("", file=config)


def gen_px_cx(config):
    """Get Px/Cx and store them to board.c
    :param config: it is a file pointer of board information for writing to
    """
    cpu_brand_lines = board_cfg_lib.get_info(
        board_cfg_lib.BOARD_INFO_FILE, "<CPU_BRAND>", "</CPU_BRAND>")
    cx_lines = board_cfg_lib.get_info(board_cfg_lib.BOARD_INFO_FILE, "<CX_INFO>", "</CX_INFO>")
    px_lines = board_cfg_lib.get_info(board_cfg_lib.BOARD_INFO_FILE, "<PX_INFO>", "</PX_INFO>")

    cx_len = len(cx_lines)
    px_len = len(px_lines)
    #print("#ifdef CONFIG_CPU_POWER_STATES_SUPPORT", file=config)
    print("static const struct cpu_cx_data board_cpu_cx[%s] = {"%str(cx_len), file=config)
    for cx_l in cx_lines:
        print("\t{0}".format(cx_l.strip()), file=config)
    print("};\n", file=config)

    print("static const struct cpu_px_data board_cpu_px[%s] = {"%str(px_len), file=config)
    for px_l in px_lines:
        print("\t{0}".format(px_l.strip()), file=config)
    print("};\n", file=config)

    for brand_line in cpu_brand_lines:
        cpu_brand = brand_line

    print("const struct cpu_state_table board_cpu_state_tbl = {", file=config)
    print("\t{0},".format(cpu_brand.strip()), file=config)
    print("\t{(uint8_t)ARRAY_SIZE(board_cpu_px), board_cpu_px,", file=config)
    print("\t(uint8_t)ARRAY_SIZE(board_cpu_cx), board_cpu_cx}", file=config)
    print("};", file=config)
    #print("#endif", file=config)


def generate_file(config):
    """Start to generate board.c
    :param config: it is a file pointer of board information for writing to
    """
    print("{0}".format(board_cfg_lib.HEADER_LICENSE), file=config)

    # insert bios info into board.c
    board_cfg_lib.handle_bios_info(config)

    # start to parser to get CAT info
    gen_cat(config)

    # start to parser PX/CX info
    gen_px_cx(config)
