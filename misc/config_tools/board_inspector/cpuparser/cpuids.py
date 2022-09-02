# Copyright (C) 2021-2022 Intel Corporation.
#
# SPDX-License-Identifier: BSD-3-Clause
#

"""CPUID register decoding."""

from cpuparser.platformbase import CPUID, cpuidfield
import struct

EAX = 0
EBX = 1
ECX = 2
EDX = 3

class LEAF_0(CPUID):
    """Basic CPUID information including vendor and max supported basic leaf."""

    leaf = 0x0
    max_leaf = cpuidfield(EAX, 31, 0, doc="Highest value the CPUID recognizes for returning basic processor information")

    @property
    def cpuid_level(self):
        return hex(self.regs.eax)

    @property
    def vendor(self):
        """Vendor identification string"""
        return struct.pack('III', self.regs.ebx, self.regs.edx, self.regs.ecx)

    attribute_bits = [
        "cpuid_level",
    ]

class LEAF_1(CPUID):
    """Basic CPUID Information

    Contains version type, family, model, and stepping ID; brand index; CLFLUSH
    line size; maximum number of addressable IDs for logical processors in the
    physical package; initial APIC ID; and feature information"""

    leaf = 0x1

    stepping = cpuidfield(EAX, 3, 0, doc="Stepping ID")
    model = cpuidfield(EAX, 7, 4, doc="Model")
    family = cpuidfield(EAX, 11, 8, doc="Family ID")
    processor_type = cpuidfield(EAX, 13, 12, doc="Processor Type")
    ext_model = cpuidfield(EAX, 19, 16, doc="Extended Model ID")
    ext_family = cpuidfield(EAX, 27, 20, doc="Extended Family ID")

    brand_index = cpuidfield(EBX, 7, 0, doc="Brand index")
    CLFLUSH_line_size = cpuidfield(EBX, 15, 8, doc="CLFLUSH instruction cache line size (in 8-byte words)")
    max_logical_processor_ids = cpuidfield(EBX, 23, 16, doc="The maximum number of addressable IDs for logical processors in the physical package.")
    initial_apic_id = cpuidfield(EBX, 31, 24, doc="Initial APIC ID")

    sse3 = cpuidfield(ECX, 0, 0)
    pclmulqdq = cpuidfield(ECX, 1, 1)
    dtes64 = cpuidfield(ECX, 2, 2)
    monitor = cpuidfield(ECX, 3, 3)
    ds_cpl = cpuidfield(ECX, 4, 4)
    vmx = cpuidfield(ECX, 5, 5)
    smx = cpuidfield(ECX, 6, 6)
    est = cpuidfield(ECX, 7, 7)
    tm2 = cpuidfield(ECX, 8, 8)
    ssse3 = cpuidfield(ECX, 9, 9)
    cnxt_id = cpuidfield(ECX, 10, 10)
    sdbg = cpuidfield(ECX, 11, 11)
    fma = cpuidfield(ECX, 12, 12)
    cmpxchg16b = cpuidfield(ECX, 13, 13)
    xtpr = cpuidfield(ECX, 14, 14)
    pdcm = cpuidfield(ECX, 15, 15)
    pcid = cpuidfield(ECX, 17, 17)
    dca = cpuidfield(ECX, 18, 18)
    sse4_1 = cpuidfield(ECX, 19, 19)
    sse4_2 = cpuidfield(ECX, 20, 20)
    x2apic = cpuidfield(ECX, 21, 21)
    movbe = cpuidfield(ECX, 22, 22)
    popcnt = cpuidfield(ECX, 23, 23)
    tsc_deadline = cpuidfield(ECX, 24, 24)
    aes = cpuidfield(ECX, 25, 25)
    xsave = cpuidfield(ECX, 26, 26)
    osxsave = cpuidfield(ECX, 27, 27)
    avx = cpuidfield(ECX, 28, 28)
    f16c = cpuidfield(ECX, 29, 29)
    rdrand = cpuidfield(ECX, 30, 30)
    hypervisor = cpuidfield(ECX, 31, 31)

    fpu = cpuidfield(EDX, 0, 0)
    vme = cpuidfield(EDX, 1, 1)
    de = cpuidfield(EDX, 2, 2)
    pse = cpuidfield(EDX, 3, 3)
    tsc = cpuidfield(EDX, 4, 4)
    msr = cpuidfield(EDX, 5, 5)
    pae = cpuidfield(EDX, 6, 6)
    mce = cpuidfield(EDX, 7, 7)
    cx8 = cpuidfield(EDX, 8, 8)
    apic = cpuidfield(EDX, 9, 9)
    sep = cpuidfield(EDX, 11, 11)
    mtrr = cpuidfield(EDX, 12, 12)
    pge = cpuidfield(EDX, 13, 13)
    mca = cpuidfield(EDX, 14, 14)
    cmov = cpuidfield(EDX, 15, 15)
    pat = cpuidfield(EDX, 16, 16)
    pse36 = cpuidfield(EDX, 17, 17)
    psn = cpuidfield(EDX, 18, 18)
    clfsh = cpuidfield(EDX, 19, 19)
    ds = cpuidfield(EDX, 21, 21)
    acpi = cpuidfield(EDX, 22, 22)
    mmx = cpuidfield(EDX, 23, 23)
    fxsr = cpuidfield(EDX, 24, 24)
    sse = cpuidfield(EDX, 25, 25)
    sse2 = cpuidfield(EDX, 26, 26)
    ss = cpuidfield(EDX, 27, 27)
    htt = cpuidfield(EDX, 28, 28)
    tm = cpuidfield(EDX, 29, 29)
    pbe = cpuidfield(EDX, 31, 31)

    @property
    def display_family(self):
        if self.family == 0xf:
            return self.ext_family + self.family
        return self.family

    @property
    def display_model(self):
        if self.family == 0xf or self.family == 0x6:
            return (self.ext_model << 4) + self.model
        return self.model

    capability_bits = [
        "sse3",
        "pclmulqdq",
        "dtes64",
        "monitor",
        "ds_cpl",
        "vmx",
        "smx",
        "est",
        "tm2",
        "ssse3",
        "cnxt_id",
        "sdbg",
        "fma",
        "cmpxchg16b",
        "xtpr",
        "pdcm",
        "pcid",
        "dca",
        "sse4_1",
        "sse4_2",
        "x2apic",
        "movbe",
        "popcnt",
        "tsc_deadline",
        "aes",
        "xsave",
        "avx",
        "f16c",
        "rdrand",
        "fpu",
        "vme",
        "de",
        "pse",
        "tsc",
        "msr",
        "pae",
        "mce",
        "cx8",
        "apic",
        "sep",
        "mtrr",
        "pge",
        "mca",
        "cmov",
        "pat",
        "pse36",
        "psn",
        "clfsh",
        "ds",
        "acpi",
        "mmx",
        "fxsr",
        "sse",
        "sse2",
        "ss",
        "htt",
        "tm",
        "pbe",
    ]

