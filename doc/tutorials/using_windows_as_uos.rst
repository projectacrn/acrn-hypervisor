.. _using_windows_as_uos:


Launch Windows as the Guest VM on ACRN
######################################

This tutorial describes how to launch Windows as a Guest (WaaG) VM on the
ACRN hypervisor.

Verified version
****************

* Clear Linux version: 33050
* ACRN-hypervisor tag: v1.6.1 (acrn-2020w18.4-140000p)
* ACRN-Kernel (Service VM kernel): 4.19.120-108.iot-lts2018-sos
* Windows 10 Version:

  - Microsoft Windows 10 Enterprise, 10.0.17134 Build 17134
  - Microsoft Windows 10 Enterprise LTSC Evaluation, 10.0.17763 Build 17763

* Windows graphics driver:

  - igfx_win10_100.7212.zip

Hardware setup
**************

The following Intel Kaby Lake NUCs are verified:

.. csv-table::
   :header: "Platform Model", "BIOS Version", "BIOS Download Link"

   "NUC7i7DNHE", "DNKBLi7v.86A.0052.2018.0808.1344", "`link <https://downloadcenter.intel.com/download/28886?v=t>`__"
   "NUC7i5DNHE", "DNKBLi5v.86A.0060.2018.1220.1536", "`link <https://downloadcenter.intel.com/download/28885?v=t>`__"

ACRN Service VM Setup
*********************

If necessary, refer to the steps in :ref:`kbl-nuc-sdc` to set up ACRN on the
KBL NUC. Once complete, you should be able to launch the Service VM
successfully.

Setup for Using Windows as the Guest VM
***************************************

All patches to support WaaG have been upstreamed; download them from the
``acrn-hypervisor`` repository.

Build ACRN EFI Images
=====================

#. Follow the steps described at :ref:`getting-started-building` to set up the build environment.
#. Use the ``make`` command to compile ``acrn.efi`` and ``acrn-dm``::

   $ git clone https://github.com/projectacrn/acrn-hypervisor.git
   $ cd acrn-hypervisor
   $ make FIRMWARE=uefi BOARD=kbl-nuc-i7

#. Get outputs from the following::

   $ build/hypervisor/acrn.efi
   $ build/devicemodel/acrn-dm

#. Replace ``acrn.efi`` and ``acrn-dm`` on your NUC:

   a. Log in to the ACRN Service VM and then ``mount`` the EFI partition to ``/boot``
   #. ``scp`` both ``acrn.efi`` and ``acrn-dm`` from your host::

      # scp <acrn.efi from your host> /boot/EFI/acrn/
      # scp <acrn-dm from your host> /usr/bin/
      # chmod +x /usr/bin/acrn-dm && sync

Build the Service VM kernel
===========================

#. Follow the steps described at :ref:`getting-started-building` to set up
   the build environment.
#. Follow the steps below to build the ACRN kernel::

   $ WORKDIR=`pwd`;
   $ JOBS=`nproc`
   $ git clone -b master https://github.com/projectacrn/acrn-kernel.git
   $ cd acrn-kernel && mkdir -p ${WORKDIR}/{build,build-rootfs}
   $ cp kernel_config_uefi_sos ${WORKDIR}/build/.config
   $ make olddefconfig O=${WORKDIR}/build && make -j${JOBS} O=${WORKDIR}/build
   $ make modules_install INSTALL_MOD_PATH=${WORKDIR}/build-rootfs O=${WORKDIR}/build -j${JOBS}

Update the KBL NUC kernel
=========================

#. Copy the new kernel image (bzImage) and its modules to the target machine::

   # scp <your host>:$WORKDIR/build/arch/x86/boot/bzImage /boot/bzImage
   # scp -r <your host>:$WORKDIR/build-rootfs/lib/modules/* /lib/modules/
   # cp /boot/loader/entries/acrn.conf  /boot/loader/entries/acrngt.conf

