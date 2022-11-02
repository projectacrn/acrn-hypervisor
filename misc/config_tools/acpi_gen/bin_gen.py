# Copyright (C) 2019-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

"""the tool to generate ACPI binary for Pre-launched VMs.

"""

import logging
import subprocess # nosec
import os, sys, argparse, re, shutil
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'board_inspector'))
import lxml.etree
from acpi_const import *
import acpiparser.tpm2
import inspectorlib.cdata
import acpiparser.rtct
import acrn_config_utilities
from acrn_config_utilities import get_node

def move_rtct_ssram_and_bin_entries(rtct, new_base_addr, new_area_max_size):
    '''
    move the guest ssram and ctl bin entries to a new base addr. the entries keeps their relative layout
    :param rtct: parsed rtct bit struct
    :param new_base_addr: the top address of the new area
    :param new_area_max_size: max size of the new area. for valid check
    :return:
    '''
    if rtct.version == 1:
        expect_ssram_type = acpiparser.rtct.ACPI_RTCT_V1_TYPE_SoftwareSRAM
    elif rtct.version == 2:
        expect_ssram_type = acpiparser.rtct.ACPI_RTCT_V2_TYPE_SoftwareSRAM
    else:
        raise Exception("RTCT version error! ", rtct.version)
    top = 0
    base = 0
    for entry in rtct.entries:
        if entry.type == expect_ssram_type:
            top = (entry.base + entry.size) if top < (entry.base + entry.size) else top
            base = entry.base if base == 0 or entry.base < base else base
    if new_area_max_size < (top - base):
        raise Exception("not enough space in guest VE820 SSRAM area!")
    rtct_move_offset = new_base_addr - base
    for entry in rtct.entries:
        if entry.type == expect_ssram_type:
            entry.base += rtct_move_offset
    # re-calculate checksum
    rtct.header.checksum = 0
    rtct.header.checksum = 0 - sum(bytes(rtct))

def asl_to_aml(dest_vm_acpi_path, dest_vm_acpi_bin_path, scenario_etree, allocation_etree, iasl_path):
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
                rc = exec_command('{} {}'.format(iasl_path, acpi_table[0]))
                if rc == 0 and os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                    shutil.move(os.path.join(dest_vm_acpi_path, acpi_table[1]),
                                os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
                else:
                    if os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                        os.remove(os.path.join(dest_vm_acpi_path, acpi_table[1]))
                    rmsg = 'failed to compile {}'.format(acpi_table[0])
                    break
        elif acpi_table[0] in ['ptct.aml', 'rtct.aml']:
            if acpi_table[0] in os.listdir(dest_vm_acpi_path):
                rtct = acpiparser.rtct.RTCT(os.path.join(dest_vm_acpi_path, acpi_table[0]))
                outfile = os.path.join(dest_vm_acpi_bin_path, acpi_table[1])
                # move the guest ssram area to the area next to ACPI region
                pre_rt_vms = get_node("//vm[load_order ='PRE_LAUNCHED_VM' and vm_type ='RTVM']", scenario_etree)
                vm_id = pre_rt_vms.get("id")
                allocation_vm_node = get_node(f"/acrn-config/vm[@id = '{vm_id}']", allocation_etree)
                ssram_start_gpa = get_node("./ssram/start_gpa/text()", allocation_vm_node)
                ssram_max_size = get_node("./ssram/max_size/text()", allocation_vm_node)
                move_rtct_ssram_and_bin_entries(rtct, int(ssram_start_gpa, 16), int(ssram_max_size, 16))
                fp = open(outfile, mode='wb')
                fp.write(rtct)
                fp.close()
        else:
            if acpi_table[0].endswith(".asl"):
                rc = exec_command('{} {}'.format(iasl_path, acpi_table[0]))
                if rc == 0 and os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                    shutil.move(os.path.join(dest_vm_acpi_path, acpi_table[1]),
                                os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))
                else:
                    if os.path.isfile(os.path.join(dest_vm_acpi_path, acpi_table[1])):
                        os.remove(os.path.join(dest_vm_acpi_path, acpi_table[1]))
                    rmsg = 'failed to compile {}'.format(acpi_table[0])
                    break
            elif acpi_table[0].endswith(".aml") and acpi_table[0] in os.listdir(dest_vm_acpi_path):
                shutil.copy(os.path.join(dest_vm_acpi_path, acpi_table[0]),
                            os.path.join(dest_vm_acpi_bin_path, acpi_table[1]))

    os.chdir(curr_path)
    if not rmsg:
        print('compile ACPI ASL code to {} successfully'.format(dest_vm_acpi_bin_path))
    return rmsg

