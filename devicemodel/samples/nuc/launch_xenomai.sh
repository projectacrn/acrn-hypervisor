#!/bin/bash
# Copyright (C) 2020 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

# set -x

usage()
{
	echo "$0 [-b disk | -f img] [-k kernel.tar.gz] {-n ethernet} {-B mem_size} {-r}"
	echo "   example:"
	echo "   $0 -b /dev/sdb3 -k ~/linux-4.19.tar.gz -n eth0"
	echo "   $0 -f clearlinux.img -k ../linux-5.2.tar -n eth0 -B 512M"
	echo "   $0 -f clearlinux.img -k ../linux-5.2.tar -n eth0 -r # restore passthroughed eth0 when script returns"
	exit
}

function get_bdf()
{
	IFS=/
	for dir in $1; do
	    case $dir in 0000:??:??.?)
		bdf=$dir;;
	    esac
	done

	unset IFS
	echo $bdf
}

modprobe pci_stub
# pci devices for passthru
dev_opts=
declare -A passthru_devs
guest_pci_nr=2

function passthru_pci_devs()
{
	for dev in "${passthrus[@]}"
	do
		dev_info=$(find /sys/devices/pci* -name $(basename $dev))
		dev_bdf=$(get_bdf $dev_info)
		dev_path=$(echo $dev_info | sed "s/$dev_bdf\(.*\)//g")$dev_bdf
		dev_vendor_id=$(cat $dev_path/vendor | sed 's/0x//g')
		dev_device_id=$(cat $dev_path/device | sed 's/0x//g')
		dev_bus=$(echo ${dev_bdf#0000:}|sed "s/:.*//g")
		dev_dev=$(echo $dev_bdf|sed "s/.*://g"|sed "s/\..*//g")
		dev_fun=$(echo $dev_bdf|sed "s/.*\.//g")
		dev_drv=$(lspci -k -s $dev_bdf | grep "Kernel driver in use"|sed "s/.*: //g")
		dev_passthru_opt="-s $guest_pci_nr,passthru,$dev_bus/$dev_dev/$dev_fun"

		echo "passthrough a pci device:"
		echo dev_info:$dev_info
		echo dev_path:$dev_path
		echo dev_vendor_id:$dev_vendor_id
		echo dev_device_id:$dev_device_id
		echo dev_bus:$dev_bus
		echo dev_dev:$dev_dev
		echo dev_fun:$dev_fun
		echo dev_drv:$dev_drv
		echo dev_passthru_opt:$dev_passthru_opt

		# replace its driver with pci-stub
		echo "$dev_vendor_id $dev_device_id" > /sys/bus/pci/drivers/pci-stub/new_id
		echo $dev_bdf > $dev_path/driver/unbind
		echo $dev_bdf > /sys/bus/pci/drivers/pci-stub/bind

		# add to global options
		dev_opts="$dev_opts $dev_passthru_opt"

		# register for de-passthrough later
		passthru_devs[$dev_bdf]=$dev_drv

		# increase virtual pci slot number
		guest_pci_nr=$(expr $guest_pci_nr + 1)
	done
}

function de_passthru_pci_devs()
{
	for dev_bdf in "${!passthru_devs[@]}"
	do
		echo $dev_bdf > /sys/bus/pci/drivers/pci-stub/unbind
		echo $dev_bdf > /sys/bus/pci/drivers/${passthru_devs[$dev_bdf]}/bind
	done
}

mem_size=1024M
eth=
disk=
img=
kernel=
passthrus=()
while getopts hn:b:f:k:B:r opt
do
	case "${opt}" in
		h)
			usage;
			;;
		r)
			# restore pci device driver on exit
			trap de_passthru_pci_devs EXIT
			;;
		n)
			eth=${OPTARG}
			passthrus+=($eth)
			;;
		b)
			disk="${OPTARG}"
			passthrus+=($disk)
			;;
		f)
			img="${OPTARG}"
			;;
		k)
			kernel="${OPTARG}"
			;;
		B)
			mem_size="${OPTARG}"
			;;
		?)
			echo "{OPTARG}"
			;;
	esac
