.. _run-kata-containers:

Run Kata Containers on a Service VM
###################################

This tutorial describes how to install, configure, and run `Kata Containers
<https://katacontainers.io/>`_ on the Ubuntu based Service VM with the ACRN
hypervisor. In this configuration,
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
#. Follow :ref:`these instructions <Ubuntu Service OS>` to set up the ACRN Service VM
   based on Ubuntu. Please note that only ACRN hypervisors compiled for
   SDC scenario support Kata Containers currently.


Install Docker
**************

The following instructions install Docker* on the Ubuntu Service VM.
Refer to the `Get Docker Engine - Community for Ubuntu
<https://docs.docker.com/engine/install/ubuntu/>`_
installation guide for detailed information.

#. Install the following prerequisite packages:

   .. code-block:: none

      $ sudo apt-get install apt-transport-https ca-certificates curl

#. Run the following commands to add Docker's official GPG key,
   set up the repository, and install the Docker Engine - Community
   from the repository:

   .. code-block:: none

      $ curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
      $ sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
      $ sudo apt-get update
      $ sudo apt-get install -y docker-ce docker-ce-cli containerd.io

Install Kata Containers
***********************

Kata Containers provide a variety of installation methods, this guide uses
:command:`kata-manager` to automate the Kata Containers installation procedure.

#. Install Kata Containers packages:

   .. code-block:: none

      $ bash -c "$(curl -fsSL https://raw.githubusercontent.com/kata-containers/tests/master/cmd/kata-manager/kata-manager.sh) install-packages"

#. Add the following settings to :file:`/etc/docker/daemon.json` to configure
   Docker to use Kata Containers by default. You may need to create the
   file if it doesn't exist.

   .. code-block:: none

      {
        "storage-driver": "devicemapper",
        "default-runtime": "kata-runtime",
        "runtimes": {
            "kata-runtime": {
                "path": "/usr/bin/kata-runtime"
            }
        }
      }

   In order to run Kata with ACRN, the container stack must provide block-based
   storage, such as :file:`device-mapper`. Since Docker may be configured to
   use :file:`overlay2` storage driver, the above configuration also instructs
   Docker to use :file:`devive-mapper` storage driver.

#. Configure Kata to use ACRN.

   .. code-block:: none

      $ sudo mkdir -p /etc/kata-containers
      $ sudo cp /usr/share/defaults/kata-containers/configuration-acrn.toml /etc/kata-containers/configuration.toml

#. Restart the Docker service.

   .. code-block:: none

      $ sudo systemctl restart docker

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
