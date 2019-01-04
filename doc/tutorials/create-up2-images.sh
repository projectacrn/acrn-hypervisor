# Copyright (C) 2018 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

#!/bin/bash

usage() {
    echo "Usage: $0 [options]"
    echo "This script builds images for Slim Boot Loader (SBL) based platforms"
    echo "options:"
    echo "--mirror-url default: 'https://cdn.download.clearlinux.org/releases/', for swupd"
    echo "--acrn-code-path: Specify acrn-hypervisor code path for ACRN SBL build. If acrn-sbl-path is provided, acrn-code-path will be ignored"
    echo "--acrn-sbl-path: Specify ACRN SBL binary path. If acrn-sbl-path isn't provided, acrn_code_path will be must option"
    echo "--clearlinux-version: mandatory option for sos image build"
    echo "--images-type: Specify the type of OS image to build (sos/laag/all, default value is all)"
    echo "--sign-key: Specify the debug key for signing, default value provided"
    echo "--sos-rootfs-size: Specify the sos_rootfs image size in MB, default value is 3584"
    echo "--laag-image-size: Specify the laag image size in MB, default value is 10240"
    echo "--sos-bundle-append: Specify additional bundles to be installed in the sos"
    echo "--laag-json: mandatory option, used by ister.py to build the uos"
}

    
create_sos_images() {
    mkdir sos_rootfs
    echo "Clean previously generated images"
    rm -fv sos_boot.img
    rm -fv sos_rootfs.img

    fallocate -l ${SOS_ROOTFS_SIZE}M sos_rootfs.img || return 1
    mkfs.ext4 sos_rootfs.img
    mount sos_rootfs.img sos_rootfs
    echo mount sos_rootfs >> .cleanup

    mountpoint sos_rootfs || return 1

    swupd verify --install --path=sos_rootfs --contenturl=$MIRRORURL --versionurl=$MIRRORURL --format=staging -m ${VERSION} ||
    {
        echo "Failed to swupd install"
        return 1
    }

    swupd bundle-add $SOS_BUNDLE_LIST --path=sos_rootfs --contenturl=$MIRRORURL --versionurl=$MIRRORURL --format=staging ||
    {
        echo "Failed to swupd bundle add"
        return 1
    }

    SOS_BOOTARGS_DEBUG=${ACRN_HV_CODE_PATH}/devicemodel/samples/up2/sos_bootargs_debug.txt

    if [[ ! ${ACRN_SBL} && ! -f ${ACRN_SBL} ]]
    then 
        if [ ${ACRN_HV_CODE_PATH} ]
        then
            make -C ${ACRN_HV_CODE_PATH} clean || return 1
            make -C ${ACRN_HV_CODE_PATH} hypervisor BOARD=up2 FIRMWARE=sbl || return 1
            ACRN_SBL=${ACRN_HV_CODE_PATH}/build/hypervisor/acrn.32.out
        else
            echo "Need to provide acrn.sbl or acrn-hypervisor source code path"
            return 1
        fi
    fi

    if [ ! -f ${ACRN_SBL} ]
    then
        echo "ACRN SBL is not found."
        return 1
    fi

    if [ -f ${SOS_BOOTARGS_DEBUG} ] 
    then
        echo -n "CMDLINE: "
        echo $(tr '\n' ' ' < $SOS_BOOTARGS_DEBUG) | tee tmp/cmdline
    else                
        echo "sos_bootargs_debug.txt is not found"
        return 1        
    fi

    SOS_KERNEL=$(ls sos_rootfs/usr/lib/kernel/org.clearlinux.iot-lts2018-sos*)
    touch tmp/hv_cmdline

    iasimage create -o iasImage -i 0x40300 -d tmp/bxt_dbg_priv_key.pem -p 4 tmp/hv_cmdline ${ACRN_SBL}  tmp/cmdline ${SOS_KERNEL} || 
    {
        echo "stitch iasimage for sos_boot failed!"
        return 1
    }

    if [ -f iasImage ]; then
        mv iasImage sos_boot.img
    fi

    return
}


create_uos_images() {
    echo "Start to create the up2_laag.img..."
    rm -fv up2_laag.img
    fallocate -l ${LAAG_IMAGE_SIZE}M up2_laag.img || return 1
    mkfs.ext4 up2_laag.img
    mkdir laag_image
    mount -v up2_laag.img laag_image
    echo mount laag_image >> .cleanup    

    mkdir -p laag_image/clearlinux
    ister.py -t $LAAG_JSON -V $MIRRORURL -C $MIRRORURL || 
    {
        echo "ister create clearlinux.img failed"
        return 1
    }

    mv clearlinux.img laag_image/clearlinux
    devloop=`losetup --partscan --find --show laag_image/clearlinux/clearlinux.img`
    echo loopdev $devloop >> .cleanup

    mkdir laag_rootfs
    mount "$devloop"p2 laag_rootfs
    echo mount laag_rootfs >> .cleanup

    mount "$devloop"p1 laag_rootfs/boot
    echo mount laag_rootfs/boot >> .cleanup

    kernel_version=`readlink laag_rootfs/usr/lib/kernel/default-iot-lts2018 | awk -F '2018.' '{print $2}'`
    cmdline=`ls laag_rootfs/usr/lib/kernel | grep cmdline-$kernel_version`

    iasimage create -o laag_rootfs/boot/iasImage -i 0x30300 -d tmp/bxt_dbg_priv_key.pem laag_rootfs/usr/lib/kernel/$cmdline laag_rootfs/usr/lib/kernel/default-iot-lts2018

}

