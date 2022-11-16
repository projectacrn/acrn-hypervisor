#!/bin/bash
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Launch script for VM name: POST_RT_VM1


#

# Helper functions

function probe_modules() {
    modprobe pci_stub
}

function offline_cpus() {
    # Each parameter of this function is considered the APIC ID (as is reported in /proc/cpuinfo, in decimal) of a CPU
    # assigned to a post-launched RTVM.
    for i in $*; do
        processor_id=$(grep -B 15 "apicid.*: ${i}$" /proc/cpuinfo | grep "^processor" | head -n 1 | cut -d ' ' -f 2)
        if [ -z ${processor_id} ]; then
            continue
        fi
        if [ "${processor_id}" = "0" ]; then
            echo "Warning: processor 0 can't be offline, there may be unexpect error!" >> /dev/stderr
            continue
        fi
        cpu_path="/sys/devices/system/cpu/cpu${processor_id}"
        if [ -f ${cpu_path}/online ]; then
            online=`cat ${cpu_path}/online`
            echo cpu${processor_id} online=${online} >> /dev/stderr
            if [ "${online}" = "1" ]; then
                echo 0 > ${cpu_path}/online
                online=`cat ${cpu_path}/online`
                # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod
                while [ "${online}" = "1" ]; do
                    sleep 1
                    echo 0 > ${cpu_path}/online
                    online=`cat ${cpu_path}/online`
                done
                echo ${processor_id} > /sys/devices/virtual/misc/acrn_hsm/remove_cpu
            fi
        fi
    done
}

function unbind_device() {
    physical_bdf=$1

    vendor_id=$(cat /sys/bus/pci/devices/${physical_bdf}/vendor)
    device_id=$(cat /sys/bus/pci/devices/${physical_bdf}/device)

    echo $(printf "%04x %04x" ${vendor_id} ${device_id}) > /sys/bus/pci/drivers/pci-stub/new_id
    echo ${physical_bdf} > /sys/bus/pci/devices/${physical_bdf}/driver/unbind
    echo ${physical_bdf} > /sys/bus/pci/drivers/pci-stub/bind
}

function create_tap() {
    # create a unique tap device for each VM
    tap=$1
    tap_exist=$(ip a show dev $tap)
    if [ "$tap_exist"x != "x" ]; then
        echo "$tap TAP device already available, reusing it."
    else
        ip tuntap add dev $tap mode tap
    fi

    # if acrn-br0 exists, add VM's unique tap device under it
    br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')
    if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
        echo "acrn-br0 bridge already exists, adding new $tap TAP device to it..."
        ip link set "$tap" master acrn-br0
        ip link set dev "$tap" down
        ip link set dev "$tap" up
    fi
}

function mount_partition() {
    partition=$1

    tmpdir=`mktemp -d`
    mount ${partition} ${tmpdir}
    echo ${tmpdir}
}

function unmount_partition() {
    tmpdir=$1

    umount ${tmpdir}
    rmdir ${tmpdir}
}

# Generators of device model parameters

function add_cpus() {
    # Each parameter of this function is considered the apicid of processor (as is reported in /proc/cpuinfo) of
    # a CPU assigned to a post-launched RTVM.

    if [ "${vm_type}" = "RTVM" ] || [ "${scheduler}" = "SCHED_NOOP" ] || [ "${own_pcpu}" = "y" ]; then
        offline_cpus $*
    fi

    cpu_list=$(local IFS=, ; echo "$*")
    echo -n "--cpu_affinity ${cpu_list}"
}

function add_interrupt_storm_monitor() {
    threshold_per_sec=$1
    probe_period_in_sec=$2
    inject_delay_in_ms=$3
    delay_duration_in_ms=$4

    echo -n "--intr_monitor ${threshold_per_sec},${probe_period_in_sec},${inject_delay_in_ms},${delay_duration_in_ms}"
}

