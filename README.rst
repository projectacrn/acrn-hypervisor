Project ACRN Embedded Hypervisor
################################


The open source project ACRN defines a device hypervisor reference stack
and an architecture for running multiple software subsystems, managed
securely, on a consolidated system by means of a virtual machine
manager. It also defines a reference framework implementation for
virtual device emulation, called the "ACRN Device Model".

The ACRN Hypervisor is a Type 1 reference hypervisor stack, running
directly on the bare-metal hardware, and is suitable for a variety of
IoT and embedded device solutions. The ACRN hypervisor addresses the
gap that currently exists between datacenter hypervisors, and hard
partitioning hypervisors. The ACRN hypervisor architecture partitions
the system into different functional domains, with carefully selected
guest OS sharing optimizations for IoT and embedded devices.

.. start_include_here

Community Support
*****************

The Project ACRN Developer Community includes developers from member
organizations and the general community all joining in the development of
software within the project. Members contribute and discuss ideas,
submit bugs and bug fixes. They also help those in need
through the community's forums such as mailing lists and IRC channels. Anyone
can join the developer community and the community is always willing to help
its members and the User Community to get the most out of Project ACRN.

Welcome to the project ARCN community!

We're now holding weekly Technical Community Meetings and encourage you
to call in and learn more about the project. Meeting information is on
the `TCM Meeting page`_ in our `ACRN wiki <https://wiki.projectacrn.org/>`_.

.. _TCM Meeting page:
   https://github.com/projectacrn/acrn-hypervisor/wiki/ACRN-Committee-and-Working-Group-Meetings#technical-community-meetings

Resources
*********

Here's a quick summary of resources to find your way around the Project
ACRN support systems:

* **Project ACRN Website**: The https://projectacrn.org website is the
  central source of information about the project. On this site, you'll
  find background and current information about the project as well as
  relevant links to project material.  For a quick start, refer to the
  `Introduction`_ and `Getting Started Guide`_.

* **Source Code in GitHub**: Project ACRN source code is maintained on a
  public GitHub repository at https://github.com/projectacrn/acrn-hypervisor.
  You'll find information about getting access to the repository and how to
  contribute to the project in this `Contribution Guide`_ document.

* **Documentation**: Project technical documentation is developed
  along with the project's code, and can be found at
  https://projectacrn.github.io.  Additional documentation is maintained in
  the `Project ACRN GitHub wiki`_.

* **Issue Reporting and Tracking**: Requirements and Issue tracking is done in
  the Github issues system: https://github.com/projectacrn/acrn-hypervisor/issues.
  You can browse through the reported issues and submit issues of your own.

* **Reporting a Potential Security Vulnerability**: If you have discovered potential
  security vulnerability in ACRN, please send an e-mail to secure@intel.com. For issues
  related to Intel Products, please visit https://security-center.intel.com.

  It is important to include the following details:

  - The projects and versions affected
  - Detailed description of the vulnerability
  - Information on known exploits

  Vulnerability information is extremely sensitive. Please encrypt all security vulnerability
  reports using our `PGP key`_.

  A member of the Intel Product Security Team will review your e-mail and contact you to
  to collaborate on resolving the issue. For more information on how Intel works to resolve
  security issues, see: `vulnerability handling guidelines`_.

* **Mailing List**: The `Project ACRN Development mailing list`_ is perhaps the most convenient
  way to track developer discussions and to ask your own support questions to
  the project ACRN community.  There are also specific `ACRN mailing list
  subgroups`_ for builds, users, and Technical
  Steering Committee notes, for example.
  You can read through the message archives to follow
  past posts and discussions, a good thing to do to discover more about the
  project.


.. _Introduction: https://projectacrn.github.io/latest/introduction/
.. _Getting Started Guide: https://projectacrn.github.io/latest/getting_started/
.. _Contribution Guide: https://projectacrn.github.io/latest/contribute.html
.. _Project ACRN GitHub wiki: https://github.com/projectacrn/acrn-hypervisor/wiki
.. _PGP Key: https://www.intel.com/content/www/us/en/security-center/pgp-public-key.html
.. _vulnerability handling guidelines:
   https://www.intel.com/content/www/us/en/security-center/vulnerability-handling-guidelines.html
.. _Project ACRN Development mailing list: https://lists.projectacrn.org/g/acrn-dev
.. _ACRN mailing list subgroups: https://lists.projectacrn.org/g/main/subgroups
