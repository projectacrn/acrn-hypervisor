#!/bin/bash
# Copyright (C) 2021 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause
#
# PREREQUISITES:
# 1) Get your specific "IntelGopDriver.efi" and "Vbt.bin"
#    from your BIOS vender
# 2) Install Docker on your host machine and allow non-root users
#    For Ubuntu: https://docs.docker.com/engine/install/ubuntu/
#    To enable non-root users: https://docs.docker.com/engine/install/linux-postinstall/#manage-docker-as-a-non-root-user
# 3) If you are working behind proxy, create a file named
#    "proxy.conf" in ${your_working_directory} with
#    configurations like below:
#    Acquire::http::Proxy "http://x.y.z:port1";
#    Acquire::https::Proxy "https://x.y.z:port2";
#    Acquire::ftp::Proxy "ftp://x.y.z:port3";
#
# HOWTO:
# 1) mkdir ${your_working_directory}
# 2) cd ${your_working_directory}
# 2) mkdir gop
# 3) cp /path/to/IntelGopDriver.efi /path/to/Vbt.bin gop
# 4) cp /path/to/build_acrn_ovmf.sh ${your_working_directory}
# 5) ./build_acrn_ovmf.sh
#
# OUTPUT: ${your_working_directory}/acrn-edk2/Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd
#
# For more information, ./build_acrn_ovmf.sh -h
#

gop_bin_dir="./gop"
docker_image_name="ubuntu:ovmf.16.04"
proxy_conf="proxy.conf"
acrn_ver="latest"

if [ ! -x "$(command -v docker)" ]; then
    echo "Install Docker first:"
    echo "If you are using Ubuntu, you can refer to: https://docs.docker.com/engine/install/ubuntu/"
    exit
fi

if [ ! -d "${gop_bin_dir}" ]; then
    mkdir ${gop_bin_dir}
    echo "Copy IntelGopDriver.efi and Vbt.bin to ${gop_bin_dir}"
    exit
fi

if [ ! -f "${gop_bin_dir}/IntelGopDriver.efi" ]; then
    echo "Copy IntelGopDriver.efi to ${gop_bin_dir}"
    exit
fi

if [ ! -f "${gop_bin_dir}/Vbt.bin" ]; then
    echo "Copy Vbt.bin to ${gop_bin_dir}"
    exit
fi

if [ ! -f "${proxy_conf}" ]; then
    touch "${proxy_conf}"
fi

usage()
{
    echo "$0 [-v ver] [-i] [-s] [-h]"
    echo "  -v ver: The release version of ACRN, e.g. 2.3"
    echo "  -i:     Delete the existing docker image ${docker_image_name} and re-create it"
    echo "  -s:     Delete the existing acrn-edk2 source code and re-download/re-patch it"
    echo "  -h:     Show this help"
    exit
}

re_download=0
re_create_image=0

while getopts "hisv:" opt
do
    case "${opt}" in
        h)
            usage
            ;;
        i)
            re_create_image=1
            ;;
        s)
            re_download=1
            ;;
        v)
            acrn_ver=${OPTARG}
            ;;
        ?)
            echo "${OPTARG}"
            ;;
    esac
done
shift $((OPTIND-1))

if [[ "${re_create_image}" -eq 1 ]]; then
    if [[ "$(docker images -q ${docker_image_name} 2> /dev/null)" != "" ]]; then
        echo "===================================================================="
        echo "Deleting the old Docker image ${docker_image_name}  ..."
        echo "===================================================================="
        docker image rm -f "${docker_image_name}"
    fi
fi

if [[ "${re_download}" -eq 1 ]]; then
    echo "===================================================================="
    echo "Deleting the old acrn-edk2 source code ..."
    echo "===================================================================="
    sudo rm -rf acrn-edk2
fi

