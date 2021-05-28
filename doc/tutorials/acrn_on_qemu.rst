.. _acrn_on_qemu:

Enable ACRN Over QEMU/KVM
#########################

Goal of this document is to bring-up ACRN as a nested Hypervisor on top of QEMU/KVM
with basic functionality such as running Service VM (SOS) and User VM (UOS) for primarily 2 reasons,

1. Allow users to evaluate ACRN.
2. Make ACRN platform agnostic and remove hardware-specific platform configurations setup overhead.

This setup was tested with the following configuration,

- ACRN Hypervisor: tag ``v2.0``
- ACRN Kernel: release_2.0 (5.4.43-PKT-200203T060100Z)
- QEMU emulator version 4.2.0
- Service VM/User VM is ubuntu 18.04
- Platforms Tested: Apollo Lake, Kaby Lake, Coffee Lake

.. note::

   ACRN versions newer than v2.0 do not work on QEMU.


Prerequisites
*************
1. Make sure the platform supports Intel VMX as well as VT-d
   technologies. On Ubuntu 18.04, this
   can be checked by installing ``cpu-checker`` tool. If the output displays **KVM acceleration can be used**
   the platform supports it.

   .. code-block:: none

      $ kvm-ok
      INFO: /dev/kvm exists
      KVM acceleration can be used

2. Ensure the Ubuntu18.04 Host kernel version is **at least 5.3.0** and above.

3. Make sure KVM and the following utilities are installed.

   .. code-block:: none

      $ sudo apt update && sudo apt upgrade -y
      $ sudo apt install qemu-kvm libvirt-bin virtinst -y


Prepare Service VM (L1 Guest)
*****************************
1. Use ``virt-install`` command to create Service VM.

   .. code-block:: none

      $ virt-install \
      --connect qemu:///system \
      --name ACRNSOS \
      --machine q35 \
      --cpu host-passthrough,+invtsc \
      --ram 4096 \
      --disk path=/var/lib/libvirt/images/acrnsos.img,size=32 \
      --vcpus 4 \
      --virt-type kvm \
      --os-type linux \
      --os-variant ubuntu18.04 \
      --graphics none \
      --clock offset=utc,tsc_present=yes,kvmclock_present=no \
      --qemu-commandline="-machine kernel-irqchip=split -device intel-iommu,intremap=on,caching-mode=on,aw-bits=48" \
      --location 'http://archive.ubuntu.com/ubuntu/dists/bionic/main/installer-amd64/' \
      --extra-args "console=tty0 console=ttyS0,115200n8"

2. Walk through the installation steps as prompted. Here are a few things to note:

   a. Make sure to install an OpenSSH server so that once the installation is complete, we can SSH into the system.

      .. figure:: images/acrn_qemu_1.png
         :align: center

   b. We use GRUB to boot ACRN, so make sure you install it when prompted.

      .. figure:: images/acrn_qemu_2.png
         :align: center

3. To login to the Service VM guest, find the IP address of the guest to SSH. This can be done via the
   virsh command as shown below,

   .. figure:: images/acrn_qemu_3.png
      :align: center

4. Once ACRN hypervisor is enabled, the above virsh command might not display the IP. So enable Serial console by,

   .. code-block:: none

      $ sudo systemctl enable serial-getty@ttyS0.service
      $ sudo systemctl start serial-getty@ttyS0.service

   .. note::
      You might want to write down the Service VM IP address in case you want to SSH to it.

5. Enable GRUB menu to choose between Ubuntu vs ACRN hypervisor. Modify :file:`/etc/default/grub` and edit below entries,

   .. code-block:: none

      GRUB_TIMEOUT_STYLE=menu
      GRUB_TIMEOUT=5
      GRUB_CMDLINE_LINUX_DEFAULT=""
      GRUB_GFXMODE=text

6. Update GRUB changes by ``sudo update-grub``

7. Once the above steps are done, Service VM guest can also be launched using, ``virsh start ACRNSOS --console``. Make sure to use the domain name
   you used while creating the VM instead of ``ACRNSOS``.

This concludes setting up of Service VM and preparing it to boot ACRN hypervisor.

.. _install_acrn_hypervisor:

Install ACRN Hypervisor
***********************

1. Clone the ACRN repo and check out the ``v2.0`` tag.

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-hypervisor.git
      $ cd acrn-hypervisor/
      $ git checkout v2.0

2. Use the following command to build ACRN for QEMU,

   .. code-block:: none

      $ make all BOARD_FILE=./misc/acrn-config/xmls/board-xmls/qemu.xml  SCENARIO_FILE=./misc/acrn-config/xmls/config-xmls/qemu/sdc.xml

 For more details, refer to :ref:`getting-started-building`.

3. Copy ``acrn.32.out`` from ``build/hypervisor`` to Service VM guest ``/boot/`` directory.

4. Clone and build the Service VM kernel that includes the virtio-blk driver. User VM (L2 guest) uses virtio-blk
   driver to mount rootfs.

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel
      $ cp kernel_config_uefi_sos .config
      $ make olddefconfig
      $ make menuconfig
      $ make

   The below figure shows the drivers to be enabled using ``make menuconfig`` command.

      .. figure:: images/acrn_qemu_4.png
         :align: center

   Once the Service VM kernel is built successfully, copy ``arch/x86/boot/bzImage`` to the Service VM /boot/ directory and rename it to ``bzImage_sos``.

   .. note::
      The Service VM kernel contains all needed drivers so you won't need to install extra kernel modules.

