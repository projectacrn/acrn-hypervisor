.. _GVT-g_api:

ACRN GVT-g APIs
###############

GVT-g is Intel's open source GPU virtualization solution and is up-streamed to
the Linux kernel. Its implementation over KVM is named KVMGT, over Xen it is
named XenGT, and over ACRN it is named AcrnGT. GVT-g can exports multiple
virtual GPU (vGPU) instances for virtual machine system (VM). A VM could be
assigned one vGPU instance. The guest OS graphic driver needs minor
modification to drive the vGPU adapter in a VM. Every vGPU instance will adopt
the full HW GPU's accelerate capability for 3D render and display.

In the following document, AcrnGT refers to the glue layer between ACRN
hypervisor and GVT-g core device model. It works as the agent of
hypervisor-related services. It is the only layer that needs to get rewritten
when porting GVT-g to another hypervisor. For simplicity, in the rest of this
document, GVT is used to refer to the core device model component of GVT-g,
specifically corresponding to ``gvt.ko`` when built as a module.

Core Driver Infrastructure
**************************

This section covers core driver infrastructure API used by both the display
and the `Graphics Execution Manager(GEM)`_ parts of `i915 driver`_.

.. _Graphics Execution Manager(GEM): https://lwn.net/Articles/283798/

.. _i915 driver: https://01.org/linuxgraphics/gfx-docs/drm/gpu/i915.html

Intel GVT-g Guest Support(vGPU)
===============================

.. kernel-doc:: drivers/gpu/drm/i915/i915_vgpu.c
   :doc: Intel GVT-g guest support

.. kernel-doc:: drivers/gpu/drm/i915/i915_vgpu.c
   :internal:

Intel GVT-g Host Support(vGPU device model)
===========================================

.. kernel-doc:: drivers/gpu/drm/i915/intel_gvt.c
   :doc: Intel GVT-g host support

.. kernel-doc:: drivers/gpu/drm/i915/intel_gvt.c
   :internal:


VHM APIs called from AcrnGT
****************************

The Virtio and Hypervisor Service Module (VHM) is a kernel module in the
Service OS acting as a middle layer to support the device model. (See the
:ref:`ACRN-io-mediator` introduction for details.)

VHM requires an interrupt (vIRQ) number, and exposes some APIs to external
kernel modules such as GVT-g and the Virtio back-end (BE) service running in
kernel space.  VHM exposes a ``char`` device node in user space, and only
interacts with DM. The DM routes I/O request and response from and to other
modules via the ``char`` device to and from VHM. DM may use VHM for hypervisor
service (including remote memory map). VHM may directly service the request
such as for the remote memory map, or invoke hypercall. VHM also sends I/O
responses to user space modules, notified by vIRQ injections.


.. kernel-doc:: include/linux/vhm/vhm_vm_mngt.h
   :functions: put_vm
               vhm_get_vm_info
               vhm_inject_msi
               vhm_vm_gpa2hpa

.. kernel-doc:: include/linux/vhm/acrn_vhm_ioreq.h
   :internal:

.. kernel-doc:: include/linux/vhm/acrn_vhm_mm.h
   :functions: acrn_hpa2gpa
               map_guest_phys
               unmap_guest_phys
               add_memory_region
               del_memory_region
               write_protect_page

.. _MPT_interface:

AcrnGT mediated pass-through (MPT) interface
**************************************************

AcrnGT receives request from GVT module through MPT interface. Refer to the
:ref:`Graphic_mediation` page.

A collection of function callbacks in the MPT module will be attached to GVT
host at the driver loading stage. AcrnGT MPT function callbacks are described
as below:


.. code-block:: c

        struct intel_gvt_mpt acrn_gvt_mpt = {
                .host_init = acrngt_host_init,
                .host_exit = acrngt_host_exit,
                .attach_vgpu = acrngt_attach_vgpu,
                .detach_vgpu = acrngt_detach_vgpu,
                .inject_msi = acrngt_inject_msi,
                .from_virt_to_mfn = acrngt_virt_to_mfn,
                .enable_page_track = acrngt_page_track_add,
                .disable_page_track = acrngt_page_track_remove,
                .read_gpa = acrngt_read_gpa,
                .write_gpa = acrngt_write_gpa,
                .gfn_to_mfn = acrngt_gfn_to_pfn,
                .map_gfn_to_mfn = acrngt_map_gfn_to_mfn,
                .dma_map_guest_page = acrngt_dma_map_guest_page,
                .dma_unmap_guest_page = acrngt_dma_unmap_guest_page,
                .set_trap_area = acrngt_set_trap_area,
                .set_pvmmio = acrngt_set_pvmmio,
                .dom0_ready = acrngt_dom0_ready,

	};
	EXPORT_SYMBOL_GPL(acrn_gvt_mpt);

