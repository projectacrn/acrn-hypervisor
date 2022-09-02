# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from cpuparser.platformbase import MSR, msrfield

class MSR_IA32_MISC_ENABLE(MSR):
    addr = 0x1a0
    fast_string = msrfield(0, 0, doc="Fast-strings enable")

    capability_bits = [
        "fast_string",
    ]

class MSR_IA32_FEATURE_CONTROL(MSR):
    addr = 0x03a
    lock = msrfield(0, 0, doc="Lock bit")
    vmx_outside_smx = msrfield(2, 2, doc="Enable VMX outside SMX operation")

    @property
    def disable_vmx(self):
        return self.lock and not self.vmx_outside_smx

    capability_bits = [
        "disable_vmx",
    ]

class VMXCapabilityReportingMSR(MSR):
    def get_field_idx(self, field):
        if isinstance(field, int):
            return field
        if isinstance(field, str):
            return getattr(type(self), field).lsb
        assert False, f"Invalid field type: {field}, {type(field)}"

    def allows_0_setting(self, field):
        field_idx = self.get_field_idx(field)
        if field_idx >= 32:
            return False

        bit_mask = (1 << field_idx)
        return (self.value & bit_mask) == 0

    def allows_1_setting(self, field):
        field_idx = self.get_field_idx(field)
        if field_idx >= 32:
            return False

        bit_mask = (1 << field_idx)
        high = self.value >> 32
        return (high & bit_mask) == bit_mask

    def allows_flexible_setting(self, field):
        field_idx = self.get_field_idx(field)
        return self.allows_0_setting(field_idx) and self.allows_1_setting(field_idx)

class MSR_IA32_VMX_PROCBASED_CTLS2(VMXCapabilityReportingMSR):
    addr = 0x0000048B

    vapic_access = msrfield(0, 0, doc="Virtualize APIC accesses")
    ept = msrfield(1, 1, doc="Enable EPT")
    rdtscp = msrfield(3, 3, doc="Enable RDTSCP")
    vx2apic = msrfield(4, 4, doc="Virtualize x2APIC mode")
    vpid = msrfield(5, 5, doc="Enable VPID")
    unrestricted_guest = msrfield(7, 7, doc="Unrestricted guest")
    apic_reg_virt = msrfield(8, 8, doc="APIC-register virtualization")

    @property
    def vmx_procbased_ctls2_vapic(self):
        return self.allows_flexible_setting("vapic_access")

    @property
    def vmx_procbased_ctls2_ept(self):
        return self.allows_flexible_setting("ept")

    @property
    def vmx_procbased_ctls2_vpid(self):
        return self.allows_flexible_setting("vpid")

    @property
    def vmx_procbased_ctls2_rdtscp(self):
        return self.allows_flexible_setting("rdtscp")

    @property
    def vmx_procbased_ctls2_unrestrict(self):
        return self.allows_flexible_setting("unrestricted_guest")

    capability_bits = [
        "vmx_procbased_ctls2_vapic",
        "vmx_procbased_ctls2_ept",
        "vmx_procbased_ctls2_vpid",
        "vmx_procbased_ctls2_rdtscp",
        "vmx_procbased_ctls2_unrestrict",
    ]

class MSR_IA32_VMX_PINBASED_CTLS(VMXCapabilityReportingMSR):
    addr = 0x00000481

    irq_exiting = msrfield(0, 0, doc="External-interrupt existing")

    @property
    def vmx_pinbased_ctls_irq_exit(self):
        return self.allows_flexible_setting("irq_exiting")

    capability_bits = [
        "vmx_pinbased_ctls_irq_exit",
    ]

class MSR_IA32_VMX_PROCBASED_CTLS(VMXCapabilityReportingMSR):
    addr = 0x00000482

    tsc_offsetting = msrfield(3, 3, doc="Use TSC offsetting")
    hlt_exiting = msrfield(7, 7, doc="HLT exiting")
    tpr_shadow = msrfield(21, 21, doc="Use TPR shadow")
    io_bitmaps = msrfield(25, 25, doc="Use I/O bitmaps")
    msr_bitmaps = msrfield(28, 28, doc="Use MSR bitmaps")
    secondary_ctrls = msrfield(31, 31, doc="Activate secondary controls")

    @property
    def vmx_procbased_ctls_tsc_off(self):
        return self.allows_flexible_setting("tsc_offsetting")

    @property
    def vmx_procbased_ctls_tpr_shadow(self):
        return self.allows_flexible_setting("tpr_shadow")

    @property
    def vmx_procbased_ctls_io_bitmap(self):
        return self.allows_flexible_setting("io_bitmaps")

    @property
    def vmx_procbased_ctls_msr_bitmap(self):
        return self.allows_flexible_setting("msr_bitmaps")

    @property
    def vmx_procbased_ctls_hlt(self):
        return self.allows_flexible_setting("hlt_exiting")

    @property
    def vmx_procbased_ctls_secondary(self):
        return self.allows_flexible_setting("secondary_ctrls")

    @property
    def ept(self):
        if self.allows_1_setting("secondary_ctrls"):
            ctls2 = MSR_IA32_VMX_PROCBASED_CTLS2.rdmsr(self.cpu_id)
            return ctls2.allows_1_setting("ept")
        return False

    @property
    def apicv(self):
        if not self.allows_1_setting("tpr_shadow"):
            return False

        ctls2 = MSR_IA32_VMX_PROCBASED_CTLS2.rdmsr(self.cpu_id)
        return \
            ctls2.allows_1_setting("vapic_access") and \
            ctls2.allows_1_setting("vx2apic")

    capability_bits = [
        "ept",
        "apicv",
        "vmx_procbased_ctls_tsc_off",
        "vmx_procbased_ctls_tpr_shadow",
        "vmx_procbased_ctls_io_bitmap",
        "vmx_procbased_ctls_msr_bitmap",
        "vmx_procbased_ctls_hlt",
        "vmx_procbased_ctls_secondary",
    ]

