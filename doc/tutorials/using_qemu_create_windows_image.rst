.. _using_qemu_create_windows_image:


Launch Windows as the Guest VM on ACRN
######################################

This tutorial describes how to create WaaG image with Qemu and launch it as a Guest (WaaG) VM on the
ACRN hypervisor.


ACRN Service VM Setup
*********************

Follow the steps in this :ref:`rt_industry_ubuntu_setup` to set up ACRN
based on Ubuntu as Service VM.

Setup for Using Windows as the Guest VM
***************************************

In the following steps, you'll first create a Windows image using **QEMU**
in native ubuntu first,then boot into Service VM and launch this Windows image as a Guest VM to install virtio and GFX driver.


Verified version
================

* Windows 10 Version:

  - Microsoft Windows 10 Enterprise LTSC Evaluation

* Windows graphics driver:

  - igfx_win10_100.9030.zip

.. note::
   WHL needs following setting in BOIS:
   set **DVMT Pre-Allocated** to **64MB** and set **PM Support**
   to **Enabled**.

Create a Windows 10 image in native ubuntu
==========================================

Before you start this tutorial, make sure the KVM tools are installed on your board and monitor is connectted.

.. code-block:: none

   $ sudo apt install qemu-kvm libvirt-clients libvirt-daemon-system bridge-utils virt-manager ovmf

.. rst-class:: numbered-step

Build the Windows KVM Image
***************************

The next steps will detail how to use the Windows (ISO) image to install Windows 10 onto a virtual disk.

#. Download `MediaCreationTool20H2.exe <https://www.microsoft.com/software-download/windows10>`_.

   - Run this file and select **Create installation media(USB flash drive,DVD, or ISO file) for another PC**;
     Then click **ISO file** to create ``windows10.iso``.

#. Install the Windows ISO via the virt-manager tool:

   .. code-block:: none

      $ sudo virt-manager

#. Verify that you can see the main menu as shown following.

   .. figure:: images/WaaG_image_1.png

#. Right-click **QEMU/KVM** and select **New**.

   Choose **Local install media (ISO image or CD-ROM)** and then click **Forward**.

#. Choose **Use ISO image** and click **Browse** - **Browse Local**.
   Select the ISO image you get from Step 1 above.

#. Choose the **OS type:** Windows, **Version:** Microsoft Windows 10 and then click **Forward**.

   **Create a new virtual machine** box displays, as shown following.

   .. figure:: images/WaaG_image_2.png

#. Choose **Create a disk image for virtual machine**. Set the
   storage to 20 GB or more if necessary and click **Forward**.

#. You must check the **customize configuration before install** option before you finish all stages.

   .. figure:: images/WaaG_image_3.png

#. Verify that you can see the **Overview** screen has been set up as shown following:

   .. figure:: images/WaaG_image_4.png

#. After Windows installation is compeletd; please disable the ``firewall`` and set NIC as following:

   .. figure:: images/WaaG_image_5.png

   .. Note:: Poweroff and restart are needed for the setting taking effect.

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
     will see ``Setup.exe``. Run it to install virtio driver.

   .. Note:: You can also scp the virtio driver if you already downloaded it in other machine.

#. Once virtio driver installation is completion, poweroff the windows. The KVM image is created in the
   ``/var/lib/libvirt/images`` folder.Convert the `gcow2` format to `img`
   **as the root user**:

   .. code-block:: none

      $ cd /home/acrn/
      $ qemu-img convert -f qcow2 -O raw /var/lib/libvirt/images/debian10.qcow2 win10.img

#. Prepare WaaG launch script, a **/home/acrn/work/launch_win.sh** file should be created with the following content.

