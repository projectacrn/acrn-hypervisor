#!/usr/bin/env python3
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import sys, os
import argparse
import lxml.etree as etree
import logging
import re
import copy

def eval_xpath(element, xpath, default_value=None):
    return next(iter(element.xpath(xpath)), default_value)

class LaunchScript:
    script_template_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "launch_script_template.sh")

    class VirtualBDFAllocator:
        def __init__(self):
            # Reserved slots:
            #    0 - For (virtual) hostbridge
            #    1 - For (virtual) LPC
            #    2 - For passthrough integarted GPU (either PF or VF)
            #   31 - For LPC bridge needed by integrated GPU
            self._free_slots = list(range(3, 30))

        def get_virtual_bdf(self, device_etree = None, options = None):
            if device_etree is not None:
                bus = eval_xpath(device_etree, "../@address")
                vendor_id = eval_xpath(device_etree, "vendor/text()")
                class_code = eval_xpath(device_etree, "class/text()")

                # VGA-compatible controller, either integrated or discrete GPU
                if class_code == "0x030000":
                    return 2

            if options:
                if "igd" in options:
                    return 2

            next_vbdf = self._free_slots.pop(0)
            return next_vbdf

    class PassThruDeviceOptions:
        passthru_device_options = {
            # "0x0200": ["enable_ptm"],  # Ethernet controller, added if PTM is enabled for the VM
        }

        def _add_option(self, class_code, option):
            current_option = self._options.setdefault(class_code, [])
            self._options[class_code] = current_option.append("enable_ptm")

        def __init__(self, vm_launch_etree):
            self._options = copy.copy(self.passthru_device_options)
            if eval_xpath(vm_launch_etree, ".//enable_ptm/text()") == "y":
                self._add_option("0x0200", "enable_ptm")

        def get_option(self, device_etree):
            passthru_options = []
            if device_etree is not None:
                class_code = eval_xpath(device_etree, "class/text()", "")
                for k,v in self._options.items():
                    if class_code.startswith(k):
                        passthru_options.extend(v)
            return ",".join(passthru_options)

    def __init__(self, board_etree, vm_name, vm_launch_etree):
        self._board_etree = board_etree
        self._vm_launch_etree = vm_launch_etree

        self._vm_name = vm_name
        self._vm_descriptors = {}
        self._init_commands = []
        self._dm_parameters = []
        self._deinit_commands = []

        self._vbdf_allocator = self.VirtualBDFAllocator()
        self._passthru_options = self.PassThruDeviceOptions(vm_launch_etree)

    def add_vm_descriptor(self, name, value):
        self._vm_descriptors[name] = value

    def add_init_command(self, command):
        if command not in self._init_commands:
            self._init_commands.append(command)

    def add_deinit_command(self, command):
        if command not in self._deinit_commands:
            self._deinit_commands.append(command)

    def add_plain_dm_parameter(self, opt):
        full_opt = f"\"{opt}\""
        if full_opt not in self._dm_parameters:
            self._dm_parameters.append(full_opt)

    def add_dynamic_dm_parameter(self, cmd, opt=""):
        full_cmd = f"{cmd:40s} {opt}".strip()
        full_opt = f"`{full_cmd}`"
        if full_opt not in self._dm_parameters:
            self._dm_parameters.append(full_opt)

    def to_string(self):
        s = ""

        with open(self.script_template_path, "r") as f:
            s += f.read()

        s += """
###
# The followings are generated by launch_cfg_gen.py
###
"""
        s += "\n"

        s += "# Defining variables that describe VM types\n"
        for name, value in self._vm_descriptors.items():
            s += f"{name}={value}\n"
        s += "\n"

        s += "# Initializing\n"
        for command in self._init_commands:
            s += f"{command}\n"
        s += "\n"

        s += "# Invoking ACRN device model\n"
        s += "dm_params=(\n"
        for param in self._dm_parameters:
            s += f"    {param}\n"
        s += ")\n\n"

        s += "echo \"Launch device model with parameters: ${dm_params[*]}\"\n"
        s += "acrn-dm ${dm_params[*]}\n\n"

        s += "# Deinitializing\n"
        for command in self._deinit_commands:
            s += f"{command}\n"

        return s

    def write_to_file(self, path):
        with open(path, "w") as f:
            f.write(self.to_string())
            logging.info(f"Successfully generated launch script {path} for VM '{self._vm_name}'.")

    def add_virtual_device(self, kind, vbdf=None, options=""):
        if "virtio" in kind and eval_xpath(self._vm_launch_etree, ".//rtos_type/text()", "no") != "no":
            self.add_plain_dm_parameter("--virtio_poll 1000000")

        if vbdf is None:
            vbdf = self._vbdf_allocator.get_virtual_bdf()
        self.add_dynamic_dm_parameter("add_virtual_device", f"{vbdf} {kind} {options}")

    def add_passthru_device(self, bus, dev, fun, options=""):
        device_etree = eval_xpath(self._board_etree, f"//bus[@type='pci' and @address='0x{bus:x}']/device[@address='0x{(dev << 16) | fun:x}']")
        if not options:
            options = self._passthru_options.get_option(device_etree)

        vbdf = self._vbdf_allocator.get_virtual_bdf(device_etree, options)
        self.add_dynamic_dm_parameter("add_passthrough_device", f"{vbdf} 0000:{bus:02x}:{dev:02x}.{fun} {options}")

        # Enable interrupt storm monitoring if the VM has any passthrough device other than the integrated GPU (whose
        # vBDF is fixed to 2)
        if vbdf != 2:
            self.add_dynamic_dm_parameter("add_interrupt_storm_monitor", "10000 10 1 100")

    def has_dm_parameter(self, fn):
        try:
            next(filter(fn, self._dm_parameters))
            return True
        except StopIteration:
            return False

