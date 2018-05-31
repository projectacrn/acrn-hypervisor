#!/bin/bash

function launch_clearlinux()
{
if [ ! -f "/data/$5/$5.img" ]; then
  echo "no /data/$5/$5.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/ens4/address)
vm_name=vm$1
vm_name=${vm_name}-${mac:9:8}

# create a unique tap device for each VM
tap=tap_LaaG
tap_exist=$(ifconfig | grep acrn_"$tap" | awk '{print $1}')
if [ "$tap_exist"x != "x" ]; then
  echo "tap device existed, reuse acrn_$tap"
else
  ip tuntap add dev acrn_$tap mode tap
fi

# if acrn-br0 exists, add VM's unique tap device under it
br_exist=$(ifconfig | grep acrn-br0 | awk '{print $1}')
if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
  brctl addif acrn-br0 acrn_"$tap"
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

# extract clearlinux bzImage from clearlinux virtual disk
if [ $6 == 0 ];then
mkdir -p /home/root/$5
mkdir -p /mnt/loop
mount -o loop,offset=1048576 /data/$5/$5.img /mnt/loop
cp /mnt/loop/bzImage /home/root/$5/bzImage
umount /mnt/loop
fi

#for VT-d device setting
modprobe pci_stub
echo "8086 5aa8" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.0" > /sys/bus/pci/devices/0000:00:15.0/driver/unbind
echo "0000:00:15.0" > /sys/bus/pci/drivers/pci-stub/bind

echo "8086 5aaa" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.1" > /sys/bus/pci/devices/0000:00:15.1/driver/unbind
echo "0000:00:15.1" > /sys/bus/pci/drivers/pci-stub/bind

#For CSME passthrough
echo "8086 5a9a" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:0f.0" > /sys/bus/pci/devices/0000:00:0f.0/driver/unbind
echo "0000:00:0f.0" > /sys/bus/pci/drivers/pci-stub/bind

# for ipu passthrough - ipu device 0:3.0
echo "8086 5a88" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:03.0" > /sys/bus/pci/devices/0000:00:03.0/driver/unbind
echo "0000:00:03.0" > /sys/bus/pci/drivers/pci-stub/bind

# for ipu passthrough - ipu related i2c 0:16.0
echo "8086 5aac" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:16.0" > /sys/bus/pci/devices/0000:00:16.0/driver/unbind
echo "0000:00:16.0" > /sys/bus/pci/drivers/pci-stub/bind

# for sd card passthrough - SDXC/MMC Host Controller 00:1b.0
echo "8086 5aca" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:1b.0" > /sys/bus/pci/devices/0000:00:1b.0/driver/unbind
echo "0000:00:1b.0" > /sys/bus/pci/drivers/pci-stub/bind

#for memsize setting
memsize=`cat /proc/meminfo|head -n 1|awk '{print $2}'`
if [ $memsize -gt 4000000 ];then
    mem_size=2048M
else
    mem_size=1750M
fi

if [ "$setup_mem" != "" ];then
    mem_size=$setup_mem
fi

boot_image_option="-k /home/root/$5/bzImage"
boot_dev_flag=""

if [ $6 == 1 ];then
  boot_dev_flag=",b"
  if [ $7 == 1 ];then
    boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"
  else
    boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"
  fi
fi


acrn-dm --help 2>&1 | grep 'GVT args'
if [ $? == 0 ];then
  GVT_args=$3
  boot_GVT_option=" -s 0:2:0,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi


acrn-dm -T -A -m $mem_size -c $2$boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 8,wdt-i6300esb \
  -s 3,virtio-blk$boot_dev_flag,/data/$5/$5.img \
  -s 4,virtio-net,$tap $boot_image_option \
  -s 7,passthru,0/15/0 \
  -s 15,passthru,0/f/0 \
  -s 12,passthru,0/3/0 \
  -s 22,passthru,0/16/0 \
  -s 27,passthru,0/1b/0 \
  -i /run/acrn/ioc_$vm_name,0x20 \
  -l com2,/run/acrn/ioc_$vm_name \
  -B "root=/dev/vda2 rw rootwait maxcpus=$2 nohpet console=hvc0 \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$4 i915.enable_guc_loading=0 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 i915.enable_initial_modeset=1" $vm_name
}

