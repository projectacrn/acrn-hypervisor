.. _using_windows_as_uos:

Using Windows as Guest OS on ACRN
#################################
This tutorial describes how to launch Windows as the Guest OS on the ACRN hypervisor.

Hardware setup
**************
The following Intel Kaby Lake NUCs are verified:

.. csv-table::
   :header: "Platform Model", "Bios Version", "Download Link"

   "NUC7i7DNHE", "DNKBLi7v.86A.0052.2018.0808.1344", "`link <https://downloadcenter.intel.com/download/28886?v=t>`__"
   "NUC7i5DNHE", "DNKBLi5v.86A.0060.2018.1220.1536", "`link <https://downloadcenter.intel.com/download/28885?v=t>`__"

ACRN Service OS Setup
*********************
You may refer to the steps in :ref:`getting-started-apl-nuc` for
Intel NUC to set up ACRN on the KBL NUC. After following the steps in that guide,
you should be able to launch a Clear Linux UOS successfully.

Setup for Using Windows as Guest OS
***********************************
All the patches to support WaaG have been upstreamed; you can download them
from the acrn-hypervisor repository.

Build ACRN EFI Images
=====================
#. Follow the steps described at :ref:`getting-started-building` to set up the build environment.
#. Use the ``make`` command to compile the ``acrn.efi`` and ``acrn-dm``::

   $ git clone https://github.com/projectacrn/acrn-hypervisor.git
   $ cd acrn-hypervisor
   $ make FIRMWARE=uefi BOARD=kbl-nuc-i7

#. Get the outputs from::

   $ build/hypervisor/acrn.efi
   $ build/devicemodel/acrn-dm

#. Replace the ``acrn.efi`` and ``acrn-dm`` on your NUC:

   a. Log in to the ACRN Service OS and then ``mount`` the EFI partition to ``/boot``
   #. ``scp`` the ``acrn.efi`` and ``acrn-dm`` from your host::

      # scp <acrn.efi from your host> /boot/EFI/acrn/
      # scp <acrn-dm from your host> /usr/bin/
      # chmod +x /usr/bin/acrn-dm && sync

Build Service OS Kernel
=======================
#. Follow the steps described at :ref:`getting-started-building` to set up the build environment.
#. Follow the steps below to build the ACRN kernel::

   $ WORKDIR=`pwd`;
   $ JOBS=`nproc`
   $ git clone -b master https://github.com/projectacrn/acrn-kernel.git
   $ cd acrn-kernel && mkdir -p ${WORKDIR}/{build,build-rootfs}
   $ cp kernel_config_uefi_sos ${WORKDIR}/build/.config
   $ make olddefconfig O=${WORKDIR}/build && make -j${JOBS} O=${WORKDIR}/build
   $ make modules_install INSTALL_MOD_PATH=${WORKDIR}/build-rootfs O=${WORKDIR}/build -j${JOBS}

Update Kernel on KBL NUC
========================
#. Copy the new kernel image (bzImage) and its modules to the target machine::

   # scp <your host>:$WORKDIR/build/arch/x86/boot/bzImage /boot/bzImage
   # scp -r <your host>:$WORKDIR/build-rootfs/lib/modules/* /lib/modules/
   # cp /boot/loader/entries/acrn.conf  /boot/loader/entries/acrngt.conf

#. Modify ``acrngt.conf`` to the content as given below:

   .. code-block:: none

      title The ACRNGT Service OS
      linux /bzImage
      options console=tty0 console=ttyS0 root=/dev/sda3 rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.nuclear_pageflip=1 i915.avail_planes_per_pipe=0x010101 i915.domain_plane_owners=0x011100001111 i915.enable_gvt=1 i915.enable_conformance_check=0 i915.enable_guc=0 hvlog=2M@0x1FE00000

   .. note:: Change ``/dev/sda3`` to your file system partition.

#. ``reboot`` the Service OS and select ``The ACRNGT Service OS`` from the boot menu to apply
   the ACRN kernel and hypervisor updates.

Create Windows 10 Image
=======================
Create a Windows 10 image which includes two steps:

#. Re-generate an ISO that includes virtio-win drivers and the Windows graphics drivers that were pre-installed
   from the original Windows ISO.

#. Install Windows 10 onto the virtual disk.

Preparations
------------
* Download `Windows 10 ADK <https://docs.microsoft.com/en-us/windows-hardware/get-started/adk-install>`_
  according to your working Windows 10 version.

.. note:: :kbd:`Win` + :kbd:`R` to open the Run window. Key in ``winver`` to get your working Windows version.

* Download `Windows 10 LTSC ISO
  <https://software-download.microsoft.com/download/sg/17763.107.101029-1455.rs5_release_svc_refresh_CLIENT_LTSC_EVAL_x64FRE_en-us.iso>`_.

* Download `virtio Windows driver
  <https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/archive-virtio/virtio-win-0.1.141-1/virtio-win-0.1.141.iso>`_
  to the Service OS in ``/root/img/virtio-win-0.1.141.iso``.

* Download `Intel DCH Graphics Driver <https://downloadmirror.intel.com/28148/a08/dch_win64_25.20.100.6444.exe>`_.

Install Windows 10 ADK
----------------------
#. Double click ``adksetup.exe`` to start the installation.

   .. figure:: images/adk_install_1.png
      :align: center

#. Click ``Next``.

   .. figure:: images/adk_install_2.png
      :align: center

#. Select ``Deployment Tools`` and ``Windows Preinstallation Environment (Windows PE)``,
   and click ``Install`` to continue the installation.

   .. note:: You need to install Windows 10 ADK only once.

Pre-install drivers and re-generate Windows ISO
-----------------------------------------------
#. Create a folder on the ``C:`` drive called ``WIM``, so you have a folder ``C:\WIM``

#. Create a folder on the ``C:`` drive called ``Mount``, so you have a folder ``C:\Mount``

#. Right click the downloaded ``virtio-win-0.1.141.iso`` and select ``Mount``. The ISO will be mounted to a drive;
   in my case, it is ``D:``

#. Unzip the downloaded Windows graphics driver ``dch_win64_25.20.100.6444.exe`` to a folder,
   in my case, it is unzipped to ``C:\Dev\Temp\wim\dch_win64_25.20.100.6444``

   .. note:: We use ``7-zip`` to unzip this ``dch_win64_25.20.100.6444.exe`` driver.

#. Right click the downloaded Windows ISO, for example, ``windows10-17763-107-LTSC.iso``, select ``Mount``,
   the ISO will be mounted to a drive; in my case, it is ``E:``

#. Copy ``E:\sources\boot.wim and E:\sources\install.wim`` to ``C:\WIM``

#. Depending on your Windows ISO image,  varying amounts of images may be  included in the ``WIM``.
   Run ``dism /get-wiminfo /wimfile:C:\WIM\install.wim`` with administrator privileges.
   Select the ``Index`` you want. For ``windows10-17763-107-LTSC.iso``,
   there is only one ``Index``; it is ``1``

   .. figure:: images/install_wim_index.png
      :align: center

