#!/bin/bash


kernel_version=$(uname -r | awk -F. '{ printf("%d.%d", $1,$2) }')

ipu_passthrough=0

# Check the device file of /dev/vbs_ipu to determine the IPU mode
if [ ! -e "/dev/vbs_ipu" ]; then
ipu_passthrough=1
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

function launch_clearlinux()
{
if [ ! -f "/data/$5/$5.img" ]; then
  echo "no /data/$5/$5.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/en*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name} 

# create a unique tap device for each VM
tap=tap_$6
tap_exist=$(ip a | grep acrn_"$tap" | awk '{print $1}')
if [ "$tap_exist"x != "x" ]; then
  echo "tap device existed, reuse acrn_$tap"
else
  ip tuntap add dev acrn_$tap mode tap
fi

# if acrn-br0 exists, add VM's unique tap device under it
br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')
if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
  ip link set acrn_"$tap" master acrn-br0
  ip link set dev acrn_"$tap" down
  ip link set dev acrn_"$tap" up
fi

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for VT-d device setting
modprobe pci_stub
echo "8086 5aaa" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.1" > /sys/bus/pci/devices/0000:00:15.1/driver/unbind
echo "0000:00:15.1" > /sys/bus/pci/drivers/pci-stub/bind

boot_ipu_option=""
if [ $ipu_passthrough == 1 ];then
    # for ipu passthrough - ipu device 0:3.0
    if [ -d "/sys/bus/pci/devices/0000:00:03.0" ]; then
        echo "8086 5a88" > /sys/bus/pci/drivers/pci-stub/new_id
        echo "0000:00:03.0" > /sys/bus/pci/devices/0000:00:03.0/driver/unbind
        echo "0000:00:03.0" > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 12,passthru,0/3/0 "
    fi

    # for ipu passthrough - ipu related i2c 0:16.0
    # please use virtual slot 22 for i2c 0:16.0 to make sure that the i2c controller
    # could get the same virtaul BDF as physical BDF
    if [ -d "/sys/bus/pci/devices/0000:00:16.0" ]; then
        echo "8086 5aac" > /sys/bus/pci/drivers/pci-stub/new_id
        echo "0000:00:16.0" > /sys/bus/pci/devices/0000:00:16.0/driver/unbind
        echo "0000:00:16.0" > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 22,passthru,0/16/0 "
    fi
else
    boot_ipu_option="$boot_ipu_option"" -s 21,virtio-ipu "
fi

boot_cse_option=""
if [ $cse_passthrough == 1 ]; then
    echo "8086 5a9a" > /sys/bus/pci/drivers/pci-stub/new_id
    echo "0000:00:0f.0" > /sys/bus/pci/devices/0000:00:0f.0/driver/unbind
    echo "0000:00:0f.0" > /sys/bus/pci/drivers/pci-stub/bind
    boot_cse_option="$boot_cse_option"" -s 15,passthru,0/0f/0 "
else
    boot_cse_option="$boot_cse_option"" -s 15,virtio-heci,0/0f/0 "
fi

# for sd card passthrough - SDXC/MMC Host Controller 00:1c.0
# echo "8086 5acc" > /sys/bus/pci/drivers/pci-stub/new_id
# echo "0000:00:1c.0" > /sys/bus/pci/devices/0000:00:1c.0/driver/unbind
# echo "0000:00:1c.0" > /sys/bus/pci/drivers/pci-stub/bind

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
if [ $7 == 1 ];then
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"
else
  boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"
fi

#interrupt storm monitor for pass-through devices, params order: 
#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)
intr_storm_monitor="--intr_monitor 10000,10,1,100"

acrn-dm --help 2>&1 | grep 'GVT args'
if [ $? == 0 ];then
  GVT_args=$3
  boot_GVT_option=" -s 0:2:0,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi


acrn-dm -A -m $mem_size -c $2$boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 8,wdt-i6300esb \
  -s 3,virtio-blk$boot_dev_flag,/data/$5/$5.img \
  -s 4,virtio-net,$tap $boot_image_option \
  -s 7,xhci,1-1:1-2:1-3:2-1:2-2:2-3:cap=apl \
  -s 9,passthru,0/15/1 \
  $boot_cse_option \
  $intr_storm_monitor \
  $boot_ipu_option      \
  -i /run/acrn/ioc_$vm_name,0x20 \
  -l com2,/run/acrn/ioc_$vm_name \
  --mac_seed $mac_seed \
  -B "root=/dev/vda2 rw rootwait maxcpus=$2 nohpet console=hvc0 \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$4 i915.enable_guc_loading=0 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 \
  i915.enable_guc_submission=0 i915.enable_guc=0" $vm_name
}