function launch_android()
{
if [ ! -f "/data/$5/$5.img" ]; then
  echo "no /data/$5/$5.img, exit"
  exit
fi

#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/ens4/address)
vm_name=vm$1
vm_name=${vm_name}-${mac:9:8}

# create a unique tap device for each VM
tap=tap_AaaG
tap_exist=$(ifconfig | grep acrn_"$tap" | awk '{print $1}')
if [ "$tap_exist"x != "x" ]; then
  echo "tap device existed, reuse acrn_$tap"
else
  ip tuntap add dev acrn_$tap mode tap
fi

# if acrn-br0 exists, add VM's unique tap device under it
br_exist=$(ifconfig | grep acrn-br0 | awk '{print $1}')
if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
  echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
  brctl addif acrn-br0 acrn_"$tap"
  ip link set dev acrn_"$tap" down
  ip link set dev acrn_"$tap" up
fi

#Use MMC name + serial for ADB serial no., same as native android
mmc_name=`cat /sys/block/mmcblk1/device/name`
mmc_serial=`cat /sys/block/mmcblk1/device/serial | sed -n 's/^..//p'`
ser=$mmc_name$mmc_serial

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

# extract kernel and ramdisk from android disk image
if [ $6 == 0 ];then
  mkdir -p /home/root/$5
  dd if=/data/$5/$5.img of=/home/root/$5/boot.img skip=63488 bs=512 count=81920
  split_bootimg.pl /home/root/$5/boot.img
fi

#for VT-d device setting
modprobe pci_stub
echo "8086 5aa8" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.0" > /sys/bus/pci/devices/0000:00:15.0/driver/unbind
echo "0000:00:15.0" > /sys/bus/pci/drivers/pci-stub/bind

echo "8086 5aaa" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:15.1" > /sys/bus/pci/devices/0000:00:15.1/driver/unbind
echo "0000:00:15.1" > /sys/bus/pci/drivers/pci-stub/bind

#for audio device
echo "8086 5a98" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:0e.0" > /sys/bus/pci/devices/0000:00:0e.0/driver/unbind
echo "0000:00:0e.0" > /sys/bus/pci/drivers/pci-stub/bind

#for audio codec
echo "8086 5ab4" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:17.0" > /sys/bus/pci/devices/0000:00:17.0/driver/unbind
echo "0000:00:17.0" > /sys/bus/pci/drivers/pci-stub/bind

#For CSME passthrough
echo "8086 5a9a" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:0f.0" > /sys/bus/pci/devices/0000:00:0f.0/driver/unbind
echo "0000:00:0f.0" > /sys/bus/pci/drivers/pci-stub/bind

# for sd card passthrough - SDXC/MMC Host Controller 00:1b.0
echo "8086 5aca" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:1b.0" > /sys/bus/pci/devices/0000:00:1b.0/driver/unbind
echo "0000:00:1b.0" > /sys/bus/pci/drivers/pci-stub/bind

# WIFI is 4:0.0 on SBL, and 3:0.0 on ABL
echo "11ab 2b38" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:04:00.0" > /sys/bus/pci/devices/0000:04:00.0/driver/unbind
echo "0000:04:00.0" > /sys/bus/pci/drivers/pci-stub/bind

# Bluetooth passthrough depends on WIFI
echo "8086 5abc" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:18.0" > /sys/bus/pci/devices/0000:00:18.0/driver/unbind
echo "0000:00:18.0" > /sys/bus/pci/drivers/pci-stub/bind

#for memsize setting
memsize=`cat /proc/meminfo|head -n 1|awk '{print $2}'`
if [ $memsize -gt 4000000 ];then
    mem_size=2048M
else
    mem_size=1750M
fi

if [ "$setup_mem" != "" ];then
    mem_size=$setup_mem
fi

boot_image_option="-k /home/root/$5/boot.img-kernel -r /home/root/$5/boot.img-ramdisk.gz "
boot_dev_flag=""
kernel_cmdline_generic="maxcpus=$2 nohpet tsc=reliable intel_iommu=off \
   androidboot.serialno=$ser \
   i915.enable_rc6=1 i915.enable_fbc=1 i915.enable_guc_loading=0 i915.avail_planes_per_pipe=$4 \
   i915.enable_hangcheck=0 use_nuclear_flip=1 i915.enable_initial_modeset=1 "

if [ $6 == 1 ];then
  boot_dev_flag=",b"
  if [ $7 == 1 ];then
    boot_image_option="--vsbl /usr/share/acrn/bios/VSBL_debug.bin"
  else
    boot_image_option="--vsbl /usr/share/acrn/bios/VSBL.bin"
  fi
  kernel_cmdline="$kernel_cmdline_generic"
else
  kernel_cmdline="$kernel_cmdline_generic"" initrd=0x48800000 root=/dev/ram0 rw rootwait \
   console=ttyS0 no_timer_check log_buf_len=16M consoleblank=0 reboot_panic=p,w \
   relative_sleep_states=1 vga=current \
   androidboot.hardware=gordon_peak_acrn firmware_class.path=/vendor/firmware \
   sys.init_log_level=7  drm.atomic=1 i915.nuclear_pageflip=1 \
   i915.modeset=1 drm.vblankoffdelay=1 i915.fastboot=1 \
   i915.hpd_sense_invert=0x7"
fi

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

acrn-dm --help 2>&1 | grep 'GVT args'
if [ $? == 0 ];then
  GVT_args=$3
  boot_GVT_option=" -s 2,pci-gvt -G "
else
  boot_GVT_option=''
  GVT_args=''
fi

 acrn-dm -T -A -m $mem_size -c $2$boot_GVT_option"$GVT_args" -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
   -l com1,stdio \
   -s 9,virtio-net,$tap \
   -s 3,virtio-blk$boot_dev_flag,/data/$5/$5.img \
   -s 7,passthru,0/15/0 \
   -s 8,passthru,0/15/1 \
   -s 13,virtio-rpmb \
   -s 10,virtio-hyper_dmabuf \
   -s 11,wdt-i6300esb \
   -s 14,passthru,0/e/0 \
   -s 23,passthru,0/17/0 \
   -s 15,passthru,0/f/0 \
   -s 27,passthru,0/1b/0 \
   -s 24,passthru,0/18/0 \
   -s 18,passthru,4/0/0 \
   -M \
   $boot_image_option \
   --enable_trusty \
   -B "$kernel_cmdline" $vm_name
}

