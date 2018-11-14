.. _partition_mode:

Using partition mode on UP2
###########################
This tutorial describes the steps needed to use partition mode with the ACRN
hypervisor on UP2.

Build kernel and modules for partition mode guest
*************************************************

* Build kernel and modules

  .. code-block:: none

     git clone https://github.com/projectacrn/acrn-kernel.git
     cd ~/acrn-kernel
     cp kernel_config_uos .config
     make oldconfig
     make

* Install modules

  First of all, please prepare one USB flash drive and one SATA disk. Plug them
  in one PC and follow Clear Linux's `online instruction <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install#bare-metal-install>` to install Clear Linux on them.

  Plug the USB flash drive and SATA disk into your PC. Assuming Clear Linux
  rootfs is installed into /dev/sdx3.

  .. code-block:: none

     mount /dev/sdx3 /mnt/sdx3
     cd ~/acrn-kernel
     make INSTALL_MOD_PATH=/mnt/sdx3/ modules_install

* Install kernel

  First, please following Ubuntu's `online instruction <https://tutorials.ubuntu.com/tutorial/tutorial-install-ubuntu-desktop?_ga=2.114179015.1954550575.1530817291-1278304647.1523530035>` to install it on your UP2's eMMC.

  Copy kernel image to EFI System Partition

  .. code-block:: none

     scp username@IP:~/acrn-kernel/arch/x86/boot/bzImage /boot/efi/

Enabling partition mode
***********************

* Clone ACRN source code

  .. code-block:: none

     git clone https://github.com/projectacrn/acrn-hypervisor.git

* Configure the ACRN hypervisor to use partition mode

  .. code-block:: none

     cd ~/acrn-hypervisor/hypervisor
     make menuconfig

  Select entry ``Hypervisor mode (Sharing mode)`` and press ``Enter``.
  Then select ``Partition mode`` sub-entry.

* Configure the ACRN hypervisor's ``serial base address``

  .. code-block:: none

     cd ~/acrn-hypervisor/hypervisor
     make menuconfig

  Select entry ``Base address of serial MMIO region`` and press ``Enter``.
  Then we will see a prompt msgbox with title ``Value for `Base address of serial MMIO region` (hex)``.
  Please type the serial base mmio address of your UP2 board and press ``Enter``.

* Configure the PCI info about devices assigned to vm.

  For the current code, we can only configure it by modifying the ``vpci_vdev_arrary``
  structure (one per vm) in source code file ``hypervisor/partition/vm_description.c``.

  Here is an reference patch for configuring vm1's assgined PCI devices based on
  UP2 board. This patch can also apply to other vms you try to configure.

  .. code-block:: none

     diff --git a/hypervisor/partition/vm_description.c b/hypervisor/partition/vm_description.c
     index 12818185..88799b00 100644
     --- a/hypervisor/partition/vm_description.c
     +++ b/hypervisor/partition/vm_description.c
     @@ -41,6 +41,7 @@ static struct vpci_vdev_array vpci_vdev_array1 = {
                     }
              },

              {/*vdev 1: SATA controller*/
               .vbdf.bits = {.b = 0x00U, .d = 0x01U, .f = 0x0U},
               .ops = &pci_ops_vdev_pt,
     @@ -65,23 +66,24 @@ static struct vpci_vdev_array vpci_vdev_array1 = {
                     .bdf.bits = {.b = 0x00U, .d = 0x12U, .f = 0x0U},
                     .bar = {
                             [0] = {
     -                       .base = 0xb3f10000UL,
     +                       .base = 0x91514000UL,
                             .size = 0x2000UL,
                             .type = PCIBAR_MEM32
                             },
                             [1] = {
     -                       .base = 0xb3f53000UL,
     +                       .base = 0x91537000UL,
                             .size = 0x100UL,
                             .type = PCIBAR_MEM32
                             },
                             [5] = {
     -                       .base = 0xb3f52000UL,
     +                       .base = 0x91536000UL,
                             .size = 0x800UL,
                             .type = PCIBAR_MEM32
                             },
                      }
                    }
                 },
              }
       };

* Configure ``.bootargs``

  A default command line is pre-configured in the source code. Here we just
  configure ``root=`` option to help kernel locate the right rootfs and keep the
  other options as default.

  .. code-block:: none

     @@ -171,7 +180,8 @@ struct vm_description_array vm_desc_partition = {
                                     .start_hpa = 0x100000000UL,
                                     .mem_size = 0x20000000UL, /* uses contiguous memory from host */
                                     .vm_vuart = true,
     -                               .bootargs = "root=/dev/sda rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
     +                               .bootargs = "root=PARTUUID=<Your rootfs PARTUUID> rw rootwait noxsave maxcpus=2 nohpet console=hvc0 \
                                                   console=ttyS0 no_timer_check ignore_loglevel log_buf_len=16M \
                                                   consoleblank=0 tsc=reliable xapic_phys",
                                     .vpci_vdev_array = &vpci_vdev_array1,

* Build ACRN with the above changes.

  .. code-block:: none

     $ make PLATFORM=sbl


* Install ACRN

  First we should add one menuentry in ``/boot/efi/grub/grub.cfg`` on UP2 board

  .. code-block:: none

     menuentry 'ACRN Partition Mode' --class ubuntu --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-e23c76ae-b06d-4a6e-ad42-46b8eedfd7d3' {
             recordfail
             load_video
             gfxmode $linux_gfx_mode
             insmod gzio
             insmod part_gpt
             insmod ext2

             echo 'Loading partition mode hypervisor ...'
             multiboot /acrn.32.out
             module /bzImage
     }

  Copy ``acrn.32.out`` and bzImage to ``/boot/efi``

  .. code-block:: none

     scp username@IP:~/acrn-hypervisor/hypervisor/build/acrn.32.out /boot/efi

* Boot ACRN

  Plug the USB flash drive and SATA Disk into your UP2 board and then reboot the
  board to the built-in EFI shell. Then execute the following command to load
  the ACRN hypervisor.

  .. code-block:: none

     shell> fs1:
     fs1:\> EFI\ubuntu\grubx64.efi

  Here we will see the following GRUB menu. Select entry ``ACRN Partition Mode`` and
  press ``Enter`` and then the ACRN hypervisor will be loaded automatically.

  .. code-block:: console
     :emphasize-lines: 2

     *Ubuntu
      ACRN Partition Mode
      Advanced options for Ubuntu
      System setup
      Restore Ubuntu 16.04 to factory state


Playing with ACRN hypervisor with partition mode
************************************************

* Connect the serial port of the UP2 board with another PC and use the following
  command to check if vms booted successfully. If vms are booted successfully,
  we will see the following message.

   .. code-block:: none

      ACRN:\>sos_console 1

      ----- Entering Guest 1 Shell -----

      root@clr-e8216ad453ad4f08914d83cbc50f838c ~ #

      ---Entering ACRN SHELL---
      ACRN:\>sos_console 2

      ----- Entering Guest 2 Shell -----

      root@clr-bdbe97b256864a92b44a9449bf229769 ~ #

      ---Entering ACRN SHELL---
      ACRN:\>
