# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import shutil
from collections import defaultdict
import dmar
import parser_lib
import logging
from inspectorlib import external_tools

SYS_PATH = ['/proc/cpuinfo', '/sys/firmware/acpi/tables/', '/sys/devices/system/cpu/']

ACPI_OP = {
    'AML_ZERO_OP':0x00,
    'AML_ONE_OP':0x01,
    'AML_ALIAS_OP':0x06,
    'AML_NAME_OP':0x08,
    'AML_BYTE_OP':0x0a,
    'AML_WORD_OP':0x0b,
    'AML_DWORD_OP':0x0c,
    'AML_PACKAGE_OP':0x12,
    'AML_VARIABLE_PACKAGE_OP':0x13,
    }

SPACE_ID = {
    0:'SPACE_SYSTEM_MEMORY',
    1:'SPACE_SYSTEM_IO',
    2:'SPACE_PCI_CONFIG',
    3:'SPACE_Embedded_Control',
    4:'SPACE_SMBUS',
    10:'SPACE_PLATFORM_COMM',
    0x7F:'SPACE_FFixedHW',
    }

FACP_OFF = {
    'facs_addr':36,
    'reset_addr':116,
    'reset_value':128,
    'pm1a_evt':148,
    'pm1b_evt':160,
    'pm1a_cnt':172,
    'pm1b_cnt':184,
    }

class SxPkg:
    """This is Sx state structure for power"""
    def __init__(self):
        # This is Sx state structure for power
        self.val_pm1a = ''
        self.val_pm1b = ''
        self.reserved = ''

    def style_check_1(self):
        """Style check if have public method"""
        self.val_pm1a = ''

    def style_check_2(self):
        """Style check if have public method"""
        self.val_pm1a = ''

class GasType:
    """This is generic address structure for power"""
    def __init__(self):
        self.space_id_8b = 0
        self.bit_width_8b = 0
        self.bit_offset_8b = 0
        self.access_size_8b = 0
        self.address_64b = 0

    def style_check_1(self):
        """Style check if have public method"""
        self.space_id_8b = ''

    def style_check_2(self):
        """Style check if have public method"""
        self.space_id_8b = ''

class PxPkg:
    """This is Px state structure for power"""
    def __init__(self):
        self.core_freq = 0
        self.power = 0
        self.trans_latency = 0
        self.bus_latency = 0
        self.control = 0
        self.status = 0

    def style_check_1(self):
        """Style check if have public method"""
        self.power = ''

    def style_check_2(self):
        """Style check if have public method"""
        self.power = ''

class ResetReg:
    """This is Reset Registers meta data"""
    def __init__(self):
        self.reset_reg_addr = 0
        self.reset_reg_space_id = 0
        self.reset_reg_val = 0

    def style_check_1(self):
        """Style check if have public method"""
        self.reset_reg_val = ''

    def style_check_2(self):
        """Style check if have public method"""
        self.reset_reg_val = ''

DWORD_LEN = 4
PACK_TYPE_LEN = 12
WAKE_VECTOR_OFFSET_32 = 12
WAKE_VECTOR_OFFSET_64 = 24
MCFG_OFFSET = 4
MCFG_ENTRY1_OFFSET = 60
MCFG_ENTRY0_BASE_OFFSET = 44

S3_PKG = SxPkg()
S5_PKG = SxPkg()
PackedGas = GasType
PackedCx = GasType


def store_cpu_info(sysnode, config):
    """This will get CPU information
    :param sysnode: the path to get cpu information, like: /proc/cpuifo
    :param config: file pointer that opened for writing board config information
    """
    with open(sysnode, 'r') as f_node:
        line = f_node.readline()
        while line:
            if len(line.split(':')) >= 2:
                if line.split(':')[0].strip() == "model name":
                    model_name = line.split(':')[1].strip()
                    print('\t"{0}"'.format(model_name), file=config)
                    break
            line = f_node.readline()


