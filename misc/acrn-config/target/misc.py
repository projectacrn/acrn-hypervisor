# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib

IO_MEM_PATH = '/proc/iomem'


def get_system_ram(config):
    """This will get systemd ram which are usable
    :param config: file pointer that opened for writing board config information
    """
    print("\t<SYSTEM_RAM_INFO>", file=config)
    with open(IO_MEM_PATH, 'rt') as mem_info:

        while True:
            line = mem_info.readline().strip()
            if not line:
                break

            pat_type = line.split(':')[1].strip()
            if pat_type == "System RAM":
                print("\t{}".format(line), file=config)

    print("\t</SYSTEM_RAM_INFO>", file=config)
    print("", file=config)


def get_root_dev(config):
    """This will get available root device
    :param config: file pointer that opened for writing board config information
    """
    cmd = 'blkid'
    desc = 'ROOT_DEVICE_INFO'
    parser_lib.dump_execute(cmd, desc, config)
    print("", file=config)


def generate_info(board_info):
    """Get System Ram information
    :param board_info: this is the file which stores the hardware board information
    """
    with open(board_info, 'a+') as config:

        get_system_ram(config)

        get_root_dev(config)

