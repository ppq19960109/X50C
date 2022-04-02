cd /oem && ./logoapp
wpa_supplicant -B -i wlan0 -c /data/cfg/wpa_supplicant.conf
#./wakeWordAgent -e gpio &

echo 4 4 1 7 >/proc/sys/kernel/printk
if [ ! -d /userdata/nfs ]; then
    mkdir -p /userdata/nfs
fi

for file in $(ls /oem/upgrade*.bin)
do
    if [ -f $file ];then
        echo $file
        sh $file
        rm -rf /oem/upgrade*.bin
        sync
        reboot
        break
    fi
done 
sh /oem/marssenger/S100Marssenger start
