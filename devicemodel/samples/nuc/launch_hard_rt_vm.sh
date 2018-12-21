#!/bin/bash
# This is an example of launch script for KBL NUC7i7DNH, may need to revise for other platform.

function launch_hard_rt_vm()
{
#for memsize setting
mem_size=1024M

modprobe pci_stub
# Ethernet pass-through
#echo "8086 156f" > /sys/bus/pci/drivers/pci-stub/new_id
#echo "0000:00:1f.6" > /sys/bus/pci/devices/0000:00:1f.6/driver/unbind
#echo "0000:00:1f.6" > /sys/bus/pci/drivers/pci-stub/bind

# SATA pass-through
echo "8086 9d03" > /sys/bus/pci/drivers/pci-stub/new_id
echo "0000:00:17.0" > /sys/bus/pci/devices/0000:00:17.0/driver/unbind
echo "0000:00:17.0" > /sys/bus/pci/drivers/pci-stub/bind

/usr/bin/acrn-dm -m $mem_size -c $1 \
  -k /root/rt_uos_kernel \
   --lapic_pt \
   --virtio_poll 1000000 \
   -s 2,passthru,0/17/0 \
   -s 3,virtio-console,@stdio:stdio_port \
  -B "root=/dev/sda3 rw rootwait maxcpus=$1 nohpet console=hvc0 \
  no_timer_check ignore_loglevel log_buf_len=16M \
  consoleblank=0 tsc=reliable x2apic_phys" hard_rtvm
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

launch_hard_rt_vm 1
