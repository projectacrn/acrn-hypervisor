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

Increase the virtual disk size
******************************

.. note::

   This step **must** be performed while the User OS (UOS) is powered off.

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

Now that you have increased the size of the virtual disk, power up the Virtual
Machine (VM) before moving to the next steps.

Resize the ``rootfs`` partition
*******************************

Follow the `Resize the partition of the virtual disk <https://clearlinux.org/
documentation/clear-linux/guides/maintenance/increase-virtual-disk-size#resize-
the-partition-of-the-virtual-disk>`_ section to resize the partition hosting the
UOS rootfs (typically ``/dev/sda3``).

Resize the filesystem
*********************

Follow the `Resize the filesystem <https://clearlinux.org/documentation/
clear-linux/guides/maintenance/increase-virtual-disk-size#resize-the-filesytem>`_
section to resize the filesystem to use the entire storage capacity.

Congratulations! You have successfully resized the disk, partition, and
filesystem or your User OS.