create_acrn_edk2_workspace()
{
    echo "===================================================================="
    echo "Downloading & patching acrn_edk2 source code ..."
    echo "===================================================================="

    [ -d acrn-edk2 ] && sudo rm -rf acrn-edk2

    git clone https://github.com/projectacrn/acrn-edk2.git
    if [ $? -ne 0 ]; then
        echo "git clone acrn-edk2 failed"
        return 1
    fi

    cd acrn-edk2
    git submodule update --init CryptoPkg/Library/OpensslLib/openssl
    if [ $? -ne 0 ]; then
        echo "git submodule acrn-edk2 failed"
        return 1
    fi

    if [ "${acrn_ver}" != "latest" ]; then
        git checkout --recurse-submodules -b "v${acrn_ver}" "ovmf-acrn-v${acrn_ver}"
        if [ $? -ne 0 ]; then
            echo "git checkout --recurse-submodules -b v${acrn_ver} ovmf-acrn-v${acrn_ver} failed"
            return 1
        fi
    fi

    wget -q https://projectacrn.github.io/${acrn_ver}/_static/downloads/Use-the-default-vbt-released-with-GOP-driver.patch
    if [ $? -ne 0 ]; then
        echo "Downloading Use-the-default-vbt-released-with-GOP-driver.patch failed"
        return 1
    fi

    wget -q https://projectacrn.github.io/${acrn_ver}/_static/downloads/Integrate-IntelGopDriver-into-OVMF.patch
    if [ $? -ne 0 ]; then
        echo "Downloading Integrate-IntelGopDriver-into-OVMF.patch failed"
        return 1
    fi

    git am --keep-cr Use-the-default-vbt-released-with-GOP-driver.patch
    if [ $? -ne 0 ]; then
        echo "Apply Use-the-default-vbt-released-with-GOP-driver.patch failed"
        return 1
    fi

    git am --keep-cr Integrate-IntelGopDriver-into-OVMF.patch
    if [ $? -ne 0 ]; then
        echo "Apply Integrate-IntelGopDriver-into-OVMF.patch failed"
        return 1
    fi

    return 0
}

create_docker_image()
{
    echo "===================================================================="
    echo "Creating Docker image ..."
    echo "===================================================================="

    cat > Dockerfile.ovmf <<EOF
FROM ubuntu:16.04

WORKDIR /root/acrn

COPY ${proxy_conf} /etc/apt/apt.conf.d/proxy.conf
RUN apt-get update && apt-get install -y vim build-essential uuid-dev iasl git gcc-5 nasm python-dev
EOF

    docker build -t "${docker_image_name}" -f Dockerfile.ovmf .
    rm Dockerfile.ovmf
}

if [[ "$(docker images -q ${docker_image_name} 2> /dev/null)" == "" ]]; then
    create_docker_image
fi

if [ ! -d acrn-edk2 ]; then
    create_acrn_edk2_workspace
    if [ $? -ne 0 ]; then
        echo "Download/patch acrn-edk2 failed"
        exit
    fi
else
    cd acrn-edk2
fi

cp -f ../${gop_bin_dir}/IntelGopDriver.efi OvmfPkg/IntelGop/IntelGopDriver.efi
cp -f ../${gop_bin_dir}/Vbt.bin OvmfPkg/Vbt/Vbt.bin

source edksetup.sh

sed -i 's:^ACTIVE_PLATFORM\s*=\s*\w*/\w*\.dsc*:ACTIVE_PLATFORM       = OvmfPkg/OvmfPkgX64.dsc:g' Conf/target.txt
sed -i 's:^TARGET_ARCH\s*=\s*\w*:TARGET_ARCH           = X64:g' Conf/target.txt
sed -i 's:^TOOL_CHAIN_TAG\s*=\s*\w*:TOOL_CHAIN_TAG        = GCC5:g' Conf/target.txt

cd ..

docker run \
    -ti \
    --rm \
    -w $PWD/acrn-edk2 \
    --privileged=true \
    -v $PWD:$PWD \
    ${docker_image_name} \
    /bin/bash -c "source edksetup.sh && make -C BaseTools && build -DFD_SIZE_2MB -DDEBUG_ON_SERIAL_PORT=TRUE"
