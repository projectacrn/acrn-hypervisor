# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import getopt
import shutil
import subprocess # nosec
import defusedxml.ElementTree as ET
import re
import lxml


ACRN_CONFIG_TARGET = ''
SOURCE_ROOT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../')
HV_LICENSE_FILE = SOURCE_ROOT_DIR + 'misc/config_tools/library/hypervisor_license'
SCENARIO_SCHEMA_FILE = SOURCE_ROOT_DIR + 'misc/config_tools/schema/config.xsd'
DATACHECK_SCHEMA_FILE = SOURCE_ROOT_DIR + 'misc/config_tools/schema/datachecks.xsd'


PY_CACHES = ["__pycache__", "../board_config/__pycache__", "../scenario_config/__pycache__"]
GUEST_FLAG = ["0", "0UL", "GUEST_FLAG_SECURE_WORLD_ENABLED", "GUEST_FLAG_LAPIC_PASSTHROUGH",
              "GUEST_FLAG_IO_COMPLETION_POLLING", "GUEST_FLAG_NVMX_ENABLED", "GUEST_FLAG_HIDE_MTRR",
              "GUEST_FLAG_RT", "GUEST_FLAG_SECURITY_VM", "GUEST_FLAG_VCAT_ENABLED",
              "GUEST_FLAG_TEE", "GUEST_FLAG_REE"]

MULTI_ITEM = ["guest_flag", "pcpu_id", "vcpu_clos", "input", "block", "network", "pci_dev", "shm_region", "communication_vuart"]

SIZE_K = 1024
SIZE_M = SIZE_K * 1024
SIZE_2G = 2 * SIZE_M * SIZE_K
SIZE_4G = 2 * SIZE_2G
SIZE_G = SIZE_M * 1024

VM_COUNT = 0
BOARD_INFO_FILE = ""
SCENARIO_INFO_FILE = ""
LAUNCH_INFO_FILE = ""
LOAD_ORDER = {}
RTVM = {}
MAX_VM_NUM = 16

MAX_VUART_NUM = 8

HV_BASE_RAM_SIZE = 0x1400000
VM_RAM_SIZE = 0x400000
TRUSTY_RAM_SIZE = 0x1000000

class MultiItem():

    def __init__(self):
        self.guest_flag = []
        self.pcpu_id = []
        self.vcpu_clos = []
        self.vir_input = []
        self.vir_block = []
        self.vir_console = []
        self.vir_network = []
        self.pci_dev = []
        self.shm_region = []
        self.communication_vuart = []

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
    params = {'--board':'', '--scenario':'', '--out':''}
    args_list = args[1:]

    try:
        (optlist, args_list) = getopt.getopt(args_list, '', ['hv=', 'board=', 'scenario=', 'out='])
    except getopt.GetoptError as err:
        usage(args[0])
        sys.exit(2)
    for arg_k, arg_v in optlist:
        if arg_k == '--board':
            params['--board'] = arg_v
        if arg_k == '--scenario':
            params['--scenario'] = arg_v
        if arg_k == '--out':
            params['--out'] = arg_v

    for par_k, par_v in params.items():
        if par_k == '--out':
            continue

        if not par_v:
            usage(args[0])
            err_dic['wrong usage'] = "Parameter for {} should not empty".format(par_k)
            return (err_dic, params)

        if not os.path.exists(par_v):
            err_dic['wrong usage'] = "{} does not exist!".format(par_v)
            return (err_dic, params)

    return (err_dic, params)


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
                    err_dic['acrn_config_utilities error'] = "The {} attribute is not in xml file".format(attrib)
                    return (err_dic, value)

                attrib_list = line.split()
                for attrib_value in attrib_list:
                    if attrib in attrib_value:
                        value = attrib_value.split('"')[1].strip('"')

    return (err_dic, value)

def count_nodes(xpath, etree):
    return int(etree.xpath(f"count({xpath})"))

def get_node(xpath, etree):
    result = etree.xpath(f"{xpath}")
    assert len(result) <= 1, f"Internal error: multiple element nodes are found for {xpath}"
    return result[0] if len(result) == 1 else None

def update_text(xpath, value, etree, overwrite=False):
    result = etree.xpath(f"{xpath}")
    assert len(result) == 1, "Internal error: cannot set text to multiple nodes at a time"
    if overwrite or not result[0].text:
        result[0].text = str(value)

def append_node(xpath, value, etree, **attribute):
    # Look for an existing ancestor node
    parts = xpath.split("/")
    ancestor_level = 1
    ancestor = None
    while ancestor_level < len(parts):
        result = etree.xpath("/".join(parts[:-ancestor_level]))
        assert len(result) <= 1, "Internal error: cannot append element nodes to multiple ancestors"
        if len(result) == 1:
            ancestor = result[0]
            break
        ancestor_level += 1

    assert ancestor is not None, f"Internal error: cannot find an existing ancestor for {xpath}"
    for tag in parts[-ancestor_level:]:
        child = lxml.etree.Element(tag)
        ancestor.append(child)
        ancestor = child
    if value:
        child.text = str(value)
    for key, value in attribute.items():
        child.set(key, value)
    return ancestor

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


