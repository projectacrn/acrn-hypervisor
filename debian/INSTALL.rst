Full Build and Installation Example
===================================

This document shows the entire build and installation procedure for ACRN
on a Debian/Ubuntu based distribution via apt package management only.

Prerequisites used in this example:

-  `Maxtang AXWL10 <http://www.maxtangpc.com/AXSeriesEmbeddedPCs/140.html>`__ Whiskey Lake based Industrial PC
-  `Ubuntu 22.04 LTS server (Jammy Jellyfish) <https://releases.ubuntu.com/22.04/ubuntu-22.04-live-server-amd64.iso>`__

Remark: This guide only serves as an example and may deviate in certain
steps depending on the chosen distribution. It has also been
successfully applied for:

-  `Intel NUC7i7DNHE <https://ark.intel.com/content/www/us/en/ark/products/130393/intel-nuc-kit-nuc7i7dnhe.html>`__ Kaby Lake based PC using `Debian 10 (Buster <https://cdimage.debian.org/cdimage/archive/10.12.0/amd64/iso-cd/debian-10.12.0-amd64-netinst.iso>`__
-  `Siemens Simatic IPC227G <https://mall.industry.siemens.com/mall/en/WW/Catalog/Products/10416195>`__ Elkhart Lake based Industrial PC using `Debian 11 (Bullseye) <https://cdimage.debian.org/debian-cd/current/amd64/iso-cd/debian-11.3.0-amd64-netinst.iso>`__
-  `TTTech MFN 100 <https://www.tttech-industrial.com/wp-content/uploads/TTTech-Industrial_MFN-100.pdf>`__ Apollo Lake based Industrial PC using `Ubuntu 20.04 LTS server (Focal Fossa) <https://releases.ubuntu.com/20.04/ubuntu-20.04.4-live-server-amd64.iso>`__

Table of Contents
-----------------

1. `Basic installation on target <#basic-installation-on-target>`__
2. `Build the ACRN kernel for the Service
   VM <#build-the-acrn-kernel-for-the-service-vm>`__
3. `Build & Deploy ACRN Debian
   Packages <#build--deploy-acrn-debian-packages>`__
4. `ACRN Package Reconfiguration <#acrn-package-reconfiguration>`__
5. `ACRN Removal <#acrn-removal>`__

Basic installation on target
----------------------------

-  Use the install ISO for basic installation of your distribution
-  optional for Ubuntu: remove cloud-init and snap, reconfigure netplan to get faster boot times (we’re rebooting often!)
-  optional for Ubuntu: enable grub menu in ``/etc/default/grub``:

   -  Comment out ``GRUB_TIMEOUT_STYLE=hidden`` ->
      ``#GRUB_TIMEOUT_STYLE=hidden``
   -  Set a reasonable timeout: ``GRUB_TIMEOUT=0`` -> ``GRUB_TIMEOUT=3``
   -  When finished update grub config:

      ::

        root@target:~# update-grub

-  optional: enable serial console for grub in ``/etc/default/grub``
   (can be used on otherwise headless systems):

   -  Add to or edit in ``/etc/default/grub``:

      ::

        GRUB_TERMINAL=“console serial”
        GRUB_SERIAL_COMMAND=“serial –unit=0 –speed=115200 –word=8 –parity=no –stop=1”

   -  When finished update grub config:

      ::

        root@target:~# update-grub

   -  Remark: Choose an appropriate legacy serial port: ``–unit=0 <->
      ttyS0``, see `GRUB Manual <https://www.gnu.org/software/grub/manual/grub/html_node/Serial-terminal.html>`__

   -  Verify by connecting a serial terminal to the target. On reboot
      the grub menu should be displayed:

      ::

                                      GNU GRUB  version 2.06

          +----------------------------------------------------------------------------+
          |*Ubuntu                                                                     |
          | Advanced options for Ubuntu                                                |
          | UEFI Firmware Settings                                                     |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          +----------------------------------------------------------------------------+

               Use the ^ and v keys to select which entry is highlighted.          
               Press enter to boot the selected OS, `e' to edit the commands       
               before booting or `c' for a command-line. ESC to return             
               previous menu.                                                      

-  optional: enable root ssh access on target and deploy your public
   keys for easy install access

-  Prepare local APT repository for install:

   ::

     root@target:~# mkdir /apt

Build the ACRN kernel for the Service VM
----------------------------------------

Since we need at least the proper acrn kernel module providing the
services needed, building the appropriate ACRN enabled kernel for the
target’s service vm is required.

Recommendation: Using a docker container to build the kernel keeps your
development host clean and avoids polluting it with any additional
packages. Nevertheless, if you are brave, you can do the following stuff
on your development host directly!

