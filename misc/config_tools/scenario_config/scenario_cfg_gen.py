# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import copy
import lxml.etree as etree
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'library'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'hv_config'))
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'acpi_gen'))
from scenario_item import HwInfo, VmInfo
import board_cfg_lib
import scenario_cfg_lib
import acrn_config_utilities
import hv_cfg_lib
import board_defconfig
from hv_item import HvInfo
import asl_gen

try:
    import xmlschema
except ImportError:
    pass

ACRN_PATH = acrn_config_utilities.SOURCE_ROOT_DIR
ACRN_CONFIG_DEF = ACRN_PATH + 'misc/config_tools/data/'
GEN_FILE = ["vm_configurations.h", "vm_configurations.c", "pci_dev.c", ".config", "ivshmem_cfg.h", "pt_intx.c"]


def get_scenario_item_values(board_info, scenario_info):
    """
    Glue code to provide user selectable options to config UI tool.
    Return a dictionary of key-value pairs containing features and corresponding lists of
    user selectable values to the config UI tool.
    :param board_info: file that contains board information
    """
    hv_cfg_lib.ERR_LIST = {}
    scenario_item_values = {}
    hw_info = HwInfo(board_info)
    hv_info = HvInfo(scenario_info)

    # get vm count
    acrn_config_utilities.BOARD_INFO_FILE = board_info
    acrn_config_utilities.SCENARIO_INFO_FILE = scenario_info
    acrn_config_utilities.get_vm_num(scenario_info)
    acrn_config_utilities.get_load_order()

    # per scenario
    guest_flags = copy.deepcopy(acrn_config_utilities.GUEST_FLAG)
    guest_flags.remove('0UL')
    scenario_item_values['vm,vm_type'] = scenario_cfg_lib.LOAD_VM_TYPE
    scenario_item_values["vm,cpu_affinity"] = hw_info.get_processor_val()
    scenario_item_values["vm,guest_flags"] = guest_flags
    scenario_item_values["vm,clos,vcpu_clos"] = hw_info.get_clos_val()
    scenario_item_values["vm,pci_devs"] = scenario_cfg_lib.avl_pci_devs()
    scenario_item_values["vm,os_config,kern_type"] = scenario_cfg_lib.KERN_TYPE_LIST
    scenario_item_values["vm,mmio_resources,p2sb"] = hv_cfg_lib.N_Y
    scenario_item_values["vm,mmio_resources,TPM2"] = hv_cfg_lib.N_Y
    scenario_item_values.update(scenario_cfg_lib.avl_vuart_ui_select(scenario_info))
    scenario_item_values["vm,console_vuart,base"] = ['INVALID_PCI_BASE', 'PCI_VUART']
    scenario_item_values["vm,communication_vuart,base"] = ['INVALID_PCI_BASE', 'PCI_VUART']

    # board

    scenario_item_values["hv,DEBUG_OPTIONS,RELEASE"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,DEBUG_OPTIONS,NPK_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,MEM_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,CONSOLE_LOGLEVEL"] = hv_cfg_lib.get_select_range("DEBUG_OPTIONS", "LOG_LEVEL")
    scenario_item_values["hv,DEBUG_OPTIONS,SERIAL_CONSOLE"] = board_cfg_lib.get_native_ttys_info(board_info)

    scenario_item_values["hv,CAPACITIES,MAX_IOAPIC_NUM"] = hv_cfg_lib.get_select_range("CAPACITIES", "IOAPIC_NUM")

    scenario_item_values["hv,FEATURES,MULTIBOOT2_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,RDT,RDT_ENABLED"] = board_cfg_lib.get_rdt_select_opt()
    scenario_item_values["hv,FEATURES,RDT,CDP_ENABLED"] = board_cfg_lib.get_rdt_select_opt()
    scenario_item_values["hv,FEATURES,SCHEDULER"] = hv_cfg_lib.SCHEDULER_TYPE
    scenario_item_values["hv,FEATURES,RELOC_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,HYPERV_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,ACPI_PARSE_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,L1D_VMENTRY_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,MCE_ON_PSC_DISABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,IOMMU_ENFORCE_SNP"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,IVSHMEM,IVSHMEM_ENABLED"] = hv_cfg_lib.N_Y
    scenario_item_values["hv,FEATURES,SSRAM,SSRAM_ENABLED"] = hv_cfg_lib.N_Y

    scenario_cfg_lib.ERR_LIST.update(hv_cfg_lib.ERR_LIST)
    return scenario_item_values


def validate_scenario_schema(scenario_info):
    """
    Validate settings in scenario xml if there is scenario schema
    :param xsd_doc: scenario schema
    :param scenario_info: scenario file
    """

    """
    XMLSchema does not process XInclude.
    Use lxml to expand the schema which is feed to XMLSchema as a string.
    """
    xsd_doc = etree.parse(acrn_config_utilities.SCENARIO_SCHEMA_FILE)
    xsd_doc.xinclude()
    my_schema = xmlschema.XMLSchema11(etree.tostring(xsd_doc, encoding="unicode"))

    it = my_schema.iter_errors(scenario_info)
    for idx, validation_error in enumerate(it, start=1):
        key = ""
        if not validation_error:
            continue
        else:
            path = str(validation_error.path).split("/")
            cnt = 0
            for p in path:
                if '[' in p:
                    idx = int(p.split("[")[1].split("]")[0]) - 1
                    p = p.split("[")[0] + ":id=" + str(idx)
                    path[cnt] = p
                cnt = cnt + 1
            key =','.join(path[2:])
            element = "'" + path[-1] + "' "
            reason = validation_error.reason + ": last call: " + str(validation_error.obj)
            scenario_cfg_lib.ERR_LIST[key] = element + reason

def apply_data_checks(board_info, scenario_info):
    xsd_doc = etree.parse(acrn_config_utilities.DATACHECK_SCHEMA_FILE)
    xsd_doc.xinclude()
    datachecks_schema = xmlschema.XMLSchema11(etree.tostring(xsd_doc, encoding="unicode"))

    main_etree = etree.parse(board_info)
    scenario_etree = etree.parse(scenario_info)
    main_etree.getroot().extend(scenario_etree.getroot()[:])
    # FIXME: Figure out proper error keys for data check failures
    error_key = ""

    it = datachecks_schema.iter_errors(main_etree)
    for idx, error in enumerate(it, start=1):
        anno = error.validator.annotation
        description = anno.documentation[0].text
        severity = anno.elem.get("{https://projectacrn.org}severity")

        if severity == "error":
            if error_key in scenario_cfg_lib.ERR_LIST.keys():
                scenario_cfg_lib.ERR_LIST[error_key].append("\n" + description)
            else:
                scenario_cfg_lib.ERR_LIST[error_key] = description

def validate_scenario_setting(board_info, scenario_info):
    hv_cfg_lib.ERR_LIST = {}
    scenario_cfg_lib.ERR_LIST = {}

    if "xmlschema" in sys.modules.keys():
        validate_scenario_schema(scenario_info)
        apply_data_checks(board_info, scenario_info)

    """
    Validate settings in scenario xml
    :param board_info: board file
    :param scenario_info: scenario file
    :return: return a dictionary that contains errors
    """
    acrn_config_utilities.BOARD_INFO_FILE = board_info
    acrn_config_utilities.SCENARIO_INFO_FILE = scenario_info

    hv_info = HvInfo(scenario_info)
    hv_info.get_info()
    hv_info.check_item()

    scenario_info_items = {}
    vm_info = VmInfo(board_info, scenario_info)
    vm_info.get_info()
    vm_info.set_ivshmem(hv_info.mem.ivshmem_region)
    vm_info.check_item()

    scenario_info_items['vm'] = vm_info
    scenario_info_items['hv'] = hv_info

    scenario_cfg_lib.ERR_LIST.update(hv_cfg_lib.ERR_LIST)
    return (scenario_cfg_lib.ERR_LIST, scenario_info_items)


def main(args):
    """
    Generate board related source code
    :param args: command line args
    """
    err_dic = {}

    (err_dic, params) = acrn_config_utilities.get_param(args)
    if err_dic:
        return err_dic

    # check env
    err_dic = acrn_config_utilities.prepare()
    if err_dic:
        return err_dic

    acrn_config_utilities.BOARD_INFO_FILE = params['--board']
    acrn_config_utilities.SCENARIO_INFO_FILE = params['--scenario']
    acrn_config_utilities.get_vm_num(params['--scenario'])
    acrn_config_utilities.get_load_order()

    # get board name
    (err_dic, board_name) = acrn_config_utilities.get_board_name()

    # get scenario name
    (err_dic, scenario) = acrn_config_utilities.get_scenario_name()
    if err_dic:
        return err_dic

    if acrn_config_utilities.VM_COUNT > acrn_config_utilities.MAX_VM_NUM:
        err_dic['vm count'] = "Number of VMs in scenario xml file should be no greater than hv/CAPACITIES/MAX_VM_NUM ! " \
                              "Now this value is {}.".format(acrn_config_utilities.MAX_VM_NUM)
        return err_dic

    if params['--out']:
        if os.path.isabs(params['--out']):
            scen_output = params['--out'] + "/scenarios/" + scenario + "/"
        else:
            scen_output = ACRN_PATH + params['--out'] + "/scenarios/" + scenario + "/"
    else:
        scen_output = ACRN_CONFIG_DEF + "/" + scenario + "/"

    scen_board = scen_output + "/"
    acrn_config_utilities.mkdir(scen_board)
    acrn_config_utilities.mkdir(scen_output)

    vm_config_h  = scen_output + GEN_FILE[0]
    vm_config_c  = scen_output + GEN_FILE[1]
    pci_config_c = scen_board + GEN_FILE[2]
    config_hv = scen_board + board_name + GEN_FILE[3]
    ivshmem_config_h = scen_board + GEN_FILE[4]
    pt_intx_config_c = scen_board + GEN_FILE[5]

    # parse the scenario.xml
    get_scenario_item_values(params['--board'], params['--scenario'])
    (err_dic, scenario_items) = validate_scenario_setting(params['--board'], params['--scenario'])
    if err_dic:
        acrn_config_utilities.print_red("Scenario xml file validation failed:", err=True)
        return err_dic

    # generate board defconfig
    with open(config_hv, 'w+') as config:
        err_dic = board_defconfig.generate_file(scenario_items['hv'], config)
        if err_dic:
            return err_dic

    # generate ASL code of ACPI tables for Pre-launched VMs
    if not err_dic:
        err_dic = asl_gen.main(args)

    if not err_dic:
        print("Scenario configuration files were created successfully.")
    else:
        print("Failed to create scenario configuration files.")

    return err_dic


def ui_entry_api(board_info, scenario_info, out=''):

    arg_list = ['scenario_cfg_gen.py', '--board', board_info, '--scenario', scenario_info, '--out', out]

    err_dic = acrn_config_utilities.prepare()
    if err_dic:
        return err_dic

    err_dic = main(arg_list)

    return err_dic


if __name__ == '__main__':

    ARGS = sys.argv
    err_dic = main(ARGS)
    if err_dic:
        for err_k, err_v in err_dic.items():
            acrn_config_utilities.print_red("{}: {}".format(err_k, err_v), err=True)
    sys.exit(1 if err_dic else 0)
