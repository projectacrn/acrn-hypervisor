.. _contribute:

Contribution Guidelines
#######################

As an open-source project, we welcome and encourage the community to
submit patches directly to Project ACRN.  In our collaborative open
source environment, standards and methods for submitting changes help
reduce the chaos that can result from an active development community.

This document explains how to participate in project conversations, log
bugs and enhancement requests, and submit patches to the project so your
patch will be accepted quickly in the codebase.

Licensing
*********

Licensing is very important to open source projects. It helps ensure the
software continues to be available under the terms that the author
desired.

Project ACRN uses a BSD license, as found in the license_header in
the project's GitHub repo.

A license tells you what rights you have as a developer, as provided by
the copyright holder. It is important that the contributor fully
understands the licensing rights and agrees to them. Sometimes the
copyright holder isn't the contributor, such as when the contributor is
doing work on behalf of a company.

.. _DCO:

Developer Certification of Origin (DCO)
***************************************

To make a good faith effort to ensure licensing criteria are met,
Project ACRN requires the Developer Certificate of Origin (DCO) process
to be followed.

The DCO is an attestation attached to every contribution made by every
developer. In the commit message of the contribution, (described more
fully later in this document), the developer simply adds a
``Signed-off-by`` statement and thereby agrees to the DCO.

When a developer submits a patch, it is a commitment that the
contributor has the right to submit the patch per the license.  The DCO
agreement is shown below and at http://developercertificate.org/.

.. code-block:: none

    Developer's Certificate of Origin 1.1

    By making a contribution to this project, I certify that:

    (a) The contribution was created in whole or in part by me and I
        have the right to submit it under the open source license
        indicated in the file; or

    (b) The contribution is based upon previous work that, to the
        best of my knowledge, is covered under an appropriate open
        source license and I have the right under that license to
        submit that work with modifications, whether created in whole
        or in part by me, under the same open source license (unless
        I am permitted to submit under a different license), as
        Indicated in the file; or

    (c) The contribution was provided directly to me by some other
        person who certified (a), (b) or (c) and I have not modified
        it.

    (d) I understand and agree that this project and the contribution
        are public and that a record of the contribution (including
        all personal information I submit with it, including my
        sign-off) is maintained indefinitely and may be redistributed
        consistent with this project or the open source license(s)
        involved.

DCO Sign-Off Methods
====================

The DCO requires a sign-off message in the following format appear on each
commit in the pull request::

   Signed-off-by: acrnus acrn <acrnus@projectacrn.org>

The DCO text can either be manually added to your commit body, or you can add
either ``-s`` or ``--signoff`` to your usual Git commit commands. If you forget
to add the sign-off you can also amend a previous commit with the sign-off by
running ``git commit --amend -s``. If you've pushed your changes to GitHub
already you'll need to force push your branch after this with ``git push -f``.

Prerequisites
*************

.. _Project ACRN website: https://projectacrn.org

As a contributor, you'll want to be familiar with Project ACRN, how to
configure, install, and use it as explained in the `Project ACRN website`_
and how to set up your development environment as introduced in the
Project ACRN `Getting Started Guide`_.

.. _Getting Started Guide:
   https://acrnproject.github.com/acrn-documentation/getting_started.html

You should be familiar with common developer tools such as Git and CMake, and
platforms such as GitHub.

If you haven't already done so, you'll need to create a (free) GitHub account
on http://github.com and have Git tools available on your development system.

Repository layout
*****************

To clone the main Project ACRN repository use::

    git clone https://github.com/projectacrn/acrn-hypervisor

The Project ACRN directory structure is described in `Source Tree Structure`_
documentation. In addition to the Project ACRN hypervisor itself, you'll also find the
sources for technical documentation, sample code and supported board
configurations. All of these are available for developers to contribute to and enhance.

.. _Source Tree Structure:
   https://projectacrn.github.io/acrn-documentation/

Pull Requests and Issues
************************

.. _ACRN-dev mailing list:
   https://lists.projectacrn.org/g/acrn-dev

Before starting on a patch, first check in our issues in the `ACRN-dev
mailing list`_ system to see what's been reported on the issue you'd
like to address.  Have a conversation on the `ACRN-dev mailing list`_ to
see what others think of your issue (and proposed solution).  You may
find others that have encountered the issue you're finding, or that have
similar ideas for changes or additions.  Send a message to the `ACRN-dev
mailing list`_ to introduce and discuss your idea with the development
community.

It's always a good practice to search for existing or related issues
before submitting your own. When you submit an issue (bug or feature
request), the triage team will review and comment on the submission,
typically within a few business days.

 .. _Contribution Tools:

Contribution Tools and Git Setup
********************************

Signed-off-by
=============

The name in the commit message ``Signed-off-by:`` line and your email must
match the change authorship information. Make sure your :file:`.gitconfig`
is set up correctly:

.. code-block:: console

   git config --global user.name "David Developer"
   git config --global user.email "david.developer@company.com"


Coding Style
************

Use these coding guidelines to ensure that your development complies with the
project's style and naming conventions.

.. _Linux kernel coding style:
   https://kernel.org/doc/html/latest/process/coding-style.html

In general, follow the `Linux kernel coding style`_, with the
following exceptions:

* Add braces to every ``if`` and ``else`` body, even for single-line code
  blocks. Use the ``--ignore BRACES`` flag to make *checkpatch* stop
  complaining.
