#
# Copyright (C) 2008-2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca85xx-sw
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca85xx-sw
  SUBMENU:=Network Devices
  TITLE:=Driver for QCA85xx Switch Device
  FILES:=$(PKG_BUILD_DIR)/qca-85xx-sw.ko
  AUTOLOAD:=$(call AutoProbe,qca-85xx-sw)
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR) modules
endef

$(eval $(call KernelPackage,qca85xx-sw))
