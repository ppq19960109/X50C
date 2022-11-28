#!/bin/sh

cd /oem && ./logoapp
##echo 4 > /sys/class/graphics/fb0/blank
echo 0 >/sys/class/graphics/fb0/blank
