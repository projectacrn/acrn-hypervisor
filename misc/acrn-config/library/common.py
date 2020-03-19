# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import getopt
import shutil
import subprocess
import xml.etree.ElementTree as ET

SOURCE_ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../')
HV_LICENSE_FILE = SOURCE_ROOT_DIR + 'misc/acrn-config/library/hypervisor_license'


PY_CACHES = ["__pycache__", "../board_config/__pycache__", "../scenario_config/__pycache__"]
GUEST_FLAG = ["0UL", "GUEST_FLAG_SECURE_WORLD_ENABLED", "GUEST_FLAG_LAPIC_PASSTHROUGH",
              "GUEST_FLAG_IO_COMPLETION_POLLING", "GUEST_FLAG_HIDE_MTRR", "GUEST_FLAG_RT"]

MULTI_ITEM = ["guest_flag", "pcpu_id", "vcpu_clos", "input", "block", "network"]

SIZE_K = 1024
SIZE_M = SIZE_K * 1024
SIZE_2G = 2 * SIZE_M * SIZE_K
SIZE_4G = 2 * SIZE_2G
SIZE_G = SIZE_M * 1024

VM_COUNT = 0
BOARD_INFO_FILE = ""
SCENARIO_INFO_FILE = ""
LAUNCH_INFO_FILE = ""

class MultiItem():

    def __init__(self):
        self.guest_flag = []
        self.pcpu_id = []
        self.vcpu_clos = []
        self.vir_input = []
        self.vir_block = []
        self.vir_console = []
        self.vir_network = []

class TmpItem():

    def __init__(self):
        self.tag = {}
        self.multi = MultiItem()

def open_license():
    """ Get the license """
    with open(HV_LICENSE_FILE, 'r') as f_licence:
        license_s = f_licence.read().strip()
        return license_s


def print_yel(msg, warn=False):
    """
    Print the message with 'Warning' if warn is true
    :param msg: the stings which will be output to STDOUT
    :param warn: the condition if needs to be output the color of yellow with 'Warning'
    """
    if warn:
        print("\033[1;33mWarning\033[0m: "+msg)
    else:
        print("\033[1;33m{0}\033[0m".format(msg))


def print_red(msg, err=False):
    """
    Print the message with 'Error' if err is true
    :param msg: the stings which will be output to STDOUT
    :param err: the condition if needs to be output the color of red with 'Error'
    """
    if err:
        print("\033[1;31mError\033[0m: "+msg)
    else:
        print("\033[1;31m{0}\033[0m".format(msg))


def usage(file_name):
    """ This is usage for how to use this tool """
    print("usage= {} [h] ".format(file_name), end="")
    print("--board <board_info_file> --scenario <scenario_info_file> --out [output folder]")
    print('board_info_file :  file name of the board info')
    print('scenario_info_file :  file name of the scenario info')
    print('output folder :  path to acrn-hypervisor_folder')


def get_param(args):
    """
    Get the script parameters from command line
    :param args: this the command line of string for the script without script name
    """
    err_dic = {}
    board_info_file = False
    scenario_info_file = False
    output_folder = False

    if '--board' not in args or '--scenario' not in args:
        usage(args[0])
        err_dic['common error: get wrong parameter'] = "wrong usage"
        return (err_dic, board_info_file, scenario_info_file, output_folder)

    args_list = args[1:]
    (optlist, args_list) = getopt.getopt(args_list, '', ['board=', 'scenario=', 'out='])
    for arg_k, arg_v in optlist:
        if arg_k == '--board':
            board_info_file = arg_v
        if arg_k == '--scenario':
            scenario_info_file = arg_v
        if arg_k == '--out':
            output_folder = arg_v

    if not board_info_file or not scenario_info_file:
        usage(args[0])
        err_dic['common error: get wrong parameter'] = "wrong usage"
        return (err_dic, board_info_file, scenario_info_file, output_folder)

    if not os.path.exists(board_info_file):
        err_dic['common error: get wrong parameter'] = "{} is not exist!".format(board_info_file)
        return (err_dic, board_info_file, scenario_info_file, output_folder)

    if not os.path.exists(scenario_info_file):
        err_dic['common error: get wrong parameter'] = "{} is not exist!".format(scenario_info_file)
        return (err_dic, board_info_file, scenario_info_file, output_folder)

    return (err_dic, board_info_file, scenario_info_file, output_folder)


def prepare():
    """ Prepare to check the environment """
    err_dic = {}
    bin_list = []

    for excute in bin_list:
        res = subprocess.Popen("which {}".format(excute), shell=True, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, close_fds=True)

        line = res.stdout.readline().decode('ascii')

        if not line:
            err_dic['commn error: check env failed'] = "'{}' not found, please install it!".format(excute)

    for py_cache in PY_CACHES:
        if os.path.exists(py_cache):
            shutil.rmtree(py_cache)

    return err_dic

