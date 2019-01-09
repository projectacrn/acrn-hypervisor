.. _skl-nuc-gpu-passthrough:

GPU Passthrough on Skylake NUC
##############################

.. warning::
   This community reference release for the Skylake NUC with GPU
   passthrough is a one-time snapshot release and is not supported
   or maintained.

Hardware platform
*****************

* `Skylake Skull Canyon NUC NUC6i7KYK
  <https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc6i7kyk.html>`_

Software Configuration
**********************

* `acrn-hypervisor tag acrn-2018w39.6-140000p
  <https://github.com/projectacrn/acrn-hypervisor/releases/tag/acrn-2018w39.6-140000p>`_
* `acrn-kernel tag acrn-2018w39.6-140000p
  <https://github.com/projectacrn/acrn-kernel/releases/tag/acrn-2018w39.6-140000p>`_
* Clear Linux:       version: 25130 (UOS and SOS use this version)

Source code patches are provided in `skl-patches-for-acrn.tar file
<../_static/downloads/skl-patches-for-acrn.tar>`_ to work around or add support for
enabling GPU passthrough:

* 0001-hv-workaround-for-system-hang-on-non-apicv-devices.patch
* 0002-hv-More-changes-to-enable-GPU-passthru.patch
* 0003-dm-increase-interrupt-storm-threshold-for-gpu-passth.patch
* 0004-dm-passthrough-opregion-to-uos-gpu.patch
* 0005-dm-modify-launch-script-to-support-gpu-passthrough.patch

Software Setup
**************

Please follow the :ref:`getting-started-apl-nuc`, with the following changes:

1. Set up a Clear Linux Operating System

   Clear Linux will update to the latest version during installation.
   Run this command (as root) to roll back to version 25130, using the
   ``–x`` switch to ignore version mismatch::

      # swupd verify -x --fix --picky -m 25130
      # swupd autoupdate -–disable
      # reboot

#. Add the ACRN hypervisor to the EFI Partition

   Refer to :ref:`getting-started-building`
   to build the  hypervisor, device model, and tools.

   Download and untar this `skl-patches-for-acrn.tar file
   <../_static/downloads/skl-patches-for-acrn.tar>`_, apply these patches to the
   acrn-hypervisor, and build it::

      $ git clone https://github.com/projectacrn/acrn-hypervisor
      $ cd acrn-hypervisor
      $ git checkout acrn-2018w39.6-140000p
      $ curl https://projectacrn.github.io/latest/_static/downloads/skl-patches-for-acrn.tar | tar x
      $ git am *.patch
      $ make

   This build process creates new ``acrn-dm``, ``acrn.efi`` and
   ``launch_uos.sh`` files.

#. Replace ``acrn-dm`` with this new version (as root)::

      # cp build/devicemodel/acrn-dm  /usr/bin/acrn-dm

#. Put the new ``acrn.efi`` hypervisor application (included in the
   Clear Linux release) on the EFI partition (as root)::

      # mount /dev/nvme0n1p1 /mnt
      # mkdir /mnt/EFI/acrn
      # cp build/hypervisor/acrn.efi /mnt/EFI/acrn/

#. Configure the EFI firmware to boot the ACRN hypervisor by default.
   This assumes you are on an NVMe SSD as in the Skull Canyon::

      # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/nvme0n1 -p 1 -L "ACRN"

#. Create a boot entry for ACRN Service OS by making a few edits to the
   ``acrn.conf`` file (note the options line must be one long line, without
   any line breaks)::

      # vim /mnt/loader/entries/acrn.conf
      title The ACRN Service OS
      linux   /EFI/org.clearlinux/kernel-org.clearlinux.pk414-sos.4.14.68-99
      options pci_devices_ignore=(0:18:1) console=tty0 console=ttyS2 i915.nuclear_pageflip=1 root=/dev/nvme0n1p3 rw rootwait ignore_loglevel no_timer_check consoleblank=0 i915.tsd_init=7 i915.tsd_delay=2000 i915.avail_planes_per_pipe=0x01010F i915.domain_plane_owners=0x011111110000 i915.enable_guc_loading=0 i915.enable_guc_submission=0 i915.enable_preemption=1 i915.context_priority_mode=2 i915.enable_gvt=1 i915.enable_initial_modeset=0 i915.enable_guc=0 hvlog=2M@0x1FE00000

#. Don't Enable weston service (skip this step found in the NUC's getting
   started guide).

#. Set up Reference UOS by running the modified ``launch_uos.sh`` in
   ``acrn-hypervisor/devicemodel/samples/nuc/launch_uos.sh``

#. After UOS is launched, do these steps to run GFX workloads:

   a) install weston and glmark2::

         #swupd bundle-add desktop glmark2
   #) Add new user cl_uos::

         # useradd cl_uos
         # passwd cl_uos
         # usermod -G wheel -a cl_uos
   #) Enable weston service::

         # systemctl enable weston@cl_uos
         # systemctl start weston@cl_uos
   #) Disable weston screen saver::

         # vim .config/weston.ini
         [core]
         idle-time=0
   #) run glmark2::

         # glmark2-es2-wayland