function launch_android()
{
if [ ! -f "/data/$5/$5.img" ]; then
  echo "no /data/$5/$5.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/en*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name} 

# create a unique tap device for each VM
tap=tap_$6
tap_exist=$(ip a | grep acrn_"$tap" | awk '{print $1}')
if [ "$tap_exist"x != "x" ]; then
  echo "tap device existed, reuse acrn_$tap"
else
  ip tuntap add dev acrn_$tap mode tap
fi

# if acrn-br0 exists, add VM's unique tap device under it
br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')
if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
  ip link set acrn_"$tap" master acrn-br0
  ip link set dev acrn_"$tap" down
  ip link set dev acrn_"$tap" up
fi

#Use MMC name + serial for ADB serial no., same as native android
mmc_name=`cat /sys/block/mmcblk0/device/name`
mmc_serial=`cat /sys/block/mmcblk0/device/serial | sed -n 's/^..//p'`
ser=$mmc_name$mmc_serial

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for VT-d device setting
modprobe pci_stub
echo "8086 5aaa" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.1" > /sys/bus/pci/devices/0000:00:15.1/driver/unbind
echo "0000:00:15.1" > /sys/bus/pci/drivers/pci-stub/bind

#for audio device
boot_audio_option=""
if [ $audio_passthrough == 1 ]; then
    echo "8086 5a98" > /sys/bus/pci/drivers/pci-stub/new_id
    echo "0000:00:0e.0" > /sys/bus/pci/devices/0000:00:0e.0/driver/unbind
    echo "0000:00:0e.0" > /sys/bus/pci/drivers/pci-stub/bind

    #for audio codec
    echo "8086 5ab4" > /sys/bus/pci/drivers/pci-stub/new_id
    echo "0000:00:17.0" > /sys/bus/pci/devices/0000:00:17.0/driver/unbind
    echo "0000:00:17.0" > /sys/bus/pci/drivers/pci-stub/bind

    boot_audio_option="-s 14,passthru,0/e/0,keep_gsi -s 23,passthru,0/17/0"
else
    boot_audio_option="-s 14,virtio-audio"
fi
# # for sd card passthrough - SDXC/MMC Host Controller 00:1b.0
# echo "8086 5acc" > /sys/bus/pci/drivers/pci-stub/new_id
# echo "0000:00:1c.0" > /sys/bus/pci/devices/0000:00:1c.0/driver/unbind
# echo "0000:00:1c.0" > /sys/bus/pci/drivers/pci-stub/bind

# Check if the NPK device/driver is present
ls -d /sys/bus/pci/drivers/intel_th_pci/0000* 2>/dev/null 1>/dev/null
if [ $? == 0 ];then
  npk_virt="-s 0:0:2,npk,8/24"
else
  npk_virt=""
fi

# WA for USB role switch hang issue, disable runtime PM of xHCI device
echo on > /sys/devices/pci0000:00/0000:00:15.0/power/control

boot_ipu_option=""
if [ $ipu_passthrough == 1 ];then
    # for ipu passthrough - ipu device 0:3.0
    if [ -d "/sys/bus/pci/devices/0000:00:03.0" ]; then
        echo "8086 5a88" > /sys/bus/pci/drivers/pci-stub/new_id
        echo "0000:00:03.0" > /sys/bus/pci/devices/0000:00:03.0/driver/unbind
        echo "0000:00:03.0" > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 12,passthru,0/3/0 "
    fi

    # for ipu passthrough - ipu related i2c 0:16.0
    # please use virtual slot 22 for i2c 0:16.0 to make sure that the i2c controller
    # could get the same virtaul BDF as physical BDF
    if [ -d "/sys/bus/pci/devices/0000:00:16.0" ]; then
        echo "8086 5aac" > /sys/bus/pci/drivers/pci-stub/new_id
        echo "0000:00:16.0" > /sys/bus/pci/devices/0000:00:16.0/driver/unbind
        echo "0000:00:16.0" > /sys/bus/pci/drivers/pci-stub/bind
        boot_ipu_option="$boot_ipu_option"" -s 22,passthru,0/16/0 "
    fi
else
    boot_ipu_option="$boot_ipu_option"" -s 21,virtio-ipu "
fi

boot_cse_option=""
if [ $cse_passthrough == 1 ]; then
    echo "8086 5a9a" > /sys/bus/pci/drivers/pci-stub/new_id
    echo "0000:00:0f.0" > /sys/bus/pci/devices/0000:00:0f.0/driver/unbind
    echo "0000:00:0f.0" > /sys/bus/pci/drivers/pci-stub/bind
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

kernel_cmdline_generic="maxcpus=$2 nohpet tsc=reliable intel_iommu=off \
   androidboot.serialno=$ser \
   i915.enable_rc6=1 i915.enable_fbc=1 i915.enable_guc_loading=0 i915.avail_planes_per_pipe=$4 \
   i915.enable_hangcheck=0 use_nuclear_flip=1 i915.enable_guc_submission=0 i915.enable_guc=0"

boot_dev_flag=",b"
if [ $7 == 1 ];then
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
  GVT_args=$3
  boot_GVT_option=" -s 2,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi

 acrn-dm -A -m $mem_size -c $2$boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio $npk_virt\
   -s 9,virtio-net,$tap \
   -s 3,virtio-blk$boot_dev_flag,/data/$5/$5.img \
   -s 7,xhci,1-1:1-2:1-3:2-1:2-2:2-3:cap=apl \
   -s 8,passthru,0/15/1 \
   -s 13,virtio-rpmb \
   -s 10,virtio-hyper_dmabuf \
   -s 11,wdt-i6300esb \
   $boot_audio_option \
   $boot_cse_option \
   $intr_storm_monitor \
   $boot_ipu_option      \
   --mac_seed $mac_seed \
   -i /run/acrn/ioc_$vm_name,0x20 \
   -l com2,/run/acrn/ioc_$vm_name \
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
}

