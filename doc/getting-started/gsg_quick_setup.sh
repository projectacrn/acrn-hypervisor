#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# This script provides a quick and automatic setup of the SOS or UOS.
# You can also upgrade the SOS or UOS with a specified Clear Linux version.
# Also you should know that system may have boot problems once the script is running failed
# since it will modify various system parameters with full root access.
#
# Usages:
#   Upgrade sos to 28100
#     sudo <script> -t 28100
#   Upgrade sos to 28100 with specified proxy server
#     sudo <script> -t 28100 -p <your proxy server>:<port>
#   Upgrade uos to 28100 with specified proxy server
#     sudo <script> -t 28100 -p <your proxy server>:<port> -u
#   Upgrade uos to 28100 without downloading uos image, you should put uos image in /root directory previously.
#     sudo <script> -t 28100 -u -s


function print_help()
{
echo "Usage:"
echo "Launch this script as: sudo $0 -t 28100"
echo -e "\t-t to specify clear linux version for upgrading"
echo -e "\t-p to specify a proxy server"
echo -e "\t-m to use swupd mirror url"
echo -e "\t-u to upgrade UOS."
echo -e "\t-s, skip downloading uos; if enabled, you have to download uos img firstly before upgrading, default is off"
exit 1
}

# switch for download uos function
skip_download_uos=0
# turn on if you need to setup uos
upgrade_uos=0

# acrn.conf path
acrn_conf_path=/usr/share/acrn/samples/nuc/acrn.conf
# acrn.efi path
acrn_efi_path=/usr/lib/acrn/acrn.efi

function upgrade_sos()
{
# setup mirror and proxy url while specified with m and p options
if [[ -n $mirror ]]; then
    echo "Setting swupd mirror to: $mirror"
    swupd mirror -s $mirror
fi
if [[ -n $proxy ]]; then
    echo "Setting proxy to: $proxy"
    export https_proxy=$proxy
fi

# get partitions
type_efi=EFI\ System
partition_number=`fdisk -l | grep "$type_efi" | awk -F' ' '{print $1}' | wc -l`
if [ $partition_number != "1" ]; then
    echo "Multiple partitions detected."
    c=0
    for i in `fdisk -l | grep "$type_efi" | awk -F' ' '{print $1}'`; do
        if [[ ! -z $efi_partition ]]; then break; fi
        c=$((c+1))
        echo "$c. $i"
        while true; do
            read -p "Do you wish to setup on this partition ? (Y/N)" yn
            case $yn in
                [Yy]* ) efi_partition=$i; break;;
                [Nn]* ) break;;
                * ) echo "Please input yes or no.";;
            esac
        done
    done
else
    efi_partition=`fdisk -l | grep "$type_efi" | awk -F' ' '{print $1}'`
fi

if [[ -z $efi_partition ]]; then echo "You didn't choose a partition, please try it again."; exit 1; fi
efi_mount_point=`findmnt $efi_partition -n | cut -d' ' -f1`
root_partition=`echo $efi_partition | sed 's/[0-9]$/3/g'`
partition=`echo $efi_partition | sed 's/[0-9]$//g'`
# unmount efi partition if it's already mounted
if [[ -n $efi_mount_point ]]; then
    echo "Unmount EFI partition: $efi_mount_point..."
    umount $efi_mount_point
fi

echo "Disable auto update..."
swupd autoupdate --disable

# Compare with current clear linux and skip upgrade sos if get the same version.
source /etc/os-release
if [[ $VERSION_ID -eq $target_version ]]; then
    echo "Specify with the same clear linux version, continue to setup sos..."
else
    echo "Upgrade clear linux version to : $target_version ..."
    swupd verify --fix --picky -m $target_version
fi

# Do the setups if previous process succeed.
if [[ $? -eq 0 ]]; then
    echo "Add service-os kernel-iot-lts2018 bundle"
    swupd bundle-add service-os kernel-iot-lts2018

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
    if [[ $? -ne 0 ]]; then echo "You didn't choose an right EFI partition." && exit 1; fi
    echo "Copy $acrn_efi_path to /mnt/EFI/acrn"
    if [[ ! -f $acrn_efi_path ]]; then
        echo "Missing acrn.efi file in folder: $acrn_efi_path"
        umount /mnt && sync
        exit 1
    fi
    cp -r $acrn_efi_path /mnt/EFI/acrn/
    echo "Check ACRN efi boot event"
    check_acrn_bootefi=`efibootmgr | grep ACRN`
    if [[ "$check_arcn_bootefi" -ge "ACRN" ]]; then
        echo "Clean all ACRN efi boot event"
        efibootmgr | grep ACRN | cut -d'*' -f1 | cut -d't' -f2 | xargs -i efibootmgr -b {} -B
    fi
    echo "Check linux bootloader event"
    number=$(expr `efibootmgr | grep 'Linux bootloader' | wc -l` - 1)
    if [[ $number -ge 1 ]]; then
        echo "Clean all Linux bootloader event"
        efibootmgr | grep 'Linux bootloader' | cut -d'*' -f1 | cut -d't' -f2 | head -n$number | xargs -i efibootmgr -b {} -B
    fi

    echo "Add new ACRN efi boot event"
    efibootmgr -c -l "\EFI\acrn\acrn.efi" -d $partition -p 1 -L "ACRN"

    echo "Create loader.conf"
    mv /mnt/loader/loader.conf /mnt/loader/loader.conf.bck && touch /mnt/loader/loader.conf
    echo "Add default boot wait time"
    echo "timeout 5" > /mnt/loader/loader.conf
    echo "Add default boot to ACRN"
    echo "default acrn" >> /mnt/loader/loader.conf

    new_kernel=`ls /mnt/EFI/org.clearlinux/*sos* -tl | grep kernel | head -n1 | awk -F'/' '{print $5}'`
    echo "Getting latest Service OS kernel version: $new_kernel"
    cur_kernel=`cat /mnt/loader/entries/acrn.conf | sed -n 2p | cut -d'/' -f4`
    echo "Getting current Service OS kernel version: $cur_kernel"

    echo "Replacing root partition uuid in acrn.conf"
    sed -i "s/<UUID of rootfs partition>/`blkid -s PARTUUID -o value $root_partition`/g" /mnt/loader/entries/acrn.conf

    if [[ "$cur_kernel" == "$new_kernel" ]]; then
        echo "No need to replace kernel conf info"
    else
        echo "Replace with new sos kernel in acrn.conf"
        sed -i "s/$cur_kernel/$new_kernel/g" /mnt/loader/entries/acrn.conf
    fi

    echo "Service OS setup done!"
    echo "Rebooting Service OS to taking effect changes."
