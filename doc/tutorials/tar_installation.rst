.. _tar_installation:

ACRN Installation via Tar Files
####################################

Project ACRN offers two ways to install ACRN on target systems, either via
Debian packages or tar files. This document covers the tar file method. For
information about the Debian method, see :ref:`debian_packaging`.

Tar files provide a way to package ACRN configurations for Linux
target systems running non-Debian operating systems. You generate the tar
files on your development computer, copy them to your target system,
extract the tar files to the right places,
and reboot the system with ACRN up and running.

This document shows examples of commands used to build and install the tar
files. You might need to modify the commands for your environment.

Prerequisites
*************

* A development computer configured according to the :ref:`gsg` (for example, it
  has an Ubuntu OS, ACRN dependencies, and ACRN hypervisor and kernel source
  code from the ACRN GitHub repository).

Build the Tar Files
***************************

#. On your development computer, navigate to the ``acrn-hypervisor`` source code
   directory and build the ACRN hypervisor tar files. Replace
   ``<board.xml>`` and ``<scenario.xml>`` with the paths to your board
   configuration file and scenario configuration file.

   .. code-block:: bash

      cd ~/acrn-work/acrn-hypervisor
      make -j $(nproc) BOARD=<board.xml> SCENARIO=<scenario.xml>
      make targz-pkg

   The build typically takes a few minutes. By default, the build results are
   found in the ``build`` directory.

#. Navigate to the ``acrn-kernel`` source code directory and build the ACRN
   kernel tar files for the Service VM:

   .. code-block:: bash

      cd ~/acrn-work/acrn-kernel
      cp kernel_config_service_vm .config
      make olddefconfig
      make -j $(nproc) targz-pkg

   The kernel build can take 15 minutes or less on a fast computer, but could
   take an hour or more depending on the performance of your development
   computer. By default, the build results are found in the current directory. 

Install and Run ACRN
**************************

In the following steps, you will install the tar files, install the serial
configuration tool, configure GRUB, and run ACRN on the target system.

#. Copy all the necessary files generated on the development computer to the
   target system. The following steps show how to copy via USB disk; feel free
   to use a different method. Modify the file names in the following commands to
   match your files.

   a. Insert the USB disk into the development computer and run these commands:

      .. code-block:: bash

         cd ~/acrn-work/acrn-kernel
         disk="/media/$USER/"$(ls /media/$USER)
         cp linux-5.10.90-acrn-service-vm-206626-g140f5035e1b1-x86.tar.gz "$disk"/
         cp ~/acrn-work/acpica-unix-20210105/generate/unix/bin/iasl "$disk"/
         cp ~/acrn-work/acrn-hypervisor/build/acrn-2.8-unstable.tar.gz "$disk"/
         sync && sudo umount "$disk"/

   #. Insert the USB disk you just used into the target system and run these
      commands to copy the files locally:

      .. code-block:: bash

         disk="/media/$USER/"$(ls /media/$USER)
         cp "$disk"/linux-5.10.90-acrn-service-vm-206626-g140f5035e1b1-x86.tar.gz ~/acrn-work
         cp "$disk"/acrn-2.8-unstable.tar.gz ~/acrn-work
         sudo cp "$disk"/iasl /usr/sbin/
         sync && sudo umount "$disk"/

#. Extract the Service VM files onto the target system:

   .. code-block:: bash

      cd ~/acrn-work
      sudo tar -zxvf linux-5.10.90-acrn-service-vm-206626-g140f5035e1b1-x86.tar.gz -C / --keep-directory-symlink

#. Extract the ACRN tools and images:

   .. code-block:: bash

      sudo tar -zxvf acrn-2.8-unstable.tar.gz -C / --keep-directory-symlink

#. Copy the ACRN bin file to the boot directory. Replace ``<board>`` and
   ``<scenario>`` to match your file.

   .. code-block:: bash

      sudo mkdir -p /boot/acrn/
      sudo cp /usr/lib64/acrn/acrn.<board>.<scenario>.bin /boot/acrn

#. Install the serial configuration tool in the target system as follows:

   .. code-block:: bash

      sudo apt install setserial

