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
   UART virtualization <uart-virt-hld>
   Watchdog virtualization <watchdog-hld>
   AHCI virtualization <ahci-hld>
   System timer virtualization <system-timer-hld>
   UART emulation in hypervisor <vuart-virt-hld>
   RTC emulation in hypervisor <rtc-virt-hld>
   Hostbridge emulation <hostbridge-virt-hld>
   AT keyboard controller emulation <atkbdc-virt-hld>
   Split Device Model <split-dm>
   Shared memory based inter-VM communication <ivshmem-hld>
