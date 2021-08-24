.. _acrn_on_qemu:

Enable ACRN Over QEMU/KVM
#########################

Goal of this document is to bring-up ACRN as a nested Hypervisor on top of QEMU/KVM
with basic functionality such as running Service VM (SOS) and User VM (UOS) for primarily 2 reasons,

1. Allow users to evaluate ACRN.
2. Make ACRN platform agnostic and remove hardware-specific platform configurations setup overhead.

This setup was tested with the following configuration,

- ACRN Hypervisor: ``v2.6`` tag
- ACRN Kernel: ``v2.6`` tag
- QEMU emulator version 4.2.1
- Service VM/User VM is Ubuntu 20.04
- Platforms Tested: Kaby Lake, Skylake

Prerequisites
*************
1. Make sure the platform supports Intel VMX as well as VT-d
   technologies. On Ubuntu 20.04, this
   can be checked by installing ``cpu-checker`` tool. If the
   output displays **KVM acceleration can be used**
   the platform supports it.

   .. code-block:: none

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
1. Use ``virt-install`` command to create Service VM.

   .. code-block:: none

      virt-install \
      --connect qemu:///system \
      --name ACRNSOS \
      --machine q35 \
      --ram 4096 \
      --disk path=/var/lib/libvirt/images/acrnsos.img,size=32 \
      --vcpus 4 \
      --virt-type kvm \
      --os-type linux \
      --os-variant ubuntu18.04 \
      --graphics none \
      --clock offset=utc,tsc_present=yes,kvmclock_present=no \
      --qemu-commandline="-machine kernel-irqchip=split -cpu Denverton,+invtsc,+lm,+nx,+smep,+smap,+mtrr,+clflushopt,+vmx,+x2apic,+popcnt,-xsave,+sse,+rdrand,+vmx-apicv-xapic,+vmx-apicv-x2apic,+vmx-flexpriority,+tsc-deadline,+pdpe1gb -device intel-iommu,intremap=on,caching-mode=on,aw-bits=48" \
      --location 'http://archive.ubuntu.com/ubuntu/dists/bionic/main/installer-amd64/' \
      --extra-args "console=tty0 console=ttyS0,115200n8"

#. Walk through the installation steps as prompted. Here are a few things to note:

   a. Make sure to install an OpenSSH server so that once the installation is complete, we can SSH into the system.

      .. figure:: images/acrn_qemu_1.png
         :align: center

   b. We use Grub to boot ACRN, so make sure you install it when prompted.

      .. figure:: images/acrn_qemu_2.png
         :align: center

   c. The Service VM (guest) will be restarted once the installation is complete.

#. Login to the Service VM guest. Find the IP address of the guest and use it to connect
   via SSH. The IP address can be retrieved using the ``virsh`` command as shown below.

   .. code-block:: console

      virsh domifaddr ACRNSOS
       Name       MAC address          Protocol     Address
      -------------------------------------------------------------------------------
       vnet0      52:54:00:72:4e:71    ipv4         192.168.122.31/24

#. Once logged into the Service VM, enable the serial console. Once ACRN is enabled,
   the ``virsh`` command will no longer show the IP.

   .. code-block:: none

      sudo systemctl enable serial-getty@ttyS0.service
      sudo systemctl start serial-getty@ttyS0.service

#. Enable the Grub menu to choose between Ubuntu and the ACRN hypervisor.
   Modify :file:`/etc/default/grub` and edit below entries,

   .. code-block:: none

      GRUB_TIMEOUT_STYLE=menu
      GRUB_TIMEOUT=5
      GRUB_CMDLINE_LINUX_DEFAULT=""
      GRUB_GFXMODE=text

#. The Service VM guest can also be launched again later using ``virsh start ACRNSOS --console``.
   Make sure to use the domain name you used while creating the VM in case it is different than ``ACRNSOS``.

This concludes the initial configuration of the Service VM, the next steps will install ACRN in it.

.. _install_acrn_hypervisor:

Install ACRN Hypervisor
***********************

1. Launch the ``ACRNSOS`` Service VM guest and log onto it (SSH is recommended but the console is
   available too).

   .. important:: All the steps below are performed **inside** the Service VM guest that we built in the
      previous section.

#. Install the ACRN build tools and dependencies following the :ref:`gsg`

#. Clone ACRN repo and check out the ``v2.6`` tag.

   .. code-block:: none

      cd ~
      git clone https://github.com/projectacrn/acrn-hypervisor.git
      cd acrn-hypervisor
      git checkout v2.6

