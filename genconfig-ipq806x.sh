#!/bin/sh

arch=ipq806x
wget https://downloads.openwrt.org/snapshots/targets/${arch}/generic/config.buildinfo -O config.buildinfo
cat config.buildinfo | grep -v CONFIG_TARGET_DEVICE_ | grep -v CONFIG_TARGET_ALL_PROFILES | grep -v CONFIG_TARGET_MULTI_PROFILE > .config
echo CONFIG_TARGET_ALL_PROFILES=n >> .config
echo CONFIG_TARGET_MULTI_PROFILE=n >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr42=y >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr52=y >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr42=\"\" >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr52=\"\" >> .config

#add luci
echo CONFIG_PACKAGE_luci=y >> .config
make defconfig

#skip xdp
cat .config | grep -v "CONFIG_PACKAGE.*xdp" > .config.tmp
cp .config.tmp .config