class LEAF_2(CPUID):
    """TLB, Cache, and Prefetch Information"""

    leaf = 0x2
    times_to_run = cpuidfield(EAX, 7, 0, doc="Number of times CPUID must be executed with EAX = 2 to retrieve a complete description of the processor's TLB, Cache, and Prefetch hardware")

class LEAF_4(CPUID):
    """Deterministic cache parameters

    Returns encoded data that describes a set of deterministic cache parameters
    for the cache level associated in ECX"""

    leaf = 0x4
    cache_type = cpuidfield(EAX, 4, 0, doc="Cache Type Field")
    cache_level = cpuidfield(EAX, 7, 5, doc="Cache Level")
    self_initializing = cpuidfield(EAX, 8, 8, doc="Self Initializing Cache Level")
    fully_associative = cpuidfield(EAX, 9, 9, doc="Fully Associative Cache")
    max_logical_processors_sharing_cache_z = cpuidfield(EAX, 25, 14, doc="Max number of addressable IDs for logical processors sharing this cache (zero based)")
    max_cores_sharing_cache_z = cpuidfield(EAX, 31, 26, doc="Max number of addressable IDs for processor cores in the physical package (zero based)")

    line_size_z = cpuidfield(EBX, 11, 0, doc="System Coherency Line Size (zero-based)")
    partitions_z = cpuidfield(EBX, 21, 12, doc="Physical Line Partitions (zero-based)")
    ways_z = cpuidfield(EBX, 31, 22, doc="Ways of associativity (zero-based)")

    sets_z = cpuidfield(ECX, 31, 0, doc="Sets (zero-based)")

    write_back_invalidate = cpuidfield(EDX, 0, 0, doc="Write-back Invalidate/Invalidate")
    cache_inclusiveness = cpuidfield(EDX, 1, 1, doc="Cache Inclusiveness")
    complex_cache_indexing = cpuidfield(EDX, 2, 2, doc="Complex Cache indexing")

    @property
    def max_logical_processors_sharing_cache(self):
        """Maximum number of addressable IDs for logical processors sharing this cache"""
        return self.max_logical_processors_sharing_cache_z + 1

    @property
    def max_cores_sharing_cache(self):
        """Maximum number of addressable IDs for processor cores in the physical pacakge"""
        return self.max_cores_sharing_cache_z + 1

    @property
    def partitions(self):
        """Number of physical line partitions"""
        return self.partitions_z + 1

    @property
    def line_size(self):
        """System Coherency line size"""
        return self.line_size_z + 1

    @property
    def ways(self):
        """Ways of associativity"""
        return self.ways_z + 1

    @property
    def sets(self):
        """Number of sets"""
        return self.sets_z + 1

    @property
    def cache_size(self):
        """Cache size in bytes"""
        return self.ways * self.partitions * self.line_size * self.sets

