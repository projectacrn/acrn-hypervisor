:orphan:

.. _sample-app:

ACRN Sample Application
#######################

This directory contains a software application that runs a real-time
application between a real-time VM and a standard VM using the
acrn-hypervisor.

The ``rtvm`` directory contains the code that reads and pipes data from
``cyclictest`` to the User VM using the inter-vm shared memory feature that
acrn-hypervisor exposes to its VMs.

The ``uservm`` directory contains the code that reads the piped data from the
RTVM, processes the data, and displays the data over a web application that
can be accessed from the hypervisor's Service VM.

To build and run the applications, copy this repo to your VMs, run make in the
directory that corresponds to the VM that you are running, and then follow the
sample app guide in the acrn-hypervisor documentation.

For more information, please go to: https://github.com/projectacrn/acrn-hypervisor/wiki/ACRN-Sample-App
