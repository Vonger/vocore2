# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2.

Feel free to submit patch and bug or email me at support@vocore.io

This patch is only for study/personal usage, and only for VoCore2, it might burn your house, launch nuke, etc... please use it carefully and risk is on you. 
Source code of mt7628 is from internet.


# Why use Linux 4.14

For faster speed and more security.


# How to use it

1. add this feeds to your openwrt source.

```
cd ~/openwrt
cp feeds.conf.default feeds.conf
echo src-git vocore2 https://github.com/vonger/vocore2.git >> feeds.conf
./scripts/feeds update vocore2
cp -r ./feeds/vocore2/mt7628 ./package/kernel
```

FIXME: I have no idea why `./scripts/feeds install -a -p vocore2` not work...

2. patch your openwrt with necessary patches to use this driver.

```
cd ~/openwrt
patch -p1 < ./package/kernel/mt7628/openwrt/000-*.patch

mkdir ./package/network/utils/iwinfo/patches
cp ./package/kernel/mt7628/openwrt/080-*.patch ./package/network/utils/iwinfo/patches
```


3. config mt7628 in `make menuconfig` Kernel modules -> Wireless Drivers -> kmod-mt7628

- Target System: MediaTek Ralink MIPS
- Subtarget: MT76x8 based boards
- Kernel modules -> Wireless Drivers -> unselect kmod-mt76 / select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode and AdHoc mode; enable SNIFFER for monitor mode.
- Base System -> select wireless-tools (need its iwpriv)
- Network(option): unselect wapd-mini/hostapd-common (mt7628.ko already have WPA support)
- Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)


4. compile and enjoy!

### Option

1. enable luci setting up STA mode, WPA/WPA2. default: disable

install luci feeds first.

patch -p1 ./package/kernel/mt7628/openwrt/luci/*.patch


# Known Issue + TODO

1. AP + STA mode is not fully support all mode

currently it is only for wpa2, rest mode need to upgrade the ralink.sh

2. support mutilssid in uci wireless config.

3. support monitor in uci wireless config.

note: 2, 3 are supported already, check my blog at vonger.cn to get tutorial about how to make them work.


# Thank you

Thanks to all openwrt contributors! Hope we can make wireless more freedom and better :) 
