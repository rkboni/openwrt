#!/bin/sh

arch=ipq806x
target=generic
version=snapshots
wget https://downloads.openwrt.org/${version}/targets/${arch}/${target}/config.buildinfo -O config.buildinfo
cat config.buildinfo | grep -v CONFIG_TARGET_DEVICE_ | grep -v CONFIG_TARGET_ALL_PROFILES | grep -v CONFIG_TARGET_MULTI_PROFILE > .config
echo CONFIG_TARGET_ALL_PROFILES=n >> .config
echo CONFIG_TARGET_MULTI_PROFILE=n >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr42=y >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr52=y >> .config
echo CONFIG_TARGET_DEVICE_${arch}_generic_DEVICE_meraki_mr53=y >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr42=\"\" >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr52=\"\" >> .config
echo CONFIG_TARGET_DEVICE_PACKAGES_${arch}_generic_DEVICE_meraki_mr53=\"\" >> .config

#add luci, tcpdump, iperf3 & some kmods
echo CONFIG_PACKAGE_luci=y >> .config
echo CONFIG_PACKAGE_iperf3=y >> .config
echo CONFIG_PACKAGE_tcpdump=y >> .config
echo CONFIG_PACKAGE_mdio-tools=y >> .config
echo CONFIG_PACKAGE_kmod-of-mdio=y >> .config
echo CONFIG_PACKAGE_kmod-eeprom-at24=y >> .config
echo CONFIG_PACKAGE_kmod-gpio-button-hotplug=y >> .config
echo CONFIG_PACKAGE_kmod-hwmon-ina2xx=y >> .config
echo CONFIG_PACKAGE_kmod-leds-gpio=y >> .config
echo CONFIG_PACKAGE_kmod-ledtrigger-netdev=y >> .config
echo CONFIG_PACKAGE_kmod-nft-offload=y >> .config
echo CONFIG_PACKAGE_kmod-phy-aquantia=y >> .config
echo CONFIG_PACKAGE_kmod-phy-at803x=y >> .config
echo CONFIG_PACKAGE_kmod-phy-proxy=y >> .config
echo CONFIG_PACKAGE_kmod-phylib-qcom=y >> .config
echo CONFIG_PACKAGE_kmod-phylink=y >> .config
echo CONFIG_PACKAGE_kmod-qca85xx-sw=y >> .config
make defconfig
