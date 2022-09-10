# install vocore screen driver on raspbian

## upgrade the system

```
sudo apt update && sudo apt upgrade -y
```

## install kernel headers and build tools

```
sudo apt install raspberrypi-kernel-headers build-essential -y
```

## reboot for use new kernel

```
sudo reboot
```

## download & compile & install driver

```
git clone https://github.com/Vonger/vocore2 # download source code
cd vocore2
cd utils/fbusb/src
make -C /usr/src/linux-headers-`uname -r`/ M=`pwd` modules # compile
sudo mkdir -p /lib/modules/`uname -r`/extra
sudo cp fbusb.ko /lib/modules/`uname -r`/extra/ # install module
sudo depmod -a # for autoloading
```

## install xorg configuration file

```
sudo cp /etc/X11/xorg.conf /etc/X11/xorg.conf.bak # backup origin config file
sudo nano /etc/X11/xorg.conf
```

please paste:

```
Section "Device"
	Identifier      "vocore screen"
	Driver          "fbturbo"
	Option          "fbdev" "/dev/fb0"

	Option          "SwapbuffersWait" "true"
EndSection
```

Use 

```
Ctrl+X
y
Enter
```

to save file

## ready for use

1. plugout hdmi cable

2. connect vocore screen

3. reboot raspberrypi board

4. the graphic will display on vocore screen by default (test on rpi4)

## note

some screen is small, like your old android 2.3 phone

you may be need tune the ui, like android app

eg: button need big size easy to tap

there is some suggest project:

* https://plasma-mobile.org/

* https://sxmo.org/

* XFCE4