launch_type=1
debug=0

while getopts "V:M:hd" opt
do
	case $opt in
		V) launch_type=$[$OPTARG]
			;;
		M) setup_mem=$OPTARG
			;;
		d) debug=1
			;;
		h) help
			exit 1
			;;
		?) help
			exit 1
			;;
	esac
done

if [ ! -b "/dev/mmcblk0p3" ]; then
  echo "no /dev/mmcblk0p3 data partition, exit"
  exit
fi

mkdir -p /data
mount /dev/mmcblk0p3 /data

if [ $launch_type == 6 ]; then
	if [ -f "/data/android/android.img" ]; then
	  launch_type=2;
	elif [ -f "/data/alios/alios.img" ]; then
	  launch_type=5;
	else
	  launch_type=1;
	fi
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
                echo $idx > /sys/class/vhm/acrn_vhm/offline_cpu
        fi
done

case $launch_type in
	1) echo "Launch clearlinux UOS"
		launch_clearlinux 1 1 "64 448 8" 0x070F00 clearlinux "LaaG" $debug
		;;
	2) echo "Launch android UOS"
		launch_android 1 1 "64 448 8" 0x070F00 android "AaaG" $debug
		;;
	3) echo "Launch clearlinux UOS + android UOS"
		launch_android 1 2 "64 448 4" 0x00000C android "AaaG" $debug &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x070F00 clearlinux "LaaG" $debug
		;;
	4) echo "Launch two clearlinux UOSs"
		launch_clearlinux 1 1 "64 448 4" 0x00000C clearlinux "L1aaG" $debug &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x070F00 clearlinux_dup "L2aaG" $debug
		;;
	5) echo "Launch alios UOS"
		launch_alios 1 3 "64 448 8" 0x070F00 alios "AliaaG" $debug
		;;
esac

umount /data