.. code-block:: none

   #!/bin/bash
   # board: WHL-IPC-I5, scenario: INDUSTRY, uos: WINDOWS
   # pci devices for passthru
   declare -A passthru_vpid
   declare -A passthru_bdf

   passthru_vpid=(
   ["audio"]="8086 9dc8"
   ["gpu"]="8086 3ea0"
   )
   passthru_bdf=(
   ["audio"]="0000:00:1f.3"
   ["gpu"]="0000:00:02.0"
   )

   function tap_net() {
   # create a unique tap device for each VM
   tap=$1
   tap_exist=$(ip a | grep "$tap" | awk '{print $1}')
   if [ "$tap_exist"x != "x" ]; then
     echo "tap device existed, reuse $tap"
   else
     ip tuntap add dev $tap mode tap
   fi

   # if acrn-br0 exists, add VM's unique tap device under it
   br_exist=$(ip a | grep acrn-br0 | awk '{print $1}')
   if [ "$br_exist"x != "x" -a "$tap_exist"x = "x" ]; then
     echo "acrn-br0 bridge aleady exists, adding new tap device to it..."
     ip link set "$tap" master acrn-br0
     ip link set dev "$tap" down
     ip link set dev "$tap" up
   fi
   }

   function launch_windows()
   {
   #vm-name used to generate uos-mac address
   mac=$(cat /sys/class/net/e*/address)
   vm_name=post_vm_id$1
   mac_seed=${mac:0:17}-${vm_name}

   tap_net tap_WaaG
   #check if the vm is running or not
   vm_ps=$(pgrep -a -f acrn-dm)
   result=$(echo $vm_ps | grep -w "${vm_name}")
   if [[ "$result" != "" ]]; then
     echo "$vm_name is running, can't create twice!"
     exit
   fi

   echo ${passthru_vpid["gpu"]} > /sys/bus/pci/drivers/pci-stub/new_id
   echo ${passthru_bdf["gpu"]} > /sys/bus/pci/devices/${passthru_bdf["gpu"]}/driver/unbind
   echo ${passthru_bdf["gpu"]} > /sys/bus/pci/drivers/pci-stub/bind
   modprobe pci_stub
   kernel_version=$(uname -r)
   audio_module="/usr/lib/modules/$kernel_version/kernel/sound/soc/intel/boards/snd-soc-sst_bxt_sos_tdf8532.ko"

   # use the modprobe to force loading snd-soc-skl/sst_bxt_bdf8532
   if [ ! -e $audio_module ]; then
   modprobe -q snd-soc-skl
   modprobe -q snd-soc-sst_bxt_tdf8532
   else

   modprobe -q snd_soc_skl
   modprobe -q snd_soc_tdf8532
   modprobe -q snd_soc_sst_bxt_sos_tdf8532
   modprobe -q snd_soc_skl_virtio_be
   fi
   audio_passthrough=0

   # Check the device file of /dev/vbs_k_audio to determine the audio mode
   if [ ! -e "/dev/vbs_k_audio" ]; then
   audio_passthrough=1
   fi
   boot_audio_option=""
   if [ $audio_passthrough == 1 ]; then
       # for audio device
       echo ${passthru_vpid["audio"]} > /sys/bus/pci/drivers/pci-stub/new_id
       echo ${passthru_bdf["audio"]} > /sys/bus/pci/devices/${passthru_bdf["audio"]}/driver/unbind
       echo ${passthru_bdf["audio"]} > /sys/bus/pci/drivers/pci-stub/bind

       boot_audio_option="-s 0:31:0,passthru,00/1f/3"
   else
       boot_audio_option="-s 0:31:0,virtio-audio"
   fi
   mem_size=4096M
   #interrupt storm monitor for pass-through devices, params order:
   #threshold/s,probe-period(s),intr-inject-delay-time(ms),delay-duration(ms)
   intr_storm_monitor="--intr_monitor 10000,10,1,100"

   #logger_setting, format: logger_name,level; like following
   logger_setting="--logger_setting console,level=4;kmsg,level=3;disk,level=5"

   acrn-dm -A -m $mem_size -s 0:0,hostbridge -U d2795438-25d6-11e8-864e-cb7a18b34643 \
      --windows \
      $logger_setting \
      -s 5,virtio-blk,/work/acrn/win10.img \
      -s 6,virtio-net,tap_WaaG \
      -s 2,passthru,0/2/0,gpu  \
      --ovmf /usr/share/acrn/bios/OVMF.fd \
      $intr_storm_monitor \
      -s 1:0,lpc \
      -l com1,stdio \
      $boot_audio_option \
      --mac_seed $mac_seed \
      $vm_name
   }
   launch_windows 1


Launch Windows 10 by GVT-d
**************************

#. Boot into Ubuntu Service VM and run **launch_win.sh**

   .. code-block:: none

      cd /home/acrn/work/
      sudo chmod +x launch_win.sh
      sudo ./launch_win.sh

#. Run following command in Ubuntu Service VM and you will get the IP of WaaG:

   .. code-block:: none

      sudo apt install arp-scan
      arp-scan -l | grep 00:16:3e

   .. Note:: 00:16:3e is the first part of User VM's MAC address it is hardcoded ad the second part of User VM's MAC address
      is coming from the acrn-dm parameter ``mac_seed`` .

#. Remote Desktop Connection with the scanned WaaG IP
   In another Windows OS, input ``Remote Desktop Connection`` in the search bar; then input the scanned IP and User name.

#. Download the `Intel DCH Graphics Driver
   <https://downloadcenter.intel.com/download/30066?v=t>`__.in
   Windows and install it as you do in native.
   The latest version(27.20.100.9030) was verified on WHL.You’d better use the same version as the one in native Windows 10 on your board.

#. Root WaaG after the grapics driver install sucessfully;The WaaG desktop displays on the monitor.

ACRN Windows verified feature list
**********************************

.. csv-table::
   :header: "Items", "Details", "Status"

    "IO Devices", "Virtio block as the boot device", "Working"
                , "AHCI as the boot device",         "Working"
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

Explanation for acrn-dm popular command lines
*********************************************

.. note:: Use these acrn-dm command line entries according to your
   real requirements.

* ``-s 2,passthru,0/2/0,gpu``:
  This is GVT-d to passthrough the VGA controller to Windows.
  You may need to change 0/2/0 to match the bdf of the VGA controller on your platform.

* ``-s 4,virtio-net,tap0``:
  This is for the network virtualization.

* ``-s 5,fbuf,tcp=0.0.0.0:5900,w=800,h=600``:
  This opens port 5900 on the Service VM, which can be connected to via
  ``vncviewer``.

* ``-s 6,virtio-input,/dev/input/event4``:
  This is to passthrough the mouse/keyboard to Windows via virtio.
  Change ``event4`` accordingly. Use the following command to check
  the event node on your Service VM::

   <To get the input event of mouse>
   # cat /proc/bus/input/devices | grep mouse

* ``-s 7,ahci,cd:/home/acrn/work/Windows10.iso``:
  This is the IOS image used to install Windows 10. It appears as a CD-ROM
  device. Make sure that the slot ID **7** points to your win10 ISO path.

* ``-s 8,ahci,cd:/home/acrn/work/winvirtio.iso``:
  This is CD-ROM device to install the virtio Windows driver. Make sure it points to your VirtIO ISO path.

* ``-s 9,passthru,0/14/0``:
  This is to passthrough the USB controller to Windows.
  You may need to change ``0/14/0`` to match the BDF of the USB controller on
  your platform.

* ``--ovmf /home/acrn/work/OVMF.fd``:
  Make sure it points to your OVMF binary path.

Secure boot enabling
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
