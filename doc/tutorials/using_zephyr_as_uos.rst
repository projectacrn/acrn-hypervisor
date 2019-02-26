.. _using_zephyr_as_uos:

Using Zephyr as User OS
#######################

This tutorial describes how to run Zephyr as the User OS on the ACRN hypervisor. We are using
Kaby Lake-based NUC (model NUC7i5DNHE) in this tutorial.
Other :ref:`ACRN supported platforms <hardware>` should work as well.

Introduction to Zephyr
**********************

The Zephyr RTOS is a scalable real-time operating-system supporting multiple hardware architectures,
optimized for resource constrained devices, and built with safety and security in mind.

Steps for Using Zephyr as User OS
*********************************

#. Build Zephyr

   Follow the `Zephyr Getting Started Guide <https://docs.zephyrproject.org/latest/getting_started/>`_ to
   setup the Zephyr development environment.

   The build process for ACRN UOS target is similar to other boards. We will build the `Hello World
   <https://docs.zephyrproject.org/latest/samples/hello_world/README.html>`_ sample for ACRN:

   .. code-block:: none

      $ cd samples/hello_world
      $ mkdir build && cd build
      $ cmake -DBOARD=acrn ..
      $ make

   This will build the application ELF binary in ``samples/hello_world/build/zephyr/zephyr.elf``.

#. Build grub2 boot loader image

   We can build grub2 bootloader for Zephyr using ``boards/x86/common/scripts/build_grub.sh``
   which locate in `Zephyr Sourcecode <https://github.com/zephyrproject-rtos/zephyr>`_.

   .. code-block:: none

      $ ./boards/x86/common/scripts/build_grub.sh x86_64

   The EFI executable binary will be found at ``boards/x86/common/scripts/grub/bin/grub_x86_64.efi``.

#. Preparing the boot device

   .. code-block:: none

      $ dd if=/dev/zero of=zephyr.img bs=1M count=35
      $ mkfs.vfat -F 32 zephyr.img
      $ sudo mount `sudo losetup -f -P --show zephyr.img` /mnt

   Create the following directories.

   .. code-block:: none

      $ sudo mkdir -p /mnt/efi/boot
      $ sudo mkdir -p /mnt/kernel

   Copy ``zephyr.elf`` and ``grub_x86_64.efi``

   .. code-block:: none

      $ sudo cp boards/x86/common/scripts/grub/bin/grub_x86_64.efi /mnt/efi/boot/bootx64.efi
      $ sudo cp samples/hello_world/build/zephyr/zephyr.elf /mnt/kernel

   Create ``/mnt/efi/boot/grub.cfg`` containing the following:

   .. code-block:: console

      set default=0
      set timeout=10

      menuentry "Zephyr Kernel" {
          multiboot /kernel/zephyr.elf
      }

   Unmount the loopback device:

   .. code-block:: none

      $ sudo umount /mnt

   You now have a virtual disk image with a bootable Zephyr in ``zephyr.img``. If the Zephyr build system is not
   the ACRN SOS, then you will need to transfer this image to the ACRN SOS (via, e.g, a USB stick or network )

#. Follow :ref:`getting-started-apl-nuc` to boot "The ACRN Service OS"

#. Boot Zephyr as User OS

   On the ACRN SOS, prepare a directory and populate it with Zephyr files.

   .. code-block:: none

      $ mkdir zephyr && cd zephyr
      $ cp /usr/share/acrn/samples/nuc/launch_zephyr.sh .
      $ cp /usr/share/acrn/bios/OVMF.fd .

   You will also need to copy the ``zephyr.img`` created in the above section into directory ``zephyr``.

   Run the ``launch_zephyr.sh`` script to launch Zephyr as UOS.

   .. code-block:: none

      $ sudo ./launch_zephyr.sh

   Then Zephyr will boot up automatically. You will see a console message from the hello_world sample application:

   .. code-block:: console

      Hello World! acrn
