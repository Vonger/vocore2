# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2.

Feel free to submit patch and bug or email me at support@vocore.io

This patch is only for study/personal usage, and only for VoCore2, it might burn your house, launch nuke, kill a baby... use it carefully and risk is on you. 

For details tutorial, please check vonger.cn, Beginner Tutorial.


# Why use Linux 4.14

For faster speed and more security.


# How to use it

1. add this feeds to your openwrt source(support 18.06.02).

```
git clone https://github.com/openwrt/openwrt.git
git checkout v18.06.2
cd path/to/openwrt
path/to/openwrt/scripts/feeds update luci
path/to/openwrt/scripts/feeds install -a -p luci
cd path/to/openwrt/package
git clone https://github.com/vonger/vocore2.git
```

2. patch your openwrt with necessary patches to use this driver.

```
cd path/to/openwrt
patch -p1 < path/to/openwrt/package/vocore2/mt7628/openwrt/000-*.patch
patch -p1 < path/to/openwrt/package/vocore2/mt7628/openwrt/luci/*.patch

mkdir path/to/openwrt/package/network/utils/iwinfo/patches
cp path/to/openwrt/package/vocore2/mt7628/openwrt/080-*.patch path/to/openwrt/package/network/utils/iwinfo/patches
```


3. config mt7628 in `make menuconfig` Kernel modules -> Wireless Drivers -> kmod-mt7628

- Target System: MediaTek Ralink MIPS
- Subtarget: MT76x8 based boards
- Target Profile: VoCore VoCore2
- Kernel modules -> Wireless Drivers -> unselect kmod-mt76 / select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode; enable SNIFFER for monitor mode.
- Base System -> select wireless-tools (need its iwpriv)
- Network(option): unselect wapd-mini / hostapd-common (mt7628.ko already have WPA support)
- Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)


4. compile and enjoy!

### Option

1. support es8388 for VoCore2 Ultimate

```
patch -p1 < path/to/openwrt/package/vocore2/es8388/openwrt/000-*.patch
cp path/to/openwrt/package/vocore2/es8388/openwrt/810*.patch path/to/openwrt/target/linux/ramips/patches-4.14
```


# Thank you

Thanks to all openwrt contributors! Hope we can make wireless more freedom and better :) 
