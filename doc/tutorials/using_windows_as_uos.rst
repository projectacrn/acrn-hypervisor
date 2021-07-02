.. _using_windows_as_uos:


Launch Windows as the Guest VM on ACRN
######################################

This tutorial describes how to launch Windows as a Guest (WaaG) VM on the
ACRN hypervisor.


ACRN Service VM Setup
*********************

Follow the steps in this :ref:`gsg` to set up ACRN
based on Ubuntu and launch the Service VM.

Setup for Using Windows as the Guest VM
***************************************

In the following steps, you'll first create a Windows image
in the Service VM, and then launch that image as a Guest VM.


Verified Version
================

* Windows 10 Version:

  - Microsoft Windows 10 Enterprise LTSC Evaluation

* Windows graphics driver:

  - igfx_win10_100.9030.zip

   .. note::

      WHL needs the following BIOS setting:
      set **DVMT Pre-Allocated** to **64MB** and set **PM Support**
      to **Enabled**.

Create a Windows 10 Image in the Service VM
===========================================

Create a Windows 10 image to install Windows 10 onto a virtual disk.

Download Win10 Image and Drivers
--------------------------------

#. Download `MediaCreationTool20H2.exe <https://www.microsoft.com/software-download/windows10>`_.

   - Run this file and select **Create installation media(USB flash drive, DVD, or ISO file) for another PC**;
     Then click **ISO file** to create ``Windows10.iso``.

#. Download the `Oracle Windows driver <https://edelivery.oracle.com/osdc/faces/SoftwareDelivery>`_.

   - Sign in. If you do not have an Oracle account, register for one.
   - Select **Download Package**. Key in **Oracle Linux 7.6** and click
     **Search**.
   - Click **DLP: Oracle Linux 7.6** to add to your Cart.
   - Click **Checkout**, which is located at the top-right corner.
   - Under **Platforms/Language**, select **x86 64-bit**. Click **Continue**.
   - Check **I accept the terms in the license agreement**. Click **Continue**.
   - From the list, right check the item labeled **Oracle VirtIO Drivers
     Version for Microsoft Windows 1.1.x, yy MB**, and then **Save link as
     ...**.  Currently, it is named ``V982789-01.zip``.
   - Click **Download**. When the download is complete, unzip the file. You
     will see an ISO named ``winvirtio.iso``.

Create a Raw Disk
-----------------

Run these commands on the Service VM::

   $ sudo apt-get install qemu-utils
   $ mkdir /home/acrn/work
   $ cd /home/acrn/work
   $ qemu-img create -f raw win10-ltsc.img 30G

Prepare the Script to Create an Image
-------------------------------------

#. Refer :ref:`gpu-passthrough` to enable GVT-d GOP feature; then copy above .iso files and  the built OVMF.fd to /home/acrn/work
#. Prepare WaaG install script, a **/home/acrn/work/install_win.sh** file should be created with the following content.

.. code-block:: bash

   #!/bin/bash
   function launch_win()
   {
   vm_name=win_vm$1
   #check if the vm is running or not
   vm_ps=$(pgrep -a -f acrn-dm)
   result=$(echo $vm_ps | grep "${vm_name}")
   if [[ "$result" != "" ]]; then
     echo "$vm_name is running, can't create twice!"
     exit
   fi
   echo "8086 9ded" > /sys/bus/pci/drivers/pci-stub/new_id
   echo "0000:00:14.0" > /sys/bus/pci/devices/0000:00:14.0/driver/unbind
   echo "0000:00:14.0" > /sys/bus/pci/drivers/pci-stub/bind
   echo "8086 3ea0" > /sys/bus/pci/drivers/pci-stub/new_id
   echo "0000:00:02.0" > /sys/bus/pci/devices/0000:00:02.0/driver/unbind
   echo "0000:00:02.0" > /sys/bus/pci/drivers/pci-stub/bind
   #for memsize setting
   mem_size=4096M
   acrn-dm -A -m $mem_size -s 0:0,hostbridge -s 1:0,lpc -l com1,stdio \
     -s 2,passthru,0/2/0,gpu \
     -s 8,virtio-net,tap0 \
     -s 4,virtio-blk,/home/acrn/work/win10-ltsc.img
     -s 5,ahci,cd:/home/acrn/work/Windows10.iso \
     -s 6,ahci,cd:/home/acrn/work/winvirtio.iso \
     -s 7,passthru,0/14/0,d3hot_reset \
     --ovmf /home/acrn/work/OVMF.fd \
     --windows \
     $vm_name
   }
   # offline SOS CPUs except BSP before launch UOS
   for i in `ls -d /sys/devices/system/cpu/cpu[1-99]`; do
           online=`cat $i/online`
           idx=`echo $i | tr -cd "[1-99]"`
           echo cpu$idx online=$online
           if [ "$online" = "1" ]; then
                   echo 0 > $i/online
                   # during boot time, cpu hotplug may be disabled by pci_device_probe during a pci module insmod
                  while [ "$online" = "1" ]; do
                           sleep 1
                           echo 0 > $i/online
                           online=`cat $i/online`
                   done
                   echo $idx > /sys/devices/virtual/misc/acrn_hsm/remove_cpu
           fi
   done
   launch_win 1

Install Windows 10 by GVT-d
---------------------------

