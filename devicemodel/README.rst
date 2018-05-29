ACRN Device Model
#################

Introduction
============
The ACRN Device Model provides **device sharing** capabilities between the Service OS and Guest OSs. It is a component that is used in conjunction with the `ACRN Hypervisor`_ and this is installed within the Service OS. You can find out more about Project ACRN on the `Project ACRN documentation`_ website.


Building the Device Model
=========================

Build dependencies
******************

* For Clear Linux

.. code-block:: console

   sudo swupd bundle-add os-clr-on-clr \
          os-utils-gui-dev

* For CentOS

.. code-block:: console

   sudo yum install gcc \
          libuuid-devel \
          openssl-devel \
          libpciaccess-devel \
          libusb-devel

* For Fedora 27

.. code-block:: console

   sudo dnf install gcc \
          libuuid-devel \
          openssl-devel \
          libpciaccess-devel \
          libusb-devel

Build
*****
To build the Device Model

.. code-block:: console

   make

To clean the build artefacts

.. code-block:: console

   make clean

Runtime dependencies
********************

* On CentOS

.. code-block:: console

   sudo yum install openssl-libs \
                    zlib \
                    libpciaccess \
                    libuuid \
                    libusb

* On Fedora 27

.. code-block:: console

   sudo dnf install openssl-libs \
                    zlib \
                    libpciaccess \
                    libuuid \
                    libusb

.. _`ACRN Hypervisor`: https://github.com/projectacrn/acrn-hypervisor
.. _`Project ACRN documentation`: https://projectacrn.github.io/