function add_logger_settings() {
    loggers=()

    for conf in $*; do
        logger=${conf%=*}
        level=${conf#*=}
        loggers+=("${logger},level=${level}")
    done

    cmd_param=$(local IFS=';' ; echo "${loggers[*]}")
    echo -n "--logger_setting ${cmd_param}"
}

function add_virtual_device() {
    slot=$1
    kind=$2
    options=$3

    if [ "${kind}" = "virtio-net" ]; then
        # Create the tap device
        if [[ ${options} =~ tap=([^,]+) ]]; then
            tap_conf="${BASH_REMATCH[1]}"
            create_tap "${tap_conf}" >&2
        fi
    fi

    if [ "${kind}" = "virtio-input" ]; then
        options=$*
        if [[ "${options}" =~ id:([a-zA-Z0-9_\-]*) ]]; then
            unique_identifier="${BASH_REMATCH[1]}"
            options=${options/",id:${unique_identifier}"/''}
        fi

        if [[ "${options}" =~ (Device name: )(.*),( Device physical path: )(.*) ]]; then
            device_name="${BASH_REMATCH[2]}"
            phys_name="${BASH_REMATCH[4]}"
            local IFS=$'\n'
            device_name_paths=$(grep -r "${device_name}" /sys/class/input/event*/device/name)
            phys_paths=$(grep -r "${phys_name}" /sys/class/input/event*/device/phys)
        fi

        if [ -n "${device_name_paths}" ] && [ -n "${phys_paths}" ]; then
            for device_path in ${device_name_paths}; do
                for phys_path in ${phys_paths}; do
                    if [ "${device_path%/device*}" = "${phys_path%/device*}" ]; then
                        event_path=${device_path}
                        if [[ ${event_path} =~ event([0-9]+) ]]; then
                            event_num="${BASH_REMATCH[1]}"
                            options="/dev/input/event${event_num}"
                            break
                        fi
                    fi
                done
            done
        fi

        if [[ ${options} =~ event([0-9]+) ]]; then
            echo "${options} input device path is available in the service vm." >> /dev/stderr
        else
            echo "${options} input device path is not found in the service vm." >> /dev/stderr
            return
        fi

        if [ -n "${options}" ] && [ -n "${unique_identifier}" ]; then
            options="${options},${unique_identifier}"
        fi

    fi

    echo -n "-s ${slot},${kind}"
    if [ -n "${options}" ]; then
        echo -n ",${options}"
    fi
}

function add_passthrough_device() {
    slot=$1
    physical_bdf=$2
    options=$3

    unbind_device ${physical_bdf%,*}

    # bus, device and function as decimal integers
    bus_temp=${physical_bdf#*:};     bus=$((16#${bus_temp%:*}))
    dev_temp=${physical_bdf##*:};    dev=$((16#${dev_temp%.*}))
    fun=$((16#${physical_bdf#*.}))

    echo -n "-s "
    printf '%s,passthru,%x/%x/%x' ${slot} ${bus} ${dev} ${fun}
    if [ -n "${options}" ]; then
        echo -n ",${options}"
    fi
}

###
# The followings are generated by launch_cfg_gen.py
###

# Defining variables that describe VM types
vm_type='RTVM'
scheduler='SCHED_BVT'
own_pcpu='n'

# Initializing
probe_modules
mac=$(cat /sys/class/net/e*/address)


# Note for developers: The number of available logical CPUs depends on the
# number of enabled cores and whether Hyperthreading is enabled in the BIOS
# settings. CPU IDs are assigned to each logical CPU but are not the same ID
# value throughout the system:
#
# Native CPU_ID:
#       ID enumerated by the Linux Kernel and shown in the
#       ACRN Configurator's CPU Affinity option (used in the scenario.xml)
# Service VM CPU_ID:
#       ID assigned by the Service VM at runtime
# APIC_ID:
#       Advanced Programmable Interrupt Controller's unique ID as
#       enumerated by the board inspector (used in this launch script)
#
# This table shows equivalent CPU IDs for this scenario and board:
#

#   Native CPU_ID    Service VM CPU_ID    APIC_ID
#   -------------    -----------------    -------
#     0                   0                   0
#     1                   1                   2
#     2                   2                   4
#     3                   3                   6

# Invoking ACRN device model
dm_params=(
    `add_cpus                                 4 6`
    -m 1024M
    --ovmf /usr/share/acrn/bios/OVMF.fd
    `add_virtual_device                       0:0 hostbridge`
    --virtio_poll 1000000
    `add_virtual_device                       3 virtio-console @stdio:stdio_port`
    `add_virtual_device                       4 virtio-net tap=RT,mac_seed=${mac:0:17}-POST_RT_VM1`
    `add_virtual_device                       5 virtio-blk ./core-image-weston-intel-corei7-64.wic`
    --rtvm
    --lapic_pt
    `add_logger_settings                      console=4 kmsg=3 disk=5`
    POST_RT_VM1
)

echo "Launch device model with parameters: ${dm_params[@]}"
acrn-dm "${dm_params[@]}"

# Deinitializing
