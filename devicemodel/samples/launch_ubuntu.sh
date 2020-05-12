#!/bin/bash
# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

function launch_ubuntu()
{
vm_name=ubuntu_vm$1

logger_setting="--logger_setting console,level=5;kmsg,level=6;disk,level=5"

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for memsize setting
mem_size=1024M

acrn-dm -A -m $mem_size -s 0:0,hostbridge \
  -s 3,virtio-blk,/home/guestl1/acrn-dm-bins/ubuntuUOS.img \
  -s 4,virtio-net,tap0 \
  -s 5,virtio-console,@stdio:stdio_port \
  -k /home/guestl1/acrn-dm-bins/bzImage_uos \
  -B "earlyprintk=serial,ttyS0,115200n8 consoleblank=0 root=/dev/vda1 rw rootwait maxcpus=1 nohpet console=tty0 console=hvc0 console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M tsc=reliable" \
  $logger_setting \
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

launch_ubuntu 1 "64 448 8"
