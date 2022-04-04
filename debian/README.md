## ACRN Debianization

ACRN is a flexible, lightweight reference hypervisor, built with real-time and safety-criticality in mind, optimized to streamline embedded development through an open source platform. This part represents the debianization for ACRN.

The Debian source package `acrn-hypervisor` provides the following Debian packages:

*   `acrn-dev`: Public headers and libraries for ACRN manager.
*   `acrn-devicemodel`: Device model for ACRN Hypervisor
*   `acrn-doc`: Reference to ACRN Documentation
*   `acrn-hypervisor`: ACRN Hypervisor for IoT
*   `acrn-lifemngr`: ACRN life manager service
*   `acrn-system`: metapackage to deploy a minimum of ACRN packages
*   `acrn-tools`: Supplementary tools for ACRN Hypervisor
*   `acrnd`: ACRN Hypervisor control daemon
*   `grub-acrn`: Grub setup scripts for ACRN Hypervisor
*   `python3-acrn-board-inspector`: Generate Board Configuration for ACRN

## Building

The `acrn-hypervisor` source package uses `git-buildpackage` (`gbp`) for package building (see `debian/gbp.conf`). For more information on `gbp` refer to the [Git-Buildpackage manual](http://honk.sigxcpu.org/projects/git-buildpackage/manual-html/gbp.html).

To ease the build process a shell script wrapper around a docker controlled build is provided:

```plaintext
debian/docker/acrn-docker-build.sh
```

This script features the `DISTRO` and `VENDOR` environment variables to set the Debian or Ubuntu distribution the ACRN packages are built for. `VENDOR` defaults to `debian` and `DISTRO` defaults to `stable`, which then is transformed to its canonical name (`bullseye` for now)

To build ACRN packages on master branch for Debian Buster use:

```plaintext
VENDOR=debian DISTRO=buster debian/docker/acrn-docker-build.sh
```

or for Ubuntu 20.04:

```plaintext
VENDOR=ubuntu DISTRO=focal debian/docker/acrn-docker-build.sh
```

If building on any other branch, the `debian/gbp.conf` file has to be adapted accordingly. To override this necessity simply add a `--git-ignore-branch`:

```plaintext
debian/docker/acrn-docker-build.sh --git-ignore-branch
```

This not only builds the ACRN packages listed above, but eventually all required packages (for build or runtime), too.

Remark: Since the required, dependent package repositories are accessed at build time, your build machine needs a properly configured internet access.

All these build results including the required third party packages are located in the `build/<distroname>` folder and prepared to be used as a local APT repository. To use it as such, add a proper apt configuration (assuming the result folder has been copied to `/apt`):

```plaintext
echo "deb [trusted=yes] file:/apt ./" > /etc/apt/sources.list.d/local.list
```

Now `apt update` will also take into account all the local packages, so you can easily install, e.g. board-inspector via:

```plaintext
apt install python3-acrn-board-inspector
```

This will install python3-acrn-board-inspector with all required dependencies.

## Package Maintenance

During development `debian/changelog` should always present the `UNRELEASED` stage of package build. Before creating new (temporary) packages `debian/changelog` can be easily updated using

```plaintext
DEBEMAIL="<add your email here> DEBFULLNAME="<enter your full name here>" gbp dch --snapshot --git-ignore-branch
```

This updates/creates an `UNRELEASED` changelog entry, which has to be committed separately. BTW, I always use the following to align with what is already configured in git:

```
DEBEMAIL=$(git config user.email) DEBFULLNAME=$(git config user.name) gbp dch --snapshot --git-ignore-branch
```

At release create a proper entry using

```plaintext
DEBEMAIL="<add your email here> DEBFULLNAME="<enter your full name here>" gbp dch --release
```

This fires up the editor to review the newly created `debian/changelog` entry. Edit, save and commit it to finish the package release from a Debian point of view.


## Special Package Properties

### acrn-hypervisor

This package contains multiple ACRN hypervisor binaries, with the final binary being chosen usually at install time via Debian's `debconf` mechanism. This allows you to choose the board as well as the respective scenario but still use the same Debian package for various hardware platforms.

**WARNING**

Always choose an appropriate board/scenario setting! Wrong settings may refuse to boot!

You can also preseed your choice by setting the respective `debconf` keys `acrn-hypervisor/board` and `acrn-hypervisor/scenario`, e.g. during image creation. Please refer to https://wiki.debian.org/debconf for details.

To reconfigure the choice later, use

```plaintext
dpkg-reconfigure acrn-hypervisor
```

The ACRN hypervisor configurations are chosen as follows:
All directories given in `CONFIGDIRS` in `debian/rules` are searched for valid board- and scenario-configuration files. The `ACRN_BOARDLIST` and `ACRN_SCENARIOLIST` in `debian/rules` can be used to restrict the hypervisor/scenario configurations built into `acrn-hypervisor`. If unset, all possible configurations found under the directories given are built.

### acrn-lifemngr

To adapt the needs of a Debian distribution the service file has been adapted and a start script wrapper added to automatically set up the parameters for User VMs or the Service VM.

### acrnd

There is also an adapted variant for the systemd service file. As for `acrn-lifemngr` this also is provided as part of the Debian packaging process rather than patching the files provided with the sources.

## General Remarks and Restrictions

*   ACRN >=2.6 needs a Linux 5.10 kernel with the respective Intel/ACRN patches applied, see [Project ACRN Documentation](https://projectacrn.github.io/latest/index.html) for details.
*   The packages are built in debug mode to be able to access the HV console. This can be changed by setting the `RELEASE` variable in `debian/rules` to 1.
*   The built configurations are restricted to the hardware platforms available for testing.

\-- Helmut Buchsbaum \<helmut.buchsbaum@opensource.tttech-industrial.com> Sat, 27 Apr 2022 19:41:17 +0200