def write_reset_reg(rst_reg_addr, rst_reg_space_id, rst_reg_val, config):
    """Write reset register info
    :param rst_reg_addr: reset register address
    :param rst_reg_space_id: reset register space id
    :param rst_reg_val: reset register value
    :param config: file pointer that opened for writing board config information
    """
    print("\t{0}".format("<RESET_REGISTER_INFO>"), file=config)
    print("\t#define RESET_REGISTER_ADDRESS  0x{:0>2X}UL".format(
        rst_reg_addr), file=config)
    print("\t#define RESET_REGISTER_SPACE_ID {0}".format(
        SPACE_ID[rst_reg_space_id]), file=config)
    print("\t#define RESET_REGISTER_VALUE    {0}U".format(
        rst_reg_val), file=config)
    print("\t{0}\n".format("</RESET_REGISTER_INFO>"), file=config)


def get_vector_reset(sysnode, config):
    """This will get reset register value
    :param sysnode: the system node of Px power state, like:/sys/firmware/acpi/tables/FACP
    :param config: file pointer that opened for writing board config information
    """
    reset_reg = ResetReg()
    for key, offset in FACP_OFF.items():
        with open(sysnode, 'rb') as f_node:
            f_node.seek(offset, 0)
            if key == 'facs_addr':
                packed_data = f_node.read(DWORD_LEN)
                packed_data_32 = int.from_bytes(packed_data, 'little')+WAKE_VECTOR_OFFSET_32
                packed_data_64 = int.from_bytes(packed_data, 'little')+WAKE_VECTOR_OFFSET_64
                print("\t{0}".format("<WAKE_VECTOR_INFO>"), file=config)
                print("\t#define WAKE_VECTOR_32          0x{:0>2X}UL".format(
                    packed_data_32), file=config)
                print("\t#define WAKE_VECTOR_64          0x{:0>2X}UL".format(
                    packed_data_64), file=config)
                print("\t{0}\n".format("</WAKE_VECTOR_INFO>"), file=config)
            elif key == 'reset_addr':
                packed_data = f_node.read(PACK_TYPE_LEN)
                reset_reg.reset_reg_space_id = packed_data[0]
                reset_reg.reset_reg_addr = int.from_bytes(packed_data[4:11], 'little')
            elif key == 'reset_value':
                packed_data = f_node.read(1)
                reset_reg.reset_reg_val = hex(packed_data[0])

    write_reset_reg(reset_reg.reset_reg_addr, reset_reg.reset_reg_space_id,
                    reset_reg.reset_reg_val, config)


