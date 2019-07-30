# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import subprocess
from collections import defaultdict
import dmar
import parser_lib


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

S3_PKG = SxPkg()
S5_PKG = SxPkg()
PackedGas = GasType
PackedCx = GasType


def store_cpu_info(sysnode, config):
    """This will get CPU information from /proc/cpuifo"""
    with open(sysnode, 'r') as f_node:
        line = f_node.readline()
        while line:
            if len(line.split(':')) >= 2:
                if line.split(':')[0].strip() == "model name":
                    model_name = line.split(':')[1].strip()
                    print('\t\t"{0}"'.format(model_name), file=config)
                    break
            line = f_node.readline()


def write_reset_reg(space_id, rst_reg_addr, rst_reg_space_id, rst_reg_val, config):
    """Write reset register info"""
    print("\t{0}".format("<RESET_REGISTER_INFO>"), file=config)
    print("\t#define RESET_REGISTER_ADDRESS  0x{:0>2X}UL".format(
        rst_reg_addr), file=config)
    print("\t#define RESET_REGISTER_SPACE_ID {0}".format(
        space_id[rst_reg_space_id]), file=config)
    print("\t#define RESET_REGISTER_VALUE    {0}U".format(
        rst_reg_val), file=config)
    print("\t{0}\n".format("</RESET_REGISTER_INFO>"), file=config)


def get_vector_reset(sysnode, config):
    """This will get reset reg value"""
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

    write_reset_reg(SPACE_ID, reset_reg.reset_reg_addr, reset_reg.reset_reg_space_id,
                    reset_reg.reset_reg_val, config)


def read_pm_sstate(sysnode, config):
    """This will read Px state of power"""
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
    """If sx name in this field"""
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
    """Read the location of sx"""
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
    """Parser and decode the sx pkg"""
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


def read_pm_sdata(sysnode, sx_name, config):
    """This will read pm Sx state of power"""
    with open(sysnode, 'rb') as f_node:
        while True:
            inc = f_node.read(1)
            if inc:
                if hex(int.from_bytes(inc, 'little')) != hex(ACPI_OP['AML_NAME_OP']):
                    continue

                (need_break, need_continue, pkg_len) = read_sx_locate(sx_name, f_node)
                if need_break:
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


def store_cx_data(sysnode1, sysnode2, config):
    """This will get Cx data of power and store it to PackedCx"""
    i = 0
    state_cpus = {}
    with open(sysnode1, 'r') as acpi_idle:
        idle_driver = acpi_idle.read(32)

        if idle_driver.find("acpi_idle") == -1:
            if idle_driver.find("intel_idle") == 0:
                parser_lib.print_red("The tool need to run with acpi_idle driver, " +
                                     "please add intel_idle.max_cstate=0 in kernel " +
                                     "cmdline to fall back to acpi_idle driver", err=True)
            else:
                parser_lib.print_red("acpi_idle driver is not found.", err=True)
            sys.exit(1)

    files = os.listdir(sysnode2)
    for d_path in files:
        if os.path.isdir(sysnode2+d_path):
            state_cpus[d_path] = sysnode2+d_path

    state_cpus = sorted(state_cpus.keys())
    del state_cpus[0]
    cpu_state = ['desc', 'latency', 'power']
    acpi_hw_type = ['HLT', 'MWAIT', 'IOPORT']

    cx_state = defaultdict(dict)
    for state in state_cpus:
        i += 1
        for item in cpu_state:
            cx_data_file = open(sysnode2+state+'/'+item, 'r')
            cx_state[state][item] = cx_data_file.read().strip()
            cx_state[state]['type'] = i

        if cx_state[state][cpu_state[0]].find(acpi_hw_type[0]) != -1 or \
             cx_state[state][cpu_state[0]].find(acpi_hw_type[1]) != -1:
            PackedCx.space_id_8b = SPACE_ID[0x7F]
            if cx_state[state][cpu_state[0]].find(acpi_hw_type[0]) != -1:
                PackedCx.bit_width_8b = 0
                PackedCx.bit_offset_8b = 0
                PackedCx.access_size_8b = 0
                PackedCx.address_64b = 0
            else:
                PackedCx.bit_width_8b = 1
                PackedCx.bit_offset_8b = 2
                PackedCx.access_size_8b = 1
                PackedCx.address_64b = cx_state[state][cpu_state[0]].split()[3]
        elif cx_state[state][cpu_state[0]].find(acpi_hw_type[2]) != -1:
            PackedCx.space_id_8b = SPACE_ID[1]
            PackedCx.bit_width_8b = 8
            PackedCx.bit_offset_8b = 0
            PackedCx.access_size_8b = 0
            PackedCx.address_64b = cx_state[state][cpu_state[0]].split()[2]
        print("\t\t{{{{{}, 0x{:0>2X}U, 0x{:0>2X}U, 0x{:0>2X}U, ".format(
            PackedCx.space_id_8b, PackedCx.bit_width_8b, PackedCx.bit_offset_8b,
            PackedCx.access_size_8b), file=config, end="")
        print("0x{:0>2X}UL}}, 0x{:0>2X}U, 0x{:0>2X}U, 0x{:0>2X}U}},".format(
            int(str(PackedCx.address_64b), 16),
            cx_state[state]['type'], int(cx_state[state][cpu_state[1]]),
            int(cx_state[state][cpu_state[2]])), file=config)


