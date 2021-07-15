.. _develop_acrn:

Advanced Guides
###############


Configuration and Tools
***********************

.. rst-class:: rst-columns2

.. toctree::
   :glob:
   :maxdepth: 1

   tutorials/acrn_configuration_tool
   reference/config-options
   user-guides/hv-parameters
   user-guides/kernel-parameters
   user-guides/acrn-shell
   user-guides/acrn-dm-parameters
   misc/debug_tools/acrn_crashlog/README
   misc/packaging/README
   misc/debug_tools/**
   misc/services/acrn_manager/**

Service VM Tutorials
********************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/running_deb_as_serv_vm
   tutorials/using_yp

User VM Tutorials
*****************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/using_windows_as_uos
   tutorials/running_ubun_as_user_vm
   tutorials/running_deb_as_user_vm
   tutorials/using_xenomai_as_uos
   tutorials/using_vxworks_as_uos
   tutorials/using_zephyr_as_uos

Enable ACRN Features
********************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/sgx_virtualization
   tutorials/nvmx_virtualization
   tutorials/vuart_configuration
   tutorials/rdt_configuration
   tutorials/waag-secure-boot
   tutorials/enable_s5
   tutorials/cpu_sharing
   tutorials/sriov_virtualization
   tutorials/gpu-passthru
   tutorials/run_kata_containers
   tutorials/trustyACRN
   tutorials/rtvm_workload_design_guideline
   tutorials/setup_openstack_libvirt
   tutorials/acrn_on_qemu
   tutorials/using_grub
   tutorials/acrn-secure-boot-with-grub
   tutorials/acrn-secure-boot-with-efi-stub
   tutorials/pre-launched-rt
   tutorials/enable_ivshmem
   tutorials/enable_ptm

Debug
*****

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/using_serial_port
   tutorials/debug
   tutorials/realtime_performance_tuning
   tutorials/rtvm_performance_tips
