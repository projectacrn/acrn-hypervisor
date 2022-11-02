# Copyright (C) 2019-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import acrn_config_utilities
import board_cfg_lib
import scenario_cfg_lib

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
        self.processor_val = board_cfg_lib.get_processor_info()
        return self.processor_val

    def get_rootdev_val(self):
        """
        Get root devices from board info
        :return: root devices list
        """
        (self.root_dev_val, num) = acrn_config_utilities.get_rootfs(self.board_info)
        return self.root_dev_val

    def get_ttys_val(self):
        """
        Get ttySn from board info
        :return: serial console list
        """
        self.ttys_val = board_cfg_lib.get_native_ttys_info(self.board_info)
        return self.ttys_val

    def get_clos_val(self):
        """
        Get clos max number from board info
        :return: clos support list
        """
        self.clos_val = []

        (rdt_resources, rdt_res_clos_max, _) = board_cfg_lib.clos_info_parser(self.board_info)
        if len(rdt_resources) != 0 and len(rdt_res_clos_max) != 0:
            common_clos_max = min(rdt_res_clos_max)

            for i_cnt in range(common_clos_max):
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
    kern_load_addr = {}
    kern_entry_addr = {}
    ramdisk_mod = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.kern_name = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "os_config", "name")
        self.kern_type = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_type")
        self.kern_mod = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_mod")
        self.kern_args = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "bootargs")
        self.kern_load_addr = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_load_addr")
        self.kern_entry_addr = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "kern_entry_addr")
        self.ramdisk_mod = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "os_config", "ramdisk_mod")

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.os_kern_name_check(self.kern_name, "os_config", "name")
        scenario_cfg_lib.os_kern_type_check(self.kern_type, "os_config", "kern_type")
        scenario_cfg_lib.os_kern_mod_check(self.kern_mod, "os_config", "kern_mod")
        scenario_cfg_lib.os_kern_load_addr_check(self.kern_type, self.kern_load_addr, "os_config", "kern_load_addr")
        scenario_cfg_lib.os_kern_entry_addr_check(self.kern_type, self.kern_entry_addr, "os_config", "kern_entry_addr")


class VuartInfo:
    """ This is Abstract of class of vm vuart setting """
    v0_vuart = {}
    v1_vuart = {}
    pci_vuarts = {}

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
        self.v0_vuart = acrn_config_utilities.get_vuart_info_id(self.scenario_info, 0)
        self.v1_vuart = acrn_config_utilities.get_vuart_info_id(self.scenario_info, 1)
        self.pci_vuarts = acrn_config_utilities.get_vuart_info(self.scenario_info)

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.check_vuart(self.v0_vuart, self.v1_vuart)
        scenario_cfg_lib.check_pci_vuart(self.pci_vuarts, self.v0_vuart, self.v1_vuart)

class MemInfo:
    """ This is Abstract of class of memory setting information """
    mem_start_hpa = {}
    mem_size = {}
    mem_start_hpa2 = {}
    mem_size_hpa2 = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.mem_start_hpa = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "memory", "start_hpa")
        self.mem_size = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "memory", "size")
        self.mem_start_hpa2 = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "memory", "start_hpa2")
        self.mem_size_hpa2 = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "memory", "size_hpa2")

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.mem_start_hpa_check(self.mem_start_hpa, "memory", "start_hpa")
        scenario_cfg_lib.mem_size_check(self.mem_size, "memory", "size")
        scenario_cfg_lib.mem_start_hpa_check(self.mem_start_hpa2, "memory", "start_hpa2")
        scenario_cfg_lib.mem_size_check(self.mem_size_hpa2, "memory", "size_hpa2")


class CfgPci:
    """ This is Abstract of class of PCi devices setting information """
    pt_pci_num = {}
    pci_devs = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_pt_pci_dev_num(self):
        """
        Get pci device number items
        :return: None
        """
        self.pt_pci_num = scenario_cfg_lib.get_pt_pci_num(self.pci_devs)

    def get_pt_pci_devs(self):
        """
        Get pci devices items
        :return: None
        """
        pci_items = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "pci_devs", "pci_dev")
        self.pci_devs = scenario_cfg_lib.get_pt_pci_devs(pci_items)


    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.get_pt_pci_devs()
        self.get_pt_pci_dev_num()

    def check_item(self):
        """ Check all items in this class
        :return: None
        """
        scenario_cfg_lib.pci_devs_check(self.pci_devs, "pci_devs", "pci_dev")


class EpcSection:
    base = {}
    size = {}

    def __init__(self, scenario_info):
        self.scenario_info = scenario_info

    def get_info(self):
        self.base = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "epc_section", "base")
        self.size = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "epc_section", "size")


class ShareMem:
    """ This is the class to get Share Memory regions for VMs """
    shmem_enabled = 'n'
    raw_shmem_regions = []
    shmem_regions = {}
    shmem_num = {}

    def __init__(self, scenario_info):
        self.scenario_info = scenario_info

    def set_ivshmem(self, ivshmem_regions):
        """
        set ivshmem regions for VMs.
        :param ivshmem_regions:
        :return:
        """
        self.raw_shmem_regions = ivshmem_regions
        self.shmem_enabled = acrn_config_utilities.get_hv_item_tag(self.scenario_info, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")
        self.shmem_regions = scenario_cfg_lib.get_shmem_regions(ivshmem_regions)
        self.shmem_num = scenario_cfg_lib.get_shmem_num(self.shmem_regions)

    def check_items(self):
        '''
        check the configurations for share memories.
        :return:
        '''
        if self.shmem_enabled == 'y':
            vm_type_info = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "vm_type")
            scenario_cfg_lib.share_mem_check(self.shmem_regions, self.raw_shmem_regions, vm_type_info,
                                         "FEATURES", "IVSHMEM", "IVSHMEM_REGION")


