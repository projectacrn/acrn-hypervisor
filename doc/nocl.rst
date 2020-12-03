:orphan:

.. _nocl:

.. comment This page is a common place holder for references to /latest/
   documentation that was removed from the 2.2 release but there are
   lingering references to these docs out in the wild and in the Google
   index. Give the reader a reference to the /2.1/ document instead.

This document was removed
#########################

.. raw:: html

    <script>
       document.write("<p>The document you tried to access is " +
         "obsolete and was removed, but can still be found " +
         "<a href=\"" +
          document.referrer.replace("/latest/","/2.1/") +
          "\">here in the v2.1 documentation</a> archive.</p>")
    </script>


In ACRN v2.2, deprivileged boot mode is no longer the default and will
be completely removed in ACRN v2.3. We're focusing instead on using
multiboot2 boot (via Grub). Multiboot2 is not supported in Clear Linux
though, so we're removing Clear Linux as the Service VM of choice and
with that, tutorial documentation about Clear Linux.
