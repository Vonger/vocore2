include $(TOPDIR)/rules.mk

PKG_NAME:=mt7628tool
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_CONFIG_DEPENDS:= CONFIG_KERNEL_DEVMEM

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/mt7628tool
  SECTION:=vocore2
  CATEGORY:=VoCore2
  TITLE:=mt7628 tool
endef

define Package/mt7628tool/description
  use for get mt7628's temperature, etc.
  need /dev/mem
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Package/mt7628tool/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mt7628tool $(1)/usr/bin
endef

$(eval $(call BuildPackage,mt7628tool))