class LoadOrderNum:
    """ This is Abstract of VM number for different load order """
    def __init__(self):
        self.pre_vm = 0
        self.sos_vm = 0
        self.post_vm = 0

    def get_info(self, load_vm):
        self.pre_vm = scenario_cfg_lib.get_load_vm_cnt(load_vm, "PRE_LAUNCHED_VM")
        self.sos_vm = scenario_cfg_lib.get_load_vm_cnt(load_vm, "SERVICE_VM")
        self.post_vm = scenario_cfg_lib.get_load_vm_cnt(load_vm, "POST_LAUNCHED_VM")


class MmioResourcesInfo:
    """ This is Abstract of class of mmio resource setting information """
    p2sb = False

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.p2sb = acrn_config_utilities.get_leaf_tag_map_bool(self.scenario_info, "mmio_resources", "p2sb")
        self.tpm2 = acrn_config_utilities.get_leaf_tag_map_bool(self.scenario_info, "mmio_resources", "TPM2")

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.check_p2sb(self.p2sb)


class PtIntxInfo:
    """ This is Abstract of class of pt intx setting information """
    phys_gsi = {}
    virt_gsi = {}

    def __init__(self, scenario_file):
        self.scenario_info = scenario_file

    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.phys_gsi, self.virt_gsi = acrn_config_utilities.get_pt_intx_table(self.scenario_info)

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """

        scenario_cfg_lib.check_pt_intx(self.phys_gsi, self.virt_gsi)


class VmInfo:
    """ This is Abstract of class of VM setting """
    name = {}
    load_vm = {}
    clos_per_vm = {}
    guest_flags = {}
    cpus_per_vm = {}

    def __init__(self, board_file, scenario_file):
        self.board_info = board_file
        self.scenario_info = scenario_file
        acrn_config_utilities.get_vm_num(self.scenario_info)

        self.epc_section = EpcSection(self.scenario_info)
        self.mem_info = MemInfo(self.scenario_info)
        self.os_cfg = CfgOsKern(self.scenario_info)
        self.vuart = VuartInfo(self.scenario_info)
        self.cfg_pci = CfgPci(self.scenario_info)
        self.load_order_cnt = LoadOrderNum()
        self.shmem = ShareMem(self.scenario_info)
        self.mmio_resource_info = MmioResourcesInfo(self.scenario_info)
        self.pt_intx_info = PtIntxInfo(self.scenario_info)


    def get_info(self):
        """
        Get all items which belong to this class
        :return: None
        """
        self.name = acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "name")
        self.load_vm= acrn_config_utilities.get_leaf_tag_map(self.scenario_info, "vm_type")
        self.guest_flags = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "guest_flags", "guest_flag")
        self.cpus_per_vm = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "cpu_affinity", "pcpu_id")
        self.clos_per_vm = acrn_config_utilities.get_leaf_tag_map(
            self.scenario_info, "clos", "vcpu_clos")

        self.epc_section.get_info()
        self.mem_info.get_info()
        self.os_cfg.get_info()
        self.vuart.get_info()
        self.cfg_pci.get_info()
        self.load_order_cnt.get_info(self.load_vm)
        self.mmio_resource_info.get_info()
        self.pt_intx_info.get_info()

    def set_ivshmem(self, ivshmem_regions):
        """
        set ivshmem regions for VMs
        :param ivshmem_regions:
        :return:
        """
        self.shmem.set_ivshmem(ivshmem_regions)

    def get_cpu_bitmap(self, index):
        """
        :param index: index list in GUESF_FLAGS
        :return: cpus per vm and their vm id
        """
        return scenario_cfg_lib.cpus_assignment(self.cpus_per_vm, index)

    def get_clos_bitmap(self, index):
        """
        :param index: index list in GUESF_FLAGS
        :return: clos per vm and their vm id
        """
        return scenario_cfg_lib.clos_assignment(self.clos_per_vm, index)

    def check_item(self):
        """
        Check all items in this class
        :return: None
        """
        scenario_cfg_lib.vm_name_check(self.name, "name")
        scenario_cfg_lib.load_vm_check(self.load_vm, "load_vm")
        scenario_cfg_lib.guest_flag_check(self.guest_flags, "guest_flags", "guest_flag")
        err_dic = scenario_cfg_lib.vm_cpu_affinity_check(self.scenario_info, None, self.cpus_per_vm)
        scenario_cfg_lib.vcpu_clos_check(self.cpus_per_vm, self.clos_per_vm, self.guest_flags, "clos", "vcpu_clos")

        self.mem_info.check_item()
        self.os_cfg.check_item()
        self.cfg_pci.check_item()
        self.vuart.check_item()
        self.shmem.check_items()
        self.mmio_resource_info.check_item()
        self.pt_intx_info.check_item()
        scenario_cfg_lib.ERR_LIST.update(err_dic)