def find_tmp_flag(flag):
    """
    Find the index in GUEST_FLAG by flag
    :param flag: flag contained by GUEST_FLAG
    :return: index of GUEST_FLAG
    """
    if flag == None or flag == '0':
        return '0UL'

    for i in range(len(GUEST_FLAG)):
        if flag == GUEST_FLAG[i]:
            return flag


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
    global VM_COUNT, MAX_VM_NUM
    vm_count = 0
    root = get_config_root(config_file)
    for item in root:
        # vm number in scenario
        if item.tag == "vm":
            vm_count += 1
    VM_COUNT = vm_count
    MAX_VM_NUM = int(root.find(".//MAX_VM_NUM").text)


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

    # get pci_dev for vm
    if leaf.tag == "pci_dev" and tag_str == "pci_dev":
        tmp.multi.pci_dev.append(leaf.text)

    # get shm_region for vm
    if leaf.tag == "shm_region" and tag_str == "shm_region":
        tmp.multi.shm_region.append(leaf.text)

    # get communication_vuart for vm
    if leaf.tag == "communication_vuart" and tag_str == "communication_vuart":
        tmp.multi.communication_vuart.append(leaf.text)


def get_sub_value(tmp, tag_str, vm_id):

    # append guest flags for each vm
    if tmp.multi.guest_flag and tag_str == "guest_flag":
        tmp.tag[vm_id] = tmp.multi.guest_flag

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

    # append pci_dev for vm
    if tmp.multi.pci_dev and tag_str == "pci_dev":
        tmp.tag[vm_id] = tmp.multi.pci_dev

    # append shm_region for vm
    if tmp.multi.shm_region and tag_str == "shm_region":
        tmp.tag[vm_id] = tmp.multi.shm_region

    # append communication_vuart for vm
    if tmp.multi.communication_vuart and tag_str == "communication_vuart":
        tmp.tag[vm_id] = tmp.multi.communication_vuart


def get_leaf_tag_map(config_file, branch_tag, tag_str=''):
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
        if not 'id' in item.attrib.keys():
            continue
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
                    if leaf.tag == tag_str and tag_str not in MULTI_ITEM and sub.tag not in ["legacy_vuart","vuart"]:
                        if leaf.text == None or not leaf.text:
                            tmp.tag[vm_id] = ''
                        else:
                            tmp.tag[vm_id] = leaf.text
                        continue

                    get_leaf_value(tmp, tag_str, leaf)

                get_sub_value(tmp, tag_str, vm_id)

    return dict(sorted(tmp.tag.items()))


def get_vuart_id(tmp_vuart, leaf_tag, leaf_text):
    """
    Get all vuart id member of class
    :param tmp_vuart: a dictionary to store member:value
    :param leaf_tag: key pattern of item tag
    :param leaf_text: key pattern of item tag's value
    :return: a dictionary to which stored member:value
    """
    if leaf_tag == "type":
        tmp_vuart['type'] = leaf_text
    if leaf_tag == "base":
        tmp_vuart['base'] = leaf_text
    if leaf_tag == "irq":
        tmp_vuart['irq'] = leaf_text

    if leaf_tag == "target_vm_id":
        tmp_vuart['target_vm_id'] = leaf_text
    if leaf_tag == "target_uart_id":
        tmp_vuart['target_uart_id'] = leaf_text

    return tmp_vuart


def get_vuart_info_id(config_file, idx):
    """
    Get vuart information by vuart id indexx
    :param config_file: it is a file what contains information for script to read from
    :param idx: vuart index in range: [0,1]
    :return: dictionary which stored the vuart-id
    """
    tmp_tag = {}
    vm_id = 0
    root = get_config_root(config_file)
    for item in root:
        if item.tag == "vm":
            vm_id = int(item.attrib['id'])

        for sub in item:
            tmp_vuart = {}
            for leaf in sub:
                if sub.tag in ["legacy_vuart","vuart"] and int(sub.attrib['id']) == idx:
                    tmp_vuart = get_vuart_id(tmp_vuart, leaf.tag, leaf.text)

            # append vuart for each vm
            if tmp_vuart and sub.tag in ["legacy_vuart","vuart"]:
                tmp_tag[vm_id] = tmp_vuart

    return tmp_tag

def get_vuart_info(config_file):
    tmp_tag = {}
    vm_id = 0
    root = get_config_root(config_file)
    for item in root:
        if item.tag == "vm":
            vm_id = int(item.attrib['id'])
            tmp_tag[vm_id] = {}

        for sub in item:
            tmp_vuart = {}
            for leaf in sub:
                if sub.tag == "console_vuart" or sub.tag == "communication_vuart":
                    vuart_id = int(sub.attrib['id'])
                    tmp_vuart = get_vuart_id(tmp_vuart, leaf.tag, leaf.text)

                    if tmp_vuart:
                        tmp_tag[vm_id][vuart_id] = tmp_vuart

    return tmp_tag


