# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""the tool to generate ACPI binary for Pre-launched VMs.

"""

import logging
import os, sys, subprocess, argparse, re, shutil
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'board_inspector'))
import lxml.etree
from acpi_const import *
import acpiparser.tpm2
import lib.cdata
import common

def asl_to_aml(dest_vm_acpi_path, dest_vm_acpi_bin_path):
    '''
    compile asl code of ACPI table to aml code.
    :param dest_vm_acpi_path: the path of the asl code of ACPI tables
    :param dest_vm_acpi_bin_path: the path of the aml code of ACPI tables
    :param passthru_devices: passthrough devce list
    :return:
    '''
    curr_path = os.getcwd()
    rmsg = ''

    os.chdir(dest_vm_acpi_path)
    for acpi_table in ACPI_TABLE_LIST:
        if acpi_table[0] == 'tpm2.asl':
            if 'tpm2.asl' in os.listdir(dest_vm_acpi_path):
                rc = exec_command('iasl {}'.format(acpi_table[0]))
                if rc == 0 and os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                    shutil.move(os.path.join(dest_vm_acpi_path, acpi_table[1]),
                                os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
                else:
                    if os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                        os.remove(os.path.join(dest_vm_acpi_path, acpi_table[1]))
                    rmsg = 'failed to compile {}'.format(acpi_table[0])
                    break
        elif acpi_table[0] == 'PTCT':
            if 'PTCT' in os.listdir(dest_vm_acpi_path):
                shutil.copyfile(os.path.join(dest_vm_acpi_path, acpi_table[0]),
                                os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
        elif acpi_table[0] == 'RTCT':
            if 'RTCT' in os.listdir(dest_vm_acpi_path):
                shutil.copyfile(os.path.join(dest_vm_acpi_path, acpi_table[0]),
                                os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
        else:
            rc = exec_command('iasl {}'.format(acpi_table[0]))
            if rc == 0 and os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                shutil.move(os.path.join(dest_vm_acpi_path, acpi_table[1]),
                            os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
            else:
                if os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                    os.remove(os.path.join(dest_vm_acpi_path, acpi_table[1]))
                rmsg = 'failed to compile {}'.format(acpi_table[0])
                break

    os.chdir(curr_path)
    if not rmsg:
        print('compile ACPI ASL code to {} successfully'.format(dest_vm_acpi_bin_path))
    return rmsg

def tpm2_acpi_gen(acpi_bin, board_etree, scenario_etree, allocation_etree):
    tpm2_enabled = common.get_node("//vm[@id = '0']/mmio_resources/TPM2/text()", scenario_etree)
    if tpm2_enabled is not None and tpm2_enabled == 'y':
        tpm2_node = common.get_node("//device[@id = 'MSFT0101']", board_etree)
        if tpm2_node is not None:
            _data_len = 0x4c if common.get_node("//capability[@id = 'log_area']", board_etree) is not None else 0x40
            _data = bytearray(_data_len)
            ctype_data = acpiparser.tpm2.TPM2(_data)
            ctype_data.header.signature = "TPM2".encode()
            ctype_data.header.length = _data_len
            ctype_data.header.revision = 0x3
            ctype_data.header.oemid = "ACRN  ".encode()
            ctype_data.header.oemtableid = "ACRNTPM2".encode()
            ctype_data.header.oemrevision = 0x1
            ctype_data.header.creatorid = "INTL".encode()
            ctype_data.header.creatorrevision = 0x20190703
            ctype_data.address_of_control_area = 0xFED40040
            ctype_data.start_method = int(common.get_node("//capability[@id = 'start_method']/value/text()", tpm2_node), 16)
            start_method_parameters = tpm2_node.xpath("//parameter/text()")
            for i in range(len(start_method_parameters)):
                ctype_data.start_method_specific_parameters[i] = int(start_method_parameters[i], 16)
            if common.get_node("//capability[@id = 'log_area']", board_etree) is not None:
                ctype_data.log_area_minimum_length = int(common.get_node("//log_area_minimum_length/text()", allocation_etree), 16)
                ctype_data.log_area_start_address = int(common.get_node("//log_area_start_address/text()", allocation_etree), 16)
            ctype_data.header.checksum = (~(sum(lib.cdata.to_bytes(ctype_data))) + 1) & 0xFF
            acpi_bin.seek(ACPI_TPM2_ADDR_OFFSET)
            acpi_bin.write(lib.cdata.to_bytes(ctype_data))
        else:
            logging.warning("Passtrhough tpm2 is enabled in scenario but the device is not presented on board.")
            logging.warning("Check there is tpm2 device on board and re-generate the xml using board inspector with --advanced option.")

def aml_to_bin(dest_vm_acpi_path, dest_vm_acpi_bin_path, acpi_bin_name, board_etree, scenario_etree, allocation_etree):
    '''
    create the binary of ACPI table.
    :param dest_vm_acpi_bin_path: the path of the aml code of ACPI tables
    :param acpi_bin: the binary file name of ACPI tables
    :param passthru_devices: passthrough devce list
    :return:
    '''
    acpi_bin_file = os.path.join(dest_vm_acpi_bin_path, acpi_bin_name)
    if os.path.isfile(acpi_bin_file):
        os.remove(acpi_bin_file)
    with open(acpi_bin_file, 'wb') as acpi_bin:
        # acpi_bin.seek(ACPI_RSDP_ADDR_OFFSET)
        # with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[0][1]), 'rb') as asl:
        #     acpi_bin.write(asl.read())

        acpi_bin.seek(ACPI_XSDT_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[1][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        acpi_bin.seek(ACPI_FADT_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[2][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        acpi_bin.seek(ACPI_MCFG_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[3][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        acpi_bin.seek(ACPI_MADT_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[4][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        if 'tpm2.asl' in os.listdir(dest_vm_acpi_path):
            acpi_bin.seek(ACPI_TPM2_ADDR_OFFSET)
            with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[5][1]), 'rb') as asl:
                acpi_bin.write(asl.read())

        acpi_bin.seek(ACPI_DSDT_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[6][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        if 'PTCT' in os.listdir(dest_vm_acpi_path):
            acpi_bin.seek(ACPI_RTCT_ADDR_OFFSET)
            with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[7][1]), 'rb') as asl:
                acpi_bin.write(asl.read())
        elif 'RTCT' in os.listdir(dest_vm_acpi_path):
            acpi_bin.seek(ACPI_RTCT_ADDR_OFFSET)
            with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[8][1]), 'rb') as asl:
                acpi_bin.write(asl.read())

        vm_id = acpi_bin_name.split('.')[0].split('ACPI_VM')[1]
        if vm_id == '0':
            tpm2_acpi_gen(acpi_bin, board_etree, scenario_etree, allocation_etree)

        acpi_bin.seek(0xfffff)
        acpi_bin.write(b'\0')
    shutil.move(acpi_bin_file, os.path.join(dest_vm_acpi_bin_path, '..', acpi_bin_name))
    print('write ACPI binary to {} successfully'.format(os.path.join(dest_vm_acpi_bin_path, '..', acpi_bin_name)))


def exec_command(cmd):
    '''
    execute the command and output logs.
    :param cmd: the command to execute.
    :return:
    '''
    print('exec: ', cmd)
    p_compile_result = r'Compilation successful. (\d+) Errors, (\d+) Warnings, (\d+) Remarks'
    cmd_list = cmd.split()
    rc = 1
    r_lines = []
    try:
        for line in subprocess.check_output(cmd_list).decode('utf8').split('\n'):
            r_lines.append(line)
            m = re.match(p_compile_result, line)
            if m and len(m.groups()) == 3:
                rc = int(m.groups()[0])
                break
    except Exception as e:
        print('exception when exec {}'.format(cmd), e)
        rc = -1

    if rc > 0:
        print('\n'.join(r_lines))

    return rc


def check_iasl():
    '''
    check iasl installed
    :return: True if iasl installed.
    '''
    try:
        p_version = 'ASL+ Optimizing Compiler/Disassembler version'
        min_version = 20190703
        output = subprocess.check_output(['iasl', '-v']).decode('utf8')
        if p_version in output:
            try:
                for line in output.split('\n'):
                    if line.find(p_version) >= 0:
                        version = int(line.split(p_version)[1].strip())
                        if version >= min_version:
                            return True
            except:
                pass
            return False
        elif 'command not found' in output:
            return False
        else:
            print(output)
            return False
    except Exception as e:
        print(e)
        return False


def main(args):

    board_type = args.board
    scenario_name = args.scenario
    board_path = os.path.join(VM_CONFIGS_PATH, 'data', board_type, board_type + '.xml')
    board_etree = lxml.etree.parse(board_path)
    scenario_path = os.path.join(VM_CONFIGS_PATH, 'data', board_type, scenario_name + '.xml')
    scenario_etree = lxml.etree.parse(scenario_path)
    allocation_path = os.path.join(common.SOURCE_ROOT_DIR, 'build', 'hypervisor', 'configs' ,'allocation.xml')
    allocation_etree = lxml.etree.parse(allocation_path)
    if args.asl is None:
        DEST_ACPI_PATH = os.path.join(VM_CONFIGS_PATH, 'scenarios', scenario_name)
    else:
        DEST_ACPI_PATH = os.path.join(common.SOURCE_ROOT_DIR, args.asl, 'scenarios', scenario_name)
    if args.out is None:
        DEST_ACPI_BIN_PATH = os.path.join(common.SOURCE_ROOT_DIR, 'build', 'hypervisor', 'acpi')
    else:
        DEST_ACPI_BIN_PATH = args.out

    if os.path.isdir(DEST_ACPI_BIN_PATH):
        shutil.rmtree(DEST_ACPI_BIN_PATH)

    if not check_iasl():
        print("Please install iasl tool with version >= 20190703 from https://www.acpica.org/downloads before ACPI generation.")
        return 1

    for config in os.listdir(DEST_ACPI_PATH):
        if os.path.isdir(os.path.join(DEST_ACPI_PATH, config)) and config.startswith('ACPI_VM'):
            print('start to generate ACPI binary for {}'.format(config))
            dest_vm_acpi_path = os.path.join(DEST_ACPI_PATH, config)
            dest_vm_acpi_bin_path = os.path.join(DEST_ACPI_BIN_PATH, config)
            os.makedirs(dest_vm_acpi_bin_path)
            if asl_to_aml(dest_vm_acpi_path, dest_vm_acpi_bin_path):
                return 1
            aml_to_bin(dest_vm_acpi_path, dest_vm_acpi_bin_path, config+'.bin', board_etree, scenario_etree, allocation_etree)

    return 0

if __name__ == '__main__':
    parser = argparse.ArgumentParser(usage="python3 bin_gen.py --board [board] --scenario [scenario]"
                                           "[ --out [output dir of acpi ASL code]]",
                                     description="the tool to generate ACPI binary for Pre-launched VMs.")
    parser.add_argument("--board", required=True, help="the board type.")
    parser.add_argument("--scenario", required=True, help="the scenario name.")
    parser.add_argument("--asl", default=None, help="the input folder to store the ACPI ASL code. ")
    parser.add_argument("--out", default=None, help="the output folder to store the ACPI binary code. "
                                                    "If not specified, the path for the binary code is"
                                                    "build/acpi/")

    args = parser.parse_args()
    rc = main(args)
    sys.exit(rc)