-  Prepare working directory on the dev host:

   ::

        me@dev-host:~$ mkdir -p ~/acrn-work
        me@dev-host:~$ cd ~/acrn-work

-  Shallow clone the latest acrn-kernel:

   ::

        me@dev-host:~/acrn-work$ git clone -b master --depth 1 https://github.com/projectacrn/acrn-kernel.git

-  Now, optionally fire up the build container and mount the work
   directory:

   ::

       me@dev-host:~/acrn-work$ docker run -it --rm -v $(pwd):/acrn-work debian:bullseye-slim

-  Make sure you have the kernel build requirements installed:

   ::

       root@9af58fa7aeb2:/# apt-get update
       root@9af58fa7aeb2:/# apt-get install -y libssl-dev build-essential bison flex dwarves kernel-wedge bc rsync kmod cpio git libelf-dev lz4 libncurses-dev

-  Prepare the kernel config: I add FB support here to be able to use
   Linux virtual consoles ;-)

   ::

       root@9af58fa7aeb2:/# cd /acrn-work/acrn-kernel
       root@9af58fa7aeb2:/acrn-work/acrn-kernel# cp kernel_config_service_vm .config
       root@9af58fa7aeb2:/acrn-work/acrn-kernel# echo "CONFIG_FB=y" >> .config
       root@9af58fa7aeb2:/acrn-work/acrn-kernel# echo "CONFIG_FRAMEBUFFER_CONSOLE=y" >> .config
       root@9af58fa7aeb2:/acrn-work/acrn-kernel# echo "CONFIG_FRAMEBUFFER_CONSOLE_DETECT_PRIMARY=y" >> .config
       root@9af58fa7aeb2:/acrn-work/acrn-kernel# make olddefconfig

-  Compile the kernel; this takes a while, maybe good time for a coffee
   break?

   ::

       root@9af58fa7aeb2:/acrn-work/acrn-kernel# make -j $(nproc) deb-pkg

   The resulting kernel packages are located in ``/acrn-work`` or
   ``~/acrn-work`` on the development host, respectively.

-  Exit the helper container and copy the new kernel package to the
   target:

   ::

        root@9af58fa7aeb2:/acrn-work/acrn-kernel# exit
        me@dev-host:~/acrn-work$ scp ~/acrn-work/linux-image-*-acrn-service-vm_*.deb root@target:/apt/

-  I recommend installing the new kernel immediately and reboot the
   target although we have not yet deployed any ACRN packages. This
   ensures/verifies you can go back to a non-acrn boot easily if
   something fails:

   ::

        root@target:~# apt update -y
        root@target:~# apt install -y /apt/linux-image-*-acrn-service-vm_*.deb
        root@target:~# reboot

-  On target distributions with a newer default kernel than 5.10
   (e.g.Ubuntu Jammy, Debian Sid) you have to choose the ACRN kernel
   manually in grub menu, since always the newest kernel version is
   selected as default:

   -  Choose ``Advanced options <..>`` first, then select the new ACRN
      kernel:

      ::

                                      GNU GRUB  version 2.06

          +----------------------------------------------------------------------------+
          | Ubuntu, with Linux 5.15.0-27-generic                                       |
          | Ubuntu, with Linux 5.15.0-27-generic (recovery mode)                       |
          |*Ubuntu, with Linux 5.10.106-acrn-service-vm                                |
          | Ubuntu, with Linux 5.10.106-acrn-service-vm (recovery mode)                |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          |                                                                            |
          +----------------------------------------------------------------------------+

               Use the ^ and v keys to select which entry is highlighted.
               Press enter to boot the selected OS, `e' to edit the commands
               before booting or `c' for a command-line. ESC to return
               previous menu.

-  Verify the correct kernel has been booted:

   ::

         root@target:~# uname -a
         Linux target 5.10.106-acrn-service-vm #1 SMP PREEMPT Mon May 2 15:54:32 UTC 2022 x86_64 x86_64 x86_64 GNU/Linux

