# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib
import subprocess

MEM_PATH = ['/proc/iomem', '/proc/meminfo']
TTY_PATH = '/sys/class/tty/'
SYS_IRQ_PATH = '/proc/interrupts'
CPU_INFO_PATH = '/proc/cpuinfo'

ttys_type = {
    '0': 'PORT',
    '3': 'MMIO',
}


ttys_irqs = []

def read_ttys_node(path):
        with open(path, 'rt') as info:
            ret_value = info.readline().strip()

        return ret_value


def detected_ttys():
    ttys_cnt = 8
    tty_used_list = []
    for s_inc in range(ttys_cnt):
        cmd = 'stty -F /dev/ttyS{}'.format(s_inc)
        res = parser_lib.cmd_execute('{}'.format(cmd))

        while True:
            line = res.stdout.readline().decode('ascii')

            line_len = len(line.split())
            if not line_len or line.split()[-1] == 'error':
                break

            ttys_n = "/dev/ttyS{}".format(s_inc)
            tty_used_list.append(ttys_n)
            break

    return tty_used_list


def irq2bdf(irq_n):
    cmd = 'lspci -vv'
    res = parser_lib.cmd_execute(cmd)
    bdf = ''
    irq = 0
    while True:
        line = res.stdout.readline().decode('ascii')
        if not line:
            break

        if ':' not in line:
            continue

        if '.' in line.split()[0]:
            bdf = line.split()[0]

        if "Interrupt:" in line.strip():
            irq = line.split()[-1]
            if irq == irq_n and bdf:
                break

    return bdf


def dump_ttys_info(ttys_list, config):
    for ttys in ttys_list:
        ttys_n = ttys.split('/')[-1]
        type_path = '{}{}/io_type'.format(TTY_PATH, ttys_n)
        serial_type = read_ttys_node(type_path)

        irq_path = '{}{}/irq'.format(TTY_PATH, ttys_n)
        irq = read_ttys_node(irq_path)

        if ttys_type[serial_type] == 'PORT':
            base_path = '{}{}/port'.format(TTY_PATH, ttys_n)
        elif ttys_type[serial_type] == 'MMIO':
            base_path = '{}{}/iomem_base'.format(TTY_PATH, ttys_n)

        base = read_ttys_node(base_path)

        ttys_irqs.append(irq)
        bdf = irq2bdf(irq)
        print("\tBDF:({}) seri:{} base:{} irq:{}".format(bdf, ttys, base, irq), file=config)


def dump_ttys(config):
    """This will get systemd ram which are usable
    :param config: file pointer that opened for writing board config information
    """
    print("\t<TTYS_INFO>", file=config)
    ttys_list = detected_ttys()

    dump_ttys_info(ttys_list, config)

    print("\t</TTYS_INFO>", file=config)
    print("", file=config)


def dump_free_irqs(config):
    irq_list = ['3', '4', '5', '6', '7', '8', '10', '11', '12', '13', '14', '15']
    for tty_irq in ttys_irqs:
        if tty_irq in irq_list:
            irq_list.remove(tty_irq)

    print("\t<AVAILABLE_IRQ_INFO>", file=config)
    with open(SYS_IRQ_PATH, 'rt') as irq_config:

        while True:
            line = irq_config.readline().strip()
            if ':' not in line:
                continue

            irq_num = line.split(':')[0]
            if not line or int(irq_num) >= 16:
                break

            if irq_num in irq_list:
                irq_list.remove(irq_num)

    i_cnt = 0
    print("\t", end="", file=config)
    for irq in irq_list:
        i_cnt += 1

        if i_cnt == len(irq_list):
            print("{}".format(irq), file=config)
        else:
            print("{}, ".format(irq), end="", file=config)

    print("\t</AVAILABLE_IRQ_INFO>", file=config)
    print("", file=config)


def dump_system_ram(config):
    """This will get systemd ram which are usable
    :param config: file pointer that opened for writing board config information
    """
    print("\t<SYSTEM_RAM_INFO>", file=config)
    with open(MEM_PATH[0], 'rt') as mem_info:

        while True:
            line = mem_info.readline().strip()
            if not line:
                break

            pat_type = line.split(':')[1].strip()
            if pat_type == "System RAM":
                print("\t{}".format(line), file=config)

    print("\t</SYSTEM_RAM_INFO>", file=config)
    print("", file=config)


def dump_root_dev(config):
    """This will get available root device
    :param config: file pointer that opened for writing board config information
    """
    cmd = 'blkid'
    desc = 'ROOT_DEVICE_INFO'
    parser_lib.dump_execute(cmd, desc, config)
    print("", file=config)



def dump_total_mem(config):

    total_mem = 0
    print("\t<TOTAL_MEM_INFO>", file=config)
    with open(MEM_PATH[1], 'rt') as mem_info:
        while True:
            line = mem_info.readline().strip()

            if not line:
                break

            if ':' in line and line.split(':')[0].strip() == "MemTotal":
                total_mem = line.split(':')[1]
                print("\t{}".format(total_mem.strip()), file=config)

    print("\t</TOTAL_MEM_INFO>", file=config)
    print("", file=config)


def dump_cpu_core_info(config):

    print("\t<CPU_PROCESSOR_INFO>", file=config)
    processor_id_list = []
    with open(CPU_INFO_PATH, 'rt') as cpu_info:
        while True:
            line = cpu_info.readline()

            if not line:
                break

            if ':' in line and line.split(':')[0].strip() == "processor":
                processor_id = line.split(':')[1].strip()
                processor_id_list.append(processor_id)

    processor_len = len(processor_id_list)
    for processor_id in processor_id_list:
        if int(processor_id) == 0:
            if processor_len == 1:
                print("\t{},".format(processor_id.strip()), file=config)
            else:
                print("\t{},".format(processor_id.strip()), end="", file=config)
        else:
            if processor_len == int(processor_id) + 1:
                print(" {}".format(processor_id.strip()), file=config)
            else:
                print(" {},".format(processor_id.strip()), end="", file=config)

    print("\t</CPU_PROCESSOR_INFO>", file=config)
    print("", file=config)


def generate_info(board_info):
    """Get System Ram information
    :param board_info: this is the file which stores the hardware board information
    """
    with open(board_info, 'a+') as config:

        dump_system_ram(config)

        dump_root_dev(config)

        dump_ttys(config)

        dump_free_irqs(config)

        dump_total_mem(config)

        dump_cpu_core_info(config)
