#!/bin/bash
# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
# This script is aim to setup or upgrade sos & uos automatically.

# switch for download uos function
skip_download_uos=0
# turn on if you need to setup uos
upgrade_uos=0

function print_help()
{
echo "Usage:"
echo "Launch this script as: ./flash.sh -t 28100"
echo "-t to specify clear linux version for upgrading"
echo "-p to specify a proxy server"
echo "-m to use swupd mirror url"
echo "-u to upgrade uos, if you just upgrade sos using this script, you'd better to manually reboot sos then continue to use this function."
echo "-s, skip downloading uos; if enabled, you have to download uos img firstly before upgrading, default is off"
exit 1
}

function upgrade_sos()
{
# setup mirror, proxy env if specified parameters to p & m opt.
swupd mirror -u
if [[ -n $mirror ]]; then
    echo "Add swupd mirror..."
    swupd mirror -s $mirror
fi
unset https_proxy
if [[ -n $proxy ]]; then
    echo "Add proxy..."
    export https_proxy=$proxy
fi
echo "disable auto update..."
swupd autoupdate --disable

# Compare with current clear linux and skip upgrade sos if get the same version.
if [[ $(echo -e `swupd info` | cut -d' ' -f3) -eq $target_version ]]; then
    echo "Specified to the same clear linux version, continue to setup sos env."
else
    echo "Upgrade clear linux version to : $target_version ..."
    swupd verify --fix --picky -m $target_version
fi

# Do the setups if previous process succeed.
if [[ $? -eq 0 ]]; then
    echo "Add service-os kernel-iot-lts2018 bundle"
    swupd bundle-add service-os kernel-iot-lts2018

    echo "mount /dev/sda1 to /mnt"
    mount /dev/sda1 /mnt

    echo "Add /mnt/EFI/acrn folder"
    mkdir -p /mnt/EFI/acrn
    echo "Copy /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/"
    cp -r /usr/share/acrn/samples/nuc/acrn.conf /mnt/loader/entries/
    echo "Copy acrn.efi to /mnt/EFI/acrn"
    cp -r /usr/lib/acrn/acrn.efi /mnt/EFI/acrn/

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
    efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/sda -p 1 -L "ACRN"

    echo "Create loader.conf"
    rm /mnt/loader/loader.conf && touch /mnt/loader/loader.conf
    echo "Add default boot wait time"
    echo "timeout 5" > /mnt/loader/loader.conf
    echo "Add default boot to ACRN"
    echo "default acrn" >> /mnt/loader/loader.conf

    echo "Get latest sos file from EFI org.clearlinux: "
    new_kernel=`ls /mnt/EFI/org.clearlinux/*sos* -tl | grep kernel | head -n1 | awk -F'/' '{print $5}'`
    echo "Get current sos kernel from acrn.conf file: "
    cur_kernel=`cat /mnt/loader/entries/acrn.conf | sed -n 2p | cut -d'/' -f4`

    echo "Replace root uuid from acrn.conf"
    sed -i "s/<UUID of rootfs partition>/`blkid -s PARTUUID -o value /dev/sda3`/g" /mnt/loader/entries/acrn.conf

    if [[ "$cur_kernel" == "$new_kernel" ]]; then
        echo "No need to replace kernel conf info"
    else
        echo "Replace with new sos kernel in acrn.conf"
        sed -i "s/$cur_kernel/$new_kernel/g" /mnt/loader/entries/acrn.conf
    fi

    echo "Sos process done!"
    echo " ***** Please reboot device to test on new sos. *****"
else
    echo -e "Fail to upgrade sos $target_clearlinux from mirror url."
fi

echo "Init swupd mirror & proxy"
swupd mirror -u
unset https_proxy
umount /mnt
}

function upgrade_uos()
{
# Add proxy if you need to download uos img automatically.
if [[ -n $proxy ]]; then
    echo "Add proxy..."
    export https_proxy=$proxy
fi

# Do upgrade uos process.
if [[ $skip_download_uos == 1 ]]; then
    uos_img_xz_path=$(find ~/ -name clear-$target_version-kvm.img.xz)
    uos_img_path=$(find ~/ -name clear-$target_version-kvm.img)
    if [[ ! -f $uos_img_xz_path ]] && [[ ! -f $uos_img_path ]]; then
        echo "You should download uos clear-$target_version-kvm.img.xz file firstly." && exit
    fi
    if [[ -f $uos_img_xz_path ]]; then
        echo "get uos xz file in: $uos_img_xz_path"
        unxz $uos_img_xz_path
        uos_img_path=$(find ~/ -name clear-$target_version-kvm.img)
    fi
    echo "get uos img file in: $uos_img_path"
    if [[ $? -eq 0 ]]; then
        losetup -f -P --show $uos_img_path
    else
        echo "fail to losetup uos img, please check uos img status" && exit 1
    fi
    mount /dev/loop0p3 /mnt
    cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
    umount /mnt
    sync
else
    cd ~
    curl https://download.clearlinux.org/releases/$target_version/clear/clear-$target_version-kvm.img.xz -o clear-$target_version-kvm.img.xz
    if [[ $? -eq 0 ]]; then
        unxz clear-$target_version-kvm.img.xz
    fi
    if [[ $? -eq 0 ]]; then
        losetup -f -P --show clear-$target_version-kvm.img
    fi
    mount /dev/loop0p3 /mnt
    cp -r /usr/lib/modules/"`readlink /usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`.iot-lts2018" /mnt/lib/modules
    umount /mnt
    sync
fi
uos_img_path=$(find ~/ -name clear-$target_version-kvm.img)
cp -r /usr/share/acrn/samples/nuc/launch_uos.sh ~/
sed -i "s/\(virtio-blk.*\)\/home\/clear\/uos\/uos.img/\1$(echo $uos_img_path | sed "s/\//\\\\\//g")/" ~/launch_uos.sh
echo "Upgrade uos done..."
echo "Now you can run below command to start UOS..."
echo "# cd ~/ && ./launch_uos.sh -V 1"
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
if [[ -z $1 ]]; then
    print_help
fi

# You need to specify the correct clear linux ver number.
if [[ `echo $target_version | awk '{print length($0)}'` -ne 5 ]]; then
    echo 'Please input right clearlinux version to upgrade' && exit 1
fi

if [[ $upgrade_uos == 1 ]]; then
    echo "Start upgrading uos to: $target_version ..."
    upgrade_uos
    exit
fi

# Exit script if you specified an newer clear linux ver.
if [[ $(echo -e `swupd info` | cut -d' ' -f3) -gt $target_version ]]; then
    echo -e 'No need to upgrade sos.' && exit
else
    upgrade_sos
    exit
fi