class LEAF_5(CPUID):
    """MONITOR/MWAIT Leaf

    Returns information about features available to MONITOR/MWAIT instructions"""

    leaf = 0x5

    smallest_monitor_line_size = cpuidfield(EAX, 15, 0, doc="Smallest monitor-line size in bytes")

    largest_monitor_line_size = cpuidfield(EBX, 15, 0, doc="Largest monitor-line size in bytes")

    monitor_mwait_supported = cpuidfield(ECX, 0, 0, doc="Enumeration of MONITOR/MWAIT extensions supported")
    interrupt_break_event_supported = cpuidfield(ECX, 1, 1, doc="Supports treating interrupts as break-events for MWAIT, even when interrupts disabled")

    c0 = cpuidfield(EDX, 3, 0, doc="Number of C0 sub C-states supported using MWAIT")
    c1 = cpuidfield(EDX, 7, 4, doc="Number of C1 sub C-states supported using MWAIT")
    c2 = cpuidfield(EDX, 11, 8, doc="Number of C2 sub C-states supported using MWAIT")
    c3 = cpuidfield(EDX, 15, 12, doc="Number of C3 sub C-states supported using MWAIT")
    c4 = cpuidfield(EDX, 19, 16, doc="Number of C4 sub C-states supported using MWAIT")

class LEAF_6(CPUID):
    """Thermal and Power Management leaf

    Returns information about the maximum input values for sub-leaves that contain extended feature flags."""

    leaf = 0x6

    digital_temperature_sensor_supported = cpuidfield(EAX, 0, 0, doc = "Digital temperature sensor is supported if set")
    turbo_boost_available = cpuidfield(EAX, 1, 1, doc = "Intel Turbo Boost technology available")
    arat_supported = cpuidfield(EAX, 2, 2, doc = "APIC-Timer-always-running feature is supported if set")
    pln_supported = cpuidfield(EAX, 4, 4, doc = "Power limit notification controls are supported if set")
    ecmd_supported = cpuidfield(EAX, 5, 5, doc = "Clock modulation duty cycle extension is supported if set")
    package_thermal_management_supported = cpuidfield(EAX, 6, 6, doc = "Package thermal management is supported if set")
    hwp_supported = cpuidfield(EAX, 7, 7, doc = "HWP base registers (IA32_PM_ENABLE[bit 0], IA32_HWP_CAPABILITIES, IA32_HWP_REQUEST, IA32_HWP_STATUS) are supported if set")
    hwp_notification = cpuidfield(EAX, 8, 8, doc = "HWP_Notification. IA32_HWP_INTERRUPT MSR is supported if set.")
    hwp_activity_window = cpuidfield(EAX, 9, 9, doc = "HWP_Activity_Window. IA32_HWP_REQUEST[bits 41:32] is supported if set.")
    hwp_energy_performance_preference = cpuidfield(EAX, 10, 10, doc = "HWP_Energy_Performance_Preference. IA32_HWP_REQUEST[bits 31:24] is supported if set.")
    hwp_package_level_request = cpuidfield(EAX, 11, 11, doc = "HWP_Package_Level_Request. IA32_HWP_REQUEST_PKG MSR is supported if set.")
    hdc = cpuidfield(EAX, 13, 13, doc = "HDC. HDC base registers IA32_PKG_HDC_CTL, IA32_PM_CTL1, IA32_THREAD_STALL MSRs are supported if set.")
    turbo_boost_30 = cpuidfield(EAX, 14, 14, doc = "Intel® Turbo Boost Max Technology 3.0 available.")
    hwp_capabilities = cpuidfield(EAX, 15, 15, doc = "HWP Capabilities. Highest Performance change is supported if set.")
    hwp_peci_override = cpuidfield(EAX, 16, 16, doc = "HWP PECI override is supported if set.")
    flexible_hwp = cpuidfield(EAX, 17, 17, doc = "Flexible HWP is supported if set.")
    fast_hwp_request = cpuidfield(EAX, 18, 18, doc = "Fast access mode for the IA32_HWP_REQUEST MSR is supported if set.")
    hw_feedback = cpuidfield(EAX, 19, 19, doc = "HW_FEEDBACK. IA32_HW_FEEDBACK_PTR MSR, IA32_HW_FEEDBACK_CONFIG MSR, IA32_PACKAGE_THERM_STATUS MSR bit 26, and IA32_PACKAGE_THERM_INTERRUPT MSR bit 25 are supported if set.")
    ignoring_idle_hwp = cpuidfield(EAX, 20, 20, doc = "Ignoring Idle Logical Processor HWP request is supported if set.")
    thread_director = cpuidfield(EAX, 23, 23, doc = "Intel® Thread Director supported if set. IA32_HW_FEEDBACK_CHAR and IA32_HW_FEEDBACK_THREAD_CONFIG MSRs are supported if set.")

    num_interrupt_thresholds = cpuidfield(EBX, 3, 0, doc="Number of interrupt thresholds in digital thermal sensor")

    hardware_coordination_feedback_capability = cpuidfield(ECX, 0, 0, doc="Hardware coordination feedback capability")
    performance_energy_bias = cpuidfield(ECX, 3, 3, doc="Performance-energy bias preference support")
    num_thread_director_classes = cpuidfield(ECX, 15, 8, "Number of Intel® Thread Director classes supported by the processor. Information for that many classes is written into the Intel Thread Director Table by the hardware.")

    hardware_feedback_interface_bitmap = cpuidfield(EDX, 7, 0, doc = "Bitmap of supported hardware feedback interface capabilities.")
    hardware_feedback_interface_structure_size = cpuidfield(EDX, 11, 8, doc = "Enumerates the size of the hardware feedback interface structure in number of 4 KB pages.")
    hardware_feedback_index = cpuidfield(EDX, 31, 16, doc = "Index (starting at 0) of this logical processor's row in the hardware feedback interface structure.")

    capability_bits = [
        "digital_temperature_sensor_supported",
        "turbo_boost_available",
        "arat_supported",
        "pln_supported",
        "ecmd_supported",
        "package_thermal_management_supported",
        "hwp_supported",
        "hwp_notification",
        "hwp_activity_window",
        "hwp_energy_performance_preference",
        "hwp_package_level_request",
        "hdc",
        "turbo_boost_30",
        "hwp_capabilities",
        "hwp_peci_override",
        "flexible_hwp",
        "fast_hwp_request",
        "hw_feedback",
        "ignoring_idle_hwp",
        "thread_director",
        "num_interrupt_thresholds",
        "hardware_coordination_feedback_capability",
        "performance_energy_bias",
        "num_thread_director_classes",
        "hardware_feedback_interface_bitmap",
        "hardware_feedback_interface_structure_size",
        "hardware_feedback_index",
        "digital_temperature_sensor_supported",
        "turbo_boost_available",
        "arat_supported",
        "pln_supported",
        "ecmd_supported",
        "package_thermal_management_supported",
        "hwp_supported",
        "hwp_notification",
        "hwp_activity_window",
        "hwp_energy_performance_preference",
        "hwp_package_level_request",
        "hdc",
        "turbo_boost_30",
        "hwp_capabilities",
        "hwp_peci_override",
        "flexible_hwp",
        "fast_hwp_request",
        "hw_feedback",
        "ignoring_idle_hwp",
        "thread_director",
        "num_interrupt_thresholds",
        "hardware_coordination_feedback_capability",
        "performance_energy_bias",
        "num_thread_director_classes",
        "hardware_feedback_interface_bitmap",
        "hardware_feedback_interface_structure_size",
        "hardware_feedback_index",
    ]