#. Build ACRN for QEMU,

   .. code-block:: none

      make BOARD=qemu SCENARIO=sdc

   For more details, refer to :ref:`gsg`.

#. Install the ACRN Device Model and tools

   .. code-block::

      sudo make install

#. Copy ``acrn.32.out`` to the Service VM guest ``/boot`` directory.

   .. code-block:: none

      sudo cp build/hypervisor/acrn.32.out /boot

#. Clone and configure the Service VM kernel repository following the instructions at
   :ref:`gsg` and using the ``v2.6`` tag. The User VM (L2 guest)
   uses the ``virtio-blk`` driver to mount the rootfs. This driver is included in the default
   kernel configuration as of the ``v2.6`` tag.

#. Update Grub to boot the ACRN hypervisor and load the Service VM kernel. Append the following
   configuration to the :file:`/etc/grub.d/40_custom`.

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
         module /boot/bzImage Linux_bzImage
      }

#. Update Grub: ``sudo update-grub``.

#. Enable networking for the User VMs

   .. code-block:: none

      sudo systemctl enable systemd-networkd
      sudo systemctl start systemd-networkd

#. Shut down the guest and relaunch it using ``virsh start ACRNSOS --console``.
   Select the ``ACRN hypervisor`` entry from the Grub menu.

   .. note::
      You may occasionnally run into the following error: ``Assertion failed in file
      arch/x86/vtd.c,line 256 : fatal error`` occasionally. This is a transient issue,
      try to restart the VM when that happens. If you need a more stable setup, you
      can work around the problem by switching your native host to a non-graphical
      environment (``sudo systemctl set-default multi-user.target``).

#. Verify that you are now running ACRN using ``dmesg``.

   .. code-block:: console

      dmesg | grep ACRN
      [    0.000000] Hypervisor detected: ACRN
      [    2.337176] ACRNTrace: Initialized acrn trace module with 4 cpu
      [    2.368358] ACRN HVLog: Initialized hvlog module with 4 cpu
      [    2.727905] systemd[1]: Set hostname to <ACRNSOS>.

   .. note::
      When shutting down the Service VM, make sure to cleanly destroy it with these commands,
      to prevent crashes in subsequent boots.

      .. code-block:: none

         virsh destroy ACRNSOS # where ACRNSOS is the virsh domain name.

Bring-Up User VM (L2 Guest)
***************************

1. Build the ACRN User VM kernel.

   .. code-block:: none

      cd ~/acrn-kernel
      cp kernel_config_uos .config
      make olddefconfig
      make

#. Copy the User VM kernel to your home folder, we will use it to launch the User VM (L2 guest)

   .. code-block:: none

      cp arch/x86/boot/bzImage ~/bzImage_uos

#. Build the User VM disk image (``UOS.img``) following :ref:`build-the-ubuntu-kvm-image` and copy it to the ACRNSOS (L1 Guest).
   Alternatively you can also use ``virt-install`` **in the host environment** to create a User VM image similarly to how we built ACRNSOS previously.

   .. code-block:: none

      virt-install \
      --name UOS \
      --ram 1024 \
      --disk path=/var/lib/libvirt/images/UOS.img,size=8,format=raw \
      --vcpus 2 \
      --virt-type kvm \
      --os-type linux \
      --os-variant ubuntu18.04 \
      --graphics none \
      --location 'http://archive.ubuntu.com/ubuntu/dists/bionic/main/installer-amd64/' \
      --extra-args "console=tty0 console=ttyS0,115200n8"

#. Transfer the ``UOS.img`` User VM disk image to the Service VM (L1 guest).

   .. code-block::

      sudo scp /var/lib/libvirt/images/UOS.img <username>@<IP address>

  Where ``<username>`` is your username in the Service VM and ``<IP address>`` its IP address.

#. Launch User VM using the ``launch_ubuntu.sh`` script.

   .. code-block:: none

      cp ~/acrn-hypervisor/misc/config_tools/data/samples_launch_scripts/launch_ubuntu.sh ~/

#. Update the script to use your disk image and kernel

   .. code-block:: none

      acrn-dm -A -m $mem_size -s 0:0,hostbridge \
      -s 3,virtio-blk,~/UOS.img \
      -s 4,virtio-net,tap0 \
      -s 5,virtio-console,@stdio:stdio_port \
      -k ~/bzImage_uos \
      -B "earlyprintk=serial,ttyS0,115200n8 consoleblank=0 root=/dev/vda1 rw rootwait maxcpus=1 nohpet console=tty0 console=hvc0 console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M tsc=reliable" \
      $logger_setting \
      $vm_name
