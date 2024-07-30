.. _hardware:

Supported Hardware
##################

The ACRN project development team is continually adding support for new hardware
products, as documented below. As we add new hardware, we also lower our support
level for older hardware products. We welcome community contributions to help
build ACRN support for a broad collection of architectures and platforms.

.. _hardware_tested:

Selecting Hardware
******************

When you are selecting hardware to use with ACRN, consider the
following:

* When the development team is working on a new ACRN version, we focus our
  development and testing on one product. The product is typically a board
  or kit from the latest processor family.

* We also provide a level of maintenance for some older products.

* For all products, we welcome and encourage the community to contribute support
  by submitting patches for code, documentation, tests, and more.

The following table shows supported processor families, along with the
products that the development team has tested. The products are categorized
into three support levels: Release, Maintenance, and Community. Each
level includes the activities described in the lower levels.

.. _NUC11TNHi5:
   https://ark.intel.com/content/www/us/en/ark/products/205594/intel-nuc-11-pro-kit-nuc11tnhi5.html

.. _NUC6CAYH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc6cayh.html

.. _NUC7i5BNH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/NUC7i5BNH.html

.. _NUC7i7BNH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/NUC7i7BNH.html

.. _NUC7i5DNH:
   https://ark.intel.com/content/www/us/en/ark/products/122488/intel-nuc-kit-nuc7i5dnhe.html

.. _NUC7i7DNHE:
   https://ark.intel.com/content/www/us/en/ark/products/130393/intel-nuc-kit-nuc7i7dnhe.html

.. _WHL-IPC-I5:
   http://www.maxtangpc.com/industrialmotherboards/142.html#parameters

.. _Vecow SPC-7100:
   https://marketplace.intel.com/s/offering/a5b3b000000PReMAAW/vecow-spc7100-series-11th-gen-intel-core-i7i5i3-processor-ultracompact-f

.. _UP2-N3350:
.. _UP2-N4200:
.. _UP2-x5-E3940:
.. _UP2 Shop:
   https://up-shop.org/home/270-up-squared.html

.. _ASRock iEPF-9010S-EY4:
   https://www.asrockind.com/en-gb/iEPF-9010S-EY4

.. _ASRock iEP-9010E:
   https://www.asrockind.com/en-gb/iEP-9010E

.. _ASUS PN64-E1:
   https://www.asus.com/displays-desktops/mini-pcs/pn-series/asus-expertcenter-pn64-e1/

.. important::
   We recommend you use a system configuration that includes a serial port.

.. # Note For easier editing, I'm using unicode non-printing spaces in this table to help force the width of the first two columns to help prevent wrapping (using &nbsp; isn't compact enough)

