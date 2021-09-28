.. _using_partition_mode_on_nuc:

Getting Started Guide for ACRN Logical Partition Mode
#####################################################

The ACRN hypervisor supports a logical partition scenario in which the User
OS, running in a pre-launched VM, can bypass the ACRN
hypervisor and directly access isolated PCI devices. The following
guidelines provide step-by-step instructions on how to set up the ACRN
hypervisor logical partition scenario on Intel NUC while running two
pre-launched VMs.

.. contents::
   :local:
   :depth: 1

Validated Versions
******************

- Ubuntu version: **18.04**
- ACRN hypervisor tag: **v2.6**

Prerequisites
*************

* `Intel NUC Kit NUC11TNBi5 <https://ark.intel.com/content/www/us/en/ark/products/205596/intel-nuc-11-pro-board-nuc11tnbi5.html>`_.
* NVMe disk
* SATA disk
* Storage device with USB interface (such as USB Flash
  or SATA disk connected with a USB 3.0 SATA converter).
* Disable **Intel Hyper Threading Technology** in the BIOS to avoid
  interference from logical cores for the logical partition scenario.
* In the logical partition scenario, two VMs (running Ubuntu OS)
  are started by the ACRN hypervisor. Each VM has its own root
  filesystem. Set up each VM by following the `Ubuntu desktop installation
  <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop>`_ instructions
  first on a SATA disk and then again on a storage device with a USB interface.
  The two pre-launched VMs will mount the root file systems via the SATA controller and
  the USB controller respectively.

.. rst-class:: numbered-step

Update Kernel Image and Modules of Pre-Launched VM
**************************************************
#. On the local Ubuntu target machine, find the kernel file,
   copy to your (``/boot`` directory) and name the file ``bzImage``.
   The ``uname -r`` command returns the kernel release, for example,
   ``4.15.0-55-generic``):

   .. code-block:: none

      $ sudo cp /boot/vmlinuz-$(uname -r)  /boot/bzImage

#. The current ACRN logical partition scenario implementation requires a
   multi-boot capable bootloader to boot both the ACRN hypervisor and the
   bootable kernel image built from the previous step. Install the Ubuntu OS
   on the onboard NVMe SSD by following the `Ubuntu desktop installation
   instructions <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop>`_ The
   Ubuntu installer creates 3 disk partitions on the onboard NVMe SSD. By
   default, the GRUB bootloader is installed on the EFI System Partition
   (ESP) that's used to bootstrap the ACRN hypervisor.


