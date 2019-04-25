#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# This script provides a quick and automatic setup of the SOS or UOS.
# You should run this script with root privilege since it will modify various system parameters.
#
# Usages:
#   Upgrade SOS to 28100 without reboot, it's highly recommended so that you can check configurations after upgrade SOS.
#     sudo <script> -s 28100 -d
#   Upgrade SOS to 28100 with specified proxy server
#     sudo <script> -s 28100 -p <your proxy server>:<port>
#   Upgrade UOS to 28100 with specified proxy server
#     sudo <script> -u 28100 -p <your proxy server>:<port>
#   Upgrade UOS to 28100 without downloading UOS image, you should put UOS image in /root directory previously.
#     sudo <script> -u 28100 -k

function print_help()
{
    echo "Usage:"
    echo "Launch this script as: sudo $0 -s 28100"
    echo -e "\t-s to upgrade SOS"
    echo -e "\t-u to upgrade UOS"
    echo -e "\t-p to specify a proxy server (HTTPS)"
    echo -e "\t-m to use swupd mirror url"
    echo -e "\t-k to skip downloading UOS; if enabled, you have to download UOS img firstly before upgrading, default is off"
    echo -e "\t-d to disable reboot device so that you can check the configurations after upgrading SOS"
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
# switcher for download UOS function
skip_download_uos=0
# switcher for disabling reboot device
disable_reboot=0

# acrn.conf path
acrn_conf_path=/usr/share/acrn/samples/nuc/acrn.conf
# acrn.efi path
acrn_efi_path=/usr/lib/acrn/acrn.efi

function upgrade_sos()
{
    # Check SOS version
    [[ `echo $sos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Please input right SOS version to upgrade" && exit 1
    [[ $VERSION_ID -gt $sos_ver ]] && echo "You're trying to install an older version of Clear Linux." && exit 1

    echo "Upgrading SOS..."

    # setup mirror and proxy url while specified with m and p options
    [[ -n $mirror ]] && echo "Setting swupd mirror to: $mirror" && swupd mirror -s $mirror
    [[ -n $proxy ]] && echo "Setting proxy to: $proxy" && export https_proxy=$proxy

    # Check EFI path exists.
    [[ ! -b $efi_partition ]] && echo "Please set the right EFI System partition firstly." && exit 1
    efi_mount_point=`findmnt $efi_partition -n | cut -d' ' -f1`
    root_partition=`echo $efi_partition | sed 's/1$/3/g'`
    partition=`echo $efi_partition | sed 's/1$//g;s/p$//g'`

    echo "Disable auto update..."
    swupd autoupdate --disable 2>/dev/null

    # Compare with current clear linux and skip upgrade SOS if get the same version.
    if [[ $VERSION_ID -eq $sos_ver ]]; then
        echo "Clear Linux version $sos_ver is already installed. Continuing to setup SOS..."
    else
        echo "Upgrading Clear Linux version from $VERSION_ID to $sos_ver ..."
        swupd verify --fix --picky -m $sos_ver 2>/dev/null
    fi

    # Do the setups if previous process succeed.
    if [[ $? -eq 0 ]]; then
        echo "Adding the service-os and kernel-iot-lts2018 bundles..."
        swupd bundle-add service-os kernel-iot-lts2018 2>/dev/null

        mount $efi_partition /mnt
        echo "Add /mnt/EFI/acrn folder"
        mkdir -p /mnt/EFI/acrn
        echo "Copy $acrn_conf_path /mnt/loader/entries/"
        if [[ ! -f $acrn_conf_path ]]; then
            echo "Missing acrn.conf file in folder: $acrn_conf_path"
            umount /mnt && sync
            exit 1
        fi
        cp -r $acrn_conf_path /mnt/loader/entries/
        if [[ $? -ne 0 ]]; then echo "Fail to copy $acrn_conf_path" && exit 1; fi
        echo "Copy $acrn_efi_path to /mnt/EFI/acrn"
        if [[ ! -f $acrn_efi_path ]]; then
            echo "Missing acrn.efi file in folder: $acrn_efi_path"
            umount /mnt && sync
            exit 1
        fi
        cp -r $acrn_efi_path /mnt/EFI/acrn/
        if [[ $? -ne 0 ]]; then echo "Fail to copy $acrn_efi_path" && exit 1; fi
        echo "Check ACRN efi boot event"
        check_acrn_bootefi=`efibootmgr | grep ACRN`
        if [[ "$check_arcn_bootefi" -ge "ACRN" ]]; then
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
        efibootmgr -c -l "\EFI\acrn\acrn.efi" -d $partition -p 1 -L "ACRN" >/dev/null

        echo "Create loader.conf"
        mv /mnt/loader/loader.conf /mnt/loader/loader.conf.bck && touch /mnt/loader/loader.conf
        echo "Add default (5 seconds) boot wait time"
        echo "timeout 5" > /mnt/loader/loader.conf
        echo "Add default boot to ACRN"
        echo "default acrn" >> /mnt/loader/loader.conf

        new_kernel=`ls /mnt/EFI/org.clearlinux/*sos* -tl | grep kernel | head -n1 | awk -F'/' '{print $5}'`
        echo "Getting latest Service OS kernel version: $new_kernel"
        cur_kernel=`cat /mnt/loader/entries/acrn.conf | sed -n 2p | cut -d'/' -f4`
        echo "Getting current Service OS kernel version: $cur_kernel"

        echo "Replacing root partition uuid in acrn.conf"
        sed -i "s/<UUID of rootfs partition>/`blkid -s PARTUUID -o value $root_partition`/g" /mnt/loader/entries/acrn.conf
        # test replacing succeed or not.
        if [[ -z `grep $(blkid -s PARTUUID -o value $root_partition) /mnt/loader/entries/acrn.conf` ]]; then
            echo "Fail to replace root uuid in this file: /mnt/loader/entries/acrn.conf"
            exit 1
        fi

        if [[ "$cur_kernel" == "$new_kernel" ]]; then
            echo "No need to replace kernel conf info"
        else
            echo "Replace with new SOS kernel in acrn.conf"
            sed -i "s/$cur_kernel/$new_kernel/g" /mnt/loader/entries/acrn.conf
        fi

        echo "Service OS setup done!"
    else
        echo "Fail to upgrade SOS to $sos_ver."
        echo "Please try upgrade SOS with this command:"
        echo "swupd update -m $sos_ver"
        exit 1
    fi

    umount /mnt
    sync
    [[ $disable_reboot == 0 ]] && echo "Rebooting Service OS to take effects." && reboot -f
}

function upgrade_uos()
{
    # Check UOS version
    [[ `echo $uos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Please input right UOS version to upgrade" && exit 1

    echo "Upgrading UOS..."

    # UOS download link
    uos_image_link="https://download.clearlinux.org/releases/$uos_ver/clear/clear-$uos_ver-kvm.img.xz"

    # Set proxy if needed.
    [[ -n $proxy ]] && echo "Setting proxy to: $proxy" && export https_proxy=$proxy
    # Corrupt script if /mnt is already mounted.
    if [[ ! -z `findmnt /mnt -n` ]]; then
        echo "/mnt is already mounted, please unmount it if you want to continue upgrade UOS."
        exit 1
    fi

    # Do upgrade UOS process.
    if [[ $skip_download_uos != 1 ]]; then
        cd ~
        echo "Downloading UOS image: $uos_image_link"
        curl $uos_image_link -o clear-$uos_ver-kvm.img.xz
        if [[ $? -ne 0 ]]; then
            echo "Download UOS failed."
            rm clear-$uos_ver-kvm.img.xz
            exit 1
        fi
    fi

    uos_img_xz=$(find ~/ -name clear-$uos_ver-kvm.img.xz)
    uos_img=$(find ~/ -name clear-$uos_ver-kvm.img)
    if [[ -f $uos_img ]] && [[ -f $uos_img.xz ]]; then echo "Moving $uos_img to $uos_img.old."; mv $uos_img $uos_img.old; fi
    if [[ ! -f $uos_img_xz ]] && [[ ! -f $uos_img ]]; then
        echo "You should download UOS clear-$uos_ver-kvm.img.xz file firstly." && exit 1
    fi
    if [[ -f $uos_img_xz ]]; then
        echo "Unxz UOS file: $uos_img_xz"
        unxz $uos_img_xz
        uos_img=`echo $uos_img_xz | sed 's/.xz$//g'`
    fi

    echo "Get UOS image: $uos_img"
    uos_partition=`losetup -f -P --show $uos_img`p3
    mount $uos_partition /mnt
    [[ $? -ne 0 ]] && echo "Fail to mount UOS partition" && exit 1
    cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
    umount /mnt
    sync

    cp -r /usr/share/acrn/samples/nuc/launch_uos.sh ~/launch_uos_$uos_ver.sh
    sed -i "s/\(virtio-blk.*\)\/home\/clear\/uos\/uos.img/\1$(echo $uos_img | sed "s/\//\\\\\//g")/" ~/launch_uos_$uos_ver.sh
    [[ -z `grep $uos_img ~/launch_uos_$uos_ver.sh` ]] && echo "Fail to replace uos image in launch script: ~/launch_uos_$uos_ver.sh" && exit 1
    echo "Upgrade UOS done..."
    echo "Now you can run this command to start UOS..."
    echo "sudo /root/launch_uos_$uos_ver.sh"
    exit
}

# Set script options.
while getopts "s:u:p:m:e:kdh" opt
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
                ?) print_help
                        ;;
        esac
done

# Check args
[[ $EUID -ne 0 ]] && echo "You have to run script as root." && exit 1
[[ -z $1 ]] && print_help
[[ -z $efi_partition ]] && efi_partition=/dev/sda1 || echo "Setting EFI System partition to: $efi_partition..."
[[ -n $sos_ver && -n $uos_ver ]] && echo "You should select upgrading SOS or UOS" && exit 1
[[ -n $uos_ver ]] && upgrade_uos || upgrade_sos
