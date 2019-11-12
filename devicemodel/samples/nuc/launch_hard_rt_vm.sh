#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
# This is an example of launch script for KBL NUC7i7DNH, may need to revise for other platform.

# pci devices for passthru
declare -A passthru_vpid
declare -A passthru_bdf

passthru_vpid=(
["eth"]="8086 156f"
["sata"]="8086 9d03"
["nvme"]="8086 f1a6"
)
passthru_bdf=(
["eth"]="0000:00:1f.6"
["sata"]="0000:00:17.0"
["nvme"]="0000:02:00.0"
)

function launch_hard_rt_vm()
{
#for memsize setting
mem_size=1024M

modprobe pci_stub
# Ethernet pass-through
#echo ${passthru_vpid["eth"]} > /sys/bus/pci/drivers/pci-stub/new_id
#echo ${passthru_bdf["eth"]} > /sys/bus/pci/devices/${passthru_bdf["eth"]}/driver/unbind
#echo ${passthru_bdf["eth"]} > /sys/bus/pci/drivers/pci-stub/bind

# SATA pass-through
#echo ${passthru_vpid["sata"]} > /sys/bus/pci/drivers/pci-stub/new_id
#echo ${passthru_bdf["sata"]} > /sys/bus/pci/devices/${passthru_bdf["sata"]}/driver/unbind
#echo ${passthru_bdf["sata"]} > /sys/bus/pci/drivers/pci-stub/bind

# NVME pass-through
echo ${passthru_vpid["nvme"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["nvme"]} > /sys/bus/pci/devices/${passthru_bdf["nvme"]}/driver/unbind
echo ${passthru_bdf["nvme"]} > /sys/bus/pci/drivers/pci-stub/bind

# for pm setting
pm_channel="--pm_notify_channel uart "
pm_by_vuart="--pm_by_vuart tty,/dev/ttyS1"


/usr/bin/acrn-dm -A -m $mem_size -s 0:0,hostbridge \
   --lapic_pt \
   --rtvm \
   --virtio_poll 1000000 \
   -U 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 \
   -s 2,passthru,02/0/0 \
   -s 3,virtio-console,@stdio:stdio_port \
   $pm_channel $pm_by_vuart \
   --ovmf /usr/share/acrn/bios/OVMF.fd \
   hard_rtvm

}

# -s 2,passthru,0/17/0 \ #please using "lspci -nn" check the bdf info

# Depends on which partation RT_LaaG is installed in;maybe need to change
# /dev/nvme0n1p3 to /dev/sda3 on NUC and uncomment SATA pass-through
# Add following RT_LaaG kernel cmdline into loader/entries/xxx.conf of EFI partation
#root=/dev/nvme0n1p3 rw rootwait nohpet console=hvc0 console=ttyS0 \
#no_timer_check ignore_loglevel log_buf_len=16M consoleblank=0 \
#clocksource=tsc tsc=reliable x2apic_phys processor.max_cstate=0 \
#intel_idle.max_cstate=0 intel_pstate=disable mce=ignore_ce audit=0 \
#isolcpus=nohz,domain,1 nohz_full=1 rcu_nocbs=1 nosoftlockup idle=poll \
#irqaffinity=0

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

launch_hard_rt_vm
