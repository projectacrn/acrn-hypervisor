# Minimal makefile for Sphinx documentation
#

ifeq ($(VERBOSE),1)
  Q =
else
  Q = @
endif

# You can set these variables from the command line.
SPHINXOPTS    ?= -q
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = "Project ACRN"
SOURCEDIR     = .
BUILDDIR      = _build

DOC_TAG      ?= development
RELEASE      ?= latest
PUBLISHDIR    = ../../projectacrn.github.io/$(RELEASE)

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
	@echo ""
	@echo "make publish"
	@echo "   publish generated html to projectacrn.github.io site:"
	@echo "   specify RELEASE=name to publish as a tagged release version"
	@echo "   and placed in a version subfolder.  Requires repo merge permission."

.PHONY: help Makefile

# Generate the doxygen xml (for Sphinx) and copy the doxygen html to the
# api folder for publishing along with the Sphinx-generated API docs.

doxy:
	$(Q)(cat acrn.doxyfile) | doxygen - > doc.log 2>&1

content:
	$(Q)scripts/extract_content.py

html: doxy content
	-$(Q)$(SPHINXBUILD) -t $(DOC_TAG) -b html -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(O) >> doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh doc.log


# Remove generated content (Sphinx and doxygen)

clean:
	rm -fr $(BUILDDIR) doxygen
	rm -fr tools

# Copy material over to the GitHub pages staging repo
# along with a README

publish:
	rm -fr $(PUBLISHDIR)/*
	cp -r $(BUILDDIR)/html/* $(PUBLISHDIR)
	cp scripts/publish-README.md $(PUBLISHDIR)/../README.md
	cp scripts/publish-index.html $(PUBLISHDIR)/../index.html
	cd $(PUBLISHDIR)/..; git add -A; git commit -s -m "publish $(RELEASE)"; git push origin master;


# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile doxy
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
