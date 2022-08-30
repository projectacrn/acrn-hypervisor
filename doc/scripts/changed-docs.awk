# parse the git diff --stat output and created a reST list of
# (significantly) changed files
#
# doc/develop.rst                                    |   2 +
# doc/developer-guides/contribute_guidelines.rst     | 116 +++-
# doc/developer-guides/hld/hld-devicemodel.rst       |   8 +-
# doc/developer-guides/hld/hld-hypervisor.rst        |   1 +
# doc/developer-guides/hld/hv-rdt.rst                | 126 ++--
# doc/developer-guides/hld/ivshmem-hld.rst           |  70 ++
# doc/developer-guides/hld/mmio-dev-passthrough.rst  |  40 ++
# doc/developer-guides/hld/virtio-net.rst            |  42 +-
# doc/developer-guides/hld/vuart-virt-hld.rst        |   2 +-
# doc/getting-started/building-from-source.rst       |  39 +-


function getLabel(filename)
{
   label="Label not found in " filename
   while ((getline line < filename) > 0) {
      # looking for first occurance of  .. _label name here:
      if (match(line, /^\.\. _([^:]+):/, a) !=0) {
         label=a[1]
         break
      }
   }
   close(filename)
   return label
}

BEGIN {
    if (changes < 1) {changes=10}
    print "Showing docs in master branch with " changes " or more changes."
}

# print label for files with more than specified changed lines
$3 >= changes {
   lable=getLabel($1)
   if (label !~ /^Label not/ ) { print "* :ref:`" label "`" }
   else { print "* " substr($1,5) " was deleted." }
}
