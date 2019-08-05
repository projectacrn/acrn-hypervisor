.. _using_hybrid_mode_on_nuc:

Using Hybrid mode on NUC
########################
ACRN hypervisor supports hybrid scenario that the User VM
(such as Zephyr or Clear Linux) could run in a pre-launched VM
or in a post-launched VM which launched by Device model in Service VM.
The following guidelines shows how to set up the ACRN hypervisor hybrid
scenario on Intel NUC. The hybrid scenario on Intel NUC is shown in
:numref:`hybrid_scenario_on_nuc`.

.. figure:: images/hybrid_scenario_on_nuc.png
   :align: center
   :width: 400px
   :name: hybrid_scenario_on_nuc

   The HYBRID scenario on Intel NUC

Prerequisites
*************
- `Intel NUC Kit NUC7i7DNHE
  <https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc7i7dnhe.html>`_
- Connecting to the serial port, described in :ref:`Connecting to the serial port
  <connect_serial_port>`
- Install Grub on SATA or NVME disk of NUC

Update Ubuntu GRUB to Boot hypervisor and Load Kernel Image
***********************************************************
#. Append the following configuration to the ``/etc/grub.d/40_custom`` file:

   .. code-block:: bash
      :emphasize-lines: 18,20

      menuentry 'ACRN hypervisor Hybird Scenario' --class ubuntu --class gnu-linux --class gnu --class os $menuentry_id_option 'gnulinux-simple-e23c76ae-b06d-4a6e-ad42-46b8eedfd7d3' {

         recordfail

         load_video

         gfxmode $linux_gfx_mode

         insmod gzio

         insmod part_gpt

         insmod ext2

         echo 'Loading hypervisor Hybrid scenario ...'

         multiboot --quirk-modules-after-kernel /boot/acrn.32.out

         module /boot/zephyr.bin XXXXXX

         module /boot/bzImage yyyyyy

      }

   .. note:: The module ``/boot/zephyr.bin`` is VM0(Zephyr) kernel file,
      param ``xxxxxx`` is VM0’s kernel file tag and must exactly match the ``kernel_mod_tag``
      of VM0 which configured in file ``hypervisor/scenarios/hybrid/vm_configurations.c``.
      The multiboot module ``/boot/bzImage`` is Service VM kernel file, param ``yyyyyy``
      is the bzImage tag and must exactly match the ``kernel_mod_tag`` of VM1 in file
      ``hypervisor/scenarios/hybrid/vm_configurations.c``.
      The kernel command line arguments used to boot the Service VM is located in header file
      ``hypervisor/scenarios/hybrid/vm_configurations.h`` and configured by `SOS_VM_BOOTARGS` MACRO.

#. Modify the ``/etc/default/grub`` file as follows to make the GRUB menu
   visible when booting:

   .. code-block:: bash

      # GRUB_HIDDEN_TIMEOUT=0
      GRUB_HIDDEN_TIMEOUT_QUIET=false

#. Update grub::

   $ sudo update-grub

#. Reboot the NUC. Select the **ACRN hypervisor Hybrid Scenario** entry to boot
   the ACRN hypervisor on the NUC’s display. The GRUB loader will boot the
   hypervisor, and the hypervisor will start VMs automatically.

Hybrid Scenario Startup Checking
********************************
#. Use these steps to verify the hypervisor is properly running:

   a. Login ACRN hypervisor shell from serial console.
   #. Use the vm_list check pre-launched VM and Service VM are launched successfully.

#. Use these steps to verify all VMs are running properly:

   a. Use the ``vm_console 0`` to switch to VM0(Zephyr)’s console, it will show
      a string of **Hello world! acrn**.
   #. Use a :kbd:`Ctrl+Spacebar` to return to the ACRN hypervisor shell.
   #. Use the ``vm_console 1`` to switch to VM1(Service VM)’s console.
   #. The VM1’s Service VM could boot up and login in.
   #. SSH to VM1 and Launch post-launched VM2 by ACRN device model launch script.
   #. Back to Service VM console, use a :kbd:`Ctrl+Spacebar` to return to the ACRN hypervisor shell.
   #. Use the ``vm_console 2`` to switch to VM2(User VM)’s console.
   #. The VM2 could boot up and login in.

Refer to the :ref:`acrnshell` user guide for more information about available commands.