#. Modify ``acrngt.conf`` to the content as shown below:

   .. code-block:: none

      title The ACRNGT Service VM
      linux /bzImage
      options console=tty0 console=ttyS0 root=/dev/sda3 rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.nuclear_pageflip=1 i915.avail_planes_per_pipe=0x010101 i915.domain_plane_owners=0x011100001111 i915.enable_gvt=1 i915.enable_conformance_check=0 i915.enable_guc=0 hvlog=2M@0x1FE00000

   .. note:: Change ``/dev/sda3`` to your file system partition.

#. Reboot the Service VM and select ``The ACRNGT Service VM`` from the
   boot menu to apply the ACRN kernel and hypervisor updates.

Create a Windows 10 image in the Service VM
===========================================

Create a Windows 10 image to install Windows 10 onto a virtual disk.

Download Win10 ISO and drivers
------------------------------

#. Download `Windows 10 LTSC ISO <https://www.microsoft.com/en-us/evalcenter/evaluate-windows-10-enterprise>`_.

   - Select **ISO-LTSC** and click **Continue**.
   - Complete the required info. Click **Continue**.
   - Select the language and **x86 64 bit**. Click **Download ISO** and save as ``windows10-LTSC-17763.iso``.

#. Download the `Intel DCH Graphics Driver <https://downloadmirror.intel.com/29074/a08/igfx_win10_100.7212.zip>`_.

#. Download the `Oracle Windows driver <https://edelivery.oracle.com/osdc/faces/SoftwareDelivery>`_.

   - Sign in. If you do not have an Oracle account, register for one.
   - Select **Download Package**. Key in **Oracle Linux 7.6** and click
     **Search**.
   - Click **DLP: Oracle Linux 7.6** to add to your Cart.
   - Click **Checkout** which is located at the top-right corner.
   - Under **Platforms/Language**, select **x86 64 bit**. Click **Continue**.
   - Check **I accept the terms in the license agreement**. Click **Continue**.
   - From the list, right check the item labeled **Oracle VirtIO Drivers
     Version for Microsoft Windows 1.x.x, yy MB**, and then **Save link as
     ...**.  Currently, it is named **V982789-01.zip**.
   - Click **Download**. When the download is complete, unzip the file. You
     will see an ISO named **winvirtio.iso**.

Create a raw disk
-----------------

Run these commands on the Service VM::

   # swupd bundle-add kvm-host
   # mkdir /root/img
   # cd /root/img
   # qemu-img create -f raw win10-ltsc.img 30G

Prepare the script to create an image
-------------------------------------

#. Copy ``/usr/share/acrn/samples/nuc/launch_win.sh`` to ``install_win.sh``::

   # cp /usr/share/acrn/samples/nuc/launch_win.sh ~/install_win.sh


#. Edit the ``acrn-dm`` command line in ``install_win.sh`` as follows:

   .. note:: Make sure you use GVT-g ``-s 2,pci-gvt -G "$2"`` in the
      ``acrn-dm`` command line. Currently, we do not support creating a
      windows image by GVT-d.

   - Change ``-s 3,virtio-blk,./win10-ltsc.img`` to your path to the Windows
     10 image.

   - Add ``-s 6,xhci,1-5:1-9``. You may need to change 1-5:1-9 to match the
     ports of the USB keyboard/mouse and flash on your platform.

   - Add ``-s 8,ahci,cd:./windows10-LTSC-17763.iso`` to point to the Win10
     ISO.

   - Add ``-s 9,ahci,cd:./winvirtio.iso`` to point to your path to the
     winvirtio iso.

Install Windows 10 by GVT-g
---------------------------

.. note:: Make sure you have configured your monitor and display according
   to **3** of :ref:`Boot Windows with GVT-g on ACRN <waag_display_conf_lable>`.

#. Run ``install_win.sh``. When you see the UEFI shell, input **exit**.

#. Select **Boot Manager** and boot up from Win10 ISO.

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

