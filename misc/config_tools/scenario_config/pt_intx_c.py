# Copyright (C) 2020 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import common
import scenario_cfg_lib
import board_cfg_lib


def generate_file(vm_info, config):
    """
    Generate pt_intx.c for Pre-Launched VMs in a scenario.
    :param config: it is pointer for for file write to
    :return: None
    """

    print("{}".format(scenario_cfg_lib.HEADER_LICENSE), file=config)
    print("", file=config)
    print("#include <x86/vm_config.h>", file=config)
    print("", file=config)

    if (board_cfg_lib.is_matched_board(("ehl-crb-b"))
        and vm_info.pt_intx_info.phys_gsi.get(0) is not None
        and len(vm_info.pt_intx_info.phys_gsi[0]) > 0):

        print("struct pt_intx_config vm0_pt_intx[{}U] = {{".format(len(vm_info.pt_intx_info.phys_gsi[0])), file=config)
        for i, (p_pin, v_pin) in enumerate(zip(vm_info.pt_intx_info.phys_gsi[0], vm_info.pt_intx_info.virt_gsi[0])):
            print("\t[{}U] = {{".format(i), file=config)
            print("\t\t.phys_gsi = {}U,".format(p_pin), file=config)
            print("\t\t.virt_gsi = {}U,".format(v_pin), file=config)
            print("\t},", file=config)

        print("};", file=config)
    else:
        print("struct pt_intx_config vm0_pt_intx[1U];", file=config)

    print("", file=config)
