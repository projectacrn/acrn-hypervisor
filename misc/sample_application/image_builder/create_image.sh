#!/bin/bash
# Copyright (C) 2020-2022 Intel Corporation.
# SPDX-License-Identifier: BSD-3-Clause

build_dir="$PWD/build"
cloud_image="${build_dir}/jammy-server-cloudimg-amd64.img"
cloud_image_url=https://cloud-images.ubuntu.com/jammy/current/jammy-server-cloudimg-amd64.img
hmi_vm_image="${build_dir}/hmi_vm.img"
rt_vm_image="${build_dir}/rt_vm.img"
rt_kernel=(linux-libc linux-headers linux-image)

vm_type=$1
if [[ ${vm_type} != "rt-vm" ]] && [[ ${vm_type} != "hmi-vm" ]]; then
    cat <<EOT
Usage: $0 <vm_type>
This script creates VM images based on Ubuntu cloud images.

VM type options:
  hmi-vm        create a VM with GNOME desktop
  rt-vm         create a VM with a preempt-RT-patched kernel and real-time test utilities
EOT
    exit 1
fi

########################################
# Environment checks
########################################

if [[ ! -d /etc/schroot/chroot.d ]]; then
    echo "Package schroot is not installed."
    exit 1
fi

if [[ ! -d $(dirname $PWD)/build ]]; then
    make -C $(dirname $PWD)
fi

arr=("$PWD/mnt" "$PWD/build")
for dir in "${arr[@]}"; do
    if [[ ! -d "$dir" ]]; then
        mkdir $dir
    fi
done

########################################
# Helper functions
########################################

source logger.sh

########################################
# Actions defined as functions
########################################

