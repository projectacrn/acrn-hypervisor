#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# This script provides a quick and automatic setup of the Service VM or User VM.
# You should run this script with root privilege since it will modify various system parameters.
#
# Usages:
#   Upgrade Service VM to 31080 without reboot, it's highly recommended so that you can check configurations after upgrade Service VM.
#     sudo <script> -s 31080 -d
#   Upgrade Service VM to 31080 with specified proxy server
#     sudo <script> -s 31080 -p <your proxy server>:<port>
#   Upgrade User VM to 31080 with specified proxy server
#     sudo <script> -u 31080 -p <your proxy server>:<port>
#   Upgrade User VM to 31080 without downloading User VM image, you should put User VM image in /root directory previously.
#     sudo <script> -u 31080 -k

function print_help()
{
    echo "Usage:"
    echo "Launch this script as: sudo $0 -s 31080"
    echo -e "\t-s to upgrade Service VM"
    echo -e "\t-u to upgrade User VM"
    echo -e "\t-p to specify a proxy server (HTTPS)"
    echo -e "\t-m to use swupd mirror url"
    echo -e "\t-k to skip downloading User VM; if enabled, you have to download User VM img firstly before upgrading, default is off"
    echo -e "\t-d to disable reboot device so that you can check the configurations after upgrading Service VM"
    echo -e "\t-i to set up Service VM with industry scenario"
    echo -e "\t-e to specify EFI System Partition (ESP), default: /dev/sda1"
    echo -e "\n\t<Note>:"
    echo -e "\tThis script is using /dev/sda1 as default EFI System Partition (ESP)."
    echo -e "\tThe ESP may be different based on your hardware and then you should specify it directly with '-e' option."
    echo -e "\tIt will typically be something like /dev/mmcblk0p1 on platforms that have an on-board eMMC"
    echo -e "\tOr /dev/nvme0n1p1 if your system has a non-volatile storage media attached via a PCI Express (PCIe) bus (NVMe)."
    exit 1
}

# get clear linux version previously
source /etc/os-release
# switcher for download User VM function
skip_download_uos=0
# switcher for disabling reboot device
disable_reboot=0

# acrn.efi path
acrn_efi_path=/usr/lib/acrn/acrn.nuc6cayh.sdc.efi

