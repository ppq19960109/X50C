#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=270
export TZ='Asia/Shanghai'

usleep 500000
cd /oem/marssenger && ./X50PanguQML &
