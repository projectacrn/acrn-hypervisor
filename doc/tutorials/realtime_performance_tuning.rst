.. _rt_performance_tuning:

Trace and Data Collection for ACRN Real-Time(RT) Performance Tuning
###################################################################
The document describes the methods to collect trace/data for ACRN RT VM real-time
performance analysis. Two parts are included:

- Method to use trace for the VM exits analysis;
- Method to collect performance monitoring counts for tuning based on PMU.

VM exits analysis for ACRN RT performance
*****************************************

VM exits in response to certain instructions and events are a key source of 
performance degradation in virtual machines. During the runtime of hard RTVM 
of ACRN, there are still some instructions and events which will impact the 
RT latency's determinism.

  - CPUID
  - TSC_Adjust read/write
  - TSC write
  - APICID/LDR read
  - ICR write

Generally, we don't want to see any VM exits occur during the critical section 
of the RT task.

The methodology of VM exits analysis is very simple. Firstly, we should clearly 
identify the critical section of RT task. The critical section is the duration 
of time where we do not want to see any VM exits occur. Different RT tasks get 
different critical section. So this article will take the cyclictest as an example
to elaborate how to do VM exits analysis.

The critical sections
=====================

Here is example pseudocode of cyclictest implementation.

.. code-block:: none

   while (!shutdown) {
         …
         clock_nanosleep(&next)
         clock_gettime(&now)
         latency = calcdiff(now, next)
         …
         next += interval
   }

Time point ``now`` is the actual point at which the cyclictest is wakeuped and 
scheduled. Time point ``next`` is the expected point at which we want the cyclictest 
to be woken up and scheduled. Here we can get the latency by ``now - next``. We don't 
want to see VM exits during ``next`` through ``now``. So define the start point of 
critical section as ``next`` and end point ``now``.

Log and trace data collection
=============================

#. Add timestamps (in TSC) at ``next`` and ``now``.
#. Capture the log with the above timestamps in RTVM.
#. Capture the acrntrace log in Service VM at the same time.

Offline analysis
================

#. Convert the raw trace data to human readable format.
#. Merge the logs in RTVM and ACRN hypervisor trace based on timestamps (in TSC).
#. Check if there is any VM exit within the critical sections, the pattern is as follows:

   .. figure:: images/vm_exits_log.png
      :align: center
      :name: vm_exits_log

Performance monitoring counts collecting
****************************************

Enable Performance Monitoring Unit (PMU) support in VM
======================================================

By default, ACRN hypervisor doesn't expose the PMU related CPUID and MSRs to 
guest VM. In order to use Performance Monitoring Counters (PMCs) in guest VM, 
need to modify the ACRN hypervisor code to expose the capability to RTVM.

.. note:: Precise Event Based Sampling (PEBS) is not enabled in VM yet.

#. Expose CPUID leaf 0xA as below:
   
   .. code-block:: none

      --- a/hypervisor/arch/x86/guest/vcpuid.c
      +++ b/hypervisor/arch/x86/guest/vcpuid.c
      @@ -345,7 +345,7 @@ int32_t set_vcpuid_entries(struct acrn_vm *vm)
      break;
      /* These features are disabled */
      /* PMU is not supported */
      - case 0x0aU:
      + //case 0x0aU:
      /* Intel RDT */
      case 0x0fU:
      case 0x10U:

#. Expose PMU related MSRs to VM as below:

   .. code-block:: none

      --- a/hypervisor/arch/x86/guest/vmsr.c
      +++ b/hypervisor/arch/x86/guest/vmsr.c
      @@ -337,6 +337,41 @@ void init_msr_emulation(struct acrn_vcpu *vcpu)
      /* don't need to intercept rdmsr for these MSRs */
      enable_msr_interception(msr_bitmap, MSR_IA32_TIME_STAMP_COUNTER, INTERCEPT_WRITE);
      
      +
      + /* Passthru PMU related MSRs to guest */
      + enable_msr_interception(msr_bitmap, MSR_IA32_FIXED_CTR_CTL, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERF_GLOBAL_CTRL, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERF_GLOBAL_STATUS, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERF_GLOBAL_OVF_CTRL, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERF_GLOBAL_STATUS_SET, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERF_GLOBAL_INUSE, INTERCEPT_DISABLE);
      +
      + enable_msr_interception(msr_bitmap, MSR_IA32_FIXED_CTR0, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_FIXED_CTR1, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_FIXED_CTR2, INTERCEPT_DISABLE);
      +
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC0, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC1, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC2, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC3, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC4, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC5, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC6, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PMC7, INTERCEPT_DISABLE);
      +
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC0, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC1, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC2, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC3, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC4, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC5, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC6, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_A_PMC7, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERFEVTSEL0, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERFEVTSEL1, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERFEVTSEL2, INTERCEPT_DISABLE);
      + enable_msr_interception(msr_bitmap, MSR_IA32_PERFEVTSEL3, INTERCEPT_DISABLE);
      +
      /* Setup MSR bitmap - Intel SDM Vol3 24.6.9 */
      value64 = hva2hpa(vcpu->arch.msr_bitmap);
      exec_vmwrite64(VMX_MSR_BITMAP_FULL, value64);

Use Perf/PMU tool in performance analysis
=========================================

After exposing PMU related CPUID/MSRs to VM, the performance analysis tool such as 
perf and pmu tool can be used inside VM to locate the bottleneck of the application.
**Perf** is a profiler tool for Linux 2.6+ based systems that abstracts away CPU 
hardware differences in Linux performance measurements and presents a simple command 
line interface. Perf is based on the perf_events interface exported by recent versions 
of the Linux kernel.
**PMU** tools is a collection of tools for profile collection and performance analysis 
on Intel CPUs on top of Linux Perf. You can refer to the following links for the usage 
of Perf:

  - https://perf.wiki.kernel.org/index.php/Main_Page
  - https://perf.wiki.kernel.org/index.php/Tutorial

You can refer to https://github.com/andikleen/pmu-tools for the usage of PMU tool.

Top-down Micro-architecture Analysis Method (TMAM)
==================================================

The Top-down Micro-architecture Analysis Method based on the Top-Down Characterization 
methodology aims to provide an insight into whether you have made wise choices with your 
algorithms and data structures. See the Intel |reg| 64 and IA-32 `Architectures Optimization 
Reference Manual <http://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-optimization-manual.pdf>`_,
Appendix B.1 for more details on the Top-down Micro-architecture Analysis Method.
You can refer to this `technical paper
<https://fd.io/wp-content/uploads/sites/34/2018/01/performance_analysis_sw_data_planes_dec21_2017.pdf>`_
which adopts TMAM for systematic performance benchmarking and analysis of compute-native 
Network Function data planes executed on Commercial-Off-The-Shelf (COTS) servers using available
open-source measurement tools.

Example: Using Perf to analysis TMAM level 1 on CPU core 1.

   .. code-block:: console

      perf stat --topdown -C 1 taskset -c 1 dd if=/dev/zero of=/dev/null count=10
      10+0 records in
      10+0 records out
      5120 bytes (5.1 kB, 5.0 KiB) copied, 0.00336348 s, 1.5 MB/s
      
      Performance counter stats for 'CPU(s) 1':
      
              retiring bad speculation frontend bound backend bound
      S0-C1 1 10.6%               1.5%           3.9%         84.0%
      
      0.006737123 seconds time elapsed

