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
BUILDDIR      ?= _build

# document publication assumes the folder structure is setup
# with the acrn-hypervisor and projectacrn.github.io repos as
# sibling folders and make is run inside the acrn-hypervisor/docs
# folder.

ACRN_BASE     = "$(CURDIR)/../.."
DOC_TAG      ?= development
RELEASE      ?= latest
PUBLISHDIR    = $(ACRN_BASE)/projectacrn.github.io/$(RELEASE)

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(OPTS)
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
	$(Q)find doxygen/xml/* | xargs sed -i 's/simplesect kind="pre"/simplesect kind="preconditions"/'

content:
	$(Q)scripts/extract_content.py . misc

kconfig:
	$(Q)srctree=../hypervisor \
	    python3 scripts/genrest.py Kconfig reference/kconfig/

pullsource:
	$(Q)scripts/pullsource.sh

html: doxy content kconfig
	-$(Q)$(SPHINXBUILD) -t $(DOC_TAG) -b html -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(OPTS) >> doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh doc.log

singlehtml: doxy content kconfig
	-$(Q)$(SPHINXBUILD) -t $(DOC_TAG) -b singlehtml -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(OPTS) >> doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh doc.log

# Remove generated content (Sphinx and doxygen)

clean:
	rm -fr $(BUILDDIR) doxygen
	rm -fr misc
	rm -fr reference/kconfig/*.rst

# Copy material over to the GitHub pages staging repo
# along with a README

publish:
	mkdir -p $(PUBLISHDIR)
	cd $(PUBLISHDIR)/..; git pull origin master
	rm -fr $(PUBLISHDIR)/*
	cp -r $(BUILDDIR)/html/* $(PUBLISHDIR)
	cp scripts/publish-README.md $(PUBLISHDIR)/../README.md
	cp scripts/publish-index.html $(PUBLISHDIR)/../index.html
	cp scripts/publish-robots.txt $(PUBLISHDIR)/../robots.txt
	cd $(PUBLISHDIR)/..; git add -A; git commit -s -m "publish $(RELEASE)"; git push origin master;


# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(OPTS) is meant as a shortcut for $(SPHINXOPTS).
%: Makefile doxy
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(OPTS)