cleanup() {
    # Process .cleanup file in reverse order
    [ -e .cleanup ] && tac .cleanup | while read key val; do
        case $key in
            loopdev)
                losetup --detach $val
                ;;
            mount)
                umount -R -v $val && rmdir $val
                ;;
            mkdir)
                rm -rfv $val
        esac
    done
    rm -fv .cleanup
}

# Default values
SOS_BASE_BUNDLE_LIST="service-os os-core-update openssh-server software-defined-cockpit"
SOS_BUNDLE_APPEND=""
LAAG_BUNDLE_APPEND=""
SOS_ROOTFS_SIZE=3584
LAAG_IMAGE_SIZE=10240
LAAG_VDISK_SIZE=5120
MIRRORURL="https://cdn.download.clearlinux.org/releases/"
SIGN_KEY="https://download.clearlinux.org/secureboot/DefaultIASSigningPrivateKey.pem"
IMAGE=all

while [ $# -gt 0 ]; do
    case $1 in
        --mirror-url)
            MIRRORURL=$2
            shift 2
            ;;
        --acrn-code-path)
            ACRN_HV_CODE_PATH=$2
            shift 2
            ;;
        --acrn-sbl-path)
            ACRN_SBL=$2
            shift 2
            ;;      
        --clearlinux-version)
            VERSION=$2
            echo ${VERSION}
            shift 2
            ;;
        --images-type)
            IMAGE=$2
            shift 2
            ;;
        --sign-key)
            SIGN_KEY=$2
            shift 2
            ;;
        --sos-rootfs-size)
            SOS_ROOTFS_SIZE=$2
            shift 2
            ;;
        --laag-image-size)
            LAAG_IMAGE_SIZE=$2
            shift 2
            ;;
        --sos-bundle-append)
            SOS_BUNDLE_APPEND=$2
            shift 2
            ;;
        --laag-json)
            LAAG_JSON=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit -1
            ;;
        *)
            echo Invalid argument: $1
            usage
            exit -1
            ;;
    esac
done

SOS_BUNDLE_LIST=${SOS_BASE_BUNDLE_LIST}" "${SOS_BUNDLE_APPEND}


# check valid images type
if [[ ${IMAGE} != "sos"  && ${IMAGE} != "laag" && ${IMAGE} != "all" ]]; then
    echo "--images-type: must be one of sos, laag, all, and default is all"
    exit 1
fi

# check valid LaaG image and vdisk sizes
if [[ ${IMAGE} == "sos" || ${IMAGE} == "all" ]]; then
    if [[ ! ${VERSION} ]]; then
        echo "--clearlinux-version: must be provided for SOS images building."
        exit 1
    fi
    if [[ ! ${ACRN_SBL} && ! ${ACRN_HV_CODE_PATH} ]]; then
    echo "Should provide --acrn-sbl-path or --acrn-code-path for SOS images building"
    exit 1
    fi
fi

# check valid LaaG image and vdisk sizes
if [[ ${IMAGE} == "laag" || ${IMAGE} == "all" ]] && [[ ! ${LAAG_JSON} ]]; then
    echo "--laag-uos is mandatory option for laag image build"
    exit 1
fi

# check superuser privileges
if [[ $EUID -ne 0 ]]; then
    echo "Need to be run as root"
    exit 1
fi

trap cleanup EXIT

# mkdir tmp for tempoaray files
mkdir tmp
echo mkdir tmp >> .cleanup

#download debug key for iasimage signing
curl -o tmp/bxt_dbg_priv_key.pem -k ${SIGN_KEY} ||
{
    echo "Failed to retrieve debug key"
    exit 1
}

# Add iasimage bundle
swupd bundle-add iasimage --contenturl=$MIRRORURL --versionurl=$MIRRORURL ||
{
    echo "Failed to swupd add iasimage"
    exit 1
}

if [[ ${IMAGE} == 'sos' || ${IMAGE} == 'all' ]]
then
    if create_sos_images
    then
        echo "Successful create sos images"
    else
        echo "Failed to create sos images"
        exit 1
    fi
fi

if [[ ${IMAGE} == 'laag' || ${IMAGE} == 'all' ]]
then
    if create_uos_images
    then
        echo "Successful create uos images"
    else
        echo "Failed to create uos images"
        exit 1
    fi
fi

exit 0

