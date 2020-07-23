.. _mmio-device-passthrough:

MMIO Device Passthrough
########################

The ACRN Hypervisor supports both PCI and MMIO device passthrough.
However there are some constraints on and hypervisor assumptions about
MMIO devices: there can be no DMA access to the MMIO device and the MMIO
device may not use IRQs.

Here is how ACRN supports MMIO device passthrough:

* For a pre-launched VM, the VM configuration tells the ACRN hypervisor
  the addresses of the physical MMIO device's regions and where they are
  mapped to in the pre-launched VM.  The hypervisor then removes these
  MMIO regions from the Service VM and fills the vACPI table for this MMIO
  device based on the device's physical ACPI table.

* For a post-launched VM, the same actions are done as in a
  pre-launched VM, plus we use the command line to tell which MMIO
  device we want to pass through to the post-launched VM.

  If the MMIO device has ACPI Tables, use ``--acpidev_pt HID`` and
  if not, use ``--mmiodev_pt MMIO_regions``.

.. note::
   Currently, the vTPM and PT TPM in the ACRN-DM have the same HID so we
   can't support them both at the same time. The VM will fail to boot if
   both are used.

These issues remain to be implemented:

* Save the MMIO regions in a field of the VM structure in order to
  release the resources when the post-launched VM shuts down abnormally.
* Allocate the guest MMIO regions for the MMIO device in a guest-reserved
  MMIO region instead of being hard-coded. With this, we could add more
  passthrough MMIO devices.
* De-assign the MMIO device from the Service VM first before passing
  through it to the post-launched VM and not only removing the MMIO
  regions from the Service VM.