GVT-g core logic will call these APIs through wrap functions with prefix
``intel_gvt_hypervisor_`` to request specific services from hypervisor through
VHM.

This section describes the wrap functions:

.. kernel-doc:: drivers/gpu/drm/i915/gvt/mpt.h
   :functions: intel_gvt_hypervisor_host_init
               intel_gvt_hypervisor_host_exit
               intel_gvt_hypervisor_attach_vgpu
               intel_gvt_hypervisor_detach_vgpu
               intel_gvt_hypervisor_inject_msi
               intel_gvt_hypervisor_virt_to_mfn
               intel_gvt_hypervisor_enable_page_track
               intel_gvt_hypervisor_disable_page_track
               intel_gvt_hypervisor_read_gpa
               intel_gvt_hypervisor_write_gpa
               intel_gvt_hypervisor_gfn_to_mfn
               intel_gvt_hypervisor_map_gfn_to_mfn
               intel_gvt_hypervisor_dma_map_guest_page
               intel_gvt_hypervisor_dma_unmap_guest_page
               intel_gvt_hypervisor_set_trap_area
               intel_gvt_hypervisor_set_pvmmio
               intel_gvt_hypervisor_dom0_ready

.. _intel_gvt_ops_interface:

GVT-g intel_gvt_ops interface
*****************************

This section contains APIs for GVT-g intel_gvt_ops interface. Sources are found
in the `ACRN kernel GitHub repo`_


.. _ACRN kernel GitHub repo: https://github.com/projectacrn/acrn-kernel/


.. code-block:: c

	static const struct intel_gvt_ops intel_gvt_ops = {
		.emulate_cfg_read = intel_vgpu_emulate_cfg_read,
		.emulate_cfg_write = intel_vgpu_emulate_cfg_write,
		.emulate_mmio_read = intel_vgpu_emulate_mmio_read,
		.emulate_mmio_write = intel_vgpu_emulate_mmio_write,
		.vgpu_create = intel_gvt_create_vgpu,
		.vgpu_destroy = intel_gvt_destroy_vgpu,
		.vgpu_reset = intel_gvt_reset_vgpu,
		.vgpu_activate = intel_gvt_activate_vgpu,
		.vgpu_deactivate = intel_gvt_deactivate_vgpu,
	};

.. kernel-doc:: drivers/gpu/drm/i915/gvt/cfg_space.c
   :functions: intel_vgpu_emulate_cfg_read
               intel_vgpu_emulate_cfg_write

.. kernel-doc:: drivers/gpu/drm/i915/gvt/mmio.c
   :functions: intel_vgpu_emulate_mmio_read
               intel_vgpu_emulate_mmio_write

.. kernel-doc:: drivers/gpu/drm/i915/gvt/vgpu.c
   :functions: intel_gvt_create_vgpn
               intel_gvt_destroy_vgpu
               intel_gvt_reset_vgpu
               intel_gvt_activate_vgpu
               intel_gvt_deactivate_vgpu

.. _sysfs_interface:

AcrnGT sysfs interface
***********************

This section contains APIs for the AcrnGT sysfs interface. Sources are found
in the `ACRN kernel GitHub repo`_


sysfs nodes
===========

In below examples all accesses to these interfaces are via bash command
``echo`` or ``cat``. This is a quick and easy way to get/control things. But
when these operations fails, it is impossible to get respective error code by
this way.

When accessing sysfs entries, people should use library functions such as
``read()`` or ``write()``.

On **success**, the returned value of ``read()`` or ``write()`` indicates how
many bytes have been transferred.  On **error**, the returned value is ``-1``
and the global ``errno`` will be set appropriately. This is the only way to
figure out what kind of error occurs.


/sys/kernel/gvt/
----------------

The ``/sys/kernel/gvt/`` class sub-directory belongs to AcrnGT and provides a
centralized sysfs interface for configuring vGPU properties.


/sys/kernel/gvt/control/
------------------------

The ``/sys/kernel/gvt/control/`` sub-directory contains all the necessary
switches for different purposes.

/sys/kernel/gvt/control/create_gvt_instance
-------------------------------------------

The ``/sys/kernel/gvt/control/create_gvt_instance`` node is used by ACRN-DM to
create/destroy a vGPU instance.

/sys/kernel/gvt/vmN/
--------------------

After a VM is created, a new sub-directory ``vmN`` ("N" is the VM id) will be
created.

/sys/kernel/gvt/vmN/vgpu_id
---------------------------

The ``/sys/kernel/gvt/vmN/vgpu_id`` node is to get vGPU id from VM which id is
N.
