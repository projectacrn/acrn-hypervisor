.. _hardware:

Supported Hardware
##################

We welcome community contributions to help build Project ACRN support
for a broad collection of architectures and platforms.

Minimum System Requirements for Installing ACRN
***********************************************

+------------------------+-----------------------------------+---------------------------------------------------------------------------------+
| Hardware               | Minimum Requirements              | Recommended                                                                     |
+========================+===================================+=================================================================================+
| Processor              | Compatible x86 64-bit processor   | 2 core with Intel Hyper Threading Technology enabled in the BIOS or more cores  |
+------------------------+-----------------------------------+---------------------------------------------------------------------------------+
| System memory          | 4GB RAM                           | 8GB or more (< 32G)                                                             |
+------------------------+-----------------------------------+---------------------------------------------------------------------------------+
| Storage capabilities   | 20GB                              | 120GB or more                                                                   |
+------------------------+-----------------------------------+---------------------------------------------------------------------------------+

Minimum Requirements for Processor
**********************************
1 GB Large pages

Known Limitations
*****************
Platforms with multiple PCI segments

ACRN assumes the following conditions are satisfied from the Platform BIOS

* All the PCI device BARs should be assigned resources, including SR-IOv VF BARs if a device supports.

* Bridge windows for PCI bridge devices and the resources for root bus, should be programmed with values
  that enclose resources used by all the downstream devices.

* There should be no conflict in resources among the PCI devices and also between PCI devices and other platform devices.


New Processor Families
**********************

Here are announced Intel processor architectures that are supported by ACRN v2.2, but don't yet have a recommended platform available:

* `Tiger Lake <https://ark.intel.com/content/www/us/en/ark/products/codename/88759/tiger-lake.html#@Embedded>`_
  (Q3'2020 Launch Date)
* `Elkhart Lake <https://ark.intel.com/content/www/us/en/ark/products/codename/128825/elkhart-lake.html#@Embedded>`_
  (Q1'2021 Launch Date)


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

.. _WHL-IPC-I5:
   http://www.maxtangpc.com/industrialmotherboards/142.html#parameters

.. _UP2 Shop:
   https://up-shop.org/home/270-up-squared.html


For general instructions setting up ACRN on supported hardware platforms, visit the :ref:`rt_industry_ubuntu_setup` page.


+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
|   Platform (Intel x86)         |   Product/Kit Name      |               Usage Scenario - BKC Examples      |
|                                |                         +-----------+-----------+-------------+------------+
|                                |                         | SDC with  | IU without| IU with     | Logical    |
|                                |                         | 2 VMs     | Safety VM | Safety VM   | Partition  |
|                                |                         |           |           |             |            |
+================================+=========================+===========+===========+=============+============+
| | **Apollo Lake**              | | `NUC6CAYH`_           | V         | V         |             |            |
| | (Formal name: Arches Canyon  | | (Board: NUC6CAYB)     |           |           |             |            |
|                                |                         |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| **Apollo Lake**                | | UP2 - N3350           | V         |           |             |            |
|                                | | UP2 - N4200           |           |           |             |            |
|                                | | UP2 - x5-E3940        |           |           |             |            |
|                                | | (see `UP2 Shop`_)     |           |           |             |            |
|                                |                         |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| | **Kaby Lake**                | | `NUC7i5BNH`_          | V         |           |             |            |
| | (Codename: Baby Canyon)      | | (Board: NUC7i5BNB)    |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| | **Kaby Lake**                | | `NUC7i7BNH`_          | V         |           |             |            |
| | (Codename: Baby Canyon)      | | (Board: NUC7i7BNB     |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| | **Kaby Lake**                | | `NUC7i5DNH`_          | V         |           |             |            |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i5DNB)    |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| | **Kaby Lake**                | | `NUC7i7DNH`_          | V         | V         | V           | V          |
| | (Codename: Dawson Canyon)    | | (Board: NUC7i7DNB)    |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+
| | **Whiskey Lake**             | | `WHL-IPC-I5`_         | V         | V         | V           | V          |
| |                              | | (Board: WHL-IPC-I5)   |           |           |             |            |
+--------------------------------+-------------------------+-----------+-----------+-------------+------------+

V: Verified by engineering team; remaining scenarios are not in verification scope

Verified Hardware Specifications Detail
***************************************

+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
|   Platform (Intel x86)         |   Product/Kit Name     |   Hardware Class       |   Description                                             |
+================================+========================+========================+===========================================================+
| | **Apollo Lake**              | | NUC6CAYH             | Processor              | -  Intel® Celeron™ CPU J3455 @ 1.50GHz (4C4T)             |
| | (Formal name: Arches Canyon) | | (Board: NUC6CAYB)    |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Intel® HD Graphics 500                                 |
|                                |                        |                        | -  VGA (HDB15); HDMI 2.0                                  |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR3L SO-DIMM sockets                              |
|                                |                        |                        |    (up to 8 GB, 1866 MHz), 1.35V                          |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  SDXC slot with UHS-I support on the side               |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        |                        |    (up to 9.5 mm thickness)                               |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  No                                                     |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Apollo Lake**              | | UP2 - N3350          | Processor              | -  Intel® Celeron™ N3350 (2C2T, up to 2.4 GHz)            |
|                                | | UP2 - N4200          |                        | -  Intel® Pentium™ N4200 (4C4T, up to 2.5 GHz)            |
|                                | | UP2 - x5-E3940       |                        | -  Intel® Atom ™ x5-E3940 (4C4T)                          |
|                                |                        |                        |    (up to 1.8GHz)/x7-E3950 (4C4T, up to 2.0GHz)           |
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
| | **Kaby Lake**                | | NUC7i5BNH            | Processor              | -  Intel® Core™ i5-7260U CPU @ 2.20GHz (2C4T)             |
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
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i7BNH            | Processor              | -  Intel® Core™ i7-7567U CPU @ 3.50GHz (2C4T)             |
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
| | **Kaby Lake**                | | NUC7i5DNH            | Processor              | -  Intel® Core™ i5-7300U CPU @ 2.64GHz (2C4T)             |
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
| | **Whiskey Lake**             | | WHL-IPC-I5           | Processor              | -  Intel® Core™ i5-8265U CPU @ 1.60GHz (4C8T)             |
| |                              | | (Board: WHL-IPC-I5)  |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  HD Graphics 610/620                                    |
|                                |                        |                        | -  ONE HDMI\* 1.4a ports supporting 4K at 60 Hz           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2400 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector for WIFI                             |
|                                |                        |                        | -  One M.2 connector for 3G/4G module, supporting         |
|                                |                        |                        |    LTE Category 6 and above                               |
|                                |                        |                        | -  One M.2 connector for 2242 SSD                         |
|                                |                        |                        | -  TWO SATA3 port (only one if Celeron onboard)           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Whiskey Lake**             | | WHL-IPC-I7           | Processor              | -  Intel® Core™ i5-8265U CPU @ 1.80GHz (4C8T)             |
| |                              | | (Board: WHL-IPC-I7)  |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  HD Graphics 610/620                                    |
|                                |                        |                        | -  ONE HDMI\* 1.4a ports supporting 4K at 60 Hz           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2400 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector for WIFI                             |
|                                |                        |                        | -  One M.2 connector for 3G/4G module, supporting         |
|                                |                        |                        |    LTE Category 6 and above                               |
|                                |                        |                        | -  One M.2 connector for 2242 SSD                         |
|                                |                        |                        | -  TWO SATA3 port (only one if Celeron onboard)           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+


.. # vim: tw=200
