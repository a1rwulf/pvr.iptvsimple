# Omniyon IPTV
IPTV Live TV and Radio PVR client addon for [Omniyon]

## Build instructions

### Linux

1. `git clone https://git.yachtcloud.eu/yachtcloud/kodi`
2. `git clone https://git.yachtcloud.eu/yachtcloud/pvr.iptvsimple`
3. `cd pvr.iptvsimple && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.iptvsimple -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../cmake/addons`
5. `make`