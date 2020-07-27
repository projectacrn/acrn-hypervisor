#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

offline_path="/sys/class/vhm/acrn_vhm"

# Check the device file of /dev/acrn_hsm to determine the offline_path
if [ -e "/dev/acrn_hsm" ]; then
offline_path="/sys/class/acrn/acrn_hsm"
fi


function run_container()
{
vm_name=vm1
config_src="/usr/share/acrn/samples/nuc/runC.json"
shell="/usr/share/acrn/conf/add/$vm_name.sh"
arg_file="/usr/share/acrn/conf/add/$vm_name.args"
runc_bundle="/usr/share/acrn/conf/add/runc/$vm_name"
rootfs_dir="/usr/share/acrn/conf/add/runc/rootfs"
config_dst="$runc_bundle/config.json"


input=$(runc list -f table | awk '{print $1}''{print $3}')
arr=(${input// / })

for((i=0;i<${#arr[@]};i++))
do
	if [ "$vm_name" = "${arr[$i]}" ]; then
		if [ "running" = "${arr[$i+1]}" ]; then
			echo "runC instance ${arr[$i]} is running"
			exit
		else
			runc kill ${arr[$i]}
			runc delete ${arr[$i]}
		fi
	fi
done
vmsts=$(acrnctl list)
vms=(${vmsts// / })
for((i=0;i<${#vms[@]};i++))
do
	if [ "$vm_name" = "${vms[$i]}" ]; then
		if [ "stopped" != "${vms[$i+1]}" ]; then
			echo "Uos ${vms[$i]} ${vms[$i+1]}"
			acrnctl stop ${vms[$i]}
	        fi
	fi
done


if [ ! -f "$shell" ]; then
	echo "Pls add the vm at first!"
	exit
fi

if [ ! -f "$arg_file" ]; then
	echo "Pls add the vm args!"
	exit
fi


if [ ! -d "$rootfs_dir" ]; then
	mkdir -p "$rootfs_dir"
fi
if [ ! -d "$runc_bundle" ]; then
	mkdir -p "$runc_bundle"
fi
if [ ! -f "$config_dst" ]; then
	cp  "$config_src"  "$config_dst"
	args=$(sed '{s/-C//g;s/^[ \t]*//g;s/^/\"/;s/ /\",\"/g;s/$/\"/}' ${arg_file})
	sed -i "s|\"sh\"|\"$shell\", $args|" $config_dst
fi
runc run --bundle $runc_bundle -d $vm_name
echo "The runC container is running in backgroud"
echo "'#runc exec <vmname> bash' to login the container bash"
exit
}

function launch_clear()
{
mac=$(cat /sys/class/net/e*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name}

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep -w "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#logger_setting, format: logger_name,level; like following
logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"

#for pm by vuart setting
pm_channel="--pm_notify_channel uart "
pm_by_vuart="--pm_by_vuart pty,/run/acrn/life_mngr_"$vm_name
pm_vuart_node=" -s 1:0,lpc -l com2,/run/acrn/life_mngr_"$vm_name

#for memsize setting
mem_size=2048M

acrn-dm -A -m $mem_size -s 0:0,hostbridge \
  -s 2,pci-gvt -G "$2" \
  -s 5,virtio-console,@stdio:stdio_port \
  -s 6,virtio-hyper_dmabuf \
  -s 3,virtio-blk,/home/clear/uos/uos.img \
  -s 4,virtio-net,tap0 \
  -s 7,virtio-rnd \
  --ovmf /usr/share/acrn/bios/OVMF.fd \
  $pm_channel $pm_by_vuart $pm_vuart_node \
  $logger_setting \
  --mac_seed $mac_seed \
  $vm_name
}

#add following cmdline to grub.cfg and update kernel
#when launching LaaG by OVMF
#rw rootwait maxcpus=1 nohpet console=tty0 console=hvc0
#console=ttyS0 no_timer_check ignore_loglevel
#log_buf_len=16M consoleblank=0
#tsc=reliable i915.avail_planes_per_pipe="64 448 8"
#i915.enable_hangcheck=0 i915.nuclear_pageflip=1
#i915.enable_guc_loading=0
#i915.enable_guc_submission=0 i915.enable_guc=0

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
                echo $idx > ${offline_path}/offline_cpu
        fi
done


if [ "$1" = "-C" ];then
	echo "runc_container"
	run_container
else
	launch_clear 1 "64 448 8" 0x070F00
fi