else
    echo -e "Fail to upgrade sos $target_clearlinux from mirror url."
fi

umount /mnt
sync
reboot
}

function upgrade_uos()
{
# UOS download link
uos_image_link="https://download.clearlinux.org/releases/$target_version/clear/clear-$target_version-kvm.img.xz"

# Set proxy if needed.
if [[ -n $proxy ]]; then
    echo "Setting proxy to: $proxy"
    export https_proxy=$proxy
fi

if [[ ! -z `findmnt /mnt -n` ]]; then umount /mnt; fi

# Do upgrade uos process.
if [[ $skip_download_uos == 1 ]]; then
    uos_img_xz=$(find ~/ -name clear-$target_version-kvm.img.xz)
    uos_img=$(find ~/ -name clear-$target_version-kvm.img)
    if [[ ! -f $uos_img_xz ]] && [[ ! -f $uos_img ]]; then
        echo "You should download uos clear-$target_version-kvm.img.xz file firstly." && exit 1
    fi
    if [[ -f $uos_img_xz ]]; then
        echo "Unxz uos file: $uos_img_xz"
        unxz $uos_img_xz
        uos_img=`echo $uos_img_xz | sed 's/.xz$//g'`
    fi
    echo "get uos img file: $uos_img"
    if [[ $? -eq 0 ]]; then
        uos_partition=`losetup -f -P --show $uos_img`p3
    else
        echo "fail to losetup uos img, please check uos img status" && exit 1
    fi
    mount $uos_partition /mnt
    cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
else
    cd ~
    echo "Downloading UOS image: $uos_image_link"
    curl $uos_image_link -o clear-$target_version-kvm.img.xz
    uos_img=clear-$target_version-kvm.img
    if [[ -f $uos_img ]] && [[ -f $uos_img.xz ]]; then echo "Moving $uos_img to $uos_img.old."; mv $uos_img $uos_img.old; fi
    if [[ $? -eq 0 ]]; then
        echo "Unxz UOS image: clear-$target_version-kvm.img.xz"
        unxz clear-$target_version-kvm.img.xz
    fi
    if [[ $? -eq 0 ]]; then
        uos_partition=`losetup -f -P --show $uos_img`p3
    else
        echo "Missing UOS file: $uos_img, please check if UOS is exist." && exit 1
    fi
    mount $uos_partition /mnt
    cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
fi
umount /mnt
losetup -D
sync

cp -r /usr/share/acrn/samples/nuc/launch_uos.sh ~/
sed -i "s/\(virtio-blk.*\)\/home\/clear\/uos\/uos.img/\1$(echo $uos_img | sed "s/\//\\\\\//g")/" ~/launch_uos.sh
echo "Upgrade UOS done..."
echo "Now you can run this command to start UOS..."
echo "# cd ~/ && ./launch_uos.sh"
}

# Set script options.
while getopts "t:p:m:huso" opt
do
        case "$opt" in
                t) target_version="$OPTARG"
                        ;;
                p) proxy="$OPTARG"
                        ;;
                m) mirror="$OPTARG"
                        ;;
                u) upgrade_uos=1
                        ;;
                s) skip_download_uos=1
                        ;;
                h) print_help
                        ;;
                ?) print_help
                        ;;
        esac
done

# -t opt is must
if [[ -z $1 || $EUID -ne 0 ]]; then
    print_help
fi

# You need to specify the correct clear linux ver number.
if [[ `echo $target_version | awk '{print length($0)}'` -ne 5 ]]; then
    echo 'Please input right clearlinux version to upgrade' && exit 1
fi

if [[ $upgrade_uos == 1 ]]; then
    echo "Upgrading UOS to: $target_version ..."
    upgrade_uos
    exit
fi

# Exit script if you specified an older clear linux version.
if [[ $(echo -e `swupd info` | cut -d' ' -f3) -gt $target_version ]]; then
    echo -e 'No need to upgrade Service OS.' && exit
else
    upgrade_sos
    exit
fi
