.. _acrn-sanity-test:

ACRN Sanity Test
################

The document describes the daily sanity test for ACRN by validation team, which 
includes a set of system level core tests of basic functionality to demonstrate 
ACRN hypervisor’s availability. User could manually run these after setup ACRN. 
These cases can be implemented on ``kbl-nuc-i7``.

ACRN Sanity Test Cases
######################

Hypervisor
##########

Introduction
************

ST_HV_Bootloader_LaaG_VUEFI_Default
***********************************

* Description

The test case focuses on testing boot up User VM using a virtual UEFI (OVMF) firmware and the default ``launch_uos.sh``.

* Prerequisites

Service VM boots up normally

* Test Steps

1. Launch Service VM
#. Use ``OVMF.fd`` to launch clearlinux using the default ``launch_uos.sh`` script

  .. code-block:: none

     $ mkdir -p /home/<your user name>/uos/

     $ cd /home/<your user name>/uos/

     $ curl https://download.clearlinux.org/releases/xxxxx/clear/clear-xxxxx-kvm.img.xz -o uos.img.xz

     $ unxz uos.img.xz

     $ sudo sed -i "s|/home/clear|$HOME|g" /usr/share/acrn/samples/nuc/launch_uos.sh

     $ sudo /usr/share/acrn/samples/nuc/launch_uos.sh

* Expected Result

User VM boot up normally

ST_HV_Memory_LaaG_Default_Partition_Check
*****************************************

* Description

This test case focus on checking if the pass through memory to User VM and
check if the memory size is in a reasonable scope as ``launch_uos.sh`` defined.
Since the User VM will take a part of memory, the available memory size will
not be exact as what pass-through.

* Prerequisites

Service VM and User VM boot up normally

* Test Steps

1. Boot to Service VM
#. Boot User VM with default memory setting in ``launch_uos.sh``
#. Check ``cat /proc/meminfo`` and see if the available memory is close the size passed through to User VM

Expected Result

1. Boot User VM successfully
#. memory is close the size passed through to User VM

IO
**

ST_ACRNIO_LaaG_Virtio_net
*************************

* Description

This test case focus on checking if Service VM and User VM can be allocated different IP,
and ping is ok by sharing same physical network card.

* Prerequisites

VirtIO and bridge/tap is created normally in Service VM

* Test Steps

1. Boot the Device Under Test (DUT), and enter Service VM from the ACRN hypervisor console (serial port).

   .. code-block:: none

      ACRN:\>vm_console

#. Check IP and bridge/tap after log in Service VM.

   .. code-block:: none
     
      $ ip a
      $ brctl show
      bridge name     bridge id               STP enabled     interfaces
      acrn-br0         8000.1ad4ac453b20       no              acrn_tap0
                                                               ens4

#. Boot User VM using the ``launch_uos.sh`` and check IP address.

   .. code-block:: none
   
      $ sudo /usr/share/acrn/samples/nuc/launch_uos.sh

      $ ip a

* Expected Result

IP address should be allocated for both Service VM  and User VM, and ping is ok


ST_ACRNIO_SOS_Storage
*********************

* Description

This test case focus on checking if storage device other than the one hosting the 
rootfs works correctly in Service VM.

* Prerequisites

storage device is installed

* Test Steps

1. Boot into Service VM

   .. code-block:: none
  
   $ fdisk -l 

#. Find storage device  

   * SATA -> /dev/sda
   * NVMe -> /dev/nvme0n1
   * eMMC -> /dev/mmcblk0

#. Mount a partition (in the example below, the third partition) read/write and verify you can access it

   * SATA -> mount /dev/sda3 
   * NVMe -> mount /dev/nvme0n1p3
   * eMMC -> mount /dev/mmcblk0p3   

* Expected Result

storage device be used normally in Service VM

Graphics Virtualization (GVT)
*****************************

ST_ACRNGT_SOS_Rendering_Weston_APP
**********************************

* AcrnGT provide virtual display on Service VM: run weston weston-simple-egl in Service VM,

workload will show normally on Service VM

* Prerequisites

System boot up normally

Install the `IAS Wayland compositor <https://github.com/intel/ias>`_ by installing these bundles

* Test Steps

1. Boot up hypervisor and Service VM
#. Start weston by command in Service VM
#. run ./weston-simple-egl in Service VM

   .. code-block:: none

      $ sudo swupd bundle-add software-defined-cockpit x11-server weston-extras
      $ ps -ef | grep -E 'weston|tty1' | grep -v grep | awk '{print $2}' | xargs kill ; pids=$(lsof |grep tty1 |awk '{print $2}' |uniq -d) ; for pid in $pids; do kill -9 $pid; done;cd /usr/share/xdg/weston/ ; [ -f weston.ini ] && mv weston.ini weston.ini_bak ; cd - &> /dev/null; export XDG_RUNTIME_DIR=/run/user/0;sync;sync;sync;
      $ weston --tty=1 -i 0 &
      $ cd /usr/bin
      $ weston-simple-egl

   .. note::

      do not install bundle desktop, these two may had conflict

* Expected Result

Able to run test apps in Service VM without exception and display normally

TC_ACRNGT_SOS_Rendering_Weston_APP
**************************************

* Description

AcrnGT support GPU sharing feature, run weston apps weston-simple-shm in Service VM,
Service VM will show workload normally

* Prerequisites

System boot up normally

Install the `IAS Wayland compositor <https://github.com/intel/ias>`_ by installing these bundles

* Test Steps

1. Boot up hypervisor, Service VM 
#. Start weston by command in Service VM:
#. Run ./weston-simple-shm
   
   .. code-block:: none

      $ sudo swupd bundle-add software-defined-cockpit x11-server weston-extras
      $ ps -ef | grep -E 'weston|tty1' | grep -v grep | awk '{print $2}' | xargs kill ; pids=$(lsof |grep tty1 |awk '{print $2}' |uniq -d) ; for pid in $pids; do kill -9 $pid; done;cd /usr/share/xdg/weston/ ; [ -f weston.ini ] && mv weston.ini weston.ini_bak ; cd - &> /dev/null; export XDG_RUNTIME_DIR=/run/user/0;sync;sync;sync;
      $ weston --tty=1 -i 0 &
      $ cd /usr/bin
      $ weston-simple-shm

   .. note::

      do not install bundle desktop, these two may had conflict

* Expected Result

Able to run test apps in Service VM.

