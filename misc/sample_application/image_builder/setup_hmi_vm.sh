#!/bin/bash
# Copyright (C) 2020-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

logger_prefix="(hmi-vm-rootfs) "
source /root/.bashrc
source logger.sh

function umount_directory() {
    target_dir=$1
    umount -q ${target_dir} || true
}

function update_package_info() {
    apt update -y && apt install python3 python3-pip \
        net-tools python3-matplotlib -y
    pip3 install flask numpy pandas posix_ipc

}

function install_desktop() {
    apt install ubuntu-gnome-desktop
}

function change_root_password() {
    passwd root
}

function add_normal_user() {
    useradd -s /bin/bash -d /home/acrn/ -m -G sudo acrn && \
        passwd acrn
}

# Change current working directory to the root to avoid "target is busy" errors
# on unmounting.
cd /

try_step "Unmounting /root" umount_directory /root
try_step "Unmounting /home" umount_directory /home
try_step "Updating package information" update_package_info
try_step "Installing GNOME desktop" install_desktop
try_step "Changing the password of the root user" change_root_password
try_step "Adding the normal user acrn" add_normal_user