def get_xml_attrib(config_file, attrib):
    """
    Get attribute from xml at fist line
    :param config_file: it is a file what contains board information for script to read from
    :param attrib: attribute of item in xml
    """
    value = ''
    err_dic = {}
    with open(config_file, 'rt') as fp_info:
        while True:
            line = fp_info.readline()
            if not line:
                break

            if 'board=' in line or 'scenario=' in line:

                if attrib not in line:
                    err_dic['common error: Not match'] = "The root item is not in xml file"
                    return (err_dic, value)

                attrib_list = line.split()
                for attrib_value in attrib_list:
                    if attrib in attrib_value:
                        value = attrib_value.split('"')[1].strip('"')

    return (err_dic, value)


def get_board_name():
    """
    Get board name from board.xml at fist line
    :param board_info: it is a file what contains board information for script to read from
    """
    (err_dic, board) = get_xml_attrib(BOARD_INFO_FILE, "board")
    return (err_dic, board)


def get_scenario_name():
    """
    Get scenario name from scenario.xml at fist line
    :param scenario_info: it is a file what contains board information for script to read from
    """
    (err_dic, scenario) = get_xml_attrib(SCENARIO_INFO_FILE, "scenario")
    return (err_dic, scenario)


def is_config_file_match():

    (err_dic, scenario_for_board) = get_xml_attrib(SCENARIO_INFO_FILE, "board")
    (err_dic, board_name) = get_xml_attrib(BOARD_INFO_FILE, "board")

    if scenario_for_board == board_name:
        return (err_dic, True)
    else:
        return (err_dic, False)


def find_index_guest_flag(flag):
    """
    Find the index in GUEST_FLAG by flag
    :param flag: flag contained by GUEST_FLAG
    :return: index of GUEST_FLAG
    """
    if not flag or flag == '0':
        return '0'

    if not flag.isnumeric():
        for i in range(len(GUEST_FLAG)):
            if flag == GUEST_FLAG[i]:
                flag_str = i
                return flag_str

def find_tmp_flag(leaf_text):
    """
    Get flag and append the value
    :param leaf_text: it is a value of guest flag item
    :return: a list of flag or none
    """
    tmp_flag = []
    tmp_flag = find_index_guest_flag(leaf_text)
    #flag_index = find_index_guest_flag(leaf_text)
    #if flag_index == '0':
    #    tmp_flag.append(0)
    #else:
    #    tmp_flag.append(flag_index)

    return tmp_flag


def get_config_root(config_file):
    """
    This is get root of xml config
    :param config_file: it is a file what contains information for script to read from
    :return: top of root entry
    """
    # create element tree object
    tree = ET.parse(config_file)
    # get root element
    root = tree.getroot()

    return root


def get_vm_num(config_file):
    """
    Get vm number
    :param config_file: it is a file what contains information for script to read from
    :return: total vm number
    """
    vm_count = 0
    root = get_config_root(config_file)
    for item in root:
        # vm number in scenario
        if item.tag == "vm":
            vm_count += 1
    VM_COUNT = vm_count
    return vm_count


# TODO: This will be abandonment in future
def get_sub_leaf_tag(config_file, branch_tag, tag_str=''):
    """
     This is get tag value by tag_str from config file
     :param config_file: it is a file what contains information for script to read from
     :param branch_tag: it is key of patter to config file branch tag item
     :param tag_str: it is key of pattern to config file leaf tag item
     :return: value of tag_str item
     """
    tmp_tag = []
    root = get_config_root(config_file)
    for item in root:
        # for each 2th level item
        for sub in item:
            tmp_flag = []
            tmp_cpus = []
            if sub.tag == branch_tag:
                if not tag_str:
                    tmp_tag.append(sub.text)
                    continue

                # for each 3rd level item
                for leaf in sub:
                    if leaf.tag == tag_str and tag_str not in MULTI_ITEM and sub.tag != "vuart":
                        tmp_tag.append(leaf.text)
                        continue

                    # get guest flag for logical partition vm1
                    if leaf.tag == "guest_flag" and tag_str == "guest_flag":
                        t_flag = find_tmp_flag(leaf.text)
                        tmp_flag.append(t_flag)
                        #continue

                    # get cpu for vm
                    if leaf.tag == "pcpu_id" and tag_str == "pcpu_id":
                        tmp_cpus.append(leaf.text)
                        continue

                    # get vcpu_clos for vm
                    if leaf.tag == "vcpu_clos" and tag_str == "vcpu_clos":
                        tmp_cpus.append(leaf.text)
                        continue

                # append guest flags for each vm
                if tmp_flag and tag_str == "guest_flag":
                    tmp_tag.append(tmp_flag)
                    continue

                # append cpus for vm
                if tmp_cpus and tag_str == "pcpu_id":
                    tmp_tag.append(tmp_cpus)
                    continue

    return tmp_tag


