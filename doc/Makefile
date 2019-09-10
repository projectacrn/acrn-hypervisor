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
BUILDDIR      ?= _build
SOURCEDIR     = $(BUILDDIR)/rst/

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

.PHONY: help Makefile copy-to-sourcedir doxy content kconfig html singlehtml clean publish

# Generate the doxygen xml (for Sphinx) and copy the doxygen html to the
# api folder for publishing along with the Sphinx-generated API docs.

copy-to-sourcedir:
	$(Q)mkdir -p $(SOURCEDIR)
	$(Q)rsync -rt --exclude=$(BUILDDIR) . $(SOURCEDIR)

doxy: copy-to-sourcedir
	$(Q)(cat acrn.doxyfile ; echo "OUTPUT_DIRECTORY=$(SOURCEDIR)/doxygen" ) | doxygen - > $(BUILDDIR)/doc.log 2>&1
	$(Q)find $(SOURCEDIR)/doxygen/xml/* | xargs sed -i 's/simplesect kind="pre"/simplesect kind="preconditions"/'

content: copy-to-sourcedir
	$(Q)scripts/extract_content.py $(SOURCEDIR) misc

kconfig: copy-to-sourcedir
	$(Q)srctree=../hypervisor \
	    python3 scripts/genrest.py Kconfig $(SOURCEDIR)/reference/kconfig/

pullsource:
	$(Q)scripts/pullsource.sh

html: copy-to-sourcedir doxy content kconfig
	-$(Q)$(SPHINXBUILD) -t $(DOC_TAG) -b html -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(OPTS) >> $(BUILDDIR)/doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh $(BUILDDIR)/doc.log

singlehtml: doxy content kconfig
	-$(Q)$(SPHINXBUILD) -t $(DOC_TAG) -b singlehtml -d $(BUILDDIR)/doctrees $(SOURCEDIR) $(BUILDDIR)/html $(SPHINXOPTS) $(OPTS) >> $(BUILDDIR)/doc.log 2>&1
	$(Q)./scripts/filter-doc-log.sh $(BUILDDIR)/doc.log

# Remove generated content (Sphinx and doxygen)

clean:
	rm -fr $(BUILDDIR)
	# Keeping these temporarily, but no longer strictly needed.
	rm -fr doxygen
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
