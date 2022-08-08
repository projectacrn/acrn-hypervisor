#!/bin/bash
# Copyright (C) 2020-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

cloud_image=focal-server-cloudimg-amd64.img
cloud_image_url=https://cloud-images.ubuntu.com/focal/current/focal-server-cloudimg-amd64.img
hmi_vm_image=hmi_vm.img
rt_vm_image=rt_vm.img
rt_kernel=(linux-libc linux-headers linux-image)

vm_type=$1
if [[ ${vm_type} != "rt-vm" ]] && [[ ${vm_type} != "hmi-vm" ]]; then
    cat <<EOT
Usage: $0 <vm_type>
This script creates VM images based on Ubuntu cloud images.

VM type options:
  hmi-vm        create a VM with GNOME desktop
  rt-vm         create a VM with a preempt-RT-patched kernel and real-time test utilities
EOT
    exit 1
fi

########################################
# Environment checks
########################################

if [[ ! -d /etc/schroot/chroot.d ]]; then
    echo "Package schroot is not installed."
    exit 1
fi

if [[ ! -d ../build ]]; then
    echo "Please make the SampleApplication at first."
    exit 1
fi

if [ ! -d mnt ]; then
    mkdir mnt
fi

########################################
# Helper functions
########################################

source logger.sh

########################################
# Actions defined as functions
########################################

function check_rt_kernel() {
    for file in ${rt_kernel[@]}
    do
        ls *.deb | grep ${file}
        if [ $? -eq 1 ]; then
            echo "RT VM kernel package ${file} is not found."
            exit
        fi
    done
}

function download_image() {
    local dest=$1
    local url=$2

    if [[ -f ${dest} ]]; then
        print_info "${dest} already exists. Do not redownload."
    else
        wget -O ${dest} ${url}
    fi
}

function copy_and_enlarge_image() {
    local source_image=$1
    local dest_image=$2
    local size_modifier=$3

    if [[ -f ${dest_image} ]]; then
        echo -n "${dest_image} already exists! Regenerate the image? (y/N)> "
        read answer
        if [[ $answer =~ ^[Yy]$ ]]; then
            rm ${dest_image}
        else
            exit 1
        fi
    fi

    qemu-img convert -f qcow2 -O raw ${source_image} ${dest_image} && \
        qemu-img resize -f raw ${dest_image} ${size_modifier} && \
        growpart ${dest_image} 1
}

function resizing_guest_root() {
    local part_file=$1

    sudo e2fsck -f ${part_file} && \
        sudo resize2fs ${part_file}
}

function mount_filesystem() {
    local part_file=$1
    local mount_point=$2

    # The symlink /etc/resolv.conf in a fresh cloud image is broken, which will
    # prevent schroot from working. Touch that linked file to work it around.
    mkdir -p ${mount_point} && \
        sudo mount ${part_file} ${mount_point} && \
        sudo mkdir -p ${mount_point}/run/systemd/resolve/ && \
        sudo touch ${mount_point}/run/systemd/resolve/stub-resolv.conf
}

function create_schroot_config() {
    local mount_point=$1
    local temp_file=$(mktemp /tmp/acrn-guest.XXXX)

    cat << EOF > ${temp_file}
[acrn-guest]
description=Contains ACRN guest root file system.
type=directory
directory=${mount_point}
users=root
root-groups=root
profile=desktop
personality=linux
preserve-environment=true
EOF

    sudo mv ${temp_file} /etc/schroot/chroot.d/acrn-guest && \
        sudo chown root:root /etc/schroot/chroot.d/acrn-guest
}

function setup_hmi_vm_rootfs() {
    local mount_point=$1

    sudo cp setup_hmi_vm.sh logger.sh ${mount_point}/ && \
	sudo cp ../build/userApp ${mount_point}/root && \
	sudo cp ../build/histapp.py ${mount_point}/root && \
        sudo schroot -c acrn-guest bash /setup_hmi_vm.sh && \
        sudo rm ${mount_point}/setup_hmi_vm.sh ${mount_point}/logger.sh
}

function setup_rt_vm_rootfs() {
    local mount_point=$1

    sudo cp *.deb ${mount_point}/root && \
	sudo cp ../build/rtApp ${mount_point}/root && \
        sudo mkdir ${mount_point}/root/scripts && \
        sudo cp configRTcores.sh ${mount_point}/root/scripts/ && \
        sudo cp setup_rt_vm.sh logger.sh ${mount_point}/ && \
        sudo schroot -c acrn-guest bash /setup_rt_vm.sh && \
        sudo rm ${mount_point}/setup_rt_vm.sh ${mount_point}/logger.sh
}

function cleanup() {
    local mount_point=$1
    local loop_dev=$2

    sudo umount ${mount_point}
    sudo rmdir ${mount_point}
    sudo kpartx -vd /dev/${loop_dev}
    sudo losetup -vd /dev/${loop_dev}
    true
}

########################################
# Do it!
########################################

mount_point=$(pwd)/mnt
if [[ ${vm_type} == "hmi-vm" ]]; then
    target_image=${hmi_vm_image}
    size_modifier="+4G"
elif [[ ${vm_type} == "rt-vm" ]]; then
    target_image=${rt_vm_image}
    size_modifier="+1G"
else
    echo "Internal error: undefined VM type '${vm_type}'"
    exit 1
fi

try_step "Download Ubuntu Focal cloud image" download_image ${cloud_image} ${cloud_image_url}
if [[ ${vm_type} == "rt-vm" ]]; then
    try_step "Check availability of RT kernel image" check_rt_kernel
fi
try_step "Creating an enlarged copy of ${cloud_image}" copy_and_enlarge_image ${cloud_image} ${target_image} ${size_modifier}

loop_dev=$(sudo kpartx -va ${target_image} 2>&1 | egrep -o -m 1 "loop[0-9]+")
print_info "Guest image loop-mounted at /dev/${loop_dev}"

try_step "Resizing guest root file system" resizing_guest_root /dev/mapper/${loop_dev}p1
try_step "Mounting guest root file system at ${mount_point}" mount_filesystem /dev/mapper/${loop_dev}p1 ${mount_point}
try_step "Preparing schroot configuration" create_schroot_config ${mount_point}

if [[ ${vm_type} == "hmi-vm" ]]; then
    try_step "Initializing guest root file system for HMI VM" setup_hmi_vm_rootfs ${mount_point}
else
    try_step "Initializing guest root file system for RT VM" setup_rt_vm_rootfs ${mount_point}
fi

do_step  "Cleaning up" cleanup ${mount_point} ${loop_dev}
print_info "VM image created at ${target_image}."
