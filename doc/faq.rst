.. _faq:

Frequently Asked Questions
##########################

Here are some frequently asked questions about the ACRN project.

.. contents::
   :local:
   :backlinks: entry


What Hardware Does ACRN Support?
********************************

ACRN runs on Intel-based boards, as documented in
our :ref:`hardware` documentation.

.. _config_32GB_memory:

How Do I Configure ACRN's Memory Size?
**************************************

It's important that the ACRN configuration settings are aligned with the
physical memory on your platform. Check the documentation for these
option settings for details:

* :option:`hv.MEMORY.PLATFORM_RAM_SIZE`
* :option:`hv.MEMORY.HV_RAM_SIZE`

Check the :ref:`acrn_configuration_tool` for more information on how
to adjust these settings.

Why Does ACRN Need to Know How Much RAM the System Has?
*******************************************************

Configuring ACRN at compile time with the system RAM size is a tradeoff between
flexibility and functional safety certification. For server virtualization, one
binary is typically used for all platforms with flexible configuration options
given at run time. But, for IoT applications, the image is typically configured
and built for a particular product platform and optimized for that product.

Important features for ACRN include Functional Safety (FuSa) and real-time
behavior. FuSa requires a static allocation policy to avoid the potential of
dynamic allocation failures. Real-time applications similarly benefit from
static memory allocation. This is why ACRN removed all ``malloc()``-type code,
and why it needs to pre-identify the size of all buffers and structures used in
the Virtual Memory Manager. For this reason, knowing the available RAM size at
compile time is necessary to statically allocate memory usage.
