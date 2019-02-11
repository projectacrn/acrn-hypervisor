.. _release_notes_0.6:

ACRN v0.6 (Feb 2019)
####################

We are pleased to announce the release of Project ACRN version 0.6.

ACRN is a flexible, lightweight reference hypervisor, built with
real-time and safety-criticality in mind, optimized to streamline
embedded development through an open source platform. Check out the
:ref:`introduction` for more information.


All project ACRN source code is maintained in the
https://github.com/projectacrn/acrn-hypervisor repository and includes
folders for the ACRN hypervisor, the ACRN device model, tools, and
documentation.  You can either download this source code as a zip or
tar.gz file (see the `ACRN v0.6 GitHub release page
<https://github.com/projectacrn/acrn-hypervisor/releases/tag/v0.6>`_ or
use Git clone and checkout commands:

.. code-block:: bash

   git clone https://github.com/projectacrn/acrn-hypervisor
   cd acrn-hypervisor
   git checkout v0.6

The project's online technical documentation is also tagged to correspond
with a specific release: generated v0.6 documents can be found at
https://projectacrn.github.io/0.6/.  Documentation for the latest
(master) branch is found at https://projectacrn.github.io/latest/.

ACRN v0.6 requires Clear Linux OS version 27600 or newer.  Please follow the
instructions in the :ref:`getting-started-apl-nuc`.

Version 0.6 new features
************************

**Enable Privileged VM support for real-time UOS in ACRN**:
Initial patches to enable a User OS
(UOS) running as a virtual machine (VM)
with real-time characteristics, also called a "Privileged VM". We've
published a tutorial :ref:`rt_linux_setup`.  More patches
for ACRN real time support will continue.

**Document updates**: Several new documents have been added in this release, including:

* :ref:`Running Automotive Grade Linux as a VM <agl-vms>`
* :ref:`Using PREEMPT_RT-Linux for real-time UOS <rt_linux_setup>`
* :ref:`Frequently Asked Questions <faq>`
* :ref:`An introduction to Trusty and Security services on ACRN
  <trusty-security-services>`
* A Wiki article about `Porting ClearLinux/ACRN to support Yocto/ACRN
  <https://github.com/projectacrn/acrn-hypervisor/wiki/Yocto-based-Service-OS-(SOS)-and-User-OS-(UOS)-on-ACRN>`_
* An `ACRN brochure update (English and Chinese)
  <https://projectacrn.org/#code-docs>`_

Fixed Issues
************


Known Issues
************


.. comment
   Use the syntax:

   :acrn-issue:`663` - Short issue description
     Longer description that helps explain the problem from the user's
     point of view (not internal reasons).  **Impact:** What's the
     consequences of the issue, and how it can affect the user or system.
     **Workaround:** Describe a workaround if one exists (or refer them to the
     :acrn-issue:`663`` if described well there. If no workaround, say
     "none".


Change Log
**********

These commits have been added to the acrn-hypervisor repo since the v0.5
release in Jan 2019 (click on the CommitID link to see details):

.. comment

   This list is obtained from the command:
   git log --pretty=format:'- :acrn-commit:`%h` %s' --after="2018-03-01"


- :acrn-commit:`ea250c51` doc: fixes to rt-linux tutorial
- :acrn-commit:`1a4a1c30` Need to delete '# CONFIG_BLK_DEV_NVME is not set' to enable NVME driver
- :acrn-commit:`37ce259f` modify create-up2-images scripts
- :acrn-commit:`eb7091bb` HV: add rdmsr/wrmsr debug cmd
- :acrn-commit:`648450c6` HV: cpu: add msr_read_pcpu()& msr_write_pcpu()
- :acrn-commit:`39ffd29a` schedule: add magic number at the bottom of schedule stack
- :acrn-commit:`efc64d77` hv: fix host call stack dump issue
- :acrn-commit:`5214a60b` hv: replace improper use of ASSERT with panic for parse_madt
- :acrn-commit:`9291fbe4` hv: multiboot: replace improper use of ASSERT with panic
- :acrn-commit:`2474c601` hv: replace improper use of panic with ASSERT
- :acrn-commit:`a01c3cb9` doc: change term of vm0 to sos_vm
- :acrn-commit:`7da9161d` hv:no need to use lock  for the bit operations of local variable
- :acrn-commit:`e2cb6acb` doc: add Trusty ACRN doc
- :acrn-commit:`9c3c316f` doc: add rt-linux tutorial
- :acrn-commit:`0881bae7` doc: fix correct use of Clear Linux OS
- :acrn-commit:`fc887ead` doc: update coding guidelines
- :acrn-commit:`72faca50` doc: update documents for "--lapic_pt" feature
- :acrn-commit:`2ffc683d` hv: move some api declaration from mmu.h to ept.h
- :acrn-commit:`615c2bf8` hv:move e820 related macro and structure to e820.h
- :acrn-commit:`2b2dbe43` hv:move some files to guest folder
- :acrn-commit:`e9bb4267` hv:move vpic.h & vioapic.h to dm folder
- :acrn-commit:`89b6dc59` HV:  MISRA clean in reloc.c
- :acrn-commit:`723ff1f4` HV: modularization improve UEFI macro control code
- :acrn-commit:`2a25f4e9` Doc: Remove CL release number from GSG document
- :acrn-commit:`fea541bd` hv: exception: low prioirity exception inject fix
- :acrn-commit:`c6d2908f` hv: vmexit: add handler for vmexit not supported for guest
- :acrn-commit:`cc2c0c3a` hv:Move several inline APIs from vm.h to \*.c
- :acrn-commit:`61552458` Kconfig: enlarge range of maximum number of IOMMU
- :acrn-commit:`69371f41` EFI: fix potential memory overwrite due to mmap table
- :acrn-commit:`b038ade2` hv: fix misra-c violations in reused partition mode functions
- :acrn-commit:`7d4ba5d7` Documentation build tools: update min version for kconfiglib
- :acrn-commit:`878c4e2d` dm: add example script to launch vm for realtime scenarios
- :acrn-commit:`c873d60a` dm: add option "lapic_pt" to create VM for realtime scenarios
- :acrn-commit:`8925da64` dm: adapt mptable generation for VM with lapic pt
- :acrn-commit:`e2cecfb5` hv: send IPI instead of irq injection to notify vcpu with lapic pt
- :acrn-commit:`16df57aa` hv: don't remap msi for pt devices if lapic_pt
- :acrn-commit:`a073ebee` hv: extend lapic pass-through for DM launched VM
- :acrn-commit:`c853eb4b` hv: remove redundant code for virtual interrupt injection
- :acrn-commit:`6d5456a0` hv: Bit Representation for IOAPIC RTE
- :acrn-commit:`7d57eb05` hv: Add bit representation for MSI addr and data
- :acrn-commit:`68250430` hv:Move severl variable declaration for boot code
- :acrn-commit:`c20d095a` HV: refine sos_vm config header
- :acrn-commit:`66e00230` HV: sanitize vm config
- :acrn-commit:`285b64fa` replace arch_switch_to with pure asm code instead of inline asm
- :acrn-commit:`c233bf54` make sure secondary CPU's stack is aligned with CPU STACK
- :acrn-commit:`ee066a7f` hv: fix possible buffer overflow in 'vcpu_set_eoi_exit()'
- :acrn-commit:`88eeae3f` hv: remove unused fields in 'struct acrn_vcpu'
- :acrn-commit:`5e99565b` security: Increase buffer size to avoid buffer overflow error
- :acrn-commit:`d0eb83aa` HV: move Kconfig IOREQ_POLLING to acrn vm config
- :acrn-commit:`6584b547` Makefile: add missing dependency
- :acrn-commit:`c43bca9c` doc: add a FAQ doc
- :acrn-commit:`bb8f5390` doc: add AGL as VMs on ACRN doc
- :acrn-commit:`5c5f4352` HV: modify RELOC kconfig option default to "enable"
- :acrn-commit:`8f22a6e8` HV: fix per-cpu stack relocation in trampoline.c
- :acrn-commit:`41dd38ba` HV: init_paging() wrongly calcuate the size of hypervisor
- :acrn-commit:`9feab4cf` HV: adjust the starting addr of HV to be 2M-aligned
- :acrn-commit:`07f14401` HV: save efi_ctx into HV to use after init_paging()
- :acrn-commit:`a445a4ea` EFI: Allocate EFI boot related struct from EFI allocation pool
- :acrn-commit:`ad0f8bc3` EFI: Allocate 2M aligned memory for hypervisor image
- :acrn-commit:`912be6c4` tools: respect CFLAGS and LDFLAGS from environment
- :acrn-commit:`899c9146` hv:Fix MISRA-C violations in vm.h
- :acrn-commit:`5ba4afcf` Use $(MAKE) when recursing
- :acrn-commit:`d0c9fce7` doc: add more rules in coding guidelines
- :acrn-commit:`3c605127` io_emul: reorg function definition to pass partition mode build
- :acrn-commit:`15030f6f` io_emul: reshuffle io emulation path
- :acrn-commit:`fb41ea5c` io_emul: remove pending_pre_work
- :acrn-commit:`4fc54f95` schedule: add full context switch support
- :acrn-commit:`21092e6f` schedule: use per_cpu idle object
- :acrn-commit:`5e947886` hv: vlapic: remove `calcvdest`
- :acrn-commit:`fd327920` kconfig: update .config on missed or conflicting symbol values
- :acrn-commit:`ca925f0d` dm: storage: change DISCARD to synchronous mode
- :acrn-commit:`46422692` dm: vhpet: add vHPET support
- :acrn-commit:`0343da8c` dm: vhpet: add HPET-related header files
- :acrn-commit:`3fe4c3f2` dm: provide timer callback handlers the number of expirations
- :acrn-commit:`0f7535fd` dm: add absolute timer mode
- :acrn-commit:`d1e1aa30` dm: create mevent's pipe in non-blocking mode
- :acrn-commit:`a9709bf8` hv: Makefile: add the dependency of $(LIB_FLAGS)
- :acrn-commit:`b489aec3` hv: idt: separate the MACRO definition
- :acrn-commit:`862ed16e` Makefile: add rules for installing debug information
- :acrn-commit:`173b534b` HV: modularization cleanup instr_emul header file
- :acrn-commit:`18dbdfd5` HV: replace lapic_pt with guest flag in vm_config
- :acrn-commit:`68aa718c` HV: replace bootargs config with acrn_vm_os_config
- :acrn-commit:`23f8e5e5` HV: replace memory config with acrn_vm_mem_config
- :acrn-commit:`253b2593` HV: remove vm_config pointer in acrn_vm struct
- :acrn-commit:`7bf9b1be` HV: enable pcpu bitmap config for partition mode
- :acrn-commit:`bc62ab79` HV: remove unused vm num config
- :acrn-commit:`2e32fba5` HV: remove sworld_supported in acrn_vm_config
- :acrn-commit:`ec199d96` HV: add get_sos_vm api
- :acrn-commit:`f3014a3c` HV: show correct vm name per config
- :acrn-commit:`e6117e0d` HV: refine launch vm interface
- :acrn-commit:`49e6deaf` HV: rename the term of vm0 to sos vm
- :acrn-commit:`55e5ed2e` hv:move ept violation handler to io_emul.c
- :acrn-commit:`1d98b701` hv: move 'setup_io_bitmap' to vm.c
- :acrn-commit:`de4ab6fd` hv:modulization for IO Emulation
- :acrn-commit:`808d0af2` HV: check to avoid interrupt delay timer add twice
- :acrn-commit:`d9c38baf` HV: remove unused mptable info
- :acrn-commit:`a8e4f227` HV: add new acrn_vm_config member and config files
- :acrn-commit:`c4a230f3` HV: rename the term of vm_description to vm_config
- :acrn-commit:`fe35dde4` Makefile: support SBL binary for E2E build
- :acrn-commit:`13c44f56` acrn/dm: Check device file of /dev/acrn_hsm to determine the path of offline VCPU
- :acrn-commit:`ca328816` acrn/dm: Add the check of acrn_vhm/acrn_hsm to open the VHM driver
- :acrn-commit:`e4a3a634` acrn/vhm: change the default notification vector to 0xF3
- :acrn-commit:`f45605dd` HV: modularization to separate CR related code
- :acrn-commit:`8265770f` hv:Change acrn_vhm_vector to static
- :acrn-commit:`b22c8b69` hv: add more MSR definitions
- :acrn-commit:`6372548e` hv:Fix violation "Cyclomatic complexity greater than 20" in instr_emul.c
- :acrn-commit:`ae144e1a` hv:fix MISRA-C violation in virq.c
- :acrn-commit:`6641bc79` hv: remove ACRN_REQUEST_TMR_UPDATE and unnecessary codes
- :acrn-commit:`fc61536b` hv: rework EOI_EXIT_BITMAP update logic
- :acrn-commit:`f15cc7d6` hv: set/clear TMR bit like hardware behave
- :acrn-commit:`c9b61748` hv: Make reserved regions in E820 table to Supervisor pages
- :acrn-commit:`4322b024` version: 0.6-unstable
