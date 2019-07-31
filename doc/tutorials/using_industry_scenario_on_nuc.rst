.. _using_industry_scenario_on_nuc:

Launch Two Guest VMs on NUC Using Industry Scenario
###################################################

ACRN hypervisor supports new industry scenario
since the tag ``acrn-2019w18.7-140000p``. In industry scenario,
up to three Guest VMs (e.g. Vxworks + RTVM + Windows) can be launched from
the Service VM. This tutorial provides step by step instructions on how
to enable the ACRN hypervisor industry scenario on Intel NUC to activate
two post-launched VMs running Linux based guest OSes. The same process
can be applied to launch the third VM as well.

ACRN Service VM Setup
*********************

Refer to the steps in :ref:`getting-started-apl-nuc` to set up ACRN on
Intel NUC. The target device must be capable of launching a Clear Linux Guest VM
as a starting point.

Re-build ACRN UEFI Executable
*****************************

The ACRN pre-built UEFI executable ``acrn.efi`` is compiled for SDC scenario
by default, in which only one post-launched VM is supported.
To activate more than one post-launched VM, you need to enable the industry
scenario and rebuild the UEFI executable with the following steps:

#. Refer to :ref:`getting-started-building` to set up the development environment
   for re-compiling the UEFI executable from ACRN source tree.

#. Enter the ``hypervisor`` directory under the ACRN source tree to
   reconfigure the ACRN hypervisor for industry scenario. Please refer following
   patch to enable industry scenario.
   
   .. code-block:: bash
	   
      diff --git a/hypervisor/arch/x86/Kconfig b/hypervisor/arch/x86/Kconfig
      index 2430ed67..7f92d0fd 100644
      --- a/hypervisor/arch/x86/Kconfig
      +++ b/hypervisor/arch/x86/Kconfig
      @@ -1,6 +1,6 @@
       choice
              prompt "ACRN Scenario"
      -       default SDC
      +       default INDUSTRY
              help
                Select the scenario name to load corresponding VM configuration.

   ``kbl-nuc-i7`` is used for example; you can specify other board type which is closed
   to your target platform.

#. Go to the root of ACRN source tree to build the ACRN UEFI executable
   with the customized configurations(``kbl-nuc-i7`` board as a example):

    .. code-block:: bash

       $ cd ..
       $ make FIRMWARE=uefi BOARD=kbl-nuc-i7

#. Copy the generated ``acrn.efi`` executable to the ESP partition (e.g. ``/dev/sda1``).
   You may need to mount the ESP partition if it's not mounted.

    .. code-block:: bash

       $ sudo mount /dev/sda1 /boot
       $ sudo cp build/hypervisor/acrn.efi /boot/EFI/acrn/acrn.efi

#. Reboot the ACRN hypervisor and the Service VM.

Launch Guest VMs with Predefined UUIDs
**************************************

In industry scenario, VMs launched by the ACRN device model ``acrn-dm``
must match the following UUIDs. You will have to add the ``-U`` parameter
to the launch script: ``launch_uos.sh`` ``launch_hard_rt_vm.sh`` ``launch_win.sh``,
in order to attach to specific VM through ``acrn-dm``.

    * d2795438-25d6-11e8-864e-cb7a18b34643
    * 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5
    * 38158821-5208-4005-b72a-8a609e4190d0

.. note:: ``495ae2e5-2603-4d64-af76-d4bc5a8ec0e5`` is only for hard RTVM, you may write
   this uuid into ``launch_hard_rt_vm.sh`` script.
	
For example, the following code snippet is used to launch the hard RT Guest VM:

    .. code-block:: none
       :emphasize-lines: 2

       /usr/bin/acrn-dm -A -m $mem_size -c $1 -s 0:0,hostbridge \
        -U 495ae2e5-2603-4d64-af76-d4bc5a8ec0e5 \
        -k /usr/lib/kernel/default-iot-lts2018-preempt-rt \
        --lapic_pt \
        --rtvm \
        --virtio_poll 1000000 \
        -s 2,passthru,2/0/0 \
        -s 3,virtio-console,@stdio:stdio_port \
       -B "root=/dev/nvme0n1p3 rw rootwait maxcpus=$1 nohpet console=hvc0 \
        no_timer_check ignore_loglevel log_buf_len=16M \
        consoleblank=0 tsc=reliable x2apic_phys" hard_rtvm

    .. note::
       These UUID are defined at hypervisor/scenarios/industry/vm_configurations.c
