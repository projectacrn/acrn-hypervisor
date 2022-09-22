.. _enable-s5:

Enable S5
#########

About System S5 Support
***********************

S5 refers to the ACPI “soft off” system state. ACRN system S5 support enables
you to gracefully shut down or reset the whole system when multiple VMs are
running. This is done by requesting and waiting for all pre-launched and
post-launched VMs to gracefully shut themselves down before the Service VM
triggers a system-wide shutdown or reset. 

We recommend using ACRN system S5 support to shut down or reset a system unless
you have other mechanisms in place to protect external storage from being
corrupted by a mechanical off.

Dependencies and Constraints
****************************

Consider the following dependencies and constraints: 

* ACRN system S5 support is hardware neutral but requires the deployment of a
  daemon (named Lifecycle Manager) in all VMs. The Lifecycle Manager manages
  power state transitions.

* The COM2 port is reserved for the Lifecycle Manager to communicate requests
  and responses. Console vUARTs and inter-VM UART connections should avoid using
  COM2 as an interface.

* The S5 feature needs a communication vUART to control a User VM. However, you
  don't need to configure a vUART connection for S5 via the ACRN Configurator,
  because ACRN code already has a vUART connection between the Service VM and
  User VMs by default.

Example Configuration
*********************

The following steps show how to enable S5 by extending the information provided
in the :ref:`gsg`. The scenario has a Service VM and one Ubuntu post-launched
User VM.

#. On the development computer, build the Lifecycle Manager daemon::

      cd acrn-hypervisor
      make life_mngr

   The build generates files in the ``build/misc/services/life_mngr`` directory.

#. Copy ``life_mngr.conf``, ``s5_trigger_linux.py``, ``life_mngr``, and
   ``life_mngr.service`` into the Service VM and User VM.

   These commands assume you have a network connection between the development
   computer and target system. You can also use a USB stick to transfer files.

   .. code-block:: bash

      scp build/misc/services/s5_trigger_linux.py acrn@<target board address>:~/
      scp build/misc/services/life_mngr acrn@<target board address>:~/
      scp build/misc/services/life_mngr.service acrn@<target board address>:~/
      scp build/misc/services/life_mngr.conf acrn@<target board address>:~/

   Log in to the target system and run the following commands::

      sudo mkdir /etc/life_mngr
      sudo mv ~/life_mngr.conf /etc/life_mngr/
      sudo mv ~/life_mngr.service /lib/systemd/system/
      sudo mv ~/life_mngr /usr/bin/

#. Copy ``user_vm_shutdown.py`` from the development computer into the Service
   VM::

      scp misc/services/life_mngr/user_vm_shutdown.py acrn@<target board address>:~/

#. ACRN code sets the COM2 (``/dev/ttyS1``) as the default communication port of
   the User VM, so we need only check the S5 vUART of the Service VM. Use the
   following steps to get the Service VM S5 connection information.

   Log in to the Service VM and run the command ``cat /etc/serial.conf`` to get
   the connection information between the Service VM and User VM. Output
   example:

   .. code-block:: console

      # User_VM_id: 1
      /dev/ttyS8 port 0X9008 irq 0 uart 16550A baud_base 115200

   This example means the Service VM uses the ``/dev/ttyS8`` connection to the
   User VM's ``/dev/ttyS1``.

#. Configure the S5 feature:

   a. In the Service VM, edit the following options in
      ``/etc/life_mngr/life_mngr.conf``. Make sure ``VM_NAME`` is the Service VM
      name specified in the ACRN Configurator. Replace ``/dev/ttyS8`` with your
      Service VM's S5 vUART, if it was different from the example in the
      previous step.

      .. code-block:: bash

         VM_TYPE=service_vm
         VM_NAME= ACRN_Service_VM
         DEV_NAME=tty:/dev/ttyS8
         ALLOW_TRIGGER_S5=/dev/ttySn

   #. In the User VM, edit the following options in
      ``/etc/life_mngr/life_mngr.conf``. Replace ``<User VM name>`` with the
      VM name specified in the ACRN Configurator.

      .. code-block:: bash

         VM_TYPE=user_vm
         VM_NAME=<User VM name>
         DEV_NAME=tty:/dev/ttyS1
         ALLOW_TRIGGER_S5=/dev/ttySn

#. Enable ``life_mngr.service`` and restart the Service VM and User VM::

      sudo chmod +x /usr/bin/life_mngr
      sudo systemctl enable life_mngr.service
      sudo reboot

#. To trigger a system S5, run ``s5_trigger_linux.py`` in the Service VM.
   The Service VM shuts down (transitioning to the S5 state) and sends a
   poweroff request to shut down the User VM.

.. note::

   The S5 state is not automatically triggered by a Service VM shutdown; you
   need to run ``s5_trigger_linux.py`` in the Service VM.