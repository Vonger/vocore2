################################################################################
#
# mt76-firmware
#
################################################################################

MT76_FIRMWARE_VERSION = 679254c50f279fe4f1e3ae1fb943f0d1ecdac4ab
MT76_FIRMWARE_SITE = https://github.com/openwrt/mt76
MT76_FIRMWARE_SITE_METHOD = git

# mt7628
ifeq ($(BR2_PACKAGE_MT76_FIRMWARE_MT7628),y)
MT76_FIRMWARE_FILES += firmware/mt7603_e1.bin
MT76_FIRMWARE_FILES += firmware/mt7603_e2.bin
MT76_FIRMWARE_FILES += firmware/mt7628_e1.bin
MT76_FIRMWARE_FILES += firmware/mt7628_e2.bin
endif

ifneq ($(MT76_FIRMWARE_FILES),)
define MT76_FIRMWARE_INSTALL_FILES
        cd $(@D) && \
                $(TAR) cf install.tar $(sort $(MT76_FIRMWARE_FILES)) && \
                $(TAR) xf install.tar -C $(TARGET_DIR)/lib/
endef
endif

define MT76_FIRMWARE_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/lib/firmware/
	$(MT76_FIRMWARE_INSTALL_FILES)
endef

define MT76_FIRMWARE_INSTALL_IMAGES_CMDS
	$(call MT76_FIRMWARE_INSTALL_FW, $(BINARIES_DIR))
endef

$(eval $(generic-package))