#. Download the `Virtio-Inject.bat
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/scripts/Virtio-Inject.bat>`_
   to a folder in your Windows PC.

   .. note:: If ``virtio-win-0.1.141.iso`` is mounted to a different drive than ``D:``, you need to change the
      ``driver:d`` in the script according to your case.

   .. note:: If you unzipped the Windows graphics driver to a different folder, you must change
      ``c:\Dev\Temp\wim\dch_win64_25.20.100.6444`` according to your case.

#. Open a command prompt as administrator and go to the folder in which you saved the above ``Virtio-Inject.bat``
   and run ``Virtio-Inject.bat``. Make sure no errors occur during the script execution.

   .. note:: The execution of the script will take several minutes depending on your working Windows performance.
      It will take about 8 minutes on a (KBL NUCi5 + 16G memory) Windows 10 machine.

   .. note:: You can split the script above into two parts; one injects drivers into ``boot.wim``,
      and the other injects drivers into ``install.wim``. Execute them one at a time if you return
      one of the following errors:

      - *0xc1420113*: The user attempted to mount to a directory that already contained a mounted image.
        This is not supported.

      - *0xc1420127*: The specified image in the specified wim is already mounted for read/write access.

#. ``C:\WIM\boot.wim`` and ``C:\WIM\install.wim`` will be updated after you have executed ``Virtio-Inject.bat``
   successfully. The following drivers have been pre-installed into the image:

   - Virtio-balloon
   - Virtio-net
   - Virtio-rng
   - Virtio-scsi
   - Virtio-serial
   - Virtio-block
   - Virtio-input
   - Windows graphics drivers

#. Use 7-zip to unzip the downloaded Windows ISO to a folder; in my case, it is unzipped to
   ``C:\Dev\Temp\wim\windows10-17763-107-LTSC``

#. Delete ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources\boot.wim`` and
   ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources\install.wim``

#. Copy ``C:\WIM\boot.wim`` and ``C:\WIM\install.wim`` to ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources``

#. Download and unzip `cdrtools-3.01.a23-bootcd.ru-mkisofs.7z
   <http://reboot.pro/index.php?app=core&module=attach&section=attach&attach_id=15214>`_ to a folder; in my case,
   it is unzipped to ``C:\Dev\Temp\wim\cdrtools-3.01.a23-bootcd.ru-mkisofs``

