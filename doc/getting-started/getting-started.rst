.. _gsg:
.. _rt_industry_ubuntu_setup:
.. _getting-started-building:

Getting Started Guide
#####################

This guide will help you get started with ACRN. We'll show how to prepare a
build environment on your development computer. Then we'll walk through the
steps to set up a simple ACRN configuration on a target system. The
configuration is an ACRN shared scenario and consists of an ACRN hypervisor,
Service VM, and one post-launched User VM as illustrated in this figure:

.. image:: ./images/gsg_scenario-1-0.75x.png

Throughout this guide, you will be exposed to some of the tools, processes, and
components of the ACRN project. Let's get started.

.. _gsg-prereq:

Prerequisites
**************

You will need two machines: a development computer and a target system. The
development computer is where you configure and build ACRN and your application.
The target system is where you deploy and run ACRN and your application.

.. image:: ./images/gsg_host_target.png

Before you begin, make sure your machines have the following prerequisites:

**Development computer**:

* Hardware specifications

  - A PC with Internet access (A fast system with multiple cores and 16GB
    memory or more will make the builds go faster.)

* Software specifications

  - Ubuntu Desktop 20.04 LTS (ACRN development is not supported on Windows.)

**Target system**:

* Hardware specifications

  - Target board (see :ref:`hardware_tested`)
  - Ubuntu Desktop 20.04 LTS bootable USB disk: download the latest `Ubuntu
    Desktop 20.04 LTS ISO image <https://releases.ubuntu.com/focal/>`__ and
    follow the `Ubuntu documentation
    <https://ubuntu.com/tutorials/create-a-usb-stick-on-ubuntu#1-overview>`__
    for creating the USB disk.
  - USB keyboard and mouse
  - Monitor
  - Ethernet cable and Internet access
  - A second USB disk with minimum 1GB capacity to copy files between the
    development computer and target system (this guide offers steps for
    copying via USB disk, but you can use another method if you prefer)
  - Local storage device (NVMe or SATA drive, for example)

.. _gsg-target-hardware:

.. rst-class:: numbered-step

Set Up the Target Hardware
**************************

To set up the target hardware environment:

#. Connect the mouse, keyboard, monitor, and power supply cable to the target
   system.

#. Connect the target system to the LAN with the Ethernet cable.

Example of a target system with cables connected:

.. image:: ./images/gsg_nuc.png
   :scale: 25%

.. _gsg-dev-computer:

.. rst-class:: numbered-step

Prepare the Development Computer
********************************

To set up the ACRN build environment on the development computer:

#. On the development computer, run the following command to confirm that Ubuntu
   Desktop 20.04 is running:

   .. code-block:: bash

      cat /etc/os-release

   If you have an older version, see `Ubuntu documentation
   <https://ubuntu.com/tutorials/install-ubuntu-desktop#1-overview>`__ to
   install a new OS on the development computer.

#. Download the information database about all available package updates for
   your Ubuntu release. We'll need it to get the latest tools and libraries used
   for ACRN builds:

   .. code-block:: bash

      sudo apt update

   This next command upgrades packages already installed on your system with
   minor updates and security patches. This command is optional as there is a
   small risk that upgrading your system software can introduce unexpected
   issues:

   .. code-block:: bash

      sudo apt upgrade -y #optional command to upgrade system

#. Install the necessary ACRN build tools:

   .. code-block:: bash

      sudo apt install -y gcc \
           git \
           make \
           vim \
           libssl-dev \
           libpciaccess-dev \
           uuid-dev \
           libsystemd-dev \
           libevent-dev \
           libxml2-dev \
           libxml2-utils \
           libusb-1.0-0-dev \
           python3 \
           python3-pip \
           python3.8-venv \
           libblkid-dev \
           e2fslibs-dev \
           pkg-config \
           libnuma-dev \
           libcjson-dev \
           liblz4-tool \
           flex \
           bison \
           xsltproc \
           clang-format \
           bc \
           libpixman-1-dev \
           libsdl2-dev \
           libegl-dev \
           libgles-dev \
           libdrm-dev

#. Install Python package dependencies:

   .. code-block:: bash

      sudo pip3 install lxml xmlschema defusedxml tqbm

#. Create a working directory:

   .. code-block:: bash

      mkdir ~/acrn-work

#. Install the iASL compiler/disassembler used for advanced power management,
   device discovery, and configuration (ACPI) within the host OS:

   .. code-block:: bash

      cd ~/acrn-work
      wget https://acpica.org/sites/acpica/files/acpica-unix-20210105.tar.gz
      tar zxvf acpica-unix-20210105.tar.gz
      cd acpica-unix-20210105
      make clean && make iasl
      sudo cp ./generate/unix/bin/iasl /usr/sbin

