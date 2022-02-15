#!/bin/bash
#
# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
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
        cpu_path="/sys/devices/system/cpu/cpu${processor_id}"
        if [ -f ${cpu_path}/online ]; then
            online=`cat ${cpu_path}/online`
            echo cpu${processor_id} online=${online} >> /dev/stderr
            if [ "${online}" = "1" ] && [ "${processor_id}" != "0" ]; then
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
    tap_exist=$(ip a | grep "$tap" | awk '{print $1}')
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
    # Each parameter of this function is considered the processor ID (as is reported in /proc/cpuinfo) of a CPU assigned
    # to a post-launched RTVM.

    if [ "${rtos_type}" != "no" ]; then
        offline_cpus $*
    fi

    cpu_list=$(local IFS=, ; echo "$*")
    echo -n "--cpu_affinity ${cpu_list}"
}

function add_rtvm_options() {
    if [ "${rtos_type}" = "Soft RT" ]; then
        echo -n "--rtvm"
    elif [ "${rtos_type}" = "Hard RT" ]; then
        echo -n "--rtvm --lapic_pt"
    fi
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
        tap_conf=${options%,*}
        create_tap "tap_${tap_conf#tap=}" >> /dev/stderr
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

    unbind_device $physical_bdf

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