def read_pm_sstate(sysnode, config):
    """This will read Px state of power
    :param sysnode: the system node of Px power state, like:/sys/firmware/acpi/tables/FACP
    :param config: file pointer that opened for writing board config information
    """
    get_vector_reset(sysnode, config)
    print("\t{0}".format("<PM_INFO>"), file=config)
    for key, offset in FACP_OFF.items():
        with open(sysnode, 'rb') as f_node:
            f_node.seek(offset, 0)

            packed_data = f_node.read(PACK_TYPE_LEN)
            PackedGas.space_id_8b = packed_data[0]
            PackedGas.bit_width_8b = packed_data[1]
            PackedGas.bit_offset_8b = packed_data[2]
            PackedGas.access_size_8b = packed_data[3]
            PackedGas.address_64b = int.from_bytes(packed_data[4:11], 'little')

            if key == 'pm1a_evt':
                print("\t#define PM1A_EVT_SPACE_ID       {0}".format(
                    SPACE_ID[PackedGas.space_id_8b]), file=config)
                print("\t#define PM1A_EVT_BIT_WIDTH      {0}U".format(
                    hex(PackedGas.bit_width_8b)), file=config)
                print("\t#define PM1A_EVT_BIT_OFFSET     {0}U".format(
                    hex(PackedGas.bit_offset_8b)), file=config)
                print("\t#define PM1A_EVT_ADDRESS        {0}UL".format(
                    hex(PackedGas.address_64b)), file=config)
                print("\t#define PM1A_EVT_ACCESS_SIZE    {0}U".format(
                    hex(PackedGas.access_size_8b)), file=config)
            elif key == 'pm1a_cnt':
                print("\t#define PM1A_CNT_SPACE_ID       {0}".format(
                    SPACE_ID[PackedGas.space_id_8b]), file=config)
                print("\t#define PM1A_CNT_BIT_WIDTH      {0}U".format(
                    hex(PackedGas.bit_width_8b)), file=config)
                print("\t#define PM1A_CNT_BIT_OFFSET     {0}U".format(
                    hex(PackedGas.bit_offset_8b)), file=config)
                print("\t#define PM1A_CNT_ADDRESS        {0}UL".format(
                    hex(PackedGas.address_64b)), file=config)
                print("\t#define PM1A_CNT_ACCESS_SIZE    {0}U".format(
                    hex(PackedGas.access_size_8b)), file=config)
            elif key == 'pm1b_evt':
                print("\t#define PM1B_EVT_SPACE_ID       {0}".format(
                    SPACE_ID[PackedGas.space_id_8b]), file=config)
                print("\t#define PM1B_EVT_BIT_WIDTH      {0}U".format(
                    hex(PackedGas.bit_width_8b)), file=config)
                print("\t#define PM1B_EVT_BIT_OFFSET     {0}U".format(
                    hex(PackedGas.bit_offset_8b)), file=config)
                print("\t#define PM1B_EVT_ADDRESS        {0}UL".format(
                    hex(PackedGas.address_64b)), file=config)
                print("\t#define PM1B_EVT_ACCESS_SIZE    {0}U".format(
                    hex(PackedGas.access_size_8b)), file=config)
            elif key == 'pm1b_cnt':
                print("\t#define PM1B_CNT_SPACE_ID       {0}".format(
                    SPACE_ID[PackedGas.space_id_8b]), file=config)
                print("\t#define PM1B_CNT_BIT_WIDTH      {0}U".format(
                    hex(PackedGas.bit_width_8b)), file=config)
                print("\t#define PM1B_CNT_BIT_OFFSET     {0}U".format(
                    hex(PackedGas.bit_offset_8b)), file=config)
                print("\t#define PM1B_CNT_ADDRESS        {0}UL".format(
                    hex(PackedGas.address_64b)), file=config)
                print("\t#define PM1B_CNT_ACCESS_SIZE    {0}U".format(
                    hex(PackedGas.access_size_8b)), file=config)
    print("\t{0}\n".format("</PM_INFO>"), file=config)


def if_sx_name(sx_name, f_node):
    """If sx name in this field
    :param sx_name: Sx name in DSDT of apci table, like _s3_, _s5_
    :param f_node: f_node: file pointer that opened for reading sx from
    """
    need_break = need_continue = 0
    name_buf = f_node.read(4)
    if not name_buf:
        need_break = True
        return (need_break, need_continue)

    try:
        if name_buf.decode('ascii').find(sx_name) != -1:
            pass
        else:
            need_continue = True
    except ValueError:
        need_continue = True
        return (need_break, need_continue)
    else:
        pass

    return (need_break, need_continue)


def read_sx_locate(sx_name, f_node):
    """Read the location of sx
    :param sx_name: Sx name in DSDT of apci table, like _s3_, _s5_
    :param f_node: file pointer that opened for sx reading from
    """
    need_continue = need_break = pkg_len = 0

    (need_break, need_continue) = if_sx_name(sx_name, f_node)

    tmp_char = f_node.read(1)
    if not tmp_char:
        need_break = True
        return (need_break, need_continue, pkg_len)
    if hex(int.from_bytes(tmp_char, 'little')) != hex(ACPI_OP['AML_PACKAGE_OP']):
        need_continue = True
        return (need_break, need_continue, pkg_len)

    pkg_len = f_node.read(1)
    if not pkg_len:
        need_break = True
        return (need_break, need_continue, pkg_len)
    if int.from_bytes(pkg_len, 'little') < 5 or int.from_bytes(pkg_len, 'little') > 28:
        need_continue = True
        return (need_break, need_continue, pkg_len)

    return (need_break, need_continue, pkg_len)


