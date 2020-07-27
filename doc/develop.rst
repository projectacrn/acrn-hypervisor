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
   reference/kconfig/index
   user-guides/hv-parameters
   user-guides/kernel-parameters
   user-guides/acrn-shell
   user-guides/acrn-dm-parameters
   misc/tools/acrn-crashlog/README
   misc/packaging/README
   misc/tools/**
   misc/acrn-manager/**

Service VM Tutorials
********************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/using_ubuntu_as_sos
   tutorials/running_deb_as_serv_vm
   tutorials/cl_servicevm

User VM Tutorials
*****************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/building_uos_from_clearlinux
   tutorials/using_windows_as_uos
   tutorials/running_ubun_as_user_vm
   tutorials/running_deb_as_user_vm
   tutorials/using_xenomai_as_uos
   tutorials/using_celadon_as_uos
   tutorials/using_vxworks_as_uos
   tutorials/using_zephyr_as_uos
   tutorials/agl-vms

Enable ACRN Features
********************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/acrn-dm_QoS
   tutorials/open_vswitch
   tutorials/sgx_virtualization
   tutorials/vuart_configuration
   tutorials/rdt_configuration
   tutorials/using_sbl_on_up2
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
   tutorials/pre-launched-rt

Debug
*****

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/debug
   tutorials/realtime_performance_tuning
   tutorials/rtvm_performance_tips

Additional Tutorials
********************

.. rst-class:: rst-columns2

.. toctree::
   :maxdepth: 1

   tutorials/up2
   tutorials/building_acrn_in_docker
   tutorials/acrn_ootb
   tutorials/static-ip
   tutorials/increase-uos-disk-size
   tutorials/sign_clear_linux_image
   tutorials/enable_laag_secure_boot
   tutorials/kbl-nuc-sdc
