# Welcome

Because recently OpenWrt Official support is much better, the patches are not necessary anymore. For histrical reason, I will keep this.

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2, but the package should be able to port to other MT76x8 based platform too.

Feel free to submit patch and bug or email me at support@vocore.io 

For detailed tutorial, please check vonger.cn, Beginner Tutorial. If you are new to VoCore2, please follow the steps exactly to avoid any mysterious issue :)

# How to compile it

Please follow my steps in order to avoid issue. "path/to/" is your openwrt location.
Please use Linux, MacOS or other Unix compatible system to compile it, the file system must be case sensitive. 

## Use container (optional)

if your host compiler is too new for openwrt, or you want compile openwrt on a clean environment. you can use container to compile openwrt:

```
$ cd ./docker/
$ docker build -t vocore2-dev .
$ docker run --name vocore2-dev vocore2-dev
$ docker export vocore2-dev | tar2sqfs vocore2-dev.sqfs
$ singularity shell -e vocore2-dev.sqfs # use it
singularity> cd /path/to/openwrt # and more, see below
```

## Download Patch

```sh
git clone https://github.com/openwrt/openwrt.git
cd openwrt
git checkout v18.06.5 ...(or v19.07.3)
./scripts/feeds update -a
./scripts/feeds install -a 
cd ./package
git clone https://github.com/vonger/vocore2.git
```

## For OpenWrt 23.05

1. sdcard (patch dts, tutorial at  https://vonger.cn/?p=15483 )

2. audio/sound patch

```
patch -p1 < [path]/vocore2/openwrt.2305/sound/openwrt/vocore2-support-es8388.patch
patch -p1 < [path]/vocore2/openwrt.2305/sound/openwrt/vocore2-fixmaster-es8388.patch
cp [path]/vocore2/openwrt.2305/sound/patch-5.15/*.patch [openwrt]/target/linux/ramips/patches-5.15/
```

3. display/video patch

```
patch -p1 < [path]/vocore2/openwrt.2305/video/openwrt/vocore2-enable-video.patch
cp [path]/vocore2/openwrt.2305/video/patch-5.15/*.patch [openwrt]/target/linux/ramips/patches-5.15/
cp -r [path]/vocore2/openwrt.2305/video/package/mpro [openwrt]/package/utils/
```

4. ***.config*** here: [path]/vocore2/openwrt.2305/menu.config, include LuCI, madplay, camera, vocroe display, sound card driver.

## For OpenWrt 22.03

1. sdcard

```
patch -p1 < package/vocore2/openwrt.2102/vocore2-sdcard-support.patch
```

2. audio(not work well)

```
patch -p1 < package/vocore2/openwrt.2203/vocore2-fixmaster-es8388.patch
cp package/vocore2/openwrt.2203/0810-sound-es8388-aplay.patch target/linux/ramips/patches-5.10/
cp ./package/vocore2/openwrt.1907/0882-pinctrl-fix-gpio-name.patch target/linux/ramips/patches-5.10/
```

## For OpenWrt 21.02

1. sdcard

```
patch -p1 < package/vocore2/openwrt.2102/vocore2-sdcard-support.patch
```

2. audio

```
patch -p1 < package/vocore2/openwrt.2102/vocore2-fixmaster-es8388.patch
cp package/vocore2/openwrt.2102/0810-sound-es8388-aplay.patch target/linux/ramips/patches-5.4/
cp ./package/vocore2/openwrt.1907/0882-pinctrl-fix-gpio-name.patch target/linux/ramips/patches-5.4/
```

## For OpenWrt 19.07

1. apply patch for mt76 to support switch between 1T1R and 2T2R.
   
   ```sh
   mkdir ./package/kernel/mt76/patches
   cp ./package/vocore2/openwrt.1907/0822-mt76-read-factory-eeprom.patch ./package/kernel/mt76/patches
   ```
   
   note: patch has submitted to https://github.com/openwrt/mt76/pull/426, once merge this can be removed.

2. apply patch for different functions
   
   - use VoCore2 banner as default banner: patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-banner.patch 
   - default enable wifi once firmware load: patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-enable-wireless.patch 
   - use 192.168.61.1 as default ip address to avoid conflict: patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-lan-address.patch
   - use default root password "vocore": patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-password.patch
   - fix sdcard do not work well: patch -p1 < ./package/vocore2/openwrt.1907/vocore2-fixmaster-sdcard.patch

3. enable sound es8388(note: 19.07.3 still has bug, for stable version please use 18.05.6)
   
   - patch -p1 < ./package/vocore2/openwrt.1907/vocore2-fixmaster-es8388.patch
   - cp ./package/vocore2/openwrt.1907/0045-i2c-add-mt7621-driver.patch ./target/linux/ramips/patches-4.14
   - cp ./package/vocore2/openwrt.1907/0810-es8388-support.patch ./target/linux/ramips/patches-4.14
   - cp ./package/vocore2/openwrt.1907/0882-pinctrl-fix-gpio-name.patch ./target/linux/ramips/patches-4.14

4. enable sd card.
   
   - patch -p1 < ./package/vocore2/openwrt.1907/vocore2-default-enable-sdcard.patch

5. use default config setting.
   
   - cd path/to/openwrt
   - cp ./package/vocore2/openwrt.1907/config-4.14 ./target/linux/ramips/mt76x8/

Then ready to compile.

## For OpenWrt 18.06

Note: this version source code broken, patch might not work.

1. patch your openwrt with necessary patches to use this driver.
   
   ```sh
   cd path/to/openwrt
   patch -p1 < ./package/vocore2/openwrt.1806/mt7628/openwrt/000-*.patch
   patch -p1 < ./package/vocore2/openwrt.1806/mt7628/openwrt/luci/*.patch
   mkdir ./package/network/utils/iwinfo/patches
   cp ./package/vocore2/openwrt.1806/mt7628/openwrt/080-*.patch ./package/network/utils/iwinfo/patches
   ```
   
   note: patch for iwinfo might broken wifi driver based on 802.11(such as USB WiFi), but it is necessary to make mt7628 works with uci system. In futher, I consider to add patch to make mt7628 driver support 802.11.

2. you can direct `cp ./package/vocore2/openwrt.1806/.config ./` ***OR*** configure mt7628 in `make menuconfig`
   
   - Target System: MediaTek Ralink MIPS
   - Subtarget: MT76x8 based boards
   - Target Profile: VoCore VoCore2
   - Kernel modules -> Wireless Drivers -> unselect kmod-mt76 and select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode; enable SNIFFER for monitor mode.
   - Base System -> select wireless-tools (need its iwpriv)
   - Network(option): unselect wapd-mini then hostapd-common (mt7628.ko already have WPA support)
   - Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)

