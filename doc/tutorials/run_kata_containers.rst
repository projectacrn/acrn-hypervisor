.. _run-kata-containers:

Run Kata Containers on a Service VM
###################################

This tutorial describes how to install, configure, and run `Kata Containers
<https://katacontainers.io/>`_ on the ACRN Service VM. In this configuration,
Kata Containers leverage the ACRN hypervisor instead of QEMU which is used by
default. Refer to the `Kata Containers with ACRN
<https://drive.google.com/file/d/1ZrqM5ouWUJA0FeIWhU_aitEJe8781rpe/view?usp=sharing>`_
presentation from a previous ACRN Project Technical Community Meeting for more
details on Kata Containers and how the integration with ACRN has been done.

Pre-Requisites
**************

.. _kata prerequisites:
   https://github.com/kata-containers/documentation/blob/master/how-to/how-to-use-kata-containers-with-acrn.md#pre-requisites

#. Refer to the :ref:`ACRN supported hardware <hardware>`.
#. For a default prebuilt ACRN binary in the E2E package, you must have 4 CPU cores or enable "CPU Hyper-Threading” in order to have 4 CPU threads for 2 CPU cores.
#. Follow :ref:`these instructions <quick-setup-guide>` to set up the ACRN Service VM.
#. Build the ACRN kernel (required to support ``macvtap``, enabled by default since `247a3ba9243b <https://github.com/projectacrn/acrn-kernel/commit/247a3ba9243b1fd8c2d763158d55f8791a9cac94>`_).

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel.git
      $ cd acrn-kernel
      $ cp kernel_config_sos .config
      $ make clean && make olddefconfig && make && make modules_install INSTALL_MOD_PATH=out/

   Log in to the Service VM and use the new ACRN kernel:

   .. code-block:: none

      $ sudo mount /dev/sda1 /mnt
      $ sudo scp -r <user name>@<host address>:<your workspace>/acrn-kernel/arch/x86/boot/bzImage /mnt/
      $ sudo scp -r <user name>@<host address>:<your workspace>/acrn-kernel/out/lib/modules/* /lib/modules/
      $ conf_file=`sed -n '$ s/default //p' /mnt/loader/loader.conf`.conf
      $ kernel_img=`sed -n 2p /mnt/loader/entries/$conf_file | cut -d'/' -f4`
      $ sed -i "s/$kernel_img/bzImage/g" /mnt/loader/entries/$conf_file
      $ sync && sudo umount /mnt && reboot

  .. note::
     Adjust the EFI System Partition (ESP) device node (``/dev/sda1`` in the example above) to match your system setup.

Configure Kata on ACRN
**********************

Follow these `kata instructions
<https://github.com/kata-containers/documentation/blob/master/how-to/how-to-use-kata-containers-with-acrn.md>`_
to configure and launch the Kata VMs with ACRN.
