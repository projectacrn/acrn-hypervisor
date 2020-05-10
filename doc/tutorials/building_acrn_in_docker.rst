.. _building-acrn-in-docker:

Build ACRN in Docker
####################

This tutorial shows how to build ACRN in a Clear Linux Docker image.

.. rst-class:: numbered-step

Install Docker
**************

#. Install Docker according to the `Docker installation instructions <https://docs.docker.com/install/>`_.
#. If you are behind an HTTP or HTTPS proxy server, follow the
   `HTTP/HTTPS proxy instructions <https://docs.docker.com/config/daemon/systemd/#httphttps-proxy>`_
   to set up an HTTP/HTTPS proxy to pull the Docker image and
   `Configure Docker to use a proxy server <https://docs.docker.com/network/proxy/>`_
   to set up an HTTP/HTTPS proxy for the Docker container.
#. Docker requires root privileges by default.
   Follow these `additional steps <https://docs.docker.com/install/linux/linux-postinstall/>`_
   to enable a non-root user.

   .. note::

      Performing these post-installation steps is not required. If you
      choose not to, add `sudo` in front of every `docker` command in
      this tutorial.

.. rst-class:: numbered-step

Get the Docker Image
********************

Pick one of these two ways to get the Clear Linux Docker image needed to build ACRN.

Get the Docker Image from Docker Hub
====================================

If you're not working behind a corporate proxy server, you can pull a
pre-built Docker image from Docker Hub to your development machine using
this command:

.. code-block:: none

   $ docker pull acrn/clearlinux-acrn-builder:latest

Build the Docker Image from Dockerfile
======================================

Alternatively, you can build your own local Docker image using the
provided Dockerfile build instructions by following these steps.  You'll
need this if you're working behind a corporate proxy.

#. Download `Dockerfile <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/getting-started/Dockerfile>`_
   to your development machine.
#. Build the Docker image:

   If you are behind an HTTP proxy server, use this command,
   with your proxy settings, to let docker build know about the proxy
   configuration for the docker image:

   .. code-block:: none

      $ docker build --build-arg HTTP_PROXY=http://<proxy_host>:<proxy_port> \
      --build-arg HTTPS_PROXY=https://<proxy_host>:<proxy_port> \
      -t clearlinux-acrn-builder:latest -f <path/to/Dockerfile> .

   Otherwise, you can simply use this command:

   .. code-block:: none

      $ docker build -t clearlinux-acrn-builder:latest -f <path/to/Dockerfile> .


.. rst-class:: numbered-step

Build ACRN from Source in Docker
********************************

#. Clone the acrn-hypervisor repo:

   .. code-block:: none

      $ mkdir -p ~/workspace && cd ~/workspace
      $ git clone https://github.com/projectacrn/acrn-hypervisor
      $ cd acrn-hypervisor

#. Build the acrn-hypervisor with the default configuration (Software Defined Cockpit [SDC] configuration):

   For the Docker image build from Dockerfile, use this command to build ACRN:

   .. code-block:: none

      $ docker run -u`id -u`:`id -g` --rm -v $PWD:/workspace \
        clearlinux-acrn-builder:latest bash -c "make clean && make"

   For the Docker image downloaded from Docker Hub, use this command to build ACRN:

   .. code-block:: none

      $ docker run -u`id -u`:`id -g` --rm -v $PWD:/workspace \
        acrn/clearlinux-acrn-builder:latest bash -c "make clean && make"

   The build artifacts are found in the `build` directory.

.. rst-class:: numbered-step

Build the ACRN Service VM Kernel in Docker
******************************************

#. Clone the acrn-kernel repo:

   .. code-block:: none

      $ mkdir -p ~/workspace && cd ~/workspace
      $ git clone https://github.com/projectacrn/acrn-kernel
      $ cd acrn-kernel

#. Build the ACRN Service VM kernel:

   For the Docker image built from Dockerfile, use this command to build ACRN:

   .. code-block:: none

      $ cp kernel_config_sos .config
      $ docker run -u`id -u`:`id -g` --rm -v $PWD:/workspace \
        clearlinux-acrn-builder:latest \
        bash -c "make clean && make olddefconfig && make && make modules_install INSTALL_MOD_PATH=out/"

   For the Docker image downloaded from Docker Hub, use this command to build ACRN:

   .. code-block:: none

      $ cp kernel_config_sos .config
      $ docker run -u`id -u`:`id -g` --rm -v $PWD:/workspace \
        acrn/clearlinux-acrn-builder:latest \
        bash -c "make clean && make olddefconfig && make && make modules_install INSTALL_MOD_PATH=out/"

   The commands build the bootable kernel image as ``arch/x86/boot/bzImage``,
   and the loadable kernel modules under the ``./out/`` folder.

.. rst-class:: numbered-step

Build the ACRN User VM PREEMPT_RT Kernel in Docker
**************************************************

#. Clone the preempt-rt kernel repo:

   .. code-block:: none

      $ mkdir -p ~/workspace && cd ~/workspace
      $ git clone -b 4.19/preempt-rt https://github.com/projectacrn/acrn-kernel preempt-rt
      $ cd preempt-rt

#. Build the ACRN User VM PREEMPT_RT kernel:

   For the Docker image built from Dockerfile, use this command to build ACRN:

   .. code-block:: none

      $ cp x86-64_defconfig .config
      $ docker run -u`id -u`:`id -g` --rm  -v $PWD:/workspace \
        clearlinux-acrn-builder:latest \
        bash -c "make clean && make olddefconfig && make && make modules_install INSTALL_MOD_PATH=out/"

   For the Docker image downloaded from Docker Hub, use this command to build ACRN:

   .. code-block:: none

      $ cp x86-64_defconfig .config
      $ docker run -u`id -u`:`id -g` --rm  -v $PWD:/workspace \
        acrn/clearlinux-acrn-builder:latest \
        bash -c "make clean && make olddefconfig && make && make modules_install INSTALL_MOD_PATH=out/"

   The commands build the bootable kernel image as ``arch/x86/boot/bzImage``,
   and the loadable kernel modules under the ``./out/`` folder.

.. rst-class:: numbered-step

Build the ACRN documentation
****************************

#. Make sure you have both the ``acrn-hypervisor`` and ``acrn-kernel``
   repositories already available in your workspace (see steps above for
   instructions on how to clone them).

#. Build the ACRN documentation:

   .. code-block:: none

      $ cd ~/workspace
      $ docker run -u`id -u`:`id -g` --rm  -v $PWD:/workspace \
        acrn/clearlinux-acrn-builder:latest \
        bash -c "cd acrn-hypervisor && make clean && make doc"

   The HTML documentation can be found in ``acrn-hypervisor/build/doc/html``
