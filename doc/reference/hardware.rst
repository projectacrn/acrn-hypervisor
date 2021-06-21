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
| Processor              | Compatible x86 64-bit processor   | 2 core with Intel Hyper-threading Technology enabled in the BIOS or more cores  |
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

Platforms with multiple PCI segments are not supported.

ACRN assumes the following conditions are satisfied from the Platform BIOS:

* All the PCI device BARs must be assigned resources, including SR-IOV VF BARs if a device supports it.

* Bridge windows for PCI bridge devices and the resources for root bus must be programmed with values
  that enclose resources used by all the downstream devices.

* There should be no conflict in resources among the PCI devices or with other platform devices.



Tested Platforms by ACRN Release
********************************

These platforms have been tested by the development team with the noted ACRN
release version and may not work as expected on later ACRN releases.

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

.. _NUC7i7DNH:
   https://ark.intel.com/content/www/us/en/ark/products/130393/intel-nuc-kit-nuc7i7dnhe.html

.. _WHL-IPC-I7:
   http://www.maxtangpc.com/industrialmotherboards/142.html#parameters

.. _UP2 Shop:
   https://up-shop.org/home/270-up-squared.html


For general instructions setting up ACRN on supported hardware platforms, visit the :ref:`rt_industry_ubuntu_setup` page.

.. list-table:: Supported Target Platforms
  :widths: 20 20 12 5 5
  :header-rows: 1

  * - Intel x86 Platform Family
    - Product / Kit Name
    - Board configuration
    - ACRN Release
    - Graphics
  * - **Tiger Lake**
    - `NUC11TNHi5`_ |br| (Board: NUC11TNBi5)
    - :acrn_file:`nuc11tnbi5.xml <misc/config_tools/data/nuc11tnbi5/nuc11tnbi5.xml>`
    - v2.5
    - GVT-d
  * - **Whiskey Lake**
    - `WHL-IPC-I7`_ |br| (Board: WHL-IPC-I7)
    - :acrn_file:`whl-ipc-i7.xml <misc/config_tools/data/whl-ipc-i7/whl-ipc-i7.xml>`
    - v2.0
    - GVT-g
  * - **Kaby Lake** |br| (Codename: Dawson Canyon)
    - `NUC7i7DNH`_ |br| (board: NUC7i7DNB)
    - :acrn_file:`nuc7i7dnb.xml <misc/config_tools/data/nuc7i7dnb/nuc7i7dnb.xml>`
    - v1.6.1
    - GVT-g
  * - **Apollo Lake**
    - `NUC6CAYH`_, |br| `UP2-N3350 <UP2 Shop>`_, |br| `UP2-N4200, UP2-x5-E3940 <UP2 Shop>`_
    - 
    - v1.0
    - GVT-g

If an XML file is not provided by project ACRN for your board, we recommend you
use the board inspector tool to generate an XML file specifically for your board.
Refer to the :ref:`acrn_configuration_tool` for more details on using the board inspector
tool.


Tested Hardware Specifications Detail
*************************************

+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
|   Platform (Intel x86)         |   Product/Kit Name     |   Hardware Class       |   Description                                             |
+================================+========================+========================+===========================================================+
| | **Tiger Lake**               | | NUC11TNHi5           | Processor              | -  Intel® Core™ i5-113G7 CPU (8M Cache, up to 4.2 GHz)    |
| |                              | | (Board: NUC11TNBi5)  |                        |                                                           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Dual HDMI 2.0b w/HDMI CEC, Dual DP 1.4a via Type C     |
|                                |                        |                        | -  Supports 4 displays                                    |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 64 GB, 3200 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector for storage                          |
|                                |                        |                        |    22x80 NVMe (M), 22x42 SATA (B)                         |
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
|                                |                        | Storage capabilities   | -  One M.2 connector for Wi-Fi                            |
|                                |                        |                        | -  One M.2 connector for 3G/4G module, supporting         |
|                                |                        |                        |    LTE Category 6 and above                               |
|                                |                        |                        | -  One M.2 connector for 2242 SSD                         |
|                                |                        |                        | -  TWO SATA3 port (only one if Celeron onboard)           |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Kaby Lake**                | | NUC7i7DNH            | Processor              | -  Intel® Core™ i7-8650U Processor                        |
| | (Code name: Dawson Canyon)   | | (Board: NUC7i7DNB)   |                        |    (8M Cache, up to 4.2 GHz)                              |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Graphics               | -  Dual HDMI 2.0a, 4-lane eDP 1.4                         |
|                                |                        |                        | -  Supports 2 displays                                    |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Two DDR4 SO-DIMM sockets (up to 32 GB, 2400 MHz), 1.2V |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  One M.2 connector supporting 22x80 M.2 SSD             |
|                                |                        |                        | -  One M.2 connector supporting 22x30 M.2 card            |
|                                |                        |                        | -  One SATA3 port for connection to 2.5" HDD or SSD       |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+
| | **Apollo Lake**              | | NUC6CAYH             | Processor              | -  Intel® Celeron™ CPU J3455 @ 1.50GHz (4C4T)             |
| | (Code name: Arches Canyon)   | | (Board: NUC6CAYB)    |                        |                                                           |
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
|                                |                        | Graphics               | -  2GB (single channel) LPDDR4                            |
|                                |                        |                        | -  4GB/8GB (dual channel) LPDDR4                          |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | System memory          | -  Intel® Gen 9 HD, supporting 4K Codec                   |
|                                |                        |                        |    Decode and Encode for HEVC4, H.264, VP8                |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Storage capabilities   | -  32 GB / 64 GB / 128 GB eMMC                            |
|                                |                        +------------------------+-----------------------------------------------------------+
|                                |                        | Serial Port            | -  Yes                                                    |
+--------------------------------+------------------------+------------------------+-----------------------------------------------------------+



.. # vim: tw=200
