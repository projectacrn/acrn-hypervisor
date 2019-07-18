.. _sign_clear_linux_image:

How to sign binaries of the Clear Linux image
#############################################

In this tutorial, you will see how to sign the binaries of a Clear Linux image so that you can
boot it through a secure boot enabled OVMF.

Prerequisites
*************
* Install **sbsigntool** on Ubuntu (Verified on 18.04)::

  $ sudo apt install sbsigntool

* Download and extract the Clear Linux image from the `release <https://cdn.download.clearlinux.org/releases/>`_::

  $ export https_proxy=<your https proxy>:<port>
  $ wget https://cdn.download.clearlinux.org/releases/29880/clear/clear-29880-kvm.img.xz
  $ unxz clear-29880-kvm.img.xz

* Download script `sign_image.sh
  <https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/scripts/>`_ on Ubuntu.

Steps to sign the binaries of the Clear Linux image
***************************************************
#. Follow the `KeyGeneration <https://wiki.ubuntu.com/UEFI/SecureBoot/KeyManagement/KeyGeneration>`_ to generate
   the key and certification which will be used to sign the binaries.

#. Get these files from the previous step:

   * archive-subkey-private.key
   * archive-subkey-public.crt

#. Use the script to sign binaries in the Clear Linux image::

   $ sudo sh sign_image.sh $PATH_TO_CLEAR_IMAGE $PATH_TO_KEY $PATH_TO_CERT

#. **clear-xxx-kvm.img.signed** will be generated in the same folder as the original clear-xxx-kvm.img.