class LEAF_7(CPUID):
    """Structured Extended Feature Flags Enumeration Leaf

    Returns information about the maximum input value for sub-leaves that contain
    extended feature flags"""

    leaf = 0x7

    max_input_values = cpuidfield(EAX, 31, 0, doc="Reports the maximum input value for supported leaf 7 sub-leaves")

    fsgsbase = cpuidfield(EBX, 0, 0, doc="Supports RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE if 1")
    ia32_tsc_adjust_msr = cpuidfield(EBX, 1, 1, doc="IA32_TSC_ADJUST MSR is supported if 1")
    sgx = cpuidfield(EBX, 2, 2, doc="Supports Intel® Software Guard Extensions (Intel® SGX Extensions) if 1")
    bmi1 = cpuidfield(EBX, 3, 3)
    hle = cpuidfield(EBX, 4, 4)
    avx2 = cpuidfield(EBX, 5, 5)
    fdp_excptn_only = cpuidfield(EBX, 6, 6, doc="x87 FPU Data Pointer updated only on x87 exceptions if 1")
    smep = cpuidfield(EBX, 7, 7, doc="Supports Supervisor Mode Execution Protection if 1")
    bmi2 = cpuidfield(EBX, 8, 8)
    erms = cpuidfield(EBX, 9, 9, doc="Supports Enhanced REP MOVSB/STOSB if 1")
    invpcid = cpuidfield(EBX, 10, 10, doc="Supports INVPCID instruction for system software that manages process-context identifiers if 1")
    rtm = cpuidfield(EBX, 11, 11)
    qm = cpuidfield(EBX, 12, 12, doc="Supports Quality of Service Monitoring capability if 1")
    deprecate_fpu = cpuidfield(EBX, 13, 13, doc="Deprecates FPS CS and FPU DS values if 1")
    mpx = cpuidfield(EBX, 14, 14, doc="Supports Intel® Memory Protection Extensions if 1")
    rdt_a = cpuidfield(EBX, 15, 15, doc="Supports Intel® Resource Director Technology (Intel® RDT) Allocation capability if 1")
    avx512f = cpuidfield(EBX, 16, 16)
    avx512dq = cpuidfield(EBX, 17, 17)
    rdseed = cpuidfield(EBX, 18, 18)
    adx = cpuidfield(EBX, 19, 19)
    smap = cpuidfield(EBX, 20, 20, doc="Supports Supervisor-Mode Access Prevention (and the CLAC/STAC instructions) if 1")
    avx512_ifma = cpuidfield(EBX, 21, 21)
    clflushopt = cpuidfield(EBX, 23, 23)
    clwb = cpuidfield(EBX, 24, 24)
    intel_pt = cpuidfield(EBX, 25, 25)
    avx512pf = cpuidfield(EBX, 26, 26)
    avx512er = cpuidfield(EBX, 27, 27)
    avx512cd = cpuidfield(EBX, 28, 28)
    sha = cpuidfield(EBX, 29, 29, doc="Supports Intel® Secure Hash Algorithm Extensions (Intel® SHA Extensions) if 1")
    avx512bw = cpuidfield(EBX, 30, 30)
    avx512vl = cpuidfield(EBX, 31, 31)

    prefetchwt1 = cpuidfield(ECX, 0, 0)
    avx512_vbmi = cpuidfield(ECX, 1, 1)
    umip = cpuidfield(ECX, 2, 2, doc="Supports user-mode instruction prevention if 1")
    pku = cpuidfield(ECX, 3, 3, doc="Supports protection keys for user-mode pages if 1")
    ospke = cpuidfield(ECX, 4, 4, doc="If 1, OS has set CR4.PKE to enable protection keys (and the RDPKRU/WRPKRU instructions)")
    waitpkg = cpuidfield(ECX, 5, 5)
    avx512_vbmi2 = cpuidfield(ECX, 6, 6)
    cet_ss = cpuidfield(ECX, 7, 7, doc="Supports CET shadow stack features if 1")
    gfni = cpuidfield(ECX, 8, 8)
    vaes = cpuidfield(ECX, 9, 9)
    vpclmulqdq = cpuidfield(ECX, 10, 10)
    avx512_vnni = cpuidfield(ECX, 11, 11)
    avx512_bitalg = cpuidfield(ECX, 12, 12)
    tme_en = cpuidfield(ECX, 13, 13)
    avx512_vpopcntdq = cpuidfield(ECX, 14, 14)
    la57 = cpuidfield(ECX, 16, 16, doc="Supports 57-bit linear addresses and five-level paging if 1")
    mawau = cpuidfield(ECX, 21, 17, doc="The value of MAWAU used by the BNDLDX and BNDSTX instructions in 64-bit mode")
    rdpid = cpuidfield(ECX, 22, 22, doc="RDPID and IA32_TSC_AUX are available if 1")
    kl = cpuidfield(ECX, 23, 23, doc="Supports Key Locker if 1")
    cldemote = cpuidfield(ECX, 25, 25, doc="Supports cache line demote if 1")
    movdiri = cpuidfield(ECX, 27, 27, doc="Supports MOVDIRI if 1")
    movdiri64b = cpuidfield(ECX, 28, 28, doc="Supports MOVDIRI64B if 1")
    sgx_lc = cpuidfield(ECX, 30, 30, doc="Supports SGX Launch Configuration if 1")
    pks = cpuidfield(ECX, 31, 31, doc="Supports protection keys for supervisor-mode pages if 1")

    avx512_4vnniw = cpuidfield(EDX, 2, 2)
    avx512_4fmaps = cpuidfield(EDX, 3, 3)
    fast_short_rep_mov = cpuidfield(EDX, 4, 4)
    avx512_vp2intersect = cpuidfield(EDX, 8, 8)
    md_clear = cpuidfield(EDX, 10, 10)
    hybrid = cpuidfield(EDX, 15, 15, doc="If 1, the processor is identified as a hybrid part")
    pconfig = cpuidfield(EDX, 18, 18, doc="Supports PCONFIG if 1")
    cet_ibt = cpuidfield(EDX, 20, 20, doc="Supports CET indirect branch tracking features if 1")
    ibrs_ibpb = cpuidfield(EDX, 26, 26, doc="Enumerates support for indirect branch restricted speculation (IBRS) and the indirect branch predictor barrier (IBPB)")
    stibp = cpuidfield(EDX, 27, 27, doc="Enumerates support for single thread indirect branch predictors (STIBP)")
    l1d_flush = cpuidfield(EDX, 28, 28, doc="Enumerates support for L1D_FLUSH")
    ia32_arch_capabilities = cpuidfield(EDX, 29, 29, doc="Enumerates support for the IA32_ARCH_CAPABILITIES MSR")
    ia32_core_capabilities = cpuidfield(EDX, 30, 30, doc="Enumerates support for the IA32_CORE_CAPABILITIES MSR")
    ssbd = cpuidfield(EDX, 31, 31, doc="Enumerates support for Speculative Store Bypass Disable (SSBD)")

    capability_bits = [
        "fsgsbase",
        "ia32_tsc_adjust_msr",
        "sgx",
        "bmi1",
        "hle",
        "avx2",
        "fdp_excptn_only",
        "smep",
        "bmi2",
        "erms",
        "invpcid",
        "rtm",
        "qm",
        "deprecate_fpu",
        "mpx",
        "rdt_a",
        "avx512f",
        "avx512dq",
        "rdseed",
        "adx",
        "smap",
        "avx512_ifma",
        "clflushopt",
        "clwb",
        "intel_pt",
        "avx512pf",
        "avx512er",
        "avx512cd",
        "sha",
        "avx512bw",
        "avx512vl",
        "prefetchwt1",
        "avx512_vbmi",
        "umip",
        "pku",
        "waitpkg",
        "avx512_vbmi2",
        "cet_ss",
        "gfni",
        "vaes",
        "vpclmulqdq",
        "avx512_vnni",
        "avx512_bitalg",
        "tme_en",
        "avx512_vpopcntdq",
        "la57",
        "mawau",
        "rdpid",
        "kl",
        "cldemote",
        "movdiri",
        "movdiri64b",
        "sgx_lc",
        "pks",
        "avx512_4vnniw",
        "avx512_4fmaps",
        "fast_short_rep_mov",
        "avx512_vp2intersect",
        "md_clear",
        "hybrid",
        "pconfig",
        "cet_ibt",
        "ibrs_ibpb",
        "stibp",
        "l1d_flush",
        "ia32_arch_capabilities",
        "ia32_core_capabilities",
        "ssbd",
    ]

