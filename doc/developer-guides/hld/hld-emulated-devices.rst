.. _hld-emulated-devices:

Emulated Devices High-Level Design
##################################

Full virtualization device models can typically
reuse existing native device drivers to avoid implementing front-end
drivers. ACRN implements several fully virtualized devices, as
documented in this section.

.. toctree::
   :maxdepth: 1

   usb-virt-hld
   UART Virtualization <uart-virt-hld>
   Watchdog Virtualization <watchdog-hld>
   AHCI Virtualization <ahci-hld>
   System Timer Virtualization <system-timer-hld>
   UART Emulation in Hypervisor <vuart-virt-hld>
   RTC Emulation in Hypervisor <rtc-virt-hld>
   hostbridge-virt-hld
   AT Keyboard Controller Emulation <atkbdc-virt-hld>
   Split Device Model <split-dm>
   Shared Memory Based Inter-VM Communication <ivshmem-hld>