#. Download the `mkisofs_both_legacy_and_uefi.bat
   <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/scripts/mkisofs_both_legacy_and_uefi.bat>`_
   to a folder in your Windows PC.

   .. note:: Change these parameters to your case: ``inputdir``, ``outputiso``,
      ``mkisofs.exe path``

#. The ISO will be generated in ``outputiso`` to the location you specified in the script above.

Create Raw Disk
---------------
Run these commands on the Service OS::

   # swupd bundle-add kvm-host
   # mkdir /root/img
   # cd /root/img
   # qemu-img create -f raw win10-ltsc-virtio.img 30G

Install Windows 10
------------------
Currently, the ACRNGT OVMF GOP driver is not ready; thus, a special VGA version is used to install Windows 10
on ACRN from scratch. The ``acrn.elf``, ``acrn-dm`` and ``OVMF`` binaries are included in the
`tarball <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/tutorials/install_by_vga_gsg.tar.gz>`_
together with the script used to install Windows 10.

#. Uncompress ``install_by_vga_gsg.tar.gz`` to the Service OS::

   # tar zxvf install_by_vga_gsg.tar.gz && cd install_by_vga_gsg

#. Edit the ``acrn-dm`` command line in ``install_vga.sh`` if your configuration is different.

   - Change ``-s 3,virtio-blk,./win10-ltsc-virtio.img`` to your path to the Windows 10 image.
   - Change ``-s 8,ahci,cd:./windows10-17763-107-LTSC-Virtio-Gfx.iso`` to the ISO you re-generated above.
   - Change ``-s 9,ahci,cd:./virtio-win-0.1.141.iso`` to your path to the virtio-win iso.

#. Run ``install_vga.sh`` and connect to the Windows guest using a vnc client.::

   # vncviewer <IP-OF-HOST-MACHINE>:5900

#. Input ``exit`` followed by :kbd:`ENTER`

   .. figure:: images/windows_install_1.png
      :align: center

#. Select ``Boot Manager``

   .. figure:: images/windows_install_2.png
      :align: center

#. Select ``UEFI ACRN-DM SATA DVD ROM ACRN--F9B7-5503-A05B``, which is using the PCI slot 7.
   This is what we configured in the script for the Windows ISO cdrom.

   .. figure:: images/windows_install_3.png
      :align: center

#. Select :kbd:`ENTER` followed by any key press to be prompted to the Windows installation screen.

   .. figure:: images/windows_install_4.png
      :align: center

   .. figure:: images/windows_install_5.png
      :align: center

   .. figure:: images/windows_install_6.png
      :align: center

   .. figure:: images/windows_install_7.png
      :align: center

   .. figure:: images/windows_install_8.png
      :align: center

#. Connect again after Windows guest reboots. Use ``vncviewer <IP-OF-HOST-MACHINE>:5900``.

   .. figure:: images/windows_install_9.png
      :align: center

#. Connect again after Windows guest reboots a second time. Use ``vncviewer <IP-OF-HOST-MACHINE>:5900``.

   .. figure:: images/windows_install_10.png
      :align: center

#. Perform a few configuration steps. The Windows desktop appears.

   .. figure:: images/windows_install_11.png
      :align: center

   .. figure:: images/windows_install_12.png
      :align: center

Boot Windows with GVT-g on ACRN
===============================
#. Launch the Windows Guest using the ``launch_igx-waag.sh``. You should see the WaaG desktop
   coming up over the HDMI monitor (instead of the VNC).

   .. note:: Use the following command to disable the GNOME Display Manager (GDM) if it is enabled::

      # sudo systemctl mask gdm.service

   .. note:: You must connect two monitors to the KBL NUC in order to launch Windows with
      the default configurations above.

   .. note:: The second monitor must include the Weston desktop. If you have set up Weston in the Service OS,
      follow the steps in :ref:`skl-nuc-gpu-passthrough` to set up Weston as
      the desktop environment in SOS in order to experience Windows with the AcrnGT local display feature.

ACRN Windows verified feature list
**********************************
* Windows 10 Version:

  - Microsoft Windows 10 Enterprise, 10.0.17134 Build 17134
  - Microsoft Windows 10 Pro, 10.0.17763 Build 17763

* Windows graphics driver:

  - dch_win64_25.20.100.6444.exe


.. csv-table::
   :header: "Items", "Details", "Status"

    "IO Devices", "Virtio block as the boot device", "Working"
                , "AHCI as the boot device",         "Working"
                , "AHCI cdrom",                      "Working"
                , "Virtio network",                  "Working"
                , "Virtio input - mouse",            "Working"
                , "Virtio input - keyboard",         "Working"
                , "GOP & VNC remote display",        "Working"
    "GVT-g",      "GVT-g without local display",     "Working with 3D benchmark"
           ,      "GVT-g  with local display",       "Working with 3D benchmark"
    "Tools",      "WinDbg",                          "Working"
    "Test cases", "Install Windows 10 from scratch", "OK"
                , "Windows reboot",                  "OK"
                , "Windows shutdown",                "OK"
    "Built-in Apps", "Microsoft Edge",               "OK"
                   , "Maps",                         "OK"
                   , "Microsoft Store",              "OK"
                   , "3D Viewer",                    "OK"

Known Limitations
*****************
* The cursor is not visible with the GVG-g local display.
* The Windows graphic driver version must be ``dch_win64_25.20.100.6444.exe``;
  the latest version ``dch_win64_25.20.100.6577.exe`` cannot be installed correctly.

Device configurations of acrn-dm command line
*********************************************
* *-s 3,ahci,hd:/root/img/win10.img*:
  This is the hard disk onto which to install Windows 10.
  Make sure that the slot ID 3 points to your win10 img path.

* *-s 4,virtio-net,tap0*:
  This is for the network virtualization.

* *-s 5,fbuf,tcp=0.0.0.0:5900,w=800,h=600*:
  This will open a port 5900 on SOS which can be connected to via vncviewer.

* *-s 6,virtio-input,/dev/input/event4*:
  This is to passthrough the mouse/keyboard to Windows via virtio.
  Please change ``event4`` accordingly. You can use the following command to check the event node on your SOS::

   <To get the input event of mouse>
   # cat /proc/bus/input/devices | grep mouse

* *-s 7,ahci,cd:/root/img/Windows.iso*:
  This is the IOS image used to install Windows 10. It appears as a cdrom device.
  Make sure that the slot ID 7 points to your win10 ISO path.

* *-s 8,ahci,cd:/root/img/virtio-win-0.1.141.iso*: This is another cdrom device
  to install the virtio Windows driver later. Make sure it points to your VirtIO ISO path.

* *--ovmf /root/bios/OVMF.fd*:
  Make sure it points to your OVMF binary path.
