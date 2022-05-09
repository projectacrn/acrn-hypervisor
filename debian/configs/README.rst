**Purpose of this folder**

This folder serves as a hook to add proprietary ACRN configuration for package building. To avoid polluting ``misc/config_tools/data`` with otherwise unsupported configurations, any additional board and scenario configurations can be put in here (``debian/configs``) using the same structure as in ``misc/config_tools/data``:

::

  debian/configs + <board name> + <board name>.xml
                                |
                                + <scenario 1>.xml
                                ..
                                + <scenario n>.xml


This enables an easy merge form acrn-hypervisor repository without any merge conflict. To get any changes built into the package you have to commit your changes, since ``git-buildpackage`` always requires this to be able to reproduce a package without impact of local changes.

To add proprietary configuration, you create a proper working branch:

::

  me@devhost:~/acrn-work/acrn-hypervisor$ git checkout -b my-proprietary-acrn-package

Add your proprietary configuration, e.g., adding a configuration for a NUC7i7DNHE system with a new-shared scenario:

::

  me@devhost:~/acrn-work/acrn-hypervisor$ mkdir -p debian/configs/nuc7i7dnhe
  me@devhost:~/acrn-work/acrn-hypervisor$ cp <your-config-path>/nuc7i7dnhe.xml debian/configs/nuc7i7dnhe/
  me@devhost:~/acrn-work/acrn-hypervisor$ cp <your-config-path>/new-shared.xml debian/configs/nuc7i7dnhe/

Create a ``debian/configs/configurations.mk`` with the following contents:

::

  me@devhost:~/acrn-work/acrn-hypervisor$ cat debian/configs/configurations.mk
  ACRN_BOARDLIST += nuc7i7dnhe
  ACRN_SCENARIOLIST += new-shared

This adds nuc7i7dnhe:new-shared to the configuration built-into ``acrn-hypervisor`` package. Commit yor changes

::

  me@devhost:~/acrn-work/acrn-hypervisor$ git add debian/configs/nuc7i7dnhe debian/configs/configurations.mk
  me@devhost:~/acrn-work/acrn-hypervisor$ git commit -s -m "Add nuc7i7dnhe:new-shared"

Now you can use this branch ``my-proprietary-acrn-package`` to build the ACRN packages including your new config:

::

  me@devhost:~/acrn-work/acrn-hypervisor$  DISTRO=<your distro> VENDOR=<your vendor> debian/docker/acrn-docker-build.sh --git-ignore-branch

This can then easily be rebased onto any ACRN changes whenever required without any merge-issues.

*Remark*: For a quick solution without the need to commit your changes you can circumvent ``git-buildpackage`` using the procedure outlined `here <../README.rst#development-build-from-source-package>`__.

