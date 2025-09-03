.. _acrn_on_qemu:

Enable ACRN Over QEMU/KVM
#########################

This document shows how to bring up ACRN as a nested hypervisor on top of
QEMU/KVM with basic functionality such as running a Service VM and User VM.
Running ACRN as a nested hypervisor gives you an easy way to evaluate ACRN in an
emulated environment instead of setting up a separate hardware platform
configuration.

This setup was tested with the following configuration:

- ACRN hypervisor: ``v3.0`` tag
- ACRN kernel: ``acrn-v3.0`` tag
- QEMU emulator version: 4.2.1
- Host OS: Ubuntu 20.04
- Service VM/User VM OS: Ubuntu 20.04
- Platforms tested: Kaby Lake, Skylake, Whiskey Lake, Tiger Lake

Prerequisites
*************

1. Make sure the platform supports Intel VMX as well as VT-d
   technologies. On Ubuntu 20.04, this
   can be checked by installing the ``kvm-ok`` tool found in the ``cpu-checker`` package.


   .. code-block:: bash

      sudo apt install cpu-checker

   Run the ``kvm-ok`` tool and if the output displays **KVM acceleration can be used**,
   the platform supports Intel VMX and VT-d technologies.

   .. code-block:: console

      kvm-ok
      INFO: /dev/kvm exists
      KVM acceleration can be used

2. The host kernel version must be **at least 5.3.0** or above.
   Ubuntu 20.04 uses a 5.8.0 kernel (or later),
   so no changes are needed if you are using it.

3. Make sure KVM and the following utilities are installed.

   .. code-block:: none

      sudo apt update && sudo apt upgrade -y
      sudo apt install qemu-kvm virtinst libvirt-daemon-system -y


Prepare Service VM (L1 Guest)
*****************************

1. Use the ``virt-install`` command to create the Service VM.

   .. code-block:: none

      virt-install \
      --connect qemu:///system \
      --name ServiceVM \
      --machine q35 \
      --ram 4096 \
      --disk path=/var/lib/libvirt/images/servicevm.img,size=32 \
      --vcpus 4 \
      --virt-type kvm \
      --os-type linux \
      --os-variant ubuntu20.04 \
      --graphics none \
      --clock offset=utc,tsc_present=yes,kvmclock_present=no \
      --qemu-commandline="-machine kernel-irqchip=split -cpu Denverton,+invtsc,+lm,+nx,+smep,+smap,+mtrr,+clflushopt,+vmx,+x2apic,+popcnt,-xsave,+sse,+rdrand,-vmx-apicv-vid,+vmx-apicv-xapic,+vmx-apicv-x2apic,+vmx-flexpriority,+tsc-deadline,+pdpe1gb -device intel-iommu,intremap=on,caching-mode=on,aw-bits=48" \
      --location 'http://archive.ubuntu.com/ubuntu/dists/focal/main/installer-amd64/' \
      --extra-args "console=tty0 console=ttyS0,115200n8"

#. Walk through the installation steps as prompted. Here are a few things to note:

   a. Make sure to install an OpenSSH server so that once the installation is
      complete, you can SSH into the system.

      .. figure:: images/acrn_qemu_1.png
         :align: center

   b. We use GRUB to boot ACRN, so make sure you install it when prompted.

      .. figure:: images/acrn_qemu_2.png
         :align: center

   c. After the installation is complete, the Service VM (guest) will restart.

#. Log in to the Service VM guest. Find the IP address of the guest and use it
   to connect via SSH. The IP address can be retrieved using the ``virsh``
   command as shown below.

   .. code-block:: console

      virsh domifaddr ServiceVM
       Name       MAC address          Protocol     Address
      -------------------------------------------------------------------------------
       vnet0      52:54:00:72:4e:71    ipv4         192.168.122.31/24

#. Once logged into the Service VM, enable the serial console. Once ACRN is enabled,
   the ``virsh`` command will no longer show the IP.

   .. code-block:: none

      sudo systemctl enable serial-getty@ttyS0.service
      sudo systemctl start serial-getty@ttyS0.service

