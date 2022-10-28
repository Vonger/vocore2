include $(TOPDIR)/rules.mk

PKG_NAME:=memtester
PKG_VERSION:=4.5.1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://gentoo.osuosl.org/distfiles/
PKG_MD5SUM:=1bc22c01e987d6a67fac39dc5656a4d3

include $(INCLUDE_DIR)/package.mk

define Package/memtester
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=memory test program, userspace
endef

define Build/Compile
	sed -i -e "s/^cc/$(TARGET_CC)/" $(PKG_BUILD_DIR)/conf-cc
	sed -i -e "s/^cc/$(TARGET_CC)/" $(PKG_BUILD_DIR)/conf-ld
	$(MAKE) -C $(PKG_BUILD_DIR)
endef


define Package/memtester/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/memtester $(1)/usr/bin/memtester
endef

$(eval $(call BuildPackage,memtester))
