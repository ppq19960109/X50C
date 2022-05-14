#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=270
export TZ='Asia/Shanghai'

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
    sleep 10
    check X50app
    check X50QML
    sleep 20
done