function help()
{
echo "Use luanch_UOS.sh like that ./launch_UOS.sh -U/-V <#>"
echo "The option -U means the UOSs group to be launched as below"
echo "-U 1 means just launching 1 clearlinux UOS"
echo "-U 2 means just launching 1 android UOS"
echo "-U 3 means launching 1 clearlinux UOS + 1 android UOS"
echo "-U 4 means launching 2 clearlinux UOSs"
echo "The option -V means the UOSs group to be launched by vsbl as below"
echo "-V 1 means just launching 1 clearlinux UOS"
echo "-V 2 means just launching 1 android UOS"
echo "-V 3 means launching 1 clearlinux UOS + 1 android UOS"
echo "-V 4 means launching 2 clearlinux UOSs"
}

launch_type=1
debug=0

while getopts "U:V:M:hd" opt
do
	case $opt in
		U) launch_type=$OPTARG
			;;
		V) launch_type=$[$OPTARG+4]
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

mkdir -p /data
mount /dev/mmcblk1p3 /data

# make sure there is enough 2M hugepages in the pool
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

case $launch_type in
	1) echo "Launch clearlinux UOS"
		launch_clearlinux 1 3 "64 448 8" 0x000C00 clearlinux 0
		;;
	2) echo "Launch android UOS"
		launch_android 1 3 "64 448 8" 0x000C00 android 0
		;;
	3) echo "Launch clearlinux UOS + android UOS"
		launch_android 1 2 "64 448 4" 0x00000C android 0 &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x000C00 clearlinux 0
		;;
	4) echo "Launch two clearlinux UOSs"
		launch_clearlinux 1 1 "64 448 4" 0x00000C clearlinux 0 &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x000C00 clearlinux_dup 0
		;;
	5) echo "Launch clearlinux UOS"
		launch_clearlinux 1 3 "64 448 8" 0x000C00 clearlinux 1 $debug
		;;
	6) echo "Launch android UOS"
		launch_android 1 3 "64 448 8" 0x000C00 android 1 $debug
		;;
	7) echo "Launch clearlinux UOS + android UOS"
		launch_android 1 2 "64 448 4" 0x00000C android 1 $debug &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x000C00 clearlinux 1 $debug
		;;
	8) echo "Launch two clearlinux UOSs"
		launch_clearlinux 1 1 "64 448 4" 0x00000C clearlinux 1 $debug &
		sleep 5
		launch_clearlinux 2 1 "64 448 4" 0x000C00 clearlinux_dup 1 $debug
		;;
esac

umount /data
