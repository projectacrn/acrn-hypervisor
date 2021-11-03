# Copyright (C) 2019 Intel Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
import re

import common
import board_cfg_lib
import launch_cfg_lib
import scenario_cfg_lib

class AcrnDmArgs:
    args = {}

    def __init__(self, board_info, scenario_info, launch_info):
        self.board_info = board_info
        self.scenario_info = scenario_info
        self.launch_info = launch_info

    def get_args(self):
        self.args["user_vm_type"] = common.get_leaf_tag_map(self.launch_info, "user_vm_type")
        self.args["rtos_type"] = common.get_leaf_tag_map(self.launch_info, "rtos_type")
        self.args["mem_size"] = common.get_leaf_tag_map(self.launch_info, "mem_size")
        self.args["vbootloader"] = common.get_leaf_tag_map(self.launch_info, "vbootloader")
        self.args["vuart0"] = common.get_leaf_tag_map(self.launch_info, "vuart0")
        self.args["cpu_sharing"] = common.get_hv_item_tag(self.scenario_info, "FEATURES", "SCHEDULER")
        self.args["pm_channel"] = common.get_leaf_tag_map(self.launch_info, "poweroff_channel")
        self.args["cpu_affinity"] = common.get_leaf_tag_map(self.launch_info, "cpu_affinity", "pcpu_id")
        self.args["shm_enabled"] = common.get_hv_item_tag(self.scenario_info, "FEATURES", "IVSHMEM", "IVSHMEM_ENABLED")
        self.args["shm_regions"] = common.get_leaf_tag_map(self.launch_info, "shm_regions", "shm_region")
        for vmid, shm_regions in self.args["shm_regions"].items():
            if self.args["shm_enabled"] == 'y':
                self.args["shm_regions"][vmid] = [x for x in shm_regions if (x is not None and x.strip != '')]
            else:
                self.args["shm_regions"][vmid] = []
        self.args["xhci"] = common.get_leaf_tag_map(self.launch_info, "usb_xhci")
        self.args["communication_vuarts"] = common.get_leaf_tag_map(self.launch_info, "communication_vuarts", "communication_vuart")
        self.args["console_vuart"] = common.get_leaf_tag_map(self.launch_info, "console_vuart")
        self.args["enable_ptm"] = common.get_leaf_tag_map(self.launch_info, "enable_ptm")
        self.args["allow_trigger_s5"] = common.get_leaf_tag_map(self.launch_info, "allow_trigger_s5")

    def check_item(self):
        (rootfs, num) = board_cfg_lib.get_rootfs(self.board_info)
        launch_cfg_lib.args_aval_check(self.args["user_vm_type"], "user_vm_type", launch_cfg_lib.USER_VM_TYPES)
        launch_cfg_lib.args_aval_check(self.args["rtos_type"], "rtos_type", launch_cfg_lib.RTOS_TYPE)
        launch_cfg_lib.mem_size_check(self.args["mem_size"], "mem_size")
        launch_cfg_lib.args_aval_check(self.args["vbootloader"], "vbootloader", launch_cfg_lib.BOOT_TYPE)
        launch_cfg_lib.args_aval_check(self.args["vuart0"], "vuart0", launch_cfg_lib.DM_VUART0)
        launch_cfg_lib.args_aval_check(self.args["enable_ptm"], "enable_ptm", launch_cfg_lib.y_n)
        launch_cfg_lib.args_aval_check(self.args["allow_trigger_s5"], "allow_trigger_s5", launch_cfg_lib.y_n)
        cpu_affinity = launch_cfg_lib.user_vm_cpu_affinity(self.args["cpu_affinity"])
        err_dic = scenario_cfg_lib.vm_cpu_affinity_check(self.launch_info, cpu_affinity, "pcpu_id")
        launch_cfg_lib.ERR_LIST.update(err_dic)
        launch_cfg_lib.check_shm_regions(self.args["shm_regions"], self.scenario_info)
        launch_cfg_lib.check_console_vuart(self.args["console_vuart"],self.args["vuart0"], self.scenario_info)
        launch_cfg_lib.check_communication_vuart(self.args["communication_vuarts"], self.scenario_info)
        launch_cfg_lib.check_enable_ptm(self.args["enable_ptm"], self.scenario_info)


