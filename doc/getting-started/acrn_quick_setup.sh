#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# This script provides a quick and automatic setup of the Service VM or User VM.
# Run this script with root privilege since it will modify various system parameters.
#
# Usages:
#   Upgrade Service VM to 31080 without reboot; this is highly recommended so that you can check configurations after the upgrade Service VM.
#     sudo <script> -s 31080 -d
#   Upgrade Service VM to 31080 with the specified proxy server
#     sudo <script> -s 31080 -p <your proxy server>:<port>
#   Upgrade User VM to 31080 with the specified proxy server
#     sudo <script> -u 31080 -p <your proxy server>:<port>
#   Upgrade User VM to 31080 without downloading the User VM image; you should put the User VM image in the /root directory previously.
#     sudo <script> -u 31080 -k

function print_help()
{
    echo "Usage:"
    echo "Launch this script as: sudo $0 -s 31470"
    echo -e "\t-s to upgrade the Service VM"
    echo -e "\t-u to upgrade the User VM"
    echo -e "\t-p to specify a proxy server (HTTPS)"
    echo -e "\t-m to use the swupd mirror url"
    echo -e "\t-k to skip downloading the User VM; if enabled, you must download the User VM image before you upgrade. Default is off"
    echo -e "\t-d to disable the reboot device so that you can check configurations after you upgrade the Service VM"
    echo -e "\t-i to set up the Service VM with the industry scenario"
    echo -e "\t-e to specify the EFI System Partition (ESP); default: /dev/sda1"
    echo -e "\n\t<Note>:"
    echo -e "\tThis script is using /dev/sda1 as the default EFI System Partition (ESP)."
    echo -e "\tThe ESP may be different based on your hardware; in this case, specify it directly with the '-e' option."
    echo -e "\tThis will typically be something like /dev/mmcblk0p1 on platforms that have an on-board eMMC"
    echo -e "\tOr /dev/nvme0n1p1 if your system has a non-volatile storage media attached via a PCI Express (PCIe) bus (NVMe)."
    exit 1
}

# get the previous Clear Linux version
source /etc/os-release
# switcher for downloading the User VM function
skip_download_uos=0
# switcher for disabling the reboot device
disable_reboot=0
# set default scenario name
scenario=sdc
# swupd config file path
swupd_config=/usr/share/defaults/swupd/config

