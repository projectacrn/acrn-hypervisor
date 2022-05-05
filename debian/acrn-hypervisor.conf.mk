# set these variables to define build of certain boards/scenarios, e.g.
ACRN_BOARDLIST := whl-ipc-i5 nuc11tnbi5 cfl-k700-i7 tgl-vecow-spc-7100-Corei7 nuc7i7dnh kontron-COMe-mAL10 simatic-ipc227g
ACRN_SCENARIOLIST := partitioned shared hybrid hybrid_rt shared+initrd
# alternatively, unset ACRN_BOARDLIST to build for all boards,
# ACRN_SCENARIOLIST must be set explicitly: scenario configs must be located
# in the same directory as the board config, since there are no board and
# scenario attributes any more in the scenario configs since
# commit c25de24a92c26faa59e7d5e23966dd54215b66e4
#
# undefine ACRN_BOARDLIST

# add builtin and eventually explicitely provided config directories
# misc/config_tools/data: contains ACRN supported configuration
# debian/configs: add additional configurations here!
CONFIGDIRS = misc/config_tools/data debian/configs


# for now build the debug versions
# set to 1 for RELEASE build
export RELEASE ?= 0
