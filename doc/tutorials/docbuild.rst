.. _acrn_doc:

ACRN documentation generation
#############################

These instructions will walk you through generating the Project ACRN's
documentation and publishing it to https://projectacrn.github.io.
You can also use these instructions to generate the ARCN documentation
on your local system.

Documentation overview
**********************

Project ACRN content is written using the reStructuredText markup
language (.rst file extension) with Sphinx extensions, and processed
using Sphinx to create a formatted stand-alone website. Developers can
view this content either in its raw form as .rst markup files, or you
can generate the HTML content and view it with a web browser directly on
your workstation.

You can read details about `reStructuredText`_, and `Sphinx`_ from
their respective websites.

The project's documentation contains the following items:

* ReStructuredText source files used to generate documentation found at the
  http://projectacrn.github.io website. All of the reStructuredText sources
  are found in this acrn-documentation repo.

* Doxygen-generated material used to create all API-specific documents
  found at http://projectacrn.github.io/api/.  Source files from the
  acrn-hypervisor/hypervisor and acrn-hypervisor/devicemodel repos are also used to access the
  header files for the the public APIs, but more about that later.

The reStructuredText files are processed by the Sphinx documentation system,
and make use of the breathe extension for including the doxygen-generated API
material.


Set up the documentation working folders
****************************************

You'll need git installed to get the working folders set up:

* For an Ubuntu development system use:

  .. code-block:: bash

     sudo apt-get install git

* For a Fedora development system use

  .. code-block:: bash

     sudo dnf install git

We use the source header files to generate API docs and we use github.io
for publishing the generated documentation.
Here's the recommended folder setup for documentation contributions and
generation:

.. code-block: none

   projectacrn/
      acrn-hypervisor/
         devicemodel/
         doc/
         hypervisor/

The parent projectacrn folder is there because we'll also be creating a
publishing area later in these steps.  It's best if the acrn-hypervisor
folder is an ssh clone of your personal fork of the upstream project
repos (though https clones work too):

#. Use your browser to visit https://github.com/projectacrn and do a
   fork of the **acrn-hypervisor** repo to your personal GitHub account.)

   .. image:: images/acrn-doc-fork.png
      :align: center

#. At a command prompt, create the working folder and clone the acrn-hypervisor
   repository to your local computer (and if you have publishing rights, the
   projectacrn.github.io repo).  If you don't have publishing rights
   you'll still be able to generate the docs locally, but not publish them:

   .. code-block:: bash

      cd ~
      mkdir projectacrn && cd projectacrn
      git clone git@github.com:<github-username>/projectacrn/acrn-hypervisor.git

#. For the cloned local repos, tell git about the upstream repo:

   .. code-block:: bash

      cd acrn-hypervisor
      git remote add upstream git@github.com:projectacrn/acrn-hypervisor.git

#. If you haven't do so already, be sure to configure git with your name
   and email address for the signed-off-by line in your commit messages:

   .. code-block:: bash

      git config --global user.name "David Developer"
      git config --global user.email "david.developer@company.com"

Installing the documentation tools
**********************************

Our documentation processing has been tested to run with:

* Python 3.6.3
* Doxygen version 1.8.13
* Sphinx version 1.7.5
* Breathe version 4.9.1
* docutils version 0.14
* sphinx_rtd_theme version 0.4.0

Depending on your Linux version, install the needed tools:

* For Ubuntu use:

  .. code-block:: bash

     sudo apt-get install doxygen python3-pip \
       python3-wheel make graphviz

* For Fedora use:

  .. code-block:: bash

     sudo dnf install doxygen python3-pip python3-wheel make graphviz

And for either Linux environment, install the remaining python-based
tools:

.. code-block:: bash

   cd ~/projectacrn/acrn-hypervisor/doc
   pip3 install --user -r scripts/requirements.txt

And with that you're ready to generate the documentation.

Documentation presentation theme
********************************

Sphinx supports easy customization of the generated documentation
appearance through the use of themes.  Replace the theme files and do
another ``make html`` and the output layout and style is changed.
The ``read-the-docs`` theme is installed as part of the
``requirements.txt`` list above.

Running the documentation processors
************************************

The acrn-hypervisor/doc directory has all the .rst source files, extra tools, and Makefile for
generating a local copy of the ACRN technical documentation.

.. code-block:: bash

   cd ~/projectacrn/acrn-hypervisor/doc
   make html

Depending on your development system, it will take about 15 seconds to
collect and generate the HTML content.  When done, you can view the HTML
output with your browser started at ``~/projectacrn/acrn-hypervisor/doc/_build/html/index.html``

Publishing content
******************

If you have merge rights to the projectacrn repo called
projectacrn.github.io, you can update the public project documentation
found at https://projectacrn.github.io.

You'll need to do a one-time clone of the upstream repo (we publish
directly to the upstream repo rather than to a personal forked copy):

.. code-block:: bash

   cd ~/projectacrn
   git clone git@github.com:projectacrn/projectacrn.github.io.git

Then, after you've verified the generated HTML from ``make html`` looks
good, you can push directly to the publishing site with:

.. code-block:: bash

   make publish

This will delete everything in the publishing repo's **latest** folder
(in case the new version has
deleted files) and push a copy of the newly-generated HTML content
directly to the GitHub pages publishing repo.  The public site at
https://projectacrn.github.io will be updated (nearly) immediately
so it's best to verify the locally generated html before publishing.

Filtering expected warnings
***************************

Alas, there are some known issues with the doxygen/Sphinx/Breathe
processing that generates warnings for some constructs, in particular
around unnamed structures in nested unions or structs.
While these issues are being considered for fixing in
Sphinx/Breathe, we've added a post-processing filter on the output of
the documentation build process to check for "expected" messages from the
generation process output.

The output from the Sphinx build is processed by the python script
``scripts/filter-known-issues.py`` together with a set of filter
configuration files in the ``.known-issues/doc`` folder.  (This
filtering is done as part of the ``Makefile``.)

If you're contributing components included in the ACRN API
documentation and run across these warnings, you can include filtering
them out as "expected" warnings by adding a conf file to the
``.known-issues/doc`` folder, following the example of other conf files
found there.

.. _reStructuredText: http://sphinx-doc.org/rest.html
.. _Sphinx: http://sphinx-doc.org/
