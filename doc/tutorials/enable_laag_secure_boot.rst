.. _enable_laag_secure_boot:

Enable Secure Boot in the Clear Linux User VM
#############################################

Prerequisites
*************

- ACRN Service VM is installed on the KBL NUC.
- ACRN OVMF version is v1.2 or above ( :acrn-issue:`3506` ).
- ACRN DM support OVMF write back ( :acrn-issue:`3413` ).
- ``efi-tools`` and ``sbsigntools`` are installed in the Service VM::

  # swupd bundle-add os-clr-on-clr

Validated versions
******************

- **Clear Linux version:** 31080
- **ACRN-hypervisor tag:** v1.3
- **ACRN-Kernel(Service VM kernel):** 4.19.73-92.iot-lts2018-sos
- **OVMF version:** v1.3

Prepare keys (PK/KEK/DB)
************************

Generate keys
=============

.. _Ubuntu-KeyGeneration:
   https://wiki.ubuntu.com/UEFI/SecureBoot/KeyManagement/KeyGeneration

.. _Windows-secure-boot-key-creation-and-management-guidance:
   https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-secure-boot-key-creation-and-management-guidance

For formal case, key generation and management can be referenced by:
`Ubuntu-KeyGeneration`_ or `Windows-secure-boot-key-creation-and-management-guidance`_.

For testing, the keys can be created on the KBL NUC with these commands:

.. code-block:: none

   $ openssl req -new -x509 -newkey rsa:2048 -subj "/CN=test platform key/" -keyout PK.key -out PK.crt -days 3650 -nodes -sha256
   $ openssl req -new -x509 -newkey rsa:2048 -subj "/CN=test key-exchange-key/" -keyout KEK.key -out KEK.crt -days 3650 -nodes -sha256
   $ openssl req -new -x509 -newkey rsa:2048 -subj "/CN=test signing key/" -keyout db.key -out db.crt -days 3650 -nodes -sha256
   $ cert-to-efi-sig-list -g "$(uuidgen)" PK.crt PK.esl
   $ sign-efi-sig-list -k PK.key -c PK.crt PK PK.esl PK.auth
   $ cert-to-efi-sig-list -g "$(uuidgen)" KEK.crt KEK.esl
   $ sign-efi-sig-list -a -k PK.key -c PK.crt KEK KEK.esl KEK.auth
   $ cert-to-efi-sig-list -g "$(uuidgen)" db.crt db.esl
   $ sign-efi-sig-list -a -k KEK.key -c KEK.crt db db.esl db.auth
   $ openssl x509 -outform DER -in PK.crt -out PK.der
   $ openssl x509 -outform DER -in KEK.crt -out KEK.der
   $ openssl x509 -outform DER -in db.crt -out db.der

The keys to be enrolled in UEFI BIOS: **PK.der**,  **KEK.der**, **db.der**
The keys to sign bootloader or kernel: **db.key**, **db.crt**

Create virtual disk to hold the keys
====================================

Follow these commands to create a virtual disk and copy the keys
generated above:

.. code-block:: none

   $ sudo dd if=/dev/zero of=$PWD/hdd_keys.img bs=1024 count=10240
   $ mkfs.msdos hdd_keys.img
   $ sudo losetup -D
   $ sudo losetup -f -P --show $PWD/hdd_keys.img
   $ sudo mount /dev/loop0 /mnt
   $ sudo cp PK.der KEK.der db.der /mnt
   $ sync
   $ sudo umount /mnt
   $ sudo losetup -d /dev/loop0

Enroll keys in OVMF
===================

#. Customize the ``launch_uos.sh`` script to boot with the virtual disk
   that contains the keys for enrollment:

   .. code-block:: none
      :emphasize-lines: 6,7,9

      $ cp /usr/share/acrn/samples/nuc/launch_uos.sh ./launch_virtual_disk.sh
      $ sudo vim ./launch_virtual_disk.sh

      acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge \
        -s 2,pci-gvt -G "$3" \
        -l com1,stdio \
        -s 5,virtio-console,@pty:pty_port \
        -s 6,virtio-hyper_dmabuf \
        -s 3,virtio-blk,./hdd_keys.img \
        -s 4,virtio-net,tap0 \
        -s 7,virtio-rnd \
        --ovmf w,/usr/share/acrn/bios/OVMF.fd \
        $pm_channel $pm_by_vuart $pm_vuart_node \
        $logger_setting \
        --mac_seed $mac_seed \
        $vm_name
      }

#. Launch the customized script to enroll keys::

      $ sudo ./launch_virtual_disk.sh

#. Type ``exit`` command in UEFI shell.

   .. figure:: images/exit_uefi_shell.png

   |

#. Select **Device Manager** \-\-> **Secure Boot Configuration**.

   .. figure:: images/secure_boot_config_1.png

   |

   .. figure:: images/secure_boot_config_2.png

   |

   .. figure:: images/secure_boot_config_3.png

   |

#. Select **Secure Boot Mode** \-\-> **Custom Mode** \-\-> **Custom Secure Boot Options**.

   .. figure:: images/select_custom_mode.png

   |

   .. figure:: images/enable_custom_boot.png

   |

