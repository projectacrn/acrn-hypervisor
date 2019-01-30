.. _static_ip:

Using a static IP address
#########################

When you install ACRN on your system following the :ref:`getting_started`, a
bridge called ``acrn-br0`` will be created and attached to the Ethernet network
interface of the platform. By default, the bridge gets its network configuration
using DHCP. This guide will explain how to modify the system to use a static IP
address. You need ``root`` privileges to make these changes to the system.

ACRN Network Setup
******************

The ACRN Service OS is based on `Clear Linux OS`_ and it uses `systemd-networkd`_
to set up the Service OS networking. A few files are responsible for setting up the
ACRN bridge (``acrn-br0``), the TAP device (``acrn_tap0``), and how these are all
connected. Those files are installed in ``/usr/lib/systemd/network``
on the target device and can also be found under ``tools/acrnbridge`` in the source code.

Setting up the static IP address
********************************

You can set up a static IP address by copying the
``/usr/lib/systemd/network/50-eth.network`` file to
``/etc/systemd/network/`` directory. You can create this directory and
copy the file with the following command:

.. code-block:: none

   mkdir -p /etc/systemd/network
   cp /usr/lib/systemd/network/50-eth.network /etc/systemd/network

Modify the ``[Network]`` section in the
``/etc/systemd/network/50-eth.network`` file you just created.
This is the content of the file used in ACRN by default.

.. literalinclude:: ../../tools/acrnbridge/eth.network
   :caption: tools/acrnbridge/eth.network
   :emphasize-lines: 5

Edit the file to remove the line highlighted above and add your network settings in
that ``[Network]`` section. You will typically need to add the ``Address=``, ``Gateway=``
and ``DNS=`` parameters in there. There are many more parameters that can be used and
detailing them is beyond the scope of this document. For an extensive list of those,
please visit the official `systemd-network`_ page.

This is an example of what a typical ``[Network]`` section would look like, specifying
a static IP address:

.. code-block:: none

   [Network]
   Address=192.168.1.87/24
   Gateway=192.168.1.254
   DNS=192.168.1.254

Activate the new configuration
******************************

You do not need to reboot the machine after making the changes to the system, the
following steps that restart the ``systemd-networkd`` service will suffice (run as ``root``):

.. code-block:: none

   systemctl daemon-reload
   systemctl restart systemd-networkd

If you encounter connectivity issues after following this guide, please contact us on the
`ACRN-users mailing list`_ or file an issue in `ACRN hypervisor issues`_. Provide the details
of the configuration you are trying to set up, the modifications you have made to your system, and
the output of ``journalctl -b -u systemd-networkd`` so we can best assist you.

.. _systemd-networkd: https://www.freedesktop.org/software/systemd/man/systemd-networkd.service.html
.. _Clear Linux OS: https://clearlinux.org
.. _systemd-network: https://www.freedesktop.org/software/systemd/man/systemd.network.html
.. _ACRN-users mailing list: https://lists.projectacrn.org/g/acrn-users
.. _ACRN hypervisor issues: https://github.com/projectacrn/acrn-hypervisor/issues
