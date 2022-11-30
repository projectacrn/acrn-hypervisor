#!/bin/bash
# Copyright (C) 2020-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

logger_prefix="(rt-vm-rootfs) "
source logger.sh

function umount_directory() {
    target_dir=$1
    umount -q ${target_dir} || true
}

function disable_os_prober() {
    if [[ -f /etc/grub.d/30_os-prober ]]; then
        mv /etc/grub.d/30_os-prober /etc/grub.d/.30_os-prober
    fi
}

function update_package_info() {
    apt update -y
}

function install_tools() {
    apt install rt-tests -y
}

function update_kernel_cmdline() {
    cat <<EOF >> /etc/default/grub

GRUB_CMDLINE_LINUX="rootwait rootfstype=ext4 console=ttyS0,115200 console=tty0 rw nohpet console=hvc0 no_timer_check ignore_loglevel log_buf_len=16M consoleblank=0 tsc=reliable clocksource=tsc tsc=reliable x2apic_phys processor.max_cstate=0 intel_idle.max_cstate=0 intel_pstate=disable mce=ignore_ce audit=0 isolcpus=nohz,domain,1 nohz_full=1 rcu_nocbs=1 nosoftlockup idle=poll irqaffinity=0 no_ipi_broadcast=1"
EOF
}

function install_rt_kernel() {
    search_dir=$1
    for file in $(ls -r ${search_dir}/*acrn-kernel-*.deb)
    do
        cp ${file} /tmp
        sudo apt install /tmp/${file##*/} -y
    done
}

function change_root_password() {
    passwd root
}

function disable_services() {
    services=(systemd-timesyncd.service \
                  systemd-journal-flush.service \
                  apt-daily.service \
                  apt-daily-upgrade.service \
                  snapd.autoimport.service \
                  snapd.seeded.service)

    for service in ${services[*]}
    do
        systemctl disable ${service}
        systemctl mask ${service}
    done

    for timer in $(systemctl list-unit-files | grep -o "^.*\.timer"); do
        systemctl disable ${timer}
    done

    apt-get remove unattended-upgrades -y
 }

# Change current working directory to the root to avoid "target is busy" errors
# on unmounting.
cd /

try_step "Unmounting /root" umount_directory /root
try_step "Unmounting /home" umount_directory /home
try_step "Disabling GRUB OS prober" disable_os_prober
try_step "Updating package information" update_package_info
try_step "Installing tools" install_tools
try_step "Updating kernel command line" update_kernel_cmdline
try_step "Installing RT kernel" install_rt_kernel /root
try_step "Changing the password of the root user" change_root_password
try_step "Disabling services that impact real-time performance" disable_services
