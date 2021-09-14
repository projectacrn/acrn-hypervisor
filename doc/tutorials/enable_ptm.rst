.. _enable-ptm:

Enable PCIe Precision Time Management in a User VM
##################################################

The PCI Express (PCIe) specification defines a Precision Time Measurement (PTM)
mechanism that lets you coordinate and synchronize events across multiple PCI
components within the same system with very fine time precision.

ACRN adds PCIe root port emulation in the hypervisor to support the PTM feature
and emulates a simple PTM hierarchy.  ACRN enables PTM in a User VM if the user
sets the ``enable_ptm`` option when passing through a device to a post-launched
VM and :ref:`vm.PTM` is enabled in the scenario configuration. When you enable
PTM, the passthrough device is connected to a virtual root port instead of the host
bridge as it normally would.

Before you can enable PTM in a User VM, PTM must be enabled in the Service VM.
You should verify this by using ``dmesg`` in a Service VM terminal::

    dmesg | grep -i ptm
    [    3.072144] pci 0000:00:06.0: PTM enabled (root), 4ns granularity
    [    3.101591] pci 0000:00:1a.0: PTM enabled (root), 4ns granularity
    [    3.103434] pci 0000:00:1c.0: PTM enabled (root), 4ns granularity
    [    5.089042] igc 0000:04:00.0: PTM enabled, 4ns granularity
    [   88.103910] acrn_dm: <PTM>: opt=enable_ptm.
    [   88.217266] acrn_dm: <PTM>-get_ptm_reg_value: device [4:0.0]: ptm pos=0x1f0, ptm reg val=0x401.
    [   88.217566] acrn_dm: <PTM>-get_ptm_reg_value: device [0:1c.0]: ptm pos=0x150, ptm reg val=0x416.
    [   88.217743] acrn_dm: <PTM>-get_ptm_reg_value: device [0:1c.0]: ptm pos=0x150, ptm reg val=0x3.


Here is an example launch script that configures a supported Ethernet card for
passthrough and enables PTM on it:

.. code-block:: bash
   :emphasize-lines: 9-11,17

   declare -A passthru_vpid
   declare -A passthru_bdf
   passthru_vpid=(
    ["ethptm"]="8086 15f2"
    )
   passthru_bdf=(
    ["ethptm"]="0000:a9:00.0"
    )
   echo ${passthru_vpid["ethptm"]} > /sys/bus/pci/drivers/pci-stub/new_id
   echo ${passthru_bdf["ethptm"]} > /sys/bus/pci/devices/${passthru_bdf["ethptm"]}/driver/unbind
   echo ${passthru_bdf["ethptm"]} > /sys/bus/pci/drivers/pci-stub/bind

   acrn-dm -A -m $mem_size -s 0:0,hostbridge \
      -s 3,virtio-blk,uos-test.img \
      -s 4,virtio-net,tap0 \
      -s 5,virtio-console,@stdio:stdio_port \
      -s 6,passthru,a9/00/0,enable_ptm \
      --ovmf /usr/share/acrn/bios/OVMF.fd

.. important:: By default, the :ref:`vm.PTM` option is disabled in ACRN VMs. Use the
    :ref:`ACRN configuration tool <acrn_configuration_tool>` to enable PTM
    in the scenario XML file that configures the Guest VM.

Here is the bus hierarchy in the User VM (as shown by the ``lspci`` command)::

   lspci -tv
   -[0000:00]-+-00.0  Network Appliance Corporation Device 1275
              +-03.0  Red Hat, Inc. Virtio block device
              +-04.0  Red Hat, Inc. Virtio network device
              +-05.0  Red Hat, Inc. Virtio console
              \-06.0-[01]----00.0  Intel Corporation Device 15f2

(Instead of ``Device 15f2`` you might see ``Ethernet Controller I225LM``.)

You can verify that PTM was enabled by using ``dmesg`` in the User VM::

    dmesg | grep -i ptm
    [    0.774547] pci 0000:00:05.0: PTM enabled (root), 4ns granularity
    [    2.478440] igc 0000:01:00.0: PTM enabled, 4ns granularity

PTM Implementation Notes
************************

To simplify the implementation, the virtual root port only supports the most
basic PCIe configuration and operation, in addition to PTM capabilities.

To use PTM in a virtualized environment, you may want to first verify that PTM
is supported by the device and is enabled on the bare metal machine and in the
Guest VM kernel (e.g., ``CONFIG_PCIE_PTM=y`` option is set in the Linux kernel).

You can find more details about the PTM implementation in the
:ref:`ACRN HLD PCIe PTM documentation <PCIe PTM implementation>`.