-  **IMPORTANT** (for all distributions with newer kernel, i.e. >=5.12):

   Linux kernel versions since 5.12 officially contain the ACRN
   Hypervisor Service Module and are detected as *ACRN capable* if the
   module is enabled. Nevertheless, the version of ACRN HSM provided in
   upstream kernels is not (yet) compatible with ACRN 3.0 and thus will
   fail! Please remove the distribution’s original kernel (5.15 in case
   of Ubuntu jammy) to avoid any issues when booting such a kernel with
   ACRN 3.x:

   -  Find out the exact version of the original image:

      ::

          root@target:~# dpkg -l 'linux-image*'
          Desired=Unknown/Install/Remove/Purge/Hold
          | Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
          |/ Err?=(none)/Reinst-required (Status,Err: uppercase=bad)
          ||/ Name                                   Version                    Architecture Description
          +++-======================================-==========================-============-=======================================>
          un  linux-image                            <none>                     <none>       (no description available)
          ii  linux-image-5.10.106-acrn-service-vm   5.10.106-acrn-service-vm-1 amd64        Linux kernel, version 5.10.106-acrn-ser>
          ii  linux-image-5.15.0-27-generic          5.15.0-27.28               amd64        Signed kernel image generic
          ii  linux-image-generic                    5.15.0.27.30               amd64        Generic Linux kernel image
          un  linux-image-unsigned-5.15.0-27-generic <none>                     <none>       (no description available)

   -  E.g., we find ``5.15.0-27-generic`` here, so purge all respective
      packages (image, modules, …):

      ::

          root@target:~# apt purge '*5.15.0-27-generic*'

   -  Verify there is only the acrn-kernel installed now:

      ::

          root@target:~# dpkg -l 'linux-image*'
          Desired=Unknown/Install/Remove/Purge/Hold
          | Status=Not/Inst/Conf-files/Unpacked/halF-conf/Half-inst/trig-aWait/Trig-pend
          |/ Err?=(none)/Reinst-required (Status,Err: uppercase=bad)
          ||/ Name                                 Version                    Architecture Description
          +++-====================================-==========================-============-=========================================>
          ii  linux-image-5.10.106-acrn-service-vm 5.10.106-acrn-service-vm-1 amd64        Linux kernel, version 5.10.106-acrn-servi>

   -  Remark: Removing the distribution’s default kernel is not required
      whenever the ACRN kernel is detected as the most recent kernel!

Build & Deploy ACRN Debian Packages
-----------------------------------

-  Clone acrn-hypervisor (with debian packaging commits included, once
   the changes are merged)

   ::

        me@dev-host:~/acrn-work$ git clone -b master --depth 1 https://github.com/projectacrn/acrn-hypervisor.git
        me@dev-host:~/acrn-work$ cd acrn-hypervisor

-  Build the ACRN Debian packages (for Ubuntu jammy in this example)

   ::

        me@dev-host:~/acrn-work/acrn-hypervisor$ rm -rf build/jammy DISTRO=jammy VENDOR=ubuntu debian/docker/acrn-docker-build.sh --git-ignore-branch

   The ACRN Debian/Ubuntu packages are then located in
   ~/acrn-work/acrn-hypervisor/build/jammy

