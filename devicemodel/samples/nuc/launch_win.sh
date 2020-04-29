#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

function launch_win()
{
vm_name=win_vm$1

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for memsize setting
mem_size=4096M

#add GPU passthrough device
gpudevice=`cat /sys/bus/pci/devices/0000:00:02.0/device`

echo "8086 $gpudevice" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:02.0" > /sys/bus/pci/devices/0000:00:02.0/driver/unbind
echo "0000:00:02.0" > /sys/bus/pci/drivers/pci-stub/bind

acrn-dm -A -m $mem_size -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 2,passthru,0/2/0,gpu \
  -s 3,virtio-blk,./win10-ltsc.img \
  -s 4,virtio-net,tap0 \
  --ovmf /usr/share/acrn/bios/OVMF.fd \
  --windows \
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
                echo $idx > /sys/class/vhm/acrn_vhm/offline_cpu
        fi
done

launch_win 1 "64 448 8"
