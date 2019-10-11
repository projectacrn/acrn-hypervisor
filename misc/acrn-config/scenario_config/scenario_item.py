# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import scenario_cfg_lib
import board_cfg_lib


class HwInfo:
    """ This is Abstract of class of Hardware information """
    processor_val = []
    clos_val = []
    root_dev_val = []
    ttys_val = []

    def __init__(self, board_file):
        self.board_info = board_file

    def get_processor_val(self):
        """
        Get cpu core list
        :return: cpu processor which one cpu has
        """
        self.processor_val = scenario_cfg_lib.get_processor_info(self.board_info)
        return self.processor_val

    def get_rootdev_val(self):
        """
        Get root devices from board info
        :return: root devices list
        """
        self.root_dev_val = scenario_cfg_lib.get_rootdev_info(self.board_info)
        return self.root_dev_val

    def get_ttys_val(self):
        """
        Get ttySn from board info
        :return: serial console list
        """
        self.ttys_val = scenario_cfg_lib.get_ttys_info(self.board_info)
        return self.ttys_val

    def get_clos_val(self):
        """
        Get clos max number from board info
        :return: clos support list
        """
        self.clos_val = []
        (clos_support, clos_max) = board_cfg_lib.clos_info_parser(self.board_info)
        if clos_support:
            for i_cnt in range(clos_max):
                self.clos_val.append(str(i_cnt))

        return self.clos_val

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.processor_val = self.get_processor_val()
        self.get_rootdev_val()
        self.get_ttys_val()
        self.get_clos_val()


class CfgOsKern:
    """ This is Abstract of class of configuration of vm os kernel setting """
    kern_name = {}
    kern_type = {}
    kern_mod = {}
    kern_args = {}
    kern_console = {}
    kern_load_addr = {}
    kern_entry_addr = {}
    kern_root_dev = {}
    kern_args_append = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.kern_name = scenario_cfg_lib.get_leaf_tag_map(self.scenario_info, "os_config", "name")
        self.kern_type = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_type")
        self.kern_mod = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_mod")
        self.kern_args = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "bootargs")
        self.kern_console = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "console")
        self.kern_load_addr = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_load_addr")
        self.kern_entry_addr = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_entry_addr")
        self.kern_root_dev = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "os_config", "rootfs")
        self.kern_args_append = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "boot_private", "bootargs")

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.os_kern_name_check(self.kern_name, "name")
        scenario_cfg_lib.os_kern_type_check(self.kern_type, "kern_type")
        scenario_cfg_lib.os_kern_mod_check(self.kern_mod, "kern_mod")
        scenario_cfg_lib.os_kern_args_check(self.kern_args, "kern_args")
        scenario_cfg_lib.os_kern_console_check(self.kern_console, "console")
        scenario_cfg_lib.os_kern_load_addr_check(self.kern_load_addr, "kern_load_addr")
        scenario_cfg_lib.os_kern_entry_addr_check(self.kern_entry_addr, "kern_entry_addr")
        scenario_cfg_lib.os_kern_root_dev_check(self.kern_root_dev, "rootdev")


class VuartTarget:
    """ This is Abstract of class of vm target vuart """
    t_vm_id = []
    t_vuart_id = []

    def __init__(self):
        self.t_vm_id = []

    def style_check_1(self):
        """ This is public method for style check"""
        self.t_vm_id = []

    def style_check_2(self):
        """ This is public method for style check"""
        self.t_vm_id = []


class VuartCfg(VuartTarget):
    """ This is Abstract of class of vm vuart configuration """
    v_type = []
    v_base = []
    v_irq = []
    target = VuartTarget()

    def __init__(self):
        self.v1_type = []

    def style_check_1(self):
        """ This is public method for style check"""
        self.v1_type = []