-  Copy these packages to the target:

   ::

        me@dev-host:~/acrn-work/acrn-hypervisor$ scp build/jammy/* root@target:/apt/

-  On target add the local apt repository and update the repository
   cache:

   ::

        root@target:~# echo "deb [trusted=yes] file:/apt ./" > /etc/apt/sources.list.d/acrn-local.list
        root@target:~# echo "deb-src [trusted=yes] file:/apt ./" >> /etc/apt/sources.list.d/acrn-local.list
        root@target:~# apt update -y

-  Install ACRN and optionally its tools (for debugging and tracing
   purposes). **Always select the correct board and scenario!** I
   recommend to start with the ``shared`` scenario.

   ::

        root@target:~# DEBIAN_FRONTEND=readline apt install -y acrn-system acrn-tools
        < .. skipped .. >
        Configuring acrn-hypervisor
        ---------------------------

        Define the board ACRN will be running on. Selecting the wrong board might render your board unusable!

          1. cfl-k700-i7  2. kontron-COMe-mAL10  3. nuc11tnbi5  4. nuc7i7dnh  5. simatic-ipc227g  6. tgl-vecow-spc-7100-Corei7  7. whl-ipc-i5

        ACRN hypervisor board selection 7

        Define the appropriate VM configuration (aka scenario) for the ACRN hypervisor.

          1. partitioned  2. shared  3. hybrid  4. hybrid_rt

        ACRN hypervisor scenario selection 2

        < .. skipped .. >
        root@target:~# reboot

-  ACRN should boot the service VM flawlessly! Verify ACRN started
   properly:

   ::

       root@target:~# dmesg | grep Hypervisor
       [    0.000000] Hypervisor detected: ACRN
       root@target:~# systemctl status acrnd acrn-lifemngr
       ● acrnd.service - ACRN manager daemon
            Loaded: loaded (/lib/systemd/system/acrnd.service; enabled; vendor preset: enabled)
            Active: active (running) since Fri 2022-05-06 10:20:29 UTC; 2min 48s ago
          Main PID: 570 (acrnd)
             Tasks: 3 (limit: 36031)
            Memory: 436.0K
               CPU: 932ms
            CGroup: /system.slice/acrnd.service
                    └─570 /usr/bin/acrnd -t

       May 06 10:20:29 target systemd[1]: Started ACRN manager daemon.

       ● acrn-lifemngr.service - ACRN lifemngr daemon
            Loaded: loaded (/lib/systemd/system/acrn-lifemngr.service; enabled; vendor preset: enabled)
            Active: active (running) since Fri 2022-05-06 10:20:29 UTC; 2min 48s ago
          Main PID: 568 (acrn-lifemngr)
             Tasks: 17 (limit: 36031)
            Memory: 1.1M
               CPU: 1.167s
            CGroup: /system.slice/acrn-lifemngr.service
                    └─568 /usr/bin/acrn-lifemngr

       May 06 10:20:29 target systemd[1]: Started ACRN lifemngr daemon.

-  Remarks:

   -  ``acrn-tools`` provides additional services: ``acrnlog``,
      ``acrnprobe`` and ``usercrash``. ``acrnlog`` is only started
      successfully if you specified a respective ``hvlog`` setting in
      the ``bootargs`` of your scenario which then are added the
      kernel’s parameters in the grub configuration. This is not the
      case in this example (``whl-ipc-i5:shared``).
   -  To start a VM, the respective launch scripts are also provided in
      ``/usr/share/acrn/launch-scripts/<board>/<scenario>``. Just
      provide your VM image (e.g. YaaG.img) and use the respective
      script!
   -  There is no networking bridge (acrn-br0) created automatically
      during install, since this heavily depends on the network tools
      used for the respective target distribution (e.g. netplan) and/or
      the respective backend (NetworkManager, systemd-networkd, ..) and
      might break your network setup. So, if needed, please configure
      the required network items (e.g. acrn-br0) according to your
      system needs!

ACRN Package Reconfiguration
----------------------------

To reconfigure the board/scenario settings use (here an example usable
on a simple line terminal - ``DEBIAN_FRONTEND=readline``):

::

   root@target:~# DEBIAN_FRONTEND=readline dpkg-reconfigure acrn-hypervisor
   Configuring acrn-hypervisor
   ---------------------------

   Define the board ACRN will be running on. Selecting the wrong board might render your board unusable!

     1. cfl-k700-i7  2. kontron-COMe-mAL10  3. nuc11tnbi5  4. nuc7i7dnh  5. simatic-ipc227g  6. tgl-vecow-spc-7100-Corei7  7. whl-ipc-i5

   ACRN hypervisor board selection 7

   Define the appropriate VM configuration (aka scenario) for the ACRN hypervisor.

     1. partitioned  2. shared  3. hybrid  4. hybrid_rt

   ACRN hypervisor scenario selection 2

**Remember: Always choose the appropriate board/scenario!**

ACRN Removal
------------

If you want to remove ACRN from your system completely, use:

::

   root@target:~# apt purge -y acrn-system acrn-tools; apt autoremove --purge

Remember: The system still uses the ACRN specific kernel, so you
eventually have to re-install the distribution’s default kernel, reboot
and then remove the ACRN kernel: Here for the Ubuntu example:

::

   root@target:~# apt install linux-image-generic
   root@target:~# reboot

Choose the new kernel in grub menu, again via
``Advanced options <..>``):

::

                                GNU GRUB  version 2.06

    +----------------------------------------------------------------------------+
    |*Ubuntu, with Linux 5.15.0-27-generic                                       | 
    | Ubuntu, with Linux 5.15.0-27-generic (recovery mode)                       |
    | Ubuntu, with Linux 5.10.106-acrn-service-vm                                |
    | Ubuntu, with Linux 5.10.106-acrn-service-vm (recovery mode)                |
    |                                                                            |
    |                                                                            |
    |                                                                            |
    |                                                                            |
    |                                                                            |
    |                                                                            |
    |                                                                            | 
    +----------------------------------------------------------------------------+

         Use the ^ and v keys to select which entry is highlighted.          
         Press enter to boot the selected OS, `e' to edit the commands       
         before booting or `c' for a command-line. ESC to return             
         previous menu.                                                      

Finally remove the unused acrn-kernel:

::

   root@target:~# apt purge linux-image-*-acrn-service-vm

This completes removing ACRN from your system.

-- Helmut Buchsbaum <helmut.buchsbaum@opensource.tttech-industrial.com>
Sat, 06 May 2022 18:21:44 +0200

