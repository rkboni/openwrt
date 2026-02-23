#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=switch-phy
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/switch-phy
  SUBMENU:=Network Devices
  TITLE:=Virtual Switch PHY Network Device
  FILES:=$(PKG_BUILD_DIR)/switch-phy.ko
  AUTOLOAD:=$(call AutoProbe,switch-phy)
  DEPENDS:=+kmod-bonding +kmod-phylink
endef

define KernelPackage/switch-phy/description
  Creates virtual network devices that show real PHY status
  for switch-connected PHYs with automatic bonding support
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR) modules
endef

$(eval $(call KernelPackage,switch-phy))