class VuartInfo:
    """ This is Abstract of class of vm vuart setting """
    v0_vuart = VuartCfg()
    v1_vuart = VuartCfg()

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def style_check_1(self):
        """ This is public method for style check"""
        self.v1_vuart = []

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.v0_vuart = scenario_cfg_lib.get_vuart_info_id(self.scenario_info, 0)
        self.v1_vuart = scenario_cfg_lib.get_vuart_info_id(self.scenario_info, 1)
        scenario_cfg_lib.check_board_private_info()


class MemInfo:
    """ This is Abstract of class of memory setting information """
    mem_start_hpa = {}
    mem_size = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.mem_start_hpa = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "memory", "start_hpa")
        self.mem_size = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "memory", "size")

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.mem_start_hpa_check(self.mem_start_hpa, "start_hpa")
        scenario_cfg_lib.mem_size_check(self.mem_size, "size")


class CfgPci:
    """ This is Abstract of class of PCi devices setting information """
    pci_dev_num = {}
    pci_devs = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_pci_dev_num(self):
        """
        Get pci device number items
        :return: None
        """
        self.pci_dev_num = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "pci_dev_num")

    def get_pci_devs(self):
        """
        Get pci devices items
        :return: None
        """
        self.pci_devs = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "pci_devs")

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.get_pci_dev_num()
        self.get_pci_devs()

    def check_item(self):
        """ Check all items in this class
        :return: None
        """
        scenario_cfg_lib.pci_dev_num_check(self.pci_dev_num, "pci_dev_num")
        scenario_cfg_lib.pci_devs_check(self.pci_devs, "pci_devs")


class EpcSection:
    base = {}
    size = {}

    def __init__(self, scenario_info):
        self.scenario_info = scenario_info

    def get_info(self):
        self.base = scenario_cfg_lib.get_leaf_tag_map(self.scenario_info, "epc_section", "base")
        self.size = scenario_cfg_lib.get_leaf_tag_map(self.scenario_info, "epc_section", "size")


class VmInfo:
    """ This is Abstract of class of VM setting """
    name = {}
    load_order = {}
    uuid = {}
    clos_set = {}
    guest_flag_idx = {}
    cpus_per_vm = {}

    def __init__(self, board_file, scenario_file):
        self.board_info = board_file
        self.scenario_info = scenario_file
        scenario_cfg_lib.VM_COUNT = scenario_cfg_lib.get_vm_num(self.scenario_info)

        self.epc_section = EpcSection(self.scenario_info)
        self.mem_info = MemInfo(self.scenario_info)
        self.os_cfg = CfgOsKern(self.scenario_info)
        self.vuart = VuartInfo(self.scenario_info)
        self.cfg_pci = CfgPci(self.scenario_info)

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.name = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "name")
        self.load_order = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "load_order")
        self.uuid = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "uuid")
        self.guest_flag_idx = scenario_cfg_lib.get_sub_leaf_tag(
            self.scenario_info, "guest_flags", "guest_flag")
        self.cpus_per_vm = scenario_cfg_lib.get_leaf_tag_map(
            self.scenario_info, "pcpu_ids", "pcpu_id")
        self.clos_set = scenario_cfg_lib.get_branch_tag_map(self.scenario_info, "clos")
        self.epc_section.get_info()
        self.mem_info.get_info()
        self.os_cfg.get_info()
        self.vuart.get_info()
        self.cfg_pci.get_info()

    def get_cpu_bitmap(self, index):
        """
        :param index: index list in GUESF_FLAGS
        :return: cpus per vm and their vm id
        """
        return scenario_cfg_lib.cpus_assignment(self.cpus_per_vm, index)

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.vm_name_check(self.name, "name")
        scenario_cfg_lib.load_order_check(self.load_order, "load_order")
        scenario_cfg_lib.uuid_format_check(self.uuid, "uuid")
        scenario_cfg_lib.guest_flag_check(self.guest_flag_idx, "guest_flags", "guest_flag")
        scenario_cfg_lib.cpus_per_vm_check(self.cpus_per_vm, "pcpu_id")

        self.mem_info.check_item()
        self.os_cfg.check_item()
        self.cfg_pci.check_item()
