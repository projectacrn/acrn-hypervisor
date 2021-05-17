# Copyright (C) 2021 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
from acpiparser import parse_rtct
import acpiparser.rtct
import parser_lib

def dump_ssram(config):
    print("\t<RTCT>", file=config)

    rtct = None
    if os.path.exists("/sys/firmware/acpi/tables/PTCT"):
        rtct = parse_rtct(path="/sys/firmware/acpi/tables/PTCT")
    elif os.path.exists("/sys/firmware/acpi/tables/RTCT"):
        rtct = parse_rtct(path="/sys/firmware/acpi/tables/RTCT")

    if rtct:
        for entry in rtct.entries:
            if entry.type == acpiparser.rtct.ACPI_RTCT_TYPE_SoftwareSRAM:
                print("\t\t<SoftwareSRAM>", file=config)
                print("\t\t\t<cache_level>{}</cache_level>".format(entry.cache_level), file=config)
                print("\t\t\t<base>{}</base>".format(hex(entry.base)), file=config)
                print("\t\t\t<ways>{}</ways>".format(hex(entry.ways)), file=config)
                print("\t\t\t<size>{}</size>".format(hex(entry.size)), file=config)
                for apic_id in entry.apic_id_tbl:
                    print("\t\t\t<apic_id>{}</apic_id>".format(hex(apic_id)), file=config)
                print("\t\t</SoftwareSRAM>", file=config)
    else:
        parser_lib.print_yel("No PTCT or RTCT found. The platform may not support pseudo RAM.")

    print("\t</RTCT>", file=config)
    print("", file=config)


def generate_info(board_info):
    """Get system pseudo RAM information
    :param board_info: this is the file which stores the hardware board information
    """
    with open(board_info, 'a+') as config:
        dump_ssram(config)
