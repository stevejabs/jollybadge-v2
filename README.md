# jollybadge-v2
JollyBadge [V2] is a sequential puzzle / challenge badge. It’s going to test basic observational puzzle solving skills, but also may require you to create software tools, perform OSINT work, and even some hardware hacking. 

# which firmware 
Right now, there is the one and only. The current version is: JollyBadgeV2-0.5.1.bin

# how do?
1. Install `bossac` from here: [https://github.com/shumatech/BOSSA](https://github.com/shumatech/BOSSA)
2. Plug in your JollyBadge [V2] to an available USB port
3. Double-press the reset button on your JollyBadge and you'll see the LEDs freeze
4. Be sure that Arduino IDE or any other software that may use `bossac` is running
5. Run the following CLI command: `bossac -i -d --port=tty.usbmodem31301 --offset=0x2000 -e -w -v -R JollyBadgeV2-0.5.1.bin`

NOTE: Be sure to replace the port with what it is on your machine. On Windows it's one thing, on macOS it's another, and linux again... different. Look up how to search these out on your system.

NOTE: Double check and make sure the offset is unchanged. That will make sure only the image flashes and the bootloader remains untouched.


