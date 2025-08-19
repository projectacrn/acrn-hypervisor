# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import parser_lib, os, re
from inspectorlib import external_tools
from extractors.helpers import get_bdf_from_realpath

MEM_PATH = ['/proc/iomem', '/proc/meminfo']
TTY_PATH = '/sys/class/tty/'
SYS_IRQ_PATH = '/proc/interrupts'
CPU_INFO_PATH = '/sys/devices/system/cpu/possible'

# Please refer kernel_src/include/linux/serial_core.h
ttys_type = {
    '0': 'PORT', # 8b I/O port access
    '2': 'MMIO', # driver-specific
    '3': 'MMIO', # 32b little endian
    '6': 'MMIO', # 32b big endian
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
        res = external_tools.run('{}'.format(cmd))

        while True:
            line = res.stdout.readline().decode('ascii')

            line_len = len(line.split())
            if not line_len or line.split()[-1] == 'error':
                break

            ttys_n = "/dev/ttyS{}".format(s_inc)
            tty_used_list.append(ttys_n)
            break

    return tty_used_list


def is_bdf_format(bdf):
    is_bdf = False

    if len(bdf) == 7 and len(bdf.split(':')[0]) == 2 and len(bdf.split(':')[1].split('.')[0]) == 2 and len(bdf.split(".")[1]) == 1:
        is_bdf = True

    return is_bdf


def iomem2bdf(base):
    bdf = ''
    base_iomem = base.strip('0x').lower()
    with open(MEM_PATH[0], 'rt') as mem_info:

        while True:
            line = mem_info.readline().strip('\n')
            if not line:
                break

            if base_iomem in line and '0000:' in line:
                bdf = line.split(" ")[-1].lstrip("0000").lstrip(":")
                if not is_bdf_format(bdf):
                    continue
                else:
                    break

    return bdf


def get_mmio_register_width(base_addr, ttys_name):
    """Get MMIO register width for serial port
    :param base_addr: base address of MMIO registers
    :param ttys_name: tty device name (e.g., ttyS0)
    :return: register width in bytes as string, or 'unknown' if not found
    """
    # Method 1: Try to get from sysfs attributes
    try:
        # Check for register width attribute
        reg_width_path = '{}{}/iomem_width'.format(TTY_PATH, ttys_name)
        if os.path.exists(reg_width_path):
            reg_width_bits = int(read_ttys_node(reg_width_path))
            reg_width_bytes = reg_width_bits // 8
            return str(reg_width_bytes)
    except (FileNotFoundError, ValueError):
        pass

    # Method 2: Try to get from register shift (common for UART)
    try:
        reg_shift_path = '{}{}/iomem_reg_shift'.format(TTY_PATH, ttys_name)
        if os.path.exists(reg_shift_path):
            reg_shift = int(read_ttys_node(reg_shift_path))
            # Register shift indicates address spacing, can infer width
            # reg_shift = 0: 8-bit registers (1 byte access)
            # reg_shift = 1: 16-bit registers (2 byte access)
            # reg_shift = 2: 32-bit registers (4 byte access)
            if reg_shift == 0:
                return '1'   # 1 byte (8-bit)
            elif reg_shift == 1:
                return '2'   # 2 bytes (16-bit)
            elif reg_shift == 2:
                return '4'   # 4 bytes (32-bit)
            else:
                width_bits = 8 << reg_shift
                return str(width_bits // 8)  # Convert bits to bytes
    except (FileNotFoundError, ValueError):
        pass

    # Method 3: Check device compatible string or driver info
    try:
        device_path = '{}{}/device'.format(TTY_PATH, ttys_name)
        if os.path.exists(device_path):
            # Try to read driver name or compatible string
            uevent_path = '{}/uevent'.format(device_path)
            if os.path.exists(uevent_path):
                with open(uevent_path, 'rt') as uevent_file:
                    content = uevent_file.read()
                    # Look for common UART types that indicate width
                    if '16550' in content or '8250' in content:
                        return '1'   # 1 byte (8-bit UART)
                    elif 'dw-apb-uart' in content:
                        return '4'   # 4 bytes (32-bit DesignWare UART)
    except (FileNotFoundError, ValueError):
        pass

    # Method 4: Check from /proc/iomem description
    try:
        base_clean = base_addr.strip('0x').lower()
        with open(MEM_PATH[0], 'rt') as mem_info:
            while True:
                line = mem_info.readline().strip('\n')
                if not line:
                    break

                if base_clean in line and ('serial' in line.lower() or 'uart' in line.lower()):
                    # Look for hints in the description
                    line_lower = line.lower()
                    if '8250' in line_lower or '16550' in line_lower:
                        return '1'   # 1 byte (8-bit UART)
                    elif 'dw-apb' in line_lower or 'designware' in line_lower:
                        return '4'   # 4 bytes (32-bit UART)
    except (FileNotFoundError, ValueError):
        pass

    # Return None if no width information found
    return None


def dump_ttys_info(ttys_list, config):
    for ttys in ttys_list:
        ttys_n = ttys.split('/')[-1]
        type_path = '{}{}/io_type'.format(TTY_PATH, ttys_n)
        serial_type = read_ttys_node(type_path)

        irq_path = '{}{}/irq'.format(TTY_PATH, ttys_n)
        irq = read_ttys_node(irq_path)
        ttys_irqs.append(irq)

        if ttys_type[serial_type] == 'PORT':
            base_path = '{}{}/port'.format(TTY_PATH, ttys_n)
            base = read_ttys_node(base_path)
            try:
                b = get_bdf_from_realpath(os.path.join(TTY_PATH, ttys_n, 'device'))
                bdf = f'{b[0]}:{b[1]}.{b[2]}'
            except AssertionError:
                bdf = ''
            if bdf:
                print("\tseri:{} type:portio base:{} irq:{} bdf:{}".format(ttys, base, irq, bdf), file=config)
            else:
                print("\tseri:{} type:portio base:{} irq:{}".format(ttys, base, irq), file=config)
        elif ttys_type[serial_type] == 'MMIO':
            base_path = '{}{}/iomem_base'.format(TTY_PATH, ttys_n)
            base = read_ttys_node(base_path)

            # Get MMIO register width in bytes
            mmio_width = get_mmio_register_width(base, ttys_n)

            bdf = iomem2bdf(base)
            if bdf:
                if mmio_width is not None:
                    print('\tseri:{} type:mmio base:{} width:{}byte irq:{} bdf:"{}"'.format(ttys, base, mmio_width, irq, bdf), file=config)
                else:
                    print('\tseri:{} type:mmio base:{} irq:{} bdf:"{}"'.format(ttys, base, irq, bdf), file=config)
            else:
                if mmio_width is not None:
                    print('\tseri:{} type:mmio base:{} width:{}byte irq:{}'.format(ttys, base, mmio_width, irq), file=config)
                else:
                    print('\tseri:{} type:mmio base:{} irq:{}'.format(ttys, base, irq), file=config)


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
    print("\t<IOMEM_INFO>", file=config)
    with open(MEM_PATH[0], 'rt', errors='ignore') as mem_info:

        while True:
            line = mem_info.readline().strip('\n')
            line = re.sub('[^!-~]+', ' ', line)

            if not line:
                break

            print("\t{}".format(line), file=config)

    print("\t</IOMEM_INFO>", file=config)
    print("", file=config)


def dump_block_dev(config):
    """This will get available block device
    :param config: file pointer that opened for writing board config information
    """
    cmd = 'blkid'
    desc = 'BLOCK_DEVICE_INFO'
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
    with open(CPU_INFO_PATH, 'rt') as cpu_info:
        line = cpu_info.readline()

        processor_id = int(line.split('-')[0].strip())
        print("\t{}".format(processor_id), end="", file=config)

        processor_id += 1
        while (processor_id <= int(line.split('-')[1].strip())):
            print(", {}".format(processor_id), end="", file=config)
            processor_id += 1

    print("", file=config)
    print("\t</CPU_PROCESSOR_INFO>", file=config)
    print("", file=config)

def dump_max_msix_table_num(config):

    msix_table_num_list = []
    max_msix_table_num = 1
    cmd = 'lspci -vv | grep "MSI-X" | grep "Count="'
    res_lines = parser_lib.get_output_lines(cmd)
    for line in res_lines:
        tmp_num = line.split('=')[1].split()[0]
        msix_table_num_list.append(int(tmp_num))

    if msix_table_num_list:
        max_msix_table_num = max(msix_table_num_list)
    print("\t<MAX_MSIX_TABLE_NUM>", file=config)
    print("\t{}".format(max_msix_table_num), file=config)
    print("\t</MAX_MSIX_TABLE_NUM>", file=config)


def dump_dev_config_info(config):

    dump_max_msix_table_num(config)
    print("", file=config)

def generate_info(board_info):
    """Get System Ram information
    :param board_info: this is the file which stores the hardware board information
    """
    with open(board_info, 'a+') as config:

        dump_system_ram(config)

        dump_block_dev(config)

        dump_ttys(config)

        dump_free_irqs(config)

        dump_total_mem(config)

        dump_cpu_core_info(config)

        dump_dev_config_info(config)