#. Run **install_win.sh**

   .. code-block:: bash

      cd /home/acrn/work/
      sudo chmod +x install_win.sh
      sudo ./install_win.sh

When you see the UEFI shell, input **exit**.

#. Select **Boot Manager** and boot from Win10 ISO.

#. When the display reads **Press any key to boot from CD or DVD** on the
   monitor, press any key in the terminal on the **Host** side.

   .. figure:: images/windows_install_1.png
      :align: center

   .. figure:: images/windows_install_2.png
      :align: center

   .. figure:: images/windows_install_3.png
      :align: center

#. Click **Load driver**.

   .. figure:: images/windows_install_4.png
      :align: center

#. Click **Browser** and go to the drive that includes the virtio
   Windows drivers. Select **all** under **vio\\w10\\amd64**. Install the
   following drivers into the image:

   - Virtio-balloon
   - Virtio-net
   - Virtio-rng
   - Virtio-scsi
   - Virtio-serial
   - Virtio-block
   - Virtio-input

   .. note:: Be sure to unselect **Hide Drivers that aren't compatible with
      this computer's hardware** near the bottom of the page.

   .. figure:: images/windows_install_5.png
      :align: center

#. Click **Next**.

   .. figure:: images/windows_install_6.png
      :align: center

#. Continue with the installation.

   .. figure:: images/windows_install_7.png
      :align: center

#. Verify that the system restarts.

   .. figure:: images/windows_install_8.png
      :align: center

#. Configure your system when Windows completes its restart cycle.

   .. figure:: images/windows_install_9.png
      :align: center

#. Verify that the Windows desktop displays after the Windows installation is complete.

   .. figure:: images/windows_install_10.png
      :align: center

#. Download the `Intel DCH Graphics Driver
   <https://downloadcenter.intel.com/download/30066?v=t>`__ in
   Windows and install in safe mode.
   Version 27.20.100.9030 was verified on WHL. You should use the same version as the one in native Windows 10 on your board.

Boot Windows on ACRN With a Default Configuration
=================================================

#. Prepare WaaG launch script::

      cp /home/acrn/work/install_win.sh  /home/acrn/work/launch_win.sh

   Remove following lines in launch_win.sh

   .. code-block:: bash

      -s 5,ahci,cd:/home/acrn/work/Windows10.iso \
      -s 6,ahci,cd:/home/acrn/work/winvirtio.iso \

#. Launch WaaG

   .. code-block:: bash

      cd /home/acrn/work/
      sudo ./launch_win.sh

The WaaG desktop displays on the monitor.

ACRN Windows Verified Feature List
**********************************

.. csv-table::
   :header: "Items", "Details", "Status"

    "IO Devices", "Virtio block as the boot device", "Working"
                , "AHCI CD-ROM",                     "Working"
                , "Virtio network",                  "Working"
                , "Virtio input - mouse",            "Working"
                , "Virtio input - keyboard",         "Working"
    "GVT-d",      "GVT-d with local display",        "Working"
    "Tools",      "WinDbg",                          "Working"
    "Test cases", "Install Windows 10 from scratch", "OK"
                , "Windows reboot",                  "OK"
                , "Windows shutdown",                "OK"
    "Built-in Apps", "Microsoft Edge",               "OK"
                   , "Maps",                         "OK"
                   , "Microsoft Store",              "OK"
                   , "3D Viewer",                    "OK"

Explanation for acrn-dm Popular Command Lines
*********************************************

.. note:: Use these acrn-dm command line entries according to your
   real requirements.

* ``-s 2,passthru,0/2/0,gpu``:
  This is GVT-d to passthrough the VGA controller to Windows.
  You may need to change 0/2/0 to match the bdf of the VGA controller on your platform.

* ``-s 8,virtio-net,tap0``:
  This is for the network virtualization.

* ``-s 3,virtio-input,/dev/input/event4``:
  This is to passthrough the mouse/keyboard to Windows via virtio.
  Change ``event4`` accordingly. Use the following command to check
  the event node on your Service VM::

   <To get the input event of mouse>
   # cat /proc/bus/input/devices | grep mouse

* ``-s 5,ahci,cd:/home/acrn/work/Windows10.iso``:
  This is the IOS image used to install Windows 10. It appears as a CD-ROM
  device. Make sure that it points to your win10 ISO path.

* ``-s 6,ahci,cd:/home/acrn/work/winvirtio.iso``:
  This is CD-ROM device to install the virtio Windows driver. Make sure it points to your VirtIO ISO path.

* ``-s 7,passthru,0/14/0,d3hot_reset``:
  This is to passthrough the USB controller to Windows;d3hot_reset is needed for WaaG reboot when USB controller is passthroughed to Windows.
  You may need to change ``0/14/0`` to match the BDF of the USB controller on
  your platform.

* ``--ovmf /home/acrn/work/OVMF.fd``:
  Make sure it points to your OVMF binary path.

Secure Boot Enabling
********************
Refer to the steps in :ref:`How-to-enable-secure-boot-for-windows` for
secure boot enabling.

Activate Windows 10
********************
If you use a trial version of Windows 10, you might find that some
apps and features do not work or that Windows 10 automatically gets shut
down by the Windows licensing monitoring service. To avoid these issues,
obtain a licensed version of Windows.

For Windows 10 activation steps, refer to
`Activate Windows 10 <https://support.microsoft.com/en-us/help/12440/windows-10-activate>`__.