#. Click **Browser** and go to the drive that includes the virtio win
   drivers. Select **all** under **vio\\w10\\amd64**. Install the
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

#. Copy the `Intel DCH Graphics Driver <https://downloadmirror.intel.com/29074/a08/igfx_win10_100.7212.zip>`_ into
   Windows and install in safe mode. The display driver is updated to 7212.

.. _waag_display_conf_lable:

Boot Windows on ACRN with a default configuration
=================================================

#. Modify the ``/usr/share/acrn/samples/nuc/launch_win.sh`` script to
   specify the Windows image that is generated above.

#. Run ``launch_win.sh``. The WaaG desktop displays on the HDMI monitor.

   .. note::
      We support GVT-g and GVT-d while launching Windows guest. If you use GVT-g, you can set up Weston in the Service VM, and set up
      Weston as the desktop environment in the Service VM in order to
      experience Windows with the AcrnGT local display feature. If you use
      GVT-d, set **DVMT Pre-Allocated** to **64MB** and set **PM Support**
      to **Enabled** in the BIOS. Then, only Windows displays.

ACRN Windows verified feature list
**********************************

.. csv-table::
   :header: "Items", "Details", "Status"

    "IO Devices", "Virtio block as the boot device", "Working"
                , "AHCI as the boot device",         "Working"
                , "AHCI cdrom",                      "Working"
                , "Virtio network",                  "Working"
                , "Virtio input - mouse",            "Working"
                , "Virtio input - keyboard",         "Working"
                , "GVT-g GOP & VNC remote display",  "Working"
    "GVT-g",      "GVT-g without local display",     "Working with 3D benchmark"
           ,      "GVT-g with local display",        "Working with 3D benchmark"
    "GVT-d",      "GVT-d with local display",        "Working"
    "Tools",      "WinDbg",                          "Working"
    "Test cases", "Install Windows 10 from scratch", "OK"
                , "Windows reboot",                  "OK"
                , "Windows shutdown",                "OK"
    "Built-in Apps", "Microsoft Edge",               "OK"
                   , "Maps",                         "OK"
                   , "Microsoft Store",              "OK"
                   , "3D Viewer",                    "OK"

Known limitations
*****************

* The cursor is not visible with the GVT-g local display.

Explanation for acrn-dm popular command lines
*********************************************

.. note:: Use these acrn-dm command line entries according to your
   real requirements.

* **-s 2,passthru,0/2/0,gpu**:
  This is GVT-d to passthrough the VGA controller to Windows.
  You may need to change 0/2/0 to match the bdf of the VGA controller on your platform.

* **-s 3,ahci,hd:/root/img/win10.img**:
  This is the hard disk onto which to install Windows 10.
  Make sure that the slot ID **3** points to your win10 img path.

* **-s 4,virtio-net,tap0**:
  This is for the network virtualization.

* **-s 5,fbuf,tcp=0.0.0.0:5900,w=800,h=600**:
  This opens port 5900 on the Service VM which can be connected to via vncviewer.

* **-s 6,virtio-input,/dev/input/event4**:
  This is to passthrough the mouse/keyboard to Windows via virtio.
  Change ``event4`` accordingly. Use the following command to check
  the event node on your Service VM::

   <To get the input event of mouse>
   # cat /proc/bus/input/devices | grep mouse

* **-s 7,ahci,cd:/root/img/Windows10.iso**:
  This is the IOS image used to install Windows 10. It appears as a cdrom
  device. Make sure that the slot ID **7** points to your win10 ISO path.

* **-s 8,ahci,cd:/root/img/winvirtio.iso**:
  This is cdrom device to install the virtio Windows driver. Make sure it points to your VirtIO ISO path.

* **-s 9,passthru,0/14/0**:
  This is to passthrough the USB controller to Windows.
  You may need to change 0/14/0 to match the bdf of the USB controller on
  your platform.

* **--ovmf /usr/share/acrn/bios/OVMF.fd**:
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

.. comment Reviewed for grammatical content on 20 May 2020.