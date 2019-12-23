.. _run-kata-containers:

Run Kata containers on a Service VM
###################################

This tutorial describes how to install, configure, and run Kata containers on
the ACRN Service VM.

Pre-Requisites
**************

.. _kata prerequisites:
   https://github.com/kata-containers/documentation/blob/master/how-to/how-to-use-kata-containers-with-acrn.md#pre-requisites

#. Refer to the :ref:`ACRN supported hardware <hardware>`.
#. For a default prebuilt ACRN binary in the E2E package, you must have 4 CPU cores or enable "CPU Hyper-Threading” in order to have 4 CPU threads for 2 CPU cores.
#. Follow :ref:`these instructions <quick-setup-guide>` to set up the ACRN Service VM.
#. Build the ACRN kernel to enable the ``macvtap`` function:

   .. code-block:: none

      $ git clone https://github.com/projectacrn/acrn-kernel.git
      $ cd acrn-kernel
      $ cp kernel_config_sos .config
      $ sed -i "s/# CONFIG_MACVLAN is not set/CONFIG_MACVLAN=y/" .config
      $ sed -i '$ i CONFIG_MACVTAP=y' .config
      $ make clean && make olddefconfig && make && sudo make modules_install INSTALL_MOD_PATH=out/

   Log in to the Service VM and use the new ACRN kernel:

   .. code-block:: none

      $ sudo mount /dev/sda1 /mnt
      $ sudo scp -r <user name>@<host address>:<your workspace>/acrn-kernel/arch/x86/boot/bzImage /mnt/
      $ sudo scp -r <user name>@<host address>:<your workspace>/acrn-kernel/out/lib/modules/* /lib/modules/
      $ conf_file=`sed -n '$ s/default //p' /mnt/loader/loader.conf`.conf
      $ kernel_img=`sed -n 2p /mnt/loader/entries/$conf_file | cut -d'/' -f4`
      $ sed -i "s/$kernel_img/bzImage/g" /mnt/loader/entries/$conf_file
      $ sync && umount /mnt && sudo reboot

Configure Kata on ACRN
**********************

Follow these `kata instructions
<https://github.com/kata-containers/documentation/blob/master/how-to/how-to-use-kata-containers-with-acrn.md>`_
to configure and launch the Kata VMs with ACRN.
