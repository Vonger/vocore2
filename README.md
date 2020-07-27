# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2, but the package should be able to port to other MT76x8 based platform too.

Feel free to submit patch and bug or email me at support@vocore.io 

For detailed tutorial, please check vonger.cn, Beginner Tutorial. If you are new to VoCore2, please follow the steps exactly to avoid any mysterious issue :)


# How to compile it

Please follow my steps in order to avoid issue. "path/to/" is your openwrt location.
Please use Linux, MacOS or other Unix compatible system to compile it, the file system must be case sensitive. 

## For OpenWrt 19.07

1. apply patch for mt76 to support switch between 1T1R and 2T2R.

  ```sh
mkdir ./package/kernel/mt76/patches
cp ./package/vocore2/openwrt.1907/0822-mt76-read-factory-setting.patch ./package/kernel/mt76/patches
  ``` 
note: submit to https://github.com/openwrt/mt76/pull/426, once merge this can be removed.

## For OpenWrt 18.06 

1. add the code to your openwrt source(support openwrt 18.06.5).

  ```sh
git clone https://github.com/openwrt/openwrt.git
cd openwrt
git checkout v18.06.5
./scripts/feeds update luci
./scripts/feeds install -a -p luci
cd ./package
git clone https://github.com/vonger/vocore2.git
  ```

2. patch your openwrt with necessary patches to use this driver.

  ```sh
cd path/to/openwrt
patch -p1 < ./package/vocore2/openwrt.1806/mt7628/openwrt/000-*.patch
patch -p1 < ./package/vocore2/openwrt.1806/mt7628/openwrt/luci/*.patch
mkdir ./package/network/utils/iwinfo/patches
cp ./package/vocore2/openwrt.1806/mt7628/openwrt/080-*.patch ./package/network/utils/iwinfo/patches
  ```
  
  note: patch for iwinfo might broken wifi driver based on 802.11(such as USB WiFi), but it is necessary to make mt7628 works with uci system. In futher, I consider to add patch to make mt7628 driver support 802.11.


3. you can direct `cp ./package/vocore2/openwrt.1806/.config ./` ***OR*** configure mt7628 in `make menuconfig`

  - Target System: MediaTek Ralink MIPS
  - Subtarget: MT76x8 based boards
  - Target Profile: VoCore VoCore2
  - Kernel modules -> Wireless Drivers -> unselect kmod-mt76 and select kmod-mt7628 -> select WiFi Operation Mode -> enable AP-Client support for AP+STA mode; enable SNIFFER for monitor mode.
  - Base System -> select wireless-tools (need its iwpriv)
  - Network(option): unselect wapd-mini then hostapd-common (mt7628.ko already have WPA support)
  - Global build settings(option): Kernel build options -> /dev/mem virtual device support(enable /dev/mem for easy debug)

4. support es8388 (sound card on VoCore2 Ultimate) for VoCore2 Ultimate

  ```sh
cd path/to/openwrt
patch -p1 < ./package/vocore2/es8388/*.patch
cp ./package/vocore2/openwrt/810-*.patch ./target/linux/ramips/patches-4.14
cp ./package/vocore2/openwrt/0045-*.patch ./target/linux/ramips/patches-4.14
  ```
  
  - Kernel modules -> Sound Support -> select kmod-sound-core and kmod-sound-mt7628
  - Kernel modules -> I2C support -> select kmod-i2c-mt7628

5. (optional)support spi full duplex and gpio cs for more spi devices.

  ```sh
cd path/to/openwrt
cp ./package/vocore2/openwrt/0043-spi-add-mt7621-support.patch ./target/linux/ramips/patches-4.14
cp ./package/vocore2/openwrt/811-spi-gpio-chip-select.patch ./target/linux/ramips/patches-4.14
  ```

6. copy config-4.14 to openwrt kernel config folder to avoid missing any kernel module.

  ```sh
cd path/to/openwrt
cp ./package/vocore2/openwrt.1806/config-4.14 ./target/linux/ramips/mt76x8/
  ```

7. compile and enjoy!

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
