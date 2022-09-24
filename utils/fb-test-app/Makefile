include $(TOPDIR)/rules.mk

PKG_NAME:=fb-test-app
PKG_RELEASE:=$(AUTORELEASE)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/andy-shev/fb-test-app.git
PKG_SOURCE_DATE:=20210901
PKG_SOURCE_VERSION:=e8fa7f17d643190cb2119284d02cd3c5281ca87d
PKG_MIRROR_HASH:=86be55835f31ade6140f2482433949cde85c1b274dac9f41f36111dcfbd37feb

include $(INCLUDE_DIR)/package.mk

define Package/fb-test-app
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=fb test app
endef

define Package/fb-test-app/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/perf $(1)/usr/bin/fb-test-perf
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/rect $(1)/usr/bin/fb-test-rect
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fb-test $(1)/usr/bin/fb-test
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/offset $(1)/usr/bin/fb-test-offset
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/fb-string $(1)/usr/bin/fb-test-string
endef

$(eval $(call BuildPackage,fb-test-app))
