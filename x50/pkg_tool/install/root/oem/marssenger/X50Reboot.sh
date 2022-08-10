#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=270
export TZ='Asia/Shanghai'

sleep 1
cd /oem/marssenger && ./X50QML &
