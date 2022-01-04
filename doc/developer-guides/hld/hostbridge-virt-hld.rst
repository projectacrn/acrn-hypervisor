.. _hostbridge_virt_hld:

Hostbridge Emulation
####################

Overview
********

Hostbridge emulation is based on PCI emulation; however, the hostbridge
emulation only sets the PCI configuration space. The Device Model (DM) sets the
PCI configuration space for hostbridge in the Service VM and then exposes it to
the User VM to detect the PCI hostbridge.

PCI Host Bridge and Hierarchy
*****************************

There is PCI host bridge emulation in DM. The bus hierarchy is determined by ``acrn-dm`` command line input. Using this command line, as an example::

        acrn-dm -A -m $mem_size -s 0:0,hostbridge \
        -s 5,virtio-console,@stdio:stdio_port \
        -s 6,virtio-hyper_dmabuf \
        -s 3,virtio-blk,/home/acrn/UserVM.img \
        -s 4,virtio-net,tap0 \
        -s 7,virtio-rnd \
        --ovmf /usr/share/acrn/bios/OVMF.fd \
        $logger_setting \
        --mac_seed $mac_seed \
        $vm_name

the bus hierarchy would be:

.. code-block:: console

   # lspci
   00:00.0 Host bridge: Network Appliance Corporation Device 1275
   00:01.0 ISA bridge: Intel Corporation 82371SB PIIX3 ISA [Natoma/Triton II]
   00:03.0 SCSI storage controller: Red Hat, Inc. Virtio block device
   00:05.0 Serial controller: Red Hat, Inc. Virtio console
   00:06.0 RAM memory: Intel Corporation Device 8606
   00:08.0 Network and computing encryption device: Red Hat, Inc. Virtio RNG
   00:09.0 Ethernet controller: Red Hat, Inc. Virtio network device
