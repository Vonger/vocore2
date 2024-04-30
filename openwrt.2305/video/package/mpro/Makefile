include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mpro
PKG_VERSION:=v1
PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define KernelPackage/mpro
  TITLE:=MPRO DRM driver
  FILES:=$(PKG_BUILD_DIR)/mpro.ko
  SECTION:=vocore2
  CATEGORY:=VoCore2
  KCONFIG:= \
	CONFIG_DRM_FBDEV_EMULATION=y
  AUTOLOAD:=$(call AutoLoad,99,mpro)
  DEPENDS:=+kmod-drm +kmod-drm-kms-helper +kmod-drm-gem-shmem-helper +kmod-usb-core
endef

define KernelPackage/mpro/description
  MPRO DRM driver.
endef

MAKE_OPTS := \
	ARCH="$(LINUX_KARCH)" \
	CROSS_COMPILE="$(TARGET_CROSS)" \
	SUBDIRS="$(PKG_BUILD_DIR)" \
	M="$(PKG_BUILD_DIR)"

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" $(MAKE_OPTS) modules
endef

$(eval $(call KernelPackage,mpro))