#. After installing the Ubuntu OS, power off the Intel NUC. Attach the
   SATA disk and storage device with the USB interface to the Intel NUC. Power on
   the Intel NUC and make sure it boots the Ubuntu OS from the NVMe SSD. Plug in
   the removable disk with the kernel image into the Intel NUC and then copy the
   loadable kernel modules built in Step 1 to the ``/lib/modules/`` folder
   on both the mounted SATA disk and storage device with USB interface. For
   example, assuming the SATA disk and storage device with USB interface are
   assigned to ``/dev/sda`` and ``/dev/sdb`` respectively, the following
   commands set up the partition mode loadable kernel modules onto the root
   file systems to be loaded by the pre-launched VMs:

   .. code-block:: none

      # Mount the Ubuntu OS root filesystem on the SATA disk
      $ sudo mount /dev/sda3 /mnt
      $ sudo cp -r /lib/modules/* /mnt/lib/modules
      $ sudo umount /mnt
      # Mount the Ubuntu OS root filesystem on the USB flash disk
      $ sudo mount /dev/sdb3 /mnt
      $ sudo cp -r /lib/modules/* /mnt/lib/modules
      $ sudo umount /mnt



Update ACRN Hypervisor Image
****************************

#. Before building the ACRN hypervisor, find the I/O address of the serial
   port and the PCI BDF addresses of the SATA controller and the USB
   controllers on the Intel NUC. Enter the following command to get the
   I/O addresses of the serial port. The Intel NUC supports one serial port, **ttyS0**.
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
      00:14.0 USB controller: Intel Corporation Device 9ded (rev 30) (prog-if 30 [XHCI])
              Subsystem: Intel Corporation Device 7270
      00:17.0 SATA controller: Intel Corporation Device 9dd3 (rev 30) (prog-if 01 [AHCI 1.0])
              Subsystem: Intel Corporation Device 7270
      02:00.0 Non-Volatile memory controller: Intel Corporation Device f1a8 (rev 03) (prog-if 02 [NVM Express])
              Subsystem: Intel Corporation Device 390d
      03:00.0 Ethernet controller: Intel Corporation I210 Gigabit Network Connection (rev 03)
              Subsystem: Intel Corporation I210 Gigabit Network Connection
      04:00.0 Ethernet controller: Intel Corporation I210 Gigabit Network Connection (rev 03)
              Subsystem: Intel Corporation I210 Gigabit Network Connection

#. Clone the ACRN source code and configure the build options.

   Refer to :ref:`gsg` to set up the ACRN build
   environment on your development workstation.

   Clone the ACRN source code and check out to the tag v2.6:

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-hypervisor.git
      $ cd acrn-hypervisor
      $ git checkout v2.6

#. Check the ``pci_devs`` sections in ``misc/config_tools/data/nuc11tnbi5/logical_partition.xml``
   for each pre-launched VM to ensure you are using the right PCI device BDF information (as
   reported by ``lspci -vv``). If you need to make changes to this file, create a copy of it and
   use it subsequently when building ACRN (``SCENARIO=/path/to/newfile.xml``).

#. Build the ACRN hypervisor and ACPI binaries for pre-launched VMs with default xmls:

   .. code-block:: none

      $ make hypervisor BOARD=nuc11tnbi5  SCENARIO=logical_partition RELEASE=0

   .. note::
      The ``acrn.bin`` will be generated to ``./build/hypervisor/acrn.bin``.
      The ``ACPI_VM0.bin`` and ``ACPI_VM1.bin`` will be generated to ``./build/hypervisor/acpi/``.

#. Check the Ubuntu bootloader name.

   In the current design, the logical partition depends on the GRUB boot
   loader; otherwise, the hypervisor will fail to boot. Verify that the
   default bootloader is GRUB:

   .. code-block:: none

      $ sudo update-grub -V

   The above command output should contain the ``GRUB`` keyword.

#. Copy the artifact ``acrn.bin``, ``ACPI_VM0.bin``, and ``ACPI_VM1.bin`` to the ``/boot`` directory on NVME:

   #. Copy ``acrn.bin``, ``ACPI_VM1.bin`` and ``ACPI_VM0.bin`` to a removable disk.

   #. Plug the removable disk into the Intel NUC's USB port.

   #. Copy the ``acrn.bin``, ``ACPI_VM0.bin``, and ``ACPI_VM1.bin`` from the removable disk to ``/boot``
      directory.

.. rst-class:: numbered-step

Update Ubuntu GRUB to Boot Hypervisor and Load Kernel Image
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
              module2 /boot/ACPI_VM0.bin ACPI_VM0
              module2 /boot/ACPI_VM1.bin ACPI_VM1
      }

   .. note::
      Update the UUID (``--set``) and PARTUUID (``root=`` parameter)
      (or use the device node directly) of the root partition (e.g.``/dev/nvme0n1p2). Hint: use ``sudo blkid``.
      The kernel command-line arguments used to boot the pre-launched VMs is ``bootargs``
      in the ``misc/config_tools/data/nuc11tnbi5/logical_partition.xml``
      The ``module2 /boot/bzImage`` param ``XXXXXX`` is the bzImage tag and must exactly match the ``kern_mod``
      in the ``misc/config_tools/data/nuc11tnbi5/logical_partition.xml`` file.
      The module ``/boot/ACPI_VM0.bin`` is the binary of ACPI tables for pre-launched VM0, the parameter ``ACPI_VM0`` is
      VM0's ACPI tag and should not be modified.
      The module ``/boot/ACPI_VM1.bin`` is the binary of ACPI tables for pre-launched VM1 the parameter ``ACPI_VM1`` is
      VM1's ACPI tag and should not be modified.

#. Correct example Grub configuration (with ``module2`` image paths set):

   .. code-block:: console

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
           module2 /boot/bzImage Linux_bzImage
           module2 /boot/ACPI_VM0.bin ACPI_VM0
           module2 /boot/ACPI_VM1.bin ACPI_VM1
      }

#. Modify the ``/etc/default/grub`` file as follows to make the GRUB menu
   visible when booting:

   .. code-block:: none

      GRUB_DEFAULT=ACRN_Logical_Partition
      #GRUB_HIDDEN_TIMEOUT=0
      #GRUB_HIDDEN_TIMEOUT_QUIET=true
      GRUB_TIMEOUT=10
      GRUB_DISTRIBUTOR=`lsb_release -i -s 2> /dev/null || echo Debian`
      GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
      GRUB_CMDLINE_LINUX=""

#. Update GRUB:

   .. code-block:: none

      $ sudo update-grub

#. Reboot the Intel NUC. Select the **ACRN hypervisor Logical Partition
   Scenario** entry to boot the logical partition of the ACRN hypervisor on
   the Intel NUC's display. The GRUB loader will boot the hypervisor, and the
   hypervisor will automatically start the two pre-launched VMs.

.. rst-class:: numbered-step

Logical Partition Scenario Startup Check
****************************************
#. Connect to the serial port as described in this :ref:`Connecting to the
   serial port <connect_serial_port>` tutorial.

#. Use these steps to verify that the hypervisor is properly running:

   #. Log in to the ACRN hypervisor shell from the serial console.
   #. Use the ``vm_list`` to check the pre-launched VMs.

#. Use these steps to verify that the two pre-launched VMs are running
   properly:

   #. Use the ``vm_console 0`` to switch to VM0's console.
   #. The VM0's OS should boot and log in.
   #. Use a :kbd:`Ctrl` + :kbd:`Space` to return to the ACRN hypervisor shell.
   #. Use the ``vm_console 1`` to switch to VM1's console.
   #. The VM1's OS should boot and log in.

Refer to the :ref:`ACRN hypervisor shell user guide <acrnshell>`
for more information about available commands.
