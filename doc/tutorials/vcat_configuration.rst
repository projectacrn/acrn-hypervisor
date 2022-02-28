.. _vcat_configuration:

Enable vCAT Configuration
#########################

vCAT is built on top of RDT, so to use vCAT we must first enable RDT.
For details on enabling RDT configuration on ACRN, see :ref:`rdt_configuration`.
For details on ACRN vCAT high-level design, see :ref:`hv_vcat`.

The vCAT feature is disabled by default in ACRN. You can enable vCAT via the UI,
the steps listed below serve as an FYI to show how those settings are translated
into XML in the scenario file:

#. Configure system level features:

   - Edit :option:`hv.FEATURES.RDT.RDT_ENABLED` to `y` to enable RDT

   - Edit :option:`hv.FEATURES.RDT.CDP_ENABLED` to `n` to disable CDP.
     Currently vCAT requires CDP to be disabled.

   - Edit :option:`hv.FEATURES.RDT.VCAT_ENABLED` to `y` to enable vCAT

     .. code-block:: xml
        :emphasize-lines: 3,4,5

        <FEATURES>
            <RDT>
                <RDT_ENABLED>y</RDT_ENABLED>
                <CDP_ENABLED>n</CDP_ENABLED>
                <VCAT_ENABLED>y</VCAT_ENABLED>
                <CLOS_MASK></CLOS_MASK>
            </RDT>
        </FEATURES>

#. In each Guest VM configuration:

   - Edit :option:`vm.virtual_cat_support` to 'y' to enable the vCAT feature on the VM.

   - Edit :option:`vm.clos.vcpu_clos` to assign COS IDs to the VM.

     If ``GUEST_FLAG_VCAT_ENABLED`` is not specified for a VM (abbreviated as RDT VM):
     ``vcpu_clos`` is per CPU in a VM and it configures each CPU in a VM to a desired COS ID.
     So the number of vcpu_closes is equal to the number of vCPUs assigned.

     If ``GUEST_FLAG_VCAT_ENABLED`` is specified for a VM (abbreviated as vCAT VM):
     ``vcpu_clos`` is not per CPU anymore; instead, it specifies a list of physical COS IDs (minimum 2)
     that are assigned to a vCAT VM. The number of vcpu_closes is not necessarily equal to
     the number of vCPUs assigned, but may be not only greater than the number of vCPUs assigned but
     less than this number. Each vcpu_clos will be mapped to a virtual COS ID, the first vcpu_clos
     is mapped to virtual COS ID 0 and the second is mapped to virtual COS ID 1, etc.

     .. code-block:: xml
        :emphasize-lines: 3,10,11,12,13

        <vm id="1">
          <guest_flags>
            <guest_flag>GUEST_FLAG_VCAT_ENABLED</guest_flag>
          </guest_flags>
          <cpu_affinity>
              <pcpu_id>1</pcpu_id>
              <pcpu_id>2</pcpu_id>
          </cpu_affinity>
          <clos>
            <vcpu_clos>2</vcpu_clos>
            <vcpu_clos>4</vcpu_clos>
            <vcpu_clos>5</vcpu_clos>
            <vcpu_clos>7</vcpu_clos>
          </clos>
        </vm>

     .. note::
        CLOS_MASK defined in scenario file is a capacity bitmask (CBM) starting
        at bit position low (the lowest assigned physical cache way) and ending at position
        high (the highest assigned physical cache way, inclusive). As CBM only allows
        contiguous '1' combinations, so CLOS_MASK essentially is the maximum CBM that covers
        all the physical cache ways assigned to a vCAT VM.

        The config tool imposes oversight to prevent any problems with invalid configuration data for vCAT VMs:

        * For a vCAT VM, its vcpu_closes cannot be set to 0, COS ID 0 is reserved to be used only by hypervisor

        * There should not be any COS ID overlap between a vCAT VM and any other VMs. e.g. the vCAT VM has exclusive use of the assigned COS IDs

        * For a vCAT VM, each vcpu_clos must be less than L2/L3 COS_MAX

        * For a vCAT VM, its vcpu_closes cannot contain duplicate values

#. Follow instructions in :ref:`gsg` and build with this XML configuration.
