#!/bin/sh
export QT_QPA_PLATFORM=linuxfb:tty=/dev/fb0:rotation=270
export TZ='Asia/Shanghai'
# export QT_QPA_FB_DRM=1

function check() {
  count=$(ps | grep $1 | grep -v "grep" | wc -l)
  #echo $count
  if [ 1 != $count ]; then
    killall -9 $1
    echo $1
    cd /oem/marssenger && ./$1 &
  fi
}

function check2() {
  line=$(ps | grep $1 | grep -v "grep")
  count=$(echo $line | wc -l)
  # echo $count
  if [ 1 != $count ]; then
    killall -9 $1
    echo $1
    cd /oem/marssenger && ./$1 &
    return 0
  fi

  # hour=$(date "+%H")
  # echo "hour:$hour"
  # if [ $hour -gt 3 ] || [ $hour -lt 1 ]; then
  #   echo "time no arrive"
  #   return 1
  # fi

  size=$(echo $line | awk '{print $3}')
  echo "mem:$size"

  case $size in
  *"m")
    # echo "It's exist m"
    msize=$(echo $size | tr -cd "[0-9]")
    echo $msize
    if [ "$msize" -gt "160" ]; then
      echo "big,reboot"
      killall -10 $1
      # else
      # echo "small"
    fi
    ;;
  [0-9]*)
    # echo "It's number"
    if [ $size -gt 160000 ]; then
      echo "big,reboot"
      killall -10 $1
      # else
      # echo "small"
    fi
    ;;
  *)
    echo "error"
    ;;
  esac

}

while true; do
  sleep 30
  check X50app
  check2 X50QML
done
