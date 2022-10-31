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
        net-tools python3-matplotlib \
	linux-modules-extra-$(uname -r) \
	openssh-server \
	isc-dhcp-server -y
    pip3 install flask 'numpy>=1.18.5' pandas posix_ipc

}

function install_desktop() {
    apt install ubuntu-gnome-desktop -y
}

function change_root_password() {
    passwd root
}

function enable_root_login() {
    sed -i -e '3 s/^/#/' /etc/pam.d/gdm-password
    sed -i 's/\[daemon\]/& \n  AllowRoot=true /' /etc/gdm3/custom.conf
}

function add_normal_user() {
    useradd -s /bin/bash -d /home/acrn/ -m -G sudo acrn && \
        passwd acrn
}

function enable_services() {
    services=(ssh.service isc-dhcp-server)
    for service in ${services[*]}
    do
        systemctl enable ${service}
	systemctl unmask ${service}
    done
}

function config_ssh() {

    sudo sed -ie 's/PasswordAuthentication no/PasswordAuthentication yes/g' \
	    /etc/ssh/sshd_config
    sudo ssh-keygen -t dsa -f /etc/ssh/ssh_host_dsa_key
    sudo ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key
}

# Change current working directory to the root to avoid "target is busy" errors
# on unmounting.
cd /

try_step "Unmounting /root" umount_directory /root
try_step "Unmounting /home" umount_directory /home
try_step "Updating package information" update_package_info
try_step "Installing GNOME desktop" install_desktop
try_step "Changing the password of the root user" change_root_password
try_step "Enable root user login" enable_root_login
try_step "Adding the normal user acrn" add_normal_user
try_step "Configure the ssh service" config_ssh
