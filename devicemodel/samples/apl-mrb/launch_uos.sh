#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

offline_path="/sys/class/vhm/acrn_vhm"

# Check the device file of /dev/acrn_hsm to determine the offline_path
if [ -e "/dev/acrn_hsm" ]; then
offline_path="/sys/class/acrn/acrn_hsm"
fi

kernel_version=$(uname -r)
audio_module="/usr/lib/modules/$kernel_version/kernel/sound/soc/intel/boards/snd-soc-sst_bxt_sos_tdf8532.ko"
ipu_passthrough=0

# Check the device file of /dev/vbs_ipu to determine the IPU mode
if [ ! -e "/dev/vbs_ipu" ]; then
ipu_passthrough=1
fi

# use the modprobe to force loading snd-soc-skl/sst_bxt_bdf8532
if [ ! -e $(audio_module)]; then
modprobe -q snd-soc-skl
modprobe -q snd-soc-sst_bxt_tdf8532
else

modprobe -q snd_soc_skl
modprobe -q snd_soc_tdf8532
modprobe -q snd_soc_sst_bxt_sos_tdf8532
modprobe -q snd_soc_skl_virtio_be
fi
audio_passthrough=0

# Check the device file of /dev/vbs_k_audio to determine the audio mode
if [ ! -e "/dev/vbs_k_audio" ]; then
audio_passthrough=1
fi

cse_passthrough=0
hbm_ver=`cat /sys/class/mei/mei0/hbm_ver`
major_ver=`echo $hbm_ver | cut -d '.' -f1`
minor_ver=`echo $hbm_ver | cut -d '.' -f2`
if [[ "$major_ver" -lt "2" ]] || \
   [[ "$major_ver" == "2" && "$minor_ver" -lt "2" ]]; then
    cse_passthrough=1
fi

# pci devices for passthru
declare -A passthru_vpid
declare -A passthru_bdf

passthru_vpid=(
["usb_xdci"]="8086 5aaa"
["ipu"]="8086 5a88"
["ipu_i2c"]="8086 5aac"
["cse"]="8086 5a9a"
["sd_card"]="8086 5aca"
["audio"]="8086 5a98"
["audio_codec"]="8086 5ab4"
["wifi"]="11ab 2b38"
["bluetooth"]="8086 5abc"
)
passthru_bdf=(
["usb_xdci"]="0000:00:15.1"
["ipu"]="0000:00:03.0"
["ipu_i2c"]="0000:00:16.0"
["cse"]="0000:00:0f.0"
["sd_card"]="0000:00:1b.0"
["audio"]="0000:00:0e.0"
["audio_codec"]="0000:00:17.0"
["wifi"]="0000:03:00.0"
["bluetooth"]="0000:00:18.0"
)

