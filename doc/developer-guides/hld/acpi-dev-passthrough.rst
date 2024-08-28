.. _acpi-device-passthrough:

ACPI Device Passthrough
########################

The ACRN Hypervisor supports ACPI device passthrough with MMIO, PIO and
IRQ resource.

Here is how ACRN supports ACPI device passthrough:

* Before passthrough, to ensure those ACPI devices not being accessd by
  Service VM after assigned to post launched VM, launch script should add
  command to unbind device instance with driver's unbind interface according
  to capability of different ACPI device drivers before launching VMs.
  For TPM, UART and GPIO controller, all their drivers provide such unbind
  interface under sysfs node.

* For MMIO resource, we use the command line to tell the ACRN hypervisor
  the addresses of physical ACPI device's MMIO regions and where they are
  mapped to in the post-launched VM. The hypervisor then remove these
  MMIO regions from the Service VM and fills the vACPI table for this ACPI
  device.

* For PIO resource, we use the command line to tell the ACRN hypervisor
  the addresses of physical ACPI device's PIO regions and they will be
  identically mapped in the post-launched VM. The hypervisor then remove
  these PIO regions from the Service VM and fills the vACPI table for this
  ACPI device.

* For IRQ resource, we use the command line to tell the ACRN hypervisor
  the addresses of physical ACPI device's IRQ numbers and they will be
  identically mapped in the post-launched VM, as all passthrough-supported
  ACPI devices use IRQs within number 0-15 whose usage are commonly accepted.
  The hypervisor then remove these IRQ mapping from Service VM and fills the
  vACPI table specifying polarity and trigger mode of interrupt as options
  for this ACPI device.

Supported ACPI devices are TPM, UART and GPIO controller.

.. note::
   The vTPM and PT TPM in the ACRN-DM have the same HID so we
   can't support them both at the same time. The VM will fail to boot if
   both are used.