def decode_sx_pkg(pkg_len, f_node):
    """Parse and decode the sx pkg
    :param pkg_len: the length of sx package read from f_node
    :param f_node: file pointer that opened for sx reading from
    """
    pkg_val_pm1a = pkg_val_pm1b = pkg_val_resv = need_break = 0
    pkg_buf = f_node.read(int.from_bytes(pkg_len, 'little'))
    if hex(pkg_buf[1]) == ACPI_OP['AML_ZERO_OP'] or \
            hex(pkg_buf[1]) == hex(ACPI_OP['AML_ONE_OP']):
        pkg_val_pm1a = pkg_buf[1]
        if hex(pkg_buf[2]) == hex(ACPI_OP['AML_ZERO_OP']) or \
                hex(pkg_buf[2]) == hex(ACPI_OP['AML_ONE_OP']):
            pkg_val_pm1b = pkg_buf[2]
            pkg_val_resv = pkg_buf[3:5]
        elif hex(pkg_buf[2]) == hex(ACPI_OP['AML_BYTE_OP']):
            pkg_val_pm1b = pkg_buf[3]
            pkg_val_resv = pkg_buf[4:6]
        else:
            need_break = True
            return (pkg_val_pm1a, pkg_val_pm1b, pkg_val_resv, need_break)

    elif hex(pkg_buf[1]) == hex(ACPI_OP['AML_BYTE_OP']):
        pkg_val_pm1a = pkg_buf[2]
        if hex(pkg_buf[3]) == hex(ACPI_OP['AML_ZERO_OP']) or \
                hex(pkg_buf[3]) == hex(ACPI_OP['AML_ONE_OP']):
            pkg_val_pm1b = pkg_buf[3]
            pkg_val_resv = pkg_buf[4:6]
        elif hex(pkg_buf[3]) == hex(ACPI_OP['AML_BYTE_OP']):
            pkg_val_pm1b = pkg_buf[4]
            pkg_val_resv = pkg_buf[5:7]
        else:
            need_break = True
            return (pkg_val_pm1a, pkg_val_pm1b, pkg_val_resv, need_break)
    else:
        need_break = True

    return (pkg_val_pm1a, pkg_val_pm1b, pkg_val_resv, need_break)


def set_default_sx_value(sx_name, config):
    print("\t/* {} is not supported by BIOS */".format(sx_name), file=config)
    print("\t#define {}_PKG_VAL_PM1A         0x0U".format(sx_name), file=config)
    print("\t#define {}_PKG_VAL_PM1B         0x0U".format(sx_name), file=config)
    print("\t#define {}_PKG_RESERVED         0x0U".format(sx_name), file=config)


def read_pm_sdata(sysnode, sx_name, config):
    """This will read pm Sx state of power
    :param sysnode: the system node of Sx power state, like:/sys/firmware/acpi/tables/DSDT
    :param sx_name: Sx name in DSDT of apci table, like _s3_, _s5_
    :param config: file pointer that opened for writing board config information
    """
    with open(sysnode, 'rb') as f_node:
        while True:
            inc = f_node.read(1)
            if inc:
                if hex(int.from_bytes(inc, 'little')) != hex(ACPI_OP['AML_NAME_OP']):
                    continue

                (need_break, need_continue, pkg_len) = read_sx_locate(sx_name, f_node)
                if need_break:
                    # BIOS dose not support for SX, set it to default value
                    s_name = ''
                    if 'S3' in sx_name:
                        s_name = 'S3'
                    else:
                        s_name = 'S5'

                    set_default_sx_value(s_name, config)
                    break
                if need_continue:
                    continue

                # decode sx pkg
                (pkg_val_pm1a, pkg_val_pm1b, pkg_val_resv, need_break) =\
                    decode_sx_pkg(pkg_len, f_node)
                if need_break:
                    break

            else:
                break

            if sx_name == '_S3_':
                S3_PKG.val_pm1a = pkg_val_pm1a
                S3_PKG.val_pm1b = pkg_val_pm1b
                S3_PKG.reserved = pkg_val_resv
                print("\t#define S3_PKG_VAL_PM1A         {0}".format(
                    hex(S3_PKG.val_pm1a))+'U', file=config)
                print("\t#define S3_PKG_VAL_PM1B         {0}".format(
                    S3_PKG.val_pm1b)+'U', file=config)
                print("\t#define S3_PKG_RESERVED         {0}".format(
                    hex(int.from_bytes(S3_PKG.reserved, 'little')))+'U', file=config)

            if sx_name == "_S5_":
                S5_PKG.val_pm1a = pkg_val_pm1a
                S5_PKG.val_pm1b = pkg_val_pm1b
                S5_PKG.reserved = pkg_val_resv
                print("\t#define S5_PKG_VAL_PM1A         {0}".format(
                    hex(S5_PKG.val_pm1a))+'U', file=config)
                print("\t#define S5_PKG_VAL_PM1B         {0}".format(
                    S5_PKG.val_pm1b)+'U', file=config)
                print("\t#define S5_PKG_RESERVED         {0}".format(
                    hex(int.from_bytes(S5_PKG.reserved, 'little')))+'U', file=config)


