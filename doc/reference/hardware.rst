.. _hardware:

Supported Hardware
##################

We welcome community contributions to help build Project ACRN support
for a broad collection of architectures and platforms.

Minimum System Requirements for Installing ACRN
***********************************************

+------------------------+-----------------------------------+---------------------------------+
| Hardware               | Minimum Requirements              | Recommended                     |
+========================+===================================+=================================+
| Processor              | Compatible x86 64-bit processor   | 4 core or more                  |
+------------------------+-----------------------------------+---------------------------------+
| System memory          | 4GB RAM                           | 8GB or more (< 32G)             |
+------------------------+-----------------------------------+---------------------------------+
| Storage capabilities   | 20GB                              | 120GB or more                   |
+------------------------+-----------------------------------+---------------------------------+

Verified Platforms According to ACRN Usage
******************************************

These Apollo Lake and Kaby Lake platforms have been verified by the
development team for Software-Defined Cockpit (SDC), Industrial Usage
(IU), and Logical Partition scenarios.

.. _NUC6CAYH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/nuc6cayh.html

.. _NUC7i5BNH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/NUC7i5BNH.html

.. _NUC7i7BNH:
   https://www.intel.com/content/www/us/en/products/boards-kits/nuc/kits/NUC7i7BNH.html

.. _NUC7i5DNH:
   https://ark.intel.com/content/www/us/en/ark/products/122488/intel-nuc-kit-nuc7i5dnhe.html

.. _NUC7i7DNH:
   https://ark.intel.com/content/www/us/en/ark/products/130393/intel-nuc-kit-nuc7i7dnhe.html

.. _UP2 Shop:
   https://up-shop.org/home/270-up-squared.html


For general instructions setting up ACRN on supported hardware platforms, visit the :ref:`getting-started-apl-nuc`
and :ref:`getting-started-up2` pages.



+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
|   Platform (Intel x86)         |   Product/Kit Name      |               Usage Scenerio - BKC Examples                   |
|                                |                         +-----------+-----------+-------------+------------+------------+
|                                |                         | SDC with  | SDC with  | IU without  | IU with    | Logical    |
|                                |                         | 2 VMs     | 4 VMs     | Safety VM   | Safety VM  | Partition  |
|                                |                         |           |           |             |            |            |
+================================+=========================+===========+===========+=============+============+============+
| | **Apollo Lake**              | | `NUC6CAYH`_           | V         | V         | V           | V          |            |
| | (Formal name: Arches Canyon  | | (Board: NUC6CAYB)     |           |           |             |            |            |
|                                |                         |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
| **Apollo Lake**                | | UP2 - N3350           | V         |           |             |            |            |
|                                | | UP2 - N4200           |           |           |             |            |            |
|                                | | UP2 - x5-E3940        |           |           |             |            |            |
|                                | | (see `UP2 Shop`_)     |           |           |             |            |            |
|                                |                         |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
| | **Kaby Lake**                | | `NUC7i5BNH`_          | V         |           |             |            |            |
| | (Codename: Baby Canyon)      | | (Board: NUC7i5BNB)    |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
| | **Kaby Lake**                | | `NUC7i7BNH`_          | V         |           |             |            |            |
| | (Codename: Baby Canyon)      | | (Board: NUC7i7BNB     |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
| | **Kaby Lake**                | | `NUC7i5DNH`_          | V         |           |             |            |            |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i5DNB)    |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+
| | **Kaby Lake**                | | `NUC7i7DNH`_          | V         | V         | V           | V          | V          |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i7DNB)    |           |           |             |            |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+------------+

V: Verified by engineering team; remaining scenarios are not in verification scope

Verified Hardware Specifications Detail
***************************************

