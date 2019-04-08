#!/bin/bash

if [ ! -b "/dev/mmcblk1p3" ]; then
  echo "no /dev/mmcblk1p3 data partition, exit"
  exit
fi

mkdir -p /data
mount /dev/mmcblk1p3 /data

if [ ! -f "/data/android/android.img" ]; then
  echo "no /data/android/android.img, exit"
  exit
fi

# create a unique tap device for each VM
tap=tap_AaaG
tap_exist=$(ip a | grep "$tap" | awk '{print $1}')
if [ "$tap_exist"x != "x" ]; then
  echo "tap device existed, reuse $tap"
else
  ip tuntap add dev $tap mode tap
fi

# if acrn-br0 exists, add VM's unique tap device under it
br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')
if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
  ip link set "$tap" master acrn-br0
  ip link set dev "$tap" down
  ip link set dev "$tap" up
fi

modprobe pci_stub

# offline SOS CPUs except BSP before launch UOS
for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do
	online=`cat $i/online`
	idx=`echo $i | tr -cd "[1-99]"`
	echo cpu$idx online=$online
	if [ "$online" = "1" ]; then
		echo 0 > $i/online
		echo $idx > /sys/class/vhm/acrn_vhm/offline_cpu
	fi
done