class LEAF_9(CPUID):
    """Direct Cache Access Information leaf

    Returns information about Direct Cache Access capabilities"""

    leaf = 0x9

    platform_dca_cap = cpuidfield(EAX, 31, 0, doc="Value of bits of IA32_PLATFORM_DCA_CAP MSR (address 1F8H)")

class LEAF_A(CPUID):
    """Architectural Performance Monitoring Leaf

    Returns information about support for architectural performance monitoring capabilities"""

    leaf = 0xA

    architectural_performance_monitor_version_id = cpuidfield(EAX, 7, 0, doc="Version ID of architectural performance monitoring")
    gp_performance_monitor_counters = cpuidfield(EAX, 15, 8, doc="Number of general-purpose performance monitoring counter per logical processor")
    gp_performance_counter_width = cpuidfield(EAX, 23, 16, doc="Bit width of general-purpose, performance monitoring counter")
    ebx_bit_vector_length = cpuidfield(EAX, 31, 24, doc="Length of EBX bit vector to enumerate architectural performance monitoring events")

    core_cycle_event = cpuidfield(EBX, 0, 0, doc="Core cycle event not available if 1")
    instruction_retired_event = cpuidfield(EBX, 1, 1, doc="Instruction retired event not available if 1")
    reference_cycles_event = cpuidfield(EBX, 2, 2, doc="Reference cycles event not available if 1")
    llc_ref_event = cpuidfield(EBX, 3, 3, doc="Last-level cache reference event not available if 1")
    llc_misses_event = cpuidfield(EBX, 4, 4, doc= "Last-level cache misses event not available if 1")
    branch_instruction_retired_event = cpuidfield(EBX, 5, 5, doc="Branch instruction retired event not available if 1")
    branch_mispredict_retired_event = cpuidfield(EBX, 6, 6, doc="Branch mispredict retired event not available if 1")

    ff_performance_counters = cpuidfield(EDX, 4, 0, doc="Number of fixed-function performance counters")
    ff_performance_counter_width = cpuidfield(EDX, 12, 5, doc="Bit width of fixed-function performance counters")