#. Get the ACRN hypervisor and kernel source code. (Because the ``acrn-kernel`` repo
   has a lot of Linux kernel history, you can clone the relevant release branch
   with minimal history, as shown here.)

   .. code-block:: bash

      cd ~/acrn-work
      git clone https://github.com/projectacrn/acrn-hypervisor.git
      cd acrn-hypervisor
      git checkout acrn-2022w18.4-180000p

      cd ..
      git clone --depth 1 --branch acrn-2022w18.4-180000p https://github.com/projectacrn/acrn-kernel.git

.. _gsg-board-setup:

.. rst-class:: numbered-step

Prepare the Target and Generate a Board Configuration File
***************************************************************

In this step, you will use the **Board Inspector** to generate a board
configuration file.

A **board configuration file** is an XML file that stores hardware-specific
information extracted from the target system. The file is used to configure the
ACRN hypervisor, because each hypervisor instance is specific to your target
hardware.

.. important::

   Whenever you change the configuration of the board, such as peripherals, BIOS
   settings, additional memory, or PCI devices, you must generate a new board
   configuration file.

Install OS on the Target
============================

The target system needs Ubuntu Desktop 20.04 LTS to run the Board Inspector
tool.

To install Ubuntu 20.04:

#. Insert the Ubuntu bootable USB disk into the target system.

#. Power on the target system, and select the USB disk as the boot device
   in the UEFI
   menu. Note that the USB disk label presented in the boot options depends on
   the brand/make of the USB drive. (You will need to configure the BIOS to boot
   off the USB device first, if that option isn't available.)

#. After selecting the language and keyboard layout, select the **Normal
   installation** and **Download updates while installing Ubuntu** (downloading
   updates requires the target to have an Internet connection).

   .. image:: ./images/gsg_ubuntu_install_01.png

#. Use the check boxes to choose whether you'd like to install Ubuntu alongside
   another operating system, or delete your existing operating system and
   replace it with Ubuntu:

   .. image:: ./images/gsg_ubuntu_install_02.png

#. Complete the Ubuntu installation and create a new user account ``acrn`` and
   set a password.

#. The next section shows how to configure BIOS settings.

Configure Target BIOS Settings
===============================

#. Boot your target and enter the BIOS configuration editor.

   Tip: When you are booting your target, you'll see an option (quickly) to
   enter the BIOS configuration editor, typically by pressing :kbd:`F2` during
   the boot and before the GRUB menu (or Ubuntu login screen) appears.

#. Configure these BIOS settings:

   * Enable **VMX** (Virtual Machine Extensions, which provide hardware
     assist for CPU virtualization).
   * Enable **VT-d** (Intel Virtualization Technology for Directed I/O, which
     provides additional support for managing I/O virtualization).
   * Disable **Secure Boot**. This setting simplifies the steps for this example.

   The names and locations of the BIOS settings differ depending on the target
   hardware and BIOS version.

Generate a Board Configuration File
=========================================

#. Build the Board Inspector Debian package on the development computer:

   a. Move to the development computer.

   #. On the development computer, go to the ``acrn-hypervisor`` directory:

      .. code-block:: bash

         cd ~/acrn-work/acrn-hypervisor

   #. Build the Board Inspector Debian package:

      .. code-block:: bash

         make clean && make board_inspector

      When done, the build generates a Debian package in the ``./build``
      directory.

#. Copy the Board Inspector Debian package from the development computer to the
   target system via USB disk as follows:

   a. On the development computer, insert the USB disk that you intend to use to
      copy files.

   #. Ensure that there is only one USB disk inserted by running the following
      command:

      .. code-block:: bash

         ls /media/$USER

      Confirm that only one disk name appears. You'll use that disk name in the following steps.

   #. Copy the Board Inspector Debian package to the USB disk:

      .. code-block:: bash

         cd ~/acrn-work/
         disk="/media/$USER/"$(ls /media/$USER)
         cp -r acrn-hypervisor/build/acrn-board-inspector*.deb "$disk"/
         sync && sudo umount "$disk"

   #. Insert the USB disk into the target system.

   #. Copy the Board Inspector Debian package from the USB disk to the target:

      .. code-block:: bash

         mkdir -p ~/acrn-work
         disk="/media/$USER/"$(ls /media/$USER)
         cp -r "$disk"/acrn-board-inspector*.deb  ~/acrn-work

#. Install the Board Inspector Debian package on the target system:

   .. code-block:: bash

      cd  ~/acrn-work
      sudo apt install ./acrn-board-inspector*.deb

#. Reboot the system:

   .. code-block:: bash

      reboot

#. Run the Board Inspector to generate the board configuration file. This
   example uses the parameter ``my_board`` as the file name.

   .. code-block:: bash

      cd ~/acrn-work
      sudo board_inspector.py my_board

   .. note::

      If you get an error that mentions Pstate and editing the GRUB
      configuration, reboot the system and run this command again.

#. Confirm that the board configuration file ``my_board.xml`` was generated in
   the current directory:

   .. code-block:: bash

      ls ./my_board.xml

#. Copy ``my_board.xml`` from the target to the development computer via USB
   disk as follows:

   a. Make sure the USB disk is connected to the target.

   #. Copy ``my_board.xml`` to the USB disk:

      .. code-block:: bash

         disk="/media/$USER/"$(ls /media/$USER)
         cp ~/acrn-work/my_board.xml "$disk"/
         sync && sudo umount "$disk"

   #. Insert the USB disk into the development computer.

   #. Copy ``my_board.xml`` from the USB disk to the development computer:

      .. code-block:: bash

         disk="/media/$USER/"$(ls /media/$USER)
         cp "$disk"/my_board.xml ~/acrn-work
         sync && sudo umount "$disk"

.. _gsg-dev-setup:

.. rst-class:: numbered-step

Generate a Scenario Configuration File and Launch Script
********************************************************

In this step, you will use the **ACRN Configurator** to generate a scenario
configuration file and launch script.

A **scenario configuration file** is an XML file that holds the parameters of
a specific ACRN configuration, such as the number of VMs that can be run,
their attributes, and the resources they have access to.

A **launch script** is a shell script that is used to configure and create a
post-launched User VM. Each User VM has its own launch script.

First, you will install dependencies, build the ACRN Configurator Debian
package, and install it on your development computer. Then you will use the ACRN
Configurator to generate a scenario configuration file and launch script.

#. On the development computer, install the ACRN Configurator build tools:

   .. code-block:: bash

      sudo apt install -y libwebkit2gtk-4.0-dev \
         build-essential \
         curl \
         wget \
         libssl-dev \
         libgtk-3-dev \
         libappindicator3-dev \
         librsvg2-dev \
         python3-venv

#. Install Node.js (npm included) as follows:

   a. We recommend using nvm to manage your Node.js runtime. It allows you to
      switch versions and update Node.js easily.

      .. code-block:: bash

         curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.35.2/install.sh | bash

   #. Rerun your ``.bashrc`` initialization script and then install the latest
      version of Node.js and npm:

      .. code-block:: bash

         source ~/.bashrc
         nvm install node --latest-npm
         nvm use node

#. Install and upgrade Yarn:

   .. code-block:: bash

      npm install --global yarn

#. Install rustup, the official installer for Rust:

   .. code-block:: bash

      curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

   When prompted by the Rust installation script, type ``1`` and press
   :kbd:`Enter`.

   .. code-block:: console

      1) Proceed with installation (default)
      2) Customize installation
      3) Cancel installation
      >1