done

if [[ ( -z "${kernel}") || (( -z "${disk}") && ( -z "${img}" )) ]]; then
    usage; exit 1
fi

# create a unique tap device for each VM
if [ -z $eth ];then
	tap=tap_rtvm
	tap_exist=$(ip a | grep "$tap" | awk '{print $1}')
	if [ "$tap_exist"x != "x" ]; then
		echo "tap device existed, reuse $tap"
	else
		ip tuntap add dev $tap mode tap
		ip link set "$tap" master acrn-br0
		ip link set dev "$tap" down
		ip link set dev "$tap" up
	fi

	dev_opts="$dev_opts -s $guest_pci_nr,virtio-net,$tap"
	guest_pci_nr=$(expr $guest_pci_nr + 1)
fi

function launch_hard_rt_vm()
{
mkdir -p /mnt
if [ ! -z $img ]; then
	# mount virtual disk (assuming the last partition is root)
	vdisk=$(losetup -f -P --show $img)
	vdisk_root=$(ls -1 ${vdisk}p* | tail -1)
	mount $vdisk_root /mnt

	# extract the kernel and install kernel modules
	bzImage=/mnt/$(tar xzvf $kernel -C /mnt --dereference --exclude=vmlinux* | grep vmlinuz)
	cp $bzImage /tmp/bzImage.xenomai

	# umount disk
	umount /mnt
	losetup -d $vdisk

	# find appropriate root partition information
	disk_part_nr=$(basename $vdisk_root| sed 's/.*p\([0-9]$\)/\1/')
	vdisk=/dev/vda$disk_part_nr

	dev_opts="$dev_opts -s $guest_pci_nr,virtio-blk,$img"
	guest_pci_nr=$(expr $guest_pci_nr + 1)
elif [ ! -z $disk ];then
	# mount the physical disk
	mount $disk /mnt

	# extract the kernel and install kernel modules
	bzImage=/mnt/$(tar xzvf $kernel -C /mnt --dereference --exclude=vmlinux* | grep vmlinuz)
	cp $bzImage /tmp/bzImage.xenomai

	# umount disk
	umount /mnt

	# detect partition information
	disk_part_nr=$(basename $disk| sed 's/.*\([0-9]$\)/\1/')
	if grep -q "nvme" "$disk"; then
    	vdisk=/dev/nvme0n1p$disk_part_nr
    else
    	vdisk=/dev/sda$disk_part_nr
    fi
fi

passthru_pci_devs

#logger_setting, format: logger_name,level; like following
logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"

/usr/bin/acrn-dm -A -m $mem_size -s 0:0,hostbridge \
-k /tmp/bzImage.xenomai \
-U 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 \
--lapic_pt \
--rtvm \
--virtio_poll 1000000 \
-s 1,virtio-console,@stdio:stdio_port \
$dev_opts \
$logger_setting \
-B "root=$vdisk rw rootwait nohpet console=hvc0 consoleblank=0 \
no_timer_check ignore_loglevel log_buf_len=16M nmi_watchdog=0 nosoftlockup \
processor.max_cstate=0 intel_idle.max_cstate=0 intel_pstate=disable idle=poll \
rcu_nocb_poll isolcpus=1 nohz_full=1 rcu_nocbs=1 \
tsc=reliable x2apic_phys xenomai.supported_cpus=2 irqaffinity=0 mce=off" hard_rtvm
}

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

echo 3 > /sys/kernel/debug/dri/0/i915_cache_sharing
echo 300 > /sys/class/drm/card0/gt_max_freq_mhz
echo 300 > /sys/class/drm/card0/gt_min_freq_mhz
echo 300 > /sys/class/drm/card0/gt_boost_freq_mhz

launch_hard_rt_vm
