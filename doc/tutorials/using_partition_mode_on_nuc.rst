.. _using_partition_mode_on_nuc:

Getting Started Guide for ACRN logical partition mode
#####################################################

The ACRN hypervisor supports a logical partition scenario in which the User
OS (such as Ubuntu OS) running in a pre-launched VM can bypass the ACRN
hypervisor and directly access isolated PCI devices. The following
guidelines provide step-by-step instructions on how to set up the ACRN
hypervisor logical partition scenario on Intel NUC while running two
pre-launched VMs.

Validated Versions
******************

- Ubuntu version: **18.04**
- ACRN hypervisor tag: **v2.1**
- ACRN kernel tag: **v2.1**

Prerequisites
*************

* `Intel Whiskey Lake <http://www.maxtangpc.com/industrialmotherboards/142.html#parameters>`_
* NVMe disk
* SATA disk
* Storage device with USB interface (such as USB Flash
  or SATA disk connected with a USB3.0 SATA converter).
* Disable **Intel Hyper Threading Technology** in the BIOS to avoid
  interference from logical cores for the logical partition scenario.
* In the logical partition scenario, two VMs (running Ubuntu OS)
  are started by the ACRN hypervisor. Each VM has its own root
  filesystem. Set up each VM by following the `Ubuntu desktop installation
  <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop>`_ instructions
  first on a SATA disk and then again on a storage device with a USB interface.
  The two pre-launched VMs will mount the root file systems via the SATA controller and
  the USB controller respectively.

Update kernel image and modules of pre-launched VM
**************************************************
#. On your development workstation, clone the ACRN kernel source tree, and
   build the Linux kernel image that will be used to boot the pre-launched VMs:

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel.git
      Cloning into 'acrn-kernel'...
      ...
      $ cd acrn-kernel
      $ cp kernel_config_uos .config
      $ make olddefconfig
      scripts/kconfig/conf  --olddefconfig Kconfig
      #
      # configuration written to .config
      #
      $ make
      $ make modules_install INSTALL_MOD_PATH=out/

   The last two commands build the bootable kernel image as
   ``arch/x86/boot/bzImage``, and loadable kernel modules under the ``./out/``
   folder. Copy these files to a removable disk for installing on the NUC later.

#. The current ACRN logical partition scenario implementation requires a
   multi-boot capable bootloader to boot both the ACRN hypervisor and the
   bootable kernel image built from the previous step. Install the Ubuntu OS
   on the on-board NVMe SSD by following the `Ubuntu desktop installation
   instructions <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop>`_ The
   Ubuntu installer creates 3 disk partitions on the on-board NVMe SSD. By
   default, the GRUB bootloader is installed on the EFI System Partition
   (ESP) that's used to bootstrap the ACRN hypervisor.

