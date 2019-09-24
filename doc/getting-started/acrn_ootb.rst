 .. _acrn_ootb:

Install ACRN use out-of-the-box image
#####################################
In this tutorial, we will learn to generate an out-of-the-box (OOTB) Service VM
or even a Preempt-RT VM image so that we can use ACRN or RTVM immediately after 
installation without any configuration or modification.

Set up build environment
************************
#. Follow the `Clear Linux OS installation guide
   <https://clearlinux.org/documentation/clear-linux/get-started/bare-metal-install-server>`_
   to install a native Clear Linux OS on development machine.

#. Login in the Clear Linux OS and install these bundles::

   $ sudo swupd bundle-add clr-installer vim network-basic

.. _set_up_ootb_service_vm:

Generate Service VM image
*************************

Create Service VM image YAML file and script
============================================
- **Example 1**: `ACRN SDC scenario`

  #. Create the ACRN SDC ``service-os.yaml`` file:
     
     .. code-block:: console
  
        $ mkdir -p ~/service-os && cd ~/service-os
        $ vim service-os.yaml
  
     Update the ``service-os.yaml`` to:
  
     .. code-block:: bash
        :emphasize-lines: 51
     
        block-devices: [
           {name: "bdevice", file: "sos.img"}
        ]
        
        targetMedia:
        - name: ${bdevice}
          size: "108.54G"
          type: disk
          children:
          - name: ${bdevice}1
            fstype: vfat
            mountpoint: /boot
            size: "512M"
            type: part
          - name: ${bdevice}2
            fstype: swap
            size: "32M"
            type: part
          - name: ${bdevice}3
            fstype: ext4
            mountpoint: /
            size: "108G"
            type: part
        
        bundles: [
            bootloader,
            editors,
            network-basic,
            openssh-server,
            os-core,
            os-core-update,
            sysadmin-basic,
            systemd-networkd-autostart,
            service-os
          ]
        
        autoUpdate: false
        postArchive: false
        postReboot: false
        telemetry: false
        hostname: clr-sos
        
        keyboard: us
        language: en_US.UTF-8
        kernel: kernel-iot-lts2018-sos
        
        post-install: [
           {cmd: "${yamlDir}/service-os-post.sh ${chrootDir}"},
        ]
        
        version: 30970
     
     .. note:: Update version value to your target Clear Linux version.
  
  #. Create ACRN SDC ``service-os-post.sh``:
  
     .. code-block:: console
  
        $ vim service-os-post.sh
  
     Update the ``service-os-post.sh`` to:
  
     .. code-block:: bash
  
        #!/bin/bash
   
        # ACRN SOS Image Post Install steps
         
        set -ex
         
        CHROOTPATH=$1
         
        # acrn.efi path
        acrn_efi_path="$CHROOTPATH/usr/lib/acrn/acrn.efi"
         
        # copy acrn.efi to efi partition
        mkdir -p "$CHROOTPATH/boot/EFI/acrn" || exit 1
        cp "$acrn_efi_path" "$CHROOTPATH/boot/EFI/acrn" || exit 1
         
        # create load.conf
        echo "Add default (5 seconds) boot wait time"
        echo "timeout 5" >> "$CHROOTPATH/boot/loader/loader.conf" || exit 1
         
        chroot $CHROOTPATH systemd-machine-id-setup
        chroot $CHROOTPATH systemctl enable getty@tty1.service
         
        echo "Welcome to the Clear Linux* ACRN SOS image!
         
        Please login as root for the first time!
         
        " >> $1/etc/issue
         
        exit 0

- **Example 2**: `ACRN INDUSTRY scenario`

  #. Create ACRN INDUSTRY ``service-os-industry.yaml`` file:

     .. code-block:: console

        $ mkdir -p ~/service-os-industry && cd ~/service-os-industry
        $ vim service-os-industry.yaml

     Update the ``service-os-industry.yaml`` to:

     .. code-block:: bash
        :emphasize-lines: 52

        block-devices: [
           {name: "bdevice", file: "sos-industry.img"}
        ]
         
        targetMedia:
        - name: ${bdevice}
          size: "108.54G"
          type: disk
          children:
          - name: ${bdevice}1
            fstype: vfat
            mountpoint: /boot
            size: "512M"
            type: part
          - name: ${bdevice}2
            fstype: swap
            size: "32M"
            type: part
          - name: ${bdevice}3
            fstype: ext4
            mountpoint: /
            size: "108G"
            type: part
         
        bundles: [
            bootloader,
            editors,
            network-basic,
            openssh-server,
            os-core,
            os-core-update,
            sysadmin-basic,
            systemd-networkd-autostart,
            service-os
          ]
         
        autoUpdate: false
        postArchive: false
        postReboot: false
        telemetry: false
        hostname: clr-sos
         
        keyboard: us
        language: en_US.UTF-8
        kernel: kernel-iot-lts2018-sos
         
         
        post-install: [
           {cmd: "${yamlDir}/service-os-industry-post.sh ${chrootDir}"},
        ]
         
        version: 30970

     .. note:: Update version value to your target Clear Linux version.

