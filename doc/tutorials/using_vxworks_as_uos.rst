.. _using_vxworks_as_uos:

Using VxWorks* as User OS
#########################

`VxWorks`_\* is a real-time proprietary OS designed for use in embedded systems requiring real-time, deterministic
performance. This tutorial describes how to run VxWorks as the User OS on the ACRN hypervisor
based on Clear Linux 29970 (ACRN tag v1.1).

.. note:: You'll need to be a WindRiver* customer and have purchased VxWorks to follow this tutorial.

Steps for Using VxWorks as User OS
**********************************

#. Build VxWorks

   Follow the `VxWorks Getting Started Guide <https://docs.windriver.com/bundle/vxworks_7_tutorial_kernel_application_workbench_sr0610/page/rbu1422461642318.html>`_
   to setup the VxWorks development environment and build the VxWorks Image.

   .. note::
      The following kernel configuration should be **excluded**:
        * INCLUDE_PC_CONSOLE
        * DRV_SIO_PCI_NS16550
        * SELECT_PC_CONSOLE_KBD

      The following kernel configuration should be **included**:
        * DRV_SIO_IA_NS16550
        * DRV_SIO_FDT_NS16550

      The following kernel configuration value should be **set**:
        * CONSOLE_BAUD_RATE = 115200
        * SYS_CLK_RATE_MAX = 1000

#. Build GRUB2 BootLoader Image

   We use grub-2.02 as the bootloader of VxWorks in this tutorial; other versions may also work.

   .. code-block:: none

      $ curl ftp://ftp.gnu.org/gnu/grub/grub-2.02.tar.xz | tar xJv
      $ cd grub-2.02
      $ ./autogen.sh
      $ ./configure --with-platform=efi --target=x86_64
      $ make
      $ ./grub-mkimage -p /EFI/BOOT -d ./grub-core/ -O x86_64-efi -o grub_x86_64.efi \
            boot efifwsetup efi_gop efinet efi_uga lsefimmap lsefi lsefisystab \
            exfat fat multiboot2 multiboot terminal part_msdos part_gpt normal \
            all_video aout configfile echo file fixvideo fshelp gfxterm gfxmenu \
            gfxterm_background gfxterm_menu legacycfg video_bochs video_cirrus \
            video_colors video_fb videoinfo video net tftp

   This will build a ``grub_x86_64.efi`` image in the current directory.

#. Preparing the Boot Device

   .. code-block:: none

      $ dd if=/dev/zero of=VxWorks.img bs=1M count=35
      $ mkfs.vfat -F 32 VxWorks.img
      $ sudo mount `sudo losetup -f -P --show VxWorks.img` /mnt

   Create the following directories.

   .. code-block:: none

      $ sudo mkdir -p /mnt/efi/boot
      $ sudo mkdir -p /mnt/kernel

   Copy ``vxWorks`` and ``grub_x86_64.efi``.

   .. code-block:: none

      $ sudo cp vxWorks /mnt/kernel/
      $ sudo cp grub-2.02/grub_x86_64.efi /mnt/efi/boot/bootx64.efi

   Create ``/mnt/efi/boot/grub.cfg`` containing the following:

   .. code-block:: none

      set default=0
      set timeout=5

      menuentry "VxWorks Guest" {
          multiboot /kernel/vxWorks
      }

   Unmount the loopback device:

   .. code-block:: none

      $ sudo umount /mnt

   You now have a virtual disk image with bootable VxWorks in ``VxWorks.img``.

#. Follow :ref:`getting-started-apl-nuc` to boot "The ACRN Service OS".

#. Boot VxWorks as User OS.

   On the ACRN SOS, prepare a directory and populate it with VxWorks files.

   .. code-block:: none

      $ mkdir vxworks && cd vxworks
      $ cp /usr/share/acrn/samples/nuc/launch_vxworks.sh .
      $ cp /usr/share/acrn/bios/OVMF.fd .

   You will also need to copy the ``VxWorks.img`` created in the VxWorks build environment into directory
   ``vxworks`` (via, e.g. a USB stick or network).

   Run the ``launch_vxworks.sh`` script to launch VxWorks as Uos.

   .. code-block:: none

      $ sudo ./launch_vxworks.sh

   Then VxWorks will boot up automatically. You will see the prompt.

   .. code-block:: console

                     VxWorks 7 SMP 64-bit

      Copyright 1984-2019 Wind River Systems, Inc.

            Core Kernel version: 1.2.7.0
                     Build date: May  5 2019 21:40:28
                          Board: x86 Processor (ACPI_BOOT_OP) SMP/SMT
                      CPU Count: 1
                 OS Memory Size: ~1982MB
               ED&R Policy Mode: Permanently Deployed

      Adding 9315 symbols for standalone.

      ->

   Finally, you can type ``help`` to check whether the VxWorks works well.

   .. code-block:: console

      -> help

      help                           Print this list
      dbgHelp                        Print debugger help info
      edrHelp                        Print ED&R help info
      ioHelp                         Print I/O utilities help info
      nfsHelp                        Print nfs help info
      netHelp                        Print network help info
      rtpHelp                        Print process help info
      spyHelp                        Print task histogrammer help info
      timexHelp                      Print execution timer help info
      h         [n]                  Print (or set) shell history
      i         [task]               Summary of tasks' TCBs
      ti        task                 Complete info on TCB for task
      sp        adr,args...          Spawn a task, pri=100, opt=0x19, stk=20000
      taskSpawn name,pri,opt,stk,adr,args... Spawn a task
      tip       "dev=device1#tag=tagStr1", "dev=device2#tag=tagStr2", ...
                                     Connect to one or multiple serial lines
      td        task                 Delete a task
      ts        task                 Suspend a task
      tr        task                 Resume a task

      Type <CR> to continue, Q<CR> or q<CR> to stop:

.. _VxWorks: https://www.windriver.com/products/vxworks/