#. Configure the current shell:

   .. code-block:: bash

      source $HOME/.cargo/env

#. Install additional ACRN Configurator dependencies:

   .. code-block:: bash

      cd ~/acrn-work/acrn-hypervisor/misc/config_tools/configurator
      python3 -m pip install -r requirements.txt
      yarn

#. Build the ACRN Configurator Debian package:

   .. code-block:: bash

      cd ~/acrn-work/acrn-hypervisor
      make configurator

#. Install the ACRN Configurator:

   .. code-block:: bash

      sudo apt install -y ~/acrn-work/acrn-hypervisor/build/acrn-configurator_*_amd64.deb

#. Launch the ACRN Configurator:

   .. code-block:: bash

      acrn-configurator

#. On the left-hand side, look for **Start a new configuration**.
   Confirm that the working folder is ``<path to>/acrn-work/MyConfiguration``. Click **Use This Folder**.

   TODO add screenshots to Configurator steps

#. Import your board configuration file as follows:

   a. Under **Import a board configuration file**, click **Browse for file**.

   #. Browse to ``~/acrn-work/my_board.xml`` and click **Open**.

   #. Click **Import Board File**.

   The ACRN Configurator makes a copy of your board file, changes the file
   extension to ``.board.xml``, and saves the file to the working folder.

#. Create a new scenario as follows:

   a. Under **Create new or import an existing scenario**, click **Create
      Scenario**.

   #. In the dialog box, confirm that **Shared (Post-launched VMs only)** is
      selected.

   #. Confirm that one Service VM and one post-launched VM are selected.

   #. Click **Create**.

