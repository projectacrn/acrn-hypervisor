# Copyright (C) 2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

from cpuparser.platformbase import MSR, msrfield

class MSR_IA32_MISC_ENABLE(MSR):
    addr = 0x1a0
    fast_string = msrfield(1, 0, doc=None)

    capability_bits = [
        "fast_string",
    ]

class MSR_IA32_FEATURE_CONTROL(MSR):
    addr = 0x03a
    msr_ia32_feature_control_lock = msrfield(1, 0, doc=None)
    msr_ia32_feature_control_vmx_no_smx = msrfield(1, 2, doc=None)

    @property
    def disable_vmx(self):
        return self.msr_ia32_feature_control_lock and not self.msr_ia32_feature_control_vmx_no_smx

    capability_bits = [
        "disable_vmx",
    ]

class MSR_IA32_VMX_PROCBASED_CTLS2(MSR):
    addr = 0x0000048B

    @property
    def vmx_procbased_ctls2_vapic(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 0)

    @property
    def vmx_procbased_ctls2_ept(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 1)

    @property
    def vmx_procbased_ctls2_vpid(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 5)

    @property
    def vmx_procbased_ctls2_rdtscp(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 3)

    @property
    def vmx_procbased_ctls2_unrestrict(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 7)

    capability_bits = [
        "vmx_procbased_ctls2_vapic",
        "vmx_procbased_ctls2_ept",
        "vmx_procbased_ctls2_vpid",
        "vmx_procbased_ctls2_rdtscp",
        "vmx_procbased_ctls2_unrestrict",
    ]

class MSR_IA32_VMX_PINBASED_CTLS(MSR):
    addr = 0x00000481

    @property
    def vmx_pinbased_ctls_irq_exit(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 0)

    capability_bits = [
        "vmx_pinbased_ctls_irq_exit",
    ]

class MSR_IA32_VMX_PROCBASED_CTLS(MSR):
    addr = 0x00000482

    @property
    def vmx_procbased_ctls_tsc_off(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 3)

    @property
    def vmx_procbased_ctls_tpr_shadow(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 21)

    @property
    def vmx_procbased_ctls_io_bitmap(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 25)

    @property
    def vmx_procbased_ctls_msr_bitmap(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 28)

    @property
    def vmx_procbased_ctls_hlt(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 7)

    @property
    def vmx_procbased_ctls_secondary(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 31)

    @property
    def ept(self):
        is_ept_supported = False
        if ((self.value >> 32) & (1 << 31)) != 0:
            msr_val = MSR_IA32_VMX_PROCBASED_CTLS2.rdmsr(self.cpu_id)
            if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 1):
                is_ept_supported = True
        return is_ept_supported

    @property
    def apicv(self):
        features = 0
        vapic_feature_tpr_shadow = 1 << 3
        vapic_feature_virt_access = 1 << 0
        vapic_feature_vx2apic_mode = 1 << 5
        vapic_feature_virt_reg = 1 << 1
        vapic_feature_intr_delivery = 1 << 2
        vapic_feature_post_intr = 1 << 4

        if msrfield.is_ctrl_setting_allowed(self.value, 1 << 21):
            features |= vapic_feature_tpr_shadow

        msr_val = MSR_IA32_VMX_PROCBASED_CTLS2.rdmsr(self.cpu_id)
        if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 0):
            features |= vapic_feature_virt_access
        if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 4):
            features |= vapic_feature_vx2apic_mode
        if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 8):
            features |= vapic_feature_virt_reg
        if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 9):
            features |= vapic_feature_intr_delivery

        msr_val = MSR_IA32_VMX_PINBASED_CTLS.rdmsr(self.cpu_id)
        if msrfield.is_ctrl_setting_allowed(msr_val.value, 1 << 7):
            features |= vapic_feature_post_intr

        apicv_basic_feature = (vapic_feature_tpr_shadow | vapic_feature_virt_access | vapic_feature_vx2apic_mode)
        return (features & apicv_basic_feature) == apicv_basic_feature

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

    invept = msrfield(1, 20)
    ept_2mb_page = msrfield(1, 16)
    vmx_ept_1gb_page = msrfield(1, 17)
    invvpid = msrfield(1, 32) and msrfield(1, 41) and msrfield(1, 42)

    capability_bits = [
        "invept",
        "invvpid",
        "ept_2mb_page",
        "vmx_ept_1gb_page",
    ]

class MSR_IA32_VMX_MISC(MSR):
    addr = 0x00000485
    unrestricted_guest = msrfield(1, 5)

    capability_bits = [
        "unrestricted_guest",
    ]

class MSR_IA32_VMX_BASIC(MSR):
    addr = 0x00000480
    set_32bit_addr_width = msrfield(1, 48)

    capability_bits = [
        "set_32bit_addr_width",
    ]

class MSR_IA32_VMX_EXIT_CTLS(MSR):
    addr = 0x00000483

    @property
    def vmx_exit_ctls_ack_irq(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 15)

    @property
    def vmx_exit_ctls_save_pat(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 18)

    @property
    def vmx_exit_ctls_load_pat(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 19)

    @property
    def vmx_exit_ctls_host_addr64(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 9)

    capability_bits = [
        "vmx_exit_ctls_ack_irq",
        "vmx_exit_ctls_save_pat",
        "vmx_exit_ctls_load_pat",
        "vmx_exit_ctls_host_addr64",
    ]

class MSR_IA32_VMX_ENTRY_CTLS(MSR):
    addr = 0x00000484

    @property
    def vmx_entry_ctls_load_pat(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 14)

    @property
    def vmx_entry_ctls_ia32e_mode(self):
        return msrfield.is_vmx_cap_supported(self, 1 << 9)

    capability_bits = [
        "vmx_entry_ctls_load_pat",
        "vmx_entry_ctls_ia32e_mode",
    ]