class LEAF_B(CPUID):
    """Extended Topology Enumeration Leaf

    Returns information about extended topology enumeration data"""

    leaf = 0xB

    num_bit_shift = cpuidfield(EAX, 4, 0, doc="Number of bits to shift right on x2APID ID to get a unique topology ID of the next level type")

    logical_proccessors_at_level = cpuidfield(EBX, 15, 0, doc="Number of logical processors at this level type.")

    level_number = cpuidfield(ECX, 7, 0, doc="Level number")
    level_type = cpuidfield(ECX, 15, 8, doc="Level type")

    x2apic_id = cpuidfield(EDX, 31, 0, doc="x2APIC ID of the current logical processor")

class LEAF_D(CPUID):
    """Processor Extended State Enumeration Main Leaf and Sub-Leaves.

    Returns information about the bit-vector representation of all processor
    state extensions that are supported in the processor, and storage size
    requirements of the XSAVE/XRSTOR area. Output depends on initial value of ECX."""

    leaf = 0xD

    valid_bits_xcr0_lower = cpuidfield(EAX, 31, 0, doc="Reports the valid bit fields of the lower 32 bits of XCR0. If a bit is 0, the corresponding bit field in XCR0 is reserved")

    legacy_x87 = cpuidfield(EAX, 0, 0, doc="legacy x87")
    sse_128_bit = cpuidfield(EAX, 1, 1, doc="128-bit SSE")
    avx_256_bit = cpuidfield(EAX, 2, 2, doc="256-bit AVX")

    max_size_enabled_xcr0 = cpuidfield(EBX, 31, 0, doc="Maximum size (bytes, from the beginning of the XSAVE/XRSTOR save area) required by enabled features in XCR0. May be different than ECX if some features at the end of the XSAVE save area are not enabled.")

    max_size_supported_xcr0 = cpuidfield(ECX, 31, 0, doc="Maximum size (bytes, from the beginning of the XSAVE/XRSTOR save area) of the XSAVE/XRSTOR save area required by all supported features in the processor, i.e all the valid bit fields in XCR0.")

    valid_bits_xcr0_upper = cpuidfield(EDX, 31, 0, doc="The valid bit fields of the upper 32 bits of XCR0. If a bit is 0, the corresponding bit field in XCR0 is reserved.")

    def __getitem__(self, subleaf):
        if subleaf == 0:
            return self.read(self.cpu_id, subleaf)
        elif subleaf == 1:
            return LEAF_D_1.read(self.cpu_id, subleaf)
        return LEAF_D_n.read(self.cpu_id, subleaf)