+------------------------+---------------------------------+-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
|                        |                                 | .. rst-class::                                                                                                                                                                                        |
|                        |                                 |    centered                                                                                                                                                                                           |
|                        |                                 |                                                                                                                                                                                                       |
|                        |                                 |    ACRN Version                                                                                                                                                                                       |
|                        |                                 +-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+-------------------+
| Intel Processor Family | Tested Products                 | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    | .. rst-class::    |
| Code Name              |                                 |    centered       |    centered       |    centered       |    centered       |    centered       |    centered       |    centered       |    centered       |    centered       |    centered       |
|                        |                                 |                   |                   |                   |                   |                   |                   |                   |                   |                   |                   |
|                        |                                 |    v1.0           |    v1.6.1         |    v2.0           |    v2.5           |    v2.6           |    v2.7           |    v3.0           |    v3.1           |    v3.2           |    v3.3           |
+========================+=================================+===================+===================+===================+===================+===================+===================+===================+===================+===================+===================+
| Raptor Lake            | `ASUS PN64-E1`_                 |                                                                                                                                                               | .. rst-class::    | .. rst-class::    |
|                        |                                 |                                                                                                                                                               |    centered       |    centered       |
|                        |                                 |                                                                                                                                                               |                   |                   |
|                        |                                 |                                                                                                                                                               |    Community      |    Maintenance    |
+------------------------+---------------------------------+-----------------------------------------------------------------------------------------------------------------------+-------------------+-------------------+-------------------+-------------------+
| Alder Lake             | | `ASRock iEPF-9010S-EY4`_,     |                                                                                                                       | .. rst-class::    | .. rst-class::                                            |
|                        | | `ASRock iEP-9010E`_           |                                                                                                                       |    centered       |    centered                                               |
|                        |                                 |                                                                                                                       |                   |                                                           |
|                        |                                 |                                                                                                                       |    Release        |    Community                                              |
+------------------------+---------------------------------+-----------------------------------------------------------------------------------------------------------------------+-------------------+---------------------------------------+-------------------+
| Tiger Lake             | `Vecow SPC-7100`_               |                                                                                                                       | .. rst-class::                                            | .. rst-class::    |
|                        |                                 |                                                                                                                       |    centered                                               |    centered       |
|                        |                                 |                                                                                                                       |                                                           |                   |
|                        |                                 |                                                                                                                       |    Maintenance                                            |    Community      |
+------------------------+---------------------------------+-----------------------------------------------------------+-------------------+---------------------------------------+-----------------------------------------------------------+-------------------+
| Tiger Lake             | `NUC11TNHi5`_                   |                                                           | .. rst-class::    | .. rst-class::                        | .. rst-class::                                                                |
|                        |                                 |                                                           |    centered       |    centered                           |    centered                                                                   |
|                        |                                 |                                                           |                   |                                       |                                                                               |
|                        |                                 |                                                           |    Release        |    Maintenance                        |    Community                                                                  |
+------------------------+---------------------------------+---------------------------------------+-------------------+-------------------+-------------------+-------------------+-------------------------------------------------------------------------------+
| Whiskey Lake           | `WHL-IPC-I5`_                   |                                       | .. rst-class::    | .. rst-class::                        | .. rst-class::                                                                                    |
|                        |                                 |                                       |    centered       |    centered                           |    centered                                                                                       |
|                        |                                 |                                       |                   |                                       |                                                                                                   |
|                        |                                 |                                       |    Release        |    Maintenance                        |    Community                                                                                      |
+------------------------+---------------------------------+-------------------+-------------------+-------------------+-------------------+-------------------+---------------------------------------------------------------------------------------------------+
| Kaby Lake              | `NUC7i7DNHE`_                   |                   | .. rst-class::    | .. rst-class::                        | .. rst-class::                                                                                                        |
|                        |                                 |                   |    centered       |    centered                           |    centered                                                                                                           |
|                        |                                 |                   |                   |                                       |                                                                                                                       |
|                        |                                 |                   |    Release        |    Maintenance                        |    Community                                                                                                          |
+------------------------+---------------------------------+-------------------+-------------------+---------------------------------------+-----------------------------------------------------------------------------------------------------------------------+
| Apollo Lake            | | `NUC6CAYH`_,                  | .. rst-class::    | .. rst-class::    | .. rst-class::                                                                                                                                                |
|                        | | `UP2-N3350`_,                 |    centered       |    centered       |    centered                                                                                                                                                   |
|                        | | `UP2-N4200`_,                 |                   |                   |                                                                                                                                                               |
|                        | | `UP2-x5-E3940`_               |    Release        |    Maintenance    |    Community                                                                                                                                                  |
+------------------------+---------------------------------+-------------------+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------+

* **Release**: New ACRN features are complete and tested for the listed product.
  This product is recommended for this ACRN version. Support for older products
  will transition to the maintenance category as development continues for newer
  products.

* **Maintenance**: For new ACRN versions with maintenance-level support, we
  verify our :ref:`gsg` instructions to ensure the baseline development workflow
  works and the hypervisor will boot on the listed products. While we don't
  verify that all new features will work on this product, we will do best-effort
  support on reported issues. Maintenance-level support for a hardware product
  is typically done for two subsequent ACRN releases (about six months).

* **Community**: Community responds with best-effort support for that
  ACRN version to reported bugs for the listed product.

Urgent bug and security fixes are targeted to the latest release only.
Developers should either update to the most current release or back-port these
fixes to their own production release. 

When you start to explore ACRN, we recommend you select
the latest product from the table above. You can also choose
other products and give them a try. In either case, use the
:ref:`board_inspector_tool` to generate a board configuration file
you will use to configure the ACRN hypervisor, as described in the
:ref:`gsg`. We encourage your feedback on the
acrn-user@lists.projectacrn.org mailing list on your findings about
unlisted products.

.. # vim: tw=300
