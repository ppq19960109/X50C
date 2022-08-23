#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=90
export TZ='Asia/Shanghai'
export QT_LINUXFB_DRM_LOGO=/oem/logo.bmp

function check(){
  count=`ps -ef |grep $1 |grep -v "grep" |wc -l`
  #echo $count
  if [ 1 != $count ];then
    killall -9 $1
    echo $1
    cd /oem/marssenger && ./$1 &
  fi
}

while true
do
    sleep 30
    check X8app
    check X8UI
done