class AvailablePthru():

    avl = {}

    def __init__(self, board_info):
        self.board_info = board_info
        (self.bdf_desc_map, self.bdf_vpid_map) = board_cfg_lib.get_pci_info(board_info)

    def get_bdf_vpid_map(self):
        return self.bdf_vpid_map

    def get_pci_dev(self):
        self.avl["usb_xdci"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['usb_xdci'])
        self.avl["gpu"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['gpu'])
        self.avl["ipu"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['ipu'])
        self.avl["ipu_i2c"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['ipu_i2c'])
        self.avl["cse"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['cse'])
        self.avl["audio"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['audio'])
        self.avl["audio_codec"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['audio_codec'])
        self.avl["sd_card"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['sd_card'])
        self.avl["wifi"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['wifi'])
        self.avl["ethernet"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['ethernet'])
        self.avl["sata"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['sata'])
        self.avl["nvme"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['nvme'])
        self.avl["bluetooth"] = common.get_avl_dev_info(self.bdf_desc_map, launch_cfg_lib.PT_SUB_PCI['bluetooth'])

    def insert_nun(self):
        self.avl["usb_xdci"].insert(0, '')
        self.avl["gpu"].insert(0, '')
        self.avl["ipu"].insert(0, '')
        self.avl["ipu_i2c"].insert(0, '')
        self.avl["cse"].insert(0, '')
        self.avl["ethernet"].insert(0, '')
        self.avl["sd_card"].insert(0, '')
        self.avl["audio"].insert(0, '')
        self.avl["audio_codec"].insert(0, '')
        self.avl["wifi"].insert(0, '')
        self.avl["sata"].insert(0, '')
        self.avl["nvme"].insert(0, '')
        self.avl["bluetooth"].insert(0, '')


