.. _using_windows_as_uos:

Using Windows as Guest VM on ACRN
#################################
This tutorial describes how to launch Windows as a Guest (WaaG) VM on the ACRN hypervisor.

Hardware setup
**************
The following Intel Kaby Lake NUCs are verified:

.. csv-table::
   :header: "Platform Model", "BIOS Version", "BIOS Download Link"

   "NUC7i7DNHE", "DNKBLi7v.86A.0052.2018.0808.1344", "`link <https://downloadcenter.intel.com/download/28886?v=t>`__"
   "NUC7i5DNHE", "DNKBLi5v.86A.0060.2018.1220.1536", "`link <https://downloadcenter.intel.com/download/28885?v=t>`__"

ACRN Service VM Setup
*********************
You may refer to the steps in :ref:`kbl-nuc-sdc` for the
Intel NUC in order to set up ACRN on the KBL NUC. After following the steps in that guide,
you should be able to launch the Service VM successfully.

Setup for Using Windows as Guest VM
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

   a. Log in to the ACRN Service VM and then ``mount`` the EFI partition to ``/boot``
   #. ``scp`` the ``acrn.efi`` and ``acrn-dm`` from your host::

      # scp <acrn.efi from your host> /boot/EFI/acrn/
      # scp <acrn-dm from your host> /usr/bin/
      # chmod +x /usr/bin/acrn-dm && sync

Build Service VM Kernel
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

      title The ACRNGT Service VM
      linux /bzImage
      options console=tty0 console=ttyS0 root=/dev/sda3 rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.nuclear_pageflip=1 i915.avail_planes_per_pipe=0x010101 i915.domain_plane_owners=0x011100001111 i915.enable_gvt=1 i915.enable_conformance_check=0 i915.enable_guc=0 hvlog=2M@0x1FE00000

   .. note:: Change ``/dev/sda3`` to your file system partition.

#. ``reboot`` the Service VM and select ``The ACRNGT Service VM`` from the boot menu to apply
   the ACRN kernel and hypervisor updates.

Create Windows 10 Image
=======================
Create a Windows 10 image which includes two steps:

#. Re-generate an ISO that includes winvirtio or virtio-win drivers and the Windows graphics drivers that were pre-installed
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
  to the Service VM in ``/root/img/virtio-win-0.1.141.iso``.

* Download `Intel DCH Graphics Driver <https://downloadmirror.intel.com/29074/a08/igfx_win10_100.7212.zip>`_.

* Download Oracle Windows driver to Service VM in ``/root/img/winvirtio.iso``.
*  `Oracle Windows driver <https://edelivery.oracle.com/osdc/faces/SoftwareDelivery>`_.Sign in. If you do not have an oracle account, register one.
*  Select "Download Package", key in "Oracle Linux 7.6" and click "Search"
*  Click: DLP: Oracle Linux 7.6, it will be added to your Cart.
*  Click "Checkout" at the top right corner
*  In the "Platforms/Language", select "x86 64 bit", click "Continue"
*  Check " I accept the terms in the license agreement", click "Continue"
*  In the list, right check the item labeled as "Oracle VirtIO Drivers Version for Microsoft Windows 1.x.x, yy MB", and "Save link as ...".  At the time of this wiki, it is named as "V982789-01.zip"
*  Click Download, When the download is complete, unzip, you will get one ISO named "winvirtio.iso"

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
   for example, drive ``D:``
   Or used Oracle Driver
   Right click the downloaded ``winvirtio.iso`` and select ``Mount``. The ISO will be mounted to a drive;
   for example, drive ``D:``

#. Use ``7-zip`` or similar utility to unzip the downloaded Windows graphics driver
   ``igfx_win10_100.7212.zip`` to a folder,
   for example, to ``C:\Dev\Temp\wim\igfx_win10_100.7212``

#. Right click the downloaded Windows ISO, for example, ``windows10-17763-107-LTSC.iso``, select ``Mount``,
   the ISO will be mounted to a drive; for example, drive ``E:``

#. Copy ``E:\sources\boot.wim`` and ``E:\sources\install.wim`` to ``C:\WIM``

#. Depending on your Windows ISO image, more than one image may be included in the ``WIM``.
   Run ``dism /get-wiminfo /wimfile:C:\WIM\install.wim`` with administrator privileges.
   Select the ``Index`` you want. For ``windows10-17763-107-LTSC.iso``,
   there is only one ``Index``; it is ``1``

   .. figure:: images/install_wim_index.png
      :align: center