def get_value_after_str(cstate_str, hw_type):
    """ Get the value after cstate string """
    idx = 0
    cstate_list = cstate_str.split()
    for idx_hw_type, val in enumerate(cstate_list):
        if val.find(hw_type) != -1:
            idx = idx_hw_type
            break

    return cstate_list[idx + 1]


def store_cx_data(sysnode1, sysnode2, config):
    """This will get Cx data of power and store it to PackedCx
    :param sysnode1: the path of cx power state driver
    :param sysnode2: the path of cpuidle
    :param config: file pointer that opened for writing board config information
    """
    i = 0
    state_cpus = {}
    try:
        with open(sysnode1, 'r') as acpi_idle:
            idle_driver = acpi_idle.read(32)

            if idle_driver.find("acpi_idle") == -1:
                logging.info("Failed to collect processor power states because the current CPU idle driver " \
                "does not expose C-state data. If you need ACPI C-states in post-launched VMs, append " \
                "'intel_idle.max_cstate=0' to the kernel command line in GRUB config file.")
                if idle_driver.find("intel_idle") == 0:
                    logging.info("Failed to collect processor power states because the current CPU idle driver " \
                    "does not expose C-state data. If you need ACPI C-states in post-launched VMs, append " \
                    "'intel_idle.max_cstate=0' to the kernel command line in GRUB config file.")
                else:
                    logging.info("Failed to collect processor power states because the platform does not provide " \
                    "C-state data. If you need ACPI C-states in post-launched VMs, enable C-state support in BIOS.")
                print("\t/* Cx data is not available */", file=config)
                return
    except IOError:
        logging.info("Failed to collect processor power states because CPU idle PM support is disabled " \
        "in the current kernel. If you need ACPI C-states in post-launched VMs, rebuild the current kernel " \
        "with CONFIG_CPU_IDLE set to 'y' or 'm'.")
        print("\t/* Cx data is not available */", file=config)
        return

    files = os.listdir(sysnode2)
    for d_path in files:
        if os.path.isdir(sysnode2+d_path):
            state_cpus[d_path] = sysnode2+d_path

    state_cpus = sorted(state_cpus.keys())
    del state_cpus[0]
    cpu_state = ['desc', 'latency', 'power']
    acpi_hw_type = ['HLT', 'MWAIT', 'IOPORT']

    cx_state = defaultdict(dict)
    c_cnt = 1
    for state in state_cpus:
        i += 1
        for item in cpu_state:
            cx_data_file = open(sysnode2+state+'/'+item, 'r')
            cx_state[state][item] = cx_data_file.read().strip()
            cx_state[state]['type'] = i

        PackedCx.space_id_8b = SPACE_ID[0x7F]
        if cx_state[state][cpu_state[0]].find(acpi_hw_type[0]) != -1:
            PackedCx.bit_width_8b = 0
            PackedCx.bit_offset_8b = 0
            PackedCx.access_size_8b = 0
            PackedCx.address_64b = 0
        elif cx_state[state][cpu_state[0]].find(acpi_hw_type[1]) != -1:
            PackedCx.bit_width_8b = 1
            PackedCx.bit_offset_8b = 2
            PackedCx.access_size_8b = 1
            addr_val = get_value_after_str(cx_state[state][cpu_state[0]], acpi_hw_type[1])
            PackedCx.address_64b = addr_val
        elif cx_state[state][cpu_state[0]].find(acpi_hw_type[2]) != -1:
            PackedCx.space_id_8b = SPACE_ID[1]
            PackedCx.bit_width_8b = 8
            PackedCx.bit_offset_8b = 0
            PackedCx.access_size_8b = 0
            addr_val = get_value_after_str(cx_state[state][cpu_state[0]], acpi_hw_type[2])
            PackedCx.address_64b = addr_val
        print("\t{{{{{}, 0x{:0>2X}U, 0x{:0>2X}U, 0x{:0>2X}U, ".format(
            PackedCx.space_id_8b, PackedCx.bit_width_8b, PackedCx.bit_offset_8b,
            PackedCx.access_size_8b), file=config, end="")
        print("0x{:0>2X}UL}}, 0x{:0>2X}U, 0x{:0>2X}U, 0x{:0>2X}U}},\t/* C{} */".format(
            int(str(PackedCx.address_64b), 16),
            cx_state[state]['type'], int(cx_state[state][cpu_state[1]]),
            int(cx_state[state][cpu_state[2]]), c_cnt), file=config)
        c_cnt += 1