#. Generate the scenario configuration file and launch script:

   a. Under **Configure settings for scenario and launch scripts**, the
      scenario's configurable items appear. Feel free to look through all the
      available configuration settings. This is where you can change the
      settings to meet your application's particular needs. But for now, you will update only a few settings to make this example work.

   #. Click the **VM1 Post-launched** tab to access the post-launched VM's
      settings.

   #. Confirm that the Basic Parameters tab is selected, and scroll down to
      **Memory size (MB)**. Change the value to ``1024``. For this example, we
      will use Ubuntu 20.04 to boot the post-launched VM. Ubuntu 20.04 needs at
      least 1024 MB to boot.

   #. Scroll down to **Virtio block device**, click **+**, and enter
      ``~/acrn-work/ubuntu-20.04.4-desktop-amd64.iso``. This parameter
      specifies the VM's OS image and its location on the target system. Later
      in this guide, you will save the ISO file to that directory.

   #. Click **Save Scenario And Launch Scripts** to generate the scenario
      configuration file and launch script.

#. Click the **x** in the upper-right corner to close the ACRN
   Configurator.

#. Confirm that the scenario configuration file ``scenario.xml`` appears in your
   ``acrn-work/MyConfiguration`` directory::

         ls ~/acrn-work/MyConfiguration/scenario.xml

#. Confirm that the launch script ``TODO`` appears in the
   expected output directory::

         ls ~/acrn-work/MyConfiguration/TODO

.. _gsg_build:

.. rst-class:: numbered-step

Build ACRN
***************

#. On the development computer, build the ACRN hypervisor:

   .. code-block:: bash

      cd ~/acrn-work/acrn-hypervisor
      make clean && make BOARD=~/acrn-work/MyConfiguration/my_board.board.xml SCENARIO=~/acrn-work/MyConfiguration/scenario.xml

   The build typically takes a few minutes. When done, the build generates a
   Debian package in the ``./build`` directory:

   .. code-block:: bash

      cd ./build
      ls *.deb
         acrn-my_board-shared-2.7.deb # TODO update file name

   The Debian package contains the ACRN hypervisor and tools to ease installing
   ACRN on the target.

#. Build the ACRN kernel for the Service VM:

   a. If you have built the ACRN kernel before, run the following command to
      remove all artifacts from the previous build. Otherwise, an error will
      occur during the build.

      .. code-block:: bash

         make distclean

   #. Build the ACRN kernel:

      .. code-block:: bash

         cd ~/acrn-work/acrn-kernel
         cp kernel_config_service_vm .config
         make olddefconfig
         make -j $(nproc) deb-pkg

   The kernel build can take 15 minutes or less on a fast computer, but could
   take an hour or more depending on the performance of your development
   computer. When done, the build generates four Debian packages in the
   directory above the build root directory:

   .. code-block:: bash

      cd ..
      ls *.deb
         linux-headers-5.10.78-acrn-service-vm_5.10.78-acrn-service-vm-1_amd64.deb
         linux-image-5.10.78-acrn-service-vm_5.10.78-acrn-service-vm-1_amd64.deb
         linux-image-5.10.78-acrn-service-vm-dbg_5.10.78-acrn-service-vm-1_amd64.deb
         linux-libc-dev_5.10.78-acrn-service-vm-1_amd64.deb