function launch_clearlinux()
{
if [ ! -f "/data/$4/$4.img" ]; then
  echo "no /data/$4/$4.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/e*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name}

# create a unique tap device for each VM
tap=tap_$5
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

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep -w "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for VT-d device setting
modprobe pci_stub
echo ${passthru_vpid["usb_xdci"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["usb_xdci"]} > /sys/bus/pci/devices/${passthru_bdf["usb_xdci"]}/driver/unbind
echo ${passthru_bdf["usb_xdci"]} > /sys/bus/pci/drivers/pci-stub/bind

echo 100 > /sys/bus/usb/drivers/usb-storage/module/parameters/delay_use

boot_ipu_option=""
if [ $ipu_passthrough == 1 ];then
    # for ipu passthrough - ipu device
    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu"]}" ]; then
        echo ${passthru_vpid["ipu"]} > /sys/bus/pci/drivers/pci-stub/new_id
        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/devices/${passthru_bdf["ipu"]}/driver/unbind
        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 12,passthru,0/3/0 "
    fi

    # for ipu passthrough - ipu related i2c
    # please use virtual slot 22 for i2c to make sure that the i2c controller
    # could get the same virtaul BDF as physical BDF
    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}" ]; then
        echo ${passthru_vpid["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/new_id
        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}/driver/unbind
        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 22,passthru,0/16/0 "
    fi
else
    boot_ipu_option="$boot_ipu_option"" -s 21,virtio-ipu "
fi

boot_cse_option=""
if [ $cse_passthrough == 1 ]; then
    echo ${passthru_vpid["cse"]} > /sys/bus/pci/drivers/pci-stub/new_id
    echo ${passthru_bdf["cse"]} > /sys/bus/pci/devices/${passthru_bdf["cse"]}/driver/unbind
    echo ${passthru_bdf["cse"]} > /sys/bus/pci/drivers/pci-stub/bind
    boot_cse_option="$boot_cse_option"" -s 15,passthru,0/0f/0 "
else
    boot_cse_option="$boot_cse_option"" -s 15,virtio-heci,0/0f/0 "
fi

# for sd card passthrough - SDXC/MMC Host Controller 00:1b.0
echo ${passthru_vpid["sd_card"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["sd_card"]} > /sys/bus/pci/devices/${passthru_bdf["sd_card"]}/driver/unbind
echo ${passthru_bdf["sd_card"]} > /sys/bus/pci/drivers/pci-stub/bind

#for memsize setting, total 8GB(>7.5GB) uos->6GB, 4GB(>3.5GB) uos->2GB
memsize=`cat /proc/meminfo|head -n 1|awk '{print $2}'`
if [ $memsize -gt 7500000 ];then
    mem_size=6G
elif [ $memsize -gt 3500000 ];then
    mem_size=2G
else
    mem_size=1750M
fi

if [ "$setup_mem" != "" ];then
    mem_size=$setup_mem
fi

boot_dev_flag=",b"
if [ $6 == 1 ];then
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"
else
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"
fi

#interrupt storm monitor for pass-through devices, params order: 
#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)
intr_storm_monitor="--intr_monitor 10000,10,1,100"

acrn-dm --help 2>&1 | grep 'GVT args'
if [ $? == 0 ];then
  GVT_args=$2
  boot_GVT_option=" -s 0:2:0,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi


acrn-dm -A -m $mem_size $boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 8,wdt-i6300esb \
  -s 3,virtio-blk$boot_dev_flag,/data/$4/$4.img \
  -s 4,virtio-net,$tap $boot_image_option \
  -s 7,xhci,1-1:1-2:1-3:2-1:2-2:2-3:cap=apl \
  -s 9,passthru,0/15/1 \
  $boot_cse_option \
  --mac_seed $mac_seed \
  -s 27,passthru,0/1b/0 \
  $intr_storm_monitor \
  $boot_ipu_option      \
  -i /run/acrn/ioc_$vm_name,0x20 \
  -l com2,/run/acrn/ioc_$vm_name \
  --pm_notify_channel ioc \
  -B "root=/dev/vda2 rw rootwait nohpet console=hvc0 \
  snd_soc_skl_virtio_fe.domain_id=1 \
  snd_soc_skl_virtio_fe.domain_name="GuestOS" \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$3 i915.enable_guc_loading=0 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 \
  i915.enable_guc_submission=0 i915.enable_guc=0" $vm_name
}

function launch_android()
{
if [ ! -f "/data/$4/$4.img" ]; then
  echo "no /data/$4/$4.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/e*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name}

# create a unique tap device for each VM
tap=tap_$5
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

#Use MMC name + serial for ADB serial no., same as native android
mmc_name=`cat /sys/block/mmcblk1/device/name`
mmc_serial=`cat /sys/block/mmcblk1/device/serial | sed -n 's/^..//p'`
ser=$mmc_name$mmc_serial

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep -w "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for VT-d device setting
modprobe pci_stub
echo ${passthru_vpid["usb_xdci"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["usb_xdci"]} > /sys/bus/pci/devices/${passthru_bdf["usb_xdci"]}/driver/unbind
echo ${passthru_bdf["usb_xdci"]} > /sys/bus/pci/drivers/pci-stub/bind

#for audio device
boot_audio_option=""
if [ $audio_passthrough == 1 ]; then
    echo ${passthru_vpid["audio"]} > /sys/bus/pci/drivers/pci-stub/new_id
    echo ${passthru_bdf["audio"]} > /sys/bus/pci/devices/${passthru_bdf["audio"]}/driver/unbind
    echo ${passthru_bdf["audio"]} > /sys/bus/pci/drivers/pci-stub/bind

    #for audio codec
    echo ${passthru_vpid["audio_codec"]} > /sys/bus/pci/drivers/pci-stub/new_id
    echo ${passthru_bdf["audio_codec"]} > /sys/bus/pci/devices/${passthru_bdf["audio_codec"]}/driver/unbind
    echo ${passthru_bdf["audio_codec"]} > /sys/bus/pci/drivers/pci-stub/bind

    boot_audio_option="-s 14,passthru,0/e/0,keep_gsi -s 23,passthru,0/17/0"
else
    boot_audio_option="-s 14,virtio-audio"
fi

# for sd card passthrough - SDXC/MMC Host Controller 00:1b.0
echo ${passthru_vpid["sd_card"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["sd_card"]} > /sys/bus/pci/devices/${passthru_bdf["sd_card"]}/driver/unbind
echo ${passthru_bdf["sd_card"]} > /sys/bus/pci/drivers/pci-stub/bind

# WIFI
echo ${passthru_vpid["wifi"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["wifi"]} > /sys/bus/pci/devices/${passthru_bdf["wifi"]}/driver/unbind
echo ${passthru_bdf["wifi"]} > /sys/bus/pci/drivers/pci-stub/bind

# Bluetooth passthrough depends on WIFI
echo ${passthru_vpid["bluetooth"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["bluetooth"]} > /sys/bus/pci/devices/${passthru_bdf["bluetooth"]}/driver/unbind
echo ${passthru_bdf["bluetooth"]} > /sys/bus/pci/drivers/pci-stub/bind

# Check if the NPK device/driver is present
ls -d /sys/bus/pci/drivers/intel_th_pci/0000* 2>/dev/null 1>/dev/null
if [ $? == 0 ];then
  npk_virt="-s 0:0:2,npk,8/24"
else
  npk_virt=""
fi

# WA for USB role switch hang issue, disable runtime PM of xHCI device
echo on > /sys/devices/pci0000:00/0000:00:15.0/power/control

echo 100 > /sys/bus/usb/drivers/usb-storage/module/parameters/delay_use

boot_ipu_option=""
if [ $ipu_passthrough == 1 ];then
    # for ipu passthrough - ipu device
    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu"]}" ]; then
        echo ${passthru_vpid["ipu"]} > /sys/bus/pci/drivers/pci-stub/new_id
        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/devices/${passthru_bdf["ipu"]}/driver/unbind
        echo ${passthru_bdf["ipu"]} > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 12,passthru,0/3/0 "
    fi

    # for ipu passthrough - ipu related i2c
    # please use virtual slot 22 for i2c to make sure that the i2c controller
    # could get the same virtaul BDF as physical BDF
    if [ -d "/sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}" ]; then
        echo ${passthru_vpid["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/new_id
        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/devices/${passthru_bdf["ipu_i2c"]}/driver/unbind
        echo ${passthru_bdf["ipu_i2c"]} > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 22,passthru,0/16/0 "
    fi
else
    boot_ipu_option="$boot_ipu_option"" -s 21,virtio-ipu "
fi

boot_cse_option=""
if [ $cse_passthrough == 1 ]; then
    echo ${passthru_vpid["cse"]} /sys/bus/pci/drivers/pci-stub/new_id
    echo ${passthru_bdf["cse"]} > /sys/bus/pci/devices/${passthru_bdf["cse"]}/driver/unbind
    echo ${passthru_bdf["cse"]} > /sys/bus/pci/drivers/pci-stub/bind
    boot_cse_option="$boot_cse_option"" -s 15,passthru,0/0f/0 "
else
    boot_cse_option="$boot_cse_option"" -s 15,virtio-heci,0/0f/0 "
fi

#for memsize setting, total 8GB(>7.5GB) uos->6GB, 4GB(>3.5GB) uos->2GB
memsize=`cat /proc/meminfo|head -n 1|awk '{print $2}'`
if [ $memsize -gt 7500000 ];then
    mem_size=6G
elif [ $memsize -gt 3500000 ];then
    mem_size=2G
else
    mem_size=1750M
fi

if [ "$setup_mem" != "" ];then
    mem_size=$setup_mem
fi

kernel_cmdline_generic="nohpet tsc=reliable intel_iommu=off \
   androidboot.serialno=$ser \
   snd_soc_skl_virtio_fe.domain_id=1 \
   snd_soc_skl_virtio_fe.domain_name="GuestOS" \
   i915.enable_rc6=1 i915.enable_fbc=1 i915.enable_guc_loading=0 i915.avail_planes_per_pipe=$3 \
   i915.enable_hangcheck=0 use_nuclear_flip=1 i915.enable_guc_submission=0 i915.enable_guc=0"

boot_dev_flag=",b"
if [ $6 == 1 ];then
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"
else
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"
fi
kernel_cmdline="$kernel_cmdline_generic"

: '
select right virtual slots for acrn_dm:
1. some passthru device need virtual slot same as physical, like audio 0:e.0 at
virtual #14 slot, so "-s 14,passthru,0/e/0"
2. acrn_dm share vioapic irq between some virtual slots: like 6&14, 7&15. Need
guarantee no virt irq sharing for each passthru device.
FIXME: picking a virtual slot (#24 now) which is level-triggered to make sure
audio codec passthrough working
3. the bootable device slot is configured in compile stating in Android Guest
image, it should be kept using 3 as fixed value for Android Guest on Gordon_peak
ACRN project
'

#interrupt storm monitor for pass-through devices, params order: 
#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)
intr_storm_monitor="--intr_monitor 10000,10,1,100"

acrn-dm --help 2>&1 | grep 'GVT args'
if [ $? == 0 ];then
  GVT_args=$2
  boot_GVT_option=" -s 2,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi

 acrn-dm -A -m $mem_size $boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio $npk_virt\
   -s 9,virtio-net,$tap \
   -s 3,virtio-blk$boot_dev_flag,/data/$4/$4.img \
   -s 7,xhci,1-1:1-2:1-3:2-1:2-2:2-3:cap=apl \
   -s 8,passthru,0/15/1 \
   -s 13,virtio-rpmb \
   -s 10,virtio-hyper_dmabuf \
   -s 11,wdt-i6300esb \
   $boot_audio_option \
   $boot_cse_option \
   --mac_seed $mac_seed \
   -s 27,passthru,0/1b/0 \
   -s 24,passthru,0/18/0 \
   -s 18,passthru,3/0/0,keep_gsi \
   $intr_storm_monitor \
   $boot_ipu_option      \
   -i /run/acrn/ioc_$vm_name,0x20 \
   -l com2,/run/acrn/ioc_$vm_name \
   --pm_notify_channel ioc \
   $boot_image_option \
   --enable_trusty \
   -B "$kernel_cmdline" $vm_name
}

function launch_alios()
{
#AliOS is not Android, only has same configuration currently, reuse launch function

launch_android "$@"
}

function help()
{
echo "Use luanch_uos.sh like that ./launch_uos.sh -V <#>"
echo "The option -V means the UOSs group to be launched by vsbl as below"
echo "-V 1 means just launching 1 clearlinux UOS"
echo "-V 2 means just launching 1 android UOS"
echo "-V 3 means launching 1 clearlinux UOS + 1 android UOS"
echo "-V 4 means launching 2 clearlinux UOSs"
echo "-V 5 means just launching 1 alios UOS"
echo "-V 6 means auto check android/linux/alios UOS; if exist, launch it"
echo "-C means launching acrn-dm in runC container for QoS"
}

launch_type=1
debug=0
runC_enable=0

function run_container()
{
vm_name=vm1
config_src="/usr/share/acrn/samples/apl-mrb/runC.json"
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

while getopts "V:M:hdC" opt
do
	case $opt in
		V) launch_type=$[$OPTARG]
			;;
		M) setup_mem=$OPTARG
			;;
		d) debug=1
			;;
		C) runC_enable=1
			;;
		h) help
			exit 1
			;;
		?) help
			exit 1
			;;
	esac
done

if [ ! -b "/dev/mmcblk1p3" ]; then
  echo "no /dev/mmcblk1p3 data partition, exit"
  exit
fi

mkdir -p /data
mount /dev/mmcblk1p3 /data

if [ $launch_type == 6 ]; then
	if [ -f "/data/android/android.img" ]; then
	  launch_type=2;
	elif [ -f "/data/alios/alios.img" ]; then
	  launch_type=5;
	else
	  launch_type=1;
	fi
fi

if [ $runC_enable == 1 ]; then
	if [ $(hostname) = "runc" ]; then
		echo "Already in container exit!"
		exit
	fi
	run_container
	exit
fi

# offline SOS CPUs except BSP before launch UOS
for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do
        online=`cat $i/online`
        idx=`echo $i | tr -cd "[1-99]"`
        echo cpu$idx online=$online
        if [ "$online" = "1" ]; then
                echo 0 > $i/online
		online=`cat $i/online`
		# during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod
		while [ "$online" = "1" ]; do
			sleep 1
			echo 0 > $i/online
			online=`cat $i/online`
		done
                echo $idx > ${offline_path}/offline_cpu
        fi
done

case $launch_type in
	1) echo "Launch clearlinux UOS"
		launch_clearlinux 1 "64 448 8" 0x070F00 clearlinux "LaaG" $debug
		;;
	2) echo "Launch android UOS"
		launch_android 1 "64 448 8" 0x070F00 android "AaaG" $debug
		;;
	3) echo "Launch clearlinux UOS + android UOS"
		launch_android 1 "64 448 4" 0x00000C android "AaaG" $debug &
		sleep 5
		launch_clearlinux 2 "64 448 4" 0x070F00 clearlinux "LaaG" $debug
		;;
	4) echo "Launch two clearlinux UOSs"
		launch_clearlinux 1 "64 448 4" 0x00000C clearlinux "L1aaG" $debug &
		sleep 5
		launch_clearlinux 2 "64 448 4" 0x070F00 clearlinux_dup "L2aaG" $debug
		;;
	5) echo "Launch alios UOS"
		launch_alios 1 "64 448 8" 0x070F00 alios "AliaaG" $debug
		;;
esac

umount /data
