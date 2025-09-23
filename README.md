# OpenJVS (libgpiod)
What's new?  
This is an updated version of the excellent Bobby's openJVS project. 
The /sys/class/gpio interface used by openJVS for interacting with board's sense line is deprecated; this version uses libgiod instead (*). 

This version was tested and confirmed working on a raspberry pi 5.
OpenJVS HAT is also confirmed working.

How to make it work on a Raspberry pi 5 ?
1. Enable raspberry PI UART ports by updating your raspberry pi 5 config.txt file with these entries (**):  
dtoverlay=uart2-pi5  
dtoverlay=uart3-pi5

2. install dependencies "sudo apt install gpiod libgpiod-dev" (***)
3. compile and install as usual.
4. In case you use openJVS HAT  with your raspberry pi 5, change openJVS config file "DEVICE_PATH /dev/ttyAMA2", all jumpers on the left, UART2 on GPIO 4 & 5 (****).  
5. make sure to have  /etc/openjvs/devices/pwr_button.disabled, so openJVS does not take into account the raspberry pi 5's power button as input device.

  
(\*) Changes in dtoverlay:  
- https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README  
- https://pip.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-006553-WP/A-history-of-GPIO-usage-on-Raspberry-Pi-devices-and-current-best-practices.pdf  
(\*\*) OpenJVS HAT GPIO usage:   
- https://github.com/OpenJVS/OpenJVS/blob/master/docs/OpenJVS_IO_Manual_1.2.pdf  
(\*\*\*)  new tools:  
- https://libgpiod.readthedocs.io/en/latest/gpio_tools.html  
(\*\*\*\*)  UART2, GPIO 4 & 5:   
https://github.com/raspberrypi/firmware/blob/e57538c91b473d23f98bf41fcffdc61b4198a632/boot/overlays/README#L5292  



What's openJVS?  
OpenJVS is an emulator for I/O boards in arcade machines that use the JVS protocol. It requires a USB RS485 converter, or an official OpenJVS HAT.

The following arcade boards are supported:

- Naomi 1/2
- Triforce
- Chihiro
- Hikaru
- Lindbergh
- Ringedge 1/2
- Namco System 22/23
- Namco System 2x6
- Taito Type X+
- Taito Type X2
- exA-Arcadia

Questions can be asked in the discord channel: https://arcade.community. If it asks you to create an account, you can simply click anywhere away from the box  and it'll let you in!

## Installation

Installation is done from the git repository as follows:

```
sudo apt install build-essential cmake git file
git clone https://github.com/openjvs/openjvs
make
sudo make install
```

## Guides

- [Manual & Detailed Hat Guide](docs/OpenJVS_IO_Manual_1.2.pdf)
- [Software Guide](docs/guide.md) 
- [Hat Quickstart Guide](docs/hat-quickstart.md)