function upgrade_sos()
{
    # Check Service VM version
    [[ `echo $sos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Please input right Service VM version to upgrade" && exit 1
    [[ $VERSION_ID -gt $sos_ver ]] && echo "You're trying to install an older version of Clear Linux." && exit 1

    echo "Upgrading Service VM..."

    # setup mirror and proxy url while specified with m and p options
    [[ -n $mirror ]] && echo "Setting swupd mirror to: $mirror" && swupd mirror -s $mirror
    [[ -n $proxy ]] && echo "Setting proxy to: $proxy" && export https_proxy=$proxy

    # Check EFI path exists.
    [[ ! -b $efi_partition ]] && echo "Please set the right EFI System partition firstly." && exit 1
    partition=`echo $efi_partition | sed 's/1$//g;s/p$//g'`

    echo "Disable auto update..."
    swupd autoupdate --disable 2>/dev/null

    # Compare with current clear linux and skip upgrade Service VM if get the same version.
    if [[ $VERSION_ID -eq $sos_ver ]]; then
        echo "Clear Linux version $sos_ver is already installed. Continuing to setup Service VM..."
    else
        echo "Upgrading Clear Linux version from $VERSION_ID to $sos_ver ..."
        swupd repair --picky -V $sos_ver 2>/dev/null
    fi

    # Do the setups if previous process succeed.
    if [[ $? -eq 0 ]]; then
        echo "Adding the service-os and systemd-networkd-autostart bundles..."
        swupd bundle-add service-os systemd-networkd-autostart 2>/dev/null

        mount $efi_partition /mnt
        echo "Add /mnt/EFI/acrn folder"
        mkdir -p /mnt/EFI/acrn
        echo "Copy $acrn_efi_path to /mnt/EFI/acrn/acrn.efi"
        if [[ ! -f $acrn_efi_path ]]; then
            echo "Missing $acrn_efi_path file"
            umount /mnt && sync
            exit 1
        fi
        cp -a $acrn_efi_path /mnt/EFI/acrn/acrn.efi
        if [[ $? -ne 0 ]]; then echo "Fail to copy $acrn_efi_path" && exit 1; fi

        new_kernel=`ls /mnt/EFI/org.clearlinux/*sos* -tl | grep kernel | head -n1 | awk -F'/' '{print $5}'`
        new_kernel=${new_kernel#kernel-}
        echo "Getting latest Service OS kernel version: $new_kernel"

        echo "Add default (5 seconds) boot wait time."
        clr-boot-manager set-timeout 5 || { echo "Faild to add default boot wait time" && exit 1; }
        clr-boot-manager update

        echo "Set $new_kernel as default boot kernel."
        clr-boot-manager set-kernel $new_kernel || { echo "Fail to set $new_kernel as default boot kernel." && exit 1; }

        echo "Check ACRN efi boot event"
        check_acrn_bootefi=`efibootmgr | grep ACRN | wc -l`
        if [[ "$check_acrn_bootefi" -ge 1 ]]; then
            echo "Clean all ACRN efi boot event"
            efibootmgr | grep ACRN | cut -d'*' -f1 | cut -d't' -f2 | xargs -i efibootmgr -b {} -B >/dev/null
        fi
        echo "Check linux bootloader event"
        number=$(expr `efibootmgr | grep 'Linux bootloader' | wc -l` - 1)
        if [[ $number -ge 1 ]]; then
            echo "Clean all Linux bootloader event"
            efibootmgr | grep 'Linux bootloader' | cut -d'*' -f1 | cut -d't' -f2 | head -n$number | xargs -i efibootmgr -b {} -B >/dev/null
        fi

        echo "Add new ACRN efi boot event"
        efibootmgr -c -l "\EFI\acrn\acrn.efi" -d $partition -p 1 -L "ACRN" -u "uart=disabled" >/dev/null

        echo "Service OS setup done!"
    else
        echo "Fail to upgrade Service VM to $sos_ver."
        echo "Please try upgrade Service VM with this command:"
        echo "swupd update -V $sos_ver"
        exit 1
    fi

    umount /mnt
    sync
    [[ $disable_reboot == 0 ]] && echo "Rebooting Service OS to take effects." && reboot -f
}

function upgrade_uos()
{
    # Check User VM version
    [[ `echo $uos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Please input right User VM version to upgrade" && exit 1

    echo "Upgrading User VM..."

    # User VM download link
    uos_image_link="https://download.clearlinux.org/releases/$uos_ver/clear/clear-$uos_ver-kvm.img.xz"

    # Set proxy if needed.
    [[ -n $proxy ]] && echo "Setting proxy to: $proxy" && export https_proxy=$proxy
    # Corrupt script if /mnt is already mounted.
    if [[ ! -z `findmnt /mnt -n` ]]; then
        echo "/mnt is already mounted, please unmount it if you want to continue upgrade User VM."
        exit 1
    fi

    # Do upgrade User VM process.
    if [[ $skip_download_uos != 1 ]]; then
        cd ~
        echo "Downloading User VM image: $uos_image_link"
        curl $uos_image_link -o clear-$uos_ver-kvm.img.xz
        if [[ $? -ne 0 ]]; then
            echo "Download User VM failed."
            rm clear-$uos_ver-kvm.img.xz
            exit 1
        fi
    fi

    uos_img_xz=$(find ~/ -name clear-$uos_ver-kvm.img.xz)
    uos_img=$(find ~/ -name clear-$uos_ver-kvm.img)
    if [[ -f $uos_img ]] && [[ -f $uos_img.xz ]]; then echo "Moving $uos_img to $uos_img.old."; mv $uos_img $uos_img.old; fi
    if [[ ! -f $uos_img_xz ]] && [[ ! -f $uos_img ]]; then
        echo "You should download User VM clear-$uos_ver-kvm.img.xz file firstly." && exit 1
    fi
    if [[ -f $uos_img_xz ]]; then
        echo "Unxz User VM file: $uos_img_xz"
        unxz $uos_img_xz
        uos_img=`echo $uos_img_xz | sed 's/.xz$//g'`
    fi

    echo "Get User VM image: $uos_img"
    uos_loop_device=`losetup -f -P --show $uos_img`

    mount ${uos_loop_device}p3 /mnt || { echo "Fail to mount User VM rootfs partition" && exit 1; }
    mount ${uos_loop_device}p1 /mnt/boot || { echo "Fail to mount User VM EFI partition" && exit 1; }

    echo "Install kernel-iot-lts2018 to $uos_img"
    swupd bundle-add --path=/mnt kernel-iot-lts2018 || { echo "Fail to install kernel-iot-lts2018" && exit 1; }

    echo "Configure kernel-ios-lts2018 as $uos_img default boot kernel"
    uos_kernel_conf=`ls -t /mnt/boot/loader/entries/ | grep Clear-linux-iot-lts2018 | head -n1`
    uos_kernel=${uos_kernel_conf%.conf}
    echo "default $uos_kernel" > /mnt/boot/loader/loader.conf

    umount /mnt/boot
    umount /mnt
    sync

    cp -r /usr/share/acrn/samples/nuc/launch_uos.sh ~/launch_uos_$uos_ver.sh
    sed -i "s/\(virtio-blk.*\)\/home\/clear\/uos\/uos.img/\1$(echo $uos_img | sed "s/\//\\\\\//g")/" ~/launch_uos_$uos_ver.sh
    [[ -z `grep $uos_img ~/launch_uos_$uos_ver.sh` ]] && echo "Fail to replace uos image in launch script: ~/launch_uos_$uos_ver.sh" && exit 1
    echo "Upgrade User VM done..."
    echo "Now you can run this command to start User VM..."
    echo "sudo /root/launch_uos_$uos_ver.sh"
    exit
}

# Set script options.
while getopts "s:u:p:m:e:kdhi" opt
do
        case "$opt" in
                s) sos_ver="$OPTARG"
                        ;;
                u) uos_ver="$OPTARG"
                        ;;
                p) proxy="$OPTARG"
                        ;;
                m) mirror="$OPTARG"
                        ;;
                e) efi_partition="$OPTARG"
                        ;;
                k) skip_download_uos=1
                        ;;
                d) disable_reboot=1
                        ;;
                h) print_help
                        ;;
                i) acrn_efi_path=/usr/lib/acrn/acrn.kbl-nuc-i7.industry.efi
                        ;;
                ?) print_help
                        ;;
        esac
done

# Check args
[[ $EUID -ne 0 ]] && echo "You have to run script as root." && exit 1
[[ -z $1 ]] && print_help
[[ -z $efi_partition ]] && efi_partition=/dev/sda1 || echo "Setting EFI System partition to: $efi_partition..."
[[ -n $sos_ver && -n $uos_ver ]] && echo "You should select upgrading Service VM or User VM" && exit 1
[[ -n $uos_ver ]] && upgrade_uos || upgrade_sos
