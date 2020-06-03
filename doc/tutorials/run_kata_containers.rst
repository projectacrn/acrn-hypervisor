.. _run-kata-containers:

Run Kata Containers on a Service VM
###################################

This tutorial describes how to install, configure, and run `Kata Containers
<https://katacontainers.io/>`_ on the ACRN Service VM. In this configuration,
Kata Containers leverage the ACRN hypervisor instead of QEMU which is used by
default. Refer to the `Kata Containers with ACRN
<https://drive.google.com/file/d/1ZrqM5ouWUJA0FeIWhU_aitEJe8781rpe/view?usp=sharing>`_
presentation from a previous ACRN Project Technical Community Meeting for
more details on Kata Containers and how the integration with ACRN has been
done.

Prerequisites
**************

#. Refer to the :ref:`ACRN supported hardware <hardware>`.
#. For a default prebuilt ACRN binary in the E2E package, you must have 4
   CPU cores or enable "CPU Hyper-Threading" in order to have 4 CPU threads for 2 CPU cores.
#. Follow :ref:`these instructions <kbl-nuc-sdc>` to set up the ACRN Service VM.


Install Docker
**************

.. code-block:: none

   $ sudo swupd bundle-add containers-basic
   $ sudo systemctl enable docker
   $ sudo systemctl start docker

Install Kata Containers
***********************

The Kata Containers installation from Clear Linux's official repository does
not work with ACRN at the moment. Therefore, you must install Kata
Containers using the `manual installation
<https://github.com/kata-containers/documentation/blob/master/Developer-Guide.md>`__
instructions (using a ``rootfs`` image).

#. Install the build dependencies.

   .. code-block:: none

      $ sudo swupd bundle-add go-basic devpkg-elfutils

#. Install Kata Containers.

   At a high level, the `manual installation
   <https://github.com/kata-containers/documentation/blob/master/Developer-Guide.md>`__
   steps are:

   #. Build and install the Kata runtime.
   #. Create and install a ``rootfs``.
   #. Build and install the Kata Containers kernel.
   #. Build and install the Kata proxy.
   #. Build and install the Kata shim.

Configure Kata on ACRN
**********************

After the core components are installed on the system, the next step is to
configure them to work seamlessly together. This includes two parts.

#. `Configure Docker <https://github.com/kata-containers/documentation/blob/master/Developer-Guide.md#run-kata-containers-with-docker>`_
   to recognize the ``kata-runtime`` as an additional runtime available for
   use.

#. Configure Kata to use ACRN.

   .. code-block:: none

      $ sudo mkdir -p /etc/kata-containers
      $ sudo cp /usr/share/defaults/kata-containers/configuration-acrn.toml /etc/kata-containers/configuration.toml

Verify that these configurations are effective by checking the following
outputs:

.. code-block:: none

   $ sudo docker info | grep runtime
   WARNING: the devicemapper storage-driver is deprecated, and will be
   removed in a future release.
   WARNING: devicemapper: usage of loopback devices is strongly discouraged
   for production use.
            Use `--storage-opt dm.thinpooldev` to specify a custom block storage device.
   Runtimes: kata-runtime runc

.. code-block:: none

   $ kata-runtime kata-env | awk -v RS= '/\[Hypervisor\]/'
   [Hypervisor]
     MachineType = ""
     Version = "DM version is: 1.5-unstable-"2020w02.5.140000p_261" (daily tag:"2020w02.5.140000p"), build by mockbuild@2020-01-12 08:44:52"
     Path = "/usr/bin/acrn-dm"
     BlockDeviceDriver = "virtio-blk"
     EntropySource = "/dev/urandom"
     Msize9p = 0
     MemorySlots = 10
     Debug = false
     UseVSock = false
     SharedFS = ""

Run a Kata Container with ACRN
******************************

The system is now ready to run a Kata Container on ACRN. Note that a reboot
is recommended after the installation.

Before running a Kata Container on ACRN, you must offline at least one CPU:

.. code-block:: none

   $ curl -O https://raw.githubusercontent.com/kata-containers/documentation/master/how-to/offline_cpu.sh
   $ chmod +x ./offline_cpu.sh
   $ sudo ./offline_cpu.sh

Start a Kata Container on ACRN:

.. code-block:: none

   $ sudo docker run -ti --runtime=kata-runtime busybox sh

If you run into problems, contact us on the ACRN mailing list and provide as
much detail as possible about the issue. The output of ``sudo docker info``
and ``kata-runtime kata-env`` is useful.