def store_px_data(sysnode, config):
    """This will get Px data of power and store it to px data
    :param sysnode: the path of system power state, such as: /sys/devices/system/cpu/
    :param config: file pointer that opened for writing board config information
    """
    px_tmp = PxPkg()
    px_data = {}

    try:
        with open(sysnode+'cpu0/cpufreq/scaling_driver', 'r') as f_node:
            freq_driver = f_node.read()
            if freq_driver.find("acpi-cpufreq") == -1:
                logging.info("The Px data for ACRN relies on acpi-cpufreq driver but it is not found, ")
                if freq_driver.find("intel_pstate") == 0:
                    logging.info("please add intel_pstate=disable in kernel cmdline to fall back to acpi-cpufreq driver")
                else:
                    logging.info("Enable ACPI Pstate in BIOS.")
                print("\t/* Px data is not available */", file=config)
                return
    except IOError:
        logging.info("No scaling_driver found.", warn=True)
        print("\t/* Px data is not available */", file=config)
        return

    try:
        with open(sysnode+'cpufreq/boost', 'r') as f_node:
            boost = f_node.read()
    except IOError:
        boost = 0
        logging.info("Enable CPU turbo in BIOS.")

    with open(sysnode + 'cpu0/cpufreq/scaling_available_frequencies', 'r') as f_node:
        freqs = f_node.read()
    with open(sysnode + 'cpu0/cpufreq/cpuinfo_transition_latency') as f_node:
        latency = int(f_node.read().strip())
        latency = latency//1000

    i = 0
    p_cnt = 0
    for freq in freqs.split():
        if boost != 0 and i == 0:
            res = external_tools.run('rdmsr 0x1ad')
            if res.wait() != 0:
                logging.debug("MSR 0x1ad not support in this platform!")
                return

            result = res.stdout.readline().strip()
            #max_ratio_cpu = result[-2:]
            ctl_state = int(result[-2:], 16) << 8
            i += 1
        else:
            ctl_state = int(freq)//100000 << 8

        px_tmp.core_freq = int(int(freq) / 1000)
        px_tmp.power = 0
        px_tmp.trans_latency = latency
        px_tmp.bus_latency = latency
        px_tmp.control = ctl_state
        px_tmp.status = ctl_state
        px_data[freq] = px_tmp
        print("\t{{0x{:0>2X}UL, 0x{:0>2X}UL, 0x{:0>2X}UL, ".format(
            px_data[freq].core_freq, px_data[freq].power,
            px_data[freq].trans_latency), file=config, end="")
        print("0x{:0>2X}UL, 0x{:0>6X}UL, 0x{:0>6X}UL}},\t/* P{} */".format(
            px_data[freq].bus_latency, px_data[freq].control,
            px_data[freq].status, p_cnt), file=config)

        p_cnt += 1