class PthruSelected():

    bdf = {}
    vpid = {}
    slot = {}

    def __init__(self, launch_info, bdf_desc_map, bdf_vpid_map):
        self.launch_info = launch_info
        self.bdf_desc_map = bdf_desc_map
        self.bdf_vpid_map = bdf_vpid_map

    def get_bdf(self):
        self.bdf["usb_xdci"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "usb_xdci")
        self.bdf["gpu"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "gpu")
        self.bdf["ipu"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "ipu")
        self.bdf["ipu_i2c"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "ipu_i2c")
        self.bdf["cse"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "cse")
        self.bdf["ethernet"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "ethernet")
        self.bdf["sd_card"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "sd_card")
        self.bdf["sata"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "sata")
        self.bdf["nvme"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "nvme")
        self.bdf["audio"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "audio")
        self.bdf["audio_codec"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "audio_codec")
        self.bdf["wifi"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "wifi")
        self.bdf["bluetooth"] = launch_cfg_lib.get_bdf_from_tag(self.launch_info, "passthrough_devices", "bluetooth")

    def get_vpid(self):
        self.vpid["usb_xdci"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["usb_xdci"])
        self.vpid["gpu"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["gpu"])
        self.vpid["ipu"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["ipu"])
        self.vpid["ipu_i2c"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["ipu_i2c"])
        self.vpid["cse"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["cse"])
        self.vpid["ethernet"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["ethernet"])
        self.vpid["sd_card"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["sd_card"])
        self.vpid["sata"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["sata"])
        self.vpid["nvme"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["nvme"])
        self.vpid["audio"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["audio"])
        self.vpid["audio_codec"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["audio_codec"])
        self.vpid["wifi"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["wifi"])
        self.vpid["bluetooth"] = launch_cfg_lib.get_vpid_from_bdf(self.bdf_vpid_map, self.bdf["bluetooth"])


    def get_slot(self):
        self.slot["usb_xdci"] = launch_cfg_lib.get_slot(self.bdf["usb_xdci"], "usb_xdci")
        self.slot["ipu"] = launch_cfg_lib.get_slot(self.bdf["ipu"], "ipu")
        self.slot["ipu_i2c"] = launch_cfg_lib.get_slot(self.bdf["ipu_i2c"], "ipu_i2c")
        self.slot["cse"] = launch_cfg_lib.get_slot(self.bdf["cse"], "cse")
        self.slot["ethernet"] = launch_cfg_lib.get_slot(self.bdf["ethernet"], "ethernet")
        self.slot["sd_card"] = launch_cfg_lib.get_slot(self.bdf["sd_card"], "sd_card")
        self.slot["sata"] = launch_cfg_lib.get_slot(self.bdf["sata"], "sata")
        self.slot["nvme"] = launch_cfg_lib.get_slot(self.bdf["nvme"], "nvme")
        self.slot["audio"] = launch_cfg_lib.get_slot(self.bdf["audio"], "audio")
        self.slot["audio_codec"] = launch_cfg_lib.get_slot(self.bdf["audio_codec"], "audio_codec")
        self.slot["wifi"] = launch_cfg_lib.get_slot(self.bdf["wifi"], "wifi")
        self.slot["bluetooth"] = launch_cfg_lib.get_slot(self.bdf["bluetooth"], "bluetooth")

    def check_item(self):
        launch_cfg_lib.pt_devs_check(self.bdf["usb_xdci"], self.vpid["usb_xdci"], "usb_xdci")
        launch_cfg_lib.pt_devs_check(self.bdf["gpu"], self.vpid["gpu"], "gpu")
        launch_cfg_lib.pt_devs_check(self.bdf["ipu"], self.vpid["ipu"], "ipu")
        launch_cfg_lib.pt_devs_check(self.bdf["ipu_i2c"], self.vpid["ipu_i2c"], "ipu_i2c")
        launch_cfg_lib.pt_devs_check(self.bdf["cse"], self.vpid["cse"], "cse")
        launch_cfg_lib.pt_devs_check(self.bdf["ethernet"], self.vpid["ethernet"], "ethernet")
        launch_cfg_lib.pt_devs_check(self.bdf["sd_card"], self.vpid["sd_card"], "sd_card")
        launch_cfg_lib.pt_devs_check(self.bdf["sata"], self.vpid["sata"], "sata")
        launch_cfg_lib.pt_devs_check(self.bdf["nvme"], self.vpid["nvme"], "nvme")
        launch_cfg_lib.pt_devs_check(self.bdf["audio"], self.vpid["audio"], "audio")
        launch_cfg_lib.pt_devs_check(self.bdf["audio_codec"], self.vpid["audio_codec"], "audio_codec")
        launch_cfg_lib.pt_devs_check(self.bdf["wifi"], self.vpid["wifi"], "wifi")
        launch_cfg_lib.pt_devs_check(self.bdf["bluetooth"], self.vpid["bluetooth"], "bluetooth")

        # check connections between several pass-through devices
        launch_cfg_lib.pt_devs_check_audio(self.bdf['audio'], self.bdf['audio_codec'])
        launch_cfg_lib.bdf_duplicate_check(self.bdf)
        launch_cfg_lib.check_slot(self.slot)


class VirtioDeviceSelect():

    dev = {}
    def __init__(self, launch_info):
        self.launch_info = launch_info

    def get_virtio(self):
        self.dev["input"] = common.get_leaf_tag_map(self.launch_info, "virtio_devices", "input")
        self.dev["block"] = common.get_leaf_tag_map(self.launch_info, "virtio_devices", "block")
        self.dev["network"] = common.get_leaf_tag_map(self.launch_info, "virtio_devices", "network")
        self.dev["console"] = common.get_leaf_tag_map(self.launch_info, "virtio_devices", "console")

    def check_virtio(self):
        launch_cfg_lib.check_block_mount(self.dev["block"])


class SriovDeviceInput():

    dev = {}
    def __init__(self, launch_info):
        self.launch_info = launch_info

    def get_sriov(self):
        self.dev["gpu"] = common.get_leaf_tag_map(self.launch_info, "sriov", "gpu")
        temp = common.get_leaf_tag_map(self.launch_info, "sriov", "network")
        temp = {k: v[0] if v[0] else '' for k, v in temp.items()}
        self.dev["network"] = temp

    def check_sriov(self, pt_sel):
        launch_cfg_lib.check_sriov_param(self.dev, pt_sel)
