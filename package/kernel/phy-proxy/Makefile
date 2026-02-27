#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=phy-proxy
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/phy-proxy
  SUBMENU:=Network Devices
  TITLE:=Virtual Network Phy Proxy
  FILES:=$(PKG_BUILD_DIR)/phy-proxy.ko
  AUTOLOAD:=$(call AutoProbe,phy-proxy)
  DEPENDS:=+kmod-bonding +kmod-phylink
endef

define KernelPackage/phy-proxy/description
  Creates virtual network device(s) to work around CPU macs that
  have a translation layer before the phy- eg a switch or some other
  intermediate translator. The mode can be different for the mac and the phy
  in this case. We add capability to bond mac devices where an intermediate switch
  aggregates the (xx)gmii links, before translating to another (xx)gmii interface
  to the phy. The use case to make this was porting OpenWRT to Meraki MR53.
  Configure in device tree.
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR) modules
endef

$(eval $(call KernelPackage,phy-proxy))