function upgrade_sos()
{
    # Check Service VM version
    [[ `echo $sos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Input the correct Service VM version to which to upgrade" && exit 1
    [[ $VERSION_ID -gt $sos_ver ]] && echo "You are attempting to install an older version of Clear Linux." && exit 1

    echo "Upgrading Service VM..."

    # get board name
    BOARD_NAME=`cat /sys/devices/virtual/dmi/id/board_name | cut -d' ' -f1`
    BOARD_NAME=${BOARD_NAME,,}
    [[ -z $BOARD_NAME ]] && echo "Unknown board name." && exit 1
    echo "Board name is: $BOARD_NAME"
 
    # set up mirror and proxy url while specified with m and p options
    [[ -n $mirror ]] && echo "Setting swupd mirror to: $mirror" && \
        sed -i 's/#allow_insecure_http=<true\/false>/allow_insecure_http=true/' $swupd_config && \
        swupd mirror -s $mirror
    [[ -n $proxy ]] && echo "Setting proxy to: $proxy" && export https_proxy=$proxy

    # Check that the EFI path exists.
    [[ ! -b $efi_partition ]] && echo "Set the right EFI System partition first." && exit 1
    partition=`echo $efi_partition | sed 's/1$//g;s/p$//g'`

    echo "Disable auto update..."
    swupd autoupdate --disable 2>/dev/null

    # Compare with the current Clear linux and skip the upgrade Service VM if you get the same version.
    if [[ $VERSION_ID -eq $sos_ver ]]; then
        echo "Clear Linux version $sos_ver is already installed. Continuing to set up the Service VM..."
    else
        echo "Upgrading the Clear Linux version from $VERSION_ID to $sos_ver ..."
        swupd repair -x --picky -V $sos_ver 2>/dev/null
    fi

    # Do the setups if previous process succeed.
    if [[ $? -eq 0 ]]; then
        [[ -n $mirror ]] && sed -i 's/#allow_insecure_http=<true\/false>/allow_insecure_http=true/' $swupd_config
        echo "Adding the service-os and systemd-networkd-autostart bundles..."
        swupd bundle-add service-os systemd-networkd-autostart 2>/dev/null

        # get acrn.efi path
        acrn_efi_path=/usr/lib/acrn/acrn.$BOARD_NAME.$scenario.efi
        if [[ $BOARD_NAME == "wl10" ]] && [[ ! -f $acrn_efi_path ]]; then
            echo "$acrn_efi_path does not exist."
            echo "Using /usr/lib/acrn/acrn.nuc7i7dnb.industry.efi instead."
            set -x
            cp -r /usr/lib/acrn/acrn.nuc7i7dnb.industry.efi $acrn_efi_path
            { set +x; } 2>/dev/null
        fi
        if [[ ! -f $acrn_efi_path ]]; then
            echo "$acrn_efi_path doesn't exist."
            echo "Use one of these efi images from /usr/lib/acrn."
            echo "------"
            ls /usr/lib/acrn/acrn.*.$scenario.efi -1
            echo "------"
            echo "Copy the efi image to $acrn_efi_path, then run the script again."
            exit 1
        fi

        mount $efi_partition /mnt
        echo "Add /mnt/EFI/acrn folder"
        mkdir -p /mnt/EFI/acrn
        echo "Copy $acrn_efi_path to /mnt/EFI/acrn/acrn.efi"
        if [[ ! -f $acrn_efi_path ]]; then
            echo "Missing $acrn_efi_path file"
            umount /mnt && sync
            exit 1
        fi
        cp -r $acrn_efi_path /mnt/EFI/acrn/acrn.efi
        if [[ $? -ne 0 ]]; then echo "Failed to copy $acrn_efi_path" && exit 1; fi

        new_kernel=`ls /usr/lib/kernel/org*sos* -tl | head -n1 | awk -F'/' '{print $5}'`
        echo "Getting the latest Service OS kernel version: $new_kernel"

        echo "Add the default (5 seconds) boot wait time."
        clr-boot-manager set-timeout 5 || { echo "Failed to add the default boot wait time" && exit 1; }
        clr-boot-manager update
        echo "Set $new_kernel as the default boot kernel."
        clr-boot-manager set-kernel $new_kernel || { echo "Failed to set $new_kernel as the default boot kernel." && exit 1; }

        # Rename Clear-linux-iot-lts2018-sos conf to acrn.conf
        conf_directory=/mnt/loader/entries/
        conf=`sed -n 2p /mnt/loader/loader.conf | sed "s/default //" | sed "s/.conf$//"`
        cp -r ${conf_directory}${conf}.conf ${conf_directory}acrn.conf 2>/dev/null || \
        { echo "${conf_directory}${conf}.conf does not exist." && exit 1; }
        sed -i 2"s/$conf/acrn/" /mnt/loader/loader.conf

        echo "Check the ACRN efi boot event"
        check_acrn_bootefi=`efibootmgr | grep ACRN | wc -l`
        if [[ "$check_acrn_bootefi" -ge 1 ]]; then
            echo "Clean all ACRN efi boot events"
            efibootmgr | grep ACRN | cut -d'*' -f1 | cut -d't' -f2 | xargs -i efibootmgr -b {} -B >/dev/null
        fi
        echo "Check the Linux bootloader event"
        number=$(expr `efibootmgr | grep 'Linux bootloader' | wc -l` - 1)
        if [[ $number -ge 1 ]]; then
            echo "Clean all Linux bootloader events"
            efibootmgr | grep 'Linux bootloader' | cut -d'*' -f1 | cut -d't' -f2 | head -n$number | xargs -i efibootmgr -b {} -B >/dev/null
        fi

        echo "Add new ACRN efi boot events; uart is disabled by default."
        set -x
        efibootmgr -c -l "\EFI\acrn\acrn.efi" -d $partition -p 1 -L "ACRN" -u "uart=disabled " >/dev/null
        { set +x; } 2>/dev/null
        echo "Service OS setup is complete!"
    else
        echo "Failed to upgrade the Service VM to $sos_ver."
        echo "Upgrade the Service VM with this command:"
        echo "swupd update -V $sos_ver"
        exit 1
    fi

    umount /mnt
    sync
    [[ $disable_reboot == 0 ]] && echo "Rebooting the Service OS to take effect." && reboot -f
}

function upgrade_uos()
{
    # Check User VM version
    [[ `echo $uos_ver | awk '{print length($0)}'` -ne 5 ]] && echo "Input the correct User VM version to which to upgrade" && exit 1

    echo "Upgrading User VM..."

    # User VM download link
    uos_image_link="https://download.clearlinux.org/releases/$uos_ver/clear/clear-$uos_ver-kvm.img.xz"

    # Set proxy if needed.
    [[ -n $proxy ]] && echo "Setting the proxy to: $proxy" && export https_proxy=$proxy
    # Corrupt script if /mnt is already mounted.
    if [[ ! -z `findmnt /mnt -n` ]]; then
        echo "/mnt is already mounted; unmount it if you want to continue upgrading the User VM."
        exit 1
    fi

    # Do upgrade User VM process.
    if [[ $skip_download_uos != 1 ]]; then
        cd ~
        echo "Downloading the User VM image: $uos_image_link"
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
        echo "Download the User VM clear-$uos_ver-kvm.img.xz file first." && exit 1
    fi
    if [[ -f $uos_img_xz ]]; then
        echo "Unxz User VM file: $uos_img_xz"
        unxz $uos_img_xz
        uos_img=`echo $uos_img_xz | sed 's/.xz$//g'`
    fi

    echo "Get the User VM image: $uos_img"
    uos_loop_device=`losetup -f -P --show $uos_img`

    mount ${uos_loop_device}p3 /mnt || { echo "Failed to mount the User VM rootfs partition" && exit 1; }
    mount ${uos_loop_device}p1 /mnt/boot || { echo "Failed to mount the User VM EFI partition" && exit 1; }

    # set up mirror and proxy url while specified with m and p options
    [[ -n $mirror ]] && echo "Setting swupd mirror to: $mirror" && \
        sed -i 's/#allow_insecure_http=<true\/false>/allow_insecure_http=true/' /mnt$swupd_config && \
        swupd mirror -s $mirror --path=/mnt

    echo "Install kernel-iot-lts2018 to $uos_img"
    swupd bundle-add --path=/mnt kernel-iot-lts2018 || { echo "Failed to install kernel-iot-lts2018" && \
	    sync && umount /mnt/boot /mnt && exit 1; }

    echo "Configure kernel-ios-lts2018 as $uos_img default boot kernel"
    uos_kernel_conf=`ls -t /mnt/boot/loader/entries/ | grep Clear-linux-iot-lts2018 | head -n1`
    uos_kernel=${uos_kernel_conf%.conf}
    echo "default $uos_kernel" > /mnt/boot/loader/loader.conf

    umount /mnt/boot
    umount /mnt
    sync

    cp -r /usr/share/acrn/samples/nuc/launch_uos.sh ~/launch_uos_$uos_ver.sh
    sed -i "s/\(virtio-blk.*\)\/home\/clear\/uos\/uos.img/\1$(echo $uos_img | sed "s/\//\\\\\//g")/" ~/launch_uos_$uos_ver.sh
    [[ -z `grep $uos_img ~/launch_uos_$uos_ver.sh` ]] && echo "Failed to replace the User VM image in the launch script: ~/launch_uos_$uos_ver.sh" && exit 1
    echo "Upgrade User VM complete..."
    echo "Run this command to start the User VM..."
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
                i) scenario=industry
                        ;;
                ?) print_help
                        ;;
        esac
done

# Check args
[[ $EUID -ne 0 ]] && echo "You must run the script as root." && exit 1
[[ -z $1 ]] && print_help
[[ -z $efi_partition ]] && efi_partition=/dev/sda1 || echo "Setting the EFI System partition to: $efi_partition..."
[[ -n $sos_ver && -n $uos_ver ]] && echo "Select upgrading the Service VM or the User VM" && exit 1
[[ -n $uos_ver ]] && upgrade_uos || upgrade_sos