def tpm2_acpi_gen(acpi_bin, board_etree, scenario_etree, allocation_etree):
    tpm2_enabled = get_node("//vm[@id = '0']/mmio_resources/TPM2/text()", scenario_etree)
    if tpm2_enabled is not None and tpm2_enabled == 'y':
        tpm2_node = get_node("//device[@id = 'MSFT0101' or compatible_id = 'MSFT0101']", board_etree)
        if tpm2_node is not None:
            _data_len = 0x4c if get_node("//capability[@id = 'log_area']", board_etree) is not None else 0x40
            _data = bytearray(_data_len)
            ctype_data = acpiparser.tpm2.TPM2(_data)
            ctype_data.header.signature = "TPM2".encode()
            ctype_data.header.length = _data_len
            ctype_data.header.revision = 4
            ctype_data.header.oemid = "ACRN  ".encode()
            ctype_data.header.oemtableid = "ACRNTPM2".encode()
            ctype_data.header.oemrevision = 0x1
            ctype_data.header.creatorid = "INTL".encode()
            ctype_data.header.creatorrevision = 0x20190703
            ctype_data.address_of_control_area = 0xFED40040
            ctype_data.start_method = int(get_node("//capability[@id = 'start_method']/value/text()", tpm2_node), 16)
            start_method_parameters = tpm2_node.xpath("//parameter/text()")
            for i in range(len(start_method_parameters)):
                ctype_data.start_method_specific_parameters[i] = int(start_method_parameters[i], 16)
            if get_node("//capability[@id = 'log_area']", board_etree) is not None:
                ctype_data.log_area_minimum_length = int(get_node("//log_area_minimum_length/text()", allocation_etree), 16)
                ctype_data.log_area_start_address = int(get_node("//log_area_start_address/text()", allocation_etree), 16)
            ctype_data.header.checksum = (~(sum(inspectorlib.cdata.to_bytes(ctype_data))) + 1) & 0xFF
            acpi_bin.seek(ACPI_TPM2_ADDR_OFFSET)
            acpi_bin.write(inspectorlib.cdata.to_bytes(ctype_data))
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

        acpi_bin.seek(ACPI_DSDT_ADDR_OFFSET)
        with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[6][1]), 'rb') as asl:
            acpi_bin.write(asl.read())

        if ACPI_TABLE_LIST[7][1] in os.listdir(dest_vm_acpi_path):
            acpi_bin.seek(ACPI_RTCT_ADDR_OFFSET)
            with open(os.path.join(dest_vm_acpi_bin_path, ACPI_TABLE_LIST[7][1]), 'rb') as asl:
                acpi_bin.write(asl.read())
        elif ACPI_TABLE_LIST[8][1] in os.listdir(dest_vm_acpi_path):
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


def check_iasl(iasl_path, iasl_min_ver):
    '''
    check iasl installed
    :return: True if iasl installed.
    '''
    try:
        p_version = 'ASL+ Optimizing Compiler/Disassembler version'
        min_version = int(iasl_min_ver)
        output = subprocess.check_output([iasl_path, '-v']).decode('utf8')
        if p_version in output:
            try:
                for line in output.split('\n'):
                    if line.find(p_version) >= 0:
                        version = int(line.split(p_version)[1].strip())
                        print('iasl version is {}'.format(version))
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

    board_etree = lxml.etree.parse(args.board)
    scenario_etree = lxml.etree.parse(args.scenario)

    scenario_name = get_node("//@scenario", scenario_etree)

    if args.asl is None:
        DEST_ACPI_PATH = os.path.join(VM_CONFIGS_PATH, 'scenarios', scenario_name)
    else:
        DEST_ACPI_PATH = os.path.join(acrn_config_utilities.SOURCE_ROOT_DIR, args.asl, 'scenarios', scenario_name)
    if args.out is None:
        hypervisor_out = os.path.join(acrn_config_utilities.SOURCE_ROOT_DIR, 'build', 'hypervisor')
    else:
        hypervisor_out = args.out
    DEST_ACPI_BIN_PATH = os.path.join(hypervisor_out, 'acpi')

    allocation_etree = lxml.etree.parse(os.path.join(hypervisor_out, 'configs', 'allocation.xml'))

    if os.path.isdir(DEST_ACPI_BIN_PATH):
        shutil.rmtree(DEST_ACPI_BIN_PATH)

    if not check_iasl(args.iasl_path, args.iasl_min_ver):
        print('Please install iasl tool with version >= {} from https://www.acpica.org/downloads '
              'before ACPI generation.'.format(args.iasl_min_ver))
        return 1

    for config in os.listdir(DEST_ACPI_PATH):
        if os.path.isdir(os.path.join(DEST_ACPI_PATH, config)) and config.startswith('ACPI_VM'):
            print('start to generate ACPI binary for {}'.format(config))
            dest_vm_acpi_path = os.path.join(DEST_ACPI_PATH, config)
            dest_vm_acpi_bin_path = os.path.join(DEST_ACPI_BIN_PATH, config)
            os.makedirs(dest_vm_acpi_bin_path)
            if asl_to_aml(dest_vm_acpi_path, dest_vm_acpi_bin_path, scenario_etree, allocation_etree, args.iasl_path):
                return 1
            aml_to_bin(dest_vm_acpi_path, dest_vm_acpi_bin_path, config+'.bin', board_etree, scenario_etree, allocation_etree)

    return 0

if __name__ == '__main__':
    parser = argparse.ArgumentParser(usage="python3 bin_gen.py --board [board] --scenario [scenario]"
                                           " --iasl_path [the path to the iasl compiler]"
                                           " --iasl_min_ver [the minimum iasl version]"
                                           "[ --out [output dir of acpi ASL code]]",
                                     description="the tool to generate ACPI binary for Pre-launched VMs")
    parser.add_argument("--board", required=True, help="the XML file summarizing characteristics of the target board")
    parser.add_argument("--scenario", required=True, help="the XML file specifying the scenario to be set up")
    parser.add_argument("--asl", default=None, help="the input folder to store the ACPI ASL code. ")
    parser.add_argument("--iasl_path", default=None, help="the path to the iasl compiler.")
    parser.add_argument("--iasl_min_ver", default=None, help="the minimum iasl version.")
    parser.add_argument("--out", default=None, help="the output folder to store the ACPI binary code. "
                                                    "If not specified, the path for the binary code is"
                                                    "build/hypervisor/acpi/")

    args = parser.parse_args()
    rc = main(args)
    sys.exit(rc)
