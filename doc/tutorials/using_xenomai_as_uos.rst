.. _using_xenomai_as_uos:

Using Xenomai as User OS
########################

Build Xenomai kernel
********************

#. Clone Xenomai kernel souce code::

	# git clone -b F/4.19.59/base/ipipe/xenomai_3.1 https://github.com/intel/linux-stable-xenomai

#. Enter the directory and select default ACRN configuration::

	# cd linux-stable-xenomai && make acrn_defconfig

#. 	Then just start building the kernel::

	# make targz-pkg

#. Upon building completion, you will see a tarball containing kernel and its modules.

   .. code-block:: none
      :emphasize-lines: 22

      # ls -l
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
      -rw-r--r--   1 tw tw 17572590 Feb 25 16:17 linux-4.19.59-xenomai-3.1-tw+-x86.tar.gz
      ...

Launch RTVM
***********

#. Prepare a dedicated disk (NVMe or SATA) for RTVM, just as you did :ref:`for Preempt-RT <install_rtvm>`, here we suppose it's ``/dev/sda1``.

#. Then you can launch RTVM via our script. We need to tell it where the storage device and kernel tarball are::

	# /usr/share/acrn/samples/nuc/launch_xenomai.sh -b /dev/sda1 -k /path/to/linux-4.19.59-xenomai-3.1-tw+-x86.tar.gz

#. If everything works, you will see a login prompt::

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


Installing the Xenomai libraries and tools
******************************************

To use some Xenomai tools or its libraries, please follow the `official document
<https://gitlab.denx.de/Xenomai/xenomai/-/wikis/Installing_Xenomai_3#library-install>`__
to build and install them in RTVM. Please note that the according version is Xenomai-3.1 with
4.19.59 kernel.
