# ACRN Debianization

ACRN is a flexible, lightweight reference hypervisor, built with real-time and safety-criticality in mind, optimized to streamline embedded development through an open source platform. This part represents the debianization for ACRN.

### Table of Contents
1. [Building](#building)
2. [Development Build from Source Package](#development-build-from-source-package)
3. [Package Maintenance](#package-maintenance)
4. [Special Package Properties](#special-package-properties)
5. [About initramfs on Debian/Ubuntu Systems](#about-initramfs-on-debianubuntu-systems)
6. [Scenario Configuration - GRUB configuration](#scenario-configuration---grub-configuration)
7. [Known Build Warnings](#known-build-warnings)
8. [General Remarks and Restrictions](#general-remarks-and-restrictions)

The Debian source package `acrn-hypervisor` provides the following Debian packages:

* `acrn-dev`: Public headers and libraries for ACRN manager.
* `acrn-devicemodel`: Device model for ACRN Hypervisor
* `acrn-doc`: Reference to ACRN Documentation
* `acrn-hypervisor`: ACRN Hypervisor for IoT
* `acrn-lifemngr`: ACRN life manager service
* `acrn-system`: metapackage to deploy a minimum of ACRN packages
* `acrn-tools`: Supplementary tools for ACRN Hypervisor
* `acrnd`: ACRN Hypervisor control daemon
* `grub-acrn`: Grub setup scripts for ACRN Hypervisor
* `python3-acrn-board-inspector`: Generate Board Configuration for ACRN

## Building

The `acrn-hypervisor` source package uses `git-buildpackage` (`gbp`) for package building (see `debian/gbp.conf`). For more information on `gbp` refer to the [Git-Buildpackage manual](http://honk.sigxcpu.org/projects/git-buildpackage/manual-html/gbp.html).

To ease the build process a shell script wrapper around a docker controlled build is provided:

    debian/docker/acrn-docker-build.sh

This script features the `DISTRO` and `VENDOR` environment variables to set the Debian or Ubuntu distribution the ACRN packages are built for. `VENDOR` defaults to `debian` and `DISTRO` defaults to `stable`, which then is transformed to its canonical name (`bullseye` for now).

To build ACRN packages on master branch for Debian Buster use:

    VENDOR=debian DISTRO=buster debian/docker/acrn-docker-build.sh

or for Ubuntu 20.04:

   VENDOR=ubuntu DISTRO=focal debian/docker/acrn-docker-build.sh

If building on any other branch, the `debian/gbp.conf` file has to be adapted accordingly. To override this necessity simply add a `--git-ignore-branch`:

    debian/docker/acrn-docker-build.sh --git-ignore-branch

This not only builds the ACRN packages listed above, but eventually all required packages (for build or runtime), too.

Remark: Since the required, dependent package repositories are accessed at build time, your build machine needs a properly configured internet access.

All these build results including the required third party packages are located in the `build/<distroname>` folder and prepared to be used as a local APT repository. To use it as such, add a proper apt configuration (assuming the result folder has been copied to `/apt`):

    echo "deb [trusted=yes] file:/apt ./" > /etc/apt/sources.list.d/acrn-local.list
    echo "deb-src [trusted=yes] file:/apt ./" >> /etc/apt/sources.list.d/acrn-local.list

Now `apt update` will also take into account all the local packages, so you can easily install, e.g. board-inspector via:

    apt install python3-acrn-board-inspector

This will install python3-acrn-board-inspector with all required dependencies.


## Development Build from Source Package

Once the local APT repository is in place, it can also be used to quickly create proprietary built ACRN packages from the acrn-hypervisor soure package, e.g. to add a new scenario for testing or change the list of supported board. Many thanks to Pirouf for bringing in this idea!

    apt-get install devscripts build-essential
    cd <working dir>
    apt-get source acrn-hypervisor
    apt-get build-dep acrn-hypervisor
    cd acrn-hypervisor-<version>

Now you can make your changes, e.g. use a new board and scenario:

    mkdir -p misc/config_tools/data/newboard
    cp <user>/newboard.xml misc/config_tools/data/newboard/newboard.xml
    cp <user>/newscenario.xml misc/config_tools/data/newboard/newscenario.xml
    sed "s/ACRN_BOARDLIST \:\=.*/ACRN_BOARDLIST \:\= newboard/" -i debian/acrn-hypervisor.conf.mk
    sed "s/ACRN_SCENARIOLIST \:\=.*/ACRN_SCENARIOLIST \:\= newscenario/" -i debian/acrn-hypervisor.conf.mk
    dpkg-buildpackage

The resulting packages are then located in `<working dir>`.


## Package Maintenance

During development `debian/changelog` should always present the `UNRELEASED` stage of package build. Before creating new (temporary) packages `debian/changelog` can be easily updated using

    DEBEMAIL="<add your email here> DEBFULLNAME="<enter your full name here>" gbp dch --snapshot --git-ignore-branch

This updates/creates an `UNRELEASED` changelog entry, which has to be committed separately. BTW, I always use the following to align with what is already configured in git:

    DEBEMAIL=$(git config user.email) DEBFULLNAME=$(git config user.name) gbp dch --snapshot --git-ignore-branch

At release create a proper entry using

    DEBEMAIL="<add your email here> DEBFULLNAME="<enter your full name here>" gbp dch --release

This fires up the editor to review the newly created `debian/changelog` entry. Edit, save and commit it to finish the package release from a Debian point of view.


## Special Package Properties

### acrn-hypervisor

This package contains multiple ACRN hypervisor binaries, with the final binary being chosen usually at install time via Debian's `debconf` mechanism. This allows you to choose the board as well as the respective scenario but still use the same Debian package for various hardware platforms.

**WARNING**

Always choose an appropriate board/scenario setting! Wrong settings may refuse to boot!

You can also preseed your choice by setting the respective `debconf` keys `acrn-hypervisor/board` and `acrn-hypervisor/scenario`, e.g. during image creation. Please refer to https://wiki.debian.org/debconf for details.

To reconfigure the choice later, use

    dpkg-reconfigure acrn-hypervisor

The ACRN hypervisor configurations are chosen as follows:
All directories given in `CONFIGDIRS` in `acrn-hypervisor.conf.mk` are searched for valid board- and scenario-configuration files. The `ACRN_BOARDLIST` and `ACRN_SCENARIOLIST` in `acrn-hypervisor.conf.mk` can be used to restrict the hypervisor/scenario configurations built into `acrn-hypervisor` package. If unset, all possible configurations found under the directories given are built.

### acrn-lifemngr

To adapt the needs of a Debian distribution the service file has been adapted and a start script wrapper added to automatically set up the parameters for User VMs or the Service VM.

### acrnd

There is also an adapted variant for the systemd service file. As for `acrn-lifemngr` this also is provided as part of the Debian packaging process rather than patching the files provided with the sources.

## About initramfs on Debian/Ubuntu Systems

If the `ramdisk_mod` node in the scenario configuration is empty (at the moment this is true especially for all `shared` scenarios), an initrd/initramfs image is neither required nor used. Grub config helper then creates a rootfs parameter using the respective device name at install time, like `/dev/sda2`. Since this depends on device enumeration of the kernel, which might change when additional storage devices are added (your `/dev/sda2` might turn into e.g. `/dev/sdb2`, but your grub configuration stays unchanged!) Debian/Ubuntu decided to use UUIDs to identify the storage device partitions. This is implemented by respective scripts provided in initrd/initramfs and **NOT** within the kernel, so apparently initrd/initramfs is required!

To use this feature properly (as the standard distribution setup does) add a `ramdisk_mod` node value (usually `Linux_initrd`) in the scenario configuration and provide a kernel package with initrd/initramfs support. This is state-of-the-art nowadays and also supported by the acrn-kernel service vm configuration. It enables the UUID boot device support and avoids the device enumeration issues completely, see the `shared+initrd` scenarios in `debian/configs` for an example.

## Scenario Configuration - GRUB configuration

The following subnodes of the `SERVICE_VM` node (VM with `load_order=SERVICE_VM`) are considered by grub-acrn when creating the GRUB menu entries for ACRN:

* `kern_mod` must not be empty to add a menu entry
* `ramdisk_mod` enables initrd/initramfs image usage (see above)
* `bootargs`: kernel boot parameters, that are added to the kernel boot command line. Exception: `rootfs` parameter is ignored, since this is automatically determined by GRUB config helpers. This can be used e.g. to add `hvlog` parameter for logging support.

## Known Build Warnings

Depending to which distribution the build is targeted, the following warnings can occur, but can safely be ignored:

* in `override_dh_strip` build stage:

  These warnings only occur in builds for the most recent distributions, but do not harm the binary packages. The usability of dbgsym packages is questionable in this case (untested).
  * `debugedit: .debug_line offset 0xXXXX referenced multiple times`
  * `Unknown DWARF DW_FORM 0xXXXX`

* in `override_dh_auto_build-indep` build stage:
  * `package init file '<...>/__init__.py' not found (or not a regular file)`

  This is a result of the the python components not being structured for packaging with setuptools. The warnings are harmless and do not imply any restrictions to the python packages.

* in `override_dh_auto_install-arch` build stage
  * `/usr/bin/ld: <...>/boot_mod.a(cpu_primary.o): warning: relocation in read-only section 'multiboot_header'`

  This warning is emitted on all but the oldest distros (gcc/binutils version dependent?). **This might be of concern and must be investigated.** Up till now, no issues have been found when using the binaries triggering this warning.

* lintian
  * `elf-error In program headers: Unable to find program interpreter name`

  This is a known issue, see [Debian Bug#1000977](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1000977) and [Debian Bug#1000449](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1000449).


## General Remarks and Restrictions

* ACRN >=2.6 needs a Linux 5.10 kernel with the respective Intel/ACRN patches applied, see [Project ACRN Documentation](https://projectacrn.github.io/latest/index.html) for details.
* The packages are built in debug mode to be able to access the HV console. This can be changed by setting the `RELEASE` variable in `debian/rules` to 1.
* The built configurations are restricted to the hardware platforms available for testing.
* The systemd services provided by various ACRN packages are enabled at install time but not started, since they are most likely installed on a non-ACRN system which requires a reboot anyway. Only the acrn-tools related services (acrnlog, acrnprobe, usercrash) might be installed on a running ACRN system and then either need a reboot or must be started manually (`systemd start <service name>`).
* acrn-configurator is still under heavy development and therefore not yet packaged.

\-- Helmut Buchsbaum \<helmut.buchsbaum@opensource.tttech-industrial.com> Sat, 06 May 2022 20:07:19 +0200