class MSR_IA32_VMX_EPT_VPID_CAP(MSR):
    addr = 0x0000048C

    invept = msrfield(20, 20, doc="INVEPT instruction supported")
    ept_2mb_page = msrfield(16, 16, doc="EPT 2-Mbyte page supported")
    vmx_ept_1gb_page = msrfield(17, 17, doc="EPT 1-Gbyte page supported")
    invvpid_inst = msrfield(32, 32, doc="INVVPID instruction supported")
    invvpid_single_context = msrfield(41, 41, doc="single-context INVVPID type supported")
    invvpid_all_context = msrfield(42, 42, doc="all-context INVVPID type supported")

    @property
    def invvpid(self):
        return self.invvpid_inst and self.invvpid_single_context and self.invvpid_all_context

    capability_bits = [
        "invept",
        "invvpid",
        "ept_2mb_page",
        "vmx_ept_1gb_page",
    ]

class MSR_IA32_VMX_MISC(MSR):
    addr = 0x00000485
    stores_lma_on_exit = msrfield(5, 5, doc="VM exits stores the value of IA32_EFER.LMA into the 'IA-32e mode guest' VM-entry control")

    capability_bits = [
        "stores_lma_on_exit",
    ]

class MSR_IA32_VMX_BASIC(MSR):
    addr = 0x00000480
    set_32bit_addr_width = msrfield(48, 48, doc="Addresses of VMXON, VMCS and referenced structures limited to 32-bit")

    capability_bits = [
        "set_32bit_addr_width",
    ]

class MSR_IA32_VMX_EXIT_CTLS(VMXCapabilityReportingMSR):
    addr = 0x00000483

    host_addr_size = msrfield(9, 9, doc="Host address-space size")
    ack_irq_on_exit = msrfield(15, 15, doc="Acknowledge interrupt on exit")
    save_pat = msrfield(18, 18, doc="Save IA32_PAT")
    load_pat = msrfield(19, 19, doc="Load IA32_PAT")

    @property
    def vmx_exit_ctls_ack_irq(self):
        return self.allows_flexible_setting("ack_irq_on_exit")

    @property
    def vmx_exit_ctls_save_pat(self):
        return self.allows_flexible_setting("save_pat")

    @property
    def vmx_exit_ctls_load_pat(self):
        return self.allows_flexible_setting("load_pat")

    @property
    def vmx_exit_ctls_host_addr64(self):
        return self.allows_flexible_setting("host_addr_size")

    capability_bits = [
        "vmx_exit_ctls_ack_irq",
        "vmx_exit_ctls_save_pat",
        "vmx_exit_ctls_load_pat",
        "vmx_exit_ctls_host_addr64",
    ]

class MSR_IA32_VMX_ENTRY_CTLS(VMXCapabilityReportingMSR):
    addr = 0x00000484

    ia32e_mode_guest = msrfield(9, 9, doc="IA-32e mode guest")
    load_pat = msrfield(14, 14, doc="Load IA32_PAT")

    @property
    def vmx_entry_ctls_load_pat(self):
        return self.allows_flexible_setting("load_pat")

    @property
    def vmx_entry_ctls_ia32e_mode(self):
        return self.allows_flexible_setting("ia32e_mode_guest")

    capability_bits = [
        "vmx_entry_ctls_load_pat",
        "vmx_entry_ctls_ia32e_mode",
    ]

class MSR_IA32_L3_QOS_CFG(MSR):
    addr = 0x00000c81
    cdp_enable = msrfield(0, 0, doc="L3 CDP enable")

def MSR_IA32_L3_MASK_n(n):
    if n >= 128:
        logging.debug("Attempt to access an out-of-range IA32_L3_MASK_n register. Fall back to 0.")
        n = 0

    class IA32_L3_MASK_n(MSR):
        addr = 0x00000c90 + n
        bit_mask = msrfield(32, 0, doc="Capacity bit mask")

    return IA32_L3_MASK_n

class MSR_IA32_PM_ENABLE(MSR):
    addr = 0x00000770
    hwp_enable = msrfield(0, 0, doc=None)

class MSR_IA32_HWP_CAPABILITIES(MSR):
    addr = 0x00000771
    highest_performance_lvl = msrfield(7, 0, doc=None)
    guaranteed_performance_lvl = msrfield(15, 8, doc=None)
    lowest_performance_lvl = msrfield(31, 24, doc=None)

    attribute_bits = [
        "highest_performance_lvl",
        "guaranteed_performance_lvl",
        "lowest_performance_lvl",
    ]

class MSR_TURBO_RATIO_LIMIT(MSR):
    addr = 0x000001ad
    max_ratio_1core = msrfield(7, 0, doc=None)

    attribute_bits = [
        "max_ratio_1core",
    ]

class MSR_TURBO_ACTIVATION_RATIO(MSR):
    addr = 0x0000064c
    max_none_turbo_ratio = msrfield(7, 0, doc=None)

    attribute_bits = [
        "max_none_turbo_ratio",
    ]