#. After installing the Ubuntu OS, power off the NUC. Attach the
   SATA disk and storage device with the USB interface to the NUC. Power on
   the NUC and make sure it boots the Ubuntu OS from the NVMe SSD. Plug in
   the removable disk with the kernel image into the NUC and then copy the
   loadable kernel modules built in Step 1 to the ``/lib/modules/`` folder
   on both the mounted SATA disk and storage device with USB interface. For
   example, assuming the SATA disk and storage device with USB interface are
   assigned to ``/dev/sda`` and ``/dev/sdb`` respectively, the following
   commands set up the partition mode loadable kernel modules onto the root
   file systems to be loaded by the pre-launched VMs:

   .. code-block:: none

      # Mount the Ubuntu OS root filesystem on the SATA disk
      $ sudo mount /dev/sda3 /mnt
      $ sudo cp -r <kernel-modules-folder-built-in-step1>/lib/modules/* /mnt/lib/modules
      $ sudo umount /mnt
      # Mount the Ubuntu OS root filesystem on the USB flash disk
      $ sudo mount /dev/sdb3 /mnt
      $ sudo cp -r <path-to-kernel-module-folder-built-in-step1>/lib/modules/* /mnt/lib/modules
      $ sudo umount /mnt

#. Copy the bootable kernel image to the /boot directory:

   .. code-block:: none

      $ sudo cp <path-to-kernel-image-built-in-step1>/bzImage /boot/

Update ACRN hypervisor image
****************************

#. Before building the ACRN hypervisor, find the I/O address of the serial
   port and the PCI BDF addresses of the SATA controller nd the USB
   controllers on the NUC. Enter the following command to get the
   I/O addresses of the serial port. The NUC supports one serial port, **ttyS0**.
   Connect the serial port to the development workstation in order to access
   the ACRN serial console to switch between pre-launched VMs:

   .. code-block:: none

      $ dmesg | grep ttyS0
      [    0.000000] console [ttyS0] enabled
      [    1.562546] 00:01: ttyS0 at I/O 0x3f8 (irq = 4, base_baud = 115200) is
      a 16550A

   The following command prints detailed information about all PCI buses and
   devices in the system:

   .. code-block:: none

      $ sudo lspci -vv
      00:14.0 USB controller: Intel Corporation Sunrise Point-LP USB 3.0 xHCI Controller (rev 21) (prog-if 30 [XHCI])
              Subsystem: Intel Corporation Sunrise Point-LP USB 3.0 xHCI Controller
      00:17.0 SATA controller: Intel Corporation Sunrise Point-LP SATA Controller [AHCI mode] (rev 21) (prog-if 01 [AHCI 1.0])
              Subsystem: Intel Corporation Sunrise Point-LP SATA Controller [AHCI mode]
      00:1f.6 Ethernet controller: Intel Corporation Ethernet Connection I219-LM (rev 21)
              Subsystem: Intel Corporation Ethernet Connection I219-LM

   .. note::
      Verify the PCI devices BDF defined in the
      ``hypervisor/arch/x86/configs/whl-ipc-i5/pci_devices.h``
      with the information reported by the ``lspci -vv`` command.

#. Clone the ACRN source code and configure the build options.

   Refer to :ref:`getting-started-building` to set up the ACRN build
   environment on your development workstation.

   Clone the ACRN source code and check out to the tag v2.1:

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-hypervisor.git
      $ cd acrn-hypervisor
      $ git checkout v2.1

   Build the ACRN hypervisor with default xmls:

   .. code-block:: none

      $ make hypervisor BOARD_FILE=$PWD/misc/acrn-config/xmls/board-xmls/whl-ipc-i5.xml SCENARIO_FILE=$PWD/misc/acrn-config/xmls/config-xmls/whl-ipc-i5/logical_partition.xml RELEASE=0

   .. note::
      The ``acrn.bin`` will be generated to ``./build/hypervisor/acrn.bin``.

#. Check the Ubuntu boot loader name.

   In the current design, the logical partition depends on the GRUB boot
   loader; otherwise, the hypervisor will fail to boot. Verify that the
   default boot loader is GRUB:

   .. code-block:: none

      $ sudo update-grub -V

   The above command output should contain the ``GRUB`` keyword.

#. Check or update the BDF information of the PCI devices for each
   pre-launched VM; check it in the ``hypervisor/arch/x86/configs/whl-ipc-i5/pci_devices.h``.

#. Copy the artifact ``acrn.bin`` to the ``/boot`` directory:

   #. Copy ``acrn.bin`` to a removable disk.

   #. Plug the removable disk into the NUC's USB port.

   #. Copy the ``acrn.bin`` from the removable disk to ``/boot``
      directory.

Update Ubuntu GRUB to boot hypervisor and load kernel image
***********************************************************

#. Append the following configuration to the ``/etc/grub.d/40_custom`` file:

   .. code-block:: none

      menuentry 'ACRN hypervisor Logical Partition Scenario' --id ACRN_Logical_Partition --class ubuntu --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-e23c76ae-b06d-4a6e-ad42-46b8eedfd7d3' {
              recordfail
              load_video
              gfxmode $linux_gfx_mode
              insmod gzio
              insmod part_gpt
              insmod ext2

              search --no-floppy --fs-uuid --set 9bd58889-add7-410c-bdb7-1fbc2af9b0e1
              echo 'Loading hypervisor logical partition scenario ...'
              multiboot2  /boot/acrn.bin root=PARTUUID="e515916d-aac4-4439-aaa0-33231a9f4d83"
              module2 /boot/bzImage XXXXXX
      }

   .. note::
      Update this to use the UUID (``--set``) and PARTUUID (``root=`` parameter)
      (or use the device node directly) of the root partition (e.g.``/dev/nvme0n1p2). Hint: use ``sudo blkid``.
      The kernel command line arguments used to boot the pre-launched VMs is
      located in the ``misc/vm_configs/scenarios/hybrid/vm_configurations.h`` header file
      and is configured by ``VMx_CONFIG_OS_BOOTARG_*`` MACROs (where x is the VM id number and ``*`` are arguments).
      The multiboot2 module param ``XXXXXX`` is the bzImage tag and must exactly match the ``kernel_mod_tag``
      configured in the ``misc/vm_configs/scenarios/hybrid/vm_configurations.c`` file.

#. Modify the ``/etc/default/grub`` file as follows to make the GRUB menu
   visible when booting:

   .. code-block:: none

      GRUB_DEFAULT=ACRN_Logical_Partition
      GRUB_TIMEOUT=10
      GRUB_DISTRIBUTOR=`lsb_release -i -s 2> /dev/null || echo Debian`
      GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
      GRUB_CMDLINE_LINUX=""

#. Update GRUB:

   .. code-block:: none

      $ sudo update-grub

#. Reboot the NUC. Select the **ACRN hypervisor Logical Partition
   Scenario** entry to boot the logical partition of the ACRN hypervisor on
   the NUC's display. The GRUB loader will boot the hypervisor, and the
   hypervisor will automatically start the two pre-launched VMs.

Logical partition scenario startup checking
*******************************************

#. Use these steps to verify that the hypervisor is properly running:

   #. Log in to the ACRN hypervisor shell from the serial console.
   #. Use the ``vm_list`` to check the pre-launched VMs.

#. Use these steps to verify that the two pre-launched VMs are running
   properly:

   #. Use the ``vm_console 0`` to switch to VM0's console.
   #. The VM0's Clear Linux OS should boot up and log in.
   #. Use a :kbd:`Ctrl` + :kbd:`Space` to return to the ACRN hypervisor shell.
   #. Use the ``vm_console 1`` to switch to VM1's console.
   #. The VM1's Clear Linux OS should boot up and log in.

Refer to the :ref:`ACRN hypervisor shell user guide <acrnshell>`
for more information about available commands.
