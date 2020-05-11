.. _gpu-passthrough:

Enable GVT-d in ACRN
#########################################

This tutorial describes how to enable GVT-d in ACRN.

      .. note:: It is recommended to have either a serial port
         or SSH session open in the Service VM to be able to 
	 continue interact with it after enabling GVT-d.

Introduction
*****************
Intel GVT-d is one flavor of graphics virtualization approaches also
names as Intel-Graphics-Device pass-through feature.
It based on Intel VT-d technology,
and some special graphics related configuration.
This flavor allows direct assignment of an entire GPU’s prowess to a single user,
passing the native driver capabilities through the hypervisor without any limitations.

Verified version
*****************

- ACRN-hypervisor tag: **acrn-2020w17.4-140000p**
- ACRN-Kernel (Service VM kernel): **master** branch, commit id **095509221660daf82584ebdd8c50ea0078da3c2d**
- ACRN-EDK2 (OVMF): **ovmf-acrn** branch, commit id **0ff86f6b9a3500e4c7ea0c1064e77d98e9745947**

Prerequisites
*****************
Follow :ref:`these instructions <kbl-nuc-sdc>` to set up the ACRN Service VM.

Hardware Platform Supported
***************************
Currently, ACRN has enabled GVT-d on the following platforms:

* Kaby Lake
* Whiskey Lake
* Elkhart Lake

BIOS Setting
**********************

Kaby Lake Platform
==================
* set "IGD Minimum Memory" to "64MB" in "Devices → Video → IGD Minimum Memory".

Whiskey Lake Platform
=====================
* set "PM Support"  to "Enabled" in "chipset → System Agent (SA) Configuration → Graphics Configuration → PM support".
* set "DVMT Pre-Allocated" to "64MB" in "chipset → System Agent (SA) Configuration → Graphics Configuration → DVMT Pre-Allocated"

Elkhart Lake Platform
=====================
* set "DMVT Pre-Allocated" to "64MB" in "Intel Advanced Menu → System Agent(SA) Configuration → Graphics Configuration → DMVT Pre-Allocated"

Pass-through GPU to Guest
**************************

1. Copy ``/usr/share/acrn/samples/nuc/launch_win.sh`` to ``install_win.sh``

::

   cp /usr/share/acrn/samples/nuc/launch_win.sh ~/install_win.sh

2. Modify ``install_win.sh`` script to specify the Windows image you use.

3. Modify ``install_win.sh`` script to enable GVT-d.

Add the following commands before ``acrn-dm -A -m $mem_size -s 0:0,hostbridge \``

::

      gpudevice=`cat /sys/bus/pci/devices/0000:00:02.0/device`

      echo "8086 $gpudevice" > /sys/bus/pci/drivers/pci-stub/new_id
      echo "0000:00:02.0" > /sys/bus/pci/devices/0000:00:02.0/driver/unbind
      echo "0000:00:02.0" > /sys/bus/pci/drivers/pci-stub/bind

Replace ``-s 2,pci-gvt -G "$2" \`` with ``-s 2,passthru,0/2/0,gpu \``

4. Run the ``launch_win.sh``.

      .. note:: If you want to pass-through GPU to Clear Linux User VM, the steps are
         the same as above except your script.

Enable GVT-d GOP driver
***********************

Issue
======
When enabling GVT-d, guest OS couldn’t light up physical screen
before OS driver load. So guest BIOS and
grub GUI couldn’t be showed on the physical screen.

Reason
==========
Physical display is initialized by GOP driver or VBIOS
before OS driver load, but guest BIOS doesn’t have them.

Solution
==========
Integrate GOP driver binary into OVMF as a Dxe driver,
then guest OVMF could see GOP driver and run it in
graphic pass-through environment.
So physical display could be initialized
by GOP and used by guest BIOS and guest grub.

Steps
======
1. fetch ACRN OVMF

::

      git clone https://github.com/projectacrn/acrn-edk2.git

2. fetch vbt and gop driver

Fetch vbt and gop driver from the board manufacturer according to your CPU model name.

3. add vbt and gop driver into OVMF

::

      cp IntelGopDriver.efi  acrn-edk2/OvmfPkg/IntelGop/IntelGopDriver.efi
      cp Vbt.bin acrn-edk2/OvmfPkg/Vbt/Vbt.bin

Notes:

    - Please confirm these binaries names with board manufacturer. 

4. git apply the following two patches

* `Use-the-default-vbt-released-with-GOP-driver.patch
  <../_static/downloads/Use-the-default-vbt-released-with-GOP-driver.patch>`_
* `Integrate-IntelGopDriver-into-OVMF.patch
  <../_static/downloads/Integrate-IntelGopDriver-into-OVMF.patch>`_

5. compile OVMF

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

Notes:

   -  You need a build machine that has a GCC 5.X version installed.

   -  ``source edksetup.sh``, this step is needed for compilation every time
      a shell is created.

   -  This will generate the binary at
      ``Build/OvmfX64/DEBUG_GCC5/FV/OVMF.fd``, transfer the binary to
      your target machine.

   -  Modify the launch script to specify the OVMF you built just now.