.. _getting-started-up2:

Getting started guide for UP2 board
###################################

Hardware setup
**************

The `UP Squared board <http://www.up-board.org/upsquared/>`_ (UP2) is
an x86 maker board based on the Intel Apollo Lake platform. The UP boards
are used in IoT applications, industrial automation, digital signage, and more.

The UP2 features Intel `Celeron N3550
<https://ark.intel.com/products/95598/Intel-Celeron-Processor-N3350-2M-Cache-up-to-2_4-GHz>`_
and Intel `Pentium N4200
<https://ark.intel.com/products/95592/Intel-Pentium-Processor-N4200-2M-Cache-up-to-2_5-GHz>`_
SoCs. Both have been confirmed to work with ACRN.

Connecting to the serial port
=============================

The UP2 board has two serial ports. The following figure shows the UP2 board's 
40-pin HAT connector we'll be using as documented in the  `UP2 Datasheet
<https://up-board.org/wp-content/uploads/datasheets/UP-Square-DatasheetV0.5.pdf>`_.

.. image:: images/the-bottom-side-of-UP2-board.png
   :align: center
   
We'll access the serial port through the I/O pins in the 
40-pin HAT connector using a `USB TTL serial cable
<http://www.ftdichip.com/Products/USBTTLSerial.htm>`_, 
and show how to connect a serial port with 
``PL2303TA USB to TTL serial cable`` for example: 

.. image:: images/USB-to-TTL-serial-cable.png
   :align: center

Connect pin 6 (``Ground``), pin 8 (``UART_TXD``) and pin 10 (``UART_RXD``) of the HAT 
connector to respectively the ``GND``, ``RX`` and ``TX`` pins of your 
USB serial cable. Plug the USB TTL serial cable into your PC and use a 
console emulation tool such as ``minicom`` or ``putty`` to communicate 
with the UP2 board for debugging.

.. image:: images/the-connection-of-serial-port.png
   :align: center

Software setup
**************

Setting up the ACRN hypervisor (and associated components) on the UP2
board is no different than other hardware platforms so please follow
the instructions provided in the :ref:`getting-started-apl-nuc`, with
the additional information below.

There are a few parameters specific to the UP2 board that differ from
what is referenced in the :ref:`getting-started-apl-nuc` section:

1. Serial Port settings
#. Storage device name

You will need to keep these in mind in a few places:

* When mounting the EFI System Partition (ESP)

  .. code-block:: none

     # mount /dev/mmcblk0p1 /mnt

* When adjusting the ``acrn.conf`` file

  * Set the ``root=`` parameter using the ``PARTUUID`` or device name directly

* When configuring the EFI firmware to boot the ACRN hypervisor by default

  .. code-block:: none

     # efibootmgr -c -l "\EFI\acrn\acrn.efi" -d /dev/mmcblk0 -p 1 -L "ACRN Hypervisor" \
         -u "bootloader=\EFI\org.clearlinux\bootloaderx64.efi uart=mmio@0x9141e000"

UP2 serial port setting
=======================

The serial port in the 40-pin HAT connector is located at ``MMIO 0x0x9141e000``.
You can check this from the ``dmesg`` output from the initial Clearlinux installation.

.. code-block:: none

   # dmesg | grep dw-apb-uart
   [2.150689] dw-apb-uart.8: ttyS1 at MMIO 0x91420000 (irq = 4, base_baud = 115200) is a 16550A
   [2.152072] dw-apb-uart.9: ttyS2 at MMIO 0x9141e000 (irq = 5, base_baud = 115200) is a 16550A

The second entry associated with ``dw-apb-uart.9`` is the one on the 40-pin HAT connector.

UP2 block device
================

The UP2 board has an on-board eMMC device. The device name to be used
throughout the :ref:`getting_started` therefore is ``/dev/mmcblk0``
(and ``/dev/mmcblk0pX`` for any partition).

The UUID of the partition ``/dev/mmcblk0p3`` can be found by

.. code-block:: none

   # blkid /dev/mmcblk

.. note::
   You can also use the device name directly, e.g.: ``root=/dev/mmcblk0p3``

Running the hypervisor
**********************

Now that the hypervisor and Service OS have been installed on your UP2 board,
you can proceed with the rest of the instructions in the
:ref:`getting-started-apl-nuc` and install the User OS (UOS).