def store_px_data(sysnode, config):
    """This will get Px data of power and store it to px data"""
    px_tmp = PxPkg()
    px_data = {}
    with open(sysnode+'cpu0/cpufreq/scaling_driver', 'r') as f_node:
        freq_driver = f_node.read()
        if freq_driver.find("acpi-cpufreq") == -1:
            if freq_driver.find("intel_pstate") == 0:
                parser_lib.print_red("The tool need to run with acpi_cpufreq driver, " +
                                     "please add intel_pstate=disable in kernel cmdline " +
                                     "to fall back to acpi-cpufreq driver.", err=True)
            else:
                parser_lib.print_red("acpi-cpufreq driver is not found.", err=True)
            sys.exit(1)

    try:
        with open(sysnode+'cpufreq/boost', 'r') as f_node:
            boost = f_node.read()
    except IOError:
        boost = 0
        parser_lib.print_yel("CPU turbo is not enabled!")

    with open(sysnode + 'cpu0/cpufreq/scaling_available_frequencies', 'r') as f_node:
        freqs = f_node.read()
    with open(sysnode + 'cpu0/cpufreq/cpuinfo_transition_latency') as f_node:
        latency = int(f_node.read().strip())
        latency = latency//1000

    i = 0
    p_cnt = 0
    for freq in freqs.split():
        if boost != 0 and i == 0:
            try:
                subprocess.check_call('/usr/sbin/rdmsr 0x1ad', shell=True, stdout=subprocess.PIPE)
            except subprocess.CalledProcessError:
                parser_lib.print_red("MSR 0x1ad not support in this platform!", err=True)
                sys.exit(1)

            res = subprocess.Popen('/usr/sbin/rdmsr 0x1ad', shell=True,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, close_fds=True)
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
        print("\t\t{{0x{:0>2X}UL, 0x{:0>2X}UL, 0x{:0>2X}UL, ".format(
            px_data[freq].core_freq, px_data[freq].power,
            px_data[freq].trans_latency), file=config, end="")
        print("0x{:0>2X}UL, 0x{:0>6X}UL, 0x{:0>6X}UL}}, /* P{} */".format(
            px_data[freq].bus_latency, px_data[freq].control,
            px_data[freq].status, p_cnt), file=config)

        p_cnt += 1


def gen_acpi_info(board_fp):
    """This will parser the sys node form SYS_PATH and generate ACPI info"""
    read_pm_sstate(SYS_PATH[1] + 'FACP', board_fp)

    print("{0}".format("\t<S3_INFO>"), file=board_fp)
    read_pm_sdata(SYS_PATH[1] + 'DSDT', '_S3_', board_fp)
    print("{0}".format("\t</S3_INFO>\n"), file=board_fp)

    print("{0}".format("\t<S5_INFO>"), file=board_fp)
    read_pm_sdata(SYS_PATH[1] + 'DSDT', '_S5_', board_fp)
    print("{0}".format("\t</S5_INFO>\n"), file=board_fp)

    print("{0}".format("\t<DRHD_INFO>"), file=board_fp)
    dmar.write_dmar_data(SYS_PATH[1] + 'DMAR', board_fp)
    print("{0}".format("\t</DRHD_INFO>\n"), file=board_fp)

    print("{0}".format("\t<CPU_BRAND>"), file=board_fp)
    store_cpu_info(SYS_PATH[0], board_fp)
    print("{0}".format("\t</CPU_BRAND>\n"), file=board_fp)

    print("{0}".format("\t<CX_INFO>"), file=board_fp)
    store_cx_data(SYS_PATH[2]+'cpuidle/current_driver', SYS_PATH[2]+'cpu0/cpuidle/', board_fp)
    print("{0}".format("\t</CX_INFO>\n"), file=board_fp)

    print("{0}".format("\t<PX_INFO>"), file=board_fp)
    store_px_data(SYS_PATH[2], board_fp)
    print("{0}".format("\t</PX_INFO>\n"), file=board_fp)


def generate_info(board_file):
    """This will generate ACPI info from board file"""
    # Generate board info
    with open(board_file, 'a+') as board_info:
        gen_acpi_info(board_info)
