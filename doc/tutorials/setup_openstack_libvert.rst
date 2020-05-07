.. _setup_openstack_libvert:

Configure ACRN using OpenStack and libvirt
##########################################

Introduction
************

This document provides instructions for setting up libvirt to configure ACRN. We use OpenStack to use libvirt. Install OpenStack in a container to avoid crashing your system and to take advantage of easy snapshots/restores so that you can quickly roll back your system in the event of setup failure. Install OpenStack directly on Ubuntu **only** if you have a dedicated testing machine. This setup utilizes LXC/LXD on Ubuntu 16.04/18.04.

Install ACRN
************

#. Install ACRN using Ubuntu 16.04 or 18.04 as its Service VM. Refer to :ref:`Ubuntu Service OS`.

#. Make acrn-kernel; use the ``kernel_config_uefi_sos`` file located in the ``acrn-kernel`` repo.

#. Add the following kernel bootarg to give SOS more loop devices. Refer to `Kernel Boot Parameters <https://wiki.ubuntu.com/Kernel/KernelBootParameters>`_ documentation.

   ``max_loop=16``

#. Boot the Service VM with the new ``acrn-kernel`` using the ACRN
   hypervisor.
#. Use ``losetup -a`` to verify that Ubuntu's snap service is **not** using
   all available loop devices. Typically, OpenStack needs at least 4 available loop devices. Follow the `snaps guide <https://maslosoft.com/kb/how-to-clean-old-snaps/>`_ to clean up old snap revisions if you're running out of loop devices.
#. Make sure ``acrn-br0`` is created. If not, create it. Refer to :ref:`Enable network sharing <enable-network-sharing-user-vm>`.

Set up and launch LXC/LXD
*************************

1. Set up the LXC/LXD container engine. Use the `instructions <https://ubuntu.com/tutorials/tutorial-setting-up-lxd-1604>`_ provided by Ubuntu.

   Refer to the following information:

   - Answer ``dir`` when prompted for the name of the storage backend to use.
   - Make sure ``lxc-checkconfig | grep missing`` does not show any missing
     kernel features.
   - Disregard ZFS utils (they are not necessary).
   - Set up ``lxdbr0`` as instructed.

2. Create an Ubuntu Bionic container named **openstack**:

   ``lxc init ubuntu:18.04 openstack``

3. Export the kernel interfaces necessary to launch a Service VM in the
   **openstack** container:

   a. Edit the **openstack** config file:

      ``lxc config edit openstack``

      In the editor, add the following lines under **config**:

      .. code-block:: none

         linux.kernel_modules: iptable_nat, ip6table_nat, ebtables, openvswitch
         raw.lxc: |-
           lxc.cgroup.devices.allow = c 10:237 rwm
           lxc.cgroup.devices.allow = b 7:* rwm
           lxc.cgroup.devices.allow = c 243:0 rwm
           lxc.mount.entry = /dev/net/tun dev/net/tun none bind,create=file 0 0
           lxc.mount.auto=proc:rw sys:rw cgroup:rw
         security.nesting: "true"
         security.privileged: "true"

      Save and exit the editor.

   b. lxc config device add openstack eth1 nic name=eth1 nictype=bridged parent=acrn-br0
   c. lxc config device add openstack acrn_vhm unix-char path=/dev/acrn_vhm
   d. lxc config device add openstack loop-control unix-char path=/dev/loop-control
   e. for n in {0..15}; do lxc config device add openstack loop$n unix-block path=/dev/loop$n; done;

4. Launch the **openstack** container:

   ``lxc start openstack``

5. Log in to the **openstack** container:

   ``lxc exec openstack -- su -l``

6. Let ``systemd`` manage **eth1** in the container, with **eth0** as the
   default route:

   Edit ``/etc/netplan/50-cloud-init.yaml``

   .. code-block:: none

      network:

          version: 2

          ethernets:

              eth0:

                  dhcp4: true

              eth1:

                  dhcp4: true

                  dhcp4-overrides:

                      route-metric: 200


7. Log out and restart the **openstack** container:

   ``lxc restart openstack``

8. Log in to the **openstack** container:

   ``lxc exec openstack -- su -l``

9. Set up the proxy inside the **openstack** container via ``/etc/environment``
   Make sure ``no_proxy`` is properly set up in ``/etc/environment`` inside the container. Both IP addresses assigned to **eth0** and **eth1** and their subnets must be included. For example:

   ``no_proxy=xcompany.com,.xcompany.com,10.0.0.0/8,192.168.0.0/16,localhost,.local,127.0.0.0/8,134.134.0.0/16``

10. Add a new user named **stack** and set permissions:

    ``sudo useradd -s /bin/bash -d /opt/stack -m stack``

    ``echo "stack ALL=(ALL) NOPASSWD: ALL" >> /etc/sudoers``

11. Log out and restart the **openstack** container:

    ``lxc restart openstack``

The **openstack** container is now properly configured for OpenStack. Use ``lxc list`` to verify that both **eth0** and **eth1** appear in the container.

Set up ACRN prerequisites inside the container
**********************************************

1. Log in to the **openstack** container as **stack** user:

   ``lxc exec openstack -- su -l stack``

2. Download and compile ACRN’s source code. Refer to :ref:`getting-started-building`.

   .. note::
      All tools and build dependencies must be installed before you run the first ``make`` command.

   - make
   - cd misc/acrn-manager/; make
   - Install only the user-space components (acrn-dm/acrnctl/acrnd)