def store_mmcfg_base_data(mmcfg_node, config):

    print("\t/* PCI mmcfg base of MCFG */", file=config)
    with open(mmcfg_node, 'rb') as mmcfg:
        mmcfg.read(MCFG_OFFSET)
        mmcfg_len_obj = mmcfg.read(DWORD_LEN)
        mmcfg_len_int = int.from_bytes(mmcfg_len_obj, 'little')

        if mmcfg_len_int > MCFG_ENTRY1_OFFSET:
            logging.debug("Multiple PCI segment groups is not supported!")
            return

        mmcfg.seek(MCFG_ENTRY0_BASE_OFFSET, 0)
        mmcfg_base_addr_obj = mmcfg.read(DWORD_LEN)
        mmcfg_base_addr = int.from_bytes(mmcfg_base_addr_obj, 'little')
        print("\t#define DEFAULT_PCI_MMCFG_BASE   {}UL".format(hex(mmcfg_base_addr)), file=config)


def read_tpm_data(config):
    '''get TPM information from ACPI tables
    :param config: file pointer that opened for writing board config information
    :return:
    '''
    if os.path.exists('/sys/firmware/acpi/tables/TPM2'):
        print("\tTPM2", file=config)
    else:
        print("\t/* no TPM device */", file=config)


def gen_acpi_info(config):
    """This will parser the sys node form SYS_PATH and generate ACPI info
    :param config: file pointer that opened for writing board config information
    """
    read_pm_sstate(SYS_PATH[1] + 'FACP', config)

    print("{0}".format("\t<S3_INFO>"), file=config)
    read_pm_sdata(SYS_PATH[1] + 'DSDT', '_S3_', config)
    print("{0}".format("\t</S3_INFO>\n"), file=config)

    print("{0}".format("\t<S5_INFO>"), file=config)
    read_pm_sdata(SYS_PATH[1] + 'DSDT', '_S5_', config)
    print("{0}".format("\t</S5_INFO>\n"), file=config)

    print("{0}".format("\t<DRHD_INFO>"), file=config)
    dmar.write_dmar_data(SYS_PATH[1] + 'DMAR', config)
    print("{0}".format("\t</DRHD_INFO>\n"), file=config)

    print("{0}".format("\t<CPU_BRAND>"), file=config)
    store_cpu_info(SYS_PATH[0], config)
    print("{0}".format("\t</CPU_BRAND>\n"), file=config)

    print("{0}".format("\t<CX_INFO>"), file=config)
    store_cx_data(SYS_PATH[2]+'cpuidle/current_driver', SYS_PATH[2]+'cpu0/cpuidle/', config)
    print("{0}".format("\t</CX_INFO>\n"), file=config)

    print("{0}".format("\t<PX_INFO>"), file=config)
    store_px_data(SYS_PATH[2], config)
    print("{0}".format("\t</PX_INFO>\n"), file=config)

    print("{0}".format("\t<MMCFG_BASE_INFO>"), file=config)
    store_mmcfg_base_data(SYS_PATH[1] + 'MCFG', config)
    print("{0}".format("\t</MMCFG_BASE_INFO>\n"), file=config)

    print("{0}".format("\t<TPM_INFO>"), file=config)
    read_tpm_data(config)
    print("{0}".format("\t</TPM_INFO>\n"), file=config)


def generate_info(board_file):
    """This will generate ACPI info from board file
    :param board_file: this is the file which stores the hardware board information
    """
    # Generate board info
    with open(board_file, 'a+') as config:
        gen_acpi_info(config)
