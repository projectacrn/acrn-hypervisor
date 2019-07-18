#!/bin/bash
# Copyright (C) 2019 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

help() {
	echo "==================================================================================================="
	echo "Usage:"
	echo "$SIGN_SCRIPT param1 param2 param3"
	echo "    param1: path to clear linux image"
	echo "    param2: path to the key"
	echo "    param3: path to the cert"
	echo ""
	echo "Pre-requisites:"
	echo "    1. install sbsigntool: https://git.kernel.org/pub/scm/linux/kernel/git/jejb/sbsigntools.git/"
	echo "    2. download clear linux release for VM and extract the image: https://cdn.download.clearlinux.org/releases/"
	echo "    3. run this script with sudo"
	echo "==================================================================================================="
}

sign_binaries_under_dir() {
	local DIR=$1
	for file in $DIR/*
	do
		if test -f $file
		then
			echo $file
			(sbsign --key $SIGN_KEY --cert $SIGN_CRT --output $file $file) && (echo "sign $file succeed")
		else
			sign_binaries_under_dir $file
		fi
	done
}


SIGN_SCRIPT=$0
CLEAR_UOS_IMAGE=$1
SIGN_KEY=$2
SIGN_CRT=$3
BOOT_PART="p1"
MNT_POINT=/mnt

if [[ ! -f $1 || ! -f $2 || ! -f $3 ]]
then
	help
	exit
fi

if [ "$(id -u)" != "0" ]
then
	echo "This script requires root privilege. Please run it with sudo or switch to root user."
	exit
fi

CLEAR_UOS_IMAGE_SIGNED=$CLEAR_UOS_IMAGE.signed

cp $CLEAR_UOS_IMAGE $CLEAR_UOS_IMAGE_SIGNED

LOOP_DEV=`losetup -f -P --show $CLEAR_UOS_IMAGE_SIGNED`

if [ ! -d $MNT_POINT ]
then
	mkdir $MNT_POINT
fi

(mount $LOOP_DEV$BOOT_PART $MNT_POINT) && (sign_binaries_under_dir $MNT_POINT/EFI)

umount /mnt
sync
losetup -d $LOOP_DEV
