:orphan:

ACRN Sample Application Image Builder
#####################################

This directory contains the scripts to create VM images for ACRN sample
application.

Prerequisites
*************

Make sure you have Ubuntu installed with network access on your development
computer. Then execute the following command to install the prerequisites.

   .. code-block:: bash

      sudo apt install -y kpartx \
           schroot \
           mount \
	   wget \
	   qemu-utils

Also you'll need the Debian packages of Linux RT kernels built from
https://github.com/projectacrn/acrn-kernel when (and only when) building a
real-time VM image. After the kernel is built, copy those Debian packages (whose
names looks like ``linux-libc-*``, ``linux-headers-*`` and ``linux-image-*``) to
this directory (i.e. misc/sample_application/image_builder/).

Build images
************

To build the VM image for graphical HMI, run the following command under this
directory:

   .. code-block:: bash

      ./create_image.sh hmi-vm

This will generate an image named ``hmi_vm.img`` under the this directory, which
can be used as the file of a virtio-blk device of a post-launched VM. Installing
the GNOME desktop system will take some time depending on your network and
storage speed.

To build the VM image for running real-time applications, run the following
command under this directory:

   .. code-block:: bash

      ./create_image.sh rt-vm

This will generate an image named ``rt_vm.img`` under the this directory.
