#!/bin/bash

offline_path="/sys/class/vhm/acrn_vhm"

# Check the device file of /dev/acrn_hsm to determine the offline_path
if [ -e "/dev/acrn_hsm" ]; then
offline_path="/sys/class/acrn/acrn_hsm"
fi

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
logger_setting="--logger_setting console,level=4;kmsg,level=3"

#for memsize setting
mem_size=4096M

systemctl stop gdm.patch gdm

gpudevice=`cat /sys/bus/pci/devices/0000:00:02.0/device`

echo "8086 $gpudevice" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:02.0" > /sys/bus/pci/devices/0000:00:02.0/driver/unbind
echo "0000:00:02.0" > /sys/bus/pci/drivers/pci-stub/bind

echo "8086 34ed" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:14.0" > /sys/bus/pci/devices/0000:00:14.0/driver/unbind
echo "0000:00:14.0" > /sys/bus/pci/drivers/pci-stub/bind

echo "8086 8a11" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:08.0" > /sys/bus/pci/devices/0000:00:08.0/driver/unbind
echo "0000:00:08.0" > /sys/bus/pci/drivers/pci-stub/bind

acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 2,passthru,0/2/0 \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 3,virtio-blk,/home/clear/clear-29200-kvm.img \
  -s 4,virtio-net,tap0 \
  -s 7,passthru,0/14/0 \
  -s 8,passthru,0/8/0 \
  $logger_setting \
  --mac_seed $mac_seed \
  -k /usr/lib/kernel/default-iot-lts2018 \
  -B "root=/dev/vda3 rw rootwait maxcpus=$2 nohpet console=tty0 console=hvc0 \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$4 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 i915.enable_guc_loading=0 \
  i915.enable_guc_submission=0 i915.enable_guc=0 i915.alpha_support=1 " $vm_name
}

# offline SOS CPUs except BSP before launch UOS
for i in `ls -d /sys/devices/system/cpu/cpu[2-99]`; do
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

launch_clear 1 4 "64 448 8" 0x070F00
