#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wifi-mon
PKG_RELEASE:=1

PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk

define Package/wifi-mon
  SECTION:=net
  CATEGORY:=Network
  TITLE:=Wifi DFS retry watchdog
  URL:=http://openwrt.org/
endef

define Package/wifi-mon/description
  Watchdog service that monitors system log for wifi radar DFS scan failure. Without this if the interface detects
  a radar signal, or some noise that resembles it, it stays disabled. With this service it immediately restarts the radio repeatedly,
  assuming the DFS detection was a false positive. If its really active radar it still wont start the radio until the radar signal is gone.
endef

define Build/Compile
endef

define Package/wifi-mon/install
	$(CP) ./files/* $(1)/
endef

$(eval $(call BuildPackage,wifi-mon))
