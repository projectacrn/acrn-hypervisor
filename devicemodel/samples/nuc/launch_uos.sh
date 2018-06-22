#!/bin/bash

function launch_clear()
{
vm_name=vm$1


#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

#for memsize setting
mem_size=1000M

# make sure there is enough 2M hugepages in the pool
echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
  -s 2,pci-gvt -G "$3" \
  -s 5,virtio-console,@pty:pty_port \
  -s 6,virtio-hyper_dmabuf \
  -s 3,virtio-blk,/root/clear-21260-kvm.img \
  -s 4,virtio-net,tap0 \
  -k /usr/lib/kernel/org.clearlinux.pk414-standard.4.14.23-19 \
  -B "root=/dev/vda3 rw rootwait maxcpus=$2 nohpet console=tty0 console=hvc0 \
  console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable i915.avail_planes_per_pipe=$4 \
  i915.enable_hangcheck=0 i915.nuclear_pageflip=1 i915.enable_initial_modeset=1" $vm_name
}

launch_clear 2 1 "64 448 8" 0x070F00 clear
