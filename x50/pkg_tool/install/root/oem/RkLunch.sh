wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf
#./wakeWordAgent -e gpio &

echo 4 4 1 7 > /proc/sys/kernel/printk
cd /oem/marssenger
./x50app &
./X50QML &
#sh x50Daemon.sh &
