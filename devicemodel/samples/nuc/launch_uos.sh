#!/bin/bash

function launch_clear()
{
mac=$(cat /sys/class/net/e*/address)
vm_name=vm$1
mac_seed=${mac:9:8}-${vm_name}

#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for memsize setting
mem_size=2048M

acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 2,pci-gvt -G "$3" \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 3,virtio-blk,/home/clear/uos/clear-26770-kvm.img \
  -s 4,virtio-net,tap0 \
  --mac_seed $mac_seed \
  -k /usr/lib/kernel/default-iot-lts2018 \
  -B "root=/dev/vda3 rw rootwait maxcpus=$2 nohpet console=tty0 console=hvc0 \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$4 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 i915.enable_guc_loading=0 \
  i915.enable_guc_submission=0 i915.enable_guc=0" $vm_name
}

launch_clear 1 1 "64 448 8" 0x070F00
