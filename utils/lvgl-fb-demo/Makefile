include $(TOPDIR)/rules.mk

PKG_NAME:=lvgl-fb-demo
PKG_RELEASE:=$(AUTORELEASE)

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/lvgl/lv_port_linux_frame_buffer.git
PKG_SOURCE_DATE:=2022-09-17
PKG_SOURCE_VERSION:=a2384b74cec9ffd7407d12ed64a8326b5696f309
PKG_MIRROR_HASH:=641cf757d63a99c130d8482eb12a6c8682012004fc08f8a4002342dc3c54b063

include $(INCLUDE_DIR)/package.mk

define Package/lvgl-fb-demo
  SECTION:=vocore2
  CATEGORY:=VoCore2
  TITLE:=lvgl framebuffer demo 
endef

define Package/lvgl-fb-demo/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/demo $(1)/usr/bin/lvgl-fb-demo
endef

$(eval $(call BuildPackage,lvgl-fb-demo))