class LEAF_D_1(CPUID):
    """Processor Extended State Enumeration Main Leaf and Sub-Leaves.

    Returns information about the bit-vector representation of all processor
    state extensions that are supported in the processor, and storage size
    requirements of the XSAVE/XRSTOR area. Output depends on initial value of ECX."""

    leaf = 0xD

    xsaveopt = cpuidfield(EAX, 0, 0, doc="XSAVEOPT is available")

class LEAF_D_n(CPUID):
    """Processor Extended State Enumeration Main Leaf and Sub-Leaves.

    Returns information about the bit-vector representation of all processor
    state extensions that are supported in the processor, and storage size
    requirements of the XSAVE/XRSTOR area. Output depends on initial value of ECX."""

    leaf = 0xD

    size = cpuidfield(EAX, 31, 0, doc="The size in bytes (from the offset specified in EBX) of the save area for an extended state feature associated with a valid sub-leaf index")

    offset = cpuidfield(EBX, 31, 0, doc="The offset in bytes of this extended state component's save area from the beginning of the XSAVE/XRSTOR area.")

class LEAF_F(CPUID):
    """Quality of Service Resource Type Enumeration Sub-Leaf and L3 Cache QoS Capability Enumeration Sub-leaf. Depends on value of ECX

    Returns Quality of Service (QoS) Enumeration Information."""

    leaf = 0xF

    def __getitem__(self, subleaf):
        if subleaf == 0:
            return self.read(self.cpu_id, subleaf)
        elif subleaf == 1:
            return LEAF_F_1.read(self.cpu_id, subleaf)
        return LEAF_F_n.read(self.cpu_id, subleaf)

    max_range_rmid_z = cpuidfield(EBX, 31, 0, doc="Maximum range (zero-based) of RMID within this physical processor of all types.")

    l3_cache_qos = cpuidfield(EDX, 1, 1, doc="Supports L3 Cache QoS if 1")

    @property
    def max_range_rmid(self):
        """Maximum range of RMID within this physical processor of all types."""
        return self.max_range_rmid_z + 1

class LEAF_F_1(CPUID):
    """Quality of Service Resource Type Enumeration Sub-Leaf and L3 Cache QoS Capability Enumeration Sub-leaf. Depends on value of ECX

    Returns L3 Cache QoS Capability Enumeration Information."""

    leaf = 0xF

    qm_ctr_conversion_factor = cpuidfield(EBX, 31, 0, doc="Conversion factor from reported IA32_QM_CTR value to occupancy metric (bytes).")

    l3_occupancy_monitoring = cpuidfield(EDX, 0, 0, doc="Supports L3 occupancy monitoring if 1")

    max_range_rmid_z = cpuidfield(ECX, 31, 0, doc="Maximum range (zero-based) of RMID of this resource type")

    @property
    def max_range_rmid(self):
        """Maximum range of RMID of this resource type"""
        return self.max_range_rmid_z + 1

class LEAF_F_n(CPUID):
    """Quality of Service Resource Type Enumeration Sub-Leaf and L3 Cache QoS Capability Enumeration Sub-leaf. Depends on value of ECX

    Returns Quality of Service (QoS) Enumeration Information."""

    leaf = 0xF

class LEAF_10(CPUID):
    """Intel Resource Director Technology (Intel RDT) Allocation Enumeration Sub-leaf"""

    leaf = 0x10

    l3_cache_allocation = cpuidfield(EBX, 1, 1, doc="Supports L3 Cache Allocation Technology if 1.")
    l2_cache_allocation = cpuidfield(EBX, 2, 2, doc="Supports L2 Cache Allocation Technology if 1.")
    memory_bandwidth_allocation = cpuidfield(EBX, 3, 3, doc="Supports Memory Bandwidth Allocation if 1.")

class LEAF_10_1(CPUID):
    """L3/L2 Cache Allocation Technology Enumeration Sub-leaf"""

    leaf = 0x10

    capacity_mask_length_z = cpuidfield(EAX, 4, 0, doc="Length of the capacity bit mask for the corresponding ResID (zero based).")
    isolation_map = cpuidfield(EBX, 31, 0, doc="Bit-granular map of isolation/contention of allocation units.")
    code_and_data_prioritization = cpuidfield(ECX, 2, 2, doc="Code and Data Prioritization Technology supported if 1.")
    clos_number_z = cpuidfield(EDX, 15, 0, doc="Highest COS number supported for this ResID.")

    @property
    def capacity_mask_length(self):
        return self.capacity_mask_length_z + 1

    @property
    def clos_number(self):
        return self.clos_number_z + 1

