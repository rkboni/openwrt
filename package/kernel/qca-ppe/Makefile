include $(TOPDIR)/rules.mk

PKG_NAME:=qca-ppe
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/openwrt/qca-ppe.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2026-05-13
PKG_SOURCE_VERSION:=78551a8aef463848f4cf601adfd96820f2f6d66b
PKG_MIRROR_HASH:=a9800e83540eb079e2d78299cbb1c28b45e9519ad27cac8ca2f196e2866193b3

PKG_BUILD_PARALLEL:=1
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-ppe
  SECTION:=kernel
  CATEGORY:=Kernel modules
  SUBMENU:=Network Devices
  DEPENDS:=@TARGET_qualcommax +kmod-qca-uniphy +kmod-phylink
  KCONFIG:=CONFIG_FWNODE_PCS=y
  TITLE:=Qualcomm PPE switch driver
  FILES:=$(PKG_BUILD_DIR)/qca_ppe.ko
  AUTOLOAD:=$(call AutoLoad,32,qca_ppe)
endef

define KernelPackage/qca-ppe/Description
  Driver for Qualcomm PPE (Packet Processing Engine) switches.
endef

define Build/Compile
	+$(KERNEL_MAKE) $(PKG_JOBS) \
		-C "$(LINUX_DIR)" \
		M="$(PKG_BUILD_DIR)" \
		modules
endef

$(eval $(call KernelPackage,qca-ppe))
