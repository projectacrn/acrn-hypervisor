.. _using_xenomai_as_uos:
.. _using_xenomai_as_user_vm:

Run Xenomai as the User RTVM OS
###############################

`Xenomai`_ is a versatile real-time framework that provides support to user space applications that are seamlessly integrated into Linux environments.

This tutorial describes how to run Xenomai as the User VM OS (real-time VM) on the ACRN hypervisor.

.. _Xenomai: https://gitlab.denx.de/Xenomai/xenomai/-/wikis/home

Build the Xenomai Kernel
************************

Follow these instructions to build the Xenomai kernel:

#. Clone the Xenomai kernel source code::

	$ git clone -b F/4.19.59/base/ipipe/xenomai_3.1 https://github.com/intel/linux-stable-xenomai

#. Go to the directory and select the default ACRN configuration::

	$ cd linux-stable-xenomai && make acrn_defconfig

#. Build the kernel::

	$ make targz-pkg

Upon building completion, verify that you see a tarball that contains the kernel and its modules.

   .. code-block:: none
      :emphasize-lines: 22

      $ ls -l
      total 97944
      drwxr-xr-x  27 tw tw     4096 Feb 20 10:23 arch
      drwxr-xr-x   3 tw tw    12288 Feb 21 11:01 block
      -rw-r--r--   1 tw tw   789264 Feb 25 16:17 built-in.a
      drwxr-xr-x   2 tw tw     4096 Feb 21 11:01 certs
      -rw-r--r--   1 tw tw      423 Feb 20 10:17 COPYING
      -rw-r--r--   1 tw tw    98741 Feb 20 10:17 CREDITS
      drwxr-xr-x   4 tw tw    12288 Feb 21 11:01 crypto
      drwxr-xr-x 120 tw tw    12288 Feb 20 10:17 Documentation
      drwxr-xr-x 143 tw tw     4096 Feb 21 10:48 drivers
      drwxr-xr-x   2 tw tw     4096 Feb 20 10:21 firmware
      drwxr-xr-x  73 tw tw    12288 Feb 21 11:01 fs
      drwxr-xr-x  32 tw tw     4096 Feb 20 10:19 include
      drwxr-xr-x   2 tw tw     4096 Feb 25 16:25 init
      drwxr-xr-x   2 tw tw     4096 Feb 21 11:01 ipc
      -rw-r--r--   1 tw tw     2245 Feb 20 10:17 Kbuild
      -rw-r--r--   1 tw tw      563 Feb 20 10:17 Kconfig
      drwxr-xr-x  20 tw tw    12288 Feb 25 16:17 kernel
      drwxr-xr-x  13 tw tw    20480 Feb 21 11:01 lib
      drwxr-xr-x   5 tw tw     4096 Feb 20 10:17 LICENSES
      -rw-r--r--   1 tw tw 17572590 Feb 25 16:17 linux-4.19.59-xenomai-3.1-acrn+-x86.tar.gz
      ...

Launch the RTVM
***************

#. Prepare a dedicated disk (NVMe or SATA) for the RTVM; in this example, we use ``/dev/sda``.

   a. Download the Preempt-RT VM image::

      $ wget https://github.com/projectacrn/acrn-hypervisor/releases/download/acrn-2020w01.1-140000p/preempt-rt-32030.img.xz

   #. Decompress the ``image.xz`` image::

      $ xz -d preempt-rt-32030.img.xz

   #. Burn the Preempt-RT VM image onto the SATA disk::

      $ sudo dd if=preempt-rt-32030.img of=/dev/sda bs=4M oflag=sync status=progress iflag=fullblock seek=0 conv=notrunc

#. Launch the RTVM via our script. Indicate the location of the root partition (sda3 in our example) and the kernel tarball::

   $ sudo /usr/share/acrn/samples/nuc/launch_xenomai.sh -b /dev/sda3 -k /path/to/linux-4.19.59-xenomai-3.1-acrn+-x86.tar.gz

#. Verify that a login prompt displays::

    ...
    [  OK  ] Started Permit User Sessions.
    [  OK  ] Started Getty on tty1.
    [  OK  ] Started Serial Getty on hvc0.
    [  OK  ] Reached target Login Prompts.
    [  OK  ] Started Network Manager Script Dispatcher Service.
    [  OK  ] Started Proxy AutoConfig runner service.
    [  OK  ] Started Login Service.
    [  OK  ] Reached target Multi-User System.
    [  OK  ] Reached target Graphical Interface.

    clr-c1ff5bba8c3145ac8478e8e1f96e1087 login:


Install the Xenomai Libraries and Tools
***************************************

To build and install Xenomai tools or its libraries in the RVTM, refer to the official
`Xenomai documentation <https://gitlab.denx.de/Xenomai/xenomai/-/wikis/Installing_Xenomai_3#library-install>`_.
Note that the current supported version is Xenomai-3.1 with the 4.19.59 kernel.