def cpu_id_to_lapic_id(board_etree, vm_name, cpus):
    ret = []

    for cpu in cpus:
        lapic_id = eval_xpath(board_etree, f"//processors//thread[cpu_id='{cpu}']/apic_id/text()", None)
        if lapic_id is not None:
            ret.append(int(lapic_id, 16))
        else:
            logging.warning(f"CPU {cpu} is not defined in the board XML, so it can't be available to VM {vm_name}")

    return ret

def generate_for_one_vm(board_etree, vm_scenario_etree, vm_launch_etree, vm_id):
    vm_name = eval_xpath(vm_launch_etree, ".//vm_name/text()", f"ACRN Post-Launched VM")
    script = LaunchScript(board_etree, vm_name, vm_launch_etree)

    script.add_init_command("probe_modules")

    ###
    # VM types and guest OSes
    ###

    if eval_xpath(vm_launch_etree, ".//user_vm_type/text()") == "WINDOWS":
        script.add_plain_dm_parameter("--windows")
    script.add_vm_descriptor("rtos_type", f"'{eval_xpath(vm_launch_etree, './/rtos_type/text()', 'no')}'")

    ###
    # CPU and memory resources
    ###
    cpus_in_launch_xml = set(vm_launch_etree.xpath(".//cpu_affinity/pcpu_id[text() != '']/text()"))
    cpus_in_scenario_xml = set(vm_scenario_etree.xpath(".//cpu_affinity/pcpu_id[text() != '']/text()"))
    if cpus_in_launch_xml:
        cpus = cpus_in_scenario_xml & cpus_in_launch_xml
        if not cpus:
            logging.error(f"CPUs assigned to VM '{vm_name}' in the launch XML are outside of those allowed by the scenario XML.")
    else:
        cpus = cpus_in_scenario_xml
        if not cpus:
            logging.error(f"VM '{vm_name}' has no CPU assigned in either the scenario or the launch XML.")
    lapic_ids = cpu_id_to_lapic_id(board_etree, vm_name, cpus)
    if lapic_ids:
        script.add_dynamic_dm_parameter("add_cpus", f"{' '.join([str(x) for x in sorted(lapic_ids)])}")

    script.add_plain_dm_parameter(f"-m {eval_xpath(vm_launch_etree, './/mem_size/text()')}M")

    if eval_xpath(vm_scenario_etree, "//SSRAM_ENABLED") == "y" and \
       eval_xpath(vm_launch_etree, ".//user_vm_type/text()") == "PREEMPT-RT LINUX":
        script.add_plain_dm_parameter("--ssram")

    ###
    # Guest BIOS
    ###
    if eval_xpath(vm_launch_etree, ".//vbootloader/text()") == "ovmf":
        script.add_plain_dm_parameter("--ovmf /usr/share/acrn/bios/OVMF.fd")

    ###
    # Devices
    ###

    # Emulated platform devices
    if eval_xpath(vm_launch_etree, ".//user_vm_type/text()") != "PREEMPT-RT LINUX":
        script.add_virtual_device("lpc", vbdf="1:0")

    if eval_xpath(vm_launch_etree, ".//vuart0/text()") == "Enable":
        script.add_plain_dm_parameter("-l com1,stdio")

    # Emulated PCI devices
    script.add_virtual_device("hostbridge", vbdf="0:0")

    if eval_xpath(vm_scenario_etree, "//IVSHMEM_ENABLED/text()") == "y":
        for ivshmem in vm_launch_etree.xpath("//shm_region[text() != '']/text()"):
            script.add_virtual_device("ivshmem", options=ivshmem)

    if eval_xpath(vm_launch_etree, ".//console_vuart/text()") == "Enable":
        script.add_virtual_device("uart", options="vuart_idx:0")

    for comm_vuart in vm_launch_etree.xpath(".//communication_vuart/@id"):
        script.add_virtual_device("uart", options=f"vuart_idx:{comm_vuart}")

    # Mediated PCI devices, including virtio
    for usb_xhci in vm_launch_etree.xpath(".//usb_xhci[text() != '']/text()"):
        script.add_virtual_device("xhci", options=usb_xhci)

    for virtio_input in vm_launch_etree.xpath(".//virtio_devices/input[text() != '']/text()"):
        script.add_virtual_device("virtio-input", options=virtio_input)

    for virtio_console in vm_launch_etree.xpath(".//virtio_devices/console[text() != '']/text()"):
        script.add_virtual_device("virtio-console", options=virtio_console)

    for virtio_network in vm_launch_etree.xpath(".//virtio_devices/network[text() != '']/text()"):
        params = virtio_network.split(",", maxsplit=1)
        tap_conf = f"tap={params[0]}"
        params = [tap_conf] + params[1:]
        script.add_init_command(f"mac=$(cat /sys/class/net/e*/address)")
        params.append(f"mac_seed=${{mac:0:17}}-{vm_name}")
        script.add_virtual_device("virtio-net", options=",".join(params))

    for virtio_block in vm_launch_etree.xpath(".//virtio_devices/block[text() != '']/text()"):
        params = virtio_block.split(":", maxsplit=1)
        if len(params) == 1:
            script.add_virtual_device("virtio-blk", options=virtio_block)
        else:
            block_device = params[0]
            rootfs_img = params[1]
            var = f"dir_{os.path.basename(block_device)}"
            script.add_init_command(f"{var}=`mount_partition {block_device}`")
            script.add_virtual_device("virtio-blk", options=os.path.join(f"${{{var}}}", rootfs_img))
            script.add_deinit_command(f"unmount_partition ${{{var}}}")

    # Passthrough PCI devices
    bdf_regex = re.compile("([0-9a-f]{2}):([0-1][0-9a-f]).([0-7])")
    for passthru_device in vm_launch_etree.xpath(".//passthrough_devices/*/text()"):
        m = bdf_regex.match(passthru_device)
        if not m:
            continue
        script.add_passthru_device(int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16))

    for sriov_gpu_device in vm_launch_etree.xpath(".//sriov/gpu/text()"):
        m = bdf_regex.match(sriov_gpu_device)
        if not m:
            continue
        script.add_passthru_device(int(m.group(1), 16), int(m.group(2), 16), int(m.group(3), 16), options="igd-vf")

    ###
    # Miscellaneous
    ###
    script.add_dynamic_dm_parameter("add_rtvm_options")
    script.add_dynamic_dm_parameter("add_logger_settings", "console=4 kmsg=3 disk=5")

    ###
    # Lastly, conclude the device model parameters with the VM name
    ###
    script.add_plain_dm_parameter(f"{vm_name}")

    return script

