.. _debian_packaging:

ACRN Installation via Debian Packages
#####################################

Debian packages provide a simple way to build and package a collection
of ACRN configurations on a development system, based on a set of
hardware platforms and scenario choices.  You can then copy the packages
onto your target platform, select a particular configuration and reboot
the system with ACRN and an Ubuntu Service VM up and running.

Follow these instructions to build Debian packages for the acrn-hypervisor and
acrn-kernel, install them on your target system, and boot running ACRN.

.. rst-class:: numbered-step

Set up Pre-requisites
*********************

Your development system should be running Ubuntu
18.04 and be connected to the internet. (You'll be installing software
with ``apt`` and from Pypi and downloading ACRN software from
GitHub.)

It's likely you've already got python3 on your system, but we can make
sure by installing it with::

   sudo apt install python3

All of the Debian packaging work is handled by a script found in the
ACRN GitHub repo.  If you don't have it downloaded already, clone the
ACRN repo with::

   git clone https://github.com/projectacrn/acrn-hypervisor.git

All the configuration files and scripts to build the Debian packages are
in the ``misc/packaging`` folder, so let's go there::

   cd acrn-hypervisor/misc/packaging


.. rst-class:: numbered-step

Configure Debian packaging details
**********************************

The build and packaging script ``install_uSoS.py`` does all the work to
build and make the Debian packages.  You configure what the script does
by editing its configuration file, ``release.json``.  Comments in the
JSON file document how you can adjust things to your specific needs.  For
example, the default ``release.json`` file builds for all three default
ACRN configurations (industry, hybrid, and logical_partition) and for
two supported boards (nuc717dnb and whl-ipc-i5).

.. important:: In the current implementation of this packaging script, if you
   change the ``release.json`` boards configuration choices, you'll also need to
   manually update the ``acrn-hypervisor.postinst`` script to match.

Here's the default ``release.json`` configuration:

.. literalinclude:: release.json


.. rst-class:: numbered-step

Run the package-building script
*******************************

The ``install_uSoS.py`` python script does all the work to install
needed tools (such as make, gnu-efi, libssl-dev, libpciaccess-dev,
uuid-dev, and more).  It also verifies that tool versions (such as the
gcc compiler) are appropriate (as configured in the ``release.json``
file).

The script runs without further user input, and must be run with
``sudo``::

   sudo python3 install_uSoS.py

With the default ``release.json`` configuration, this script will run
for about 30 minutes, but could take longer depending on your internet
speed (for downloading files) and overall computer performance (for
compiling the Linux kernel, ACRN hypervisor, and ACRN tools).

When done, it creates two Debian packages:

  * ``acrn_deb_package.deb`` with the ACRN hypervisor and tools built
    for each configuration combination, and
  * ``acrn_kernel_deb_package.deb`` with the ACRN-patched Linux kernel.

You'll need to copy these two files onto your target system, either via
the network or simply by using a thumbdrive.


.. rst-class:: numbered-step

Prepare your target system with Ubuntu 18.04
********************************************

Your target system must be one of the choices listed in the ``release.json``
file and should be running Ubuntu 18.04.  Make sure it's updated using the
commands::

   sudo apt update
   sudo apt upgrade

Reboot your system to complete the installation.

.. rst-class:: numbered-step

Install Debian packages on your target system
*********************************************

Copy the Debian packages you created on your development system, for
example, using a thumbdrive.  Then install the ACRN Debian package::

   sudo dpkg -i acrn_deb_package.deb

Make your choices for scenario, board, and which disk to use,
for example, this selects the industry scenario, nuc7i7dnb board, and
installation on the NVMe drive (input is highlighted):

.. code-block:: console
   :emphasize-lines: 11, 16, 23

   Selecting previously unselected package acrn-package.
   (Reading database ... 163871 files and directories currently installed.)
   Preparing to unpack acrn_deb_package.deb...
   Unpacking acrn-package (2020-07-17) ...
   Setting up acrn-package (2020-07-17) ...
   please choose <scenario> ,<board> ,<disk type>
   Scenario is ->
           1. industry
           2. hybrid
           3. logical_partition
   1
   Scenario is industry
   Board is ->
           1. nuc7i7dnb
           2. whl-ipc-i5
   1
   Board is nuc7i7dnb
   Your acrn bin is ->
   /boot/acrn.industry.nuc7i7dnb.bin
   disk type is ->
           1. nvme
           2. sda
   1
   disk type is nvme
   Sourcing file '/etc/default/grub'
   Generating grub configuration file ...
   Found linux image: /boot/vmlinuz-5.3.0-62-generic
   Found initrd image: /boot/initrd.img-5.3.0-62-generic
   Found linux image: /boot/vmlinuz-5.3.0-28-generic
   Found initrd image: /boot/initrd.img-5.3.0-28-generic
   Added boot menu entry for EFI firmware configuration
   done

   


Then install the ACRN-patched kernel package::

   sudo dpkg -i acrn_kernel_deb_package.deb

After that, you're ready to reboot.

.. rst-class:: numbered-step

Boot ACRN using the multiboot2 grub choice
******************************************

This time when you boot your target system you'll see some new options:

.. code-block:: console
   :emphasize-lines: 4

    Ubuntu
    Advanced options for Ubuntu
    System setup
   *ACRN multiboot2
    ACRN efi

If your target system has a serial port active, you can simply hit
:kbd:`return` (or wait for the timeout) to boot with this
``ACRN multiboot2`` choice.

.. important:: If you don't have an active serial port, you'll need to
   edit the grub configuration a bit by pressing :kbd:`e` (for edit) and
   remove the ``i915.modeset=0 video=efifb:off`` parameters at the end of
   the multiboot2 grub command:

   .. code-block:: console
      :emphasize-lines: 8

      setparams 'ACRN multiboot2 '

          load_video
          insmod gzio
          insmod part_gpt
          insmod ext2
          search --no-floppy --fs-uuid  --set $uuid
      multiboot2 /boot/acrn.industry.nuc7i7dnb.bin root=PARTUUID="06d5265d-863a-4bc0-b469-66b5e6b8a9cb" i915.modeset=0 video=efifb:off
      module2 /boot/vmlinuz-5.4.43.PKT-200203T060100Z Linux_bzImage

   Then press :kbd:`F10` to continue booting

.. rst-class:: numbered-step

Verify ACRN is running
**********************

After the system boots, you can verify ACRN was detected and is running
by looking at the dmesg log:

.. code-block:: console
   :emphasize-lines: 3

   acrn@acrn-NUC:~$ dmesg | grep -i acrn
   [    0.000000] Linux version 5.4.43-PKT-200203T060100Z (root@acrn-NUC7i7DNHE) (gcc version 7.5.0 (Ubuntu 7.5.0-3ubuntu1~18.04)) #1 SMP PREEMPT Thu Jul 16 16:33:58 CST 2020
   [    0.000000] Hypervisor detected: ACRN
   [    2.413796] ACRNTrace: Initialized acrn trace module with 4 cpu
   [    2.413883] ACRN HVLog: Failed to init last hvlog devs, errno -19
   [    2.413884] ACRN HVLog: Initialized hvlog module with 4 cpu


.. comment

   4.python3 compile_iasl.py
   =========================
   this scriptrs is help compile iasl and cp to /usr/sbin