class LEAF_10_3(CPUID):
    """Memory Bandwidth Allocation Enumeration Sub-leaf"""

    leaf = 0x10

    max_throttling_value_z = cpuidfield(EAX, 11, 0, doc="Reports the maximum MBA throttling value supported for the corresponding ResID (zero based).")
    linear_response_delay = cpuidfield(ECX, 2, 2, doc="Reports whether the response of the delay values is linear.")
    clos_number_z = cpuidfield(EDX, 15, 0, doc="Highest COS number supported for this ResID.")

    @property
    def max_throttling_value(self):
        return self.max_throttling_value_z + 1

    @property
    def clos_number(self):
        return self.clos_number_z + 1

class LEAF_1A(CPUID):
    """Hybrid Information Enumeration Leaf"""

    leaf = 0x1A

    core_type_id = cpuidfield(EAX, 31, 24, doc="Core type")
    native_model_id = cpuidfield(EAX, 23, 0, doc="Native model ID")

    core_types = {
        0x20: "Atom",
        0x40: "Core",
    }

    @property
    def core_type(self):
        if self.core_type_id in self.core_types:
            return self.core_types[self.core_type_id]
        return "Reserved"

class LEAF_1F(LEAF_B):
    """Extened Topology Enumeration Leaf v2"""

    leaf = 0x1F

class LEAF_80000000(CPUID):
    """Extended Function CPUID Information"""

    leaf = 0x80000000

    max_extended_leaf = cpuidfield(EAX, 31, 0, doc="Highest extended function input value understood by CPUID")

class LEAF_80000001(CPUID):
    """Extended Function CPUID Information"""

    leaf = 0x80000001

    ext_signature_feature_bits = cpuidfield(EAX, 31, 0, doc="Extended processor signature and feature bits")

    lahf_sahf_64 = cpuidfield(ECX, 0, 0, doc="LAHF/SAHF available in 64-bit mode")
    lzcnt = cpuidfield(ECX, 5, 5)
    prefetchw = cpuidfield(ECX, 8, 8)

    syscall_sysret_64 = cpuidfield(EDX, 11, 11, doc="SYSCALL/SYSRET available in 64-bit mode")
    execute_disable = cpuidfield(EDX, 20, 20, doc="Execute Disable Bit available")
    gbyte_pages = cpuidfield(EDX, 26, 26, doc="GByte pages are available if 1")
    rdtscp_ia32_tsc_aux = cpuidfield(EDX, 27, 27, doc="RDTSCP and IA32_TSC_AUX are available if 1")
    intel_64 = cpuidfield(EDX, 29, 29, doc="Intel(R) 64 Architecture available if 1")

    capability_bits = [
        "lahf_sahf_64",
        "lzcnt",
        "prefetchw",
        "syscall_sysret_64",
        "execute_disable",
        "gbyte_pages",
        "rdtscp_ia32_tsc_aux",
        "intel_64",
    ]

class LEAF_80000002(CPUID):
    """Extended Function CPUID Information

    Processor Brand String"""

    leaf = 0x80000002

    @property
    def brandstring(self):
        """Processor Brand String"""
        return struct.pack('IIII', self.regs.eax, self.regs.ebx, self.regs.ecx, self.regs.edx).rstrip(b"\x00")

class LEAF_80000003(CPUID):
    """Extended Function CPUID Information

    Processor Brand String Continued"""

    leaf = 0x80000003

    @property
    def brandstring(self):
        """Processor Brand String"""
        return struct.pack('IIII', self.regs.eax, self.regs.ebx, self.regs.ecx, self.regs.edx).rstrip(b"\x00")

class LEAF_80000004(CPUID):
    """Extended Function CPUID Information

    Processor Brand String Continued"""

    leaf = 0x80000004

    @property
    def brandstring(self):
        """Processor Brand String"""
        return struct.pack('IIII', self.regs.eax, self.regs.ebx, self.regs.ecx, self.regs.edx).rstrip(b"\x00")

class LEAF_80000006(CPUID):
    """Extended Function CPUID Information"""

    leaf = 0x80000006

    cache_line_size = cpuidfield(ECX, 7, 0, doc="Cache Line size in bytes")

    l2_associativity = cpuidfield(ECX, 15, 12, doc="L2 Associativity field")
    cache_size_k = cpuidfield(ECX, 31, 16, doc="Cache size in 1K units")

class LEAF_80000007(CPUID):
    """Misc Feature Flags"""

    leaf = 0x80000007

    invariant_tsc = cpuidfield(EDX, 8, 8, doc="Invariant TSC available if 1")

    capability_bits = [
        "invariant_tsc",
    ]

class LEAF_80000008(CPUID):
    """Returns linear/physical address size"""

    leaf = 0x80000008
    physical_address_bits = cpuidfield(EAX, 7, 0, doc="# Physical Address bits")
    linear_address_bits = cpuidfield(EAX, 15, 8, doc="# Linear Address bits")

    attribute_bits = [
        "physical_address_bits",
        "linear_address_bits",
    ]