#. Create ``service-os-industry-post.sh``:

   .. code-block:: console

      $ vim service-os-industry-post.sh

   Update the ``service-os-industry-post.sh`` to:

   .. code-block:: bash

      #!/bin/bash

      # ACRN SOS Image Post Install steps
      
      set -ex
      
      CHROOTPATH=$1
      
      # acrn.kbl-nuc-i7.industry.efi path
      acrn_industry_efi_path="$CHROOTPATH/usr/lib/acrn/acrn.kbl-nuc-i7.industry.efi"
      
      # copy acrn.efi to efi partition
      mkdir -p "$CHROOTPATH/boot/EFI/acrn" || exit 1
      cp "$acrn_industry_efi_path" "$CHROOTPATH/boot/EFI/acrn/acrn.efi" || exit 1
      
      # create load.conf
      echo "Add default (5 seconds) boot wait time"
      echo "timeout 5" >> "$CHROOTPATH/boot/loader/loader.conf" || exit 1
      
      chroot $CHROOTPATH systemd-machine-id-setup
      chroot $CHROOTPATH systemctl enable getty@tty1.service
      
      echo "Welcome to the Clear Linux* ACRN SOS Industry image!
      
      Please login as root for the first time!
      
      " >> $1/etc/issue
      
      exit 0

Use clr-installer to build Service VM image
===========================================
- Build ACRN SDC Service VM image:

  .. code-block:: console

     $ cd ~/service-os
     $ sudo clr-installer -c service-os.yaml

  .. note:: The ``service-os.img`` will be generated at current directory.


- Build ACRN INDUSTRY Service VM image:

  .. code-block:: console

     $ cd ~/service-os-industry
     $ sudo clr-installer -c service-os-industry.yaml

  .. note:: The ``service-os-industry.img`` will be generated at current directory.

Deploy Service VM image
=======================
#. Prepare a U disk with at least 8GB memory.

#. Follow these steps to create two partitions on the U disk,
   the first partition is 4G, and the second partition use the left free space:

   .. code-block:: console

      # sudo gdisk /dev/sdb
      GPT fdisk (gdisk) version 1.0.3
       
      Partition table scan:
        MBR: protective
        BSD: not present
        APM: not present
        GPT: present
       
      Found valid GPT with protective MBR; using GPT.
       
      Command (? for help): n
      Partition number (1-128, default 1):
      First sector (34-15249374, default = 2048) or {+-}size{KMGTP}:
      Last sector (2048-15249374, default = 15249374) or {+-}size{KMGTP}: +4G
      Current type is 'Linux filesystem'
      Hex code or GUID (L to show codes, Enter = 8300):
      Changed type of partition to 'Linux filesystem'
       
      Command (? for help): n
      Partition number (2-128, default 2):
      First sector (34-15249374, default = 8390656) or {+-}size{KMGTP}:
      Last sector (8390656-15249374, default = 15249374) or {+-}size{KMGTP}:
      Current type is 'Linux filesystem'
      Hex code or GUID (L to show codes, Enter = 8300):
      Changed type of partition to 'Linux filesystem'
       
      Command (? for help): p
      Disk /dev/sdb: 15249408 sectors, 7.3 GiB
      Model: USB FLASH DRIVE
      Sector size (logical/physical): 512/512 bytes
      Disk identifier (GUID): 8C6BF21D-521A-49D5-8BC8-5B319FAF3F91
      Partition table holds up to 128 entries
      Main partition table begins at sector 2 and ends at sector 33
      First usable sector is 34, last usable sector is 15249374
      Partitions will be aligned on 2048-sector boundaries
      Total free space is 2014 sectors (1007.0 KiB)
       
      Number  Start (sector)    End (sector)  Size       Code  Name
         1            2048         8390655   4.0 GiB     8300  Linux filesystem
         2         8390656        15249374   3.3 GiB     8300  Linux filesystem
       
      Command (? for help): w
       
      Final checks complete. About to write GPT data. THIS WILL OVERWRITE EXISTING
      PARTITIONS!!
       
      Do you want to proceed? (Y/N): Y
      OK; writing new GUID partition table (GPT) to /dev/sdb.
      The operation has completed successfully.

#. Download and install a bootable Clear Linux on the U disk:

   .. code-block:: console

      $ wget https://download.clearlinux.org/releases/30970/clear/clear-30970-live-server.iso.xz
      $ xz -d clear-30970-live-server.iso.xz
      $ sudo dd if=clear-30970-live-server.iso of=/dev/sdb1 bs=4M oflag=sync status=progress

