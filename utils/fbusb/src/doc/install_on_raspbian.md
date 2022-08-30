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

## ready for use

1. plugout hdmi cable

2. reboot raspberrypi board

3. connect vocore screen

4. the graphic will display on vocore screen by default (test on rpi4)
