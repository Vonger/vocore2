#!/bin/sh

set -eu

# network custom
patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-lan-address.patch
patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-enable-wireless.patch

# sound
patch -p1 < ./package/vocore2/openwrt.1806/es8388/000-openwrt-compatible-sound.patch
cp ./package/vocore2/openwrt.1806/es8388/810-es8388-support.patch ./target/linux/ramips/patches-4.14/

# spi
cp ./package/vocore2/openwrt.1806/0043-spi-add-mt7621-support.patch ./target/linux/ramips/patches-4.14
cp ./package/vocore2/openwrt.1806/811-spi-gpio-chip-select.patch ./target/linux/ramips/patches-4.14

# kernel custom
cp ./package/vocore2/openwrt.1806/config-4.14 ./target/linux/ramips/mt76x8/

exit 0
