#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=90
export TZ='Asia/Shanghai'
export QT_QPA_FB_DRM=0

function check(){
  count=`ps -ef |grep $1 |grep -v "grep" |wc -l`
  #echo $count
  if [ 1 != $count ];then
    sh /oem/marssenger/S100Marssenger restart &
  fi
}

while true
do
    sleep 30
    check X8app
    check X8UI
done