3. support es8388 (sound card on VoCore2 Ultimate) for VoCore2 Ultimate
   
   ```sh
   cd path/to/openwrt
   patch -p1 < ./package/vocore2/openwrt.1806/es8388/000-*.patch
   cp ./package/vocore2/openwrt.1806/es8388/810-*.patch ./target/linux/ramips/patches-4.14
   cp ./package/vocore2/openwrt.1806/0045-*.patch ./target/linux/ramips/patches-4.14
   ```
   
   - Kernel modules -> Sound Support -> select kmod-sound-core and kmod-sound-mt7628
   - Kernel modules -> I2C support -> select kmod-i2c-mt7628

4. (optional)support spi full duplex(gpio-bitbang) and/or gpio cs for more spi devices.
   
   ```sh
   cd path/to/openwrt
   cp ./package/vocore2/openwrt.1806/0043-spi-add-mt7621-support.patch ./target/linux/ramips/patches-4.14
   cp ./package/vocore2/openwrt.1806/811-spi-gpio-chip-select.patch ./target/linux/ramips/patches-4.14
   ```

5. copy config-4.14 to openwrt kernel config folder to avoid missing any kernel module.
   
   ```sh
   cd path/to/openwrt
   cp ./package/vocore2/openwrt.1806/config-4.14 ./target/linux/ramips/mt76x8/
   ```

6. compile and enjoy!
   
   ```sh
   cd path/to/openwrt
   make
   ```

# Setup develop enviroment(based on Qt creator)

We can debug and develop based on Qt creator(or Eclipse if you like Java)
[Tutorial Link Here]http://vonger.cn/?p=14657

# Beginner Tutorial

Setup VM and Compile OpenWrt: https://www.youtube.com/watch?v=ocl6yFtKSNs

Another Setup and Compile: http://vonger.cn/?s=Beginner

# mainline uboot support

Now VoCore2 has mainline uboot available, but need more testing to verify that everything is stable.

if you need it, see below:

before replace uboot ,you need read how to fix dead uboot: https://vonger.cn/?p=8054

flash layout difference of mainline uboot and ralink uboot:

mainline uboot:

https://source.denx.de/u-boot/u-boot/-/blob/master/configs/vocore2_defconfig

| uboot | uboot-env | factory | firmware |
| ----- | --------- | ------- | -------- |
| 312k  | 4k        | 4k      | -        |

uboot from mtk:

http://vonger.cn/upload/uboot.source.zip

| uboot | uboot-env | factory | firmware |
| ----- | --------- | ------- | -------- |
| 192k  | 64k       | 64k     | -        |

mainline uboot's layout can include more code in uboot, but it not compatible with the mtk's uboot.

if you need 192k mainline uboot, please use uboot/vocore2_defconfig to compile maineline uboot:

```
cp path/to/thisrepo/uboot/vocore2_defconfig path/to/mainlineubootsrc/.config
```

note: before replace uboot, you must ensure your 'u-boot-with-spl.bin' size is < 192k

# buildroot support

add buildroot support in ./buildroot-*, use for mainline kernel build and mainline uboot build, a lot of driver not working, only for test.

how to use:

```
git clone https://github.com/buildroot/buildroot --depth=1
cd buildroot
export BR2_EXTERNAL=/path/to/repo/buildroot/
make vocore2_defconfig
make menuconfig
make -j4
```
