#!/bin/sh

set -eu

# network custom
patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-lan-address.patch
patch -p1 < ./package/vocore2/openwrt.2102/vocore2-default-enable-wireless.patch

# sdcard
patch -p1 < ./package/vocore2/openwrt.2102/vocore2-sdcard-support.patch

# sound
patch -p1 < ./package/vocore2/openwrt.2203/vocore2-fixmaster-es8388.patch
cp ./package/vocore2/openwrt.2203/0810-sound-es8388-aplay.patch ./target/linux/ramips/patches-5.10/
cp ./package/vocore2/openwrt.1907/0882-pinctrl-fix-gpio-name.patch ./target/linux/ramips/patches-5.10/

exit 0