#. Enable the GRUB menu to choose between Ubuntu and the ACRN hypervisor.
   Modify :file:`/etc/default/grub` and edit these entries:

   .. code-block:: none

      GRUB_TIMEOUT_STYLE=menu
      GRUB_TIMEOUT=5
      GRUB_CMDLINE_LINUX_DEFAULT=""
      GRUB_GFXMODE=text

#. Check the rootfs partition  with ``lsblk``, it is ``vda5`` in this example.

#. The Service VM guest can also be launched again later using
   ``virsh start ServiceVM --console``. Make sure to use the domain name you
   used while creating the VM in case it is different than ``ServiceVM``.

This concludes the initial configuration of the Service VM. The next steps will
install ACRN in it.

.. _install_acrn_hypervisor:

Install ACRN Hypervisor
***********************

1. Launch the ``ServiceVM`` Service VM guest and log into it (SSH is recommended
   but the console is available too).

   .. important:: All the steps below are performed **inside** the Service VM
      guest that we built in the previous section.

#. Install the ACRN build tools and dependencies following the :ref:`gsg`. Note
   again that we're doing these steps within the Service VM and not on a development
   system as described in the Getting Started Guide.
#. Switch to the ACRN hypervisor ``v3.0`` tag.

   .. code-block:: none

      cd ~
      git clone https://github.com/projectacrn/acrn-hypervisor.git
      cd acrn-hypervisor
      git checkout v3.0

#. Build ACRN for QEMU:

   We're using the qemu board XML and shared scenario XML files
   supplied from the repo (``misc/config_tools/data/qemu``) and not
   generated by the board inspector or configurator tools.

   .. code-block:: none

      make BOARD=qemu SCENARIO=shared

   For more details, refer to the :ref:`gsg`.

#. Install the ACRN Device Model and tools:

   .. code-block:: none

      sudo make install

#. Copy ``acrn.32.out`` to the Service VM guest ``/boot`` directory.

   .. code-block:: none

      sudo cp build/hypervisor/acrn.32.out /boot

#. Clone and configure the Service VM kernel repository following the
   instructions in the :ref:`gsg` and using the ``acrn-v3.0`` tag. The User VM (L2
   guest) uses the ``virtio-blk`` driver to mount the rootfs. This driver is
   included in the default kernel configuration as of the ``acrn-v3.0`` tag.

#. Update GRUB to boot the ACRN hypervisor and load the Service VM kernel.
   Append the following configuration to the :file:`/etc/grub.d/40_custom`.

   .. code-block:: none

      menuentry 'ACRN hypervisor' --class ubuntu --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-e23c76ae-b06d-4a6e-ad42-46b8eedfd7d3' {
         recordfail
         load_video
         gfxmode $linux_gfx_mode
         insmod gzio
         insmod part_msdos
         insmod ext2

         echo 'Loading ACRN hypervisor ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out  root=/dev/vda5
         module /boot/vmlinuz-5.10.115-acrn-service-vm Linux_bzImage
      }

   .. note::
      If your rootfs partition isn't vda5, please change it to match with yours.
      vmlinuz-5.10.115-acrn-service-vm is the kernel image of Service VM.

#. Update GRUB:

   .. code-block:: none

      sudo update-grub

#. Enable networking for the User VMs:

   .. code-block:: none

      sudo systemctl enable systemd-networkd
      sudo systemctl start systemd-networkd

#. Shut down the guest and relaunch it using
   ``virsh start ServiceVM --console``.
   Select the ``ACRN hypervisor`` entry from the GRUB menu.

   .. note::
      You may occasionally run into the following error: ``Assertion failed in
      file arch/x86/vtd.c,line 256 : fatal error``. This is a transient issue;
      try to restart the VM when that happens. If you need a more stable setup,
      you can work around the problem by switching your native host to a
      non-graphical environment (``sudo systemctl set-default
      multi-user.target``).

