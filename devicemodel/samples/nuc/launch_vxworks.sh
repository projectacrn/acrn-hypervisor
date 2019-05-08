#!/bin/bash

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
#      Here we just launch VxWorks as a normal vm without lapic_pt. If you want try it
#      with lapic_pt, you should add the following options and make sure the front-end virtio-console
#      driver use polling mode (This feature is not supported by VxWorks offically now and should
#      implement it by youself).
#
#      --virtio_poll 1000000 \
#      --s 2, virtio-console,@pty:pty_port \
#
#      Once the front-end polling mode virtio-console get supported by VxWorks offically, we will
#      add the lapic_pt option.
acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 5,virtio-console,@pty:pty_port \
  -s 3,virtio-blk,./VxWorks.img \
  --ovmf ./OVMF.fd \
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

launch_vxworks 1 1
