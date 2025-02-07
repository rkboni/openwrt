#!/bin/sh

arch=ipq40xx
target=generic
version=releases/24.10-SNAPSHOT
wget https://downloads.openwrt.org/${version}/targets/${arch}/${target}/config.buildinfo -O config.buildinfo
cat config.buildinfo | grep -v CONFIG_TARGET_DEVICE_ | grep -v CONFIG_TARGET_ALL_PROFILES | grep -v CONFIG_TARGET_MULTI_PROFILE > .config
echo CONFIG_TARGET_ALL_PROFILES=n >> .config
echo CONFIG_TARGET_MULTI_PROFILE=n >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr33=y >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr74=y >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr33="" >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr74="" >> .config

#add luci
echo CONFIG_PACKAGE_luci=y >> .config
make defconfig
