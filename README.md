# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2.

Feel free to submit patch and bug or email me at support@vocore.io

This patch is only for study/personal usage, and only for VoCore2, it might burn your house, launch nuke, etc... please use it carefully and risk is on you. 
Source code of mt7628 is from internet.


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

mkdir path/to/openwrt/package/network/utils/iwinfo/patches
cp path/to/openwrt/package/kernel/mt7628/openwrt/080-*.patch path/to/openwrt/package/network/utils/iwinfo/patches
```


3. config mt7628 in `make menuconfig` Kernel modules -> Wireless Drivers -> kmod-mt7628

- Target System: MediaTek Ralink MIPS
- Subtarget: MT76x8 based boards
- Kernel modules -> Wireless Drivers -> unselect kmod-mt76 / select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode; enable SNIFFER for monitor mode.
- Base System -> select wireless-tools (need its iwpriv)
- Network(option): unselect wapd-mini/hostapd-common (mt7628.ko already have WPA support)
- Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)
- Global build settings(option): unselect Cryptographically signed package lists (this will block compile)


4. compile and enjoy!

### Option

1. enable luci setting up AP+STA mode, WPA/WPA2. default: disable

install luci feeds first.

patch -p1 < ./package/kernel/mt7628/openwrt/luci/*.patch

details please check http://vonger.cn/?p=14525

2. fix refclk patch(default now)

```
cp ./package/kernel/mt7628/openwrt/882-* ./target/linux/ramips/patches-4.14
```

This patch will able to use refclk in DTS file, old version name conflict.
I should submit it to openwrt, not yet :)

3. add unique flash id(default now)

```
cp ./package/kernel/mt7628/openwrt/0306-* ./target/linux/ramips/patches-4.14
```

In /sys folder you can find a file named fid, that id is bind to flash and fixed and unique for every VoCore2. 

4. support es8388 for VoCore2 Ultimate

```
patch -p1 < ./package/kernel/es8388/openwrt/000-*.patch
cp ./package/kernel/es8388/openwrt/810*.patch ./target/linux/ramips/patches-4.14
```


# Known Issue + TODO

1. support mutilssid in **uci wireless config**.

2. support monitor in **uci wireless config**.

note: 1, 2 are supported already, check my blog at vonger.cn to get tutorial about how to make them work.


# Thank you

Thanks to all openwrt contributors! Hope we can make wireless more freedom and better :) 