def main(board_xml, scenario_xml, launch_xml, user_vm_id, out_dir):
    board_etree = etree.parse(board_xml)
    scenario_etree = etree.parse(scenario_xml)
    launch_etree = etree.parse(launch_xml)

    service_vm_id = eval_xpath(scenario_etree, "//vm[vm_type='SERVICE_VM']/@id")
    post_vms = scenario_etree.xpath("//vm[starts-with(vm_type, 'POST_')]")
    if service_vm_id is None and len(post_vms) > 0:
        logging.error("The scenario does not define a service VM so no launch scripts will be generated for the post-launched VMs in the scenario.")
        return 1
    service_vm_id = int(service_vm_id)

    try:
        os.mkdir(out_dir)
    except FileExistsError:
        if os.path.isfile(out_dir):
            logging.error(f"Cannot create output directory {out_dir}: File exists")
            return 1
    except Exception as e:
        logging.error(f"Cannot create output directory: {e}")
        return 1

    if user_vm_id == 0:
        post_vm_ids = [int(vm_scenario_etree.get("id")) - service_vm_id for vm_scenario_etree in post_vms]
    else:
        post_vm_ids = [user_vm_id]

    for post_vm_id in post_vm_ids:
        vm_scenario_etree = eval_xpath(scenario_etree, f"//vm[@id = {service_vm_id + post_vm_id}]")
        vm_launch_etree = eval_xpath(launch_etree, f"//user_vm[@id='{post_vm_id}']")
        if vm_scenario_etree is None:
            logging.warning(f"Post-launched VM {post_vm_id} is not specified in the scenario XML, so no launch script will be generated.")
            continue

        if vm_launch_etree is None:
            logging.warning(f"Post-launched VM {post_vm_id} is not specified in the launch XML, so no launch script will be generated.")
            continue

        script = generate_for_one_vm(board_etree, vm_scenario_etree, vm_launch_etree, post_vm_id)
        script.write_to_file(os.path.join(out_dir, f"launch_user_vm_id{post_vm_id}.sh"))

    return 0

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", help="the XML file summarizing characteristics of the target board")
    parser.add_argument("--scenario", help="the XML file specifying the scenario to be set up")
    parser.add_argument("--launch", help="the XML file specifying the parameters of post-launched VMs")
    parser.add_argument("--user_vmid", type=int, default=0, help="the post-launched VM ID (as is specified in the launch XML) whose launch script is to be generated, or 0 if all post-launched VMs shall be processed")
    parser.add_argument("--out", default="output", help="path to the directory where generated scripts are placed")
    args = parser.parse_args()

    logging.basicConfig(level="INFO")

    sys.exit(main(args.board, args.scenario, args.launch, args.user_vmid, args.out))