3. Download, compile, and install ``iasl``. Refer to :ref:`Prepare the User VM <prepare-UOS>`.

Set up libvirt
**************

1. Install the required packages:

   ``sudo apt install libdevmapper-dev libnl-route-3-dev libnl-3-dev python automake autoconf autopoint libtool xsltproc libxml2-utils gettext``

2. Download libvirt/ACRN:

   ``git clone https://github.com/projectacrn/acrn-libvirt.git``

3. Go to the libvirt directory (``cd libvirt``) and enter the following:

   ``./autogen.sh --prefix=/usr --disable-werror --with-test-suite=no --with-qemu=no --with-openvz=no --with-vmware=no --with-phyp=no --with-vbox=no --with-lxc=no --with-uml=no --with-esx=no``

   ``make``

   ``sudo make install``

4. Edit and enable these options in ``/etc/libvirt/libvirtd.conf``:

   ``unix_sock_ro_perms = "0777"``

   ``unix_sock_rw_perms = "0777"``

   ``unix_sock_admin_perms = "0777"``

5. Run the following command:

   ``sudo systemctl daemon-reload``


Set up OpenStack
****************

Use DevStack to install OpenStack. Refer to the `DevStack instructions <https://docs.openstack.org/devstack/>`_.

1. Use the latest maintenance branch **stable/train** to ensure OpenStack
   stability:

   ``git clone https://opendev.org/openstack/devstack.git -b stable/train``

2. Go to the devstack directory (``cd devstack``) and apply the following
   patch:

   ``0001-devstack-installation-for-acrn.patch``

3. Edit ``lib/nova_plugins/hypervisor-libvirt``:

   Change ``xen_hvmloader_path`` to the location of your OVMF image file. A stock image is included in the ACRN source tree (``devicemodel/bios/OVMF.fd``).

4. Copy the attached ``local.conf`` to ``devstack/``.

.. Note::
   Now is a great time to take a snapshot of the container using ``lxc snapshot``. If the OpenStack installation fails, manually rolling back to the previous state can be difficult. Currently, no step exists to reliably restart OpenStack after restarting the container.

5. Install OpenStack:

   ``execute ./stack.sh in devstack/``

   The installation should take about 20-30 minutes. Upon successful installation, the installer reports the URL of OpenStack’s management interface. This URL is accessible from the native Ubuntu.

   .. code-block:: none

      …

      Horizon is now available at http://<IP_address>/dashboard

      …

      2020-04-09 01:21:37.504 | stack.sh completed in 1755 seconds.

6. Verify in ``systemctl status libvirtd.service`` that libvirtd is active
   and running.

7. Set up SNAT for OpenStack instances to connect to the external network.

   a. Inside the container, use ``ip a`` to identify the ``br-ex`` bridge
      interface. ``br-ex`` should have two IPs. One should be visible to the native Ubuntu’s ``acrn-br0`` interface (e.g. inet 192.168.1.104/24). The other one is internal to OpenStack (e.g. inet 172.24.4.1/24). The latter corresponds to the public network in OpenStack.

   b. Set up SNAT to establish a link between ``acrn-br0`` and OpenStack.
      For example:

      ``sudo iptables -t nat -A POSTROUTING -s 172.24.4.1/24 -o br-ex -j SNAT --to-source 192.168.1.104``

Final Steps
***********

1. Create OpenStack instances.

   - OpenStack logs to systemd journal
   - libvirt logs to /var/log/libvirt/libvirtd.log

   You can now use the URL to manage OpenStack in your native Ubuntu:
     admin/intel123

2. Create a router between **public** (external network) and **shared**
   (internal network) using `OpenStack's network instructions <https://docs.openstack.org/openstackdocstheme/latest/demo/create_and_manage_networks.html>`_.


3. Launch an ACRN instance using `OpenStack's launch instructions <https://docs.openstack.org/horizon/latest/user/launch-instances.html>`_.

   - Use Clear Linux Cloud Guest as the image (qcow2 format):
     https://clearlinux.org/downloads
   - Skip **Create Key Pair** as it’s not supported by Clear Linux.
   - Select **No** for **Create New Volume** when selecting the instance
     boot source image.
   - Use **shared** as the instance’s network.

4. After the instance is created, use the hypervisor console to verify that
   it is running (``vm_list``).

5. Ping the instance inside the container using the instance’s floating IP
   address.

6. Clear Linux prohibits root SSH login by default. Use the ``virsh``
   console to configure the instance. Inside the container, run:

   ``sudo virsh -c acrn:///system``

   ``list`` (you should see the instance listed as running)

   ``console <instance_name>``

7. Log in to the Clear Linux instance and set up the root SSH. Refer to the
   Clear Linux instructions on `enabling root login <https://docs.01.org/clearlinux/latest/guides/network/openssh-server.html#enable-root-login>`_.

   a. Set up the proxy inside the instance.
   b. Configure ``systemd-resolved`` to use the correct DNS server.
   c. Install ping: ``swupd bundle-add clr-network-troubleshooter``.

   The ACRN instance should now be able to ping ``acrn-br0`` and another ACRN instance. It should also be accessible inside the container via SSH and its floating IP address.

The ACRN instance can be deleted via the OpenStack management interface.
For more advanced CLI usage, refer to this `OpenStack cheat sheet <https://docs.openstack.org/ocata/user-guide/cli-cheat-sheet.html>`_.