def get_leaf_value(tmp, tag_str, leaf):

    # get guest flag for logical partition vm1
    if leaf.tag == "guest_flag" and tag_str == "guest_flag":
        t_flag = find_tmp_flag(leaf.text)
        tmp.multi.guest_flag.append(t_flag)

    # get cpu for vm
    if leaf.tag == "pcpu_id" and tag_str == "pcpu_id":
        tmp.multi.pcpu_id.append(leaf.text)

    # get vcpu_clos for vm
    if leaf.tag == "vcpu_clos" and tag_str == "vcpu_clos":
        tmp.multi.vcpu_clos.append(leaf.text)

    # get virtio-input for vm
    if leaf.tag == "input" and tag_str == "input":
        tmp.multi.vir_input.append(leaf.text)

    # get virtio-blk for vm
    if leaf.tag == "block" and tag_str == "block":
        tmp.multi.vir_block.append(leaf.text)

    # get virtio-net for vm
    if leaf.tag == "network" and tag_str == "network":
        tmp.multi.vir_network.append(leaf.text)


def get_sub_value(tmp, tag_str, vm_id):

    # append guest flags for each vm
    if tmp.multi.guest_flag and tag_str == "guest_flag":
        tmp.tag[vm_id] = tmp.multi.guest_flag
        tmp.tag.append(tmp.multi.guest_flag)

    # append cpus for vm
    if tmp.multi.pcpu_id and tag_str == "pcpu_id":
        tmp.tag[vm_id] = tmp.multi.pcpu_id

    # append cpus for vm
    if tmp.multi.vcpu_clos and tag_str == "vcpu_clos":
        tmp.tag[vm_id] = tmp.multi.vcpu_clos

    # append virtio input for vm
    if tmp.multi.vir_input and tag_str == "input":
        tmp.tag[vm_id] = tmp.multi.vir_input

    # append virtio block for vm
    if tmp.multi.vir_block and tag_str == "block":
        tmp.tag[vm_id] = tmp.multi.vir_block

    # append virtio network for vm
    if tmp.multi.vir_network and tag_str == "network":
        tmp.tag[vm_id] = tmp.multi.vir_network


def get_leaf_tag_map(config_file, branch_tag, tag_str):
    """
     This is get tag value by tag_str from config file
     :param config_file: it is a file what contains information for script to read from
     :param branch_tag: it is key of patter to config file branch tag item
     :param tag_str: it is key of pattern to config file leaf tag item
     :return: value of tag_str item map
     """
    tmp = TmpItem()
    root = get_config_root(config_file)
    for item in root:
        vm_id = int(item.attrib['id'])
        # for each 2th level item
        for sub in item:
            tmp.multi = MultiItem()
            if sub.tag == branch_tag:
                if not tag_str:
                    if sub.text == None or not sub.text:
                        tmp.tag[vm_id] = ''
                    else:
                        tmp.tag[vm_id] = sub.text
                    continue

                # for each 3rd level item
                for leaf in sub:
                    if leaf.tag == tag_str and tag_str not in MULTI_ITEM and sub.tag != "vuart":
                        if leaf.text == None or not leaf.text:
                            tmp.tag[vm_id] = ''
                        else:
                            tmp.tag[vm_id] = leaf.text
                        continue

                    get_leaf_value(tmp, tag_str, leaf)

                get_sub_value(tmp, tag_str, vm_id)

    return tmp.tag


def order_type_map_vmid(config_file, vm_count):
    """
    This is mapping table for {id:order type}
    :param config_file: it is a file what contains information for script to read from
    :param vm_count: vm number
    :return: table of id:order type dictionary
    """
    order_id_dic = {}
    load_type_list = get_sub_leaf_tag(config_file, "load_order")
    for i in range(vm_count):
        order_id_dic[i] = load_type_list[i]

    return order_id_dic


def get_load_order_by_vmid(config_file, vm_count, idx):
    """
    Get load order by vm id
    :param config_file: it is a file what contains information for script to read from
    :param vm_count: vm number
    :param idx: index of vm id
    :return: table of id:order type dictionary
    """
    err_dic = {}
    order_id_dic = order_type_map_vmid(config_file, vm_count)
    if idx >= vm_count or not order_id_dic:
        err_dic['vm number: failue'] = "Toatal vm number is less than index number"

    return (err_dic, order_id_dic[idx])


def undline_name(name):
    """
    This convert name which has contain '-' to '_'
    :param name: name which contain '-' and ' '
    :return: name_str which contain'_'
    """
    # convert '-' to '_' in name string
    name_str = "_".join(name.split('-')).upper()

    # stitch '_' while ' ' in name string
    if ' ' in name_str:
        name_str = "_".join(name_str.split()).upper()

    return name_str


def round_up(addr, mem_align):
    """Keep memory align"""
    return ((addr + (mem_align - 1)) & (~(mem_align - 1)))


def mkdir(path):

    if not os.path.exists(path):
        try:
            subprocess.check_call('mkdir -p {}'.format(path), shell=True, stdout=subprocess.PIPE)
        except subprocess.CalledProcessError:
            print_red("{} file create failed!".format(path), err=True)
