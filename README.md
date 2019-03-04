# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2.

Feel free to submit patch and bug or email me at support@vocore.io

This patch is only for study/personal usage, and only for VoCore2, it might burn your house, launch nuke, kill a baby... use it carefully and risk is on you. 

For details tutorial, please check vonger.cn, Beginner Tutorial.


# How to use it

Please follow the steps in order to avoid issue. "path/to/" is your openwrt location.

1. add this feeds to your openwrt source(support 18.06.02).

  ```sh
git clone https://github.com/openwrt/openwrt.git
git checkout v18.06.2
cd path/to/openwrt
path/to/openwrt/scripts/feeds update luci
path/to/openwrt/scripts/feeds install -a -p luci
cd path/to/openwrt/package
git clone https://github.com/vonger/vocore2.git
  ```

2. patch your openwrt with necessary patches to use this driver.

  ```sh
cd path/to/openwrt
patch -p1 < path/to/openwrt/package/vocore2/mt7628/openwrt/000-*.patch
patch -p1 < path/to/openwrt/package/vocore2/mt7628/openwrt/luci/*.patch

mkdir path/to/openwrt/package/network/utils/iwinfo/patches
cp path/to/openwrt/package/vocore2/mt7628/openwrt/080-*.patch path/to/openwrt/package/network/utils/iwinfo/patches
  ```
  
  note: patch for iwinfo might broken wifi driver based on 802.11(such as USB WiFi), but it is necessary to make mt7628 works with uci system. In futher, I consider to add patch to make mt7628 driver support 802.11.


3. config mt7628 in `make menuconfig` Kernel modules -> Wireless Drivers -> kmod-mt7628

  - Target System: MediaTek Ralink MIPS
  - Subtarget: MT76x8 based boards
  - Target Profile: VoCore VoCore2
  - Kernel modules -> Wireless Drivers -> unselect kmod-mt76 and select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode; enable SNIFFER for monitor mode.
  - Base System -> select wireless-tools (need its iwpriv)
  - Network(option): unselect wapd-mini then hostapd-common (mt7628.ko already have WPA support)
  - Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)


4. compile and enjoy!


### Option (Sound)

1. support es8388 (sound card on VoCore2 Ultimate) for VoCore2 Ultimate

  ```sh
patch -p1 < path/to/openwrt/package/vocore2/es8388/openwrt/000-*.patch
cp path/to/openwrt/package/vocore2/es8388/openwrt/810*.patch path/to/openwrt/target/linux/ramips/patches-4.14
  ```
  - Kernel modules -> Sound Support -> select kmod-sound-core and kmod-sound-mt7628
  - Kernel modules -> I2C support -> unselect kmod-i2c-mt7628, select kmod-i2c-gpio-custom
  
note: kmod-i2c-mt7628 do not support some of the i2c features, so i2c-tools can not read from it. because i2c is a slow interface and with little data to transfer, so we can directly use gpio i2c for easy debug, but kmod-i2c-mt7628 should works too.