#. Use ``dmesg`` to verify that you are now running ACRN.

   .. code-block:: console

      dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    2.337176] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    2.368358] ACRN HVLog: Initialized hvlog module with 4 cpu
      [    2.727905] systemd[1]: Set hostname to <ServiceVM>.

   .. note::
      When shutting down the Service VM, make sure to cleanly destroy it with
      these commands, to prevent crashes in subsequent boots.

      .. code-block:: none

         virsh destroy ServiceVM # where ServiceVM is the virsh domain name.

Bring Up User VM (L2 Guest)
***************************

1. Build the User VM disk image (``UserVM.img``) following
   :ref:`build-the-ubuntu-kvm-image` and copy it to the Service VM (L1 guest).
   Alternatively you can use an
   `Ubuntu Desktop ISO image <https://ubuntu.com/#download>`_.
   Rename the downloaded ISO image to ``UserVM.iso``.

#. Transfer the ``UserVM.img``  or ``UserVM.iso`` User VM disk image to the
   Service VM (L1 guest).

#. Copy OVMF.fd to launch User VM.

   .. code-block:: none

      cp ~/acrn-hypervisor/devicemodel/bios/OVMF.fd ~/

#. Update the script to use your disk image (``UserVM.img`` or ``UserVM.iso``).

   .. code-block:: none

      #!/bin/bash
      # Copyright (C) 2020-2022 Intel Corporation.
      # SPDX-License-Identifier: BSD-3-Clause
      function launch_ubuntu()
      {
      vm_name=ubuntu_vm$1
      logger_setting="--logger_setting console,level=5;kmsg,level=6;disk,level=5"
      #check if the vm is running or not
      vm_ps=$(pgrep -a -f acrn-dm)
      result=$(echo $vm_ps | grep "${vm_name}")
      if [[ "$result" != "" ]]; then
        echo "$vm_name is running, can't create twice!"
        exit
      fi
      #for memsize setting
      mem_size=1024M
      acrn-dm -m $mem_size -s 0:0,hostbridge \
      -s 3,virtio-blk,~/UserVM.img \
      -s 4,virtio-net,tap=tap0 \
      --cpu_affinity 1 \
      -s 5,virtio-console,@stdio:stdio_port \
      --ovmf ~/OVMF.fd \
      $logger_setting \
      $vm_name
      }
      # offline Service VM CPUs except BSP before launching User VM
      for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do
        online=`cat $i/online`
        idx=`echo $i | tr -cd "[1-99]"`
        echo cpu$idx online=$online
        if [ "$online" = "1" ]; then
           echo 0 > $i/online
                # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod
                while [ "$online" = "1" ]; do
                sleep 1
                echo 0 > $i/online
                online=`cat $i/online`
                done
                echo $idx > /sys/devices/virtual/misc/acrn_hsm/remove_cpu
        fi
      done
      launch_ubuntu 1

Experimental: Running ACRN on QEMU RISCV-64
*******************************************

(WIP, Unstable)

The following documentation is tested under Ubuntu 24.04.

To compile and run ACRN on QEMU RISCV-64, we need to first install
QEMU RV64 and crossbuild toolchains. The default packages provided by
Ubuntu 24.04 will suffice:

.. code-block:: bash

    sudo apt install crossbuild-essential-riscv64 qemu-system-misc u-boot-qemu opensbi

To run ACRN on QEMU RISCV-64, first compile ACRN towards QEMU:

.. code-block:: bash

    make hypervisor BOARD=qemu SCENARIO=shared ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-

The ACRN ELF binary is generated under ``build/hypervisor/acrn.out``.

Then run QEMU with the following parameters:

.. code-block:: bash

    qemu-system-riscv64 -nographic -machine virt -cpu rv64 -m 8G -smp 4 \
        -kernel /path/to/acrn-root/build/hypervisor/acrn.out

ACRN currently only prints out "Hello World!".