#. Create a batch file named ``virtio-inject-boot.bat`` [1]_ to modify
   ``boot.wim`` to inject drivers (using the mounted Windows ISO drive
   (``D:``), image Index (``1``), and folder where the unzipped Windows
   graphics drivers were placed, from the previous steps (update this
   batch file as needed)::

      REM virt-inject-boot
      Set IDX=1

      REM Modify boot.wim file to inject drivers
      dism /Mount-Wim /WimFile:C:\Wim\boot.wim /Index:%IDX% /MountDir:C:\mount
      dism /image:C:\mount /Add-Driver "/driver:d:\balloon\w10\amd64\balloon.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\NetKVM\w10\amd64\netkvm.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\viorng\w10\amd64\viorng.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioscsi\w10\amd64\vioscsi.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioserial\w10\amd64\vioser.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\viostor\w10\amd64\viostor.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioinput\w10\amd64\vioinput.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\cui_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\HdBusExt.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\iigd_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\DisplayAudio\11.1\IntcDAud.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\msdk.inf"
      dism /unmount-wim /mountdir:c:\mount /commit


      REM inject-Oracle-driver-install
      Set IDX=1

      REM Modify boot.wim file to inject drivers
      dism /Mount-Wim /WimFile:C:\WIM\boot.wim /Index:%IDX% /MountDir:C:\mount
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\netkvmorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioinput.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\viorng.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioscsiorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioserorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\viostororcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\cui_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\HdBusExt.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\iigd_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\DisplayAudio\11.1\IntcDAud.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\msdk.inf"
      dism /unmount-wim /mountdir:c:\mount /commit


   Run this ``virtio-inject-boot.bat`` script in a command prompt
   running as administrator.  It may take 4-5 minutes to run, depending on
   your Windows system performance.

#. Similarly, create another batch file named
   ``virtio-inject-install.bat`` [1]_ to modify ``install.wim`` to inject
   drivers (and verify the ISO drive, image Index, and drivers folder)::

      REM virt-inject-install
      Set IDX=1

      REM Modify install.wim to inject drivers
      dism /Mount-Wim /WimFile:C:\WIM\install.wim /Index:%IDX% /MountDir:C:\mount
      dism /image:C:\mount /Add-Driver "/driver:d:\balloon\w10\amd64\balloon.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\NetKVM\w10\amd64\netkvm.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\viorng\w10\amd64\viorng.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioscsi\w10\amd64\vioscsi.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioserial\w10\amd64\vioser.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\viostor\w10\amd64\viostor.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:d:\vioinput\w10\amd64\vioinput.inf" /forceunsigned
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\cui_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\HdBusExt.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\iigd_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\DisplayAudio\11.1\IntcDAud.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\msdk.inf"
      dism /unmount-wim /mountdir:c:\mount /commit


      REM inject-Oracle-driver-install
      Set IDX=1

      REM Modify install.wim to inject drivers
      dism /Mount-Wim /WimFile:C:\WIM\install.wim /Index:%IDX% /MountDir:C:\mount
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\netkvmorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioinput.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\viorng.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioscsiorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\vioserorcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:d:\vio\Win10\amd64\viostororcl.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\cui_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\HdBusExt.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\iigd_dch.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\DisplayAudio\11.1\IntcDAud.inf"
      dism /image:C:\mount /Add-Driver "/driver:c:\Dev\Temp\wim\igfx_win10_100.7212\Graphics\msdk.inf"
      dism /unmount-wim /mountdir:c:\mount /commit

   Run this script in a command prompt running as administrator.  It may also
   take 4-5 minutes to run, depending on your Windows system performance.


#. After running these two scripts the files ``C:\WIM\boot.wim`` and ``C:\WIM\install.wim``
   will be updated to install these drivers into the image:

   - Virtio-balloon
   - Virtio-net
   - Virtio-rng
   - Virtio-scsi
   - Virtio-serial
   - Virtio-block
   - Virtio-input
   - Windows graphics drivers

#. Use 7-zip to unzip the downloaded Windows ISO to a folder; for example, into
   ``C:\Dev\Temp\wim\windows10-17763-107-LTSC``

#. Delete ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources\boot.wim`` and
   ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources\install.wim``

#. Copy ``C:\WIM\boot.wim`` and ``C:\WIM\install.wim`` to ``C:\Dev\Temp\wim\windows10-17763-107-LTSC\sources``

#. Download and unzip `cdrtools-3.01.a23-bootcd.ru-mkisofs.7z
   <http://reboot.pro/index.php?app=core&module=attach&section=attach&attach_id=15214>`_ to a folder;
   for example, to ``C:\Dev\Temp\wim\cdrtools-3.01.a23-bootcd.ru-mkisofs``

#. Create a batch file named ``mkisofs_both_legacy_and_uefi.bat``
   containing (update folder names as needed to reflect where the
   referenced files are located on your system, and ``inputdir``,
   ``outputiso`` and ``mkisofs.exe`` path, downloaded by the previous
   step)::

      set inputdir=C:\Dev\Temp\wim\windows10-17763-107-LTSC
      set outputiso=C:\Dev\Temp\wim\mkisofs_iso\windows10-17763-107-LTSC-Virtio-Gfx.iso
      set label="WIN10_17763_107_LTSC_VIRTIO_GFX"
      set biosboot=boot/etfsboot.com
      set efiboot=efi/microsoft/boot/efisys.bin
      C:\Dev\Temp\wim\cdrtools-3.01.a23-bootcd.ru-mkisofs\mingw\mkisofs.exe \
        -iso-level 4 -l -R -UDF -D -volid %label% -b %biosboot% -no-emul-boot \
        -boot-load-size 8 -hide boot.catalog -eltorito-alt-boot \
        -eltorito-platform efi -no-emul-boot -b %efiboot%  -o %outputiso% \
        %inputdir%

   Run this ``mkisofs_both_legacy_and_uefi.bat`` script. The resulting
   ISO will be generated in ``outputiso`` location you specified.

Create Raw Disk
---------------
Run these commands on the Service VM::

   # swupd bundle-add kvm-host
   # mkdir /root/img
   # cd /root/img
   # qemu-img create -f raw win10-ltsc-virtio.img 30G

Install Windows 10
------------------

   .. note:: Please make sure you have configured your monitor and display according to **3** of
      :ref:`Boot Windows with GVT-g on ACRN <waag_display_conf_lable>`.

#. Make a copy from ``/usr/share/acrn/samples/nuc/launch_win.sh`` to ``install_win.sh``::

   # cp /usr/share/acrn/samples/nuc/launch_win.sh ~/install_win.sh

#. Add the following lines **before** ``acrn-dm`` command line in ``install_win.sh``. It is used
   to passthrough USB to WaaG, by which both mouse and keyboard can be used during Windows installation::

      echo "8086 9d2f" > /sys/bus/pci/drivers/pci-stub/new_id
      echo "0000:00:14.0" > /sys/bus/pci/devices/0000:00:14.0/driver/unbind
      echo "0000:00:14.0" > /sys/bus/pci/drivers/pci-stub/bind

   .. note:: You may need to change the bdf and vid/pid of the USB controller in above command to match those
      of your platform. You can use ``lspci`` and ``lspci -n`` to get this information.

#. Edit the ``acrn-dm`` command line in ``install_win.sh`` as follows:

   - Change ``-s 3,virtio-blk,./win10-ltsc-virtio.img`` to your path to the Windows 10 image.
   - Add ``-s 6,passthru,0/14/0``.

    .. note:: You may need to change 0/14/0 to match bdf of the USB controller of your platform.

   - Add ``-s 8,ahci,cd:./windows10-17763-107-LTSC-Virtio-Gfx.iso`` to point to the ISO you re-generated above.
   - Add ``-s 9,ahci,cd:./virtio-win-0.1.141.iso`` to point to your path to the virtio-win iso.

   Or if you used the Oracle driver:

   - Add ``-s 9,ahci,cd:./winvirtio.iso`` to point to your path to the winvirtio iso.

#. Run ``install_win.sh``.

#. When it is showing "Press any key to boot from CD or DVD" on the monitor, press any key in the terminal on
   **Host** side.

   .. figure:: images/windows_install_1.png
      :align: center

   .. figure:: images/windows_install_2.png
      :align: center

   .. figure:: images/windows_install_3.png
      :align: center

#. Click **Load driver**.

   .. figure:: images/windows_install_4.png
      :align: center

#. Click :kbd:`Browser` and go to the drive which includes virtio win drivers. Select **viostor\\w10\\amd64\\viostor.inf**
   or **viostororcl.inf** if you are using Oracle virtio drivers and install virtio block driver.

   .. figure:: images/windows_install_5.png
      :align: center

#. Select the virtio block drive and click :kbd:`Next`.

   .. figure:: images/windows_install_6.png
      :align: center

#. Continue the installation.

   .. figure:: images/windows_install_7.png
      :align: center

#. System will restart.

   .. figure:: images/windows_install_8.png
      :align: center

#. Windows will restart several times and finally you are asked to configure your system.

   .. figure:: images/windows_install_9.png
      :align: center

#. Windows installation is complete after a few configuation steps, and you get to the Windows desktop.

   .. figure:: images/windows_install_10.png
      :align: center

.. _waag_display_conf_lable:

Boot Windows with GVT-g on ACRN
===============================
#. Modify the ``/usr/share/acrn/samples/nuc/launch_win.sh`` script to specify the Windows image generated above.

#. Run the ``launch_win.sh`` and you should see the WaaG desktop coming up over the HDMI monitor (instead of the VNC).

   .. note:: Use the following command to disable the GNOME Display Manager (GDM) if it is enabled::

      # sudo systemctl mask gdm.service

   .. note:: You must connect two monitors to the KBL NUC in order to launch Windows with
      the default configurations above.

   .. note:: The second monitor must include the Weston desktop. If you have set up Weston in the Service VM,
      follow the steps in :ref:`skl-nuc-gpu-passthrough` to set up Weston as
      the desktop environment in Service VM in order to experience Windows with the AcrnGT local display feature.


ACRN Windows verified feature list
**********************************
* Windows 10 Version:

  - Microsoft Windows 10 Enterprise, 10.0.17134 Build 17134
  - Microsoft Windows 10 Pro, 10.0.17763 Build 17763

* Windows graphics driver:

  - igfx_win10_100.7212.zip


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
* The Windows graphic driver version must be ``igfx_win10_100.7212.zip``;
  the latest version ``1910.1007372.zip`` cannot be installed correctly.

Device configurations of acrn-dm command line
*********************************************
* *-s 3,ahci,hd:/root/img/win10.img*:
  This is the hard disk onto which to install Windows 10.
  Make sure that the slot ID 3 points to your win10 img path.

* *-s 4,virtio-net,tap0*:
  This is for the network virtualization.

* *-s 5,fbuf,tcp=0.0.0.0:5900,w=800,h=600*:
  This will open a port 5900 on Service VM which can be connected to via vncviewer.

* *-s 6,virtio-input,/dev/input/event4*:
  This is to passthrough the mouse/keyboard to Windows via virtio.
  Please change ``event4`` accordingly. You can use the following command to check
  the event node on your Service VM::

   <To get the input event of mouse>
   # cat /proc/bus/input/devices | grep mouse

* *-s 7,ahci,cd:/root/img/Windows.iso*:
  This is the IOS image used to install Windows 10. It appears as a cdrom device.
  Make sure that the slot ID 7 points to your win10 ISO path.

* *-s 8,ahci,cd:/root/img/virtio-win-0.1.141.iso*: This is another cdrom device
  to install the virtio Windows driver later. Make sure it points to your VirtIO ISO path.

* *--ovmf /usr/share/acrn/bios/OVMF.fd*:
  Make sure it points to your OVMF binary path

Secure boot enabling
********************
You may refer to the steps in :ref:`How-to-enable-secure-boot-for-windows` for
secure boot enabling.

Activate Windows 10
********************
If you are using Windows 10 without activation (30 days free trial), you may encounter some problems
(e.g. Some apps and features can not work, Windows 10 automatically shutdown by Windows licensing monitoring service and so on) when
using Windows 10 that is unlicensed.

For Windows 10 activation steps, you may refer to "`windows 10 activate  <https://support.microsoft.com/en-us/help/12440/windows-10-activate>`__"

References
**********

.. [1]
   These virtio drivers injecting batch script are based on Derek Seaman's IT blog about
   `injecting VirtIO Drivers into Windows
   <https://www.derekseaman.com/2015/07/injecting-kvm-virtio-drivers-into-windows.html>`_.