* Use spaces instead of tabs to align comments after declarations, as needed.
* Use C89-style single line comments, ``/*  */``. The C99-style single line
  comment, ``//``, is not allowed.
* Use ``/**  */`` for doxygen comments that need to appear in the documentation.


.. _Contribution workflow:

Contribution Workflow
*********************

One general practice we encourage, is to make small,
controlled changes. This practice simplifies review, makes merging and
rebasing easier, and keeps the change history clear and clean.

When contributing to Project ACRN, it is also important you provide as much
information as you can about your change, update appropriate documentation,
and test your changes thoroughly before submitting.

The general GitHub workflow used by Project ACRN developers uses a combination of
command line Git commands and browser interaction with GitHub.  As it is with
Git, there are multiple ways of getting a task done.  We'll describe a typical
workflow here:

.. _Create a Fork of acrn-hypervisor:
   https://github.com/projecacrn/acrn-hypervisor#fork-destination-box

#. `Create a Fork of acrn-hypervisor`_
   to your personal account on GitHub. (Click on the fork button in the top
   right corner of the project acrn-hypervisor repo page in GitHub.)

#. On your development computer, clone the fork you just made::

     git clone https://github.com/<your github id>/acrn-hypervisor

   This would be a good time to let Git know about the upstream repo too::

     git remote add upstream https://github.com/projectacrn/acrn-hypervisor.git

   and verify the remote repos::

     git remote -v

#. Create a topic branch (off of master) for your work (if you're addressing
   an issue, we suggest including the issue number in the branch name)::

     git checkout master
     git checkout -b fix_comment_typo

#. Make changes, test locally, change, test, test again, ...

#. When things look good, start the pull request process by adding your changed
   files::

     git add [file(s) that changed, add -p if you want to be more specific]

   You can see files that are not yet staged using::

     git status

#. Verify changes to be committed look as you expected::

     git diff --cached

#. Commit your changes to your local repo::

     git commit -s

   The ``-s`` option automatically adds your ``Signed-off-by:`` to your commit
   message.  Your commit will be rejected without this line that indicates your
   agreement with the `DCO`_.  See the `Commit Guidelines`_ section for
   specific guidelines for writing your commit messages.

#. Prepare patches for e-mail submission and send to the projectacrn
   mailing list for review

     git format-patch

#. Once the patch is past review, the maintainer will commit the patch


Commit Guidelines
*****************

Changes are submitted as Git commits. Each commit message must contain:

* A short and descriptive subject line that is less than 72 characters,
  followed by a blank line. The subject line must include a prefix that
  identifies the subsystem being changed, followed by a colon, and a short
  title, for example:  ``doc: update wiki references to new site``.
  (If you're updating an existing file, you can use
  ``git log <filename>`` to see what developers used as the prefix for
  previous patches of this file.)

* A change description with your logic or reasoning for the changes, followed
  by a blank line.

* A Signed-off-by line, ``Signed-off-by: <name> <email>`` typically added
  automatically by using ``git commit -s``

* If the change addresses an issue, include a line of the form::

      Fixes #<brief description about the reported issue>.


All changes and topics sent to GitHub must be well-formed, as described above.

Commit Message Body
===================

When editing the commit message, please briefly explain what your change
does and why it's needed. A change summary of ``"Fixes stuff"`` will be rejected.

.. warning::
   An empty change summary body is not permitted. Even for trivial changes, please
   include a summary body in the commmit message.

The description body of the commit message must include:

* **what** the change does,
* **why** you chose that approach,
* **what** assumptions were made, and
* **how** you know it works -- for example, which tests you ran.

For examples of accepted commit messages, you can refer to the acrn-hypervisor GitHub
`changelog <https://github.com/projectacrn/acrn-hypervisor/commits/master>`__.

Other Commit Expectations
=========================

* Commits must build cleanly when applied on top of each other, thus avoiding
  breaking bisectability.

* Each commit must address a single identifiable issue and must be
  logically self-contained. Unrelated changes should be submitted as
  separate commits.

* You may submit pull request RFCs (requests for comments) to send work
  proposals, progress snapshots of your work, or to get early feedback on
  features or changes that will affect multiple areas in the code base.

Identifying Contribution Origin
===============================

When adding a new file to the tree, it is important to detail the source of
origin on the file, provide attributions, and detail the intended usage. In
cases where the file is an original to acrn-hypervisor, the commit message should
include the following ("Original" is the assumption if no Origin tag is
present)::

    Origin: Original

In cases where the file is imported from an external project, the commit
message shall contain details regarding the original project, the location of
the project, the SHA-id of the origin commit for the file, the intended
purpose, and if the file will be maintained by the acrn-hypervisor project,
(whether or not Project ACRN will contain a localized branch or if
it is a downstream copy).

For example, a copy of a locally maintained import::

    Origin: Contiki OS
    License: BSD 3-Clause
    URL: http://www.contiki-os.org/
    commit: 853207acfdc6549b10eb3e44504b1a75ae1ad63a
    Purpose: Introduction of networking stack.
    Maintained-by: acrn-hypervisor

For example, a copy of an externally maintained import::

    Origin: Tiny Crypt
    License: BSD 3-Clause
    URL: https://github.com/01org/tinycrypt
    commit: 08ded7f21529c39e5133688ffb93a9d0c94e5c6e
    Purpose: Introduction of TinyCrypt
    Maintained-by: External