5. Update Ubuntu GRUB to boot ACRN hypervisor and load ACRN Kernel Image. Append the following
   configuration to the :file:`/etc/grub.d/40_custom`,

   .. code-block:: none

      menuentry 'ACRN hypervisor' --class ubuntu --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-e23c76ae-b06d-4a6e-ad42-46b8eedfd7d3' {
         recordfail
         load_video
         gfxmode $linux_gfx_mode
         insmod gzio
         insmod part_msdos
         insmod ext2

         echo 'Loading ACRN hypervisor with SDC scenario ...'
         multiboot --quirk-modules-after-kernel /boot/acrn.32.out
         module /boot/bzImage_sos Linux_bzImage
      }

6. Update GRUB ``sudo update-grub``.

7. Shut down the guest and relaunch using, ``virsh start ACRNSOS --console``
   and select ACRN hypervisor from GRUB menu to launch Service
   VM running on top of ACRN.
   This can be verified using ``dmesg``, as shown below,

   .. code-block:: console

      guestl1@ACRNSOS:~$ dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    2.337176] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    2.368358] ACRN HVLog: Initialized hvlog module with 4 cpu
      [    2.727905] systemd[1]: Set hostname to <ACRNSOS>.

8. When shutting down, make sure to cleanly destroy the Service VM to prevent crashes in subsequent boots. This can be done using,

   .. code-block:: none

      $ virsh destroy ACRNSOS # where ACRNSOS is the virsh domain name.


Service VM Networking Updates for User VM
*****************************************
Follow these steps to enable networking for the User VM (L2 guest):

1. Edit your :file:`/etc/netplan/01-netcfg.yaml` file to add acrn-br0 as below,

   .. code-block:: none

      network:
         version: 2
         renderer: networkd
         ethernets:
            enp1s0:
               dhcp4: no
         bridges:
            acrn-br0:
               interfaces: [enp1s0]
               dhcp4: true
               dhcp6: no

2. Apply the new network configuration by,

   .. code-block:: none

      $ cd /etc/netplan
      $ sudo netplan generate
      $ sudo netplan apply

3. Create a tap interface (tap0) and add the tap interface as part of the acrn-br0 using the below steps,

   a. Copy files ``misc/acrnbridge/acrn.network`` and ``misc/acrnbridge/tap0.netdev`` from the cloned ACRN repo to :file:`/usr/lib/system/network`.
   b. Rename ``acrn.network`` to ``50-acrn.network``.
   c. Rename ``tap0.netdev`` to ``50-tap0.netdev``.

4. Restart ACRNSOS guest (L1 guest) to complete the setup and start with bring-up of User VM


Bring-Up User VM (L2 Guest)
***************************
1. Build the device-model, using ``make devicemodel`` and copy acrn-dm to ACRNSOS guest (L1 guest) directory ``/usr/bin/acrn-dm``

   .. note::
      It should be already built as part of :ref:`install_acrn_hypervisor`.

2. On the ACRNSOS guest, install shared libraries for acrn-dm (if not already installed).

   .. code-block:: none

      $ sudo apt-get install libpciaccess-dev

3. Install latest `IASL tool <https://acpica.org/downloads>`_ and copy the binary to ``/usr/sbin/iasl``.
   For this setup, used IASL 20200326 version but anything after 20190215 should be good.

4. Clone latest stable version or main branch and build ACRN User VM Kernel.

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel
      $ cp kernel_config_uos .config
      $ make

   Once the User VM kernel is built successfully, copy ``arch/x86/boot/bzImage`` to  ACRNSOS (L1 guest) and rename this to ``bzImage_uos``. Need this to launch the User VM (L2 guest)

   .. note::
      The User VM kernel contains all needed drivers so you won't need to install extra kernel modules.

5. Build ubuntu.img using :ref:`build-the-ubuntu-kvm-image` and copy it to the ACRNSOS (L1 Guest).
   Alternatively you can also use virt-install to create a User VM image similar to ACRNSOS as shown below,

   .. code-block:: none

      $ virt-install \
      --name UOS \
      --ram 2048 \
      --disk path=/var/lib/libvirt/images/UOSUbuntu.img,size=8 \
      --vcpus 2 \
      --virt-type kvm \
      --os-type linux \
      --os-variant ubuntu18.04 \
      --graphics none \
      --location 'http://archive.ubuntu.com/ubuntu/dists/bionic/main/installer-amd64/' \
      --extra-args "console=tty0 console=ttyS0,115200n8"

   .. note::
      Image at ``/var/lib/libvirt/images/UOSUbuntu.img`` is a qcow2 image. Convert it to raw image using, ``qemu-img convert -f qcow2 UOSUbuntu.img -O raw UOS.img``

6. Launch User VM using launch script from the cloned repo path ``devicemodel/samples/launch_ubuntu.sh``. Make sure to update with your ubuntu image and rootfs

   .. code-block:: none

      acrn-dm -A -m $mem_size -s 0:0,hostbridge \
      -s 3,virtio-blk,/home/guestl1/acrn-dm-bins/UOS.img \
      -s 4,virtio-net,tap0 \
      -s 5,virtio-console,@stdio:stdio_port \
      -k /home/guestl1/acrn-dm-bins/bzImage_uos \
      -B "earlyprintk=serial,ttyS0,115200n8 consoleblank=0 root=/dev/vda1 rw rootwait maxcpus=1 nohpet console=tty0 console=hvc0 console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M tsc=reliable" \
      $logger_setting \
      $vm_name
