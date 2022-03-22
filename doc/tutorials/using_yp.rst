:orphan:

.. _using_yp:

Using Yocto Project With ACRN
#############################

The `Yocto Project <https://yoctoproject.org>`_ (YP) is an open source
collaboration project that helps developers create custom Linux-based
systems.  The project provides a flexible set of tools and a space where
embedded developers worldwide can share technologies, software stacks,
configurations, and best practices used to create tailored Linux images
for embedded and IoT devices, or anywhere a customized Linux OS is
needed.

Yocto Project layers support the inclusion of technologies, hardware
components, and software components.  Layers are repositories containing
related sets of instructions that tell the Yocto Project build system
what to do.

The meta-acrn Layer
*******************

The meta-acrn layer integrates the ACRN hypervisor with OpenEmbedded,
letting you build your Service VM or Guest VM OS with the Yocto Project.
The `OpenEmbedded Layer Index's meta-acrn entry
<http://layers.openembedded.org/layerindex/branch/master/layer/meta-acrn/>`_
tracks work on this meta-acrn layer and lists the available meta-acrn
recipes including Service and User VM OSs for Linux Kernel 4.19 and 5.4
with the ACRN hypervisor enabled.

Read more about the meta-acrn layer and how to use it, directly from the
`meta-acrn GitHub repo documentation
<https://github.com/intel/meta-acrn/tree/master/docs>`_:

* `Getting Started guide
  <https://github.com/intel/meta-acrn/blob/master/docs/getting-started.md>`_
* `Booting ACRN with Slim Bootloader
  <https://github.com/intel/meta-acrn/blob/master/docs/slimbootloader.md>`_
* `Testing Procedure
  <https://github.com/intel/meta-acrn/blob/master/docs/qa.md>`_
* `References
  <https://github.com/intel/meta-acrn/blob/master/docs/references.md>`_
