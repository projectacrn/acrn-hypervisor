.. _gpu-passthrough:

Enable GVT-d in ACRN
####################

This tutorial describes how to enable GVT-d in ACRN.

.. note:: After GVT-d is enabled, have either a serial port
   or SSH session open in the Service VM to interact with it.

Introduction
************

Intel GVT-d is a graphics virtualization approach that is also known as
the Intel-Graphics-Device passthrough feature. Based on Intel VT-d
technology, it offers useful special graphics-related configurations.
It allows for direct assignment of an entire GPU's prowess to a single
user, passing the native driver capabilities through to the hypervisor
without any limitations.

Verified Version
*****************

- ACRN-hypervisor tag: **acrn-2020w17.4-140000p**
- ACRN-Kernel (Service VM kernel): **master** branch, commit ID **095509221660daf82584ebdd8c50ea0078da3c2d**
- ACRN-EDK2 (OVMF): **ovmf-acrn** branch, commit ID **0ff86f6b9a3500e4c7ea0c1064e77d98e9745947**

Prerequisites
*************

Follow :ref:`these instructions <gsg>` to set up
Ubuntu as the ACRN Service VM.

Supported Hardware Platform
***************************

Currently, ACRN has enabled GVT-d on the following platforms:

* Kaby Lake
* Whiskey Lake
* Elkhart Lake

BIOS Settings
*************

Kaby Lake Platform
==================

* Set **IGD Minimum Memory** to **64MB** in **Devices** |rarr|
  **Video** |rarr| **IGD Minimum Memory**.

Whiskey Lake Platform
=====================

* Set **PM Support**  to **Enabled** in **Chipset** |rarr| **System
  Agent (SA) Configuration** |rarr| **Graphics Configuration** |rarr|
  **PM support**.
* Set **DVMT Pre-Allocated** to **64MB** in **Chipset** |rarr|
  **System Agent (SA) Configuration**
  |rarr| **Graphics Configuration** |rarr| **DVMT Pre-Allocated**.

Elkhart Lake Platform
=====================

* Set **DMVT Pre-Allocated** to **64MB** in **Intel Advanced Menu**
  |rarr| **System Agent(SA) Configuration** |rarr|
  **Graphics Configuration** |rarr| **DMVT Pre-Allocated**.

Passthrough the GPU to Guest
****************************

1. Copy ``/usr/share/acrn/samples/nuc/launch_win.sh`` to ``install_win.sh``

   ::

     cp /usr/share/acrn/samples/nuc/launch_win.sh ~/install_win.sh

2. Modify the ``install_win.sh`` script to specify the Windows image you use.

3. Modify the ``install_win.sh`` script to enable GVT-d:

   Add the following commands before ``acrn-dm -A -m $mem_size -s 0:0,hostbridge \``

   ::

     gpudevice=`cat /sys/bus/pci/devices/0000:00:02.0/device`

     echo "8086 $gpudevice" > /sys/bus/pci/drivers/pci-stub/new_id
     echo "0000:00:02.0" > /sys/bus/pci/devices/0000:00:02.0/driver/unbind
     echo "0000:00:02.0" > /sys/bus/pci/drivers/pci-stub/bind


4. Run ``launch_win.sh``.

Enable the GVT-d GOP Driver
***************************

When enabling GVT-d, the Guest OS cannot light up the physical screen
before the OS driver loads. As a result, the Guest BIOS and the Grub UI
are not visible on the physical screen. This occurs because the physical
display is initialized by the GOP driver or VBIOS before the OS driver
loads, and the Guest BIOS doesn't have them.

The solution is to integrate the GOP driver binary into the OVMF as a DXE
driver. Then the Guest OVMF can see the GOP driver and run it in the graphic
passthrough environment. The physical display can be initialized
by the GOP and used by the Guest BIOS and Guest Grub.

Steps
=====

1. Fetch the ACRN OVMF:

   ::

     git clone https://github.com/projectacrn/acrn-edk2.git

#. Fetch the VBT and GOP drivers.

   Fetch the **VBT** and **GOP** drivers from the board manufacturer
   according to your CPU model name.

#. Add the **VBT** and **GOP** drivers to the OVMF:

   ::

     cp IntelGopDriver.efi  acrn-edk2/OvmfPkg/IntelGop/IntelGopDriver.efi
     cp Vbt.bin acrn-edk2/OvmfPkg/Vbt/Vbt.bin

   Confirm that these binaries names match the board manufacturer names.

#. Use ``git apply`` to add the following two patches:

   * `Use-the-default-vbt-released-with-GOP-driver.patch <../_static/downloads/Use-the-default-vbt-released-with-GOP-driver.patch>`_

   * `Integrate-IntelGopDriver-into-OVMF.patch <../_static/downloads/Integrate-IntelGopDriver-into-OVMF.patch>`_

#. Compile the OVMF:

   ::

     cd acrn-edk2
     git submodule update --init CryptoPkg/Library/OpensslLib/openssl

     source edksetup.sh
     make -C BaseTools

     vim Conf/target.txt

       ACTIVE_PLATFORM = OvmfPkg/OvmfPkgX64.dsc
       TARGET_ARCH = X64
       TOOL_CHAIN_TAG = GCC5

     build -DFD_SIZE_2MB -DDEBUG_ON_SERIAL_PORT=TRUE

Keep in mind the following:

   -  Use a build machine that has GCC 5.X version installed.

   -  The ``source edksetup.sh`` step is needed for compilation every time
      a shell is created.

   -  This will generate the binary at
      ``Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd``. Transfer the binary to
      your target machine.
   -  Modify the launch script to specify the OVMF you built just now.

Script
======

Once you've installed the Docker environment, you can use this
`script <../_static/downloads/build_acrn_ovmf.sh>`_ to build ACRN OVMF
with the GOP driver enabled. For more details about the script usage,
run ``build_acrn_ovmf.sh -h``.
