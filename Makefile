# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = "Project ACRN"
SOURCEDIR     = .
BUILDDIR      = _build

PUBLISHDIR    = ../projectacrn.github.io

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

pullsource:
	$(Q)scripts/pullsource.sh


# Generate the doxygen xml (for Sphinx) and copy the doxygen html to the
# api folder for publishing along with the Sphinx-generated API docs.

doxy: pullsource
	$(Q)(cat acrn.doxyfile) | doxygen -  2>&1
	$(Q)mkdir -p _build/html/api/doxygen
	$(Q)cp -r doxygen/html/* _build/html/api/doxygen

# Remove generated content (Sphinx and doxygen)

clean:
	$(Q)rm -fr $(BUILDDIR) doxygen hypervisor devicemodel

# Copy material over to the GitHub pages staging repo

publish:
	$(Q)mv $(PUBLISHDIR)/README.md $(PUBLISHDIR)/.README.md
	$(Q)rm -fr $(PUBLISHDIR)/*
	$(Q)mv $(PUBLISHDIR)/.README.md $(PUBLISHDIR)/README.md
	$(Q)cp -r _build/html/* $(PUBLISHDIR)


# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile doxy
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
