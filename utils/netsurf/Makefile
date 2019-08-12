# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.

include $(TOPDIR)/rules.mk

PKG_NAME:=netsurf-all
PKG_VERSION:=3.9

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://download.netsurf-browser.org/netsurf/releases/source-full/
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/netsurf
	MAINTAINER:="Qin Wei" <support@vocore.io>
	SUBMENU:=Web
	SECTION:=network
	CATEGORY:=Network
	TITLE:=netsurf
	URL:=http://www.netsurf-browser.org/
	DEPENDS:=+libjpeg +libpng +libsdl +libcss +libdom +libhubbub \
		+libparserutils +libnsfb +libnsgif +libnsbmp +libutf8proc \
		+libnsutils +libxml2 +libcurl +libfreetype +liberation-fonts-ttf \
		+libopenssl $(ICONV_DEPENDS)
endef

define Package/netsurf/description
	NetSurf is a free, open source web browser. It is written in C and released under the GNU Public Licence version 2. NetSurf has its own layout and rendering engine entirely written from scratch. It is small and capable of handling many of the web standards in use today.
endef

define Build/Prepare
	$(call Build/Prepare/Default)
endef

MAKE_FLAGS += TARGET=framebuffer TMP_PREFIX=$(STAGING_DIR)/usr Q=

define Build/Compile
	$(MAKE_VARS) \
	$(MAKE) $(PKG_JOBS) -C $(PKG_BUILD_DIR)/$(MAKE_PATH) \
		$(MAKE_FLAGS)
endef

define Package/netsurf/install
	$(INSTALL_DIR) \
		$(1)/usr/bin $(1)/usr/share/netsurf

	$(INSTALL_BIN) \
		$(PKG_INSTALL_DIR)/usr/bin/netsurf \
		$(1)/usr/bin/

	$(INSTALL_DATA) \
		$(PKG_INSTALL_DIR)/usr/share/netsurf/* \
		$(1)/usr/share/netsurf/
endef

$(eval $(call BuildPackage,netsurf))
