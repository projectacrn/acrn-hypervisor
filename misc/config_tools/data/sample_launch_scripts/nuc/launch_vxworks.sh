#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

function launch_vxworks()
{
vm_name=vxworks_vm$1

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep -w "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for memsize setting
mem_size=2048M

# Note:
#      Here we just launch VxWorks as a normal vm without lapic pt. If you want try it
#      with lapic pt, you should add the following options and make sure the front-end virtio-console
#      driver use polling mode (This feature is not supported by VxWorks offically now and should
#      be implemented it by youself).
#
#      --lapic_pt \
#      --virtio_poll 1000000 \
#      -U $rtvm_uuid \
#
#      Once the front-end polling mode virtio-console get supported by VxWorks offically, we will
#      set the lapic_pt option as default.
acrn-dm -A -m $mem_size -s 0:0,hostbridge \
  -s 5,virtio-console,@stdio:stdio_port \
  -s 3,virtio-blk,./VxWorks.img \
  --ovmf /usr/share/acrn/bios/OVMF.fd \
  $vm_name
}

# offline SOS CPUs except BSP before launch UOS
for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do
        online=`cat $i/online`
        idx=`echo $i | tr -cd "[1-99]"`
        echo cpu$idx online=$online
        if [ "$online" = "1" ]; then
                echo 0 > $i/online
		# during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod
		while [ "$online" = "1" ]; do
			sleep 1
			echo 0 > $i/online
			online=`cat $i/online`
		done
                echo $idx > /sys/devices/virtual/misc/acrn_hsm/remove_cpu
        fi
done

launch_vxworks 1
