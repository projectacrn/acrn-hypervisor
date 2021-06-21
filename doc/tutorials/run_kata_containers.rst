.. _run-kata-containers:

Run Kata Containers on a Service VM
###################################

This tutorial describes how to install, configure, and run `Kata Containers
<https://katacontainers.io/>`_ on the Ubuntu based Service VM with the ACRN
hypervisor. In this configuration,
Kata Containers leverage the ACRN hypervisor instead of QEMU, which is used by
default. Refer to the `Kata Containers with ACRN presentation
<https://www.slideshare.net/ProjectACRN/acrn-kata-container-on-acrn>`_
for more details on Kata Containers and how the integration with ACRN
has been done.

Prerequisites
**************

#. Refer to the :ref:`ACRN supported hardware <hardware>`.
#. For a default prebuilt ACRN binary in the end-to-end (E2E) package, you must have 4
   CPU cores or enable "CPU Hyper-threading" in order to have 4 CPU threads for 2 CPU cores.
#. Follow the :ref:`gsg` to set up the ACRN Service VM
   based on Ubuntu.
#. This tutorial is validated on the following configurations:

   - ACRN v2.0 (branch: ``release_2.0``)
   - Ubuntu 20.04

#. Kata Containers are supported for ACRN hypervisors configured for
   the Industry or SDC scenarios.


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
`kata-deploy <https://github.com/kata-containers/packaging/tree/master/kata-deploy>`_
to automate the Kata Containers installation procedure.

#. Install Kata Containers:

   .. code-block:: none

      $ sudo docker run -v /opt/kata:/opt/kata -v /var/run/dbus:/var/run/dbus -v /run/systemd:/run/systemd -v /etc/docker:/etc/docker -it katadocker/kata-deploy kata-deploy-docker install

#. Install the ``acrnctl`` tool:

   .. code-block:: none

      $ cd /home/acrn/work/acrn-hypervisor
      $ sudo cp build/misc/tools/acrnctl /usr/bin/

   .. note:: This assumes you have built ACRN on this machine following the
      instructions in the :ref:`gsg`.

#. Modify the :ref:`daemon.json` file in order to:

   a. Add a ``kata-acrn`` runtime (``runtimes`` section).

      .. note:: In order to run Kata with ACRN, the container stack must provide
         block-based storage, such as :file:`device-mapper`. Since Docker may be
         configured to use :file:`overlay2` storage driver, the above
         configuration also instructs Docker to use :file:`device-mapper`
         storage driver.

   #. Use the ``device-mapper`` storage driver.

   #. Make Docker use Kata Containers by default.

   These changes are highlighted below.

   .. code-block:: none
      :emphasize-lines: 2,3,21-24
      :name: daemon.json
      :caption: /etc/docker/daemon.json

      {
        "storage-driver": "devicemapper",
        "default-runtime": "kata-acrn",
        "runtimes": {
          "kata-qemu": {
            "path": "/opt/kata/bin/kata-runtime",
            "runtimeArgs": [ "--kata-config", "/opt/kata/share/defaults/kata-containers/configuration-qemu.toml" ]
          },
          "kata-qemu-virtiofs": {
            "path": "/opt/kata/bin/kata-runtime",
            "runtimeArgs": [ "--kata-config", "/opt/kata/share/defaults/kata-containers/configuration-qemu-virtiofs.toml" ]
          },
          "kata-fc": {
            "path": "/opt/kata/bin/kata-runtime",
            "runtimeArgs": [ "--kata-config", "/opt/kata/share/defaults/kata-containers/configuration-fc.toml" ]
          },
          "kata-clh": {
            "path": "/opt/kata/bin/kata-runtime",
            "runtimeArgs": [ "--kata-config", "/opt/kata/share/defaults/kata-containers/configuration-clh.toml" ]
          },
          "kata-acrn": {
            "path": "/opt/kata/bin/kata-runtime",
            "runtimeArgs": [ "--kata-config", "/opt/kata/share/defaults/kata-containers/configuration-acrn.toml" ]
          }
        }
      }

#. Configure Kata to use ACRN.

   Modify the ``[hypervisor.acrn]`` section in the ``/opt/kata/share/defaults/kata-containers/configuration-acrn.toml``
   file.

   .. code-block:: none
      :emphasize-lines: 2,3
      :name: configuration-acrn.toml
      :caption: /opt/kata/share/defaults/kata-containers/configuration-acrn.toml

      [hypervisor.acrn]
      path = "/usr/bin/acrn-dm"
      ctlpath = "/usr/bin/acrnctl"
      kernel = "/opt/kata/share/kata-containers/vmlinuz.container"
      image = "/opt/kata/share/kata-containers/kata-containers.img"

#. Restart the Docker service.

   .. code-block:: none

      $ sudo systemctl restart docker

Verify that these configurations are effective by checking the following
outputs:

.. code-block:: console

   $ sudo docker info | grep -i runtime
   WARNING: the devicemapper storage-driver is deprecated, and will be removed in a future release.
   WARNING: devicemapper: usage of loopback devices is strongly discouraged for production use.
            Use `--storage-opt dm.thinpooldev` to specify a custom block storage device.
    Runtimes: kata-clh kata-fc kata-qemu kata-qemu-virtiofs runc kata-acrn
    Default Runtime: kata-acrn

.. code-block:: console

   $ /opt/kata/bin/kata-runtime --kata-config /opt/kata/share/defaults/kata-containers/configuration-acrn.toml kata-env | awk -v RS= '/\[Hypervisor\]/'
   [Hypervisor]
     MachineType = ""
     Version = "DM version is: 2.0-unstable-7c7bf767-dirty (daily tag:acrn-2020w23.5-180000p), build by acrn@2020-06-11 17:11:17"
     Path = "/usr/bin/acrn-dm"
     BlockDeviceDriver = "virtio-blk"
     EntropySource = "/dev/urandom"
     SharedFS = ""
     VirtioFSDaemon = ""
     Msize9p = 0
     MemorySlots = 10
     PCIeRootPort = 0
     HotplugVFIOOnRootBus = false
     Debug = false
     UseVSock = false

Run a Kata Container With ACRN
******************************

The system is now ready to run a Kata Container on ACRN. Note that a reboot
is recommended after the installation.

Before running a Kata Container on ACRN, you must take at least one CPU offline:

.. code-block:: none

   $ curl -O https://raw.githubusercontent.com/kata-containers/documentation/master/how-to/offline_cpu.sh
   $ chmod +x ./offline_cpu.sh
   $ sudo ./offline_cpu.sh

Start a Kata Container on ACRN:

.. code-block:: none

   $ sudo docker run -ti busybox sh

If you run into problems, contact us on the `ACRN mailing list
<https://lists.projectacrn.org/g/acrn-dev>`_ and provide as
much detail as possible about the issue. The output of ``sudo docker info``
and ``kata-runtime kata-env`` is useful.
