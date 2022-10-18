# for openwrt 18
define KernelPackage/fb-sys-ram
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=Framebuffer in system RAM support
  DEPENDS:=+kmod-fb
  KCONFIG:= \
        CONFIG_FB_SYS_COPYAREA \
        CONFIG_FB_SYS_FILLRECT \
        CONFIG_FB_SYS_IMAGEBLIT
  FILES:= \
        $(LINUX_DIR)/drivers/video/fbdev/core/syscopyarea.ko \
        $(LINUX_DIR)/drivers/video/fbdev/core/sysfillrect.ko \
        $(LINUX_DIR)/drivers/video/fbdev/core/sysimgblt.ko
  AUTOLOAD:=$(call AutoLoad,07,syscopyarea sysfillrect sysimgblt)
endef

define KernelPackage/fb-sys-ram/description
 Kernel support for framebuffers in system RAM
endef

$(eval $(call KernelPackage,fb-sys-ram))

define KernelPackage/fb-tft
  SUBMENU:=$(VIDEO_MENU)
  TITLE:=Support for small TFT LCD display modules
  DEPENDS:= \
          @GPIO_SUPPORT +kmod-backlight \
          +kmod-fb +kmod-fb-sys-fops +kmod-fb-sys-ram +kmod-spi-bitbang
  KCONFIG:= \
       CONFIG_FB_BACKLIGHT=y \
       CONFIG_FB_DEFERRED_IO=y \
       CONFIG_FB_TFT
  FILES:= \
       $(LINUX_DIR)/drivers/staging/fbtft/fbtft.ko
  AUTOLOAD:=$(call AutoLoad,08,fbtft)
endef

define KernelPackage/fb-tft/description
  Support for small TFT LCD display modules
endef

$(eval $(call KernelPackage,fb-tft))