+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
|   Platform (Intel x86)         |   Product/Kit Name     |   Hardware Class       |   Description                                             |
+================================+========================+========================+===========================================================+
| | **Apollo Lake**              | | NUC6CAYH             | Processor              | -  Intel(R) Celeron(R) CPU J3455 @ 1.50GHz                |
| | (Formal name: Arches Canyon) | | (Board: NUC6CAYB)    |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Intel HD Graphics 500                                  |
|                                |                        |                        | -  VGA (HDB15); HDMI 2.0                                  |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR3L SO-DIMM sockets                              |
|                                |                        |                        |    (up to 8 GB, 1866 MHz), 1.35V                          |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  SDXC slot with UHS-I support on the side               |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness)                               |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  No                                                     |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Apollo Lake**              | | UP2 - N3350          | Processor              | -  Intel® Celeron™ N3350 (up to 2.4 GHz)                  |
|                                | | UP2 - N4200          |                        | -  Intel® Pentium™ N4200 (up to 2.5 GHz)                  |
|                                | | UP2 - x5-E3940       |                        | -  Intel® Atom ™ x5-E3940                                 |
|                                |                        |                        |    (up to 1.8Ghz)/x7-E3950 ( up to 2.0GHz)                |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  2GB ( single channel) LPDDR4                           |
|                                |                        |                        | -  4GB/8GB ( dual channel) LPDDR4                         |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Intel® Gen 9 HD, supporting 4K Codec                   |
|                                |                        |                        |    Decode and Encode for HEVC4, H.264, VP8                |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  32 GB / 64 GB / 128 GB eMMC                            |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i5BNH            | Processor              | -  Intel(R) Core(TM) i5-7260U CPU @ 2.20GHz               |
| | (Codename: Baby Canyon)      | | (Board: NUC7i5BNB)   |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Intel® Iris™ Plus Graphics 640                         |
|                                |                        |                        | -  One HDMI\* 2.0 port with 4K at 60 Hz                   |
|                                |                        |                        | -  Thunderbolt™ 3 port with support for USB\* 3.1         |
|                                |                        |                        |    Gen 2, DisplayPort\* 1.2 and 40 Gb/s Thunderbolt       |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2133 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  Micro SDXC slot with UHS-I support on the side         |
|                                |                        |                        | -  One M.2 connector supporting 22x42 or 22x80 M.2 SSD    |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness)                               |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  No                                                     |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i7BNH            | Processor              | -  Intel(R) Core(TM) i7-7567U CPU @ 3.50GHz               |
| | (Codename: Baby Canyon)      | | (Board: NUC7i7BNB)   |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Intel® Iris™ Plus Graphics 650                         |
|                                |                        |                        | -  One HDMI\* 2.0 port with 4K at 60 Hz                   |
|                                |                        |                        | -  Thunderbolt™ 3 port with support for USB\* 3.1 Gen 2,  |
|                                |                        |                        |    DisplayPort\* 1.2 and 40 Gb/s Thunderbolt              |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2133 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  Micro SDXC slot with UHS-I support on the side         |
|                                |                        |                        | -  One M.2 connector supporting 22x42 or 22x80 M.2 SSD    |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness)                               |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  No                                                     |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i5DNH            | Processor              | -  Intel(R) Core(TM) i5-7300U CPU @ 2.64GHz               |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i5DNB)   |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Intel® HD Graphics 620                                 |
|                                |                        |                        | -  Two HDMI\* 2.0a ports supporting 4K at 60 Hz           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2133 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector supporting 22x80 M.2 SSD             |
|                                |                        |                        | -  One M.2 connector supporting 22x30 M.2 card            |
|                                |                        |                        |    (NUC7i5DNBE only)                                      |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness) (NUC7i5DNHE only)             |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  No                                                     |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i7DNH            | Processor              | -  Intel(R) Core(TM) i7-8650U CPU @ 1.90GHz               |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i7DNB)   |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  UHD Graphics 620                                       |
|                                |                        |                        | -  Two HDMI\* 2.0a ports supporting 4K at 60 Hz           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2400 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector supporting 22x80 M.2 SSD             |
|                                |                        |                        | -  One M.2 connector supporting 22x30 M.2 card            |
|                                |                        |                        |    (NUC7i7DNBE Only)                                      |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness) (NUC7i7DNHE only)             |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+

.. # vim: tw=200