#. Copy all the necessary files generated on the development computer to the
   target system by USB disk as follows:

   a. Insert the USB disk into the development computer and run these commands:

      .. code-block:: bash

         disk="/media/$USER/"$(ls /media/$USER)
         cp ~/acrn-work/acrn-hypervisor/build/acrn-my_board-shared-2.7.deb "$disk"/ # TODO update file name
         cp ~/acrn-work/*acrn-service-vm*.deb "$disk"/
         cp ~/acrn-work/my_board/output/launch_user_vm_id3.sh "$disk"/
         cp ~/acrn-work/acpica-unix-20210105/generate/unix/bin/iasl "$disk"/
         sync && sudo umount "$disk"

      Even though our sample default scenario defines six User VMs, we're only
      going to launch one of them, so we'll only need the one launch script.

   #. Insert the USB disk you just used into the target system and run these
      commands to copy the files locally:

      .. code-block:: bash

         disk="/media/$USER/"$(ls /media/$USER)
         cp "$disk"/acrn-my_board-shared-2.7.deb ~/acrn-work # TODO update file name
         cp "$disk"/*acrn-service-vm*.deb ~/acrn-work
         cp "$disk"/launch_user_vm_id3.sh ~/acrn-work
         sudo cp "$disk"/iasl /usr/sbin/
         sync && sudo umount "$disk"

.. _gsg-install-acrn:

.. rst-class:: numbered-step

Install ACRN
************

#. Install the ACRN Debian package and ACRN kernel Debian packages using these
   commands:

   .. code-block:: bash

      cd ~/acrn-work
      sudo apt install ./acrn-my_board-shared-2.7.deb # TODO update file name
      sudo apt install ./*acrn-service-vm*.deb

#. Reboot the system:

   .. code-block:: bash

      reboot

#. Confirm that you see the GRUB menu with the “ACRN multiboot2” entry. Select
   it and proceed to booting ACRN. (It may be autoselected, in which case it
   will boot with this option automatically in 5 seconds.)

   .. code-block:: console

                              GNU GRUB version 2.04
      ────────────────────────────────────────────────────────────────────────────────
      Ubuntu
      Advanced options for Ubuntu
      UEFI Firmware Settings
      *ACRN multiboot2

.. _gsg-run-acrn:

.. rst-class:: numbered-step

Run ACRN and the Service VM
******************************

The ACRN hypervisor boots the Ubuntu Service VM automatically.

#. On the target, log in to the Service VM. (It will look like a normal Ubuntu
   session.)

#. Verify that the hypervisor is running by checking ``dmesg`` in the Service
   VM:

   .. code-block:: bash

      dmesg | grep -i hypervisor

   You should see "Hypervisor detected: ACRN" in the output. Example output of a
   successful installation (yours may look slightly different):

   .. code-block:: console

      [  0.000000] Hypervisor detected: ACRN

#. Enable and start the Service VM's system daemon for managing network configurations,
   so the Device Model can create a bridge device (acrn-br0) that provides User VMs with
   wired network access:

   .. code-block:: bash

      sudo systemctl enable --now systemd-networkd

.. _gsg-user-vm:

.. rst-class:: numbered-step

Launch the User VM
*******************

#. Go to the `official Ubuntu website <https://releases.ubuntu.com/focal/>`__ to
   get the Ubuntu Desktop 20.04 LTS ISO image
   ``ubuntu-20.04.4-desktop-amd64.iso`` for the User VM. (The same image you
   specified earlier in the ACRN Configurator UI.)

#. Put the ISO file in the path ``~/acrn-work/`` on the target system.

#. Launch the User VM:

   .. code-block:: bash

      sudo chmod +x ~/acrn-work/launch_user_vm_id3.sh # TODO update file name
      sudo ~/acrn-work/launch_user_vm_id3.sh # TODO update file name

#. It may take about one minute for the User VM to boot and start running the
   Ubuntu image. You will see a lot of output, then the console of the User VM
   will appear as follows:

   .. code-block:: console

      Ubuntu 20.04.4 LTS ubuntu hvc0

      ubuntu login:

#. Log in to the User VM. For the Ubuntu 20.04 ISO, the user is ``ubuntu``, and
   there's no password.

#. Confirm that you see output similar to this example:

   .. code-block:: console

      Welcome to Ubuntu 20.04.4 LTS (GNU/Linux 5.11.0-27-generic x86_64)

      * Documentation:  https://help.ubuntu.com
      * Management:     https://landscape.canonical.com
      * Support:        https://ubuntu.com/advantage

      0 packages can be updated.
      0 updates are security updates.

      Your Hardware Enablement Stack (HWE) is supported until April 2025.

      The programs included with the Ubuntu system are free software;
      the exact distribution terms for each program are described in the
      individual files in /usr/share/doc/*/copyright.

      Ubuntu comes with ABSOLUTELY NO WARRANTY, to the extent permitted by
      applicable law.

      To run a command as administrator (user "root"), use "sudo <command>".
      See "man sudo_root" for details.

      ubuntu@ubuntu:~$

#. This User VM and the Service VM are running different Ubuntu images. Use this
   command to see that the User VM is running the downloaded Ubuntu ISO image:

   .. code-block:: console

      ubuntu@ubuntu:~$ uname -r
      5.11.0-27-generic

   Then open a new terminal window and use the command to see that the Service
   VM is running the ``acrn-kernel`` Service VM image:

   .. code-block:: console

      acrn@vecow:~$ uname -r
      5.10.78-acrn-service-vm

   The User VM has launched successfully. You have completed this ACRN setup.

#. (Optional) To shut down the User VM, run this command in the terminal that is
   connected to the User VM:

   .. code-block:: bash

      sudo poweroff

.. _gsg-next-steps:

Next Steps
**************

:ref:`overview_dev` describes the ACRN configuration process, with links to
additional details.

