.. _using_celadon_as_uos:

Using Celadon as User OS
########################

`Celadon <https://01.org/projectceladon/>`_ is an open source Android* software reference stack
for Intel architecture. It builds upon a vanilla Android stack and incorporates open sourced components
that are optimized for the hardware. This tutorial describes how to run Celadon as the User OS
on the ACRN hypervisor. We are using Kaby Lake-based NUC (model NUC7i7DNHE) in this tutorial.

Prerequisites
*************

* Ubuntu 18.04 or higher with at least 150G free disk space
* Intel Kaby Lake NUC7ixDNHE ( Reference Platforms: :ref:`ACRN supported platforms <hardware>` )
* BIOS version 0059 or later firmware should be flashed on the NUC system,
  and the ``Device Mode`` option is selected on the USB category of the Devices tab,
  in order to enable USB device function through the internal USB 3.0 port header.
* Two HDMI monitors
* A USB dongle (e.g. `Dawson Canyon USB 3.0 female
  to 10-pin header cable <https://www.gorite.com/dawson-canyon-usb-3-0-female-to-10-pin-header-cable>`_)
  is optional if you plan to use the ``adb`` and ``fastboot`` tools in the Celadon User OS for debugging.
  Refer to the `Technical Product Specification
  <https://www.intel.com/content/dam/support/us/en/documents/mini-pcs/nuc-kits/NUC7i5DN_TechProdSpec.pdf>`_
  to identify the USB 3.0 port header on the main board.

Build Celadon from source
*************************

#. Follow the instructions in the `Build Celadon from source
   <https://01.org/projectceladon/documentation/getting_started/build-source>`_ guide
   to set up the Celadon project source code.

#. Select Celadon build target::

      $ cd <Celadon project directory>
      $ source build/envsetup.sh
      $ lunch cel_apl-userdebug

   .. note:: You can run ``lunch`` with no arguments to manually choose your Celadon build variants.

#. Download these additional patches and apply each one individually with the command::

       $ git apply <patch-filename>

   .. table:: ACRN patch list
      :widths: auto
      :name: ACRN patch list

      +--------------------------------------------------------------------+-------------------------------------------+
      | Patch link                                                         | Description                               |
      +====================================================================+===========================================+
      | https://github.com/projectceladon/device-androidia/pull/458        | kernel config: Add the support of ACRN    |
      +--------------------------------------------------------------------+-------------------------------------------+
      | https://github.com/projectceladon/device-androidia-mixins/pull/293 | graphic/mesa: Add the support of ACRN     |
      +--------------------------------------------------------------------+-------------------------------------------+
      | https://github.com/projectceladon/device-androidia/pull/441        | cel_apl: use ttyS0 instead of ttyUSB0     |
      +--------------------------------------------------------------------+-------------------------------------------+
      | https://github.com/projectceladon/device-androidia/pull/439        | Disable trusty and pstore                 |
      +--------------------------------------------------------------------+-------------------------------------------+

   .. note:: If the ``git apply`` command shows an error, you may need to modify
      the source code manually instead.

#. Build Celadon image::

   $ device/intel/mixins/mixin-update
   $ make SPARSE_IMG=true gptimage -j $(nproc)

.. note:: Replace the ``$(nproc)`` argument with the number of processor threads on your workstation
   in order to build the source code with parallel tasks. The Celadon gptimage will be
   generated to ``out/target/product/cel_apl/cel_apl_gptimage.img``

Steps for Using Celadon as User OS
**********************************

#. Follow :ref:`getting-started-apl-nuc` to boot the "ACRN Service OS" based on Clear Linux 29880.

#. Prepare dependencies on your NUC::

   # mkdir ~/celadon && cd ~/celadon
   # cp /usr/share/acrn/bios/OVMF.fd ./
   # cp /usr/share/acrn/samples/nuc/launch_win.sh ./launch_android.sh
   # sed -i "s/win10-ltsc/android/" launch_android.sh
   # scp <cel_apl_gptimage.img from your host> ./android.img
   # sh launch_android.sh

#. You will see the shell console from the terminal and the Celadon GUI on the secondary monitor
   after system boots. You can check the build info using the ``getprop`` command in shell console:

   .. code-block:: bash

      console:/ $
      console:/ $ getprop | grep finger
      [ro.bootimage.build.fingerprint]: [cel_apl/cel_apl/cel_apl:9/PPR2.181005.003.A1/rui06241613:userdebug/test-keys]
      [ro.build.fingerprint]: [cel_apl/cel_apl/cel_apl:9/PPR2.181005.003.A1/rui06241613:userdebug/test-keys]
      [ro.vendor.build.fingerprint]: [cel_apl/cel_apl/cel_apl:9/PPR2.181005.003.A1/rui06241613:userdebug/test-keys]

   .. figure:: images/Celadon_home.png
      :width: 700px
      :align: center

   .. figure:: images/Celadon_apps.png
      :width: 700px
      :align: center