function copy_rt_kernel() {
    for file in ~/acrn-work/*rtvm*.deb
    do
        if [[ ${file} != *"dbg"* ]]; then
            cp ${file} ${build_dir}
       fi
    done
}

function check_rt_kernel() {
    for file in ${rt_kernel[@]}
    do
        ls ${build_dir}/*.deb | grep ${file}
        if [ $? -eq 1 ]; then
            echo "RT VM kernel package ${file} is not found."
            exit
        fi
    done
}

function download_image() {
    local dest=$1
    local url=$2

    if [[ -f ${dest} ]]; then
        print_info "${dest} already exists. Do not redownload."
    else
        wget -O ${dest} ${url}
    fi
}

function copy_and_enlarge_image() {
    local source_image=$1
    local dest_image=$2
    local size_modifier=$3

    if [[ -f ${dest_image} ]]; then
        echo -n "${dest_image} already exists! Regenerate the image? (y/N)> "
        read answer
        if [[ $answer =~ ^[Yy]$ ]]; then
            rm ${dest_image}
        else
            exit 1
        fi
    fi

    qemu-img convert -f qcow2 -O raw ${source_image} ${dest_image} && \
        qemu-img resize -f raw ${dest_image} ${size_modifier} && \
        LANG=C growpart ${dest_image} 1
}

function dump_proxy() {
	local temp_file=$(mktemp /tmp/proxy.XXXX)

	sudo apt-config dump | grep -i proxy > ${temp_file} 2>&1
	sudo mv ${temp_file} proxy.conf

	echo "$(env | grep -Ei _proxy | sed -e 's/^/export /')" > bashrc
}

function resizing_guest_root() {
    local part_file=$1

    sudo e2fsck -f ${part_file} && \
        sudo resize2fs ${part_file}
}

function mount_filesystem() {
    local part_file=$1
    local mount_point=$2

    # The symlink /etc/resolv.conf in a fresh cloud image is broken, which will
    # prevent schroot from working. Touch that linked file to work it around.
    mkdir -p ${mount_point} && \
        sudo mount ${part_file} ${mount_point} && \
        sudo mkdir -p ${mount_point}/run/systemd/resolve/ && \
        sudo touch ${mount_point}/run/systemd/resolve/stub-resolv.conf
}

function create_schroot_config() {
    local mount_point=$1
    local temp_file=$(mktemp /tmp/acrn-guest.XXXX)

    cat << EOF > ${temp_file}
[acrn-guest]
description=Contains ACRN guest root file system.
type=directory
directory=${mount_point}
users=root
root-groups=root
profile=desktop
personality=linux
preserve-environment=true
EOF

    sudo sed -ie "/passwd/d;/shadow/d;/group/d;/gshadow/d" \
        /etc/schroot/desktop/nssdatabases && \
    sudo mv ${temp_file} /etc/schroot/chroot.d/acrn-guest && \
        sudo chown root:root /etc/schroot/chroot.d/acrn-guest
}

function create_uio_config() {
    local mount_point=$1
    local temp_file=$(mktemp /tmp/rc.local.XXXX)

    cat << EOF > ${temp_file}
#!/bin/bash
modprobe uio
modprobe uio_pci_generic

for i in {0..2}
do
bash -c 'echo "1af4 1110" > /sys/bus/pci/drivers/uio_pci_generic/new_id'
if [ $? -eq 0 ]; then
    echo "uio setting result" $?
    break
fi
echo "uio setting result" $? "try again"
sleep 1
done
EOF

    sudo mv ${temp_file} $mount_point/etc/rc.local && \
        sudo chown root:root $mount_point/etc/rc.local && \
	sudo chmod 755 $mount_point/etc/rc.local
}

function setup_hmi_vm_rootfs() {
    local mount_point=$1

    sudo cp setup_hmi_vm.sh logger.sh ${mount_point}/ && \
	sudo cp $(dirname $PWD)/build/userApp $(dirname $PWD)/build/histapp.py ${mount_point}/root && \
	sudo cp proxy.conf ${mount_point}/etc/apt/apt.conf.d/proxy.conf && \
	sudo cp bashrc ${mount_point}/root/.bashrc && \
        sudo schroot -c acrn-guest bash /setup_hmi_vm.sh && \
        sudo rm ${mount_point}/setup_hmi_vm.sh ${mount_point}/logger.sh && \
	sudo rm bashrc proxy.conf
}

function setup_rt_vm_rootfs() {
    local mount_point=$1

    sudo mv ${build_dir}/*rtvm*.deb ${mount_point}/root && \
	sudo cp $(dirname $PWD)/build/rtApp ${mount_point}/root && \
        sudo mkdir ${mount_point}/root/scripts && \
	sudo cp proxy.conf ${mount_point}/etc/apt/apt.conf.d/proxy.conf && \
	sudo cp bashrc ${mount_point}/root/.bashrc && \
        sudo cp configRTcores.sh ${mount_point}/root/scripts/ && \
        sudo cp setup_rt_vm.sh logger.sh ${mount_point}/ && \
        sudo schroot -c acrn-guest bash /setup_rt_vm.sh && \
        sudo rm ${mount_point}/setup_rt_vm.sh ${mount_point}/logger.sh && \
        sudo rm bashrc proxy.conf
}

function cleanup() {
    local mount_point=$1
    local loop_dev=$2

    sudo umount ${mount_point}
    sudo rmdir ${mount_point}
    sudo kpartx -vd /dev/${loop_dev}
    sudo losetup -vd /dev/${loop_dev}
    if [[ ${has_error} != 0 ]]; then
        sudo rm ${target_image}
    fi
    true
}

########################################
# Do it!
########################################

mount_point=$(pwd)/mnt
if [[ ${vm_type} == "hmi-vm" ]]; then
    target_image=${hmi_vm_image}
    size_modifier="+5G"
elif [[ ${vm_type} == "rt-vm" ]]; then
    target_image=${rt_vm_image}
    size_modifier="+1G"
else
    echo "Internal error: undefined VM type '${vm_type}'"
    exit 1
fi

try_step "Download Ubuntu cloud image" download_image ${cloud_image} ${cloud_image_url}
if [[ ${vm_type} == "rt-vm" ]]; then
    try_step "Copy the RT kernel to build directory" copy_rt_kernel
    try_step "Check availability of RT kernel image" check_rt_kernel
fi
try_step "Creating an enlarged copy of ${cloud_image}" copy_and_enlarge_image ${cloud_image} ${target_image} ${size_modifier}

loop_dev=$(sudo kpartx -va ${target_image} 2>&1 | egrep -o -m 1 "loop[0-9]+")
print_info "Guest image loop-mounted at /dev/${loop_dev}"

try_step "Resizing guest root file system" resizing_guest_root /dev/mapper/${loop_dev}p1
try_step "Mounting guest root file system at ${mount_point}" mount_filesystem /dev/mapper/${loop_dev}p1 ${mount_point}
try_step "Preparing schroot configuration" create_schroot_config ${mount_point}
try_step "Preparing uio configuration" create_uio_config ${mount_point}
try_step "Extracting network proxy configurations" dump_proxy

if [[ ${vm_type} == "hmi-vm" ]]; then
    try_step "Initializing guest root file system for HMI VM" setup_hmi_vm_rootfs ${mount_point}
else
    try_step "Initializing guest root file system for RT VM" setup_rt_vm_rootfs ${mount_point}
fi

do_step  "Cleaning up" cleanup ${mount_point} ${loop_dev}
print_info "VM image created at ${target_image}."

