#!/bin/bash
# board: WHL-IPC-I5, scenario: INDUSTRY, uos: WINDOWS
# pci devices for passthru
declare -A passthru_vpid
declare -A passthru_bdf

passthru_vpid=(
["audio"]="8086 9dc8"
["gpu"]="8086 3ea0"
)
passthru_bdf=(
["audio"]="0000:00:1f.3"
["gpu"]="0000:00:02.0"
)

function tap_net() {
# create a unique tap device for each VM
tap=$1
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
}

function launch_windows()
{
#vm-name used to generate uos-mac address
mac=$(cat /sys/class/net/e*/address)
vm_name=post_vm_id$1
mac_seed=${mac:0:17}-${vm_name}

tap_net tap_WaaG
#check if the vm is running or not
vm_ps=$(pgrep -a -f acrn-dm)
result=$(echo $vm_ps | grep -w "${vm_name}")
if [[ "$result" != "" ]]; then
  echo "$vm_name is running, can't create twice!"
  exit
fi

echo ${passthru_vpid["gpu"]} > /sys/bus/pci/drivers/pci-stub/new_id
echo ${passthru_bdf["gpu"]} > /sys/bus/pci/devices/${passthru_bdf["gpu"]}/driver/unbind
echo ${passthru_bdf["gpu"]} > /sys/bus/pci/drivers/pci-stub/bind
modprobe pci_stub
kernel_version=$(uname -r)
audio_module="/usr/lib/modules/$kernel_version/kernel/sound/soc/intel/boards/snd-soc-sst_bxt_sos_tdf8532.ko"

# use the modprobe to force loading snd-soc-skl/sst_bxt_bdf8532
if [ ! -e $audio_module ]; then
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
boot_audio_option=""
if [ $audio_passthrough == 1 ]; then
    # for audio device
    echo ${passthru_vpid["audio"]} > /sys/bus/pci/drivers/pci-stub/new_id
    echo ${passthru_bdf["audio"]} > /sys/bus/pci/devices/${passthru_bdf["audio"]}/driver/unbind
    echo ${passthru_bdf["audio"]} > /sys/bus/pci/drivers/pci-stub/bind

    boot_audio_option="-s 5,passthru,00/1f/3"
else
    boot_audio_option="-s 5,virtio-audio"
fi
mem_size=4096M
#interrupt storm monitor for pass-through devices, params order:
#threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)
intr_storm_monitor="--intr_monitor 10000,10,1,100"

#logger_setting, format: logger_name,level; like following
logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"

acrn-dm -A -m $mem_size -s 0:0,hostbridge -U d2795438-25d6-11e8-864e-cb7a18b34643 \
   --windows \
   $logger_setting \
   -s 6,virtio-blk,./win10-ltsc.img \
   -s 7,virtio-net,tap_WaaG \
   -s 2,passthru,0/2/0  \
   --ovmf /usr/share/acrn/bios/OVMF.fd \
   $intr_storm_monitor \
   -s 31:0,lpc \
   -l com1,stdio \
   $boot_audio_option \
   $vm_name
}
launch_windows 1