#. Find the root filesystem (rootfs) device name by using the
   ``lsblk`` command:

   .. code-block:: console
      :emphasize-lines: 24

      ~$ lsblk
      NAME        MAJ:MIN RM   SIZE RO TYPE MOUNTPOINT
      loop0         7:0    0 255.6M  1 loop /snap/gnome-3-34-1804/36
      loop1         7:1    0  62.1M  1 loop /snap/gtk-common-themes/1506
      loop2         7:2    0   2.5M  1 loop /snap/gnome-calculator/884
      loop3         7:3    0 241.4M  1 loop /snap/gnome-3-38-2004/70
      loop4         7:4    0  61.8M  1 loop /snap/core20/1081
      loop5         7:5    0   956K  1 loop /snap/gnome-logs/100
      loop6         7:6    0   2.2M  1 loop /snap/gnome-system-monitor/148
      loop7         7:7    0   2.4M  1 loop /snap/gnome-calculator/748
      loop8         7:8    0  29.9M  1 loop /snap/snapd/8542
      loop9         7:9    0  32.3M  1 loop /snap/snapd/12704
      loop10        7:10   0  65.1M  1 loop /snap/gtk-common-themes/1515
      loop11        7:11   0   219M  1 loop /snap/gnome-3-34-1804/72
      loop12        7:12   0  55.4M  1 loop /snap/core18/2128
      loop13        7:13   0  55.5M  1 loop /snap/core18/2074
      loop14        7:14   0   2.5M  1 loop /snap/gnome-system-monitor/163
      loop15        7:15   0   704K  1 loop /snap/gnome-characters/726
      loop16        7:16   0   276K  1 loop /snap/gnome-characters/550
      loop17        7:17   0   548K  1 loop /snap/gnome-logs/106
      loop18        7:18   0 243.9M  1 loop /snap/gnome-3-38-2004/39
      nvme0n1     259:0    0 119.2G  0 disk 
      ├─nvme0n1p1 259:1    0   512M  0 part /boot/efi
      └─nvme0n1p2 259:2    0 118.8G  0 part /

   As highlighted, you're looking for the device name associated with the
   partition named ``/``, in this case ``nvme0n1p2``.

#. Run the ``blkid`` command to get the UUID and PARTUUID for the rootfs device
   (replace the ``nvme0n1p2`` name with the name shown for the rootfs on your
   system):

   .. code-block:: bash

      sudo blkid /dev/nvme0n1p2

   In the output, look for the UUID and PARTUUID (example below). You will need
   them in the next step.

   .. code-block:: console

      /dev/nvme0n1p2: UUID="3cac5675-e329-4cal-b346-0a3e65f99016" TYPE="ext4" PARTUUID="03db7f45-8a6c-454b-adf7-30343d82c4f4"

#. Add the ACRN Service VM to the GRUB boot menu:

   a. Edit the GRUB ``40_custom`` file. The following command uses ``vi``, but
      you can use any text editor.

      .. code-block:: bash

         sudo vi /etc/grub.d/40_custom

   #. Add the following text at the end of the file. Replace ``UUID`` and
      ``PARTUUID`` with the output from the previous step. Replace ``<board>``
      and ``<scenario>`` to match your bin file. Confirm the module2 file name
      matches the file in your ``boot`` directory.

      .. code-block:: bash
         :emphasize-lines: 6,8,9

         menuentry "ACRN Multiboot Ubuntu Service VM" --id ubuntu-service-vm {
           load_video
           insmod gzio
           insmod part_gpt
           insmod ext2
           search --no-floppy --fs-uuid --set "UUID"
           echo 'loading ACRN...'
           multiboot2 /boot/acrn/acrn.<board>.<scenario>.bin  root=PARTUUID="PARTUUID"
           module2 /boot/vmlinuz-5.10.90-acrn-service-vm-206626-g140f5035e1b1 Linux_bzImage
         }

      Example:

      .. code-block:: console

         menuentry "ACRN Multiboot Ubuntu Service VM" --id ubuntu-service-vm {
           load_video
           insmod gzio
           insmod part_gpt
           insmod ext2
           search --no-floppy --fs-uuid --set "3cac5675-e329-4cal-b346-0a3e65f99016"
           echo 'loading ACRN...'
           multiboot2 /boot/acrn/acrn.my_board.shared.bin  root=PARTUUID="03db7f45-8a6c-454b-adf7-30343d82c4f4"
           module2 /boot/vmlinuz-5.10.90-acrn-service-vm-206626-g140f5035e1b1 Linux_bzImage
         }

   #. Save and close the file.

#. Make the GRUB menu visible when
   booting and make it load the Service VM kernel by default:

   a. Edit the ``grub`` file:

      .. code-block:: bash

         sudo vi /etc/default/grub

   #. Edit lines with these settings (comment out the ``GRUB_TIMEOUT_STYLE``
      line). Leave other lines as they are:

      .. code-block:: bash

         GRUB_DEFAULT=ubuntu-service-vm
         #GRUB_TIMEOUT_STYLE=hidden
         GRUB_TIMEOUT=5

   #. Save and close the file.

#. Update GRUB and reboot the system:

   .. code-block:: bash

      sudo update-grub
      reboot

#. Confirm that you see the GRUB menu with the "ACRN Multiboot Ubuntu Service
   VM" entry. Select it and proceed to booting ACRN. (It may be autoselected, in
   which case it will boot with this option automatically in 5 seconds.)

   .. code-block:: console
      :emphasize-lines: 6

                                GNU GRUB version 2.04
      ────────────────────────────────────────────────────────────────────────────────
      Ubuntu
      Advanced options for Ubuntu
      UEFI Firmware Settings
      *ACRN Multiboot Ubuntu Service VM