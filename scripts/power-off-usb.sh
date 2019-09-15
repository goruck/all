#!/bin/sh

# Shut off power on USB ports (this shuts power on ethernet as well).
# See https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=162539#p1051587.
echo '1-1' | sudo tee /sys/bus/usb/drivers/usb/unbind

exit 0