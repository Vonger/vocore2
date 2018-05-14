# Welcome

This feed enable using MTK/Ralink official wifi driver for the latest linux kernel 4.14/openwrt. It should be much stable than current mt76 driver. I patch it for my production VoCore2.

Feel free to submit patch and bug or email me at support@vocore.io

This patch is only for study/personal usage, and only for VoCore2, it might burn your house, launch nuke, etc... please use it carefully and risk is on you. 
Source code of mt7628 is from internet.


# Why use Linux 4.14

For faster speed and more security.


# How to use it

1. add this feeds to your openwrt source.

2. patch your openwrt with openwrt/package/kernel/mt7628/openwrt/vocore2.patch

```
cd ~/openwrt
patch p1 < ./package/kernel/mt7628/openwrt/vocore2.patch
```
3. config mt7628 in `make menuconfig` -> Kernel modules -> Wireless Drivers -> kmod-mt7628

4. compile and enjoy!


# Known Issue

1. STA mode is not working

this is because in ralink.sh, sta mode is using ap_client but not directly set by iwpriv.

 
