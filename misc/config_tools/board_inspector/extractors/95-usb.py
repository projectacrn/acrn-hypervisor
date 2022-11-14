# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

import os, re, logging

from extractors.helpers import add_child, get_node

USB_DEVICES_PATH = "/sys/bus/usb/devices"
USB_DEVICES_REGEX = r"^\d-\d$"   # only devices connecting to root hub

def extract(args, board_etree):
    dev_regex = re.compile(USB_DEVICES_REGEX)
    for dev in os.listdir(USB_DEVICES_PATH):
        m = dev_regex.match(dev)
        if m:
            d = m.group(0)
            devpath = os.path.join(USB_DEVICES_PATH, d)
            try:
                with open(os.path.join(devpath, 'devnum'), 'r') as f:
                    devnum = f.read().strip()
                with open(os.path.join(devpath, 'busnum'), 'r') as f:
                    busnum = f.read().strip()
                cmd_out = os.popen('lsusb -s {b}:{d}'.format(b=busnum, d=devnum)).read()
                desc = cmd_out.split(':', maxsplit=1)[1].strip('\n')

                with open(devpath + '/port/firmware_node/path') as f:
                    acpi_path = f.read().strip()
                usb_port_node = get_node(board_etree, f"//device[acpi_object='{acpi_path}']")
                if usb_port_node is not None:
                    add_child(usb_port_node, "usb_device", location=d,
                            description=d + desc)
            except Exception as e:
                logging.debug(f"{e}: please check if a USB device has been removed form usb port {d}.")
                pass
