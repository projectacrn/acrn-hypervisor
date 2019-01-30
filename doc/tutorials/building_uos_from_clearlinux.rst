.. _build UOS from Clearlinux:

Building UOS from Clear Linux OS
################################

This document builds on the :ref:`getting_started`,
and explains how to build UOS from Clear Linux OS.

Build UOS image in Clear Linux OS
*********************************

Follow these steps to build a UOS image from Clear Linux OS:

#. In Clear Linux OS, install ``ister`` (a template-based
   installer for Linux) included in the Clear Linux OS bundle
   ``os-installer``.
   For more information about ``ister``,
   please visit https://github.com/bryteise/ister.

   .. code-block:: none

      $ sudo swupd bundle-add os-installer

#. After installation is complete, use ``ister.py`` to
   generate the image for UOS with the configuration in
   ``uos-image.json``:

   .. code-block:: none

      $ cd ~
      $ sudo ister.py -t uos-image.json

   An example of the configuration file ``uos-image.json``:

   .. code-block:: none

      {
          "DestinationType" : "virtual",
          "PartitionLayout" : [ { "disk" : "uos.img",
                                  "partition" : 1,
                                  "size" : "512M",
                                  "type" : "EFI" },
                                { "disk" : "uos.img",
                                  "partition" : 2,
                                  "size" : "1G",
                                  "type" : "swap" },
                                { "disk" : "uos.img",
                                  "partition" : 3,
                                  "size" : "8G",
                                  "type" : "linux" } ],
          "FilesystemTypes" : [ { "disk" : "uos.img",
                                  "partition" : 1,
                                  "type" : "vfat" },
                                { "disk" : "uos.img",
                                  "partition" : 2,
                                  "type" : "swap" },
                                { "disk" : "uos.img",
                                  "partition" : 3,
                                  "type" : "ext4" } ],
          "PartitionMountPoints" : [ { "disk" : "uos.img",
                                       "partition" : 1,
                                       "mount" : "/boot" },
                                     { "disk" : "uos.img",
                                       "partition" : 3,
                                       "mount" : "/" } ],
          "Version": "latest",
          "Bundles": ["bootloader",
                      "editors",
                      "kernel-iot-lts2018",
                      "network-basic",
                      "os-core-update",
                      "os-core",
                      "openssh-server",
                      "sysadmin-basic"]
       }

   .. note::
      To generate the image with a specified version,
      please modify the ``"Version"`` argument,
      and we can set ``"Version": 26550`` instead of
      ``"Version": "latest"`` for example.

   Here we will use ``"Version": 26550`` for example,
   and the UOS image called ``uos.img`` will be generated
   after successful installation. An example output log is:

   .. code-block:: none

      Reading configuration
      Validating configuration
      Creating virtual disk
      Creating partitions
      Mapping loop device
      Creating file systems
      Setting up mount points
      Starting swupd. May take several minutes
      Installing 9 bundles (and dependencies)...
      Verifying version 26550
      Downloading packs...

      Extracting emacs pack for version 26550

      Extracting vim pack for version 26550
      ...
      Cleaning up
      Successful installation

#. On your target device, boot the system and select "The ACRN Service OS", as shown below:

   .. code-block:: console
      :emphasize-lines: 1

      => The ACRN Service OS
      Clear Linux OS for Intel Architecture (Clear-linux-iot-lts2018-4.19.0-19)
      Clear Linux OS for Intel Architecture (Clear-linux-iot-lts2018-sos-4.19.0-19)
      Clear Linux OS for Intel Architecture (Clear-linux-native.4.19.1-654)
      EFI Default Loader
      Reboot Into Firmware Interface


Start the User OS (UOS)
***********************

#. Mount the UOS image and check the UOS kernel:

   .. code-block:: none

      # losetup -r -f -P --show ~/uos.img
      # mount /dev/loop0p3 /mnt

      # ls -l /mnt/usr/lib/kernel/

      cmdline-4.19.0-26.iot-lts2018
      config-4.19.0-26.iot-lts2018
      default-iot-lts2018 -> org.clearlinux.iot-lts2018.4.19.0-26
      install.d
      org.clearlinux.iot-lts2018.4.19.0-26

#. Adjust the ``/usr/share/acrn/samples/nuc/launch_uos.sh``
   script to match your installation.
   These are the couple of lines you need to modify:

   .. code-block:: none

      -s 3,virtio-blk,~/uos.img \
      -k /mnt/usr/lib/kernel/default-iot-lts2018  \

   .. note::
      UOS image ``uos.img`` is in the directory ``~/``
      and UOS kernel ``default-iot-lts2018`` is in ``/mnt/usr/lib/kernel/``.

#. You are now all set to start the User OS (UOS):

   .. code-block:: none

      $ sudo /usr/share/acrn/samples/nuc/launch_uos.sh

   You are now watching the User OS booting up!
