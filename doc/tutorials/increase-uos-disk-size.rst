.. _Increase UOS disk size:

Increasing the User OS disk size
################################

This document builds on the :ref:`getting_started` and assumes you already have
a system with ACRN installed and running correctly. The size of the pre-built
Clear Linux User OS (UOS) virtual disk is typically only 8GB and this may not be
sufficient for some applications. This guide explains a simple few steps to
increase the size of that virtual disk.

This document is largely inspired from Clear Linux's `Increase virtual disk size
of a Clear Linux* OS image <https://clearlinux.org/documentation/clear-linux/
guides/maintenance/increase-virtual-disk-size>`_ tutorial. The process can be
broken down into three steps:

1. Increase the virtual disk (``uos.img``) size
#. Resize the ``rootfs`` partition
#. Resize the filesystem

.. note::

   These steps are performed directly on the UOS disk image. The UOS VM **must**
   be powered off during this operation.

Increase the virtual disk size
******************************

We will use the ``qemu-img`` tool to increase the size of the virtual disk
(``uos.img``) file. On a Clear Linux system, you can install this tool using:

.. code-block:: none

   $ sudo swupd bundle-add clr-installer

As an example, let us add 10GB of storage to our virtual disk image called      
``uos.img``.

.. code-block:: none

   $ qemu-img resize -f raw uos.img +10G

.. note::

   Replace ``uos.img`` by the actual name of your virtual disk file if you
   deviated from the :ref:`getting_started`.

.. note::

   You can choose any increment for the additional storage space. Check the
   ``qemu-img resize`` help for more information.

Resize the ``rootfs`` partition
*******************************

The next step is to modify the ``rootfs`` partition (in Clear Linux, it is
partition 3) to use the additional space available. We will use the ``parted``
tool and perform these steps:

* Enter the ``parted`` tool
* Press ``p`` to print the partition tables
* A warning will be displayed, enter ``Fix``
* Enter ``resizepart 3``
* Enter the size of the disk (``19.9GB`` in our example)
* Enter ``q`` to quit the tool

Here is what the sequence looks like:

.. code-block:: none

   $ parted uos.img

.. code-block:: console
   :emphasize-lines: 5,7,9,19,20

   WARNING: You are not superuser.  Watch out for permissions.
   GNU Parted 3.2
   Using /home/gvancuts/uos/uos.img
   Welcome to GNU Parted! Type 'help' to view a list of commands.
   (parted) p                                                                
   Warning: Not all of the space available to /home/gvancuts/uos/uos.img appears to be used, you can fix the GPT to use all of the space (an extra 20971520 blocks) or continue with the current setting? 
   Fix/Ignore? Fix                                                           
   Model:  (file)
   Disk /home/gvancuts/uos/uos.img: 19.9GB
   Sector size (logical/physical): 512B/512B
   Partition Table: gpt
   Disk Flags: 

   Number  Start   End     Size    File system     Name     Flags
    1      1049kB  537MB   536MB   fat16           primary  boot, esp
    2      537MB   570MB   33.6MB  linux-swap(v1)  primary
    3      570MB   9160MB  8590MB  ext4            primary

   (parted) resizepart 3                                                     
   End?  [9160MB]? 19.9GB
   (parted) q                 

Resize the filesystem
*********************

The final step is to resize the ``rootfs`` filesystem to use the entire
partition space.

.. code-block:: none

   $ LOOP_DEV=`sudo losetup -f -P --show uos.img`
   $ PART_DEV=$LOOP_DEV
   $ PART_DEV+="p3"
   $ sudo e2fsck -f $PART_DEV
   $ sudo resize2fs -p $PART_DEV
   $ sudo losetup -d $LOOP_DEV

Congratulations! You have successfully resized the disk, partition, and
filesystem of your User OS.