#. Enroll Keys:

   a. Enroll PK: Select **PK Options** \-\-> **Enroll PK** \-\->
      **Enroll PK Using File** \-\-> **VOLUME** \-\- PK.der \-\-> **Commit Changes and Exit**

   #. Enroll KEK(similar with PK): Select **KEK Options** --> **Enroll KEK** -->
      **Enroll KEK Using File** --> **VOLUME** --> KEK.der --> **Commit Changes and Exit**

   #. Enroll Signatures(similar with PK): Select **DB Options** --> **Enroll Signature** -->
      **Enroll Signature Using File** --> **VOLUME** --> db.der --> **Commit Changes and Exit**

   Example for enrolling the PK file:

   .. figure:: images/enroll_pk_key_1.png

   |

   .. figure:: images/enroll_pk_key_2.png

   |

   .. figure:: images/enroll_pk_key_3.png

   |

   .. figure:: images/enroll_pk_key_4.png

   |

   .. figure:: images/enroll_pk_key_5.png

   |

   .. figure:: images/enroll_pk_key_6.png

   |

#. Press :kbd:`ESC` to go back to the **Secure Boot Configuration** interface.

   Now the **Current Secure Boot State** is **Enabled** and **Attempt Secure Boot** option is selected.

   .. figure:: images/secure_boot_enabled.png

   |

#. Go back to UEFI GUI main interface and select **Reset** to perform a formal
   reset/shutdown to ensure the key enrollment is taking effect in the next boot.

   .. figure:: images/reset_in_bios.png

   |

#. Type ``reset -s`` to shutdown the guest in the UEFI shell.

   .. figure:: images/reset_in_uefi_shell.png

   |

Sign the Clear Linux image
**************************

Follow these commands to sign the Clear Linux VM binaries.

#. Download and decompress the Clear Linux image::

      $ wget https://download.clearlinux.org/releases/31080/clear/clear-31080-kvm.img.xz
      $ unxz clear-31080-kvm.img.xz

#. Download the script to sign image::

      $ wget https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/scripts/sign_image.sh

#. Run the script to sign image.

   .. code-block:: none

      $ sudo sh sign_image.sh clear-31080-kvm.img db.key db.crt
      /mnt/EFI/BOOT/BOOTX64.EFI
      warning: data remaining[93184 vs 105830]: gaps between PE/COFF sections?
      warning: data remaining[93184 vs 105832]: gaps between PE/COFF sections?
      Signing Unsigned original image
      sign /mnt/EFI/BOOT/BOOTX64.EFI succeed
      /mnt/EFI/org.clearlinux/bootloaderx64.efi
      warning: data remaining[1065472 vs 1196031]: gaps between PE/COFF sections?
      warning: data remaining[1065472 vs 1196032]: gaps between PE/COFF sections?
      Signing Unsigned original image
      sign /mnt/EFI/org.clearlinux/bootloaderx64.efi succeed
      /mnt/EFI/org.clearlinux/kernel-org.clearlinux.kvm.5.2.17-389
      Signing Unsigned original image
      sign /mnt/EFI/org.clearlinux/kernel-org.clearlinux.kvm.5.2.17-389 succeed
      /mnt/EFI/org.clearlinux/loaderx64.efi
      warning: data remaining[93184 vs 105830]: gaps between PE/COFF sections?
      warning: data remaining[93184 vs 105832]: gaps between PE/COFF sections?
      Signing Unsigned original image
      sign /mnt/EFI/org.clearlinux/loaderx64.efi succeed

#. You will get the signed Clear Linux image: ``clear-31080-kvm.img.signed``

Boot Clear Linux signed image
*****************************

#. Modify the ``launch_uos.sh`` script to use the signed image.

   .. code-block:: none
      :emphasize-lines: 5,6,8

      $ sudo vim /usr/share/acrn/samples/nuc/launch_uos.sh

      acrn-dm -A -m $mem_size -c $2 -s 0:0,hostbridge \
        -s 2,pci-gvt -G "$3" \
        -l com1,stdio \
        -s 5,virtio-console,@pty:pty_port \
        -s 6,virtio-hyper_dmabuf \
        -s 3,virtio-blk,./clear-31080-kvm.img.signed \
        -s 4,virtio-net,tap0 \
        -s 7,virtio-rnd \
        --ovmf /usr/share/acrn/bios/OVMF.fd \
        $pm_channel $pm_by_vuart $pm_vuart_node \
        $logger_setting \
        --mac_seed $mac_seed \
        $vm_name
      }

#. You may see the UEFI shell boots by default.

   .. figure:: images/uefi_shell_boot_default.png

   |

#. Type ``exit`` to enter Bios configuration.

#. Navigate to the **Boot Manager** and select **UEFI Misc Device** to
   boot the signed Clear Linux image.

#. Login as root and use ``dmesg`` to check the secure boot status on
   the User VM.

   .. code-block:: none
      :emphasize-lines: 2

      root@clr-763e953a125f4bda94dd2efbab77f776 ~ # dmesg | grep Secure
      [    0.001330] Secure boot enabled
