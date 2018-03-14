# Minimal makefile for Sphinx documentation
#

ifeq ($(VERBOSE),1)
  Q =
else
  Q = @
endif

# You can set these variables from the command line.
SPHINXOPTS    = -q
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

html: doxy
	$(Q)$(SPHINXBUILD) -b html -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(O) > doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh doc.log


# Remove generated content (Sphinx and doxygen)

clean:
	rm -fr $(BUILDDIR) doxygen

# Copy material over to the GitHub pages staging repo
# along with a README

publish:
	rm -fr $(PUBLISHDIR)/*
	cp -r $(BUILDDIR)/html/* $(PUBLISHDIR)
	cp scripts/publish-README.md $(PUBLISHDIR)/README.md
	cd $(PUBLISHDIR); git add -A; git commit -s -m "publish"; git push origin master;


# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile doxy
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
