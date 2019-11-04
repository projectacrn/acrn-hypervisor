.. _enable-s5:

Platform S5 Enable Guide
########################

Introduction
************

S5 is one of the `ACPI Sleep State <http://acpi.sourceforge.net/documentation/sleep.html>`_.
It refers to the system being shut down (although some power may still be supplied to
certain devices). In this document, S5 means the function to shut down the
**User VMs**, **the Service VM**,  the hypervisor, and the hardware. In most cases,
directly shutting down the power of a computer system is not advisable because it can
damage some components. It can cause corruption and put the system in an unknown or
unstable state. On ACRN, the User VM must be shut down before powering off the Service VM.
Especially for some use cases, where User VMs could be used in industrial control or other
high safety requirement environment, a graceful system shutdown such as the ACRN S5
function is required.

S5 Architecture
***************

ACRN provides a mechanism to trigger the S5 state transition throughout the system.
It uses vUART channel to communicate between the Service VM and the User VMs.
The diagram below shows the overall architecture:

.. figure:: images/s5_overall_architecture.png
   :align: center

   S5 overall architecture

- **Scenario I**:

    The User VM's serial port device (``ttySn``) is emulated in Device Model, the channel from Service VM to User VM:

    .. graphviz:: images/s5-scenario-1.dot
       :name: s5-scenario-1

- **Scenario II**:

    The User VM's (like RT-Linux or other RT-VMs) serial port device (``ttySn``) is emulated in Hypervisor,
    the channel from Service OS to User VM:

    .. graphviz:: images/s5-scenario-2.dot
       :name: s5-scenario-2

How to trigger User VM's S5?
============================

On Service VM side, it uses the ``acrnctl`` tool to trigger the User VM's S5 flow:
``acrnctl stop user-vm-name``. Then Device Model will send a ``shutdown`` command
to User VM through channel, if User VM receives the command, it will send an "ACK"
to Device Model. It is the responsibility of Service VM to check if the User VMs
shutdown successfully or not, and decides when to power off itself.

What does "life cycle manager" in User VM do?
=============================================

As current S5 reference design, there is a life-cycle manager daemon (life_mngr) running in
User VM to implement S5. It will wait for the command from Service VM on the
paired serial port, there is a simple protocol between Service VM and User VM:
when it receives ``shutdown``, then send "acked" to Service VM, then it can power off
User VM.

However, if the User VM is not ready to power off, it can ignore the ``shutdown`` command.

.. _enable_s5:

Enable S5
*********

Enable S5 based on the the particular OS:

* For Linux (LaaG) or Windows (WaaG), refer to the following configurations in
  ``devicemodel/samples/nuc/launch_uos.sh`` launch script for ``acrn-dm``.

  .. literalinclude:: ../../../../devicemodel/samples/nuc/launch_uos.sh
     :name: laag-waag-script
     :caption: LaaG/WaaG launch script
     :linenos:
     :lines: 97-117
     :emphasize-lines: 2-4,17
     :language: bash

* For RT-Linux, refer to the ``devicemodel/samples/nuc/launch_hard_rt_vm.sh`` script:

  .. literalinclude:: ../../../../devicemodel/samples/nuc/launch_hard_rt_vm.sh
     :name: rt-script
     :caption: RT-Linux launch script
     :linenos:
     :lines: 42-58
     :emphasize-lines: 2-3,13
     :language: bash

  .. note:: For RT-Linux, the vUART is emulated in hypervisor, expose the node as ``/dev/ttySn``.

#. For LaaG and RT-Linux VM, a reference daemon is offered to run:

   a. Use these commands to build the life-cycle manager daemon, ``life_mngr``.

      .. code-block:: none
   
         $ cd acrn-hypervisor/misc/life_mngr
         $ make life_mngr

   #. Copy ``life_mngr`` and ``life_mngr.service`` into User VM:

      .. code-block:: none

         $ scp life_mngr root@<test board address>:/usr/bin/life_mngr
         $ scp life_mngr.service root@<test board address>:/lib/systemd/system/life_mngr.service

   #. Use below commands to enable ``life_mngr.service`` and restart User VM.

      .. code-block:: none

         # chmod +x /usr/bin/life_mngr
         # systemctl enable life_mngr.service
         # reboot

#. For WaaG VM, a reference daemon is offered to run:

   a) Build ``life_mngr_win.exe`` application::

        $ cd acrn-hypervisor/misc
        $ make life_mngr

      .. note:: If there is no ``x86_64-w64-mingw32-gcc`` compiler, you need run ``swupd bundle-add c-basic-mingw``
         to install it.

   #) Set up a Windows environment:

      I) Download the :kbd:`Visual Studio 2019` tool from `<https://visualstudio.microsoft.com/downloads/>`_,
         and choose the two options in the below screenshots to install "Microsoft Visual C++ Redistributable
         for Visual Studio 2015, 2017 and 2019 (x86 or X64)" in WaaG:

         .. figure:: images/Microsoft-Visual-C-install-option-1.png

         .. figure:: images/Microsoft-Visual-C-install-option-2.png

      #) In WaaG, use the :kbd:`WIN + R` shortcut key, input "shell:startup", click :kbd:`OK`
         and then copy ``life_mngr_win.exe`` application into this directory.

         .. figure:: images/run-shell-startup.png

         .. figure:: images/launch-startup.png

   #) Restart the WaaG VM. The COM2 window will automatically open after reboots.

         .. figure:: images/open-com-success.png

#. If the Service VM is being shut down (transitioning to the S5 state), it can call
   ``acrnctl stop vm-name`` to shut down the User VMs.

   .. note:: S5 state is not automatically triggered by a Service VM shutdown, this needs
      to be run before powering off the Service VM.

How to test
***********

.. note:: The :ref:`CBC <IOC_virtualization_hld>` tools and service installed by
   the `software-defined-cockpit
   <https://github.com/clearlinux/clr-bundles/blob/master/bundles/software-defined-cockpit>`_ bundle
   will conflict with the vUART and hence need to be masked.

   ::

      systemctl mask cbc_attach
      systemctl mask cbc_thermal_fuse
      systemctl mask cbc_thermald
      systemctl mask cbc_lifecycle.service

   Or::

      ps -ef|grep cbc; kill -9 cbc_pid

#. Refer to the :ref:`enable_s5` section to set up S5 environment for the User VMs.

   .. note:: RT-Linux's UUID must use ``495ae2e5-2603-4d64-af76-d4bc5a8ec0e5``, also the
      industry EFI image is required for launching RT-Linux VM.

   .. note:: Use ``systemctl status life_mngr.service`` command to ensure the service 
      is working on LaaG or RT-Linux:

      .. code-block:: console
     
           ● life_mngr.service - ACRN lifemngr daemon
           Loaded: loaded (/usr/lib/systemd/system/life_mngr.service; enabled; vendor p>
           Active: active (running) since Tue 2019-09-10 07:15:06 UTC; 1min 11s ago
           Main PID: 840 (life_mngr)

   .. note:: For WaaG, we need to close ``windbg`` by using ``"bcdedit /set debug off`` command
      if you executed ``bcdedit /set debug on`` when set up the WaaG, because it occupies the ``COM2``.

#. Use ``acrnctl stop`` command on Service VM to trigger S5 to the User VMs:

   .. code-block:: console

      # acrnctl stop vm1

#. Use ``acrnctl list`` command to check the User VM status.

   .. code-block:: console

      # acrnctl list
      vm1		stopped