#. Copy the ``service-os.img`` or ``service-os-industry.img`` to the U disk:

   .. code-block:: console

      $ sudo mkfs.ext4 /dev/sdb2
      $ sudo mount /dev/sdb2 /mnt

   - SDC scenario:

     .. code-block:: console

        $ cp ~/service-os/service-os.img /mnt
        $ sync && umount /mnt

   - INDUSTRY scenario:

     .. code-block:: console

        $ cp ~/service-os-industry/service-os-industry.img /mnt
        $ sync && umount /mnt

#. Unplug the U disk from development machine and plugin it to your test machine.

#. Reboot the test machine and boot from USB.

#. Login in Live Service Clear Linux OS with "root" account,
   mount the second partition on U disk:

   .. code-block:: console

      # mount /dev/sdb2 /mnt

#. Format the disk which will install the Service VM image:

   .. code-block:: console

      # sudo gdisk /dev/nvme0n1
      GPT fdisk (gdisk) version 1.0.3
       
      Partition table scan:
        MBR: protective
        BSD: not present
        APM: not present
        GPT: present
       
      Found valid GPT with protective MBR; using GPT.
       
      Command (? for help): o
      This option deletes all partitions and creates a new protective MBR.
      Proceed? (Y/N): Y
       
      Command (? for help): w
       
      Final checks complete. About to write GPT data. THIS WILL OVERWRITE EXISTING
      PARTITIONS!!
       
      Do you want to proceed? (Y/N): Y
      OK; writing new GUID partition table (GPT) to /dev/nvme0n1.
      The operation has completed successfully.

#. Delete the old ACRN EFI firmware info:

   .. code-block:: console

      # efibootmgr | grep ACRN | cut -d'*' -f1 | cut -d't' -f2 | xargs -i efibootmgr -b {} -B

#. Write the Service VM:

   - ACRN SDC scenario:

     .. code-block:: console

        # dd if=/mnt/sos.img of=/dev/nvme0n1 bs=4M oflag=sync status=progress

   - ACRN INDUSTRY scenario:

     .. code-block:: console

        # dd if=/mnt/sos-industry.img of=/dev/nvme0n1 bs=4M oflag=sync status=progress

#. Configure the EFI firmware to boot the ACRN hypervisor by default:

   .. code-block:: console

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/nvme0n1 -p 1 -L "ACRN"

#. Unplug the U disk and reboot the test machine, and after the Clear Linux OS boots,
   everything is ready, you could login as "root" for the first time.

.. _set_up_ootb_rtvm:

Generate User VM Preempt-RT image
*********************************

Create Preempt-RT image YAML file and script
============================================
#. Create the ``preempt-rt.yaml`` file:

   .. code-block:: console

      $ mkdir -p ~/preempt-rt && cd ~/preempt-rt
      $ vim preempt-rt.yaml

   Update ``preempt-rt.yaml`` to:

   .. code-block:: bash
      :emphasize-lines: 46

      block-devices: [
         {name: "bdevice", file: "preempt-rt.img"}
      ]
       
      targetMedia:
      - name: ${bdevice}
        size: "8.54G"
        type: disk
        children:
        - name: ${bdevice}1
          fstype: vfat
          mountpoint: /boot
          size: "512M"
          type: part
        - name: ${bdevice}2
          fstype: swap
          size: "32M"
          type: part
        - name: ${bdevice}3
          fstype: ext4
          mountpoint: /
          size: "8G"
          type: part
       
      bundles: [
          bootloader,
          editors,
          network-basic,
          openssh-server,
          os-core,
          os-core-update,
          sysadmin-basic,
          systemd-networkd-autostart
        ]
       
      autoUpdate: false
      postArchive: false
      postReboot: false
      telemetry: false
      hostname: clr-preempt-rt
       
      keyboard: us
      language: en_US.UTF-8
      kernel: kernel-lts2018-preempt-rt
       
      version: 30970

   .. note:: Update version value to your target Clear Linux version

Build User VM Preempt-RT image
==============================

.. code-block:: console

   $ sudo clr-installer -c preempt-rt.yaml

The ``preempt-rt.img`` will be generated at current directory.

Deploy User VM Preempt-RT image
===============================
#. Login the Service VM and copy the ``preempt-rt.img`` from development machine:

   .. code-block:: console

      $ mkdir -p preempt-rt && cd preempt-rt
      $ scp <development username>@<development machine ip>:<path to preempt-rt.img> .

#. Write ``preempt-rt.img`` to disk:

   .. code-block:: console

      $ sudo dd if=<path to preempt-rt.img> of=/dev/sda bs=4M oflag=sync status=progress

#. Copy the ``OVMF.fd`` and ``launch_hard_rt_vm.sh``:

   .. code-block:: console

      $ cp /usr/share/acrn/bios/OVMF.fd .
      $ cp /usr/share/acrn/samples/nuc/launch_hard_rt_vm.sh .

#. Launch the Preempt-RT User VM:

   .. code-block:: console

      $ chmod +x launch_hard_rt_vm.sh
      $ ./launch_hard_rt_vm.sh