def get_hv_item_tag(config_file, branch_tag, tag_str='', leaf_str=''):

    tmp = ''
    root = get_config_root(config_file)

    for item in root:
        # for each 2th level item
        for sub in item:
            if sub.tag == branch_tag:
                if not tag_str:
                    if sub.text == None or not sub.text:
                        tmp = ''
                    else:
                        tmp = sub.text
                    continue

                # for each 3rd level item
                tmp_list = []
                for leaf in sub:
                    if leaf.tag == tag_str:
                        if not leaf_str:
                            if leaf.tag == tag_str and leaf.text and leaf.text != None:
                                if tag_str == "IVSHMEM_REGION":
                                    tmp_list.append(leaf.text)
                                else:
                                    tmp = leaf.text

                        else:
                            # for each 4rd level item
                            tmp_list = []
                            for leaf_s in leaf:
                                if leaf_s.tag == leaf_str and leaf_s.text and leaf_s.text != None:
                                    if leaf_str == "CLOS_MASK" or leaf_str == "MBA_DELAY" or leaf_str == "IVSHMEM_REGION":
                                        tmp_list.append(leaf_s.text)
                                    else:
                                        tmp = leaf_s.text
                                continue

                            if leaf_str == "CLOS_MASK" or leaf_str == "MBA_DELAY" or leaf_str == "IVSHMEM_REGION":
                                tmp = tmp_list
                                break

                if tag_str == "IVSHMEM_REGION":
                    tmp = tmp_list
                    break

    return tmp


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

def round_down(addr, mem_align):
    """Keep memory align"""
    return (addr & (~(mem_align - 1)))

def round_up(addr, mem_align):
    """Keep memory align"""
    return ((addr + (mem_align - 1)) & (~(mem_align - 1)))


def mkdir(path):

    if not os.path.exists(path):
        import platform
        try:
            if platform.system().lower() == 'windows':
                os.makedirs(path)
            else:
                subprocess.check_call('mkdir -p {}'.format(path), shell=True, stdout=subprocess.PIPE)
        except subprocess.CalledProcessError:
            print_red("{} file create failed!".format(path), err=True)


def num2int(str_value):

    val = 0
    if isinstance(str_value, int):
        val = str_value
        return val
    if str_value.isnumeric():
        val = int(str_value)
    else:
        # hex value
        val = int(str_value, 16)

    return val


def get_load_order():
    global LOAD_ORDER
    LOAD_ORDER = get_leaf_tag_map(SCENARIO_INFO_FILE, "load_order")

def get_RTVM():
    global RTVM
    RTVM = get_leaf_tag_map(SCENARIO_INFO_FILE, "vm_type")


def get_avl_dev_info(bdf_desc_map, pci_sub_class):

    tmp_pci_desc = []
    for sub_class in pci_sub_class:
        for pci_desc_value in bdf_desc_map.values():
            pci_desc_sub_class = ' '.join(pci_desc_value.strip().split(':')[1].split()[1:])
            if sub_class == pci_desc_sub_class:
                tmp_pci_desc.append(pci_desc_value.strip())

    return tmp_pci_desc


def str2bool(v):
    return v.lower() in ("yes", "true", "t", "y", "1") if v else False


def get_leaf_tag_map_bool(config_file, branch_tag, tag_str=''):
    """
    This convert and return map's value from string to bool
    """

    result = {}

    tag_map = get_leaf_tag_map(config_file, branch_tag, tag_str)
    for vm_i, s in tag_map.items():
        result[vm_i] = str2bool(s)

    return result


def hpa2gpa(vm_id, hpa, size):
    return hpa


def str2int(x):
    s = x.replace(" ", "").lower()

    if s:
        base = 10
        if s.startswith('0x'): base = 16
        return int(s, base)

    return 0


def get_pt_intx_table(config_file):
    pt_intx_map = get_leaf_tag_map(config_file, "pt_intx")

    # translation table to normalize the paired phys_gsi and virt_gsi string
    table = {ord('[') : ord('('), ord(']') : ord(')'), ord('{') : ord('('),
        ord('}') : ord(')'), ord(';') : ord(','),
        ord('\n') : None, ord('\r') : None, ord(' ') : None}

    phys_gsi = {}
    virt_gsi = {}

    for vm_i, s in pt_intx_map.items():
        #normalize the phys_gsi and virt_gsi pair string
        s = s.translate(table)

        #extract the phys_gsi and virt_gsi pairs between parenthesis to a list
        s = re.findall(r'\(([^)]+)', s)

        for part in s:
            if not part: continue
            assert ',' in part, "you need to use ',' to separate phys_gsi and virt_gsi!"
            a, b = part.split(',')
            if not a and not b: continue
            assert a and b, "you need to specify both phys_gsi and virt_gsi!"
            a, b = str2int(a), str2int(b)

            if vm_i not in phys_gsi and vm_i not in virt_gsi:
                phys_gsi[vm_i] = []
                virt_gsi[vm_i] = []
            phys_gsi[vm_i].append(a)
            virt_gsi[vm_i].append(b)

    return phys_gsi, virt_gsi
